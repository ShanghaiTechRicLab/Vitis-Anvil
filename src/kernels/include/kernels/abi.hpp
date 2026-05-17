#pragma once
// Kernel ABI constants shared by host code, HLS models, cosim testbenches, and
// kernel code. This header intentionally avoids Vitis HLS / hlslib headers so
// embedded host cross-compiles do not include synthesis-only dependencies.

#include "anvil/config.hpp"

namespace kernels {

// saxpy preserves the existing ANVIL_PARALLELISM project knob.
static const int kSaxpyPackWidth = anvil::config::kParallelism;

// vadd and pipeline_demo keep their existing fixed demo ABI widths.
static const int kVaddPackWidth = 16;
static const int kPipelinePackWidth = 16;

}  // namespace kernels
