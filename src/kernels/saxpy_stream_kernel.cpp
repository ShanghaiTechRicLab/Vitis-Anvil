#include "kernels/saxpy_stream.hpp"

#include "anvil/hls/axis.hpp"
#include "anvil/hls/pack.hpp"

extern "C" void saxpy_stream(
    const kernels::PipelinePack* x,
    const kernels::PipelinePack* y,
    hls::stream<kernels::PipelinePack>& s_out,
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
    kernels::PipelinePack px = x[i];
    kernels::PipelinePack py = y[i];
    kernels::PipelinePack pz;
    for (int j = 0; j < kernels::kPipelinePackWidth; ++j) {
#pragma HLS UNROLL
      anvil::hls::SetLane(pz, j, a * anvil::hls::GetLane(px, j) + anvil::hls::GetLane(py, j));
    }
    anvil::hls::WriteAxis(s_out, pz);
  }
}
