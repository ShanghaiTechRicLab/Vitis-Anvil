#include <ap_int.h>

#include <cstdio>

extern "C" void component_tile_burst(const ap_uint<32>* in,
                                      ap_uint<32>* out,
                                      int base);

int main() {
  ap_uint<32> in[64];
  ap_uint<32> out[64];
  for (int i = 0; i < 64; ++i) {
    in[i] = i;
    out[i] = 0;
  }

  component_tile_burst(in, out, 4);

  for (int i = 4; i < 12; ++i) {
    if (out[i] != in[i] + 1) {
      std::printf("component_tile_burst mismatch at %d\n", i);
      return 1;
    }
  }
  std::printf("component_tile_burst: PASS\n");
  return 0;
}
