#include "camera/BufferPool.h"

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>

namespace camera {

struct BufferPool::State {
  State(std::size_t requested_capacity, Resolution requested_resolution, PixelFormat requested_format)
      : capacity(requested_capacity), resolution(requested_resolution), format(requested_format) {
    storage.reserve(capacity);
    for (std::size_t index = 0; index < capacity; ++index) {
      storage.push_back(std::make_unique<FrameBuffer>(index + 1U, resolution, format));
      available.push_back(storage.back().get());
    }
  }

  const std::size_t capacity;
  const Resolution resolution;
  const PixelFormat format;
  std::vector<std::unique_ptr<FrameBuffer>> storage;
  std::deque<FrameBuffer*> available;
  mutable std::mutex mutex;
  std::condition_variable cv;
  bool shutdown{false};
  std::size_t high_water_mark{};
  std::uint64_t acquisitions{};
  std::uint64_t waits{};
};

BufferPool::BufferPool(std::size_t capacity, Resolution resolution, PixelFormat format) {
  if (capacity == 0) {
    throw std::invalid_argument("buffer pool capacity must be greater than zero");
  }
  (void)FrameBuffer::requiredBytes(resolution, format);
  state_ = std::make_shared<State>(capacity, resolution, format);
}

BufferPool::~BufferPool() { shutdown(); }

std::shared_ptr<FrameBuffer> BufferPool::acquire() {
  const std::shared_ptr<State> state = state_;
  std::unique_lock lock(state->mutex);
  if (state->available.empty() && !state->shutdown) {
    ++state->waits;
  }
  state->cv.wait(lock, [&] { return state->shutdown || !state->available.empty(); });
  if (state->shutdown) {
    return {};
  }
  FrameBuffer* buffer = state->available.front();
  state->available.pop_front();
  ++state->acquisitions;
  const std::size_t in_use = state->capacity - state->available.size();
  state->high_water_mark = std::max(state->high_water_mark, in_use);
  lock.unlock();
  return std::shared_ptr<FrameBuffer>(buffer, [state](FrameBuffer* returned) {
    {
      std::lock_guard release_lock(state->mutex);
      state->available.push_back(returned);
    }
    state->cv.notify_one();
  });
}

std::shared_ptr<FrameBuffer> BufferPool::tryAcquire() {
  const std::shared_ptr<State> state = state_;
  std::unique_lock lock(state->mutex);
  if (state->shutdown || state->available.empty()) {
    return {};
  }
  FrameBuffer* buffer = state->available.front();
  state->available.pop_front();
  ++state->acquisitions;
  const std::size_t in_use = state->capacity - state->available.size();
  state->high_water_mark = std::max(state->high_water_mark, in_use);
  lock.unlock();
  return std::shared_ptr<FrameBuffer>(buffer, [state](FrameBuffer* returned) {
    {
      std::lock_guard release_lock(state->mutex);
      state->available.push_back(returned);
    }
    state->cv.notify_one();
  });
}

void BufferPool::shutdown() noexcept {
  if (!state_) {
    return;
  }
  {
    std::lock_guard lock(state_->mutex);
    state_->shutdown = true;
  }
  state_->cv.notify_all();
}

Resolution BufferPool::resolution() const noexcept { return state_->resolution; }

PixelFormat BufferPool::format() const noexcept { return state_->format; }

BufferPoolStatistics BufferPool::statistics() const {
  std::lock_guard lock(state_->mutex);
  return {
      .capacity = state_->capacity,
      .available = state_->available.size(),
      .in_use = state_->capacity - state_->available.size(),
      .high_water_mark = state_->high_water_mark,
      .acquisitions = state_->acquisitions,
      .waits = state_->waits,
  };
}

}  // namespace camera
