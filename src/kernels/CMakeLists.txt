# src/kernels/CMakeLists.txt
# Phase 2 kernel registration. The saxpy_xo target runs Vitis HLS and
# emits build/<preset>/src/kernels/saxpy_hls/saxpy.xo.

add_accel_kernel(saxpy
  TOP           saxpy
  SOURCES       saxpy_kernel.cpp
  TESTBENCH    ${CMAKE_SOURCE_DIR}/tests/kernels/saxpy_cosim_tb.cpp
  PLATFORM_KIND ${ACCEL_PLATFORM_KIND})
