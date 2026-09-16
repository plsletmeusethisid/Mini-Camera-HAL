#pragma once

#include <chrono>
#include <memory>
#include <string>

#include "camera/CameraDevice.h"
#include "camera/BufferPool.h"

namespace camera {

struct CaptureResult {
  std::uint64_t frame_number{};
  CaptureStatus status{CaptureStatus::kDeviceFailure};
  std::string message;
  CameraMetadata metadata;
  std::shared_ptr<FrameBuffer> buffer;
  std::chrono::nanoseconds capture_latency{};

  [[nodiscard]] bool ok() const noexcept { return status == CaptureStatus::kOk; }
};

class CameraSession {
 public:
  explicit CameraSession(std::unique_ptr<ICameraDevice> device);
  CameraSession(std::unique_ptr<ICameraDevice> device, std::shared_ptr<BufferPool> buffer_pool);
  ~CameraSession();

  CameraSession(const CameraSession&) = delete;
  CameraSession& operator=(const CameraSession&) = delete;
  CameraSession(CameraSession&&) = delete;
  CameraSession& operator=(CameraSession&&) = delete;

  [[nodiscard]] bool open();
  void close() noexcept;
  void cancelPending() noexcept;
  [[nodiscard]] bool isOpen() const noexcept;
  [[nodiscard]] std::string deviceName() const;
  [[nodiscard]] CaptureResult capture(const CaptureRequest& request);

  [[nodiscard]] static std::string validate(const CaptureRequest& request);

 private:
  std::unique_ptr<ICameraDevice> device_;
  std::shared_ptr<BufferPool> buffer_pool_;
};

}  // namespace camera
