#pragma once

#include <anvil/hls/op.hpp>

namespace anvil {
namespace hls {
namespace compute {

namespace detail {

template <typename Op, typename T, int N>
struct tree_reduce_impl {
  static T apply(const T (&values)[N]) {
#pragma HLS inline
    static const int ReducedN = (N + 1) / 2;
    T reduced[ReducedN];
#pragma HLS array_partition variable=reduced complete
    for (int i = 0; i < ReducedN; ++i) {
#pragma HLS unroll
      const int left = 2 * i;
      const int right = left + 1;
      reduced[i] = (right < N) ? Op::apply(values[left], values[right])
                               : values[left];
    }
    return tree_reduce_impl<Op, T, ReducedN>::apply(reduced);
  }
};

template <typename Op, typename T>
struct tree_reduce_impl<Op, T, 1> {
  static T apply(const T (&values)[1]) {
#pragma HLS inline
    return values[0];
  }
};

}  // namespace detail

template <typename Op, typename T, int N>
T reduce(const T (&values)[N]) {
#pragma HLS inline
  static_assert(N > 0, "reduce: N must be positive");
  T acc = Op::identity();
  for (int i = 0; i < N; ++i) {
#pragma HLS pipeline II=1
    acc = Op::apply(acc, values[i]);
  }
  return acc;
}

template <typename Op, typename T, int N>
T tree_reduce(const T (&values)[N]) {
#pragma HLS inline
  static_assert(N > 0, "tree_reduce: N must be positive");
  T local[N];
#pragma HLS array_partition variable=local complete
  for (int i = 0; i < N; ++i) {
#pragma HLS unroll
    local[i] = values[i];
  }
  return detail::tree_reduce_impl<Op, T, N>::apply(local);
}

template <int N, typename AccT, typename T>
AccT sum(const T (&values)[N]) {
#pragma HLS inline
  static_assert(N > 0, "sum: N must be positive");
  AccT acc = 0;
  for (int i = 0; i < N; ++i) {
#pragma HLS pipeline II=1
    acc += static_cast<AccT>(values[i]);
  }
  return acc;
}

template <int N, typename AccT, typename XT, typename WT>
AccT dot(const XT (&x)[N], const WT (&w)[N]) {
#pragma HLS inline
  static_assert(N > 0, "dot: N must be positive");
  AccT acc = 0;
  for (int i = 0; i < N; ++i) {
#pragma HLS pipeline II=1
    acc += static_cast<AccT>(x[i]) * static_cast<AccT>(w[i]);
  }
  return acc;
}

}  // namespace compute
}  // namespace hls
}  // namespace anvil
