#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "camera/Types.h"

namespace camera {

class FrameBuffer {
 public:
  FrameBuffer(std::uint64_t id, Resolution resolution, PixelFormat format);

  FrameBuffer(const FrameBuffer&) = delete;
  FrameBuffer& operator=(const FrameBuffer&) = delete;
  FrameBuffer(FrameBuffer&&) noexcept = default;
  FrameBuffer& operator=(FrameBuffer&&) noexcept = default;
  ~FrameBuffer() = default;

  [[nodiscard]] std::uint64_t id() const noexcept { return id_; }
  [[nodiscard]] Resolution resolution() const noexcept { return resolution_; }
  [[nodiscard]] PixelFormat format() const noexcept { return format_; }
  [[nodiscard]] std::size_t size() const noexcept { return bytes_.size(); }
  [[nodiscard]] std::span<std::uint8_t> bytes() noexcept { return bytes_; }
  [[nodiscard]] std::span<const std::uint8_t> bytes() const noexcept { return bytes_; }

  [[nodiscard]] static std::size_t requiredBytes(Resolution resolution, PixelFormat format);

 private:
  std::uint64_t id_;
  Resolution resolution_;
  PixelFormat format_;
  std::vector<std::uint8_t> bytes_;
};

}  // namespace camera
