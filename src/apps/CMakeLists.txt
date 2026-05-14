# src/apps/CMakeLists.txt

add_executable(gen_dataset gen_dataset.cpp)
target_link_libraries(gen_dataset PRIVATE
  accel_core
  argparse::argparse
  spdlog::spdlog
  nlohmann_json::nlohmann_json
)

# Subsequent CLI targets are added in later tasks. Guard with EXISTS so
# this task's build doesn't depend on later sources.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/run_gold.cpp)
  add_executable(run_gold run_gold.cpp)
  target_link_libraries(run_gold PRIVATE
    accel_gold
    argparse::argparse
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    tabulate::tabulate
  )
endif()

if(TARGET accel_hls_model AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/compare_gold_hls_model.cpp)
  add_executable(compare_gold_hls_model compare_gold_hls_model.cpp)
  target_link_libraries(compare_gold_hls_model PRIVATE
    accel_gold
    accel_hls_model
    argparse::argparse
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    tabulate::tabulate
  )
endif()
