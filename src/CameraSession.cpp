#include "camera/CameraSession.h"

#include <cmath>
#include <exception>
#include <stdexcept>

namespace camera {
namespace {

constexpr std::uint32_t kMaxWidth = 7680;
constexpr std::uint32_t kMaxHeight = 4320;
constexpr float kMinExposureMs = 0.01F;
constexpr float kMaxExposureMs = 1000.0F;
constexpr float kMinGain = 1.0F;
constexpr float kMaxGain = 32.0F;

}  // namespace

CameraSession::CameraSession(std::unique_ptr<ICameraDevice> device) : device_(std::move(device)) {
  if (!device_) {
    throw std::invalid_argument("camera session requires a device");
  }
}

CameraSession::CameraSession(std::unique_ptr<ICameraDevice> device,
                             std::shared_ptr<BufferPool> buffer_pool)
    : device_(std::move(device)), buffer_pool_(std::move(buffer_pool)) {
  if (!device_) {
    throw std::invalid_argument("camera session requires a device");
  }
  if (!buffer_pool_) {
    throw std::invalid_argument("pooled camera session requires a buffer pool");
  }
}

CameraSession::~CameraSession() { close(); }

bool CameraSession::open() { return device_->open(); }

void CameraSession::close() noexcept { device_->close(); }

void CameraSession::cancelPending() noexcept {
  if (buffer_pool_) {
    buffer_pool_->shutdown();
  }
}

bool CameraSession::isOpen() const noexcept { return device_->isOpen(); }

std::string CameraSession::deviceName() const { return device_->name(); }

std::string CameraSession::validate(const CaptureRequest& request) {
  if (request.frame_number == 0) {
    return "frame_number must be greater than zero";
  }
  if (request.resolution.width == 0 || request.resolution.height == 0) {
    return "resolution dimensions must be non-zero";
  }
  if (request.resolution.width > kMaxWidth || request.resolution.height > kMaxHeight) {
    return "resolution exceeds the simulator maximum of 7680x4320";
  }
  if (request.format == PixelFormat::kYuv420 &&
      ((request.resolution.width % 2U) != 0U || (request.resolution.height % 2U) != 0U)) {
    return "YUV420 resolution dimensions must be even";
  }
  if (!std::isfinite(request.exposure_time_ms) || request.exposure_time_ms < kMinExposureMs ||
      request.exposure_time_ms > kMaxExposureMs) {
    return "exposure_time_ms must be finite and within [0.01, 1000]";
  }
  if (!std::isfinite(request.sensor_gain) || request.sensor_gain < kMinGain ||
      request.sensor_gain > kMaxGain) {
    return "sensor_gain must be finite and within [1, 32]";
  }
  return {};
}

CaptureResult CameraSession::capture(const CaptureRequest& request) {
  CaptureResult result;
  result.frame_number = request.frame_number;

  const std::string validation_error = validate(request);
  if (!validation_error.empty()) {
    result.status = CaptureStatus::kInvalidRequest;
    result.message = validation_error;
    return result;
  }
  if (!device_->isOpen()) {
    result.status = CaptureStatus::kDeviceClosed;
    result.message = "camera device is closed";
    return result;
  }

  const auto start = std::chrono::steady_clock::now();
  try {
    std::shared_ptr<FrameBuffer> target;
    if (buffer_pool_) {
      if (buffer_pool_->resolution() != request.resolution || buffer_pool_->format() != request.format) {
        result.status = CaptureStatus::kInvalidRequest;
        result.message = "capture request does not match configured buffer pool";
        return result;
      }
      target = buffer_pool_->acquire();
      if (!target) {
        result.status = CaptureStatus::kDeviceFailure;
        result.message = "buffer pool is shut down";
        return result;
      }
    }
    DeviceCapture capture = device_->capture(request, std::move(target));
    result.capture_latency = std::chrono::steady_clock::now() - start;
    result.metadata = capture.metadata;
    result.buffer = std::move(capture.buffer);
    result.status = CaptureStatus::kOk;
  } catch (const std::exception& error) {
    result.capture_latency = std::chrono::steady_clock::now() - start;
    result.status = CaptureStatus::kDeviceFailure;
    result.message = error.what();
  } catch (...) {
    result.capture_latency = std::chrono::steady_clock::now() - start;
    result.status = CaptureStatus::kDeviceFailure;
    result.message = "unknown device failure";
  }
  return result;
}

}  // namespace camera
