#include <anvil/hls.hpp>

#include <ap_int.h>

namespace ahls = anvil::hls;

extern "C" void component_tile_burst(const ap_uint<32>* in,
                                      ap_uint<32>* out,
                                      int base) {
#pragma HLS interface m_axi port=in bundle=gmem0 depth=64
#pragma HLS interface m_axi port=out bundle=gmem1 depth=64
#pragma HLS interface s_axilite port=base bundle=control
#pragma HLS interface s_axilite port=return bundle=control

  ahls::mem::tile<ap_uint<32>, 8> tile;
  ahls::mem::load_burst<4>(in, base, tile);
  for (int i = 0; i < tile.size; ++i) {
#pragma HLS pipeline II=1
    tile[i] = tile[i] + 1;
  }
  ahls::mem::store_burst<4>(out, base, tile);
}
