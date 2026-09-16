#pragma once

#include <memory>

#include "camera/FrameBuffer.h"

namespace camera::processing {

[[nodiscard]] std::unique_ptr<FrameBuffer> rgbToGrayscale(const FrameBuffer& input,
                                                           std::uint64_t output_id);
void rgbToGrayscaleInto(const FrameBuffer& input, FrameBuffer& output);
void gammaCorrect(FrameBuffer& buffer, float gamma);
[[nodiscard]] std::unique_ptr<FrameBuffer> resizeBilinear(const FrameBuffer& input,
                                                           Resolution output_resolution,
                                                           std::uint64_t output_id);

}  // namespace camera::processing
