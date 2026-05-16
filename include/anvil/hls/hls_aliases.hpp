#pragma once
// Compatibility header. New code should include pack.hpp, stream.hpp, and
// dataflow.hpp directly; this header keeps older examples compiling.

#include "anvil/hls/dataflow.hpp"
#include "anvil/hls/pack.hpp"
#include "anvil/hls/stream.hpp"

namespace anvil {
namespace hls {

template <typename T, int N>
using DataPack = Pack<T, N>;

}  // namespace hls
}  // namespace anvil
