#include "kernels/vadd_stream.hpp"

using kernels::PipelinePack;
static const int kW = kernels::kPipelinePack;

extern "C" void vadd_stream(
    hls::stream<PipelinePack>& s_in,
    const PipelinePack* b,
    PipelinePack* out,
    int n_packs) {
#pragma HLS INTERFACE axis port=s_in
#pragma HLS INTERFACE m_axi port=b bundle=gmem0 offset=slave depth=1024
#pragma HLS INTERFACE m_axi port=out bundle=gmem1 offset=slave depth=1024
#pragma HLS INTERFACE s_axilite port=b bundle=control
#pragma HLS INTERFACE s_axilite port=out bundle=control
#pragma HLS INTERFACE s_axilite port=n_packs bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

  for (int i = 0; i < n_packs; ++i) {
#pragma HLS PIPELINE II=1
    PipelinePack pz = s_in.read();
    PipelinePack pb = b[i];
    PipelinePack po;
    for (int j = 0; j < kW; ++j) {
#pragma HLS UNROLL
      po[j] = pz[j] + pb[j];
    }
    out[i] = po;
  }
}
