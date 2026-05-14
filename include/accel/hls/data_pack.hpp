#pragma once
// Vector-pack typedef for the saxpy HLS pipeline. The lane count comes from
// the ACCEL_PARALLELISM build option; see cmake/ProjectOptions.cmake and the
// generated accel/config.hpp.
//
// Keep code that exchanges packed saxpy floats on this alias so the pack width
// stays a single source of truth as Phase 2 integration expands.

#include <hlslib/xilinx/DataPack.h>

#include "accel/config.hpp"

namespace accel::hls {

using SaxpyPack = ::hlslib::DataPack<float, accel::config::kParallelism>;

}  // namespace accel::hls
