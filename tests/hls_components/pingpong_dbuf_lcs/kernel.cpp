#include <anvil/hls.hpp>

#include <ap_int.h>

namespace ahls = anvil::hls;

struct ComponentDbufPolicy {
  typedef const ap_uint<32>* input_t;
  typedef ap_uint<32>* output_t;
  typedef ahls::mem::tile<ap_uint<32>, 8> tile_t;
  static const int TileSize = tile_t::size;

  static void load(input_t in, int tile_id, tile_t& dst) {
#pragma HLS inline
    ahls::mem::load_burst<4>(in, tile_id * TileSize, dst);
  }

  static void compute(int tile_id, tile_t& src, tile_t& dst) {
#pragma HLS inline
    for (int i = 0; i < TileSize; ++i) {
#pragma HLS pipeline II=1
      dst[i] = src[i] + tile_id;
    }
  }

  static void store(output_t out, int tile_id, const tile_t& src) {
#pragma HLS inline
    ahls::mem::store_burst<4>(out, tile_id * TileSize, src);
  }
};

extern "C" void component_pingpong_dbuf_lcs(const ap_uint<32>* in,
                                            ap_uint<32>* out,
                                            int n_tiles) {
#pragma HLS interface m_axi port=in bundle=gmem0 depth=64
#pragma HLS interface m_axi port=out bundle=gmem1 depth=64
#pragma HLS interface s_axilite port=n_tiles bundle=control
#pragma HLS interface s_axilite port=return bundle=control

  ahls::dataflow::dbuf_lcs<ComponentDbufPolicy>(in, out, n_tiles);
}
