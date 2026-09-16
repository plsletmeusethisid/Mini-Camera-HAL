#include <cmath>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "camera/CameraSession.h"
#include "camera/AsyncCameraSession.h"
#include "camera/FrameBuffer.h"
#include "camera/MockCameraDevice.h"
#include "camera/RequestQueue.h"
#include "camera/BufferPool.h"
#include "camera/ImageProcessing.h"
#include "camera/MetricsCollector.h"
#ifdef MCH_HAS_OPENCV
#include "camera/OpenCVCameraDevice.h"
#endif

namespace {

class TestFailure final : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

#define CHECK(condition)                                                                    \
  do {                                                                                      \
    if (!(condition)) {                                                                     \
      throw TestFailure(std::string("check failed: ") + #condition + " at " + __FILE__ + ":" + \
                        std::to_string(__LINE__));                                           \
    }                                                                                       \
  } while (false)

template <typename Exception, typename Function>
void checkThrows(Function&& function) {
  try {
    std::forward<Function>(function)();
  } catch (const Exception&) {
    return;
  }
  throw TestFailure("expected exception was not thrown");
}

void frameBufferSizesAreCorrect() {
  CHECK(camera::FrameBuffer::requiredBytes({640, 480}, camera::PixelFormat::kRgb888) == 921600U);
  CHECK(camera::FrameBuffer::requiredBytes({640, 480}, camera::PixelFormat::kGray8) == 307200U);
  CHECK(camera::FrameBuffer::requiredBytes({640, 480}, camera::PixelFormat::kYuv420) == 460800U);
}

void frameBufferRejectsInvalidShapes() {
  checkThrows<std::invalid_argument>([] {
    (void)camera::FrameBuffer::requiredBytes({0, 480}, camera::PixelFormat::kRgb888);
  });
  checkThrows<std::invalid_argument>([] {
    (void)camera::FrameBuffer::requiredBytes({641, 480}, camera::PixelFormat::kYuv420);
  });
}

void mockDeviceProducesDeterministicFrame() {
  camera::MockCameraDevice device;
  CHECK(device.open());
  const camera::CaptureRequest request{.frame_number = 7,
                                       .resolution = {4, 2},
                                       .format = camera::PixelFormat::kRgb888};
  camera::DeviceCapture capture = device.capture(request);
  CHECK(capture.buffer->size() == 24U);
  CHECK(capture.buffer->bytes()[0] == 7U);
  CHECK(capture.buffer->bytes()[1] == 8U);
  CHECK(capture.buffer->bytes()[23] == 30U);
  CHECK(std::abs(capture.metadata.exposure_time_ms - request.exposure_time_ms) < 0.0001F);
}

void closedSessionReturnsTypedFailure() {
  camera::CameraSession session(std::make_unique<camera::MockCameraDevice>());
  const camera::CaptureRequest request{.frame_number = 1,
                                       .resolution = {4, 2},
                                       .format = camera::PixelFormat::kRgb888};
  camera::CaptureResult result = session.capture(request);
  CHECK(!result.ok());
  CHECK(result.status == camera::CaptureStatus::kDeviceClosed);
  CHECK(result.buffer == nullptr);
}

void invalidRequestStopsAtSessionBoundary() {
  camera::CameraSession session(std::make_unique<camera::MockCameraDevice>());
  CHECK(session.open());
  camera::CaptureRequest request{.frame_number = 0,
                                 .resolution = {640, 480},
                                 .format = camera::PixelFormat::kRgb888};
  camera::CaptureResult result = session.capture(request);
  CHECK(result.status == camera::CaptureStatus::kInvalidRequest);
  CHECK(result.message.find("frame_number") != std::string::npos);
}

void sessionPreservesOneHundredFrameNumbers() {
  camera::CameraSession session(std::make_unique<camera::MockCameraDevice>());
  CHECK(session.open());
  for (std::uint64_t frame = 1; frame <= 100; ++frame) {
    const camera::CaptureRequest request{.frame_number = frame,
                                         .resolution = {16, 8},
                                         .format = camera::PixelFormat::kGray8};
    camera::CaptureResult result = session.capture(request);
    CHECK(result.ok());
    CHECK(result.frame_number == frame);
    CHECK(result.buffer != nullptr);
    CHECK(result.buffer->size() == 128U);
    CHECK(result.buffer->bytes()[0] == static_cast<std::uint8_t>(frame % 256U));
  }
}

void rejectNewestQueueHonorsCapacity() {
  camera::RequestQueue queue(2, camera::QueueFullPolicy::kRejectNewest);
  CHECK(queue.push({.frame_number = 1}).accepted());
  CHECK(queue.push({.frame_number = 2}).accepted());
  CHECK(queue.push({.frame_number = 3}).status == camera::EnqueueStatus::kRejectedFull);
  CHECK(queue.pop()->frame_number == 1U);
  CHECK(queue.pop()->frame_number == 2U);
  queue.shutdown();
  CHECK(!queue.pop().has_value());
}

void dropOldestQueueReportsDroppedFrame() {
  camera::RequestQueue queue(2, camera::QueueFullPolicy::kDropOldest);
  CHECK(queue.push({.frame_number = 10}).accepted());
  CHECK(queue.push({.frame_number = 11}).accepted());
  const camera::EnqueueResult result = queue.push({.frame_number = 12});
  CHECK(result.status == camera::EnqueueStatus::kAcceptedAfterDroppingOldest);
  CHECK(result.dropped_frame_number == 10U);
  CHECK(queue.pop()->frame_number == 11U);
  CHECK(queue.pop()->frame_number == 12U);
  queue.shutdown();
}

void shutdownWakesBlockedConsumer() {
  camera::RequestQueue queue(1, camera::QueueFullPolicy::kBlock);
  std::atomic<bool> returned{false};
  std::thread consumer([&] {
    CHECK(!queue.pop().has_value());
    returned.store(true);
  });
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  queue.shutdown();
  consumer.join();
  CHECK(returned.load());
}

void asyncSessionDeliversAllAcceptedFrames() {
  std::mutex mutex;
  std::condition_variable received_cv;
  std::vector<std::uint64_t> received;
  camera::AsyncCameraSession session(
      std::make_unique<camera::MockCameraDevice>(), 8, camera::QueueFullPolicy::kBlock,
      [&](camera::CaptureResult result) {
        CHECK(result.ok());
        {
          std::lock_guard lock(mutex);
          received.push_back(result.frame_number);
        }
        received_cv.notify_one();
      });
  CHECK(session.start());
  for (std::uint64_t frame = 1; frame <= 50; ++frame) {
    const camera::SubmitResult submitted = session.submit({
        .frame_number = frame,
        .resolution = {16, 8},
        .format = camera::PixelFormat::kGray8,
    });
    CHECK(submitted.accepted());
  }
  {
    std::unique_lock lock(mutex);
    CHECK(received_cv.wait_for(lock, std::chrono::seconds(2), [&] { return received.size() == 50; }));
  }
  session.shutdown();
  const camera::AsyncStatistics stats = session.statistics();
  CHECK(stats.accepted == 50U);
  CHECK(stats.completed == 50U);
  CHECK(stats.dropped == 0U);
  CHECK(stats.callback_failures == 0U);
  for (std::uint64_t index = 0; index < received.size(); ++index) {
    CHECK(received[index] == index + 1U);
  }
}

void callbackFailureDoesNotKillWorker() {
  std::mutex mutex;
  std::condition_variable cv;
  std::size_t callbacks = 0;
  camera::AsyncCameraSession session(
      std::make_unique<camera::MockCameraDevice>(), 4, camera::QueueFullPolicy::kBlock,
      [&](camera::CaptureResult) {
        bool should_throw = false;
        {
          std::lock_guard lock(mutex);
          ++callbacks;
          should_throw = callbacks == 1;
        }
        cv.notify_one();
        if (should_throw) {
          throw std::runtime_error("simulated consumer failure");
        }
      });
  CHECK(session.start());
  for (std::uint64_t frame = 1; frame <= 3; ++frame) {
    CHECK(session.submit({.frame_number = frame,
                          .resolution = {4, 2},
                          .format = camera::PixelFormat::kGray8})
              .accepted());
  }
  {
    std::unique_lock lock(mutex);
    CHECK(cv.wait_for(lock, std::chrono::seconds(2), [&] { return callbacks == 3; }));
  }
  session.shutdown();
  CHECK(session.statistics().callback_failures == 1U);
  CHECK(session.statistics().completed == 3U);
}

void bufferPoolReusesAndTracksLeases() {
  camera::BufferPool pool(2, {8, 4}, camera::PixelFormat::kGray8);
  auto first = pool.acquire();
  auto second = pool.acquire();
  CHECK(first != nullptr);
  CHECK(second != nullptr);
  CHECK(first->id() != second->id());
  CHECK(pool.tryAcquire() == nullptr);
  const std::uint64_t released_id = first->id();
  first.reset();
  auto reused = pool.acquire();
  CHECK(reused->id() == released_id);
  const camera::BufferPoolStatistics stats = pool.statistics();
  CHECK(stats.capacity == 2U);
  CHECK(stats.high_water_mark == 2U);
  CHECK(stats.acquisitions == 3U);
}

void poolLeaseCanOutlivePoolObject() {
  std::shared_ptr<camera::FrameBuffer> lease;
  {
    camera::BufferPool pool(1, {4, 2}, camera::PixelFormat::kGray8);
    lease = pool.acquire();
  }
  CHECK(lease != nullptr);
  CHECK(lease->size() == 8U);
  lease.reset();
}

void poolShutdownWakesBlockedAcquire() {
  camera::BufferPool pool(1, {4, 2}, camera::PixelFormat::kGray8);
  auto held = pool.acquire();
  std::atomic<bool> returned{false};
  std::thread waiter([&] {
    auto unavailable = pool.acquire();
    CHECK(unavailable == nullptr);
    returned.store(true);
  });
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  pool.shutdown();
  waiter.join();
  CHECK(returned.load());
}

void grayscaleUsesExpectedLumaWeights() {
  camera::FrameBuffer rgb(1, {3, 1}, camera::PixelFormat::kRgb888);
  auto bytes = rgb.bytes();
  bytes[0] = 255; bytes[1] = 0; bytes[2] = 0;
  bytes[3] = 0; bytes[4] = 255; bytes[5] = 0;
  bytes[6] = 0; bytes[7] = 0; bytes[8] = 255;
  auto gray = camera::processing::rgbToGrayscale(rgb, 2);
  CHECK(gray->bytes()[0] == 77U);
  CHECK(gray->bytes()[1] == 149U);
  CHECK(gray->bytes()[2] == 29U);
}

void gammaAndResizeProduceValidFrames() {
  camera::FrameBuffer gray(1, {2, 2}, camera::PixelFormat::kGray8);
  gray.bytes()[0] = 0; gray.bytes()[1] = 64; gray.bytes()[2] = 128; gray.bytes()[3] = 255;
  camera::processing::gammaCorrect(gray, 2.0F);
  CHECK(gray.bytes()[0] == 0U);
  CHECK(gray.bytes()[3] == 255U);
  auto resized = camera::processing::resizeBilinear(gray, {4, 4}, 2);
  CHECK((resized->resolution() == camera::Resolution{4, 4}));
  CHECK(resized->size() == 16U);
}

void metricsComputePercentilesAndFps() {
  camera::MetricsCollector metrics;
  for (int value = 1; value <= 100; ++value) {
    metrics.recordCapture(std::chrono::milliseconds(value));
  }
  const camera::MetricsSnapshot snapshot = metrics.snapshot();
  CHECK(snapshot.frames == 100U);
  CHECK(std::abs(snapshot.capture_latency.p50_ms - 50.5) < 0.01);
  CHECK(std::abs(snapshot.capture_latency.p95_ms - 95.05) < 0.01);
  CHECK(std::abs(snapshot.capture_latency.p99_ms - 99.01) < 0.01);
}

#ifdef MCH_HAS_OPENCV
void opencvAdapterHandlesMissingVideo() {
  auto device = camera::OpenCVCameraDevice::videoFile("definitely-missing-video-file.mp4");
  CHECK(device->name().find("definitely-missing") != std::string::npos);
  CHECK(!device->open());
}
#endif

}  // namespace

int main() {
  const std::vector<std::pair<std::string, std::function<void()>>> tests{
      {"FrameBuffer sizes", frameBufferSizesAreCorrect},
      {"FrameBuffer invalid shapes", frameBufferRejectsInvalidShapes},
      {"Mock deterministic frame", mockDeviceProducesDeterministicFrame},
      {"Closed session failure", closedSessionReturnsTypedFailure},
      {"Session request validation", invalidRequestStopsAtSessionBoundary},
      {"Sequential frame identity", sessionPreservesOneHundredFrameNumbers},
      {"Reject-newest queue capacity", rejectNewestQueueHonorsCapacity},
      {"Drop-oldest queue behavior", dropOldestQueueReportsDroppedFrame},
      {"Shutdown wakes consumer", shutdownWakesBlockedConsumer},
      {"Async ordered delivery", asyncSessionDeliversAllAcceptedFrames},
      {"Callback failure containment", callbackFailureDoesNotKillWorker},
      {"Buffer pool lease reuse", bufferPoolReusesAndTracksLeases},
      {"Lease outlives pool", poolLeaseCanOutlivePoolObject},
      {"Pool shutdown wakes acquirer", poolShutdownWakesBlockedAcquire},
      {"RGB grayscale conversion", grayscaleUsesExpectedLumaWeights},
      {"Gamma and bilinear resize", gammaAndResizeProduceValidFrames},
      {"Latency percentiles", metricsComputePercentilesAndFps},
#ifdef MCH_HAS_OPENCV
      {"OpenCV missing-file behavior", opencvAdapterHandlesMissingVideo},
#endif
  };

  std::size_t failures = 0;
  for (const auto& [name, test] : tests) {
    try {
      test();
      std::cout << "[PASS] " << name << '\n';
    } catch (const std::exception& error) {
      ++failures;
      std::cerr << "[FAIL] " << name << ": " << error.what() << '\n';
    }
  }
  std::cout << (tests.size() - failures) << '/' << tests.size() << " tests passed\n";
  return failures == 0 ? 0 : 1;
}
