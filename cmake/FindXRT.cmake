# cmake/FindXRT.cmake
# Locate XRT for Alveo datacenter and embedded sysroot targets. Cross builds
# honor CMAKE_SYSROOT/CMAKE_FIND_ROOT_PATH through CMake's normal find rules.

find_path(XRT_INCLUDE_DIR
  NAMES xrt/xrt_device.h
  HINTS
    $ENV{XILINX_XRT}/include
    ${XILINX_XRT}/include
    /opt/xilinx/xrt/include
  PATHS
    /usr/include)

find_library(XRT_COREUTIL_LIBRARY
  NAMES xrt_coreutil
  HINTS
    $ENV{XILINX_XRT}/lib
    $ENV{XILINX_XRT}/lib64
    ${XILINX_XRT}/lib
    ${XILINX_XRT}/lib64
    /opt/xilinx/xrt/lib
    /opt/xilinx/xrt/lib64
  PATHS
    /usr/lib
    /usr/lib64
    /usr/lib/x86_64-linux-gnu
    /usr/lib/aarch64-linux-gnu)

find_library(XRT_UUID_LIBRARY
  NAMES uuid
  PATHS
    /usr/lib
    /usr/lib64
    /usr/lib/x86_64-linux-gnu
    /usr/lib/aarch64-linux-gnu)

find_library(XRT_RT_LIBRARY
  NAMES rt
  PATHS
    /usr/lib
    /usr/lib64
    /usr/lib/x86_64-linux-gnu
    /usr/lib/aarch64-linux-gnu)

find_package(Threads REQUIRED)

set(_xrt_fail_msg "ANVIL_BUILD_XRT=ON but XRT was not found. Alveo: install XRT and source /opt/xilinx/xrt/setup.sh, or set XILINX_XRT. Embedded: set SYSROOT/CMAKE_SYSROOT to a PetaLinux sysroot containing XRT. CPU-only native presets should keep ANVIL_BUILD_XRT=OFF.")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(XRT
  REQUIRED_VARS XRT_INCLUDE_DIR XRT_COREUTIL_LIBRARY XRT_UUID_LIBRARY
  REASON_FAILURE_MESSAGE "${_xrt_fail_msg}")

if(XRT_FOUND AND NOT TARGET XRT::xrt_coreutil)
  add_library(XRT::xrt_coreutil UNKNOWN IMPORTED)
  set(_xrt_interface_libs "${XRT_UUID_LIBRARY};Threads::Threads")
  if(CMAKE_DL_LIBS)
    list(APPEND _xrt_interface_libs "${CMAKE_DL_LIBS}")
  endif()
  if(XRT_RT_LIBRARY)
    list(APPEND _xrt_interface_libs "${XRT_RT_LIBRARY}")
  endif()

  set_target_properties(XRT::xrt_coreutil PROPERTIES
    IMPORTED_LOCATION "${XRT_COREUTIL_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${XRT_INCLUDE_DIR}"
    INTERFACE_LINK_LIBRARIES "${_xrt_interface_libs}")
endif()
