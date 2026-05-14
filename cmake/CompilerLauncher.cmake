# cmake/CompilerLauncher.cmake
# Auto-detect sccache (preferred) or ccache as compiler launcher.

find_program(SCCACHE_PROGRAM sccache)
find_program(CCACHE_PROGRAM  ccache)
if(SCCACHE_PROGRAM)
  set(CMAKE_C_COMPILER_LAUNCHER   "${SCCACHE_PROGRAM}")
  set(CMAKE_CXX_COMPILER_LAUNCHER "${SCCACHE_PROGRAM}")
  message(STATUS "Using sccache: ${SCCACHE_PROGRAM}")
elseif(CCACHE_PROGRAM)
  set(CMAKE_C_COMPILER_LAUNCHER   "${CCACHE_PROGRAM}")
  set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
  message(STATUS "Using ccache: ${CCACHE_PROGRAM}")
endif()
