#pragma once
// Canonical Vitis-Anvil wrappers over hlslib DataPack. Kernel-facing and
// C++14-clean: no std::span, no C++17 nested namespace, no if constexpr.

#include <hlslib/xilinx/DataPack.h>

namespace anvil {
namespace hls {

template <typename T, int N>
using Pack = ::hlslib::DataPack<T, N>;

template <typename PackT>
struct PackTraits;

template <typename T, int N>
struct PackTraits<Pack<T, N> > {
  typedef T value_type;
  static const int width = N;
};

template <typename PackT>
inline typename PackTraits<PackT>::value_type GetLane(const PackT& pack, int lane) {
  return pack[lane];
}

template <typename PackT>
inline void SetLane(PackT& pack, int lane, typename PackTraits<PackT>::value_type value) {
  pack[lane] = value;
}

}  // namespace hls
}  // namespace anvil
