#pragma once
// Vitis HLS kernel C++ entry point. Compiles under both Vitis HLS (synth)
// and host C++ (cosim testbench, link). Kept C++14-clean — no <span>, no
// concepts, no `if constexpr`. The companion CPU model lives in
// include/anvil/hls/saxpy_hls_model.hpp. The kernel ABI intentionally fixes
// SaxpyPack to 16 float lanes; it is independent of generated
// anvil::config::kParallelism and include/anvil/hls/data_pack.hpp.
//
// Packed-buffer ABI: x, y, and out must each point to at least
// ceil(n_total / 16) SaxpyPack elements. If n_total is not a multiple of 16,
// the final output pack contains padding lanes that the host must ignore.

#include <hlslib/xilinx/DataPack.h>

using SaxpyPack = hlslib::DataPack<float, 16>;

extern "C" void saxpy(
    SaxpyPack* x,
    SaxpyPack* y,
    SaxpyPack* out,
    float       a,
    int         n_total);
