#include <anvil/kernels/vadd.hpp>

using anvil::kernels::VaddPack;
using anvil::kernels::kVaddPack;

extern "C" void vadd(const VaddPack* a, const VaddPack* b, VaddPack* out, int n_packs) {
#pragma HLS INTERFACE m_axi port=a   bundle=gmem0 offset=slave
#pragma HLS INTERFACE m_axi port=b   bundle=gmem1 offset=slave
#pragma HLS INTERFACE m_axi port=out bundle=gmem2 offset=slave
#pragma HLS INTERFACE s_axilite port=a       bundle=control
#pragma HLS INTERFACE s_axilite port=b       bundle=control
#pragma HLS INTERFACE s_axilite port=out     bundle=control
#pragma HLS INTERFACE s_axilite port=n_packs bundle=control
#pragma HLS INTERFACE s_axilite port=return  bundle=control

VADD_LOOP:
    for (int i = 0; i < n_packs; ++i) {
#pragma HLS PIPELINE II=1
        VaddPack pa = a[i];
        VaddPack pb = b[i];
        VaddPack po;
        for (int j = 0; j < kVaddPack; ++j) {
#pragma HLS UNROLL
            po[j] = pa[j] + pb[j];
        }
        out[i] = po;
    }
}
