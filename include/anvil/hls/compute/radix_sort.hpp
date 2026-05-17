#pragma once

#include <anvil/hls/util.hpp>

#include <ap_int.h>

namespace anvil {
namespace hls {
namespace compute {

namespace detail {

template <int KeyBits, int RadixBits>
inline int radix_digit(ap_uint<KeyBits> key, int pass) {
#pragma HLS inline
  static_assert(KeyBits > 0, "radix_digit: KeyBits must be positive");
  static_assert(RadixBits > 0, "radix_digit: RadixBits must be positive");
  static_assert(RadixBits <= KeyBits,
                "radix_digit: RadixBits must not exceed KeyBits");
  static_assert(RadixBits <= 8,
                "radix_digit: RadixBits above 8 creates large histograms");
  static const int Mask = (1 << RadixBits) - 1;
  return static_cast<int>((key >> (pass * RadixBits)) & Mask);
}

}  // namespace detail

template <int KeyBits, int RadixBits, int N, typename PayloadT>
void radix_sort_by_key(ap_uint<KeyBits> (&keys)[N], PayloadT (&payloads)[N]) {
#pragma HLS inline
  static_assert(N > 0, "radix_sort_by_key: N must be positive");
  static_assert(KeyBits > 0, "radix_sort_by_key: KeyBits must be positive");
  static_assert(RadixBits > 0, "radix_sort_by_key: RadixBits must be positive");
  static_assert(RadixBits <= KeyBits,
                "radix_sort_by_key: RadixBits must not exceed KeyBits");
  static_assert(RadixBits <= 8,
                "radix_sort_by_key: RadixBits above 8 creates large histograms");

  static const int Radix = 1 << RadixBits;
  static const int Passes = (KeyBits + RadixBits - 1) / RadixBits;
  static const int CountBits = anvil::hls::util::ceil_log2(N + 1);
  typedef ap_uint<CountBits> count_t;

  ap_uint<KeyBits> key_tmp[N];
  PayloadT payload_tmp[N];
  count_t hist[Radix];
  count_t cursor[Radix];
#pragma HLS array_partition variable=hist complete
#pragma HLS array_partition variable=cursor complete

  for (int pass = 0; pass < Passes; ++pass) {
    for (int b = 0; b < Radix; ++b) {
#pragma HLS unroll
      hist[b] = 0;
      cursor[b] = 0;
    }

    for (int i = 0; i < N; ++i) {
      const int digit = detail::radix_digit<KeyBits, RadixBits>(keys[i], pass);
      hist[digit] = hist[digit] + 1;
    }

    count_t sum = 0;
    for (int b = 0; b < Radix; ++b) {
#pragma HLS unroll
      const count_t count = hist[b];
      cursor[b] = sum;
      sum += count;
    }

    for (int i = 0; i < N; ++i) {
      const int digit = detail::radix_digit<KeyBits, RadixBits>(keys[i], pass);
      const int dst = static_cast<int>(cursor[digit]);
      cursor[digit] = cursor[digit] + 1;
      key_tmp[dst] = keys[i];
      payload_tmp[dst] = payloads[i];
    }

    for (int i = 0; i < N; ++i) {
#pragma HLS pipeline II=1
      keys[i] = key_tmp[i];
      payloads[i] = payload_tmp[i];
    }
  }
}

template <int KeyBits, int RadixBits, int N>
void radix_sort(ap_uint<KeyBits> (&keys)[N]) {
#pragma HLS inline
  static_assert(N > 0, "radix_sort: N must be positive");
  static_assert(KeyBits > 0, "radix_sort: KeyBits must be positive");
  static_assert(RadixBits > 0, "radix_sort: RadixBits must be positive");
  static_assert(RadixBits <= KeyBits,
                "radix_sort: RadixBits must not exceed KeyBits");
  static_assert(RadixBits <= 8,
                "radix_sort: RadixBits above 8 creates large histograms");

  static const int Radix = 1 << RadixBits;
  static const int Passes = (KeyBits + RadixBits - 1) / RadixBits;
  static const int CountBits = anvil::hls::util::ceil_log2(N + 1);
  typedef ap_uint<CountBits> count_t;

  ap_uint<KeyBits> key_tmp[N];
  count_t hist[Radix];
  count_t cursor[Radix];
#pragma HLS array_partition variable=hist complete
#pragma HLS array_partition variable=cursor complete

  for (int pass = 0; pass < Passes; ++pass) {
    for (int b = 0; b < Radix; ++b) {
#pragma HLS unroll
      hist[b] = 0;
      cursor[b] = 0;
    }

    for (int i = 0; i < N; ++i) {
      const int digit = detail::radix_digit<KeyBits, RadixBits>(keys[i], pass);
      hist[digit] = hist[digit] + 1;
    }

    count_t sum = 0;
    for (int b = 0; b < Radix; ++b) {
#pragma HLS unroll
      const count_t count = hist[b];
      cursor[b] = sum;
      sum += count;
    }

    for (int i = 0; i < N; ++i) {
      const int digit = detail::radix_digit<KeyBits, RadixBits>(keys[i], pass);
      const int dst = static_cast<int>(cursor[digit]);
      cursor[digit] = cursor[digit] + 1;
      key_tmp[dst] = keys[i];
    }

    for (int i = 0; i < N; ++i) {
#pragma HLS pipeline II=1
      keys[i] = key_tmp[i];
    }
  }
}

}  // namespace compute
}  // namespace hls
}  // namespace anvil
