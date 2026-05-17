#pragma once

namespace anvil {
namespace hls {
namespace mem {

template <typename T, int Depth>
struct scratchpad {
  static_assert(Depth > 0, "scratchpad: Depth must be positive");

  typedef T value_type;
  static const int depth = Depth;

  T data[Depth];

  void partition_complete() {
#pragma HLS inline
#pragma HLS array_partition variable=data complete
  }

  T read(int index) const {
#pragma HLS inline
    return data[index];
  }

  void write(int index, const T& value) {
#pragma HLS inline
    data[index] = value;
  }

  T& at(int index) {
#pragma HLS inline
    return data[index];
  }

  const T& at(int index) const {
#pragma HLS inline
    return data[index];
  }

  template <int Index>
  T get() const {
#pragma HLS inline
    static_assert(Index >= 0 && Index < Depth, "scratchpad::get index out of range");
    return data[Index];
  }

  template <int Index>
  void set(const T& value) {
#pragma HLS inline
    static_assert(Index >= 0 && Index < Depth, "scratchpad::set index out of range");
    data[Index] = value;
  }

  void fill(const T& value) {
#pragma HLS inline
    for (int i = 0; i < Depth; ++i) {
#pragma HLS pipeline II=1
      data[i] = value;
    }
  }
};

}  // namespace mem
}  // namespace hls
}  // namespace anvil
