#include <ap_int.h>

#include <cstdio>

extern "C" void component_pingpong_dbuf_lcs(const ap_uint<32>* in,
                                            ap_uint<32>* out,
                                            int n_tiles);

int main() {
  ap_uint<32> in[64];
  ap_uint<32> out[64];
  for (int i = 0; i < 64; ++i) {
    in[i] = i;
    out[i] = 0;
  }

  component_pingpong_dbuf_lcs(in, out, 4);

  for (int tile = 0; tile < 4; ++tile) {
    for (int i = 0; i < 8; ++i) {
      const int idx = tile * 8 + i;
      const ap_uint<32> expected = in[idx] + tile;
      if (out[idx] != expected) {
        std::printf("component_pingpong_dbuf_lcs mismatch at %d\n", idx);
        return 1;
      }
    }
  }
  std::printf("component_pingpong_dbuf_lcs: PASS\n");
  return 0;
}
