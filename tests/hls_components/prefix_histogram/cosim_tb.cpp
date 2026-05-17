#include <ap_int.h>

#include <cstdio>

extern "C" void component_prefix_histogram(const ap_uint<2>* keys,
                                            ap_uint<4>* out);

int main() {
  ap_uint<2> keys[8] = {0, 1, 3, 1, 2, 3, 3, 0};
  ap_uint<4> out[8] = {};
  const int expected[8] = {2, 2, 1, 3, 0, 2, 4, 5};

  component_prefix_histogram(keys, out);

  for (int i = 0; i < 8; ++i) {
    if (out[i] != expected[i]) {
      std::printf("component_prefix_histogram mismatch at %d: got %u expected %d\n",
                  i,
                  static_cast<unsigned>(out[i]),
                  expected[i]);
      return 1;
    }
  }
  std::printf("component_prefix_histogram: PASS\n");
  return 0;
}
