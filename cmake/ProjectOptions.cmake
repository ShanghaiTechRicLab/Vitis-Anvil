# cmake/ProjectOptions.cmake
# Centralized declaration of all ANVIL_* CMake options and cache variables.

option(ANVIL_BUILD_GOLD       "Build gold reference"     ON)
option(ANVIL_BUILD_HLS_MODEL  "Build HLS CPU model"      ON)
option(ANVIL_BUILD_APPS       "Build CLI apps"           ON)
option(ANVIL_BUILD_TESTS      "Build tests"              ON)
option(ANVIL_BUILD_XRT        "Build XRT host runtime"   OFF)   # Phase 3
option(ANVIL_BUILD_KERNELS    "Build Vitis HLS kernels"  OFF)   # Phase 2

set(ANVIL_PLATFORM_KIND  "native"  CACHE STRING
    "Target platform: native|u250|alveo_u55c|zcu104|kv260")
set(ANVIL_VITIS_PLATFORM ""        CACHE STRING "Path to .xpfm (Phase 2)")
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
  find_program(VPP_EXECUTABLE v++)
  if(NOT VPP_EXECUTABLE)
    message(FATAL_ERROR
      "Phase 2 (ANVIL_BUILD_KERNELS=ON) requires v++ in PATH.\n"
      "Source the Vitis settings first:\n"
      "  source /tools/Xilinx/Vitis/2024.2/settings64.sh\n"
      "then re-configure (rm -rf build/<preset> first).")
  endif()
  find_program(VITIS_RUN_EXECUTABLE vitis-run)
  if(NOT VITIS_RUN_EXECUTABLE)
    message(FATAL_ERROR
      "Phase 2 cosim requires vitis-run in PATH (expected in the same Vitis "
      "install as v++).\n  Found v++: ${VPP_EXECUTABLE}\nSource "
      "/tools/Xilinx/Vitis/2024.2/settings64.sh to get vitis-run too.")
  endif()
  if(NOT ANVIL_VITIS_PLATFORM OR NOT EXISTS "${ANVIL_VITIS_PLATFORM}")
    message(FATAL_ERROR
      "ANVIL_VITIS_PLATFORM not set or .xpfm missing:\n"
      "  '${ANVIL_VITIS_PLATFORM}'\n"
      "Set it in the preset cacheVariables or via "
      "-DANVIL_VITIS_PLATFORM=/path/to/xpfm.")
  endif()
  message(STATUS "Phase 2 toolchain: v++ = ${VPP_EXECUTABLE}")
  message(STATUS "Phase 2 toolchain: vitis-run = ${VITIS_RUN_EXECUTABLE}")
  message(STATUS "Phase 2 platform: ${ANVIL_VITIS_PLATFORM}")
  message(STATUS "Phase 2 Vitis target: ${ANVIL_VITIS_TARGET} (used by xclbin link; not passed to v++ --mode hls)")
endif()
