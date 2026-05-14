# src/kernels/CMakeLists.txt
# Phase 2 stub. add_subdirectory(src/kernels) runs only when
# ACCEL_BUILD_KERNELS=ON. The call below triggers VitisKernel.cmake's
# stub which fails with a clear "Phase 2 not implemented" message.

add_accel_kernel(saxpy
  TOP     saxpy
  SOURCES saxpy_kernel.cpp)
