# cmake/FindVitis.cmake
# Locate Vitis v++ and extract VITIS_VERSION.

find_program(VPP_EXECUTABLE v++
  HINTS $ENV{XILINX_VITIS}/bin
  PATHS
    /tools/Xilinx/Vitis/2024.2/bin
    /tools/Xilinx/Vitis/2023.2/bin)

if(VPP_EXECUTABLE)
  execute_process(
    COMMAND "${VPP_EXECUTABLE}" --version
    OUTPUT_VARIABLE _vitis_version_output
    ERROR_VARIABLE _vitis_version_error
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE)
  if(NOT _vitis_version_output)
    set(_vitis_version_output "${_vitis_version_error}")
  endif()
  if(_vitis_version_output MATCHES "v\\+\\+ v([0-9]+\\.[0-9]+)")
    set(VITIS_VERSION "${CMAKE_MATCH_1}")
  elseif(_vitis_version_output MATCHES "([0-9]{4}\\.[0-9]+)")
    set(VITIS_VERSION "${CMAKE_MATCH_1}")
  else()
    set(VITIS_VERSION "unknown")
  endif()
  if(NOT VITIS_VERSION MATCHES "^(2023|2024)\\.")
    message(WARNING
      "FindVitis: detected Vitis ${VITIS_VERSION}; only 2023.x/2024.x are tested. Proceed with caution.")
  endif()
  message(STATUS "Vitis: v++ = ${VPP_EXECUTABLE}, version = ${VITIS_VERSION}")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Vitis
  REQUIRED_VARS VPP_EXECUTABLE
  VERSION_VAR VITIS_VERSION)
