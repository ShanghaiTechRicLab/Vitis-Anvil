#include <ap_int.h>

#include <cstdio>

extern "C" void component_line_window(const ap_uint<16>* in, ap_uint<16>* out);

int main() {
  ap_uint<16> in[16];
  ap_uint<16> out[16];
  for (int i = 0; i < 16; ++i) {
    in[i] = i + 1;
    out[i] = 0;
  }

  component_line_window(in, out);

  for (int i = 0; i < 8; ++i) {
    const ap_uint<16> previous = (i >= 4) ? in[i - 4] : ap_uint<16>(0);
    const ap_uint<16> expected = in[i] + previous;
    if (out[i] != expected) {
      std::printf("component_line_window mismatch at %d: got %u expected %u\n",
                  i,
                  static_cast<unsigned>(out[i]),
                  static_cast<unsigned>(expected));
      return 1;
    }
  }
  std::printf("component_line_window: PASS\n");
  return 0;
}
