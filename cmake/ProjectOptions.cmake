# cmake/ProjectOptions.cmake
# Centralized declaration of all ANVIL_* CMake options and cache variables.

option(ANVIL_BUILD_GOLD       "Build gold reference"     ON)
option(ANVIL_BUILD_HLS_MODEL  "Build HLS CPU model"      ON)
option(ANVIL_BUILD_APPS       "Build CLI apps"           ON)
option(ANVIL_BUILD_TESTS      "Build tests"              ON)
option(ANVIL_BUILD_XRT        "Build XRT host runtime"   OFF)   # Phase 3
option(ANVIL_BUILD_KERNELS    "Build Vitis HLS kernels"  OFF)   # Phase 2
option(ANVIL_NEEDS_CROSS     "Cross-compile host binary for embedded target" OFF)

set(ANVIL_PLATFORM_KIND  "native"  CACHE STRING
    "Target platform: native|u50|u55c|u200|u250|u280|zcu102|zcu104|zcu106|kv260|vck5000")
set(ANVIL_VITIS_PLATFORM ""        CACHE STRING "Path to .xpfm (Phase 2)")
set(ANVIL_VITIS_PART     ""        CACHE STRING "Xilinx device part for HLS cosim (e.g. xcu250-figd2104-2L-e)")
set(ANVIL_VITIS_TARGET   "hw"      CACHE STRING
    "Vitis compile/link target for packaged artifacts: hw|hw_emu|sw_emu")
set_property(CACHE ANVIL_VITIS_TARGET PROPERTY STRINGS hw hw_emu sw_emu)
set(ANVIL_CLOCK_MHZ      "200"     CACHE STRING "Kernel clock (Phase 2)")
set(ANVIL_PARALLELISM    "8"       CACHE STRING "DataPack width / kernel parallelism (Phase 2)")
set(ANVIL_HLS_STD        "c++14"   CACHE STRING "C++ std for HLS kernel synthesis (Phase 2)")
set(ANVIL_MAX_ELEMENTS   "131072"  CACHE STRING "Max elements per saxpy frame")

function(_anvil_find_vitis_platform out_var platform_kind requested_path)
  if(requested_path AND EXISTS "${requested_path}")
    set(${out_var} "${requested_path}" PARENT_SCOPE)
    return()
  endif()

  set(_anvil_platform_roots)
  if(DEFINED ENV{XILINX_VITIS})
    list(APPEND _anvil_platform_roots "$ENV{XILINX_VITIS}/base_platforms")
  endif()
  list(APPEND _anvil_platform_roots "/opt/xilinx/platforms")
  file(GLOB _anvil_vitis_base_platform_roots
    LIST_DIRECTORIES true
    "/tools/Xilinx/Vitis/*/base_platforms")
  list(APPEND _anvil_platform_roots ${_anvil_vitis_base_platform_roots})
  list(REMOVE_DUPLICATES _anvil_platform_roots)

  set(_anvil_patterns)
  if(requested_path)
    get_filename_component(_anvil_requested_name "${requested_path}" NAME_WE)
    if(_anvil_requested_name)
      list(APPEND _anvil_patterns "*${_anvil_requested_name}*.xpfm")
    endif()
  endif()
  if(platform_kind)
    string(TOUPPER "${platform_kind}" _anvil_platform_kind_upper)
    list(APPEND _anvil_patterns
      "*${platform_kind}*.xpfm"
      "*${_anvil_platform_kind_upper}*.xpfm")
    if(platform_kind STREQUAL "kv260")
      list(APPEND _anvil_patterns "*k26*.xpfm" "*K26*.xpfm")
    endif()
  endif()

  foreach(_anvil_root IN LISTS _anvil_platform_roots)
    if(NOT IS_DIRECTORY "${_anvil_root}")
      continue()
    endif()
    foreach(_anvil_pattern IN LISTS _anvil_patterns)
      file(GLOB_RECURSE _anvil_matches
        LIST_DIRECTORIES false
        "${_anvil_root}/${_anvil_pattern}")
      if(_anvil_matches)
        list(SORT _anvil_matches)
        list(GET _anvil_matches 0 _anvil_match)
        set(${out_var} "${_anvil_match}" PARENT_SCOPE)
        return()
      endif()
    endforeach()
  endforeach()

  set(${out_var} "${requested_path}" PARENT_SCOPE)
endfunction()

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
  find_package(Vitis REQUIRED)
  if(DEFINED ENV{XILINX_VITIS} AND VITIS_RUN_EXECUTABLE)
    get_filename_component(_anvil_cached_vitis_run_bin "${VITIS_RUN_EXECUTABLE}" DIRECTORY)
    get_filename_component(_anvil_cached_vitis_run_root "${_anvil_cached_vitis_run_bin}" DIRECTORY)
    if(NOT _anvil_cached_vitis_run_root STREQUAL "$ENV{XILINX_VITIS}")
      message(STATUS "ProjectOptions: ignoring cached vitis-run from ${_anvil_cached_vitis_run_root}; XILINX_VITIS=$ENV{XILINX_VITIS}")
      unset(VITIS_RUN_EXECUTABLE CACHE)
    endif()
  endif()
  if(NOT VITIS_VERSION MATCHES "^2022[.]")
    find_program(VITIS_RUN_EXECUTABLE vitis-run
      HINTS $ENV{XILINX_VITIS}/bin)
    if(NOT VITIS_RUN_EXECUTABLE)
      message(FATAL_ERROR
        "ANVIL_BUILD_KERNELS=ON requires vitis-run in PATH for Vitis ${VITIS_VERSION}.\n"
        "Source the Vitis settings before configuring.")
    endif()
  endif()
  if(NOT ANVIL_VITIS_PLATFORM OR NOT EXISTS "${ANVIL_VITIS_PLATFORM}")
    _anvil_find_vitis_platform(_anvil_discovered_platform
      "${ANVIL_PLATFORM_KIND}" "${ANVIL_VITIS_PLATFORM}")
    if(_anvil_discovered_platform AND EXISTS "${_anvil_discovered_platform}")
      message(STATUS
        "ProjectOptions: discovered Vitis platform for ${ANVIL_PLATFORM_KIND}: "
        "${_anvil_discovered_platform}")
      set(ANVIL_VITIS_PLATFORM "${_anvil_discovered_platform}" CACHE STRING
        "Path to .xpfm (Phase 2)" FORCE)
    endif()
  endif()
  if(NOT ANVIL_VITIS_PLATFORM OR NOT EXISTS "${ANVIL_VITIS_PLATFORM}")
    message(FATAL_ERROR
      "ANVIL_VITIS_PLATFORM not set or .xpfm missing: '${ANVIL_VITIS_PLATFORM}'\n"
      "Searched $XILINX_VITIS/base_platforms, /tools/Xilinx/Vitis/*/base_platforms, and /opt/xilinx/platforms.\n"
      "Find installed platforms with: find /opt/xilinx/platforms /tools/Xilinx -name '*.xpfm' 2>/dev/null\n"
      "Then override from make with: make <target> TARGET=<board> ANVIL_PLATFORM=/path/to/platform.xpfm")
  endif()
  message(STATUS "Vitis target: ${ANVIL_VITIS_TARGET}")
  message(STATUS "Vitis platform: ${ANVIL_VITIS_PLATFORM}")
endif()
