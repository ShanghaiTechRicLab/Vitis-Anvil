cmake_minimum_required(VERSION 3.21)
project(vitis_anvil LANGUAGES CXX)

# host / gold / hls_model run C++20 (std::span). The Vitis HLS kernel
# synthesizer is constrained to ANVIL_HLS_STD (default c++14) and is
# driven separately via cmake/AnvilKernel.cmake (Phase 2).
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(CompilerLauncher)
include(ProjectOptions)
include(BuildOptionValidation)

configure_file(
  include/anvil/config.hpp.in
  ${CMAKE_BINARY_DIR}/generated/anvil/config.hpp
  @ONLY)

# anvil_core is an INTERFACE library that propagates the public include
# tree (hand-written headers + generated config.hpp) and the C++20
# requirement to every consumer.
add_library(anvil_core INTERFACE)
target_include_directories(anvil_core INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_BINARY_DIR}/generated)
target_compile_features(anvil_core INTERFACE cxx_std_20)
add_library(anvil::core ALIAS anvil_core)

add_subdirectory(third_party)

if(ANVIL_BUILD_XRT)
  find_package(XRT REQUIRED)
endif()
add_subdirectory(src/anvil)

if(ANVIL_BUILD_GOLD)
  add_subdirectory(src/gold)
endif()
if(ANVIL_BUILD_HLS_MODEL)
  add_subdirectory(src/hls_model)
endif()
if(ANVIL_BUILD_APPS)
  add_subdirectory(src/apps)
endif()

if(ANVIL_BUILD_TESTS)
  enable_testing()
endif()

if(ANVIL_BUILD_KERNELS)
  find_package(Python3 COMPONENTS Interpreter REQUIRED)
  include(AnvilKernel)
  add_subdirectory(src/kernels)
endif()

if(ANVIL_BUILD_TESTS)
  add_subdirectory(tests)
endif()
