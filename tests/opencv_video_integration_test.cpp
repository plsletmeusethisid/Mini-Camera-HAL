#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

#include "camera/OpenCVCameraDevice.h"

int main() {
  const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() /
      ("mini-camera-hal-generated-" + std::to_string(unique) + ".avi");

  try {
    cv::VideoWriter writer(path.string(), cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), 30.0,
                           cv::Size(64, 48), true);
    if (!writer.isOpened()) {
      std::cerr << "generated-video: MJPG writer failed to open\n";
      return 1;
    }
    for (int index = 0; index < 8; ++index) {
      cv::Mat frame(48, 64, CV_8UC3,
                    cv::Scalar(20 + index * 3, 80 + index, 140 - index * 2));
      writer.write(frame);
    }
    writer.release();

    auto device = camera::OpenCVCameraDevice::videoFile(path.string());
    if (!device->open()) {
      std::cerr << "generated-video: adapter failed to open generated AVI\n";
      std::filesystem::remove(path);
      return 2;
    }
    const camera::CaptureRequest request{.frame_number = 1,
                                         .resolution = {32, 24},
                                         .format = camera::PixelFormat::kRgb888};
    camera::DeviceCapture captured = device->capture(request);
    device->close();

    if (captured.buffer == nullptr) {
      std::filesystem::remove(path);
      std::cerr << "generated-video: adapter returned no buffer\n";
      return 3;
    }
    std::uint64_t byte_sum = 0;
    for (std::uint8_t byte : captured.buffer->bytes()) {
      byte_sum += byte;
    }
    const bool valid = captured.buffer->resolution() == request.resolution &&
                       captured.buffer->format() == request.format &&
                       captured.buffer->size() == 32U * 24U * 3U && byte_sum > 0U;
    std::filesystem::remove(path);
    if (!valid) {
      std::cerr << "generated-video: captured frame validation failed\n";
      return 3;
    }
    std::cout << "generated-video: captured 32x24 RGB frame, byte_sum=" << byte_sum << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::filesystem::remove(path);
    std::cerr << "generated-video: " << error.what() << '\n';
    return 4;
  }
}
