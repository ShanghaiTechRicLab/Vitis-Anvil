#include <anvil/hls.hpp>

#include <ap_int.h>

namespace ahls = anvil::hls;

extern "C" void component_radix_sort(const ap_uint<8>* in_keys,
                                      const ap_uint<16>* in_payloads,
                                      ap_uint<8>* out_keys,
                                      ap_uint<16>* out_payloads) {
#pragma HLS interface m_axi port=in_keys bundle=gmem0 depth=16
#pragma HLS interface m_axi port=in_payloads bundle=gmem1 depth=16
#pragma HLS interface m_axi port=out_keys bundle=gmem2 depth=16
#pragma HLS interface m_axi port=out_payloads bundle=gmem3 depth=16
#pragma HLS interface s_axilite port=return bundle=control

  ap_uint<8> keys[8];
  ap_uint<16> payloads[8];
#pragma HLS array_partition variable=keys complete
#pragma HLS array_partition variable=payloads complete
  for (int i = 0; i < 8; ++i) {
#pragma HLS pipeline II=1
    keys[i] = in_keys[i];
    payloads[i] = in_payloads[i];
  }

  ahls::compute::radix_sort_by_key<8, 4>(keys, payloads);

  for (int i = 0; i < 8; ++i) {
#pragma HLS pipeline II=1
    out_keys[i] = keys[i];
    out_payloads[i] = payloads[i];
  }
}
