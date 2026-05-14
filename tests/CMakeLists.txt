# tests/CMakeLists.txt
# Aggregate all test sources into a single Catch2 executable.

set(TEST_SOURCES
  gold/test_saxpy_gold.cpp
)

# Optional sources will be appended as later tasks land tests for them.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/gold/test_metrics.cpp)
  list(APPEND TEST_SOURCES gold/test_metrics.cpp)
endif()
if(TARGET accel_hls_model AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/hls_model/test_hls_model_vs_gold.cpp)
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
