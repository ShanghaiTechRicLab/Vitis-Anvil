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

if(ANVIL_BUILD_TESTS)
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

# K2K streaming pipeline demo — U250 only.
# NO_ALL: these kernels are opt-in demos; they do not build by default.
if(ANVIL_PLATFORM_KIND STREQUAL "u250")
  set(_saxpy_stream_args
    NAME          saxpy_stream
    TOP           saxpy_stream
    SOURCES       saxpy_stream_kernel.cpp
    PLATFORM_KIND ${ANVIL_PLATFORM_KIND}
    NO_ALL)
  set(_vadd_stream_args
    NAME          vadd_stream
    TOP           vadd_stream
    SOURCES       vadd_stream_kernel.cpp
    PLATFORM_KIND ${ANVIL_PLATFORM_KIND}
    NO_ALL)

  if(ANVIL_BUILD_TESTS)
    list(APPEND _saxpy_stream_args
      TESTBENCH ${CMAKE_SOURCE_DIR}/tests/kernels/saxpy_stream_cosim_tb.cpp)
    list(APPEND _vadd_stream_args
      TESTBENCH ${CMAKE_SOURCE_DIR}/tests/kernels/vadd_stream_cosim_tb.cpp)
  endif()

  add_anvil_kernel(${_saxpy_stream_args})
  add_anvil_kernel(${_vadd_stream_args})

  add_anvil_xclbin(
    NAME           pipeline_demo
    KERNEL_TARGETS saxpy_stream_xo vadd_stream_xo
    LINK_CFG       ${CMAKE_SOURCE_DIR}/config/u250/pipeline_demo.cfg
    PLATFORM_KIND  ${ANVIL_PLATFORM_KIND})
endif()
