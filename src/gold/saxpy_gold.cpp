#include "anvil/gold/saxpy_gold.hpp"
#include <stdexcept>

namespace anvil::gold {

void saxpy_gold(std::span<const float> x,
                std::span<const float> y,
                std::span<float>       out,
                const SaxpyConfig&     cfg) {
  if (x.size() != y.size() || x.size() != out.size()) {
    throw std::invalid_argument(
      "saxpy_gold: x, y, out must have equal size");
  }
  const float a = cfg.a;
  for (std::size_t i = 0; i < x.size(); ++i) {
    out[i] = a * x[i] + y[i];
  }
}

}  // namespace anvil::gold
