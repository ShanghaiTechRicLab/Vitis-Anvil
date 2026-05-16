#pragma once
// saxpy_stream: reads x[], y[] from DDR; emits z = a*x + y via AXI4-Stream.
// C++14-compatible.
#include "kernels/pipeline_types.hpp"

#include <hls_stream.h>

extern "C" void saxpy_stream(
    const kernels::PipelinePack* x,
    const kernels::PipelinePack* y,
    hls::stream<kernels::PipelinePack>& s_out,
    float a,
    int n_packs);
