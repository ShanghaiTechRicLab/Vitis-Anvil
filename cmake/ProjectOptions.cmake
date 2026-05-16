# cmake/ProjectOptions.cmake
# Centralized declaration of all ANVIL_* CMake options and cache variables.

option(ANVIL_BUILD_GOLD       "Build gold reference"     ON)
option(ANVIL_BUILD_HLS_MODEL  "Build HLS CPU model"      ON)
option(ANVIL_BUILD_APPS       "Build CLI apps"           ON)
option(ANVIL_BUILD_TESTS      "Build tests"              ON)
option(ANVIL_BUILD_XRT        "Build XRT host runtime"   OFF)   # Phase 3
option(ANVIL_BUILD_KERNELS    "Build Vitis HLS kernels"  OFF)   # Phase 2
option(ANVIL_NEEDS_CROSS     "Cross-compile host binary for embedded target" OFF)

set(ANVIL_PLATFORM_KIND  "native"  CACHE STRING
    "Target platform: native|u250|u55c|zcu102|zcu104|kv260")
set(ANVIL_VITIS_PLATFORM ""        CACHE STRING "Path to .xpfm (Phase 2)")
set(ANVIL_VITIS_PART     ""        CACHE STRING "Xilinx device part for HLS cosim (e.g. xcu250-figd2104-2L-e)")
set(ANVIL_VITIS_TARGET   "hw"      CACHE STRING
    "Vitis compile/link target for packaged artifacts: hw|hw_emu|sw_emu")
set_property(CACHE ANVIL_VITIS_TARGET PROPERTY STRINGS hw hw_emu sw_emu)
set(ANVIL_CLOCK_MHZ      "200"     CACHE STRING "Kernel clock (Phase 2)")
set(ANVIL_PARALLELISM    "8"       CACHE STRING "DataPack width / kernel parallelism (Phase 2)")
set(ANVIL_HLS_STD        "c++14"   CACHE STRING "C++ std for HLS kernel synthesis (Phase 2)")
set(ANVIL_MAX_ELEMENTS   "131072"  CACHE STRING "Max elements per saxpy frame")

# ---------------------------------------------------------------------------
# Phase 2: when ANVIL_BUILD_KERNELS=ON, verify Vitis HLS toolchain is available
# and the U250 / target platform .xpfm path is set. ANVIL_VITIS_TARGET is
# declared and validated here for xclbin link/package targets. v++ --compile
# --mode hls in Vitis 2024.2 does not accept --target, so HLS csynth
# intentionally does not pass it; add_anvil_xclbin uses it by default.
# These checks are FATAL_ERROR with a helpful hint because skipping the check
# produces obscure failures deep in v++ runs.
# ---------------------------------------------------------------------------
if(NOT ANVIL_VITIS_TARGET MATCHES "^(hw|hw_emu|sw_emu)$")
  message(FATAL_ERROR
    "ANVIL_VITIS_TARGET must be one of hw, hw_emu, or sw_emu; "
    "got '${ANVIL_VITIS_TARGET}'.")
endif()

if(ANVIL_BUILD_KERNELS)
  find_package(Vitis REQUIRED)
  find_program(VITIS_RUN_EXECUTABLE vitis-run
    HINTS $ENV{XILINX_VITIS}/bin)
  if(NOT VITIS_RUN_EXECUTABLE)
    message(FATAL_ERROR
      "ANVIL_BUILD_KERNELS=ON requires vitis-run in PATH.\n"
      "Source the Vitis settings before configuring.")
  endif()
  if(NOT ANVIL_VITIS_PLATFORM OR NOT EXISTS "${ANVIL_VITIS_PLATFORM}")
    message(FATAL_ERROR
      "ANVIL_VITIS_PLATFORM not set or .xpfm missing: '${ANVIL_VITIS_PLATFORM}'")
  endif()
  message(STATUS "Vitis target: ${ANVIL_VITIS_TARGET}")
  message(STATUS "Vitis platform: ${ANVIL_VITIS_PLATFORM}")
endif()
