#pragma once
// vadd_stream: receives z via AXI4-Stream; reads b[] from DDR; writes out = z+b.
// C++14-compatible.
#include "anvil/kernels/pipeline_types.hpp"

#include <hls_stream.h>

extern "C" void vadd_stream(
    hls::stream<anvil::kernels::PipelinePack>& s_in,
    const anvil::kernels::PipelinePack* b,
    anvil::kernels::PipelinePack* out,
    int n_packs);
