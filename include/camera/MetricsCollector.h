#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <vector>

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
  double elapsed_seconds{};
  double fps{};
  LatencySummary capture_latency;
};

class MetricsCollector {
 public:
  void recordCapture(std::chrono::nanoseconds latency);
  [[nodiscard]] MetricsSnapshot snapshot() const;
  void reset();

 private:
  mutable std::mutex mutex_;
  std::vector<std::chrono::nanoseconds> capture_latencies_;
  std::chrono::steady_clock::time_point first_record_{};
  std::chrono::steady_clock::time_point last_record_{};
};

}  // namespace camera
