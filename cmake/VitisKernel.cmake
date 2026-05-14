# cmake/VitisKernel.cmake
# Phase 2 Vitis HLS kernel helpers.

function(_accel_kernel_reject_space_path kernel_name path_label path_value)
  if("${path_value}" MATCHES "[ \t]")
    message(FATAL_ERROR
      "add_accel_kernel(${kernel_name}): ${path_label} contains whitespace, "
      "which is not supported by the generated Vitis HLS config: '${path_value}'")
  endif()
endfunction()

function(add_accel_kernel)
  set(options)
  set(one_value_args NAME TOP CLOCK_HZ PLATFORM_KIND CONFIG)
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
    message(FATAL_ERROR "add_accel_kernel: NAME is required")
  endif()
  if(NOT AK_SOURCES)
    message(FATAL_ERROR "add_accel_kernel(${AK_NAME}): SOURCES required")
  endif()
  if(NOT AK_PLATFORM_KIND)
    set(AK_PLATFORM_KIND "${ACCEL_PLATFORM_KIND}")
  endif()
  if(NOT AK_PLATFORM_KIND)
    message(FATAL_ERROR
      "add_accel_kernel(${AK_NAME}): PLATFORM_KIND required (e.g. alveo_u250) "
      "or set ACCEL_PLATFORM_KIND")
  endif()
  if(AK_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR
      "add_accel_kernel(${AK_NAME}): unexpected arguments: ${AK_UNPARSED_ARGUMENTS}")
  endif()

  if(NOT AK_TOP)
    set(AK_TOP "${AK_NAME}")
  endif()
  if(NOT AK_CLOCK_HZ)
    if(NOT ACCEL_CLOCK_MHZ MATCHES "^[0-9]+$")
      message(FATAL_ERROR
        "add_accel_kernel(${AK_NAME}): ACCEL_CLOCK_MHZ must be a positive integer, "
        "got '${ACCEL_CLOCK_MHZ}'")
    endif()
    if(ACCEL_CLOCK_MHZ LESS_EQUAL 0)
      message(FATAL_ERROR
        "add_accel_kernel(${AK_NAME}): ACCEL_CLOCK_MHZ must be greater than zero, "
        "got '${ACCEL_CLOCK_MHZ}'")
    endif()
    math(EXPR AK_CLOCK_HZ "${ACCEL_CLOCK_MHZ} * 1000000")
  elseif(NOT AK_CLOCK_HZ MATCHES "^[0-9]+$")
    message(FATAL_ERROR
      "add_accel_kernel(${AK_NAME}): CLOCK_HZ must be a positive integer, got '${AK_CLOCK_HZ}'")
  elseif(AK_CLOCK_HZ LESS_EQUAL 0)
    message(FATAL_ERROR
      "add_accel_kernel(${AK_NAME}): CLOCK_HZ must be greater than zero, got '${AK_CLOCK_HZ}'")
  endif()

  set(_ak_abs_sources)
  foreach(_ak_src IN LISTS AK_SOURCES)
    if(IS_ABSOLUTE "${_ak_src}")
      set(_ak_abs_src "${_ak_src}")
    else()
      get_filename_component(_ak_abs_src "${_ak_src}" ABSOLUTE
                             BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()
    list(APPEND _ak_abs_sources "${_ak_abs_src}")
  endforeach()

  set(_ak_work_dir "${CMAKE_CURRENT_BINARY_DIR}/${AK_NAME}_hls")
  set(_ak_cfg "${_ak_work_dir}/hls.cfg")
  set(_ak_xo "${_ak_work_dir}/${AK_NAME}.xo")
  set(_ak_csynth_xml "${_ak_work_dir}/hls/syn/report/${AK_TOP}_csynth.xml")

  _accel_kernel_reject_space_path("${AK_NAME}" "source directory" "${CMAKE_CURRENT_SOURCE_DIR}")
  _accel_kernel_reject_space_path("${AK_NAME}" "build directory" "${CMAKE_CURRENT_BINARY_DIR}")
  _accel_kernel_reject_space_path("${AK_NAME}" "Vitis platform path" "${ACCEL_VITIS_PLATFORM}")
  _accel_kernel_reject_space_path("${AK_NAME}" "public include path" "${PROJECT_SOURCE_DIR}/include")
  _accel_kernel_reject_space_path("${AK_NAME}" "hlslib include path" "${PROJECT_SOURCE_DIR}/third_party/hlslib/include")
  foreach(_ak_abs_src IN LISTS _ak_abs_sources)
    _accel_kernel_reject_space_path("${AK_NAME}" "kernel source path" "${_ak_abs_src}")
  endforeach()
  _accel_kernel_reject_space_path("${AK_NAME}" "kernel work directory" "${_ak_work_dir}")
  _accel_kernel_reject_space_path("${AK_NAME}" "kernel artifact path" "${_ak_xo}")

  file(MAKE_DIRECTORY "${_ak_work_dir}")

  set(_ak_syn_files "")
  foreach(_ak_abs_src IN LISTS _ak_abs_sources)
    string(APPEND _ak_syn_files "syn.file=${_ak_abs_src}\n")
  endforeach()

  set(_ak_cflags
    "-std=${ACCEL_HLS_STD} -DHLSLIB_SYNTHESIS -I${PROJECT_SOURCE_DIR}/include -I${PROJECT_SOURCE_DIR}/third_party/hlslib/include")

  file(WRITE "${_ak_cfg}"
    "platform=${ACCEL_VITIS_PLATFORM}\n"
    "freqhz=${AK_CLOCK_HZ}\n"
    "\n"
    "[hls]\n"
    "syn.top=${AK_TOP}\n"
    "syn.cflags=${_ak_cflags}\n"
    "${_ak_syn_files}"
    "flow_target=vitis\n"
    "package.output.format=xo\n"
    "package.output.file=${_ak_xo}\n")

  add_custom_command(
    OUTPUT "${_ak_xo}" "${_ak_csynth_xml}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_ak_work_dir}"
    COMMAND "${VPP_EXECUTABLE}"
            --compile
            --mode hls
            --config "${_ak_cfg}"
            --work_dir "${_ak_work_dir}"
    DEPENDS ${_ak_abs_sources} "${_ak_cfg}"
    COMMENT "Running Vitis HLS csynth for ${AK_NAME}"
    VERBATIM
    USES_TERMINAL)

  add_custom_target("${AK_NAME}_xo" ALL
    DEPENDS "${_ak_xo}")

  set_target_properties("${AK_NAME}_xo" PROPERTIES
    ACCEL_KERNEL_ARTIFACT "${_ak_xo}"
    ACCEL_KERNEL_CSYNTH_XML "${_ak_csynth_xml}"
    ACCEL_KERNEL_WORK_DIR "${_ak_work_dir}"
    ACCEL_KERNEL_TOP "${AK_TOP}"
    ACCEL_KERNEL_PLATFORM_KIND "${AK_PLATFORM_KIND}"
    ACCEL_KERNEL_CLOCK_HZ "${AK_CLOCK_HZ}")

  if(ACCEL_BUILD_TESTS)
    if(NOT Python3_EXECUTABLE)
      message(FATAL_ERROR
        "add_accel_kernel(${AK_NAME}): Python3 interpreter is required for csynth checks")
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
      FIXTURES_SETUP "${AK_NAME}_csynth_fixture")
    set_tests_properties("${AK_NAME}_csynth_check" PROPERTIES
      LABELS csynth
      FIXTURES_REQUIRED "${AK_NAME}_csynth_fixture")
  endif()
endfunction()

function(add_accel_kernel_xclbin)
  message(FATAL_ERROR
    "add_accel_kernel_xclbin: not implemented in Task 7; see Task 11")
endfunction()
