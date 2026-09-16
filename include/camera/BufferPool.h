#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "camera/FrameBuffer.h"

namespace camera {

struct BufferPoolStatistics {
  std::size_t capacity{};
  std::size_t available{};
  std::size_t in_use{};
  std::size_t high_water_mark{};
  std::uint64_t acquisitions{};
  std::uint64_t waits{};
};

class BufferPool {
 public:
  BufferPool(std::size_t capacity, Resolution resolution, PixelFormat format);
  ~BufferPool();

  BufferPool(const BufferPool&) = delete;
  BufferPool& operator=(const BufferPool&) = delete;

  [[nodiscard]] std::shared_ptr<FrameBuffer> acquire();
  [[nodiscard]] std::shared_ptr<FrameBuffer> tryAcquire();
  void shutdown() noexcept;

  [[nodiscard]] Resolution resolution() const noexcept;
  [[nodiscard]] PixelFormat format() const noexcept;
  [[nodiscard]] BufferPoolStatistics statistics() const;

 private:
  struct State;
  std::shared_ptr<State> state_;
};

}  // namespace camera
