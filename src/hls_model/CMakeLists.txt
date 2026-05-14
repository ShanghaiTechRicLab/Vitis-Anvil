# src/hls_model/CMakeLists.txt
add_library(accel_hls_model saxpy_hls_model.cpp)
target_link_libraries(accel_hls_model PUBLIC
  accel_core
  accel_gold        # for SaxpyConfig
  hlslib::hlslib    # propagates Threads::Threads
)
add_library(accel::hls_model ALIAS accel_hls_model)
