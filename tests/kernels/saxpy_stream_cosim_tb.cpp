#include "anvil/kernels/saxpy_stream.hpp"

#include <cstdio>
#include <vector>

// Allocate full m_axi interface depth (matches depth=1024 in pragma).
static const int kDepth = 1024;
static const int kPacks = 4;
static const int kW = anvil::kernels::kPipelinePack;
static const int kTransactions = 2;

namespace {

int RunTransaction(int tx) {
  const float A = 2.0f + static_cast<float>(tx);
  std::vector<anvil::kernels::PipelinePack> x(kDepth), y(kDepth);
  for (int p = 0; p < kPacks; ++p) {
    for (int j = 0; j < kW; ++j) {
      x[p].Set(j, static_cast<float>(tx * 100 + p * kW + j));
      y[p].Set(j, static_cast<float>(1 + tx));
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
  std::printf("saxpy_stream cosim: %s transactions=%d\n", fail == 0 ? "PASS" : "FAIL", kTransactions);
  return fail != 0;
}
