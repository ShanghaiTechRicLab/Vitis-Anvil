#pragma once
// Project-owned scalar <-> packed helpers for HLS model adapters.

#include <cstddef>
#include <span>

#include "anvil/hls/pack.hpp"

namespace hls_model {

template <typename PackT>
inline void PackScalars(std::span<const typename anvil::hls::PackTraits<PackT>::value_type> src,
                        PackT* dst) {
  static_assert(anvil::hls::PackTraits<PackT>::width > 0,
                "PackScalars requires a valid Pack type");
  const std::size_t width = static_cast<std::size_t>(anvil::hls::PackTraits<PackT>::width);
  const std::size_t n = src.size();
  const std::size_t n_pack = (n + width - 1) / width;
  for (std::size_t i = 0; i < n_pack; ++i) {
    PackT pack;
    for (std::size_t lane = 0; lane < width; ++lane) {
      const std::size_t idx = i * width + lane;
      anvil::hls::SetLane(pack,
                          static_cast<int>(lane),
                          (idx < n) ? src[idx] : typename anvil::hls::PackTraits<PackT>::value_type{});
    }
    dst[i] = pack;
  }
}

template <typename PackT>
inline void UnpackScalars(const PackT* src,
                          std::span<typename anvil::hls::PackTraits<PackT>::value_type> dst) {
  static_assert(anvil::hls::PackTraits<PackT>::width > 0,
                "UnpackScalars requires a valid Pack type");
  const std::size_t width = static_cast<std::size_t>(anvil::hls::PackTraits<PackT>::width);
  const std::size_t n = dst.size();
  for (std::size_t i = 0; i < n; ++i) {
    dst[i] = anvil::hls::GetLane(src[i / width], static_cast<int>(i % width));
  }
}

}  // namespace hls_model
