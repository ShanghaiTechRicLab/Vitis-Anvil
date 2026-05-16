#pragma once

#include <span>

namespace hls_model {

// HLS-friendly packed-memory implementation of vadd. Compiled as CPU code via
// hlslib's HLSLIB_SIMULATION path and checked bit-exactly against gold.
void vadd_hls_model(std::span<const float> a,
                    std::span<const float> b,
                    std::span<float> out);

}  // namespace hls_model
