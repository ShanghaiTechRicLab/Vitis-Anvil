#include "accel/hls/saxpy_hls_model.hpp"

#include <hlslib/xilinx/Simulation.h>
#include <hlslib/xilinx/Stream.h>

#include <cstddef>
#include <stdexcept>

namespace accel::hls {

namespace {

constexpr std::size_t kStreamDepth = 32;

void Load(const float* in, hlslib::Stream<float, kStreamDepth>& s, int n) {
  for (int i = 0; i < n; ++i) s.Push(in[i]);
}

void Compute(hlslib::Stream<float, kStreamDepth>& xs,
             hlslib::Stream<float, kStreamDepth>& ys,
             hlslib::Stream<float, kStreamDepth>& os,
             float a, int n) {
  for (int i = 0; i < n; ++i) {
    const float xi = xs.Pop();
    const float yi = ys.Pop();
    os.Push(a * xi + yi);
  }
}

void Store(hlslib::Stream<float, kStreamDepth>& s, float* out, int n) {
  for (int i = 0; i < n; ++i) out[i] = s.Pop();
}

}  // namespace

void saxpy_hls_model(std::span<const float> x,
                     std::span<const float> y,
                     std::span<float>       out,
                     const accel::gold::SaxpyConfig& cfg) {
  if (x.size() != y.size() || x.size() != out.size()) {
    throw std::invalid_argument("saxpy_hls_model: span sizes must match");
  }
  const int n = static_cast<int>(x.size());
  if (n == 0) return;   // hlslib dataflow with n=0 is undefined; bail.

  hlslib::Stream<float, kStreamDepth> sx("sx"), sy("sy"), so("so");

  // NOTE: do NOT wrap stream args with std::ref. hlslib's AddFunction
  // (Simulation.h v1.4.6) applies std::ref internally for reference
  // parameters. Passing std::reference_wrapper<Stream> here would fail
  // to bind to the function signatures below.
  HLSLIB_DATAFLOW_INIT();
  HLSLIB_DATAFLOW_FUNCTION(Load,    x.data(), sx, n);
  HLSLIB_DATAFLOW_FUNCTION(Load,    y.data(), sy, n);
  HLSLIB_DATAFLOW_FUNCTION(Compute, sx, sy, so, cfg.a, n);
  HLSLIB_DATAFLOW_FUNCTION(Store,   so, out.data(), n);
  HLSLIB_DATAFLOW_FINALIZE();
}

}  // namespace accel::hls
