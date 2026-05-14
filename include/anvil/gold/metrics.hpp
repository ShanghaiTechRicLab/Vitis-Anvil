#pragma once
#include <span>

namespace anvil::gold {

// Element-wise max absolute error: max_i |a[i] - b[i]|.
// Pre: a.size() == b.size(). Throws std::invalid_argument on mismatch.
// For empty inputs, returns 0.0f.
float max_abs_error(std::span<const float> a, std::span<const float> b);

// Root-mean-square error: sqrt(mean((a[i] - b[i])^2)).
// Pre: a.size() == b.size(). Throws std::invalid_argument on mismatch.
// For empty inputs, returns 0.0f.
float rms_error(std::span<const float> a, std::span<const float> b);

}  // namespace anvil::gold
