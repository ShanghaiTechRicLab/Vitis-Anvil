#pragma once

#include <anvil/hls/mem/pingpong.hpp>

namespace anvil {
namespace hls {
namespace dataflow {

template <typename Policy>
void double_buffered_load_compute_store(typename Policy::input_t in,
                                        typename Policy::output_t out,
                                        int n_tiles) {
#pragma HLS inline off
  typedef typename Policy::tile_t tile_t;

  anvil::hls::mem::pingpong<tile_t> input_tiles;
  anvil::hls::mem::pingpong<tile_t> output_tiles;

  if (n_tiles <= 0) {
    return;
  }

  Policy::load(in, 0, input_tiles.write(0));

  // Correctness skeleton: ping-pong banks are explicit, but this function does
  // not claim load/compute/store overlap. A loop-level pipeline pragma here
  // would still serialize calls within each C iteration and misrepresent the
  // generated hardware.
  for (int t = 1; t < n_tiles; ++t) {
    Policy::load(in, t, input_tiles.write(t));
    Policy::compute(t - 1, input_tiles.read(t - 1), output_tiles.write(t - 1));
    Policy::store(out, t - 1, output_tiles.read(t - 1));
  }

  const int last = n_tiles - 1;
  Policy::compute(last, input_tiles.read(last), output_tiles.write(last));
  Policy::store(out, last, output_tiles.read(last));
}

template <typename Policy>
void dbuf_lcs(typename Policy::input_t in,
              typename Policy::output_t out,
              int n_tiles) {
#pragma HLS inline
  double_buffered_load_compute_store<Policy>(in, out, n_tiles);
}

}  // namespace dataflow
}  // namespace hls
}  // namespace anvil
