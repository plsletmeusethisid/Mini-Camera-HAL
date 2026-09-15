#pragma once

#include <chrono>
#include <cstdint>
#include <limits>
#include <string>

namespace camera {

enum class PixelFormat { kRgb888, kGray8, kYuv420 };

struct Resolution {
  std::uint32_t width{};
  std::uint32_t height{};

  [[nodiscard]] bool operator==(const Resolution&) const = default;
};

struct CameraMetadata {
  std::uint64_t sensor_timestamp_ns{};
  float exposure_time_ms{};
  float sensor_gain{1.0F};
  std::uint32_t iso{100};
  float focus_distance_diopters{};
};

struct CaptureRequest {
  std::uint64_t frame_number{};
  Resolution resolution{};
  PixelFormat format{PixelFormat::kRgb888};
  float exposure_time_ms{8.0F};
  float sensor_gain{1.0F};
};

enum class CaptureStatus {
  kOk,
  kInvalidRequest,
  kDeviceClosed,
  kDeviceFailure,
};

[[nodiscard]] inline std::string toString(PixelFormat format) {
  switch (format) {
    case PixelFormat::kRgb888:
      return "RGB888";
    case PixelFormat::kGray8:
      return "GRAY8";
    case PixelFormat::kYuv420:
      return "YUV420";
  }
  return "UNKNOWN";
}

[[nodiscard]] inline std::string toString(CaptureStatus status) {
  switch (status) {
    case CaptureStatus::kOk:
      return "OK";
    case CaptureStatus::kInvalidRequest:
      return "INVALID_REQUEST";
    case CaptureStatus::kDeviceClosed:
      return "DEVICE_CLOSED";
    case CaptureStatus::kDeviceFailure:
      return "DEVICE_FAILURE";
  }
  return "UNKNOWN";
}

}  // namespace camera
