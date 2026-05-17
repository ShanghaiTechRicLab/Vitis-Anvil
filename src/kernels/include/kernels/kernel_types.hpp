#pragma once
// Shared demo kernel pack types. These are kernel-facing HLS types for the
// example kernels. Host code should include kernels/abi.hpp instead, so
// embedded cross builds do not pull Vitis HLS headers into the XRT host.

#include "kernels/abi.hpp"

#include "anvil/hls/pack.hpp"

namespace kernels {

typedef anvil::hls::Pack<float, kSaxpyPackWidth> SaxpyPack;
typedef anvil::hls::Pack<float, kVaddPackWidth> VaddPack;
typedef anvil::hls::Pack<float, kPipelinePackWidth> PipelinePack;

}  // namespace kernels
