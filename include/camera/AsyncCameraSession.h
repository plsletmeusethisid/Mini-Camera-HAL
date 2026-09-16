#pragma once

#include <atomic>
#include <cstddef>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

#include "camera/CameraSession.h"
#include "camera/RequestQueue.h"
#include "camera/MetricsCollector.h"

namespace camera {

enum class SubmitStatus { kAccepted, kAcceptedAfterDroppingOldest, kInvalidRequest, kQueueFull, kStopped };

enum class SessionState { kCreated, kStarting, kRunning, kStopping, kStopped, kFailed };

struct SubmitResult {
  SubmitStatus status{SubmitStatus::kStopped};
  std::optional<std::uint64_t> dropped_frame_number;

  [[nodiscard]] bool accepted() const noexcept {
    return status == SubmitStatus::kAccepted ||
           status == SubmitStatus::kAcceptedAfterDroppingOldest;
  }
};

struct AsyncStatistics {
  std::uint64_t accepted{};
  std::uint64_t completed{};
  std::uint64_t rejected{};
  std::uint64_t dropped{};
  std::uint64_t callback_failures{};
};

class AsyncCameraSession {
 public:
  using ResultCallback = std::function<void(CaptureResult)>;

  AsyncCameraSession(std::unique_ptr<ICameraDevice> device, std::size_t queue_capacity,
                     QueueFullPolicy full_policy, ResultCallback callback);
  AsyncCameraSession(std::unique_ptr<ICameraDevice> device, std::shared_ptr<BufferPool> buffer_pool,
                     std::size_t queue_capacity, QueueFullPolicy full_policy, ResultCallback callback);
  ~AsyncCameraSession();

  AsyncCameraSession(const AsyncCameraSession&) = delete;
  AsyncCameraSession& operator=(const AsyncCameraSession&) = delete;

  [[nodiscard]] bool start();
  [[nodiscard]] SubmitResult submit(CaptureRequest request);
  void shutdown() noexcept;

  [[nodiscard]] bool isRunning() const noexcept { return state() == SessionState::kRunning; }
  [[nodiscard]] SessionState state() const noexcept { return state_.load(); }
  [[nodiscard]] AsyncStatistics statistics() const noexcept;
  [[nodiscard]] MetricsSnapshot metrics() const { return metrics_.snapshot(); }

 private:
  void workerLoop() noexcept;

  CameraSession session_;
  RequestQueue queue_;
  ResultCallback callback_;
  std::thread worker_;
  std::atomic<SessionState> state_{SessionState::kCreated};
  mutable std::mutex lifecycle_mutex_;
  std::condition_variable lifecycle_cv_;
  std::thread::id worker_id_{};
  bool join_in_progress_{false};
  std::atomic<std::uint64_t> accepted_{0};
  std::atomic<std::uint64_t> completed_{0};
  std::atomic<std::uint64_t> rejected_{0};
  std::atomic<std::uint64_t> dropped_{0};
  std::atomic<std::uint64_t> callback_failures_{0};
  MetricsCollector metrics_;
};

}  // namespace camera
