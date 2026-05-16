#include "hls_model/vadd_hls_model.hpp"

#include <stdexcept>
#include <vector>

#include "anvil/hls/packed_ops.hpp"
#include "hls_model/pack_utils.hpp"
#include "kernels/kernel_types.hpp"
#include "kernels/vadd_op.hpp"

namespace hls_model {

void vadd_hls_model(std::span<const float> a,
                    std::span<const float> b,
                    std::span<float> out) {
  if (a.size() != b.size() || a.size() != out.size()) {
    throw std::invalid_argument("vadd_hls_model: span sizes must match");
  }
  const int n = static_cast<int>(a.size());
  if (n == 0) return;

  const int width = kernels::kVaddPackWidth;
  const int n_pack = (n + width - 1) / width;

  std::vector<kernels::VaddPack> a_packed(static_cast<std::size_t>(n_pack));
  std::vector<kernels::VaddPack> b_packed(static_cast<std::size_t>(n_pack));
  std::vector<kernels::VaddPack> out_packed(static_cast<std::size_t>(n_pack));

  PackScalars<kernels::VaddPack>(a, a_packed.data());
  PackScalars<kernels::VaddPack>(b, b_packed.data());
  anvil::hls::MapMem2Packs(a_packed.data(), b_packed.data(), out_packed.data(), n_pack, kernels::VaddOp());
  UnpackScalars<kernels::VaddPack>(out_packed.data(), out);
}

}  // namespace hls_model
