#pragma once
// saxpy_stream: reads x[], y[] from DDR; emits z = a*x + y via AXI4-Stream.
// C++14-compatible.
#include "anvil/kernels/pipeline_types.hpp"

#include <hls_stream.h>

extern "C" void saxpy_stream(
    const anvil::kernels::PipelinePack* x,
    const anvil::kernels::PipelinePack* y,
    hls::stream<anvil::kernels::PipelinePack>& s_out,
    float a,
    int n_packs);
