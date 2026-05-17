#pragma once

#include <anvil/hls/mem/multi_buffer.hpp>

namespace anvil {
namespace hls {
namespace mem {

template <typename TileT>
struct triple_buffer {
  typedef TileT tile_type;
  static const int slots = 3;

  multi_buffer<TileT, slots> storage;

  TileT& at(int slot_id) {
#pragma HLS inline
    return storage.slot(slot_id);
  }

  const TileT& at(int slot_id) const {
#pragma HLS inline
    return storage.slot(slot_id);
  }

  TileT& load(int tile_id) {
#pragma HLS inline
    return storage.slot_for(tile_id);
  }

  const TileT& load(int tile_id) const {
#pragma HLS inline
    return storage.slot_for(tile_id);
  }

  TileT& compute(int tile_id) {
#pragma HLS inline
    return storage.slot_for(tile_id);
  }

  const TileT& compute(int tile_id) const {
#pragma HLS inline
    return storage.slot_for(tile_id);
  }

  TileT& store(int tile_id) {
#pragma HLS inline
    return storage.slot_for(tile_id);
  }

  const TileT& store(int tile_id) const {
#pragma HLS inline
    return storage.slot_for(tile_id);
  }
};

}  // namespace mem
}  // namespace hls
}  // namespace anvil
