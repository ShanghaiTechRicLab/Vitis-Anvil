#include "anvil/kernels/vadd_stream.hpp"

#include <cstdio>
#include <vector>

static const int kDepth = 1024;
static const int kPacks = 4;
static const int kW = anvil::kernels::kPipelinePack;

int main() {
  std::vector<anvil::kernels::PipelinePack> b(kDepth), out(kDepth);
  hls::stream<anvil::kernels::PipelinePack> s_in;

  for (int p = 0; p < kPacks; ++p) {
    anvil::kernels::PipelinePack pz;
    for (int j = 0; j < kW; ++j) {
      pz.Set(j, static_cast<float>(p * kW + j));  // stream values
      b[p].Set(j, 10.0f);                         // DDR b
    }
    s_in.write(pz);
  }

  vadd_stream(s_in, b.data(), out.data(), kPacks);

  int fail = 0;
  for (int p = 0; p < kPacks; ++p) {
    for (int j = 0; j < kW; ++j) {
      const float expected = static_cast<float>(p * kW + j) + 10.0f;
      const float got = out[p][j];
      if (got != expected) {
        std::printf("FAIL p=%d j=%d got=%g exp=%g\n", p, j, got, expected);
        ++fail;
      }
    }
  }
  std::printf("vadd_stream cosim: %s\n", fail == 0 ? "PASS" : "FAIL");
  return fail != 0;
}
