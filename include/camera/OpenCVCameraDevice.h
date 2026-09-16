#pragma once

#include <memory>
#include <string>

#include "camera/CameraDevice.h"

namespace camera {

class OpenCVCameraDevice final : public ICameraDevice {
 public:
  static std::unique_ptr<OpenCVCameraDevice> webcam(int device_index = 0);
  static std::unique_ptr<OpenCVCameraDevice> videoFile(std::string path);
  ~OpenCVCameraDevice() override;

  OpenCVCameraDevice(OpenCVCameraDevice&&) noexcept;
  OpenCVCameraDevice& operator=(OpenCVCameraDevice&&) noexcept;

  [[nodiscard]] bool open() override;
  void close() noexcept override;
  [[nodiscard]] bool isOpen() const noexcept override;
  [[nodiscard]] std::string name() const override;
  [[nodiscard]] DeviceCapture capture(const CaptureRequest& request,
                                      std::shared_ptr<FrameBuffer> target = {}) override;

 private:
  struct Impl;
  explicit OpenCVCameraDevice(std::unique_ptr<Impl> impl);
  std::unique_ptr<Impl> impl_;
};

}  // namespace camera
