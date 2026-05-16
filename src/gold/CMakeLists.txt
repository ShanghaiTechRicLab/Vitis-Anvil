# src/gold/CMakeLists.txt
set(GOLD_SOURCES cpp/saxpy_gold.cpp)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/cpp/metrics.cpp)
  list(APPEND GOLD_SOURCES cpp/metrics.cpp)
endif()

add_library(anvil_gold ${GOLD_SOURCES})
target_link_libraries(anvil_gold PUBLIC anvil_core)
add_library(anvil::gold ALIAS anvil_gold)

add_subdirectory(cpp)

target_include_directories(anvil_gold PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/gold/include>)
