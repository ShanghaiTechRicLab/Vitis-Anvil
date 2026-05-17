#pragma once

namespace anvil {
namespace hls {
namespace mem {

template <typename T, int N>
struct tile {
  static_assert(N > 0, "tile: N must be positive");

  typedef T value_type;
  static const int size = N;

  T data[N];

  T& operator[](int i) {
#pragma HLS inline
    return data[i];
  }

  const T& operator[](int i) const {
#pragma HLS inline
    return data[i];
  }
};

}  // namespace mem
}  // namespace hls
}  // namespace anvil
