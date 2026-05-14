# cmake/VitisKernel.cmake
# Phase 2 stub. The real implementation will wrap hlslib's
# add_vitis_kernel / add_vitis_program (see docs/donotcommit/vitis-anvil-design.md §11).
#
# When ACCEL_BUILD_KERNELS=ON, src/kernels/CMakeLists.txt calls
# add_accel_kernel(saxpy ...), which this stub rejects with a clear error.

function(add_accel_kernel KERNEL_NAME)
  message(FATAL_ERROR
    "add_accel_kernel(${KERNEL_NAME}): ACCEL_BUILD_KERNELS=ON requires "
    "Phase 2 implementation. "
    "See docs/donotcommit/vitis-anvil-design.md §7 and §11.")
endfunction()
