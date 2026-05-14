# src/anvil/runtime/CMakeLists.txt
add_library(anvil_runtime STATIC
    xrt_context.cpp
    kernel_handle.cpp)
target_link_libraries(anvil_runtime PUBLIC
    anvil_core
    $<BUILD_INTERFACE:XRT::xrt_coreutil>
    $<INSTALL_INTERFACE:XRT::xrt_coreutil>)
add_library(anvil::runtime ALIAS anvil_runtime)
