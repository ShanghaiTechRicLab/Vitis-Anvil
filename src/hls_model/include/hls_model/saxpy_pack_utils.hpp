#pragma once
// Project-owned scalar <-> packed helpers for the saxpy HLS model. These are
// not part of the framework API because they depend on the example SaxpyPack.

#include <cstddef>
#include <span>

#include "anvil/hls/pack.hpp"
#include "kernels/kernel_types.hpp"

namespace hls_model {

inline void PackScalars(std::span<const float> src, kernels::SaxpyPack* dst) {
  const std::size_t width = static_cast<std::size_t>(kernels::kSaxpyPackWidth);
  const std::size_t n = src.size();
  const std::size_t n_pack = (n + width - 1) / width;
  for (std::size_t i = 0; i < n_pack; ++i) {
    kernels::SaxpyPack pack;
    for (std::size_t lane = 0; lane < width; ++lane) {
      const std::size_t idx = i * width + lane;
      anvil::hls::SetLane(pack, static_cast<int>(lane), (idx < n) ? src[idx] : 0.0f);
    }
    dst[i] = pack;
  }
}

inline void UnpackScalars(const kernels::SaxpyPack* src, std::span<float> dst) {
  const std::size_t width = static_cast<std::size_t>(kernels::kSaxpyPackWidth);
  const std::size_t n = dst.size();
  for (std::size_t i = 0; i < n; ++i) {
    dst[i] = anvil::hls::GetLane(src[i / width], static_cast<int>(i % width));
  }
}

}  // namespace hls_model
