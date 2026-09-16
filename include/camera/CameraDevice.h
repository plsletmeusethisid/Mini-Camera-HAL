#pragma once

#include <memory>
#include <string>

#include "camera/FrameBuffer.h"
#include "camera/Types.h"

namespace camera {

struct DeviceCapture {
  std::shared_ptr<FrameBuffer> buffer;
  CameraMetadata metadata;
};

class ICameraDevice {
 public:
  virtual ~ICameraDevice() = default;

  [[nodiscard]] virtual bool open() = 0;
  virtual void close() noexcept = 0;
  [[nodiscard]] virtual bool isOpen() const noexcept = 0;
  [[nodiscard]] virtual std::string name() const = 0;
  [[nodiscard]] virtual DeviceCapture capture(
      const CaptureRequest& request, std::shared_ptr<FrameBuffer> target = {}) = 0;
};

}  // namespace camera
