#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>

namespace camera {

struct LatencySummary {
  double mean_ms{};
  double p50_ms{};
  double p95_ms{};
  double p99_ms{};
  double max_ms{};
};

struct MetricsSnapshot {
  std::uint64_t frames{};
  std::size_t sample_count{};
  std::size_t sample_capacity{};
  double elapsed_seconds{};
  double fps{};
  LatencySummary capture_latency;
};

class MetricsCollector {
 public:
  static constexpr std::size_t kDefaultSampleCapacity = 4096;

  explicit MetricsCollector(std::size_t sample_capacity = kDefaultSampleCapacity);
  void recordCapture(std::chrono::nanoseconds latency);
  [[nodiscard]] MetricsSnapshot snapshot() const;
  void reset();

 private:
  mutable std::mutex mutex_;
  const std::size_t sample_capacity_;
  std::deque<std::chrono::nanoseconds> capture_latencies_;
  std::uint64_t frames_{};
  std::chrono::steady_clock::time_point first_record_{};
  std::chrono::steady_clock::time_point last_record_{};
};

}  // namespace camera
