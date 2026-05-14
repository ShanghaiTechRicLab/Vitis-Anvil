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
