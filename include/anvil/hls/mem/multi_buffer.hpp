#pragma once

namespace anvil {
namespace hls {
namespace mem {

template <typename TileT, int Slots>
struct multi_buffer {
  static_assert(Slots > 0, "multi_buffer: Slots must be positive");

  typedef TileT tile_type;
  static const int slots = Slots;

  TileT buffers[Slots];

  TileT& slot(int index) {
#pragma HLS inline
    return buffers[index];
  }

  const TileT& slot(int index) const {
#pragma HLS inline
    return buffers[index];
  }

  template <int Slot>
  TileT& slot() {
#pragma HLS inline
    static_assert(Slot >= 0 && Slot < Slots, "multi_buffer::slot out of range");
    return buffers[Slot];
  }

  template <int Slot>
  const TileT& slot() const {
#pragma HLS inline
    static_assert(Slot >= 0 && Slot < Slots, "multi_buffer::slot out of range");
    return buffers[Slot];
  }

  TileT& slot_for(int tile_id) {
#pragma HLS inline
    const int slot_id = tile_id % Slots;
    return buffers[(slot_id < 0) ? (slot_id + Slots) : slot_id];
  }

  const TileT& slot_for(int tile_id) const {
#pragma HLS inline
    const int slot_id = tile_id % Slots;
    return buffers[(slot_id < 0) ? (slot_id + Slots) : slot_id];
  }
};

}  // namespace mem
}  // namespace hls
}  // namespace anvil
