# src/gold/CMakeLists.txt
# anvil_gold collects the algorithmic reference and shared metrics.
# metrics.cpp is added in a later task; guard with EXISTS so this task's
# build doesn't break.

set(GOLD_SOURCES saxpy_gold.cpp)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/metrics.cpp)
  list(APPEND GOLD_SOURCES metrics.cpp)
endif()

add_library(anvil_gold ${GOLD_SOURCES})
target_link_libraries(anvil_gold PUBLIC anvil_core)
add_library(anvil::gold ALIAS anvil_gold)
