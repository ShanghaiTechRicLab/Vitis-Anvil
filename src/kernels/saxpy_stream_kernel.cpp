#include "kernels/saxpy_stream.hpp"

using kernels::PipelinePack;
static const int kW = kernels::kPipelinePack;

extern "C" void saxpy_stream(
    const PipelinePack* x,
    const PipelinePack* y,
    hls::stream<PipelinePack>& s_out,
    float a,
    int n_packs) {
#pragma HLS INTERFACE m_axi port=x bundle=gmem0 offset=slave depth=1024
#pragma HLS INTERFACE m_axi port=y bundle=gmem1 offset=slave depth=1024
#pragma HLS INTERFACE axis port=s_out
#pragma HLS INTERFACE s_axilite port=x bundle=control
#pragma HLS INTERFACE s_axilite port=y bundle=control
#pragma HLS INTERFACE s_axilite port=a bundle=control
#pragma HLS INTERFACE s_axilite port=n_packs bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

  for (int i = 0; i < n_packs; ++i) {
#pragma HLS PIPELINE II=1
    PipelinePack px = x[i];
    PipelinePack py = y[i];
    PipelinePack pz;
    for (int j = 0; j < kW; ++j) {
#pragma HLS UNROLL
      pz[j] = a * px[j] + py[j];
    }
    s_out.write(pz);
  }
}
