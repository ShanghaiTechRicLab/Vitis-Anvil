# cmake/AnvilKernel.cmake
# Phase 2 Vitis HLS kernel helpers.

function(_anvil_kernel_reject_space_path kernel_name path_label path_value)
  if("${path_value}" MATCHES "[ \t]")
    message(FATAL_ERROR
      "add_anvil_kernel(${kernel_name}): ${path_label} contains whitespace, "
      "which is not supported by the generated Vitis HLS config: '${path_value}'")
  endif()
endfunction()

function(_anvil_write_file_if_changed output_path content)
  if(EXISTS "${output_path}")
    file(READ "${output_path}" _anvil_existing_content)
  else()
    set(_anvil_existing_content "")
  endif()
  if(NOT _anvil_existing_content STREQUAL content)
    file(WRITE "${output_path}" "${content}")
  endif()
endfunction()

function(_anvil_vitis_hls_include_dirs out_var)
  set(_anvil_vitis_roots)
  if(DEFINED ENV{XILINX_VITIS})
    # Prefer the sourced Vitis tree.  Do not mix headers from a stale cached
    # VPP_EXECUTABLE in another Vitis version; HLS headers are not ABI-stable
    # across releases.
    list(APPEND _anvil_vitis_roots "$ENV{XILINX_VITIS}")
  elseif(VPP_EXECUTABLE)
    get_filename_component(_anvil_vpp_bin "${VPP_EXECUTABLE}" DIRECTORY)
    get_filename_component(_anvil_vitis_root "${_anvil_vpp_bin}" DIRECTORY)
    list(APPEND _anvil_vitis_roots "${_anvil_vitis_root}")
  endif()

  set(_anvil_vitis_hls_expanded_roots)
  foreach(_anvil_vitis_root IN LISTS _anvil_vitis_roots)
    list(APPEND _anvil_vitis_hls_expanded_roots "${_anvil_vitis_root}")
    if(_anvil_vitis_root MATCHES "[/\\]Vitis[/\\]([^/\\]+)$")
      set(_anvil_vitis_hls_sibling "${_anvil_vitis_root}")
      string(REGEX REPLACE "[/\\]Vitis[/\\][^/\\]+$" "/Vitis_HLS/${CMAKE_MATCH_1}" _anvil_vitis_hls_sibling "${_anvil_vitis_hls_sibling}")
      if(EXISTS "${_anvil_vitis_hls_sibling}")
        list(APPEND _anvil_vitis_hls_expanded_roots "${_anvil_vitis_hls_sibling}")
      endif()
    endif()
  endforeach()
  if(_anvil_vitis_hls_expanded_roots)
    list(REMOVE_DUPLICATES _anvil_vitis_hls_expanded_roots)
  endif()

  set(_anvil_vitis_hls_includes)
  foreach(_anvil_vitis_root IN LISTS _anvil_vitis_hls_expanded_roots)
    if(NOT _anvil_vitis_root)
      continue()
    endif()
    foreach(_anvil_candidate IN ITEMS
        "include"
        "system_compiler/include"
        "target/x86/include"
        "vcxx/data/include"
        "vcxx/data/autopilot"
        "common/technology/autopilot"
        "data/system_compiler/include"
        "data/emulation/sysc_gen/include")
      if(EXISTS "${_anvil_vitis_root}/${_anvil_candidate}")
        list(APPEND _anvil_vitis_hls_includes "${_anvil_vitis_root}/${_anvil_candidate}")
      endif()
    endforeach()
  endforeach()
  if(_anvil_vitis_hls_includes)
    list(REMOVE_DUPLICATES _anvil_vitis_hls_includes)
  endif()
  set(${out_var} "${_anvil_vitis_hls_includes}" PARENT_SCOPE)
endfunction()

function(add_anvil_kernel)
  set(options NO_ALL NO_CTEST)
  set(one_value_args NAME TOP CLOCK_HZ PLATFORM_KIND TESTBENCH)
  set(multi_value_args SOURCES)
  cmake_parse_arguments(AK "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

  if(NOT AK_NAME)
    list(LENGTH AK_UNPARSED_ARGUMENTS _ak_unparsed_count)
    if(_ak_unparsed_count GREATER 0)
      list(GET AK_UNPARSED_ARGUMENTS 0 AK_NAME)
      list(REMOVE_AT AK_UNPARSED_ARGUMENTS 0)
    endif()
  endif()

  if(NOT AK_NAME)
    message(FATAL_ERROR "add_anvil_kernel: NAME is required")
  endif()
  if(NOT AK_SOURCES)
    message(FATAL_ERROR "add_anvil_kernel(${AK_NAME}): SOURCES required")
  endif()
  if(NOT AK_PLATFORM_KIND)
    set(AK_PLATFORM_KIND "${ANVIL_PLATFORM_KIND}")
  endif()
  if(NOT AK_PLATFORM_KIND)
    message(FATAL_ERROR
      "add_anvil_kernel(${AK_NAME}): PLATFORM_KIND required (e.g. u250) "
      "or set ANVIL_PLATFORM_KIND")
  endif()
  if(AK_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR
      "add_anvil_kernel(${AK_NAME}): unexpected arguments: ${AK_UNPARSED_ARGUMENTS}")
  endif()
  list(FIND AK_SOURCES CONFIG _ak_config_source_index)
  if(NOT _ak_config_source_index EQUAL -1)
    list(SUBLIST AK_SOURCES ${_ak_config_source_index} -1 _ak_unexpected_sources)
    message(FATAL_ERROR
      "add_anvil_kernel(${AK_NAME}): unexpected arguments: ${_ak_unexpected_sources}")
  endif()

  if(NOT AK_TOP)
    set(AK_TOP "${AK_NAME}")
  endif()
  if(NOT AK_CLOCK_HZ)
    if(NOT ANVIL_CLOCK_MHZ MATCHES "^[0-9]+$")
      message(FATAL_ERROR
        "add_anvil_kernel(${AK_NAME}): ANVIL_CLOCK_MHZ must be a positive integer, "
        "got '${ANVIL_CLOCK_MHZ}'")
    endif()
    if(ANVIL_CLOCK_MHZ LESS_EQUAL 0)
      message(FATAL_ERROR
        "add_anvil_kernel(${AK_NAME}): ANVIL_CLOCK_MHZ must be greater than zero, "
        "got '${ANVIL_CLOCK_MHZ}'")
    endif()
    math(EXPR AK_CLOCK_HZ "${ANVIL_CLOCK_MHZ} * 1000000")
  elseif(NOT AK_CLOCK_HZ MATCHES "^[0-9]+$")
    message(FATAL_ERROR
      "add_anvil_kernel(${AK_NAME}): CLOCK_HZ must be a positive integer, got '${AK_CLOCK_HZ}'")
  elseif(AK_CLOCK_HZ LESS_EQUAL 0)
    message(FATAL_ERROR
      "add_anvil_kernel(${AK_NAME}): CLOCK_HZ must be greater than zero, got '${AK_CLOCK_HZ}'")
  endif()

  set(_ak_abs_sources)
  foreach(_ak_src IN LISTS AK_SOURCES)
    if(IS_ABSOLUTE "${_ak_src}")
      set(_ak_abs_src "${_ak_src}")
    else()
      get_filename_component(_ak_abs_src "${_ak_src}" ABSOLUTE
                             BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()
    if(NOT EXISTS "${_ak_abs_src}")
      message(FATAL_ERROR
        "add_anvil_kernel(${AK_NAME}): kernel source not found: ${_ak_abs_src}")
    endif()
    list(APPEND _ak_abs_sources "${_ak_abs_src}")
  endforeach()

  if(AK_TESTBENCH)
    if(IS_ABSOLUTE "${AK_TESTBENCH}")
      set(_ak_abs_testbench "${AK_TESTBENCH}")
    else()
      get_filename_component(_ak_abs_testbench "${AK_TESTBENCH}" ABSOLUTE
                             BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()
    if(NOT EXISTS "${_ak_abs_testbench}")
      message(FATAL_ERROR
        "add_anvil_kernel(${AK_NAME}): testbench not found: ${_ak_abs_testbench}")
    endif()

    if(NOT ANVIL_VITIS_PART)
      message(FATAL_ERROR
        "add_anvil_kernel(${AK_NAME}): ANVIL_VITIS_PART is not set. "
        "Set it via config/<target>/anvil.mk, CMakePresets.json, or -DANVIL_VITIS_PART=...")
    endif()
    set(_ak_cosim_part "${ANVIL_VITIS_PART}")
  endif()

  set(_ak_work_dir "${CMAKE_CURRENT_BINARY_DIR}/${AK_NAME}_hls")
  set(_ak_cfg "${_ak_work_dir}/hls.cfg")
  set(_ak_cosim_cfg "${_ak_work_dir}/cosim.cfg")
  set(_ak_xo "${_ak_work_dir}/${AK_NAME}.xo")
  set(_ak_csynth_stamp "${_ak_work_dir}/.csynth.stamp")
  set(_ak_csynth_depfile "${_ak_work_dir}/.csynth.d")
  set(_ak_csynth_action_json "${_ak_work_dir}/csynth.action.json")
  set(_ak_csynth_action_sha "${_ak_work_dir}/csynth.action.sha256")
  set(_ak_csynth_env_json "${_ak_work_dir}/csynth.env.json")
  set(_ak_csynth_env_sha "${_ak_work_dir}/csynth.env.sha256")
  set(_ak_csynth_manifest "${_ak_work_dir}/csynth.manifest.json")
  set(_ak_csynth_xml "${_ak_work_dir}/hls/syn/report/${AK_TOP}_csynth.xml")

  _anvil_kernel_reject_space_path("${AK_NAME}" "source directory" "${CMAKE_CURRENT_SOURCE_DIR}")
  _anvil_kernel_reject_space_path("${AK_NAME}" "build directory" "${CMAKE_CURRENT_BINARY_DIR}")
  _anvil_kernel_reject_space_path("${AK_NAME}" "Vitis platform path" "${ANVIL_VITIS_PLATFORM}")
  _anvil_kernel_reject_space_path("${AK_NAME}" "public include path" "${PROJECT_SOURCE_DIR}/include")
  _anvil_kernel_reject_space_path("${AK_NAME}" "generated include path" "${CMAKE_BINARY_DIR}/generated")
  _anvil_kernel_reject_space_path("${AK_NAME}" "project kernel include path" "${PROJECT_SOURCE_DIR}/src/kernels/include")
  _anvil_kernel_reject_space_path("${AK_NAME}" "hlslib include path" "${PROJECT_SOURCE_DIR}/third_party/hlslib/include")
  foreach(_ak_abs_src IN LISTS _ak_abs_sources)
    _anvil_kernel_reject_space_path("${AK_NAME}" "kernel source path" "${_ak_abs_src}")
  endforeach()
  if(AK_TESTBENCH)
    _anvil_kernel_reject_space_path("${AK_NAME}" "testbench path" "${_ak_abs_testbench}")
  endif()
  _anvil_kernel_reject_space_path("${AK_NAME}" "kernel work directory" "${_ak_work_dir}")
  _anvil_kernel_reject_space_path("${AK_NAME}" "kernel artifact path" "${_ak_xo}")

  file(MAKE_DIRECTORY "${_ak_work_dir}")

  set(_ak_syn_files "")
  foreach(_ak_abs_src IN LISTS _ak_abs_sources)
    string(APPEND _ak_syn_files "syn.file=${_ak_abs_src}\n")
  endforeach()

  _anvil_vitis_hls_include_dirs(_ak_vitis_include_dirs)
  set(_ak_cflags
    "-std=${ANVIL_HLS_STD} -DHLSLIB_SYNTHESIS -I${PROJECT_SOURCE_DIR}/include -I${CMAKE_BINARY_DIR}/generated -I${PROJECT_SOURCE_DIR}/src/kernels/include -I${PROJECT_SOURCE_DIR}/third_party/hlslib/include")
  set(_ak_tb_cflags
    "-std=${ANVIL_HLS_STD} -I${PROJECT_SOURCE_DIR}/include -I${CMAKE_BINARY_DIR}/generated -I${PROJECT_SOURCE_DIR}/src/kernels/include -I${PROJECT_SOURCE_DIR}/third_party/hlslib/include")
  foreach(_ak_vitis_include_dir IN LISTS _ak_vitis_include_dirs)
    string(APPEND _ak_cflags " -I${_ak_vitis_include_dir}")
    string(APPEND _ak_tb_cflags " -I${_ak_vitis_include_dir}")
  endforeach()
  set(_ak_dep_include_args
    --dep-include "${PROJECT_SOURCE_DIR}/include"
    --dep-include "${CMAKE_BINARY_DIR}/generated"
    --dep-include "${PROJECT_SOURCE_DIR}/src/kernels/include"
    --dep-include "${PROJECT_SOURCE_DIR}/third_party/hlslib/include")
  foreach(_ak_vitis_include_dir IN LISTS _ak_vitis_include_dirs)
    list(APPEND _ak_dep_include_args --dep-include "${_ak_vitis_include_dir}")
  endforeach()
  set(_ak_dep_source_args)
  set(_ak_input_args)
  foreach(_ak_abs_src IN LISTS _ak_abs_sources)
    list(APPEND _ak_dep_source_args --dep-source "${_ak_abs_src}")
    list(APPEND _ak_input_args --input "${_ak_abs_src}")
  endforeach()
  set(_ak_action_meta_args
    --target-identity "${AK_PLATFORM_KIND}"
    --mode-identity "${ANVIL_VITIS_TARGET}"
    --platform "${ANVIL_VITIS_PLATFORM}"
    --clock "${AK_CLOCK_HZ}"
    --top "${AK_TOP}")
  if(ANVIL_DEVICE_KIND)
    list(APPEND _ak_action_meta_args --target-kind "${ANVIL_DEVICE_KIND}")
  endif()
  if(ANVIL_VITIS_PART)
    list(APPEND _ak_action_meta_args --part "${ANVIL_VITIS_PART}")
  endif()

  set(_ak_hls_legacy_vitis_2022 OFF)
  if(VITIS_VERSION MATCHES "^2022[.]")
    set(_ak_hls_legacy_vitis_2022 ON)
  endif()
  if(_ak_hls_legacy_vitis_2022 AND NOT ANVIL_VITIS_PART)
    message(FATAL_ERROR
      "add_anvil_kernel(${AK_NAME}): Vitis ${VITIS_VERSION} HLS config requires "
      "ANVIL_VITIS_PART because v++ --mode hls does not accept platform= in hls.cfg")
  endif()

  if(_ak_hls_legacy_vitis_2022)
    # Vitis 2022.2 rejects top-level platform=/freqhz= in --mode hls config.
    # Use the legacy part=/[hls] clock= spelling and the older syn.output.*
    # keys. Newer Vitis accepts syn.output.* too, but keep package.output.*
    # for the 2024.x path to match the current documentation.
    set(_ak_cfg_device_line "part=${ANVIL_VITIS_PART}\n")
    set(_ak_cfg_freq_line "")
    set(_ak_cfg_clock_line "clock=${ANVIL_CLOCK_MHZ}MHz\n")
    set(_ak_cfg_output_lines "syn.output.format=xo\nsyn.output.file=${_ak_xo}\n")
  else()
    set(_ak_cfg_device_line "platform=${ANVIL_VITIS_PLATFORM}\n")
    set(_ak_cfg_freq_line "freqhz=${AK_CLOCK_HZ}\n")
    set(_ak_cfg_clock_line "")
    set(_ak_cfg_output_lines "package.output.format=xo\npackage.output.file=${_ak_xo}\n")
  endif()

  # Vitis HLS compile mode accepts platform/frequency in newer releases but not
  # --target; ANVIL_VITIS_TARGET is retained as target metadata for future
  # xclbin/link work. Keep generated config timestamps stable across repeated
  # configure calls; otherwise every Makefile invocation rewrites hls.cfg/cosim.cfg
  # and Ninja correctly assumes hour-scale HLS/cosim outputs are stale.
  string(CONCAT _ak_cfg_content
    "${_ak_cfg_device_line}"
    "${_ak_cfg_freq_line}"
    "\n"
    "[hls]\n"
    "${_ak_cfg_clock_line}"
    "syn.top=${AK_TOP}\n"
    "syn.cflags=${_ak_cflags}\n"
    "${_ak_syn_files}"
    "flow_target=vitis\n"
    "${_ak_cfg_output_lines}")
  _anvil_write_file_if_changed("${_ak_cfg}" "${_ak_cfg_content}")

  if(AK_TESTBENCH)
    if(_ak_hls_legacy_vitis_2022)
      # Vitis 2022.2 cosim is driven through v++ --compile --mode hls with
      # hls.run=vivado. It rejects freqhz=, tb.file*, and cosim.trace_level.
      string(CONCAT _ak_cosim_cfg_content
        "part=${_ak_cosim_part}\n"
        "\n"
        "[hls]\n"
        "clock=${ANVIL_CLOCK_MHZ}MHz\n"
        "syn.top=${AK_TOP}\n"
        "syn.cflags=${_ak_cflags}\n"
        "${_ak_syn_files}"
        "sim.file=${_ak_abs_testbench}\n"
        "sim.file_cflags=${_ak_abs_testbench},${_ak_tb_cflags}\n"
        "sim.hw.trace_level=none\n"
        "run=vivado\n")
    else()
      # Vitis 2024.2 cosim rejects the compile config's top-level platform= key;
      # keep cosim on a separate part-based config.
      string(CONCAT _ak_cosim_cfg_content
        "part=${_ak_cosim_part}\n"
        "freqhz=${AK_CLOCK_HZ}\n"
        "\n"
        "[hls]\n"
        "syn.top=${AK_TOP}\n"
        "syn.cflags=${_ak_cflags}\n"
        "${_ak_syn_files}"
        "tb.file=${_ak_abs_testbench}\n"
        "tb.file_cflags=${_ak_abs_testbench},${_ak_tb_cflags}\n"
        "cosim.trace_level=none\n")
    endif()
    _anvil_write_file_if_changed("${_ak_cosim_cfg}" "${_ak_cosim_cfg_content}")
  endif()

  add_custom_command(
    OUTPUT "${_ak_csynth_stamp}"
    BYPRODUCTS
      "${_ak_xo}"
      "${_ak_csynth_xml}"
      "${_ak_csynth_depfile}"
      "${_ak_csynth_action_json}"
      "${_ak_csynth_action_sha}"
      "${_ak_csynth_env_json}"
      "${_ak_csynth_env_sha}"
      "${_ak_csynth_manifest}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_ak_work_dir}"
    COMMAND "${CMAKE_COMMAND}" -E env "PYTHONPATH=${PROJECT_SOURCE_DIR}/tools"
            "${Python3_EXECUTABLE}" -m hlsflow.action_runner
            --stage csynth
            --stamp "${_ak_csynth_stamp}"
            --manifest "${_ak_csynth_manifest}"
            --action-json "${_ak_csynth_action_json}"
            --action-sha "${_ak_csynth_action_sha}"
            --env-json "${_ak_csynth_env_json}"
            --env-sha "${_ak_csynth_env_sha}"
            ${_ak_action_meta_args}
            --config "${_ak_cfg}"
            --depfile "${_ak_csynth_depfile}"
            --dep-compiler "${CMAKE_CXX_COMPILER}"
            --dep-std "${ANVIL_HLS_STD}"
            --dep-define HLSLIB_SYNTHESIS
            ${_ak_dep_include_args}
            ${_ak_dep_source_args}
            ${_ak_input_args}
            --input "${_ak_cfg}"
            --output "${_ak_xo}"
            --output "${_ak_csynth_xml}"
            --
            "${VPP_EXECUTABLE}"
            --compile
            --mode hls
            --config "${_ak_cfg}"
            --work_dir "${_ak_work_dir}"
    DEPENDS ${_ak_abs_sources} "${_ak_cfg}"
    DEPFILE "${_ak_csynth_depfile}"
    COMMENT "Running Vitis HLS csynth for ${AK_NAME}"
    COMMAND_EXPAND_LISTS
    VERBATIM
    USES_TERMINAL)

  if(AK_NO_ALL)
    add_custom_target("${AK_NAME}_xo"
      DEPENDS "${_ak_csynth_stamp}")
  else()
    add_custom_target("${AK_NAME}_xo" ALL
      DEPENDS "${_ak_csynth_stamp}")
  endif()

  set_target_properties("${AK_NAME}_xo" PROPERTIES
    ANVIL_KERNEL_ARTIFACT "${_ak_xo}"
    ANVIL_KERNEL_CSYNTH_STAMP "${_ak_csynth_stamp}"
    ANVIL_KERNEL_CSYNTH_XML "${_ak_csynth_xml}"
    ANVIL_KERNEL_WORK_DIR "${_ak_work_dir}"
    ANVIL_KERNEL_TOP "${AK_TOP}"
    ANVIL_KERNEL_PLATFORM_KIND "${AK_PLATFORM_KIND}"
    ANVIL_KERNEL_CLOCK_HZ "${AK_CLOCK_HZ}"
    ANVIL_KERNEL_VITIS_TARGET "${ANVIL_VITIS_TARGET}")

  if(AK_TESTBENCH)
    set(_ak_cosim_stamp "${_ak_work_dir}/.cosim.stamp")
    set(_ak_cosim_depfile "${_ak_work_dir}/.cosim.d")
    set(_ak_cosim_action_json "${_ak_work_dir}/cosim.action.json")
    set(_ak_cosim_action_sha "${_ak_work_dir}/cosim.action.sha256")
    set(_ak_cosim_env_json "${_ak_work_dir}/cosim.env.json")
    set(_ak_cosim_env_sha "${_ak_work_dir}/cosim.env.sha256")
    set(_ak_cosim_manifest "${_ak_work_dir}/cosim.manifest.json")
    set(_ak_cosim_dep_source_args ${_ak_dep_source_args})
    list(APPEND _ak_cosim_dep_source_args --dep-source "${_ak_abs_testbench}")
    set(_ak_cosim_input_args ${_ak_input_args})
    list(APPEND _ak_cosim_input_args
      --input "${_ak_xo}"
      --input "${_ak_abs_testbench}"
      --input "${_ak_cosim_cfg}")
    if(_ak_hls_legacy_vitis_2022)
      set(_ak_cosim_runner_command
        "${VPP_EXECUTABLE}"
        --compile
        --mode hls
        --config "${_ak_cosim_cfg}"
        --work_dir "${_ak_work_dir}")
    else()
      set(_ak_cosim_runner_command
        "${VITIS_RUN_EXECUTABLE}"
        --cosim
        --config "${_ak_cosim_cfg}"
        --work_dir "${_ak_work_dir}")
    endif()

    # vitis-run 2024.2 defaults to --mode hls; the flag is intentionally
    # omitted so this command stays valid if a future release renames mode
    # tokens (we only need the HLS cosim mode here). Vitis 2022.2 has no
    # compatible vitis-run entrypoint, so it uses v++ --compile --mode hls.
    add_custom_command(
      OUTPUT "${_ak_cosim_stamp}"
      BYPRODUCTS
        "${_ak_cosim_depfile}"
        "${_ak_cosim_action_json}"
        "${_ak_cosim_action_sha}"
        "${_ak_cosim_env_json}"
        "${_ak_cosim_env_sha}"
        "${_ak_cosim_manifest}"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${_ak_work_dir}"
      COMMAND "${CMAKE_COMMAND}" -E env "PYTHONPATH=${PROJECT_SOURCE_DIR}/tools"
              "${Python3_EXECUTABLE}" -m hlsflow.action_runner
              --stage cosim
              --stamp "${_ak_cosim_stamp}"
              --manifest "${_ak_cosim_manifest}"
              --action-json "${_ak_cosim_action_json}"
              --action-sha "${_ak_cosim_action_sha}"
              --env-json "${_ak_cosim_env_json}"
              --env-sha "${_ak_cosim_env_sha}"
              ${_ak_action_meta_args}
              --config "${_ak_cosim_cfg}"
              --depfile "${_ak_cosim_depfile}"
              --dep-compiler "${CMAKE_CXX_COMPILER}"
              --dep-std "${ANVIL_HLS_STD}"
              ${_ak_dep_include_args}
              ${_ak_cosim_dep_source_args}
              ${_ak_cosim_input_args}
              --output "${_ak_cosim_stamp}"
              --
              ${_ak_cosim_runner_command}
      DEPENDS "${_ak_csynth_stamp}" "${_ak_abs_testbench}" "${_ak_cosim_cfg}" ${_ak_abs_sources}
      DEPFILE "${_ak_cosim_depfile}"
      COMMENT "Running Vitis HLS cosim for ${AK_NAME}"
      COMMAND_EXPAND_LISTS
      VERBATIM
      USES_TERMINAL)

    add_custom_target("${AK_NAME}_cosim"
      DEPENDS "${_ak_cosim_stamp}")

    set_target_properties("${AK_NAME}_cosim" PROPERTIES
      ANVIL_KERNEL_COSIM_STAMP "${_ak_cosim_stamp}"
      ANVIL_KERNEL_COSIM_CONFIG "${_ak_cosim_cfg}"
      ANVIL_KERNEL_TESTBENCH "${_ak_abs_testbench}"
      ANVIL_KERNEL_WORK_DIR "${_ak_work_dir}"
      ANVIL_KERNEL_TOP "${AK_TOP}"
      ANVIL_KERNEL_PLATFORM_KIND "${AK_PLATFORM_KIND}"
      ANVIL_KERNEL_CLOCK_HZ "${AK_CLOCK_HZ}")
  endif()

  if(ANVIL_BUILD_TESTS AND NOT AK_NO_CTEST)
    if(NOT Python3_EXECUTABLE)
      message(FATAL_ERROR
        "add_anvil_kernel(${AK_NAME}): Python3 interpreter is required for csynth checks")
    endif()

    add_test(
      NAME "${AK_NAME}_csynth_build"
      COMMAND "${CMAKE_COMMAND}"
              --build "${CMAKE_BINARY_DIR}"
              --target "${AK_NAME}_xo")
    add_test(
      NAME "${AK_NAME}_csynth_check"
      COMMAND "${Python3_EXECUTABLE}"
              "${PROJECT_SOURCE_DIR}/cmake/parse_hls_report.py"
              "${_ak_csynth_xml}"
              --max-ii 1)
    set_tests_properties("${AK_NAME}_csynth_build" PROPERTIES
      LABELS "csynth;build"
      FIXTURES_SETUP "${AK_NAME}_csynth_fixture"
      RESOURCE_LOCK "${AK_NAME}_hls_build_tree")
    set_tests_properties("${AK_NAME}_csynth_check" PROPERTIES
      LABELS csynth
      FIXTURES_REQUIRED "${AK_NAME}_csynth_fixture")

    if(AK_TESTBENCH)
      add_test(
        NAME "${AK_NAME}_cosim_vs_gold"
        COMMAND "${CMAKE_COMMAND}"
                --build "${CMAKE_BINARY_DIR}"
                --target "${AK_NAME}_cosim")
      set_tests_properties("${AK_NAME}_cosim_vs_gold" PROPERTIES
        LABELS cosim
        FIXTURES_REQUIRED "${AK_NAME}_csynth_fixture"
        RESOURCE_LOCK "${AK_NAME}_hls_build_tree"
        TIMEOUT 600)
    endif()
  endif()
endfunction()

function(add_anvil_xclbin)
  set(options)
  set(one_value_args NAME PLATFORM_KIND LINK_CFG MODE)
  set(multi_value_args KERNEL_TARGETS)
  cmake_parse_arguments(AKX "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

  if(NOT AKX_NAME)
    message(FATAL_ERROR "add_anvil_xclbin: NAME is required")
  endif()
  if(NOT AKX_KERNEL_TARGETS)
    message(FATAL_ERROR "add_anvil_xclbin(${AKX_NAME}): KERNEL_TARGETS required")
  endif()
  if(NOT AKX_PLATFORM_KIND)
    set(AKX_PLATFORM_KIND "${ANVIL_PLATFORM_KIND}")
  endif()
  if(NOT AKX_PLATFORM_KIND)
    message(FATAL_ERROR
      "add_anvil_xclbin(${AKX_NAME}): PLATFORM_KIND required (e.g. u250) "
      "or set ANVIL_PLATFORM_KIND")
  endif()
  if(AKX_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR
      "add_anvil_xclbin(${AKX_NAME}): unexpected arguments: ${AKX_UNPARSED_ARGUMENTS}")
  endif()

  if(NOT AKX_LINK_CFG)
    set(AKX_LINK_CFG "${PROJECT_SOURCE_DIR}/config/${AKX_PLATFORM_KIND}/link.cfg")
  elseif(NOT IS_ABSOLUTE "${AKX_LINK_CFG}")
    get_filename_component(AKX_LINK_CFG "${AKX_LINK_CFG}" ABSOLUTE
                           BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
  endif()
  if(NOT EXISTS "${AKX_LINK_CFG}")
    message(FATAL_ERROR
      "add_anvil_xclbin(${AKX_NAME}): LINK_CFG not found: ${AKX_LINK_CFG}")
  endif()

  if(NOT AKX_MODE)
    if(ANVIL_VITIS_TARGET)
      set(AKX_MODE "${ANVIL_VITIS_TARGET}")
    else()
      set(AKX_MODE "hw")
    endif()
  endif()
  if(NOT AKX_MODE MATCHES "^(hw|hw_emu|sw_emu)$")
    message(FATAL_ERROR
      "add_anvil_xclbin(${AKX_NAME}): MODE must be one of hw, hw_emu, or sw_emu; "
      "got '${AKX_MODE}'")
  endif()

  set(_akx_artifacts)
  foreach(_akx_target IN LISTS AKX_KERNEL_TARGETS)
    if(NOT TARGET "${_akx_target}")
      message(FATAL_ERROR
        "add_anvil_xclbin(${AKX_NAME}): kernel target not found: ${_akx_target}")
    endif()
    get_target_property(_akx_artifact "${_akx_target}" ANVIL_KERNEL_ARTIFACT)
    if(NOT _akx_artifact)
      message(FATAL_ERROR
        "add_anvil_xclbin(${AKX_NAME}): target '${_akx_target}' does not set "
        "ANVIL_KERNEL_ARTIFACT")
    endif()
    list(APPEND _akx_artifacts "${_akx_artifact}")
  endforeach()

  set(_akx_out_dir "${CMAKE_CURRENT_BINARY_DIR}/${AKX_NAME}_xclbin")
  set(_akx_work_dir "${_akx_out_dir}/work")
  set(_akx_xclbin "${_akx_out_dir}/${AKX_NAME}.xclbin")
  set(_akx_link_stamp "${_akx_out_dir}/.link.stamp")
  set(_akx_action_json "${_akx_out_dir}/link.action.json")
  set(_akx_action_sha "${_akx_out_dir}/link.action.sha256")
  set(_akx_env_json "${_akx_out_dir}/link.env.json")
  set(_akx_env_sha "${_akx_out_dir}/link.env.sha256")
  set(_akx_manifest "${_akx_out_dir}/link.manifest.json")
  set(_akx_input_args)
  foreach(_akx_artifact IN LISTS _akx_artifacts)
    list(APPEND _akx_input_args --input "${_akx_artifact}")
  endforeach()
  list(APPEND _akx_input_args --input "${AKX_LINK_CFG}")
  set(_akx_action_meta_args
    --target-identity "${AKX_PLATFORM_KIND}"
    --mode-identity "${AKX_MODE}"
    --platform "${ANVIL_VITIS_PLATFORM}")
  if(ANVIL_DEVICE_KIND)
    list(APPEND _akx_action_meta_args --target-kind "${ANVIL_DEVICE_KIND}")
  endif()
  if(ANVIL_VITIS_PART)
    list(APPEND _akx_action_meta_args --part "${ANVIL_VITIS_PART}")
  endif()

  add_custom_command(
    OUTPUT "${_akx_link_stamp}"
    BYPRODUCTS
      "${_akx_xclbin}"
      "${_akx_action_json}"
      "${_akx_action_sha}"
      "${_akx_env_json}"
      "${_akx_env_sha}"
      "${_akx_manifest}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_akx_out_dir}" "${_akx_work_dir}"
    COMMAND "${CMAKE_COMMAND}" -E env "PYTHONPATH=${PROJECT_SOURCE_DIR}/tools"
            "${Python3_EXECUTABLE}" -m hlsflow.action_runner
            --stage xclbin
            --stamp "${_akx_link_stamp}"
            --manifest "${_akx_manifest}"
            --action-json "${_akx_action_json}"
            --action-sha "${_akx_action_sha}"
            --env-json "${_akx_env_json}"
            --env-sha "${_akx_env_sha}"
            ${_akx_action_meta_args}
            --config "${AKX_LINK_CFG}"
            ${_akx_input_args}
            --output "${_akx_xclbin}"
            --
            "${VPP_EXECUTABLE}"
            --link
            --platform "${ANVIL_VITIS_PLATFORM}"
            --target "${AKX_MODE}"
            --config "${AKX_LINK_CFG}"
            --temp_dir "${_akx_work_dir}"
            ${_akx_artifacts}
            -o "${_akx_xclbin}"
    DEPENDS ${_akx_artifacts} "${AKX_LINK_CFG}"
    COMMENT "Linking Vitis xclbin for ${AKX_NAME} (${AKX_PLATFORM_KIND}, ${AKX_MODE})"
    COMMAND_EXPAND_LISTS
    VERBATIM
    USES_TERMINAL)

  add_custom_target("${AKX_NAME}_xclbin"
    DEPENDS "${_akx_link_stamp}")

  set_target_properties("${AKX_NAME}_xclbin" PROPERTIES
    ANVIL_XCLBIN "${_akx_xclbin}"
    ANVIL_XCLBIN_LINK_STAMP "${_akx_link_stamp}"
    ANVIL_XCLBIN_MODE "${AKX_MODE}"
    ANVIL_XCLBIN_LINK_CFG "${AKX_LINK_CFG}"
    ANVIL_XCLBIN_PLATFORM_KIND "${AKX_PLATFORM_KIND}")
endfunction()
