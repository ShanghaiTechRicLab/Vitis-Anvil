# cmake/Toolchain-aarch64-linux.cmake
# Cross-compile for aarch64 embedded Linux (ZCU104 / KV260 / Versal).
# Used in Phase 4. Requires SYSROOT environment variable pointing to a
# rootfs/sysroot that contains aarch64 libc, libstdc++, and (optionally) XRT.

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER   aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

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
