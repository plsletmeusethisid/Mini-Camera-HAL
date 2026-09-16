#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>

#include "camera/ImageProcessing.h"

namespace {

template <typename Function>
double measureMilliseconds(Function&& function) {
  const auto start = std::chrono::steady_clock::now();
  function();
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
      .count();
}

void referenceFloatGrayscale(const camera::FrameBuffer& input, camera::FrameBuffer& output) {
  const auto source = input.bytes();
  auto destination = output.bytes();
  for (std::size_t pixel = 0; pixel < destination.size(); ++pixel) {
    const std::size_t offset = pixel * 3U;
    const float luma = 0.299F * source[offset] + 0.587F * source[offset + 1U] +
                       0.114F * source[offset + 2U];
    destination[pixel] =
        static_cast<std::uint8_t>(std::clamp(std::lround(luma), 0L, 255L));
  }
}

}  // namespace

int main() {
  constexpr camera::Resolution resolution{1920, 1080};
  constexpr std::size_t iterations = 100;
  camera::FrameBuffer input(1, resolution, camera::PixelFormat::kRgb888);
  camera::FrameBuffer output(2, resolution, camera::PixelFormat::kGray8);
  for (std::size_t index = 0; index < input.size(); ++index) {
    input.bytes()[index] = static_cast<std::uint8_t>((index * 31U) % 256U);
  }

  const double reference_ms = measureMilliseconds([&] {
    for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
      referenceFloatGrayscale(input, output);
    }
  });
  const std::uint8_t reference_guard = output.bytes()[12345];
  const double fixed_point_ms = measureMilliseconds([&] {
    for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
      camera::processing::rgbToGrayscaleInto(input, output);
    }
  });
  const std::uint8_t optimized_guard = output.bytes()[12345];

  std::cout << std::fixed << std::setprecision(3)
            << "resolution=1920x1080 iterations=" << iterations << '\n'
            << "reference_float_ms=" << reference_ms << '\n'
            << "fixed_point_ms=" << fixed_point_ms << '\n'
            << "speedup=" << reference_ms / fixed_point_ms << "x\n"
            << "reference_guard=" << static_cast<int>(reference_guard)
            << " optimized_guard=" << static_cast<int>(optimized_guard) << '\n';
  return 0;
}
