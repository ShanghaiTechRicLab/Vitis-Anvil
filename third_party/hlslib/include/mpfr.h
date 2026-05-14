#pragma once
// Vitis-Anvil forwarding shim, not upstream hlslib code.
//
// The vendored hlslib headers may include Xilinx/AMD Vitis header <mpfr.h>.
// Upstream hlslib does not vendor those headers, but this repo's direct header
// smoke compiles with only -Ithird_party/hlslib/include. This shim forwards to
// a later include path when present, or to the known local Vitis 2024.2
// installation used by this worktree.

#if defined(__has_include_next)
#  if __has_include_next(<mpfr.h>)
#    include_next <mpfr.h>
#  elif __has_include(</tools/Xilinx/Vitis/2024.2/include/mpfr.h>)
#    include </tools/Xilinx/Vitis/2024.2/include/mpfr.h>
#  else
#    error "Vitis-Anvil mpfr.h shim could not find Xilinx/AMD Vitis mpfr.h; install Vitis or add its include directory after third_party/hlslib/include"
#  endif
#elif defined(__has_include)
#  if __has_include(</tools/Xilinx/Vitis/2024.2/include/mpfr.h>)
#    include </tools/Xilinx/Vitis/2024.2/include/mpfr.h>
#  else
#    error "Vitis-Anvil mpfr.h shim requires a compiler with __has_include_next or local Vitis 2024.2 headers"
#  endif
#else
#  include </tools/Xilinx/Vitis/2024.2/include/mpfr.h>
#endif
