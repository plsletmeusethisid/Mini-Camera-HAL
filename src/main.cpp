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

#include "camera/AsyncCameraSession.h"
#include "camera/MockCameraDevice.h"

namespace {

std::uint64_t checksum(const camera::FrameBuffer& buffer) {
  std::uint64_t value = 1469598103934665603ULL;
  for (std::uint8_t byte : buffer.bytes()) {
    value ^= byte;
    value *= 1099511628211ULL;
  }
  return value;
}

std::uint32_t parseFrameCount(int argc, char** argv) {
  if (argc == 1) {
    return 5;
  }
  if (argc != 3 || std::string_view(argv[1]) != "--frames") {
    throw std::invalid_argument("usage: mini_camera_hal [--frames N]");
  }
  std::uint32_t count{};
  const std::string_view text(argv[2]);
  const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), count);
  if (error != std::errc{} || end != text.data() + text.size() || count == 0 || count > 1000) {
    throw std::invalid_argument("frame count must be an integer within [1, 1000]");
  }
  return count;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const std::uint32_t frame_count = parseFrameCount(argc, argv);
    std::mutex completion_mutex;
    std::condition_variable completion_cv;
    std::uint32_t completed = 0;
    bool capture_failed = false;

    camera::AsyncCameraSession session(
        std::make_unique<camera::MockCameraDevice>(), 8, camera::QueueFullPolicy::kBlock,
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
              << "Device: Deterministic Mock Camera\n"
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
    std::cout << "\nsubmitted=" << stats.accepted << " completed=" << stats.completed
              << " rejected=" << stats.rejected << " dropped=" << stats.dropped
              << " callback_failures=" << stats.callback_failures << '\n';
    return capture_failed ? 2 : 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 64;
  }
}
