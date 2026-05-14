#pragma once
#include <span>
#include <cstddef>

namespace accel::gold {

struct SaxpyConfig {
  float a = 1.0f;
};

// Compute out[i] = cfg.a * x[i] + y[i] for i in [0, x.size()).
// Pre: x.size() == y.size() == out.size().
// Throws std::invalid_argument on size mismatch.
void saxpy_gold(std::span<const float> x,
                std::span<const float> y,
                std::span<float>       out,
                const SaxpyConfig&     cfg);

}  // namespace accel::gold
