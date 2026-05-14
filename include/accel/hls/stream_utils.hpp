#pragma once
// Pack/Unpack helpers used by saxpy_hls_model (CPU). The kernel uses raw
// pointer arithmetic and m_axi directly, so it does not link this header.

#include <cstddef>
#include <span>           // C++20; kernel-side code MUST NOT include this header

#include "accel/hls/data_pack.hpp"

namespace accel::hls {

// Packs `n` scalar floats into ceil(n/W) SaxpyPack values. Tail lanes are
// zero-filled. `dst` MUST have at least (n + W - 1) / W entries.
inline void Pack(std::span<const float> src, SaxpyPack* dst) {
  constexpr int W = accel::config::kParallelism;
  const int n = static_cast<int>(src.size());
  const int n_pack = (n + W - 1) / W;
  for (int i = 0; i < n_pack; ++i) {
    SaxpyPack p;
    for (int j = 0; j < W; ++j) {
      const int idx = i * W + j;
      p[j] = (idx < n) ? src[idx] : 0.0f;
    }
    dst[i] = p;
  }
}

// Unpacks the first `n` floats from packed source into a scalar span.
// `src` MUST have at least (n + W - 1) / W entries; extra tail lanes are
// discarded.
inline void Unpack(const SaxpyPack* src, std::span<float> dst) {
  constexpr int W = accel::config::kParallelism;
  const int n = static_cast<int>(dst.size());
  for (int i = 0; i < n; ++i) {
    dst[i] = src[i / W][i % W];
  }
}

}  // namespace accel::hls
