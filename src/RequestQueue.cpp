#include "camera/RequestQueue.h"

#include <stdexcept>
#include <utility>

namespace camera {

RequestQueue::RequestQueue(std::size_t capacity, QueueFullPolicy full_policy)
    : capacity_(capacity), full_policy_(full_policy) {
  if (capacity == 0) {
    throw std::invalid_argument("request queue capacity must be greater than zero");
  }
}

EnqueueResult RequestQueue::push(CaptureRequest request) {
  std::unique_lock lock(mutex_);
  if (full_policy_ == QueueFullPolicy::kBlock) {
    not_full_.wait(lock, [this] { return shutdown_ || requests_.size() < capacity_; });
  }
  if (shutdown_) {
    return {.status = EnqueueStatus::kShutdown, .dropped_frame_number = std::nullopt};
  }

  EnqueueResult result{.status = EnqueueStatus::kAccepted, .dropped_frame_number = std::nullopt};
  if (requests_.size() == capacity_) {
    if (full_policy_ == QueueFullPolicy::kRejectNewest) {
      return {.status = EnqueueStatus::kRejectedFull, .dropped_frame_number = std::nullopt};
    }
    result.status = EnqueueStatus::kAcceptedAfterDroppingOldest;
    result.dropped_frame_number = requests_.front().frame_number;
    requests_.pop_front();
  }
  requests_.push_back(std::move(request));
  lock.unlock();
  not_empty_.notify_one();
  return result;
}

std::optional<CaptureRequest> RequestQueue::pop() {
  std::unique_lock lock(mutex_);
  not_empty_.wait(lock, [this] { return shutdown_ || !requests_.empty(); });
  if (requests_.empty()) {
    return std::nullopt;
  }
  CaptureRequest request = std::move(requests_.front());
  requests_.pop_front();
  lock.unlock();
  not_full_.notify_one();
  return request;
}

void RequestQueue::shutdown() noexcept {
  {
    std::lock_guard lock(mutex_);
    shutdown_ = true;
  }
  not_empty_.notify_all();
  not_full_.notify_all();
}

std::size_t RequestQueue::size() const {
  std::lock_guard lock(mutex_);
  return requests_.size();
}

bool RequestQueue::isShutdown() const {
  std::lock_guard lock(mutex_);
  return shutdown_;
}

}  // namespace camera
