#include "kernels/saxpy_kernel.hpp"

#include "anvil/hls/dataflow.hpp"
#include "kernels/saxpy_core.hpp"

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

  kernels::saxpy_core::SaxpyStream sx("sx"), sy("sy"), so("so");

  ANVIL_DATAFLOW_INIT();
  ANVIL_DATAFLOW_FUNCTION(kernels::saxpy_core::Load, x, sx, n_pack);
  ANVIL_DATAFLOW_FUNCTION(kernels::saxpy_core::Load, y, sy, n_pack);
  ANVIL_DATAFLOW_FUNCTION(kernels::saxpy_core::Compute, sx, sy, so, a, n_pack);
  ANVIL_DATAFLOW_FUNCTION(kernels::saxpy_core::Store, so, out, n_pack);
  ANVIL_DATAFLOW_FINALIZE();
}
