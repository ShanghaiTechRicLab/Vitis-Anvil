#include "kernels/vadd_stream.hpp"

#include <cstdio>
#include <vector>

static const int kDepth = 1024;
static const int kPacks = 4;
static const int kW = kernels::kPipelinePack;
static const int kTransactions = 2;

namespace {

int RunTransaction(int tx) {
  std::vector<kernels::PipelinePack> b(kDepth), out(kDepth);
  hls::stream<kernels::PipelinePack> s_in;

  for (int p = 0; p < kPacks; ++p) {
    kernels::PipelinePack pz;
    for (int j = 0; j < kW; ++j) {
      pz.Set(j, static_cast<float>(tx * 100 + p * kW + j));  // stream values
      b[p].Set(j, static_cast<float>(10 + tx));              // DDR b
      out[p].Set(j, 0.0f);
    }
    s_in.write(pz);
  }

  vadd_stream(s_in, b.data(), out.data(), kPacks);

  int fail = 0;
  for (int p = 0; p < kPacks; ++p) {
    for (int j = 0; j < kW; ++j) {
      const float expected = static_cast<float>(tx * 100 + p * kW + j + 10 + tx);
      const float got = out[p][j];
      if (got != expected) {
        std::printf("FAIL tx=%d p=%d j=%d got=%g exp=%g\n", tx, p, j, got, expected);
        ++fail;
      }
    }
  }
  return fail != 0;
}

}  // namespace

int main() {
  int fail = 0;
  for (int tx = 0; tx < kTransactions; ++tx) {
    fail += RunTransaction(tx);
  }
  std::printf("vadd_stream cosim: %s transactions=%d\n", fail == 0 ? "PASS" : "FAIL", kTransactions);
  return fail != 0;
}
