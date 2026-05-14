#pragma once
// Vector-pack typedef for the saxpy HLS pipeline. The lane count comes from
// the ANVIL_PARALLELISM build option; see cmake/ProjectOptions.cmake and the
// generated anvil/config.hpp.
//
// Keep code that exchanges packed saxpy floats on this alias so the pack width
// stays a single source of truth as Phase 2 integration expands.

#include <hlslib/xilinx/DataPack.h>

#include "anvil/config.hpp"

namespace anvil::hls {

using SaxpyPack = ::hlslib::DataPack<float, anvil::config::kParallelism>;

}  // namespace anvil::hls
