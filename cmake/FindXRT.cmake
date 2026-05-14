# cmake/FindXRT.cmake
# Locate XRT (Xilinx Runtime) headers and the xrt_coreutil library.
# Works for both datacenter (Alveo, /opt/xilinx/xrt) and embedded
# (sysroot /usr) installations. Cross-compilation honors CMAKE_SYSROOT
# via CMAKE_FIND_ROOT_PATH_MODE_* in toolchain files.

find_path(XRT_INCLUDE_DIR
  NAMES xrt/xrt_device.h
  HINTS
    $ENV{XILINX_XRT}/include
    ${XILINX_XRT}/include
    /opt/xilinx/xrt/include
  PATHS
    /usr/include
)

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
    /usr/lib/aarch64-linux-gnu
)

find_library(XRT_UUID_LIBRARY
  NAMES uuid
  PATHS
    /usr/lib
    /usr/lib64
    /usr/lib/x86_64-linux-gnu
    /usr/lib/aarch64-linux-gnu
)

find_library(XRT_RT_LIBRARY
  NAMES rt
  PATHS
    /usr/lib
    /usr/lib64
    /usr/lib/x86_64-linux-gnu
    /usr/lib/aarch64-linux-gnu
)

find_package(Threads REQUIRED)

include(FindPackageHandleStandardArgs)
if(NOT XRT_INCLUDE_DIR OR NOT XRT_COREUTIL_LIBRARY)
  set(XRT_PHASE3_FAILURE_MESSAGE "ANVIL_BUILD_XRT=ON requests the Phase 3 XRT runtime path, but XRT was not found. Install XRT (set XILINX_XRT or provide headers/library in the sysroot) before enabling this preset. Native CPU-only presets should keep ANVIL_BUILD_XRT=OFF.")
endif()
find_package_handle_standard_args(XRT
  REQUIRED_VARS XRT_INCLUDE_DIR XRT_COREUTIL_LIBRARY XRT_UUID_LIBRARY
  REASON_FAILURE_MESSAGE "${XRT_PHASE3_FAILURE_MESSAGE}"
)

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
    INTERFACE_LINK_LIBRARIES "${_xrt_interface_libs}"
  )
endif()
