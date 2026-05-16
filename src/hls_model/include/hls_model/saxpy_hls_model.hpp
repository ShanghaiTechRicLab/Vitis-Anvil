#pragma once
#include <span>
#include "anvil/gold/saxpy_gold.hpp"

namespace hls_model {

// HLS-friendly packed-stream implementation of saxpy. Compiled as CPU code via
// hlslib's HLSLIB_SIMULATION path and checked bit-exactly against gold.
void saxpy_hls_model(std::span<const float> x,
                     std::span<const float> y,
                     std::span<float>       out,
                     const anvil::gold::SaxpyConfig& cfg);

}  // namespace hls_model
