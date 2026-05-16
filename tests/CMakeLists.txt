# tests/CMakeLists.txt

set(TEST_SOURCES
  cpp/test_saxpy_gold.cpp
  cpp/test_metrics.cpp
  cpp/test_anvil_log.cpp
  cpp/test_anvil_compare.cpp
  cpp/test_anvil_hls_helpers.cpp
)
if(TARGET anvil_hls_model)
  list(APPEND TEST_SOURCES cpp/test_hls_model_vs_gold.cpp)
endif()
if(ANVIL_BUILD_XRT AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/cpp/test_anvil_runtime_smoke.cpp)
  list(APPEND TEST_SOURCES cpp/test_anvil_runtime_smoke.cpp)
endif()

add_executable(anvil_tests ${TEST_SOURCES})
target_include_directories(anvil_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${PROJECT_SOURCE_DIR}/src/kernels/include
  ${PROJECT_SOURCE_DIR}/src/hls_model/include)
target_link_libraries(anvil_tests PRIVATE
  anvil_gold
  anvil::log
  anvil::cli
  anvil::json
  anvil::toml
  anvil::table
  anvil::progress
  anvil::test
  anvil::compare
  anvil::hls
  Catch2::Catch2WithMain
)
if(TARGET anvil_hls_model)
  target_link_libraries(anvil_tests PRIVATE anvil_hls_model)
endif()
if(TARGET anvil_runtime)
  target_link_libraries(anvil_tests PRIVATE anvil_runtime)
endif()

add_test(NAME anvil_tests COMMAND anvil_tests)

if(TARGET gen_dataset)
  add_test(
    NAME gen_tiny_dataset_smoke
    COMMAND $<TARGET_FILE:gen_dataset>
      --manifest ${CMAKE_CURRENT_SOURCE_DIR}/data/tiny_case_001.json
      --data-dir ${CMAKE_CURRENT_BINARY_DIR}/data
  )
endif()

if(TARGET run_gold)
  add_test(
    NAME run_gold_smoke
    COMMAND $<TARGET_FILE:run_gold>
      --case ${CMAKE_CURRENT_SOURCE_DIR}/data/tiny_case_001.json
      --data-dir ${CMAKE_CURRENT_BINARY_DIR}/data
      --output-dir ${CMAKE_CURRENT_BINARY_DIR}/data
  )
  set_tests_properties(run_gold_smoke PROPERTIES DEPENDS gen_tiny_dataset_smoke)
endif()

if(TARGET compare_gold_hls_model)
  add_test(
    NAME compare_gold_hls_model_smoke
    COMMAND $<TARGET_FILE:compare_gold_hls_model>
      --case ${CMAKE_CURRENT_SOURCE_DIR}/data/tiny_case_001.json
      --data-dir ${CMAKE_CURRENT_BINARY_DIR}/data
      --output-dir ${CMAKE_CURRENT_BINARY_DIR}/data
  )
  set_tests_properties(compare_gold_hls_model_smoke
    PROPERTIES DEPENDS gen_tiny_dataset_smoke)
endif()
