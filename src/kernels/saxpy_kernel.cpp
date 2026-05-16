#include "kernels/saxpy_kernel.hpp"

#include "anvil/hls/dataflow.hpp"
#include "anvil/hls/packed_ops.hpp"
#include "anvil/hls/stream.hpp"

namespace {

typedef anvil::hls::Stream<SaxpyPack, anvil::hls::kDefaultDataflowStreamDepth> SaxpyStream;

struct SaxpyOp {
  float operator()(float a, float x, float y) const { return a * x + y; }
};

void Load(const SaxpyPack* in, SaxpyStream& out, int n_pack) {
  anvil::hls::LoadPacks(in, out, n_pack);
}

void Compute(SaxpyStream& x, SaxpyStream& y, SaxpyStream& out, float a, int n_pack) {
  anvil::hls::MapPacksWithScalar<SaxpyPack>(x, y, out, a, n_pack, SaxpyOp());
}

void Store(SaxpyStream& in, SaxpyPack* out, int n_pack) {
  anvil::hls::StorePacks(in, out, n_pack);
}

}  // namespace

extern "C" void saxpy(
    SaxpyPack* x,
    SaxpyPack* y,
    SaxpyPack* out,
    float       a,
    int         n_total) {
#pragma HLS interface m_axi   port=x   bundle=gmem0 offset=slave depth=8192
#pragma HLS interface m_axi   port=y   bundle=gmem1 offset=slave depth=8192
#pragma HLS interface m_axi   port=out bundle=gmem2 offset=slave depth=8192
#pragma HLS interface s_axilite port=x       bundle=control
#pragma HLS interface s_axilite port=y       bundle=control
#pragma HLS interface s_axilite port=out     bundle=control
#pragma HLS interface s_axilite port=a       bundle=control
#pragma HLS interface s_axilite port=n_total bundle=control
#pragma HLS interface s_axilite port=return  bundle=control

  if (n_total <= 0) return;

  const int n_pack = (n_total - 1) / kernels::kSaxpyPackWidth + 1;

  SaxpyStream sx("sx"), sy("sy"), so("so");

  ANVIL_DATAFLOW_INIT();
  ANVIL_DATAFLOW_FUNCTION(Load, x, sx, n_pack);
  ANVIL_DATAFLOW_FUNCTION(Load, y, sy, n_pack);
  ANVIL_DATAFLOW_FUNCTION(Compute, sx, sy, so, a, n_pack);
  ANVIL_DATAFLOW_FUNCTION(Store, so, out, n_pack);
  ANVIL_DATAFLOW_FINALIZE();
}
