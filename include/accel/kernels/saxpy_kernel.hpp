#pragma once
// Vitis HLS kernel C++ entry point. Compiles under both Vitis HLS (synth)
// and host C++ (cosim testbench, link). Kept C++14-clean — no <span>, no
// concepts, no `if constexpr`. The companion CPU model lives in
// include/accel/hls/saxpy_hls_model.hpp; both share the SaxpyPack alias from
// include/accel/hls/data_pack.hpp (host side) or hlslib::DataPack directly
// (synth side, via -DHLSLIB_SYNTHESIS).

#include <hlslib/xilinx/DataPack.h>

using SaxpyPack = hlslib::DataPack<float, 16>;

extern "C" void saxpy(
    SaxpyPack* x,
    SaxpyPack* y,
    SaxpyPack* out,
    float       a,
    int         n_total);
