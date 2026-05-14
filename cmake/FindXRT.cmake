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

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(XRT
  REQUIRED_VARS XRT_INCLUDE_DIR XRT_COREUTIL_LIBRARY
)

if(XRT_FOUND AND NOT TARGET XRT::xrt_coreutil)
  add_library(XRT::xrt_coreutil UNKNOWN IMPORTED)
  set_target_properties(XRT::xrt_coreutil PROPERTIES
    IMPORTED_LOCATION "${XRT_COREUTIL_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${XRT_INCLUDE_DIR}"
  )
endif()
