#include "hls_model/saxpy_hls_model.hpp"

#include <cstddef>
#include <stdexcept>
#include <vector>

#include "anvil/hls/dataflow.hpp"
#include "anvil/hls/packed_ops.hpp"
#include "anvil/hls/stream.hpp"
#include "hls_model/saxpy_pack_utils.hpp"

namespace hls_model {
namespace {

typedef anvil::hls::Stream<kernels::SaxpyPack, anvil::hls::kDefaultDataflowStreamDepth> SaxpyStream;

struct SaxpyOp {
  float operator()(float a, float x, float y) const { return a * x + y; }
};

void Load(const kernels::SaxpyPack* in, SaxpyStream& out, int n_pack) {
  anvil::hls::LoadPacks(in, out, n_pack);
}

void Compute(SaxpyStream& x, SaxpyStream& y, SaxpyStream& out, float a, int n_pack) {
  anvil::hls::MapPacksWithScalar<kernels::SaxpyPack>(x, y, out, a, n_pack, SaxpyOp());
}

void Store(SaxpyStream& in, kernels::SaxpyPack* out, int n_pack) {
  anvil::hls::StorePacks(in, out, n_pack);
}

}  // namespace

void saxpy_hls_model(std::span<const float> x,
                     std::span<const float> y,
                     std::span<float>       out,
                     const anvil::gold::SaxpyConfig& cfg) {
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

  SaxpyStream sx("sx"), sy("sy"), so("so");

  ANVIL_DATAFLOW_INIT();
  ANVIL_DATAFLOW_FUNCTION(Load, x_packed.data(), sx, n_pack);
  ANVIL_DATAFLOW_FUNCTION(Load, y_packed.data(), sy, n_pack);
  ANVIL_DATAFLOW_FUNCTION(Compute, sx, sy, so, cfg.a, n_pack);
  ANVIL_DATAFLOW_FUNCTION(Store, so, out_packed.data(), n_pack);
  ANVIL_DATAFLOW_FINALIZE();

  UnpackScalars(out_packed.data(), out);
}

}  // namespace hls_model
