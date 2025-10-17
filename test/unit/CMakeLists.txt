add_executable(afsproject_unit_tests
  utilities/test_utf8_decoding.cpp
)

target_link_libraries(afsproject_unit_tests
  PRIVATE
    afsproject::afsproject_lib # FIXME: this is wrong
    Catch2::Catch2WithMain
    afsproject::afsproject_options
    afsproject::afsproject_warnings
)

target_compile_definitions(afsproject_unit_tests
  PRIVATE
    TEST_FIXTURES_DIR="${TEST_FIXTURES_DIR}"
)

catch_discover_tests(afsproject_unit_tests)
