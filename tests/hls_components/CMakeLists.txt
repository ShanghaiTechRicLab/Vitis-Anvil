if(NOT ANVIL_BUILD_KERNELS)
  return()
endif()

add_anvil_kernel(
  NAME component_tile_burst
  TOP component_tile_burst
  SOURCES tile_burst/kernel.cpp
  PLATFORM_KIND ${ANVIL_PLATFORM_KIND}
  TESTBENCH ${CMAKE_CURRENT_SOURCE_DIR}/tile_burst/cosim_tb.cpp
  NO_ALL
  NO_CTEST)

add_anvil_kernel(
  NAME component_pingpong_dbuf_lcs
  TOP component_pingpong_dbuf_lcs
  SOURCES pingpong_dbuf_lcs/kernel.cpp
  PLATFORM_KIND ${ANVIL_PLATFORM_KIND}
  TESTBENCH ${CMAKE_CURRENT_SOURCE_DIR}/pingpong_dbuf_lcs/cosim_tb.cpp
  NO_ALL
  NO_CTEST)
