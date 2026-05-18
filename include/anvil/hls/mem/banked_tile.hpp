#pragma once

namespace anvil {
namespace hls {
namespace mem {

template <typename T, int Depth, int Banks>
struct banked_tile {
  static_assert(Depth > 0, "banked_tile: Depth must be positive");
  static_assert(Banks > 0, "banked_tile: Banks must be positive");

  typedef T value_type;
  static const int depth = Depth;
  static const int banks = Banks;

  T data[Banks][Depth];

  void partition_banks() {
#pragma HLS inline
#pragma HLS array_partition variable=data complete dim=1
  }

  T* bank(int bank_id) {
#pragma HLS inline
    return data[bank_id];
  }

  const T* bank(int bank_id) const {
#pragma HLS inline
    return data[bank_id];
  }

  T& at(int bank_id, int index) {
#pragma HLS inline
    return data[bank_id][index];
  }

  const T& at(int bank_id, int index) const {
#pragma HLS inline
    return data[bank_id][index];
  }

  template <int Bank, int Index>
  T get() const {
#pragma HLS inline
    static_assert(Bank >= 0 && Bank < Banks, "banked_tile::get bank out of range");
    static_assert(Index >= 0 && Index < Depth,
                  "banked_tile::get index out of range");
    return data[Bank][Index];
  }

  template <int Bank, int Index>
  void set(const T& value) {
#pragma HLS inline
    static_assert(Bank >= 0 && Bank < Banks, "banked_tile::set bank out of range");
    static_assert(Index >= 0 && Index < Depth,
                  "banked_tile::set index out of range");
    data[Bank][Index] = value;
  }

  void fill(const T& value) {
#pragma HLS inline
    for (int bank_id = 0; bank_id < Banks; ++bank_id) {
      for (int index = 0; index < Depth; ++index) {
#pragma HLS pipeline II=1
        data[bank_id][index] = value;
      }
    }
  }
};

}  // namespace mem
}  // namespace hls
}  // namespace anvil
