#pragma once
// Vitis HLS kernel C++ entry point. Compiles under both Vitis HLS and host
// C++ cosim testbenches. SaxpyPack width follows ANVIL_PARALLELISM through
// kernels::kSaxpyPackWidth.

#include "kernels/kernel_types.hpp"

using SaxpyPack = kernels::SaxpyPack;

extern "C" void saxpy(
    SaxpyPack* x,
    SaxpyPack* y,
    SaxpyPack* out,
    float       a,
    int         n_total);
