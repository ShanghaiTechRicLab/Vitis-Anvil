#pragma once
#include <span>
#include "anvil/gold/saxpy_gold.hpp"   // SaxpyConfig

namespace anvil::hls {

// HLS-friendly scalar-stream implementation of saxpy. Compiled as CPU
// code via hlslib's HLSLIB_SIMULATION path (std::thread per dataflow
// function). Must produce bit-exact results vs anvil::gold::saxpy_gold.
void saxpy_hls_model(std::span<const float> x,
                     std::span<const float> y,
                     std::span<float>       out,
                     const anvil::gold::SaxpyConfig& cfg);

}  // namespace anvil::hls
