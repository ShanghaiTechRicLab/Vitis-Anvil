#pragma once

#include <anvil/hls/compute/sort.hpp>

namespace anvil {
namespace hls {
namespace compute {

template <int N, int K, typename T, typename Compare>
void topk(const T (&in)[N], T (&out)[K]) {
#pragma HLS inline
  static_assert(K > 0, "topk: K must be positive");
  static_assert(K <= N, "topk: K must be <= N");

  T tmp[N];
#pragma HLS array_partition variable=tmp complete
  for (int i = 0; i < N; ++i) {
#pragma HLS unroll
    tmp[i] = in[i];
  }
  bitonic_sort<N, T, Compare>(tmp);
  for (int i = 0; i < K; ++i) {
#pragma HLS unroll
    out[i] = tmp[i];
  }
}

template <typename Compare, int N, int K, typename T>
void topk(const T (&in)[N], T (&out)[K]) {
#pragma HLS inline
  topk<N, K, T, Compare>(in, out);
}

template <int N, int K, typename T>
void topk(const T (&in)[N], T (&out)[K]) {
#pragma HLS inline
  topk<N, K, T, anvil::hls::op::less<T> >(in, out);
}

}  // namespace compute
}  // namespace hls
}  // namespace anvil
