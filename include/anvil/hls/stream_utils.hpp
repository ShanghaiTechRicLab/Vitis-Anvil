#pragma once
// Pack/Unpack helpers for host-side HLS model code. Kernel-side code should
// avoid this header because std::span is a C++20 host dependency.

#include <cstddef>
#include <span>

#include "anvil/hls/data_pack.hpp"

namespace anvil::hls {

// Packs `n` scalar floats into ceil(n/W) SaxpyPack values. Tail lanes are
// zero-filled. `dst` MUST have at least (n + W - 1) / W entries.
inline void Pack(std::span<const float> src, SaxpyPack* dst) {
  constexpr std::size_t W = static_cast<std::size_t>(anvil::config::kParallelism);
  const std::size_t n = src.size();
  const std::size_t n_pack = (n + W - 1) / W;
  for (std::size_t i = 0; i < n_pack; ++i) {
    SaxpyPack p;
    for (std::size_t j = 0; j < W; ++j) {
      const std::size_t idx = i * W + j;
      p[j] = (idx < n) ? src[idx] : 0.0f;
    }
    dst[i] = p;
  }
}

// Unpacks the first `n` floats from packed source into a scalar span.
// `src` MUST have at least (n + W - 1) / W entries; extra tail lanes are
// discarded.
inline void Unpack(const SaxpyPack* src, std::span<float> dst) {
  constexpr std::size_t W = static_cast<std::size_t>(anvil::config::kParallelism);
  const std::size_t n = dst.size();
  for (std::size_t i = 0; i < n; ++i) {
    dst[i] = src[i / W][i % W];
  }
}

}  // namespace anvil::hls
