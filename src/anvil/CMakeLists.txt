# src/anvil/CMakeLists.txt
# Vendored deps are build-tree only; install/export strips them via BUILD_INTERFACE.

add_library(anvil_log STATIC log/anvil_log.cpp)
target_link_libraries(anvil_log PUBLIC
    anvil_core
    $<BUILD_INTERFACE:spdlog::spdlog>
    $<INSTALL_INTERFACE:spdlog::spdlog>)
add_library(anvil::log ALIAS anvil_log)

add_library(anvil_cli INTERFACE)
target_link_libraries(anvil_cli INTERFACE
    anvil_core
    $<BUILD_INTERFACE:argparse::argparse>
    $<INSTALL_INTERFACE:argparse::argparse>)
add_library(anvil::cli ALIAS anvil_cli)

add_library(anvil_json INTERFACE)
target_link_libraries(anvil_json INTERFACE
    anvil_core
    $<BUILD_INTERFACE:nlohmann_json::nlohmann_json>
    $<INSTALL_INTERFACE:nlohmann_json::nlohmann_json>)
add_library(anvil::json ALIAS anvil_json)

add_library(anvil_toml INTERFACE)
target_link_libraries(anvil_toml INTERFACE
    anvil_core
    $<BUILD_INTERFACE:tomlplusplus::tomlplusplus>
    $<INSTALL_INTERFACE:tomlplusplus::tomlplusplus>)
add_library(anvil::toml ALIAS anvil_toml)

add_library(anvil_table INTERFACE)
target_link_libraries(anvil_table INTERFACE
    anvil_core
    $<BUILD_INTERFACE:tabulate::tabulate>
    $<INSTALL_INTERFACE:tabulate::tabulate>)
add_library(anvil::table ALIAS anvil_table)

add_library(anvil_progress INTERFACE)
target_link_libraries(anvil_progress INTERFACE
    anvil_core
    $<BUILD_INTERFACE:indicators::indicators>
    $<INSTALL_INTERFACE:indicators::indicators>)
add_library(anvil::progress ALIAS anvil_progress)

add_library(anvil_test INTERFACE)
target_link_libraries(anvil_test INTERFACE
    anvil_core
    $<BUILD_INTERFACE:Catch2::Catch2WithMain>
    $<INSTALL_INTERFACE:Catch2::Catch2WithMain>)
add_library(anvil::test ALIAS anvil_test)

add_library(anvil_hls INTERFACE)
target_link_libraries(anvil_hls INTERFACE
    anvil_core
    $<BUILD_INTERFACE:hlslib::hlslib>
    $<INSTALL_INTERFACE:hlslib::hlslib>)
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
