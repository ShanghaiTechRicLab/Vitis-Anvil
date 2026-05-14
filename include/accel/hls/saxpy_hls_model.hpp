#pragma once
#include <span>
#include "accel/gold/saxpy_gold.hpp"   // SaxpyConfig

namespace accel::hls {

// HLS-friendly scalar-stream implementation of saxpy. Compiled as CPU
// code via hlslib's HLSLIB_SIMULATION path (std::thread per dataflow
// function). Must produce bit-exact results vs accel::gold::saxpy_gold.
void saxpy_hls_model(std::span<const float> x,
                     std::span<const float> y,
                     std::span<float>       out,
                     const accel::gold::SaxpyConfig& cfg);

}  // namespace accel::hls
