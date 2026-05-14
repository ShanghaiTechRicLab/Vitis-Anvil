#pragma once
// Vitis-Anvil forwarding shim, not upstream hlslib code.
// Forwards Vitis nested header <etc/hls_half_sim.h> for direct hlslib header smoke builds.

#if defined(__has_include_next)
#  if __has_include_next(<etc/hls_half_sim.h>)
#    include_next <etc/hls_half_sim.h>
#  elif __has_include(</tools/Xilinx/Vitis/2024.2/include/etc/hls_half_sim.h>)
#    include </tools/Xilinx/Vitis/2024.2/include/etc/hls_half_sim.h>
#  else
#    error "Vitis-Anvil etc/hls_half_sim.h shim could not find the corresponding Xilinx/AMD Vitis header"
#  endif
#elif defined(__has_include)
#  if __has_include(</tools/Xilinx/Vitis/2024.2/include/etc/hls_half_sim.h>)
#    include </tools/Xilinx/Vitis/2024.2/include/etc/hls_half_sim.h>
#  else
#    error "Vitis-Anvil etc/hls_half_sim.h shim requires local Vitis 2024.2 headers"
#  endif
#else
#  include </tools/Xilinx/Vitis/2024.2/include/etc/hls_half_sim.h>
#endif
