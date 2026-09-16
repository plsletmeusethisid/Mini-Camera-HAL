#include "camera/OpenCVCameraDevice.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <stdexcept>
#include <utility>

#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

namespace camera {

struct OpenCVCameraDevice::Impl {
  enum class SourceKind { kWebcam, kVideoFile };
  SourceKind kind{SourceKind::kWebcam};
  int device_index{};
  std::string path;
  cv::VideoCapture capture;
  std::atomic<std::uint64_t> next_buffer_id{1};
};

OpenCVCameraDevice::OpenCVCameraDevice(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
OpenCVCameraDevice::~OpenCVCameraDevice() = default;
OpenCVCameraDevice::OpenCVCameraDevice(OpenCVCameraDevice&&) noexcept = default;
OpenCVCameraDevice& OpenCVCameraDevice::operator=(OpenCVCameraDevice&&) noexcept = default;

std::unique_ptr<OpenCVCameraDevice> OpenCVCameraDevice::webcam(int device_index) {
  auto impl = std::make_unique<Impl>();
  impl->kind = Impl::SourceKind::kWebcam;
  impl->device_index = device_index;
  return std::unique_ptr<OpenCVCameraDevice>(new OpenCVCameraDevice(std::move(impl)));
}

std::unique_ptr<OpenCVCameraDevice> OpenCVCameraDevice::videoFile(std::string path) {
  auto impl = std::make_unique<Impl>();
  impl->kind = Impl::SourceKind::kVideoFile;
  impl->path = std::move(path);
  return std::unique_ptr<OpenCVCameraDevice>(new OpenCVCameraDevice(std::move(impl)));
}

bool OpenCVCameraDevice::open() {
  if (impl_->capture.isOpened()) {
    return true;
  }
  if (impl_->kind == Impl::SourceKind::kWebcam) {
    return impl_->capture.open(impl_->device_index);
  }
  return impl_->capture.open(impl_->path);
}

void OpenCVCameraDevice::close() noexcept { impl_->capture.release(); }

bool OpenCVCameraDevice::isOpen() const noexcept { return impl_->capture.isOpened(); }

std::string OpenCVCameraDevice::name() const {
  return impl_->kind == Impl::SourceKind::kWebcam
             ? "OpenCV Webcam " + std::to_string(impl_->device_index)
             : "OpenCV Video File: " + impl_->path;
}

DeviceCapture OpenCVCameraDevice::capture(const CaptureRequest& request,
                                          std::shared_ptr<FrameBuffer> target) {
  if (!isOpen()) {
    throw std::runtime_error("OpenCV camera source is closed");
  }
  cv::Mat bgr;
  if (!impl_->capture.read(bgr) || bgr.empty()) {
    throw std::runtime_error("OpenCV camera source returned no frame");
  }
  cv::Mat resized;
  cv::resize(bgr, resized,
             cv::Size(static_cast<int>(request.resolution.width),
                      static_cast<int>(request.resolution.height)),
             0.0, 0.0, cv::INTER_LINEAR);
  cv::Mat converted;
  switch (request.format) {
    case PixelFormat::kRgb888:
      cv::cvtColor(resized, converted, cv::COLOR_BGR2RGB);
      break;
    case PixelFormat::kGray8:
      cv::cvtColor(resized, converted, cv::COLOR_BGR2GRAY);
      break;
    case PixelFormat::kYuv420:
      cv::cvtColor(resized, converted, cv::COLOR_BGR2YUV_I420);
      break;
  }
  if (!converted.isContinuous()) {
    converted = converted.clone();
  }
  auto buffer = target ? std::move(target)
                       : std::make_shared<FrameBuffer>(impl_->next_buffer_id++, request.resolution,
                                                       request.format);
  if (buffer->resolution() != request.resolution || buffer->format() != request.format) {
    throw std::invalid_argument("target buffer does not match OpenCV capture request");
  }
  const std::size_t bytes = converted.total() * converted.elemSize();
  if (bytes != buffer->size()) {
    throw std::runtime_error("OpenCV output byte count does not match frame buffer");
  }
  std::memcpy(buffer->bytes().data(), converted.data, bytes);

  const auto now = std::chrono::steady_clock::now().time_since_epoch();
  return {
      .buffer = std::move(buffer),
      .metadata = {.sensor_timestamp_ns = static_cast<std::uint64_t>(
                       std::chrono::duration_cast<std::chrono::nanoseconds>(now).count()),
                   .exposure_time_ms = request.exposure_time_ms,
                   .sensor_gain = request.sensor_gain,
                   .iso = static_cast<std::uint32_t>(100.0F * request.sensor_gain)},
  };
}

}  // namespace camera
