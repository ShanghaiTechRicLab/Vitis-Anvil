# src/runtime/CMakeLists.txt
# Phase 3 stub. add_subdirectory(src/runtime) runs only when
# ACCEL_BUILD_XRT=ON. We refuse to build until the runtime layer is
# implemented in Phase 3.

message(FATAL_ERROR
  "src/runtime: ACCEL_BUILD_XRT=ON but Phase 3 (XRT runtime wrapper) "
  "is not yet implemented. "
  "See docs/donotcommit/vitis-anvil-design.md §8.")
