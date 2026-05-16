#include "hls_model/saxpy_hls_model.hpp"

#include <cstddef>
#include <stdexcept>
#include <vector>

#include "anvil/hls/dataflow.hpp"
#include "hls_model/saxpy_pack_utils.hpp"
#include "kernels/saxpy_core.hpp"

namespace hls_model {

void saxpy_hls_model(std::span<const float> x,
                     std::span<const float> y,
                     std::span<float>       out,
                     const gold::SaxpyConfig& cfg) {
  if (x.size() != y.size() || x.size() != out.size()) {
    throw std::invalid_argument("saxpy_hls_model: span sizes must match");
  }
  const int n = static_cast<int>(x.size());
  if (n == 0) return;

  const int width = kernels::kSaxpyPackWidth;
  const int n_pack = (n + width - 1) / width;

  std::vector<kernels::SaxpyPack> x_packed(n_pack), y_packed(n_pack), out_packed(n_pack);
  PackScalars(x, x_packed.data());
  PackScalars(y, y_packed.data());

  kernels::saxpy_core::SaxpyStream sx("sx"), sy("sy"), so("so");

  ANVIL_DATAFLOW_INIT();
  ANVIL_DATAFLOW_FUNCTION(kernels::saxpy_core::Load, x_packed.data(), sx, n_pack);
  ANVIL_DATAFLOW_FUNCTION(kernels::saxpy_core::Load, y_packed.data(), sy, n_pack);
  ANVIL_DATAFLOW_FUNCTION(kernels::saxpy_core::Compute, sx, sy, so, cfg.a, n_pack);
  ANVIL_DATAFLOW_FUNCTION(kernels::saxpy_core::Store, so, out_packed.data(), n_pack);
  ANVIL_DATAFLOW_FINALIZE();

  UnpackScalars(out_packed.data(), out);
}

}  // namespace hls_model
