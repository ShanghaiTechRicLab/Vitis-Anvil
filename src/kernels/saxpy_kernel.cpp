#include "accel/kernels/saxpy_kernel.hpp"

#include <hlslib/xilinx/Simulation.h>
#include <hlslib/xilinx/Stream.h>

namespace {

constexpr int kStreamDepth = 32;

void Load(const SaxpyPack* in, hlslib::Stream<SaxpyPack, kStreamDepth>& s, int n_pack) {
  for (int i = 0; i < n_pack; ++i) {
#pragma HLS pipeline II=1
    s.Push(in[i]);
  }
}

void Compute(hlslib::Stream<SaxpyPack, kStreamDepth>& xs,
             hlslib::Stream<SaxpyPack, kStreamDepth>& ys,
             hlslib::Stream<SaxpyPack, kStreamDepth>& os,
             float a, int n_pack) {
  for (int i = 0; i < n_pack; ++i) {
#pragma HLS pipeline II=1
    SaxpyPack xv = xs.Pop();
    SaxpyPack yv = ys.Pop();
    SaxpyPack r;
    for (int j = 0; j < 16; ++j) {
#pragma HLS unroll
      r[j] = a * xv[j] + yv[j];
    }
    os.Push(r);
  }
}

void Store(hlslib::Stream<SaxpyPack, kStreamDepth>& s, SaxpyPack* out, int n_pack) {
  for (int i = 0; i < n_pack; ++i) {
#pragma HLS pipeline II=1
    out[i] = s.Pop();
  }
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

  const int n_pack = (n_total + 15) / 16;

  hlslib::Stream<SaxpyPack, kStreamDepth> sx("sx"), sy("sy"), so("so");

  HLSLIB_DATAFLOW_INIT();
  HLSLIB_DATAFLOW_FUNCTION(Load,    x,  sx, n_pack);
  HLSLIB_DATAFLOW_FUNCTION(Load,    y,  sy, n_pack);
  HLSLIB_DATAFLOW_FUNCTION(Compute, sx, sy, so, a, n_pack);
  HLSLIB_DATAFLOW_FUNCTION(Store,   so, out, n_pack);
  HLSLIB_DATAFLOW_FINALIZE();
}
