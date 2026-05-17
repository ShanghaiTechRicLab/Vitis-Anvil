#pragma once

#include <anvil/hls/op.hpp>

namespace anvil {
namespace hls {
namespace compute {

template <typename T, typename Compare>
void compare_swap(T& a, T& b) {
#pragma HLS inline
  if (Compare::before(b, a)) {
    T tmp = a;
    a = b;
    b = tmp;
  }
}

template <int N, typename T, typename Compare>
void bitonic_sort(T (&values)[N]) {
#pragma HLS inline
  static_assert(N > 0 && ((N & (N - 1)) == 0),
                "bitonic_sort: N must be a power of two");
  for (int k = 2; k <= N; k <<= 1) {
    for (int j = k >> 1; j > 0; j >>= 1) {
      for (int i = 0; i < N; ++i) {
#pragma HLS unroll
        const int ixj = i ^ j;
        if (ixj > i) {
          const bool up = ((i & k) == 0);
          if (up) {
            compare_swap<T, Compare>(values[i], values[ixj]);
          } else {
            compare_swap<T, Compare>(values[ixj], values[i]);
          }
        }
      }
    }
  }
}

template <int N, typename T>
void sort(T (&values)[N]) {
#pragma HLS inline
  bitonic_sort<N, T, anvil::hls::op::less<T> >(values);
}

template <typename Compare, int N, typename T>
void sort(T (&values)[N]) {
#pragma HLS inline
  bitonic_sort<N, T, Compare>(values);
}

}  // namespace compute
}  // namespace hls
}  // namespace anvil
