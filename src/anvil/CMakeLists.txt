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
