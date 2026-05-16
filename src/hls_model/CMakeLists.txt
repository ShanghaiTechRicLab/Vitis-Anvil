# src/hls_model/CMakeLists.txt
add_library(anvil_hls_model saxpy_hls_model.cpp)
target_link_libraries(anvil_hls_model PUBLIC
  anvil_core
  anvil_gold        # for SaxpyConfig
  hlslib::hlslib    # propagates Threads::Threads
)
add_library(anvil::hls_model ALIAS anvil_hls_model)


target_include_directories(anvil_hls_model PUBLIC
  ${PROJECT_SOURCE_DIR}/src/hls_model/include
  ${PROJECT_SOURCE_DIR}/src/kernels/include)
