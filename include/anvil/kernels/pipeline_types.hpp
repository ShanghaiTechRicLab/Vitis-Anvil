#pragma once
// Shared data type for the saxpy_stream -> vadd_stream k2k pipeline.
// C++14-compatible. Both kernels must use the same pack width.
#include <hlslib/xilinx/DataPack.h>

namespace anvil {
namespace kernels {

static const int kPipelinePack = 16;
typedef hlslib::DataPack<float, kPipelinePack> PipelinePack;

}  // namespace kernels
}  // namespace anvil
