# cmake/Toolchain-aarch64-linux.cmake
# Cross-compile for aarch64 embedded Linux (ZCU102 / ZCU104 / KV260 / Versal).
# Used in Phase 4. Requires SYSROOT environment variable pointing to a
# rootfs/sysroot that contains aarch64 libc, libstdc++, and (optionally) XRT.

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER   aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

# Vitis 2024.2 installs the aarch64 GCC runtime objects under
#   <toolchain>/aarch64-xilinx-linux/usr/lib/aarch64-xilinx-linux/<ver>/
# while the relocated compiler driver may search
#   <toolchain>/x86_64-petalinux-linux/usr/lib/aarch64-xilinx-linux/gcc/...
# on some hosts. Add the real runtime directory via -B so CMake's compiler
# sanity check can find crtbeginS.o/libgcc without patching the Xilinx install.
find_program(_ANVIL_AARCH64_GXX NAMES ${CMAKE_CXX_COMPILER})
if(_ANVIL_AARCH64_GXX)
  get_filename_component(_ANVIL_AARCH64_BIN_DIR "${_ANVIL_AARCH64_GXX}" DIRECTORY)
  get_filename_component(_ANVIL_AARCH64_ROOT "${_ANVIL_AARCH64_BIN_DIR}" DIRECTORY)
  file(GLOB _ANVIL_AARCH64_RUNTIME_DIRS
       LIST_DIRECTORIES true
       "${_ANVIL_AARCH64_ROOT}/aarch64-xilinx-linux/usr/lib/aarch64-xilinx-linux/*")
  foreach(_ANVIL_AARCH64_RUNTIME_DIR IN LISTS _ANVIL_AARCH64_RUNTIME_DIRS)
    if(EXISTS "${_ANVIL_AARCH64_RUNTIME_DIR}/crtbeginS.o")
      set(_ANVIL_AARCH64_B_FLAG "-B${_ANVIL_AARCH64_RUNTIME_DIR}")
      string(APPEND CMAKE_C_FLAGS_INIT " ${_ANVIL_AARCH64_B_FLAG}")
      string(APPEND CMAKE_CXX_FLAGS_INIT " ${_ANVIL_AARCH64_B_FLAG}")
      break()
    endif()
  endforeach()
endif()

if(DEFINED ENV{SYSROOT})
  set(CMAKE_SYSROOT "$ENV{SYSROOT}")
else()
  message(FATAL_ERROR "Toolchain-aarch64-linux.cmake: SYSROOT environment variable not set. "
                      "Export SYSROOT=/path/to/aarch64/sysroot before configuring.")
endif()

set(CMAKE_FIND_ROOT_PATH "${CMAKE_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
