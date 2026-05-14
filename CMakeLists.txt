cmake_minimum_required(VERSION 3.21)
project(vitis_anvil VERSION 0.3.0 LANGUAGES CXX)

# host / gold / hls_model run C++20 (std::span). The Vitis HLS kernel
# synthesizer is constrained to ANVIL_HLS_STD (default c++14) and is
# driven separately via cmake/AnvilKernel.cmake (Phase 2).
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(CompilerLauncher)
include(ProjectOptions)
include(BuildOptionValidation)

configure_file(
  include/anvil/config.hpp.in
  ${CMAKE_BINARY_DIR}/generated/anvil/config.hpp
  @ONLY)

# anvil_core is an INTERFACE library that propagates the public include
# tree (hand-written headers + generated config.hpp) and the C++20
# requirement to every consumer.
add_library(anvil_core INTERFACE)
target_include_directories(anvil_core INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated>
  $<INSTALL_INTERFACE:include>)
target_compile_features(anvil_core INTERFACE cxx_std_20)
add_library(anvil::core ALIAS anvil_core)

find_package(Threads REQUIRED)

add_subdirectory(third_party)

if(ANVIL_BUILD_XRT)
  find_package(XRT REQUIRED)
  include(AnvilHost)
endif()
add_subdirectory(src/anvil)

if(ANVIL_BUILD_GOLD)
  add_subdirectory(src/gold)
endif()
if(ANVIL_BUILD_HLS_MODEL)
  add_subdirectory(src/hls_model)
endif()
if(ANVIL_BUILD_APPS)
  add_subdirectory(src/apps)
endif()
if(ANVIL_BUILD_XRT)
  add_subdirectory(src/host)
endif()

if(ANVIL_BUILD_TESTS)
  enable_testing()
endif()

if(ANVIL_BUILD_KERNELS)
  find_package(Python3 COMPONENTS Interpreter REQUIRED)
  include(AnvilKernel)
  add_subdirectory(src/kernels)
endif()

if(ANVIL_BUILD_TESTS)
  add_subdirectory(tests)
endif()


# ---- install / export (find_package(anvil) support) ----
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set_target_properties(anvil_core       PROPERTIES EXPORT_NAME core)
set_target_properties(anvil_log        PROPERTIES EXPORT_NAME log)
set_target_properties(anvil_cli        PROPERTIES EXPORT_NAME cli)
set_target_properties(anvil_json       PROPERTIES EXPORT_NAME json)
set_target_properties(anvil_toml       PROPERTIES EXPORT_NAME toml)
set_target_properties(anvil_table      PROPERTIES EXPORT_NAME table)
set_target_properties(anvil_progress   PROPERTIES EXPORT_NAME progress)
set_target_properties(anvil_test       PROPERTIES EXPORT_NAME test)
set_target_properties(anvil_hls        PROPERTIES EXPORT_NAME hls)
set_target_properties(anvil_gold_iface PROPERTIES EXPORT_NAME gold_iface)
set_target_properties(anvil_compare    PROPERTIES EXPORT_NAME compare)
if(ANVIL_BUILD_GOLD)
  set_target_properties(anvil_gold PROPERTIES EXPORT_NAME gold)
endif()
if(ANVIL_BUILD_XRT)
  set_target_properties(anvil_runtime PROPERTIES EXPORT_NAME runtime)
endif()

set(_anvil_install_targets
  anvil_core
  anvil_log anvil_cli anvil_json anvil_toml
  anvil_table anvil_progress anvil_test
  anvil_hls anvil_gold_iface anvil_compare)

if(ANVIL_BUILD_GOLD)
  list(APPEND _anvil_install_targets anvil_gold)
endif()
if(ANVIL_BUILD_XRT)
  list(APPEND _anvil_install_targets anvil_runtime)
endif()

install(TARGETS ${_anvil_install_targets}
  EXPORT anvilTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(DIRECTORY include/anvil
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES ${CMAKE_BINARY_DIR}/generated/anvil/config.hpp
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/anvil)

# Vendored header subsets needed by installed wrapper headers.
install(DIRECTORY third_party/spdlog/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY third_party/nlohmann DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY third_party/hlslib/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES
  third_party/argparse/argparse.hpp
  third_party/tomlplusplus/toml.hpp
  third_party/tabulate/tabulate.hpp
  third_party/indicators/indicators.hpp
  third_party/catch2/catch_amalgamated.hpp
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT anvilTargets
  FILE anvilTargets.cmake
  NAMESPACE anvil::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/anvil)

configure_package_config_file(
  cmake/anvilConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/anvilConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/anvil)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/anvilConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/anvilConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/anvilConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/anvil)
install(FILES cmake/FindXRT.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/anvil)
