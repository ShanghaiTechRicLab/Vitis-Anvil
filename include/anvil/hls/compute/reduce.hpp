#pragma once

#include <anvil/hls/op.hpp>

namespace anvil {
namespace hls {
namespace compute {

template <typename Op, typename T, int N>
T reduce(const T (&values)[N]) {
#pragma HLS inline
  T acc = Op::identity();
  for (int i = 0; i < N; ++i) {
#pragma HLS pipeline II=1
    acc = Op::apply(acc, values[i]);
  }
  return acc;
}

template <int N, typename AccT, typename T>
AccT sum(const T (&values)[N]) {
#pragma HLS inline
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
