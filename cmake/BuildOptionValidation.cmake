# cmake/BuildOptionValidation.cmake
# Validate ACCEL_BUILD_* combinations before subdirectories are added.

if(ACCEL_BUILD_HLS_MODEL AND NOT ACCEL_BUILD_GOLD)
  message(FATAL_ERROR
    "ACCEL_BUILD_HLS_MODEL=ON requires ACCEL_BUILD_GOLD=ON "
    "(HLS model shares SaxpyConfig and is validated against gold).")
endif()

if(ACCEL_BUILD_TESTS AND NOT ACCEL_BUILD_GOLD)
  message(FATAL_ERROR "ACCEL_BUILD_TESTS=ON requires ACCEL_BUILD_GOLD=ON.")
endif()

if(ACCEL_BUILD_APPS AND NOT ACCEL_BUILD_GOLD)
  message(FATAL_ERROR "ACCEL_BUILD_APPS=ON requires ACCEL_BUILD_GOLD=ON.")
endif()
