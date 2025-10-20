cmake_minimum_required(VERSION 3.15...3.23)

project(afsproject_tests LANGUAGES CXX)

if(PROJECT_IS_TOP_LEVEL OR TEST_INSTALLED_VERSION)
  enable_testing()

  find_package(afsproject CONFIG REQUIRED)

  if(NOT TARGET afsproject_options)
    message(FATAL_ERROR "Required config package not found!")
    return()
  endif()
endif()

# find_package(Catch2 3 REQUIRED)
include(${Catch2_SOURCE_DIR}/extras/Catch.cmake)

add_test(NAME cli.has_help COMMAND AFS --help)

add_test(NAME cli.version_matches COMMAND AFS --version)
set_tests_properties(cli.version_matches PROPERTIES PASS_REGULAR_EXPRESSION "${PROJECT_VERSION}")

# Include test fixtures
# add_subdirectory(fixtures)

# Include integration test
add_subdirectory(integration)
