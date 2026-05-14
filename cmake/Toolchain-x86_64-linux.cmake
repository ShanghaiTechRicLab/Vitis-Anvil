# cmake/Toolchain-x86_64-linux.cmake
# Placeholder for explicit x86_64 host toolchain. Most users build natively
# without a toolchain file. This file exists so that future presets can
# pin a specific GCC/Clang or sysroot without disturbing the default.
#
# To use a non-default compiler, uncomment and set:
#   set(CMAKE_C_COMPILER   /opt/gcc-13/bin/gcc)
#   set(CMAKE_CXX_COMPILER /opt/gcc-13/bin/g++)
