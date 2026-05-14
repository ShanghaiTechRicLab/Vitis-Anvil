#include "anvil/gold/metrics.hpp"
#include <cmath>
#include <cstddef>
#include <stdexcept>

namespace anvil::gold {

float max_abs_error(std::span<const float> a, std::span<const float> b) {
  if (a.size() != b.size()) {
    throw std::invalid_argument("max_abs_error: size mismatch");
  }
  float worst = 0.0f;
  for (std::size_t i = 0; i < a.size(); ++i) {
    const float d = std::fabs(a[i] - b[i]);
    if (d > worst) worst = d;
  }
  return worst;
}

float rms_error(std::span<const float> a, std::span<const float> b) {
  if (a.size() != b.size()) {
    throw std::invalid_argument("rms_error: size mismatch");
  }
  if (a.empty()) return 0.0f;
  double acc = 0.0;
  for (std::size_t i = 0; i < a.size(); ++i) {
    const double d = static_cast<double>(a[i]) - static_cast<double>(b[i]);
    acc += d * d;
  }
  return static_cast<float>(std::sqrt(acc / static_cast<double>(a.size())));
}

}  // namespace anvil::gold
