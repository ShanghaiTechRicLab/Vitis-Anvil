# src/kernels/CMakeLists.txt
# Phase 2 kernel registration. The saxpy_xo target runs Vitis HLS and
# emits build/<preset>/src/kernels/saxpy_hls/saxpy.xo.

set(_saxpy_kernel_args
  NAME          saxpy
  TOP           saxpy
  SOURCES       saxpy_kernel.cpp
  PLATFORM_KIND ${ANVIL_PLATFORM_KIND})

if(ANVIL_BUILD_TESTS AND ANVIL_PLATFORM_KIND STREQUAL "u250")
  # Cosim is currently validated only for U250; other platforms still csynth
  # without wiring the U250-only testbench/part mapping.
  list(APPEND _saxpy_kernel_args
    TESTBENCH ${CMAKE_SOURCE_DIR}/tests/kernels/saxpy_cosim_tb.cpp)
endif()

add_anvil_kernel(${_saxpy_kernel_args})

if(ANVIL_PLATFORM_KIND STREQUAL "u250")
  add_anvil_xclbin(
    NAME saxpy
    KERNEL_TARGETS saxpy_xo
    PLATFORM_KIND ${ANVIL_PLATFORM_KIND})
endif()
