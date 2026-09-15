#pragma once

#include <atomic>

#include "camera/CameraDevice.h"

namespace camera {

class MockCameraDevice final : public ICameraDevice {
 public:
  [[nodiscard]] bool open() override;
  void close() noexcept override;
  [[nodiscard]] bool isOpen() const noexcept override;
  [[nodiscard]] std::string name() const override;
  [[nodiscard]] DeviceCapture capture(const CaptureRequest& request) override;

 private:
  bool open_{false};
  std::atomic<std::uint64_t> next_buffer_id_{1};
};

}  // namespace camera
