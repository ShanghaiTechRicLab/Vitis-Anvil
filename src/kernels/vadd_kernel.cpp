#include "kernels/vadd.hpp"

#include "anvil/hls/packed_ops.hpp"
#include "kernels/vadd_op.hpp"

extern "C" void vadd(const kernels::VaddPack* a,
                     const kernels::VaddPack* b,
                     kernels::VaddPack* out,
                     int n_packs) {
#pragma HLS INTERFACE m_axi port=a   bundle=gmem0 offset=slave depth=1024
#pragma HLS INTERFACE m_axi port=b   bundle=gmem1 offset=slave depth=1024
#pragma HLS INTERFACE m_axi port=out bundle=gmem2 offset=slave depth=1024
#pragma HLS INTERFACE s_axilite port=a       bundle=control
#pragma HLS INTERFACE s_axilite port=b       bundle=control
#pragma HLS INTERFACE s_axilite port=out     bundle=control
#pragma HLS INTERFACE s_axilite port=n_packs bundle=control
#pragma HLS INTERFACE s_axilite port=return  bundle=control

  anvil::hls::MapMem2Packs(a, b, out, n_packs, kernels::VaddOp());
}
