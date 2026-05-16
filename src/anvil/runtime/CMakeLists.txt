# src/anvil/runtime/CMakeLists.txt
add_library(anvil_runtime STATIC
    xrt_context.cpp
    kernel_handle.cpp)
target_link_libraries(anvil_runtime PUBLIC
    anvil_core
    $<BUILD_INTERFACE:XRT::xrt_coreutil>
    $<INSTALL_INTERFACE:XRT::xrt_coreutil>)

target_compile_definitions(anvil_runtime PUBLIC
    ANVIL_XRT_HAS_EXPERIMENTAL_XCLBIN=$<BOOL:${XRT_HAS_EXPERIMENTAL_XCLBIN}>)
add_library(anvil::runtime ALIAS anvil_runtime)
