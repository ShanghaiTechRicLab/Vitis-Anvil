# add_anvil_host(NAME <name> SOURCES <...> LINK_LIBRARIES <...>)
# Builds an XRT host binary. Cross-compilation is selected by the active toolchain/preset.

function(add_anvil_host)
    set(one_value_args NAME)
    set(multi_value_args SOURCES LINK_LIBRARIES)
    cmake_parse_arguments(AH "" "${one_value_args}" "${multi_value_args}" ${ARGN})

    if(NOT AH_NAME)
        message(FATAL_ERROR "add_anvil_host: NAME required")
    endif()
    if(NOT AH_SOURCES)
        message(FATAL_ERROR "add_anvil_host(${AH_NAME}): SOURCES required")
    endif()

    add_executable(${AH_NAME} ${AH_SOURCES})
    if(AH_LINK_LIBRARIES)
        target_link_libraries(${AH_NAME} PRIVATE ${AH_LINK_LIBRARIES})
    endif()
endfunction()
