# third_party/CMakeLists.txt
# Vendored dependencies. INTERFACE targets for header-only libs, a static
# library for Catch2 amalgamated. Threads is required by hlslib's CPU
# simulation (HLSLIB_SIMULATION uses std::thread).

cmake_minimum_required(VERSION 3.21)

# Strip sanitizer flags from vendored compiled targets to avoid spurious
# noise — third-party code should never gate our sanitizer reports.
function(disable_sanitizers_for target)
  target_compile_options(${target} PRIVATE -fno-sanitize=all)
  target_link_options   (${target} PRIVATE -fno-sanitize=all)
endfunction()

# ---------------------------------------------------------------------------
# hlslib (header-only)
# Requires Threads::Threads because hlslib::Simulation spawns std::threads
# for each dataflow function. Propagate via INTERFACE so any target that
# links hlslib::hlslib automatically picks up pthread.
# ---------------------------------------------------------------------------
find_package(Threads REQUIRED)

add_library(hlslib_anvil INTERFACE)

set(_anvil_default_xilinx_include_dir "/tools/Xilinx/Vitis/2024.2/include")
if(DEFINED ENV{XILINX_VITIS})
  if(EXISTS "$ENV{XILINX_VITIS}/include/ap_int.h" AND
     EXISTS "$ENV{XILINX_VITIS}/include/ap_fixed.h")
    set(_anvil_default_xilinx_include_dir "$ENV{XILINX_VITIS}/include")
  elseif("$ENV{XILINX_VITIS}" MATCHES "[/\\]Vitis[/\\]([^/\\]+)$")
    set(_anvil_vitis_hls_sibling "$ENV{XILINX_VITIS}")
    string(REGEX REPLACE "[/\\]Vitis[/\\][^/\\]+$" "/Vitis_HLS/${CMAKE_MATCH_1}" _anvil_vitis_hls_sibling "${_anvil_vitis_hls_sibling}")
    if(EXISTS "${_anvil_vitis_hls_sibling}/include/ap_int.h" AND
       EXISTS "${_anvil_vitis_hls_sibling}/include/ap_fixed.h")
      set(_anvil_default_xilinx_include_dir "${_anvil_vitis_hls_sibling}/include")
    endif()
  endif()
endif()
if(DEFINED ENV{XILINX_VITIS} AND ANVIL_XILINX_INCLUDE_DIR)
  if(ANVIL_XILINX_INCLUDE_DIR MATCHES "[/\\]Vitis(_HLS)?[/\\]([^/\\]+)[/\\]include")
    set(_anvil_cached_xilinx_version "${CMAKE_MATCH_2}")
    if("$ENV{XILINX_VITIS}" MATCHES "[/\\]Vitis[/\\]([^/\\]+)$")
      set(_anvil_env_vitis_version "${CMAKE_MATCH_1}")
      if(NOT _anvil_cached_xilinx_version STREQUAL _anvil_env_vitis_version)
        message(STATUS "hlslib: ignoring cached Xilinx headers from ${ANVIL_XILINX_INCLUDE_DIR}; XILINX_VITIS=$ENV{XILINX_VITIS}")
        unset(ANVIL_XILINX_INCLUDE_DIR CACHE)
      endif()
    endif()
  endif()
endif()
set(ANVIL_XILINX_INCLUDE_DIR "${_anvil_default_xilinx_include_dir}" CACHE PATH
  "Directory containing Xilinx ap_int.h/ap_fixed.h for hlslib host builds")

set(_anvil_hlslib_include_dirs ${CMAKE_CURRENT_SOURCE_DIR}/hlslib/include)
if(EXISTS "${ANVIL_XILINX_INCLUDE_DIR}/ap_int.h" AND
   EXISTS "${ANVIL_XILINX_INCLUDE_DIR}/ap_fixed.h")
  list(APPEND _anvil_hlslib_include_dirs ${ANVIL_XILINX_INCLUDE_DIR})
  message(STATUS "hlslib: using Xilinx headers from ${ANVIL_XILINX_INCLUDE_DIR}")
else()
  message(WARNING
    "hlslib: Xilinx ap_int.h/ap_fixed.h not found in "
    "ANVIL_XILINX_INCLUDE_DIR=${ANVIL_XILINX_INCLUDE_DIR}; targets that include "
    "hlslib/xilinx/DataPack.h may fail to compile. Set ANVIL_XILINX_INCLUDE_DIR "
    "to a Vitis/Vitis_HLS include directory.")
endif()

target_include_directories(hlslib_anvil INTERFACE ${_anvil_hlslib_include_dirs})
target_link_libraries(hlslib_anvil INTERFACE Threads::Threads)
add_library(hlslib::hlslib ALIAS hlslib_anvil)
unset(_anvil_hlslib_include_dirs)

# ---------------------------------------------------------------------------
# Single-header / header-only INTERFACE libs
# ---------------------------------------------------------------------------
add_library(spdlog INTERFACE)
target_include_directories(spdlog INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/spdlog/include)
add_library(spdlog::spdlog ALIAS spdlog)

add_library(argparse INTERFACE)
target_include_directories(argparse INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/argparse)
add_library(argparse::argparse ALIAS argparse)

add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)

add_library(tomlplusplus INTERFACE)
target_include_directories(tomlplusplus INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/tomlplusplus)
add_library(tomlplusplus::tomlplusplus ALIAS tomlplusplus)

add_library(tabulate INTERFACE)
target_include_directories(tabulate INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/tabulate)
add_library(tabulate::tabulate ALIAS tabulate)

add_library(indicators INTERFACE)
target_include_directories(indicators INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/indicators)
add_library(indicators::indicators ALIAS indicators)

# ---------------------------------------------------------------------------
# Catch2 v3 (amalgamated, provides its own main())
# ---------------------------------------------------------------------------
add_library(catch2_amalgamated STATIC catch2/catch_amalgamated.cpp)
target_include_directories(catch2_amalgamated PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/catch2)
target_compile_options(catch2_amalgamated PRIVATE -w)   # suppress upstream warnings
disable_sanitizers_for(catch2_amalgamated)
add_library(Catch2::Catch2WithMain ALIAS catch2_amalgamated)
