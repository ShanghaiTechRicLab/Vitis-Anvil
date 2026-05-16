#include "anvil/kernels/saxpy_stream.hpp"

#include <cstdio>
#include <vector>

// Allocate full m_axi interface depth (matches depth=1024 in pragma).
static const int kDepth = 1024;
static const int kPacks = 4;
static const int kW = anvil::kernels::kPipelinePack;

int main() {
  const float A = 2.0f;
  std::vector<anvil::kernels::PipelinePack> x(kDepth), y(kDepth);
  for (int p = 0; p < kPacks; ++p) {
    for (int j = 0; j < kW; ++j) {
      x[p].Set(j, static_cast<float>(p * kW + j));
      y[p].Set(j, 1.0f);
    }
  }

  hls::stream<anvil::kernels::PipelinePack> s_out;
  saxpy_stream(x.data(), y.data(), s_out, A, kPacks);

  int fail = 0;
  for (int p = 0; p < kPacks; ++p) {
    anvil::kernels::PipelinePack pack = s_out.read();
    for (int j = 0; j < kW; ++j) {
      const float expected = A * x[p][j] + y[p][j];
      const float got = pack[j];
      if (got != expected) {
        std::printf("FAIL p=%d j=%d got=%g exp=%g\n", p, j, got, expected);
        ++fail;
      }
    }
  }
  std::printf("saxpy_stream cosim: %s\n", fail == 0 ? "PASS" : "FAIL");
  return fail != 0;
}
