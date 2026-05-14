# cmake/BuildOptionValidation.cmake
# Validate ANVIL_BUILD_* combinations before subdirectories are added.

if(ANVIL_BUILD_HLS_MODEL AND NOT ANVIL_BUILD_GOLD)
  message(FATAL_ERROR
    "ANVIL_BUILD_HLS_MODEL=ON requires ANVIL_BUILD_GOLD=ON "
    "(HLS model shares SaxpyConfig and is validated against gold).")
endif()

if(ANVIL_BUILD_TESTS AND NOT ANVIL_BUILD_GOLD)
  message(FATAL_ERROR "ANVIL_BUILD_TESTS=ON requires ANVIL_BUILD_GOLD=ON.")
endif()

if(ANVIL_BUILD_APPS AND NOT ANVIL_BUILD_GOLD)
  message(FATAL_ERROR "ANVIL_BUILD_APPS=ON requires ANVIL_BUILD_GOLD=ON.")
endif()
