#include "camera/ImageProcessing.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace camera::processing {

std::unique_ptr<FrameBuffer> rgbToGrayscale(const FrameBuffer& input, std::uint64_t output_id) {
  if (input.format() != PixelFormat::kRgb888) {
    throw std::invalid_argument("grayscale conversion requires RGB888 input");
  }
  auto output = std::make_unique<FrameBuffer>(output_id, input.resolution(), PixelFormat::kGray8);
  rgbToGrayscaleInto(input, *output);
  return output;
}

void rgbToGrayscaleInto(const FrameBuffer& input, FrameBuffer& output) {
  if (input.format() != PixelFormat::kRgb888) {
    throw std::invalid_argument("grayscale conversion requires RGB888 input");
  }
  if (output.format() != PixelFormat::kGray8 || output.resolution() != input.resolution()) {
    throw std::invalid_argument("grayscale output must be matching-resolution GRAY8");
  }
  const auto source = input.bytes();
  auto destination = output.bytes();
  for (std::size_t pixel = 0; pixel < destination.size(); ++pixel) {
    const std::size_t offset = pixel * 3U;
    const std::uint32_t red = source[offset];
    const std::uint32_t green = source[offset + 1U];
    const std::uint32_t blue = source[offset + 2U];
    destination[pixel] =
        static_cast<std::uint8_t>((77U * red + 150U * green + 29U * blue + 128U) >> 8U);
  }
}

void gammaCorrect(FrameBuffer& buffer, float gamma) {
  if (buffer.format() == PixelFormat::kYuv420) {
    throw std::invalid_argument("gamma correction does not support YUV420");
  }
  if (!std::isfinite(gamma) || gamma <= 0.0F) {
    throw std::invalid_argument("gamma must be finite and greater than zero");
  }
  std::array<std::uint8_t, 256> lookup{};
  const float inverse_gamma = 1.0F / gamma;
  for (std::size_t value = 0; value < lookup.size(); ++value) {
    const float normalized = static_cast<float>(value) / 255.0F;
    lookup[value] = static_cast<std::uint8_t>(
        std::clamp(std::lround(std::pow(normalized, inverse_gamma) * 255.0F), 0L, 255L));
  }
  for (std::uint8_t& value : buffer.bytes()) {
    value = lookup[value];
  }
}

std::unique_ptr<FrameBuffer> resizeBilinear(const FrameBuffer& input,
                                             Resolution output_resolution,
                                             std::uint64_t output_id) {
  if (input.format() == PixelFormat::kYuv420) {
    throw std::invalid_argument("bilinear resize does not support YUV420");
  }
  auto output =
      std::make_unique<FrameBuffer>(output_id, output_resolution, input.format());
  const Resolution source_resolution = input.resolution();
  const std::size_t channels = input.format() == PixelFormat::kRgb888 ? 3U : 1U;
  const auto source = input.bytes();
  auto destination = output->bytes();

  const float x_scale = static_cast<float>(source_resolution.width) /
                        static_cast<float>(output_resolution.width);
  const float y_scale = static_cast<float>(source_resolution.height) /
                        static_cast<float>(output_resolution.height);
  for (std::uint32_t y = 0; y < output_resolution.height; ++y) {
    const float source_y = (static_cast<float>(y) + 0.5F) * y_scale - 0.5F;
    const std::uint32_t y0 = static_cast<std::uint32_t>(
        std::clamp(std::floor(source_y), 0.0F, static_cast<float>(source_resolution.height - 1U)));
    const std::uint32_t y1 = std::min(y0 + 1U, source_resolution.height - 1U);
    const float y_weight = std::clamp(source_y - static_cast<float>(y0), 0.0F, 1.0F);
    for (std::uint32_t x = 0; x < output_resolution.width; ++x) {
      const float source_x = (static_cast<float>(x) + 0.5F) * x_scale - 0.5F;
      const std::uint32_t x0 = static_cast<std::uint32_t>(
          std::clamp(std::floor(source_x), 0.0F, static_cast<float>(source_resolution.width - 1U)));
      const std::uint32_t x1 = std::min(x0 + 1U, source_resolution.width - 1U);
      const float x_weight = std::clamp(source_x - static_cast<float>(x0), 0.0F, 1.0F);
      for (std::size_t channel = 0; channel < channels; ++channel) {
        const auto sample = [&](std::uint32_t sx, std::uint32_t sy) {
          return static_cast<float>(source[(static_cast<std::size_t>(sy) * source_resolution.width +
                                            sx) *
                                               channels +
                                           channel]);
        };
        const float top = sample(x0, y0) * (1.0F - x_weight) + sample(x1, y0) * x_weight;
        const float bottom = sample(x0, y1) * (1.0F - x_weight) + sample(x1, y1) * x_weight;
        destination[(static_cast<std::size_t>(y) * output_resolution.width + x) * channels +
                    channel] = static_cast<std::uint8_t>(
            std::clamp(std::lround(top * (1.0F - y_weight) + bottom * y_weight), 0L, 255L));
      }
    }
  }
  return output;
}

}  // namespace camera::processing
