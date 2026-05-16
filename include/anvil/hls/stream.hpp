#pragma once
// Canonical Vitis-Anvil wrapper over hlslib Stream for internal dataflow.

#include <hlslib/xilinx/Stream.h>

namespace anvil {
namespace hls {

// Backward-compatible default: hls_aliases.hpp historically exposed
// Stream<T> as hlslib::Stream<T, 2>. Do not silently change implicit-depth
// call sites.
static const int kDefaultStreamDepth = 2;

// Recommended explicit depth for demo dataflow kernels/models.
static const int kDefaultDataflowStreamDepth = 32;

template <typename T, int Depth = kDefaultStreamDepth>
using Stream = ::hlslib::Stream<T, Depth>;

}  // namespace hls
}  // namespace anvil
