#pragma once

#include <cstddef>
#include <span>
#include <stdexcept>

namespace gold {

inline void vadd_gold(std::span<const float> a,
                      std::span<const float> b,
                      std::span<float> out) {
  if (a.size() != b.size() || a.size() != out.size()) {
    throw std::invalid_argument("vadd_gold: span sizes must match");
  }
  for (std::size_t i = 0; i < out.size(); ++i) {
    out[i] = a[i] + b[i];
  }
}

}  // namespace gold
