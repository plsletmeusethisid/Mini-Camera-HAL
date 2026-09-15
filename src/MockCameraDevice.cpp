#include "camera/MockCameraDevice.h"

#include <chrono>
#include <stdexcept>

namespace camera {

bool MockCameraDevice::open() {
  open_ = true;
  return true;
}

void MockCameraDevice::close() noexcept { open_ = false; }

bool MockCameraDevice::isOpen() const noexcept { return open_; }

std::string MockCameraDevice::name() const { return "Deterministic Mock Camera"; }

DeviceCapture MockCameraDevice::capture(const CaptureRequest& request) {
  if (!open_) {
    throw std::runtime_error("capture requested while mock device is closed");
  }

  auto buffer = std::make_unique<FrameBuffer>(next_buffer_id_++, request.resolution, request.format);
  auto bytes = buffer->bytes();
  for (std::size_t index = 0; index < bytes.size(); ++index) {
    bytes[index] = static_cast<std::uint8_t>((index + request.frame_number) % 256U);
  }

  const auto now = std::chrono::steady_clock::now().time_since_epoch();
  CameraMetadata metadata{
      .sensor_timestamp_ns =
          static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now).count()),
      .exposure_time_ms = request.exposure_time_ms,
      .sensor_gain = request.sensor_gain,
      .iso = static_cast<std::uint32_t>(100.0F * request.sensor_gain),
      .focus_distance_diopters = 0.0F,
  };
  return DeviceCapture{.buffer = std::move(buffer), .metadata = metadata};
}

}  // namespace camera
