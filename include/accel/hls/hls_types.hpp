#pragma once
#include "accel/config.hpp"

// Phase 1 decision: HLS model uses scalar streams only (lane width = 1).
// Phase 2 will activate the vectorized lane:
//
//   #include <hlslib/xilinx/DataPack.h>
//   namespace accel::hls {
//   using ElemPack = hlslib::DataPack<float, accel::config::kParallelism>;
//   }
//
// See docs/build.md for current Phase 0+1 build/test usage.

namespace accel::hls {}
