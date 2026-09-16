#include <charconv>
#include <chrono>
#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string_view>

#include "camera/BufferPool.h"

namespace {

std::size_t parseIterations(int argc, char** argv) {
  if (argc == 1) {
    return 1000;
  }
  if (argc != 3 || std::string_view(argv[1]) != "--iterations") {
    throw std::invalid_argument("usage: buffer_benchmark [--iterations N]");
  }
  std::size_t count{};
  const std::string_view input(argv[2]);
  const auto [end, error] = std::from_chars(input.data(), input.data() + input.size(), count);
  if (error != std::errc{} || end != input.data() + input.size() || count == 0) {
    throw std::invalid_argument("iterations must be a positive integer");
  }
  return count;
}

template <typename Function>
double measureMilliseconds(Function&& function) {
  const auto start = std::chrono::steady_clock::now();
  function();
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
      .count();
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const std::size_t iterations = parseIterations(argc, argv);
    constexpr camera::Resolution resolution{1920, 1080};
    std::uint64_t guard = 0;

    const double allocation_acquire_ms = measureMilliseconds([&] {
      for (std::size_t index = 0; index < iterations; ++index) {
        auto buffer = std::make_shared<camera::FrameBuffer>(index, resolution,
                                                            camera::PixelFormat::kRgb888);
        buffer->bytes()[index % buffer->size()] = static_cast<std::uint8_t>(index);
        guard += buffer->bytes()[index % buffer->size()];
      }
    });

    // Capacity one is the fair comparison for this sequential workload. Multi-buffer pools trade
    // cache locality for concurrent in-flight capacity and are measured separately at pipeline level.
    camera::BufferPool pool(1, resolution, camera::PixelFormat::kRgb888);
    const double pooling_acquire_ms = measureMilliseconds([&] {
      for (std::size_t index = 0; index < iterations; ++index) {
        auto buffer = pool.acquire();
        buffer->bytes()[index % buffer->size()] = static_cast<std::uint8_t>(index);
        guard += buffer->bytes()[index % buffer->size()];
      }
    });

    const double allocation_fill_ms = measureMilliseconds([&] {
      for (std::size_t index = 0; index < iterations; ++index) {
        auto buffer = std::make_shared<camera::FrameBuffer>(index, resolution,
                                                            camera::PixelFormat::kRgb888);
        std::fill(buffer->bytes().begin(), buffer->bytes().end(),
                  static_cast<std::uint8_t>(index));
        guard += buffer->bytes()[index % buffer->size()];
      }
    });
    const double pooling_fill_ms = measureMilliseconds([&] {
      for (std::size_t index = 0; index < iterations; ++index) {
        auto buffer = pool.acquire();
        std::fill(buffer->bytes().begin(), buffer->bytes().end(),
                  static_cast<std::uint8_t>(index));
        guard += buffer->bytes()[index % buffer->size()];
      }
    });

    std::cout << std::fixed << std::setprecision(3)
              << "resolution=1920x1080 format=RGB888 iterations=" << iterations << '\n'
              << "acquire.allocate_ms=" << allocation_acquire_ms << '\n'
              << "acquire.pool_ms=" << pooling_acquire_ms << '\n'
              << "acquire.speedup=" << allocation_acquire_ms / pooling_acquire_ms << "x\n"
              << "fill.allocate_ms=" << allocation_fill_ms << '\n'
              << "fill.pool_ms=" << pooling_fill_ms << '\n'
              << "fill.speedup=" << allocation_fill_ms / pooling_fill_ms << "x\n"
              << "allocation_reduction=" << iterations << "->1\n"
              << "guard=" << guard << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 64;
  }
}
