#pragma once
#include <hlslib/xilinx/DataPack.h>
#include <hlslib/xilinx/Stream.h>
#include <hlslib/xilinx/Simulation.h>
#include <cstddef>

namespace anvil::hls {

template <typename T, int N>
using DataPack = hlslib::DataPack<T, N>;

template <typename T, std::size_t D = 2>
using Stream = hlslib::Stream<T, D>;

}  // namespace anvil::hls

#define ANVIL_DATAFLOW_INIT       HLSLIB_DATAFLOW_INIT
#define ANVIL_DATAFLOW_FUNCTION   HLSLIB_DATAFLOW_FUNCTION
#define ANVIL_DATAFLOW_FINALIZE   HLSLIB_DATAFLOW_FINALIZE
