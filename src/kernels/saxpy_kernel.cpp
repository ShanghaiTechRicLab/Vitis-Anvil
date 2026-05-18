#include "kernels/saxpy_kernel.hpp"

#include "anvil/hls/pack.hpp"

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

  for (int p = 0; p < n_pack; ++p) {
#pragma HLS PIPELINE II=1
    const SaxpyPack px = x[p];
    const SaxpyPack py = y[p];
    SaxpyPack po;
    for (int lane = 0; lane < kernels::kSaxpyPackWidth; ++lane) {
#pragma HLS UNROLL
      anvil::hls::SetLane(po, lane, a * anvil::hls::GetLane(px, lane) + anvil::hls::GetLane(py, lane));
    }
    out[p] = po;
  }
}
