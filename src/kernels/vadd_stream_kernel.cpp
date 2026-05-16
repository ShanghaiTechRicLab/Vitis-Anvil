#include "kernels/vadd_stream.hpp"

#include "anvil/hls/axis.hpp"
#include "anvil/hls/pack.hpp"

extern "C" void vadd_stream(
    hls::stream<kernels::PipelinePack>& s_in,
    const kernels::PipelinePack* b,
    kernels::PipelinePack* out,
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
    kernels::PipelinePack pz = anvil::hls::ReadAxis(s_in);
    kernels::PipelinePack pb = b[i];
    kernels::PipelinePack po;
    for (int j = 0; j < kernels::kPipelinePackWidth; ++j) {
#pragma HLS UNROLL
      anvil::hls::SetLane(po, j, anvil::hls::GetLane(pz, j) + anvil::hls::GetLane(pb, j));
    }
    out[i] = po;
  }
}
