#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>

#include "camera/Types.h"

namespace camera {

enum class QueueFullPolicy { kBlock, kRejectNewest, kDropOldest };

enum class EnqueueStatus { kAccepted, kAcceptedAfterDroppingOldest, kRejectedFull, kShutdown };

struct EnqueueResult {
  EnqueueStatus status{EnqueueStatus::kShutdown};
  std::optional<std::uint64_t> dropped_frame_number;

  [[nodiscard]] bool accepted() const noexcept {
    return status == EnqueueStatus::kAccepted ||
           status == EnqueueStatus::kAcceptedAfterDroppingOldest;
  }
};

class RequestQueue {
 public:
  RequestQueue(std::size_t capacity, QueueFullPolicy full_policy);

  RequestQueue(const RequestQueue&) = delete;
  RequestQueue& operator=(const RequestQueue&) = delete;

  [[nodiscard]] EnqueueResult push(CaptureRequest request);
  [[nodiscard]] std::optional<CaptureRequest> pop();
  void shutdown() noexcept;

  [[nodiscard]] std::size_t size() const;
  [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }
  [[nodiscard]] bool isShutdown() const;

 private:
  const std::size_t capacity_;
  const QueueFullPolicy full_policy_;
  mutable std::mutex mutex_;
  std::condition_variable not_empty_;
  std::condition_variable not_full_;
  std::deque<CaptureRequest> requests_;
  bool shutdown_{false};
};

}  // namespace camera
