include(AnvilHost)

add_anvil_host(
    NAME run_saxpy
    SOURCES run_saxpy.cpp
    LINK_LIBRARIES
        anvil_runtime
        anvil_gold
        anvil_log
        anvil_cli
        anvil_json
        anvil_compare)

add_anvil_host(
    NAME run_vadd
    SOURCES run_vadd.cpp
    LINK_LIBRARIES
        anvil_runtime
        anvil_log
        anvil_cli
        anvil_json
        anvil_compare)

add_anvil_host(
    NAME run_pipeline_demo
    SOURCES run_pipeline_demo.cpp
    LINK_LIBRARIES
        anvil_runtime
        anvil_log
        anvil_cli
        anvil_json
        anvil_compare)


target_include_directories(run_saxpy PRIVATE ${PROJECT_SOURCE_DIR}/src/kernels/include)
target_include_directories(run_vadd PRIVATE ${PROJECT_SOURCE_DIR}/src/kernels/include)
target_include_directories(run_pipeline_demo PRIVATE ${PROJECT_SOURCE_DIR}/src/kernels/include)
