#pragma once

#include "kernels/kernel_types.hpp"

#include "anvil/hls/packed_ops.hpp"
#include "anvil/hls/stream.hpp"

namespace kernels {
namespace saxpy_core {

typedef anvil::hls::Stream<SaxpyPack, anvil::hls::kDefaultDataflowStreamDepth> SaxpyStream;

struct SaxpyOp {
  float operator()(float a, float x, float y) const { return a * x + y; }
};

inline void Load(const SaxpyPack* in, SaxpyStream& out, int n_pack) {
  anvil::hls::LoadPacks(in, out, n_pack);
}

inline void Compute(SaxpyStream& x, SaxpyStream& y, SaxpyStream& out, float a, int n_pack) {
  anvil::hls::MapPacksWithScalar<SaxpyPack>(x, y, out, a, n_pack, SaxpyOp());
}

inline void Store(SaxpyStream& in, SaxpyPack* out, int n_pack) {
  anvil::hls::StorePacks(in, out, n_pack);
}

}  // namespace saxpy_core
}  // namespace kernels
