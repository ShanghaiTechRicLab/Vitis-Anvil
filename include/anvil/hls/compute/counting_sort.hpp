#pragma once

#include <anvil/hls/compute/histogram.hpp>
#include <anvil/hls/compute/prefix_sum.hpp>

#include <ap_int.h>

namespace anvil {
namespace hls {
namespace compute {

template <int KeyBits, int Bins, int N, typename PayloadT>
void counting_sort_reorder(const ap_uint<KeyBits> (&keys)[N],
                           const PayloadT (&payloads)[N],
                           ap_uint<KeyBits> (&out_keys)[N],
                           PayloadT (&out_payloads)[N]) {
#pragma HLS inline
  static_assert(KeyBits > 0, "counting_sort_reorder: KeyBits must be positive");
  static_assert(Bins > 0, "counting_sort_reorder: Bins must be positive");
  static_assert(N > 0, "counting_sort_reorder: N must be positive");
  static_assert(Bins == detail::bin_domain<KeyBits, Bins>::full_bins,
                "counting_sort_reorder: Bins must cover the full key domain");
  static_assert(Bins <= detail::max_complete_bins,
                "counting_sort_reorder: Bins above 256 require radix passes");

  count_t<N> counts[Bins];
  count_t<N> offsets[Bins];
  count_t<N> cursor[Bins];
#pragma HLS array_partition variable=counts complete
#pragma HLS array_partition variable=offsets complete
#pragma HLS array_partition variable=cursor complete

  histogram<KeyBits, Bins>(keys, counts);
  exclusive_scan<Bins>(counts, offsets);

  for (int bin = 0; bin < Bins; ++bin) {
#pragma HLS unroll
    cursor[bin] = offsets[bin];
  }

  for (int i = 0; i < N; ++i) {
    const int bin = static_cast<int>(keys[i].to_uint());
    const int dst = static_cast<int>(cursor[bin].to_uint());
    cursor[bin] = cursor[bin] + 1;
    out_keys[dst] = keys[i];
    out_payloads[dst] = payloads[i];
  }
}

}  // namespace compute
}  // namespace hls
}  // namespace anvil
