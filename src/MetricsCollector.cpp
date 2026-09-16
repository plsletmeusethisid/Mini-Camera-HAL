#include "camera/MetricsCollector.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace camera {
namespace {

double toMilliseconds(std::chrono::nanoseconds value) {
  return std::chrono::duration<double, std::milli>(value).count();
}

double percentile(const std::vector<std::chrono::nanoseconds>& sorted, double rank) {
  if (sorted.empty()) {
    return 0.0;
  }
  const double position = rank * static_cast<double>(sorted.size() - 1U);
  const std::size_t low = static_cast<std::size_t>(std::floor(position));
  const std::size_t high = static_cast<std::size_t>(std::ceil(position));
  const double weight = position - static_cast<double>(low);
  return toMilliseconds(sorted[low]) * (1.0 - weight) + toMilliseconds(sorted[high]) * weight;
}

}  // namespace

void MetricsCollector::recordCapture(std::chrono::nanoseconds latency) {
  const auto now = std::chrono::steady_clock::now();
  std::lock_guard lock(mutex_);
  if (capture_latencies_.empty()) {
    first_record_ = now;
  }
  last_record_ = now;
  capture_latencies_.push_back(latency);
}

MetricsSnapshot MetricsCollector::snapshot() const {
  std::lock_guard lock(mutex_);
  MetricsSnapshot result;
  result.frames = capture_latencies_.size();
  if (capture_latencies_.empty()) {
    return result;
  }
  std::vector<std::chrono::nanoseconds> sorted = capture_latencies_;
  std::sort(sorted.begin(), sorted.end());
  const auto total = std::accumulate(sorted.begin(), sorted.end(), std::chrono::nanoseconds{});
  result.elapsed_seconds = std::chrono::duration<double>(last_record_ - first_record_).count();
  result.fps = result.elapsed_seconds > 0.0
                   ? static_cast<double>(result.frames - 1U) / result.elapsed_seconds
                   : 0.0;
  result.capture_latency = {
      .mean_ms = toMilliseconds(total) / static_cast<double>(result.frames),
      .p50_ms = percentile(sorted, 0.50),
      .p95_ms = percentile(sorted, 0.95),
      .p99_ms = percentile(sorted, 0.99),
      .max_ms = toMilliseconds(sorted.back()),
  };
  return result;
}

void MetricsCollector::reset() {
  std::lock_guard lock(mutex_);
  capture_latencies_.clear();
  first_record_ = {};
  last_record_ = {};
}

}  // namespace camera
