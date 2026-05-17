#include <anvil/hls.hpp>

#include <ap_int.h>

namespace ahls = anvil::hls;

extern "C" void component_prefix_histogram(const ap_uint<2>* keys,
                                            ap_uint<4>* out) {
#pragma HLS interface m_axi port=keys bundle=gmem0 depth=8
#pragma HLS interface m_axi port=out bundle=gmem1 depth=8
#pragma HLS interface s_axilite port=return bundle=control

  static const int N = 8;
  static const int Bins = 4;

  ap_uint<2> local_keys[N];
  ahls::compute::count_t<N> counts[Bins];
  ahls::compute::count_t<N> offsets[Bins];
#pragma HLS array_partition variable=counts complete
#pragma HLS array_partition variable=offsets complete

  for (int i = 0; i < N; ++i) {
#pragma HLS pipeline II=1
    local_keys[i] = keys[i];
  }

  ahls::compute::histogram<2, Bins>(local_keys, counts);
  ahls::compute::exclusive_scan<Bins>(counts, offsets);

  for (int i = 0; i < Bins; ++i) {
#pragma HLS pipeline II=1
    out[i] = counts[i];
    out[i + Bins] = offsets[i];
  }
}
