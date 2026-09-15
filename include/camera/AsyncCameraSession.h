#pragma once

#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

#include "camera/CameraSession.h"
#include "camera/RequestQueue.h"

namespace camera {

enum class SubmitStatus { kAccepted, kAcceptedAfterDroppingOldest, kInvalidRequest, kQueueFull, kStopped };

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
  ~AsyncCameraSession();

  AsyncCameraSession(const AsyncCameraSession&) = delete;
  AsyncCameraSession& operator=(const AsyncCameraSession&) = delete;

  [[nodiscard]] bool start();
  [[nodiscard]] SubmitResult submit(CaptureRequest request);
  void shutdown() noexcept;

  [[nodiscard]] bool isRunning() const noexcept { return running_.load(); }
  [[nodiscard]] AsyncStatistics statistics() const noexcept;

 private:
  void workerLoop() noexcept;

  CameraSession session_;
  RequestQueue queue_;
  ResultCallback callback_;
  std::thread worker_;
  std::atomic<bool> running_{false};
  std::atomic<std::uint64_t> accepted_{0};
  std::atomic<std::uint64_t> completed_{0};
  std::atomic<std::uint64_t> rejected_{0};
  std::atomic<std::uint64_t> dropped_{0};
  std::atomic<std::uint64_t> callback_failures_{0};
};

}  // namespace camera
