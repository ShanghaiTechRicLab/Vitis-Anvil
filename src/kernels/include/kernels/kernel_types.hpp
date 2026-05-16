#pragma once
// Shared demo kernel pack types. These are ABI-facing types for the example
// kernels. saxpy follows ANVIL_PARALLELISM; vadd and pipeline_demo stay fixed
// at 16 float lanes.

#include "anvil/config.hpp"
#include "anvil/hls/pack.hpp"

namespace kernels {

// saxpy preserves the existing ANVIL_PARALLELISM project knob.
static const int kSaxpyPackWidth = anvil::config::kParallelism;

// vadd and pipeline_demo keep their existing fixed demo ABI widths.
static const int kVaddPackWidth = 16;
static const int kPipelinePackWidth = 16;

typedef anvil::hls::Pack<float, kSaxpyPackWidth> SaxpyPack;
typedef anvil::hls::Pack<float, kVaddPackWidth> VaddPack;
typedef anvil::hls::Pack<float, kPipelinePackWidth> PipelinePack;

// Deprecated compatibility constants used by existing tests and docs.
// New code should use kVaddPackWidth and kPipelinePackWidth.
static const int kVaddPack = kVaddPackWidth;
static const int kPipelinePack = kPipelinePackWidth;

}  // namespace kernels
