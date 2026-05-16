# src/kernels/CMakeLists.txt
# Kernel registration. Phase 5 adds the `vadd` kernel and links it with saxpy
# into a single xclbin on accelerator cards.

set(_saxpy_kernel_args
  NAME          saxpy
  TOP           saxpy
  SOURCES       saxpy_kernel.cpp
  PLATFORM_KIND ${ANVIL_PLATFORM_KIND})

set(_vadd_kernel_args
  NAME          vadd
  TOP           vadd
  SOURCES       vadd_kernel.cpp
  PLATFORM_KIND ${ANVIL_PLATFORM_KIND})

if(ANVIL_BUILD_TESTS AND ANVIL_PLATFORM_KIND STREQUAL "u250")
  list(APPEND _saxpy_kernel_args
    TESTBENCH ${CMAKE_SOURCE_DIR}/tests/kernels/saxpy_cosim_tb.cpp)
  list(APPEND _vadd_kernel_args
    TESTBENCH ${CMAKE_SOURCE_DIR}/tests/kernels/vadd_cosim_tb.cpp)
endif()

add_anvil_kernel(${_saxpy_kernel_args})
add_anvil_kernel(${_vadd_kernel_args})

if(ANVIL_PLATFORM_KIND MATCHES "^(u250|u55c)$")
  add_anvil_xclbin(
    NAME saxpy
    KERNEL_TARGETS saxpy_xo vadd_xo
    PLATFORM_KIND ${ANVIL_PLATFORM_KIND})
elseif(ANVIL_PLATFORM_KIND MATCHES "^(zcu104|zcu102)$")
  add_anvil_xclbin(
    NAME saxpy
    KERNEL_TARGETS saxpy_xo
    PLATFORM_KIND ${ANVIL_PLATFORM_KIND})
endif()
