#include <algorithm>
#include <charconv>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <string_view>
#include <string>

#include "camera/AsyncCameraSession.h"
#include "camera/MockCameraDevice.h"
#include "camera/BufferPool.h"
#ifdef MCH_HAS_OPENCV
#include "camera/OpenCVCameraDevice.h"
#endif

namespace {

std::uint64_t checksum(const camera::FrameBuffer& buffer) {
  std::uint64_t value = 1469598103934665603ULL;
  for (std::uint8_t byte : buffer.bytes()) {
    value ^= byte;
    value *= 1099511628211ULL;
  }
  return value;
}

struct Options {
  std::uint32_t frames{5};
  std::string source{"mock"};
  int device_index{};
  std::string video_path;
};

template <typename Integer>
Integer parseInteger(std::string_view text, std::string_view label) {
  Integer value{};
  const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
  if (error != std::errc{} || end != text.data() + text.size()) {
    throw std::invalid_argument(std::string(label) + " must be an integer");
  }
  return value;
}

Options parseOptions(int argc, char** argv) {
  Options options;
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument(argv[index]);
    if (argument == "--frames" && index + 1 < argc) {
      options.frames = parseInteger<std::uint32_t>(argv[++index], "frame count");
    } else if (argument == "--source" && index + 1 < argc) {
      options.source = argv[++index];
    } else if (argument == "--device" && index + 1 < argc) {
      options.device_index = parseInteger<int>(argv[++index], "device index");
    } else if (argument == "--video" && index + 1 < argc) {
      options.video_path = argv[++index];
      options.source = "video";
    } else {
      throw std::invalid_argument(
          "usage: mini_camera_hal [--frames N] [--source mock|webcam] [--device N] [--video FILE]");
    }
  }
  if (options.frames == 0 || options.frames > 1000) {
    throw std::invalid_argument("frame count must be within [1, 1000]");
  }
  if (options.source != "mock" && options.source != "webcam" && options.source != "video") {
    throw std::invalid_argument("source must be mock, webcam, or video");
  }
  if (options.source == "video" && options.video_path.empty()) {
    throw std::invalid_argument("video source requires --video FILE");
  }
  return options;
}

std::unique_ptr<camera::ICameraDevice> makeDevice(const Options& options) {
  if (options.source == "mock") {
    return std::make_unique<camera::MockCameraDevice>();
  }
#ifdef MCH_HAS_OPENCV
  if (options.source == "webcam") {
    return camera::OpenCVCameraDevice::webcam(options.device_index);
  }
  return camera::OpenCVCameraDevice::videoFile(options.video_path);
#else
  (void)options;
  throw std::runtime_error("webcam/video support requires a CMake build with OpenCV");
#endif
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const Options options = parseOptions(argc, argv);
    const std::uint32_t frame_count = options.frames;
    std::mutex completion_mutex;
    std::condition_variable completion_cv;
    std::uint32_t completed = 0;
    bool capture_failed = false;
    auto buffer_pool =
        std::make_shared<camera::BufferPool>(8, camera::Resolution{640, 480},
                                             camera::PixelFormat::kRgb888);

    camera::AsyncCameraSession session(
        makeDevice(options), buffer_pool, 8,
        camera::QueueFullPolicy::kBlock,
        [&](camera::CaptureResult result) {
          if (!result.ok()) {
            std::cerr << "Frame " << result.frame_number << " failed: " << result.message << '\n';
            capture_failed = true;
          } else {
            const double latency_ms =
                std::chrono::duration<double, std::milli>(result.capture_latency).count();
            std::cout << "frame=" << result.frame_number << " buffer=" << result.buffer->id()
                      << " bytes=" << result.buffer->size() << " latency_ms=" << std::fixed
                      << std::setprecision(3) << latency_ms
                      << " checksum=" << checksum(*result.buffer) << '\n';
          }
          {
            std::lock_guard lock(completion_mutex);
            ++completed;
          }
          completion_cv.notify_one();
        });
    if (!session.start()) {
      std::cerr << "Failed to open camera device\n";
      return 1;
    }

    std::cout << "Mini Camera HAL - bounded asynchronous capture\n"
              << "Source: " << options.source << "\n"
              << "Input: 640x480 RGB888\n"
              << "Queue: capacity=8 policy=block\n\n";

    for (std::uint64_t frame = 1; frame <= frame_count; ++frame) {
      camera::CaptureRequest request{
          .frame_number = frame,
          .resolution = {640, 480},
          .format = camera::PixelFormat::kRgb888,
          .exposure_time_ms = 8.0F,
          .sensor_gain = 1.0F,
      };
      if (!session.submit(request).accepted()) {
        std::cerr << "Frame " << frame << " was not accepted\n";
        session.shutdown();
        return 2;
      }
    }
    {
      std::unique_lock lock(completion_mutex);
      if (!completion_cv.wait_for(lock, std::chrono::seconds(10),
                                  [&] { return completed == frame_count; })) {
        std::cerr << "Timed out waiting for capture results\n";
        session.shutdown();
        return 3;
      }
    }
    session.shutdown();
    const camera::AsyncStatistics stats = session.statistics();
    const camera::MetricsSnapshot metrics = session.metrics();
    const camera::BufferPoolStatistics pool_stats = buffer_pool->statistics();
    std::cout << "\nsubmitted=" << stats.accepted << " completed=" << stats.completed
              << " rejected=" << stats.rejected << " dropped=" << stats.dropped
              << " callback_failures=" << stats.callback_failures << '\n'
              << "fps=" << metrics.fps << " latency_mean_ms=" << metrics.capture_latency.mean_ms
              << " p50_ms=" << metrics.capture_latency.p50_ms
              << " p95_ms=" << metrics.capture_latency.p95_ms
              << " p99_ms=" << metrics.capture_latency.p99_ms << '\n'
              << "pool_capacity=" << pool_stats.capacity
              << " pool_high_water_mark=" << pool_stats.high_water_mark
              << " pool_acquisitions=" << pool_stats.acquisitions << '\n';
    return capture_failed ? 2 : 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 64;
  }
}
