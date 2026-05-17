#pragma once

namespace anvil {
namespace hls {
namespace mem {

template <typename TileT>
struct pingpong {
  typedef TileT tile_type;

  TileT buffers[2];

  TileT& operator[](int i) {
#pragma HLS inline
    return buffers[i & 1];
  }

  const TileT& operator[](int i) const {
#pragma HLS inline
    return buffers[i & 1];
  }

  TileT& read(int tile_id) {
#pragma HLS inline
    return buffers[tile_id & 1];
  }

  const TileT& read(int tile_id) const {
#pragma HLS inline
    return buffers[tile_id & 1];
  }

  TileT& write(int tile_id) {
#pragma HLS inline
    return buffers[tile_id & 1];
  }
};

}  // namespace mem
}  // namespace hls
}  // namespace anvil
