#include "kernels/saxpy_kernel.hpp"

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {

constexpr int kN = 65;
constexpr int kWidth = 16;
constexpr int kPacks = (kN + kWidth - 1) / kWidth;
// Match the m_axi depth pragmas in saxpy_kernel.cpp. Vitis cosim's C wrapper
// dumps the declared interface depth, not just the n_total-active packs.
constexpr int kInterfaceDepthPacks = 8192;
constexpr int kTransactions = 2;

std::uint32_t FloatBits(float value) {
  std::uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

float XValue(int tx, int i) {
  return static_cast<float>(tx * 100 + i - 32);
}

float YValue(int tx, int i) {
  return static_cast<float>(1000 + tx * 17 - 3 * i);
}

int RunTransaction(int tx) {
  const float a = 2.0f + static_cast<float>(tx);

  std::vector<SaxpyPack> x_p(kInterfaceDepthPacks);
  std::vector<SaxpyPack> y_p(kInterfaceDepthPacks);
  std::vector<SaxpyPack> out_p(kInterfaceDepthPacks);
  std::vector<float> gold(kN);

  for (int p = 0; p < kPacks; ++p) {
    for (int lane = 0; lane < kWidth; ++lane) {
      const int i = p * kWidth + lane;
      const float x = XValue(tx, i);
      const float y = YValue(tx, i);
      x_p[p].Set(lane, x);
      y_p[p].Set(lane, y);
      out_p[p].Set(lane, 0.0f);
      if (i < kN) {
        gold[i] = a * x + y;
      }
    }
  }

  saxpy(x_p.data(), y_p.data(), out_p.data(), a, kN);

  for (int i = 0; i < kN; ++i) {
    const int p = i / kWidth;
    const int lane = i % kWidth;
    const float got = out_p[p][lane];
    const float expected = gold[i];
    if (FloatBits(got) != FloatBits(expected)) {
      std::printf("saxpy cosim: FAIL tx=%d i=%d got=%g expected=%g got_bits=0x%08x expected_bits=0x%08x\n",
                  tx, i, got, expected, FloatBits(got), FloatBits(expected));
      return 1;
    }
  }
  return 0;
}

}  // namespace

int main() {
  static_assert(SaxpyPack::kWidth == kWidth, "unexpected SaxpyPack width");

  for (int tx = 0; tx < kTransactions; ++tx) {
    if (RunTransaction(tx) != 0) {
      return 1;
    }
  }

  std::printf("saxpy cosim: PASS transactions=%d\n", kTransactions);
  return 0;
}
