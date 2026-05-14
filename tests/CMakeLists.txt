# tests/CMakeLists.txt
# anvil_tests aggregates all Catch2 unit tests into a single executable
# (single ctest entry). Additional ctest entries invoke the CLI apps for
# end-to-end smoke validation.

set(TEST_SOURCES
  gold/test_saxpy_gold.cpp
  gold/test_metrics.cpp
)
if(TARGET accel_hls_model)
  list(APPEND TEST_SOURCES hls_model/test_hls_model_vs_gold.cpp)
endif()

add_executable(anvil_tests ${TEST_SOURCES})
target_include_directories(anvil_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(anvil_tests PRIVATE
  accel_gold
  Catch2::Catch2WithMain
  spdlog::spdlog
  nlohmann_json::nlohmann_json
)
if(TARGET accel_hls_model)
  target_link_libraries(anvil_tests PRIVATE accel_hls_model)
endif()

add_test(NAME anvil_tests COMMAND anvil_tests)

# ---- End-to-end CLI smoke tests ----
# These do NOT block anvil_tests (which uses in-process random data) —
# they exercise the gen_dataset → .bin → run_gold / compare pipeline.

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
# Phase 2: tests/kernels/saxpy_cosim_tb.cpp is NOT compiled into anvil_tests.
# It is consumed by vitis-run --cosim via VitisKernel.cmake (TESTBENCH param),
# which builds the testbench inside the HLS sim flow. Do not list it here.
