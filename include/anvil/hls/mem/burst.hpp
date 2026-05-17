#pragma once

#include <anvil/hls/mem/tile.hpp>

namespace anvil {
namespace hls {
namespace mem {

template <int BurstLen, typename T, int N>
void load_burst(const T* in, int base, tile<T, N>& dst) {
#pragma HLS inline off
  static_assert(BurstLen > 0, "load_burst: BurstLen must be positive");
  (void)BurstLen;
  for (int i = 0; i < N; ++i) {
#pragma HLS pipeline II=1
    dst[i] = in[base + i];
  }
}

template <int BurstLen, typename T, int N>
void store_burst(T* out, int base, const tile<T, N>& src) {
#pragma HLS inline off
  static_assert(BurstLen > 0, "store_burst: BurstLen must be positive");
  (void)BurstLen;
  for (int i = 0; i < N; ++i) {
#pragma HLS pipeline II=1
    out[base + i] = src[i];
  }
}

}  // namespace mem
}  // namespace hls
}  // namespace anvil
