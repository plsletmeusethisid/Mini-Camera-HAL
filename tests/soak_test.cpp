#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>

#include "camera/AsyncCameraSession.h"
#include "camera/BufferPool.h"
#include "camera/MockCameraDevice.h"

int main() {
  constexpr std::uint64_t kFrameCount = 10'000;
  auto pool = std::make_shared<camera::BufferPool>(8, camera::Resolution{16, 8},
                                                   camera::PixelFormat::kGray8);
  std::mutex mutex;
  std::condition_variable cv;
  std::uint64_t callbacks = 0;
  std::atomic<std::uint64_t> failures{0};
  std::atomic<std::uint64_t> next_expected{1};

  camera::AsyncCameraSession session(
      std::make_unique<camera::MockCameraDevice>(), pool, 64,
      camera::QueueFullPolicy::kBlock, [&](camera::CaptureResult result) {
        const std::uint64_t expected = next_expected.fetch_add(1);
        if (!result.ok() || result.frame_number != expected || result.buffer == nullptr ||
            result.buffer->size() != 128U) {
          failures.fetch_add(1);
        }
        {
          std::lock_guard lock(mutex);
          ++callbacks;
        }
        cv.notify_one();
      });

  const auto start = std::chrono::steady_clock::now();
  if (!session.start()) {
    std::cerr << "soak: session failed to start\n";
    return 1;
  }
  for (std::uint64_t frame = 1; frame <= kFrameCount; ++frame) {
    if (!session.submit({.frame_number = frame,
                         .resolution = {16, 8},
                         .format = camera::PixelFormat::kGray8})
             .accepted()) {
      std::cerr << "soak: submission rejected at frame " << frame << '\n';
      session.shutdown();
      return 2;
    }
  }
  {
    std::unique_lock lock(mutex);
    if (!cv.wait_for(lock, std::chrono::seconds(30), [&] { return callbacks == kFrameCount; })) {
      std::cerr << "soak: timed out after " << callbacks << " callbacks\n";
      session.shutdown();
      return 3;
    }
  }
  session.shutdown();

  const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start);
  const camera::AsyncStatistics stats = session.statistics();
  const camera::MetricsSnapshot metrics = session.metrics();
  const camera::BufferPoolStatistics pool_stats = pool->statistics();
  const bool valid = failures.load() == 0U && stats.accepted == kFrameCount &&
                     stats.completed == kFrameCount && stats.rejected == 0U &&
                     stats.dropped == 0U && stats.callback_failures == 0U &&
                     metrics.frames == kFrameCount &&
                     metrics.sample_count <= metrics.sample_capacity &&
                     metrics.sample_capacity == camera::MetricsCollector::kDefaultSampleCapacity &&
                     pool_stats.high_water_mark <= pool_stats.capacity &&
                     session.state() == camera::SessionState::kStopped;

  std::cout << "soak_frames=" << kFrameCount << " callbacks=" << callbacks
            << " failures=" << failures.load() << " elapsed_seconds=" << elapsed.count()
            << " metrics_samples=" << metrics.sample_count << '/' << metrics.sample_capacity
            << " pool_high_water=" << pool_stats.high_water_mark << '/' << pool_stats.capacity
            << '\n';
  return valid ? 0 : 4;
}
