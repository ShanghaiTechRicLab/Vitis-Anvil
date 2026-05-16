# src/gold/cpp/CMakeLists.txt
add_executable(saxpy_gold_bin saxpy_gold_main.cpp)
target_link_libraries(saxpy_gold_bin PRIVATE
    anvil_gold
    anvil_gold_iface
    anvil_log
    anvil_cli
    anvil_json)

target_include_directories(saxpy_gold_bin PRIVATE ${PROJECT_SOURCE_DIR}/src/gold/include)
