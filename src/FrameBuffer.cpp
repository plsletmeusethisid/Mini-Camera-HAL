#include "camera/FrameBuffer.h"

#include <limits>
#include <stdexcept>

namespace camera {
namespace {

std::size_t checkedPixelCount(Resolution resolution) {
  if (resolution.width == 0 || resolution.height == 0) {
    throw std::invalid_argument("frame dimensions must be non-zero");
  }
  const auto width = static_cast<std::size_t>(resolution.width);
  const auto height = static_cast<std::size_t>(resolution.height);
  if (width > std::numeric_limits<std::size_t>::max() / height) {
    throw std::overflow_error("frame dimensions overflow addressable memory");
  }
  return width * height;
}

}  // namespace

FrameBuffer::FrameBuffer(std::uint64_t id, Resolution resolution, PixelFormat format)
    : id_(id), resolution_(resolution), format_(format), bytes_(requiredBytes(resolution, format)) {}

std::size_t FrameBuffer::requiredBytes(Resolution resolution, PixelFormat format) {
  const std::size_t pixels = checkedPixelCount(resolution);
  switch (format) {
    case PixelFormat::kRgb888:
      if (pixels > std::numeric_limits<std::size_t>::max() / 3U) {
        throw std::overflow_error("RGB frame size overflows addressable memory");
      }
      return pixels * 3U;
    case PixelFormat::kGray8:
      return pixels;
    case PixelFormat::kYuv420:
      if ((resolution.width % 2U) != 0U || (resolution.height % 2U) != 0U) {
        throw std::invalid_argument("YUV420 dimensions must be even");
      }
      if (pixels > std::numeric_limits<std::size_t>::max() / 3U) {
        throw std::overflow_error("YUV frame size overflows addressable memory");
      }
      return (pixels * 3U) / 2U;
  }
  throw std::invalid_argument("unsupported pixel format");
}

}  // namespace camera
