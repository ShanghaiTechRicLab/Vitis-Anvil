#pragma once
// Vector-pack typedef for the saxpy HLS pipeline. The lane count comes from
// the ACCEL_PARALLELISM build option (default 16); see cmake/ProjectOptions.cmake
// and include/accel/config.hpp.
//
// Both saxpy_hls_model (CPU) and saxpy_kernel (Vitis HLS) use this alias so
// the pack width stays a single source of truth.

#include <hlslib/xilinx/DataPack.h>

#include "accel/config.hpp"

namespace accel::hls {

using SaxpyPack = ::hlslib::DataPack<float, accel::config::kParallelism>;

}  // namespace accel::hls
