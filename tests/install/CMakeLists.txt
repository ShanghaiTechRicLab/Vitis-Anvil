cmake_minimum_required(VERSION 3.21)
project(anvil_install_smoke LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED ANVIL_INSTALL_DIR)
    message(FATAL_ERROR "Pass -DANVIL_INSTALL_DIR=<cmake --install prefix or package dir>")
endif()

find_package(anvil REQUIRED
    PATHS "${ANVIL_INSTALL_DIR}"
    NO_DEFAULT_PATH)

add_executable(install_smoke smoke.cpp)
target_link_libraries(install_smoke PRIVATE anvil::compare anvil::hls)

enable_testing()
add_test(NAME install_smoke COMMAND install_smoke)
