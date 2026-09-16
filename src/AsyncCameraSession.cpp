#include "camera/AsyncCameraSession.h"

#include <exception>
#include <stdexcept>
#include <utility>

namespace camera {

AsyncCameraSession::AsyncCameraSession(std::unique_ptr<ICameraDevice> device,
                                       std::size_t queue_capacity, QueueFullPolicy full_policy,
                                       ResultCallback callback)
    : session_(std::move(device)),
      queue_(queue_capacity, full_policy),
      callback_(std::move(callback)) {
  if (!callback_) {
    throw std::invalid_argument("async camera session requires a result callback");
  }
}

AsyncCameraSession::AsyncCameraSession(std::unique_ptr<ICameraDevice> device,
                                       std::shared_ptr<BufferPool> buffer_pool,
                                       std::size_t queue_capacity, QueueFullPolicy full_policy,
                                       ResultCallback callback)
    : session_(std::move(device), std::move(buffer_pool)),
      queue_(queue_capacity, full_policy),
      callback_(std::move(callback)) {
  if (!callback_) {
    throw std::invalid_argument("async camera session requires a result callback");
  }
}

AsyncCameraSession::~AsyncCameraSession() { shutdown(); }

bool AsyncCameraSession::start() {
  bool expected = false;
  if (!running_.compare_exchange_strong(expected, true)) {
    return false;
  }
  if (!session_.open()) {
    running_.store(false);
    return false;
  }
  try {
    worker_ = std::thread(&AsyncCameraSession::workerLoop, this);
  } catch (...) {
    session_.close();
    running_.store(false);
    throw;
  }
  return true;
}

SubmitResult AsyncCameraSession::submit(CaptureRequest request) {
  if (!running_.load()) {
    rejected_.fetch_add(1);
    return {.status = SubmitStatus::kStopped, .dropped_frame_number = std::nullopt};
  }
  if (!CameraSession::validate(request).empty()) {
    rejected_.fetch_add(1);
    return {.status = SubmitStatus::kInvalidRequest, .dropped_frame_number = std::nullopt};
  }

  EnqueueResult enqueue = queue_.push(std::move(request));
  switch (enqueue.status) {
    case EnqueueStatus::kAccepted:
      accepted_.fetch_add(1);
      return {.status = SubmitStatus::kAccepted, .dropped_frame_number = std::nullopt};
    case EnqueueStatus::kAcceptedAfterDroppingOldest:
      accepted_.fetch_add(1);
      dropped_.fetch_add(1);
      return {.status = SubmitStatus::kAcceptedAfterDroppingOldest,
              .dropped_frame_number = enqueue.dropped_frame_number};
    case EnqueueStatus::kRejectedFull:
      rejected_.fetch_add(1);
      return {.status = SubmitStatus::kQueueFull, .dropped_frame_number = std::nullopt};
    case EnqueueStatus::kShutdown:
      rejected_.fetch_add(1);
      return {.status = SubmitStatus::kStopped, .dropped_frame_number = std::nullopt};
  }
  rejected_.fetch_add(1);
  return {.status = SubmitStatus::kStopped, .dropped_frame_number = std::nullopt};
}

void AsyncCameraSession::shutdown() noexcept {
  if (!running_.exchange(false)) {
    return;
  }
  queue_.shutdown();
  if (worker_.joinable()) {
    worker_.join();
  }
  session_.close();
}

AsyncStatistics AsyncCameraSession::statistics() const noexcept {
  return {
      .accepted = accepted_.load(),
      .completed = completed_.load(),
      .rejected = rejected_.load(),
      .dropped = dropped_.load(),
      .callback_failures = callback_failures_.load(),
  };
}

void AsyncCameraSession::workerLoop() noexcept {
  while (std::optional<CaptureRequest> request = queue_.pop()) {
    CaptureResult result = session_.capture(*request);
    metrics_.recordCapture(result.capture_latency);
    completed_.fetch_add(1);
    try {
      callback_(std::move(result));
    } catch (...) {
      callback_failures_.fetch_add(1);
    }
  }
}

}  // namespace camera
