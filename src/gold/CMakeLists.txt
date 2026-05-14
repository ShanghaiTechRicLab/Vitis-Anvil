# src/gold/CMakeLists.txt
# accel_gold collects the algorithmic reference and shared metrics.
# metrics.cpp is added in a later task; guard with EXISTS so this task's
# build doesn't break.

set(GOLD_SOURCES saxpy_gold.cpp)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/metrics.cpp)
  list(APPEND GOLD_SOURCES metrics.cpp)
endif()

add_library(accel_gold ${GOLD_SOURCES})
target_link_libraries(accel_gold PUBLIC accel_core)
add_library(accel::gold ALIAS accel_gold)
