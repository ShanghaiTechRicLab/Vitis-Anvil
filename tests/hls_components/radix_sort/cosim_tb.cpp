#include <ap_int.h>

#include <cstdio>

extern "C" void component_radix_sort(const ap_uint<8>* in_keys,
                                      const ap_uint<16>* in_payloads,
                                      ap_uint<8>* out_keys,
                                      ap_uint<16>* out_payloads);

int main() {
  ap_uint<8> in_keys[16] = {42, 3, 255, 0, 17, 3, 128, 9};
  ap_uint<16> in_payloads[16] = {420, 30, 2550, 0, 170, 31, 1280, 90};
  ap_uint<8> out_keys[16] = {};
  ap_uint<16> out_payloads[16] = {};
  const int expected_keys[8] = {0, 3, 3, 9, 17, 42, 128, 255};
  const int expected_payloads[8] = {0, 30, 31, 90, 170, 420, 1280, 2550};

  component_radix_sort(in_keys, in_payloads, out_keys, out_payloads);

  for (int i = 0; i < 8; ++i) {
    if (out_keys[i] != expected_keys[i] || out_payloads[i] != expected_payloads[i]) {
      std::printf("component_radix_sort mismatch at %d: got (%u,%u) expected (%d,%d)\n",
                  i,
                  static_cast<unsigned>(out_keys[i]),
                  static_cast<unsigned>(out_payloads[i]),
                  expected_keys[i],
                  expected_payloads[i]);
      return 1;
    }
  }
  std::printf("component_radix_sort: PASS\n");
  return 0;
}
