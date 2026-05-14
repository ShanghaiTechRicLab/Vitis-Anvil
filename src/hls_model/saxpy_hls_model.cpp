#include "anvil/hls/saxpy_hls_model.hpp"

#include <hlslib/xilinx/Simulation.h>
#include <hlslib/xilinx/Stream.h>

#include <cstddef>
#include <stdexcept>
#include <vector>

#include "anvil/hls/data_pack.hpp"
#include "anvil/hls/stream_utils.hpp"

namespace anvil::hls {

namespace {

constexpr std::size_t kStreamDepth = 32;

void Load(const SaxpyPack* in, hlslib::Stream<SaxpyPack, kStreamDepth>& s, int n_pack) {
  for (int i = 0; i < n_pack; ++i) s.Push(in[i]);
}

void Compute(hlslib::Stream<SaxpyPack, kStreamDepth>& xs,
             hlslib::Stream<SaxpyPack, kStreamDepth>& ys,
             hlslib::Stream<SaxpyPack, kStreamDepth>& os,
             float a, int n_pack) {
  constexpr int W = anvil::config::kParallelism;
  for (int i = 0; i < n_pack; ++i) {
    SaxpyPack xv = xs.Pop();
    SaxpyPack yv = ys.Pop();
    SaxpyPack r;
    for (int j = 0; j < W; ++j) {
      r[j] = a * xv[j] + yv[j];
    }
    os.Push(r);
  }
}

void Store(hlslib::Stream<SaxpyPack, kStreamDepth>& s, SaxpyPack* out, int n_pack) {
  for (int i = 0; i < n_pack; ++i) out[i] = s.Pop();
}

}  // namespace

void saxpy_hls_model(std::span<const float> x,
                     std::span<const float> y,
                     std::span<float>       out,
                     const anvil::gold::SaxpyConfig& cfg) {
  if (x.size() != y.size() || x.size() != out.size()) {
    throw std::invalid_argument("saxpy_hls_model: span sizes must match");
  }
  const int n = static_cast<int>(x.size());
  if (n == 0) return;

  constexpr int W = anvil::config::kParallelism;
  const int n_pack = (n + W - 1) / W;

  // Pack inputs; tail lanes get 0.
  std::vector<SaxpyPack> x_packed(n_pack), y_packed(n_pack), out_packed(n_pack);
  Pack(x, x_packed.data());
  Pack(y, y_packed.data());

  hlslib::Stream<SaxpyPack, kStreamDepth> sx("sx"), sy("sy"), so("so");

  // Same NOTE as Phase 1 saxpy_hls_model: do NOT wrap stream args with
  // std::ref. hlslib's AddFunction applies std::ref internally for reference
  // parameters via passed_by(..., std::is_reference<Args>{}). Pass streams
  // by name directly.
  HLSLIB_DATAFLOW_INIT();
  HLSLIB_DATAFLOW_FUNCTION(Load,    x_packed.data(), sx, n_pack);
  HLSLIB_DATAFLOW_FUNCTION(Load,    y_packed.data(), sy, n_pack);
  HLSLIB_DATAFLOW_FUNCTION(Compute, sx, sy, so, cfg.a, n_pack);
  HLSLIB_DATAFLOW_FUNCTION(Store,   so, out_packed.data(), n_pack);
  HLSLIB_DATAFLOW_FINALIZE();

  // Unpack first n lanes; rest is discarded.
  Unpack(out_packed.data(), out);
}

}  // namespace anvil::hls
