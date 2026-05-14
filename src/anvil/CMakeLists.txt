# src/anvil/CMakeLists.txt
# Public wrappers expose small anvil::* APIs while vendored headers are used only
# as include directories. This keeps install/export independent from build-tree
# third_party CMake targets.

set(_anvil_vendor_install_include "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

add_library(anvil_log STATIC log/anvil_log.cpp)
target_link_libraries(anvil_log PUBLIC anvil_core)
target_include_directories(anvil_log PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/third_party/spdlog/include>
    ${_anvil_vendor_install_include})
add_library(anvil::log ALIAS anvil_log)

add_library(anvil_cli INTERFACE)
target_link_libraries(anvil_cli INTERFACE anvil_core)
target_include_directories(anvil_cli INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/third_party/argparse>
    ${_anvil_vendor_install_include})
add_library(anvil::cli ALIAS anvil_cli)

add_library(anvil_json INTERFACE)
target_link_libraries(anvil_json INTERFACE anvil_core)
target_include_directories(anvil_json INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/third_party>
    ${_anvil_vendor_install_include})
add_library(anvil::json ALIAS anvil_json)

add_library(anvil_toml INTERFACE)
target_link_libraries(anvil_toml INTERFACE anvil_core)
target_include_directories(anvil_toml INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/third_party/tomlplusplus>
    ${_anvil_vendor_install_include})
add_library(anvil::toml ALIAS anvil_toml)

add_library(anvil_table INTERFACE)
target_link_libraries(anvil_table INTERFACE anvil_core)
target_include_directories(anvil_table INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/third_party/tabulate>
    ${_anvil_vendor_install_include})
add_library(anvil::table ALIAS anvil_table)

add_library(anvil_progress INTERFACE)
target_link_libraries(anvil_progress INTERFACE anvil_core)
target_include_directories(anvil_progress INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/third_party/indicators>
    ${_anvil_vendor_install_include})
add_library(anvil::progress ALIAS anvil_progress)

add_library(anvil_test INTERFACE)
target_link_libraries(anvil_test INTERFACE anvil_core)
target_include_directories(anvil_test INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/third_party/catch2>
    ${_anvil_vendor_install_include})
add_library(anvil::test ALIAS anvil_test)

add_library(anvil_hls INTERFACE)
target_link_libraries(anvil_hls INTERFACE anvil_core Threads::Threads)
target_include_directories(anvil_hls INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/third_party/hlslib/include>
    $<BUILD_INTERFACE:${ANVIL_XILINX_INCLUDE_DIR}>
    ${_anvil_vendor_install_include})
add_library(anvil::hls ALIAS anvil_hls)

add_library(anvil_gold_iface INTERFACE)
target_link_libraries(anvil_gold_iface INTERFACE anvil_core)
add_library(anvil::gold_iface ALIAS anvil_gold_iface)

add_library(anvil_compare INTERFACE)
target_link_libraries(anvil_compare INTERFACE anvil_core)
add_library(anvil::compare ALIAS anvil_compare)

if(ANVIL_BUILD_XRT)
    add_subdirectory(runtime)
endif()
