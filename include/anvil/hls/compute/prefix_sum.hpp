#pragma once

namespace anvil {
namespace hls {
namespace compute {

template <int N, typename T>
void inclusive_scan(const T (&in)[N], T (&out)[N]) {
#pragma HLS inline
  static_assert(N > 0, "inclusive_scan: N must be positive");
  T acc = 0;
  for (int i = 0; i < N; ++i) {
    acc += in[i];
    out[i] = acc;
  }
}

template <int N, typename T>
void exclusive_scan(const T (&in)[N], T (&out)[N]) {
#pragma HLS inline
  static_assert(N > 0, "exclusive_scan: N must be positive");
  T acc = 0;
  for (int i = 0; i < N; ++i) {
    const T current = in[i];
    out[i] = acc;
    acc += current;
  }
}

template <int N, typename T>
void prefix_sum(const T (&in)[N], T (&out)[N]) {
#pragma HLS inline
  exclusive_scan<N>(in, out);
}

template <int N, typename T>
void prefix_sum_inplace(T (&values)[N]) {
#pragma HLS inline
  static_assert(N > 0, "prefix_sum_inplace: N must be positive");
  T acc = 0;
  for (int i = 0; i < N; ++i) {
    const T current = values[i];
    values[i] = acc;
    acc += current;
  }
}

template <int N, typename T>
void inclusive_scan_inplace(T (&values)[N]) {
#pragma HLS inline
  static_assert(N > 0, "inclusive_scan_inplace: N must be positive");
  T acc = 0;
  for (int i = 0; i < N; ++i) {
    acc += values[i];
    values[i] = acc;
  }
}

}  // namespace compute
}  // namespace hls
}  // namespace anvil
