#pragma once

namespace anvil {
namespace hls {
namespace mem {

template <typename T, int Depth, int Banks>
struct banked {
  static_assert(Depth > 0, "banked: Depth must be positive");
  static_assert(Banks > 0, "banked: Banks must be positive");

  typedef T value_type;
  static const int depth = Depth;
  static const int banks = Banks;

  T data[Banks][Depth];

  void partition() {
#pragma HLS inline
#pragma HLS array_partition variable=data complete dim=1
  }

  T* bank(int b) {
#pragma HLS inline
    return data[b];
  }

  const T* bank(int b) const {
#pragma HLS inline
    return data[b];
  }

  T& at(int b, int i) {
#pragma HLS inline
    return data[b][i];
  }

  const T& at(int b, int i) const {
#pragma HLS inline
    return data[b][i];
  }
};

}  // namespace mem
}  // namespace hls
}  // namespace anvil
