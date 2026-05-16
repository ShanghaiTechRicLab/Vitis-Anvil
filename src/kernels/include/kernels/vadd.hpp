#pragma once

#include "kernels/kernel_types.hpp"

extern "C" void vadd(const kernels::VaddPack* a,
                     const kernels::VaddPack* b,
                     kernels::VaddPack* out,
                     int n_packs);
