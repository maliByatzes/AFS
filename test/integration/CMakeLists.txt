add_executable(afsproject_integration_tests
  test_flac_decoder.cpp
)

target_link_libraries(afsproject_integration_tests
  PRIVATE
    afsproject::afsproject_lib
    Catch2::Catch2WithMain
    afsproject::afsproject_options
    afsproject::afsproject_warnings
)

target_compile_definitions(afsproject_integration_tests
  PRIVATE
    TEST_FIXTURES_DIR="${TEST_FIXTURES_DIR}"
)

catch_discover_tests(afsproject_integration_tests TEST_PREFIX "flac.")
