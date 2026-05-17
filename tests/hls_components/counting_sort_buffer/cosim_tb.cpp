#include <ap_int.h>

#include <cstdio>

extern "C" void component_counting_sort_buffer(const ap_uint<2>* in_keys,
                                                const ap_uint<16>* in_payloads,
                                                ap_uint<2>* out_keys,
                                                ap_uint<16>* out_payloads);

int main() {
  ap_uint<2> keys[8] = {2, 1, 2, 0, 1, 3, 0, 2};
  ap_uint<16> payloads[8] = {20, 10, 21, 0, 11, 30, 1, 22};
  ap_uint<2> out_keys[8] = {};
  ap_uint<16> out_payloads[8] = {};
  const int expected_keys[8] = {0, 0, 1, 1, 2, 2, 2, 3};
  const int expected_payloads[8] = {0, 1, 10, 11, 20, 21, 22, 30};

  component_counting_sort_buffer(keys, payloads, out_keys, out_payloads);

  for (int i = 0; i < 8; ++i) {
    if (out_keys[i] != expected_keys[i] || out_payloads[i] != expected_payloads[i]) {
      std::printf("component_counting_sort_buffer mismatch at %d: got (%u,%u) expected (%d,%d)\n",
                  i,
                  static_cast<unsigned>(out_keys[i]),
                  static_cast<unsigned>(out_payloads[i]),
                  expected_keys[i],
                  expected_payloads[i]);
      return 1;
    }
  }
  std::printf("component_counting_sort_buffer: PASS\n");
  return 0;
}
