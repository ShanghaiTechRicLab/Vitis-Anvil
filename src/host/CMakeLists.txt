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
