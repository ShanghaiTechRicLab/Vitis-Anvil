# cmake/ProjectOptions.cmake
# Centralized declaration of all ACCEL_* CMake options and cache variables.

option(ACCEL_BUILD_GOLD       "Build gold reference"     ON)
option(ACCEL_BUILD_HLS_MODEL  "Build HLS CPU model"      ON)
option(ACCEL_BUILD_APPS       "Build CLI apps"           ON)
option(ACCEL_BUILD_TESTS      "Build tests"              ON)
option(ACCEL_BUILD_XRT        "Build XRT host runtime"   OFF)   # Phase 3
option(ACCEL_BUILD_KERNELS    "Build Vitis HLS kernels"  OFF)   # Phase 2

set(ACCEL_PLATFORM_KIND  "native"  CACHE STRING
    "Target platform: native|alveo_u250|alveo_u55c|zcu104|kv260")
set(ACCEL_VITIS_PLATFORM ""        CACHE STRING "Path to .xpfm (Phase 2)")
set(ACCEL_CLOCK_MHZ      "200"     CACHE STRING "Kernel clock (Phase 2)")
set(ACCEL_PARALLELISM    "8"       CACHE STRING "DataPack width / kernel parallelism (Phase 2)")
set(ACCEL_HLS_STD        "c++14"   CACHE STRING "C++ std for HLS kernel synthesis (Phase 2)")
set(ACCEL_MAX_ELEMENTS   "131072"  CACHE STRING "Max elements per saxpy frame")

# ---------------------------------------------------------------------------
# Phase 2: when ACCEL_BUILD_KERNELS=ON, verify Vitis HLS toolchain is available
# and the U250 / target platform .xpfm path is set. Both are FATAL_ERROR
# with a helpful hint because skipping the check produces obscure failures
# deep in v++ runs.
# ---------------------------------------------------------------------------
if(ACCEL_BUILD_KERNELS)
  find_program(VPP_EXECUTABLE v++)
  if(NOT VPP_EXECUTABLE)
    message(FATAL_ERROR
      "Phase 2 (ACCEL_BUILD_KERNELS=ON) requires v++ in PATH.\n"
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
  if(NOT ACCEL_VITIS_PLATFORM OR NOT EXISTS "${ACCEL_VITIS_PLATFORM}")
    message(FATAL_ERROR
      "ACCEL_VITIS_PLATFORM not set or .xpfm missing:\n"
      "  '${ACCEL_VITIS_PLATFORM}'\n"
      "Set it in the preset cacheVariables or via "
      "-DACCEL_VITIS_PLATFORM=/path/to/xpfm.")
  endif()
  message(STATUS "Phase 2 toolchain: v++ = ${VPP_EXECUTABLE}")
  message(STATUS "Phase 2 toolchain: vitis-run = ${VITIS_RUN_EXECUTABLE}")
  message(STATUS "Phase 2 platform: ${ACCEL_VITIS_PLATFORM}")
endif()
