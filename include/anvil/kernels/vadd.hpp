#pragma once

#include <hlslib/xilinx/DataPack.h>

namespace anvil {
namespace kernels {

static const int kVaddPack = 16;
typedef hlslib::DataPack<float, kVaddPack> VaddPack;

}  // namespace kernels
}  // namespace anvil

extern "C" void vadd(const anvil::kernels::VaddPack* a,
                     const anvil::kernels::VaddPack* b,
                     anvil::kernels::VaddPack* out,
                     int n_packs);
