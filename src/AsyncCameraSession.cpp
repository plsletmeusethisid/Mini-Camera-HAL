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
  std::lock_guard lock(lifecycle_mutex_);
  if (state_.load() != SessionState::kCreated) {
    return false;
  }
  state_.store(SessionState::kStarting);
  if (!session_.open()) {
    state_.store(SessionState::kFailed);
    return false;
  }
  try {
    worker_ = std::thread(&AsyncCameraSession::workerLoop, this);
    worker_id_ = worker_.get_id();
    state_.store(SessionState::kRunning);
  } catch (...) {
    session_.close();
    state_.store(SessionState::kFailed);
    throw;
  }
  return true;
}

SubmitResult AsyncCameraSession::submit(CaptureRequest request) {
  if (state_.load() != SessionState::kRunning) {
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
  bool called_from_worker = false;
  {
    std::lock_guard lock(lifecycle_mutex_);
    const SessionState current = state_.load();
    if (current == SessionState::kCreated || current == SessionState::kFailed) {
      state_.store(SessionState::kStopped);
    } else if (current == SessionState::kStarting || current == SessionState::kRunning) {
      state_.store(SessionState::kStopping);
    }
    called_from_worker = worker_id_ != std::thread::id{} &&
                         worker_id_ == std::this_thread::get_id();
  }

  queue_.shutdown();
  session_.cancelPending();

  // A callback runs on the worker. It may request shutdown, but it must never
  // attempt to join itself. A later external shutdown (or the destructor)
  // performs the join after the worker loop exits.
  if (called_from_worker) {
    return;
  }

  std::unique_lock lock(lifecycle_mutex_);
  lifecycle_cv_.wait(lock, [this] { return !join_in_progress_; });
  if (worker_.joinable()) {
    join_in_progress_ = true;
    lock.unlock();
    worker_.join();
    lock.lock();
    join_in_progress_ = false;
    worker_id_ = {};
  }
  session_.close();
  state_.store(SessionState::kStopped);
  lock.unlock();
  lifecycle_cv_.notify_all();
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
  session_.close();
  state_.store(SessionState::kStopped);
  lifecycle_cv_.notify_all();
}

}  // namespace camera
