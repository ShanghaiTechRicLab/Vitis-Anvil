#pragma once

#include <anvil/hls/util.hpp>

#include <ap_int.h>

namespace anvil {
namespace hls {
namespace compute {

namespace detail {

static const int max_complete_bins = 256;

template <int N>
struct count_width {
  static_assert(N > 0, "count width requires positive N");
  static const int raw = anvil::hls::util::ceil_log2(N + 1);
  static const int value = (raw > 0) ? raw : 1;
};

template <int KeyBits, int Bins>
struct bin_domain {
  static_assert(KeyBits > 0, "bin_domain: KeyBits must be positive");
  static_assert(Bins > 0, "bin_domain: Bins must be positive");
  static_assert(KeyBits <= 30,
                "bin_domain: KeyBits must be <= 30 for host int bin indexing");
  static_assert(Bins <= max_complete_bins,
                "bin_domain: Bins above 256 require a BRAM-backed histogram");
  static const int full_bins = 1 << KeyBits;
};

}  // namespace detail

template <int N>
using count_t = ap_uint<detail::count_width<N>::value>;

template <int Bins, typename CountT>
void histogram_reset(CountT (&counts)[Bins]) {
#pragma HLS inline
  static_assert(Bins > 0, "histogram_reset: Bins must be positive");
  static_assert(Bins <= detail::max_complete_bins,
                "histogram_reset: Bins above 256 require a sequential reset");
  for (int bin = 0; bin < Bins; ++bin) {
#pragma HLS unroll
    counts[bin] = 0;
  }
}

template <int KeyBits, int Bins, int N, typename CountT>
void histogram_accumulate(const ap_uint<KeyBits> (&keys)[N], CountT (&counts)[Bins]) {
#pragma HLS inline
  static_assert(KeyBits > 0, "histogram_accumulate: KeyBits must be positive");
  static_assert(Bins > 0, "histogram_accumulate: Bins must be positive");
  static_assert(N > 0, "histogram_accumulate: N must be positive");
  static_assert(Bins <= detail::bin_domain<KeyBits, Bins>::full_bins,
                "histogram_accumulate: Bins must fit in KeyBits");

  for (int i = 0; i < N; ++i) {
    const int bin = static_cast<int>(keys[i].to_uint());
    if (bin < Bins) {
      counts[bin] = counts[bin] + 1;
    }
  }
}

template <int KeyBits, int Bins, int N, typename CountT>
void histogram(const ap_uint<KeyBits> (&keys)[N], CountT (&counts)[Bins]) {
#pragma HLS inline
  histogram_reset<Bins>(counts);
  histogram_accumulate<KeyBits, Bins>(keys, counts);
}

template <typename KeyT, int N, int NumBins, typename BinPolicy, typename CountT>
void histogram(const KeyT (&keys)[N], CountT (&bins)[NumBins]) {
#pragma HLS inline
  static_assert(N > 0, "histogram: N must be positive");
  static_assert(NumBins > 0, "histogram: NumBins must be positive");
  static_assert(NumBins <= detail::max_complete_bins,
                "histogram: NumBins above 256 require a BRAM-backed histogram");
  histogram_reset<NumBins>(bins);
  for (int i = 0; i < N; ++i) {
    const int bin = BinPolicy::bin(keys[i]);
    if (bin >= 0 && bin < NumBins) {
      bins[bin] = bins[bin] + 1;
    }
  }
}

}  // namespace compute
}  // namespace hls
}  // namespace anvil
