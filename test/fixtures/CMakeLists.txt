cmake_minimum_required(VERSION 3.15...3.25)

set(TEST_FIXTURES_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/audio_samples")
set(TEST_FIXTURES_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/audio_samples")

file(MAKE_DIRECTORY "${TEST_FIXTURES_BINARY_DIR}")
file(MAKE_DIRECTORY "${TEST_FIXTURES_BINARY_DIR}/subset/stereo")
file(MAKE_DIRECTORY "${TEST_FIXTURES_BINARY_DIR}/subset/high_resolution")
file(MAKE_DIRECTORY "${TEST_FIXTURES_BINARY_DIR}/subset/surround")
file(MAKE_DIRECTORY "${TEST_FIXTURES_BINARY_DIR}/subset/metadata_extremes")
file(MAKE_DIRECTORY "${TEST_FIXTURES_BINARY_DIR}/subset/misc")
file(MAKE_DIRECTORY "${TEST_FIXTURES_BINARY_DIR}/faulty")
file(MAKE_DIRECTORY "${TEST_FIXTURES_BINARY_DIR}/uncommon")

function(copy_fixtures source_dir dest_dir pattern description)
  if(EXISTS "${source_dir}")
    file(GLOB fixtures "${source_dir}/${pattern}")
    if(fixtures)
      message(STATUS "  Found ${description}: ${source_dir}")
      file(COPY ${fixtures} DESTINATION "${dest_dir}")
    endif()
  endif()
endfunction()

message(STATUS "Setting up FLAC test fixtures...")

copy_fixtures(
  "${TEST_FIXTURES_SOURCE_DIR}/subset/stereo"
  "${TEST_FIXTURES_BINARY_DIR}/subset/stereo"
  "*.flac"
  "stereo files"
)

copy_fixtures(
  "${TEST_FIXTURES_SOURCE_DIR}/subset/high_resolution"
  "${TEST_FIXTURES_BINARY_DIR}/subset/high_resolution"
  "*.flac"
  "high resolution files"
)

copy_fixtures(
  "${TEST_FIXTURES_SOURCE_DIR}/subset/surround"
  "${TEST_FIXTURES_BINARY_DIR}/subset/surround"
  "*.flac"
  "surround files"
)

copy_fixtures(
  "${TEST_FIXTURES_SOURCE_DIR}/subset/metadata_extremes"
  "${TEST_FIXTURES_BINARY_DIR}/subset/metadata_extremes"
  "*.flac"
  "metadata extremes files"
)

copy_fixtures(
  "${TEST_FIXTURES_SOURCE_DIR}/subset/misc"
  "${TEST_FIXTURES_BINARY_DIR}/subset/misc"
  "*.flac"
  "misc files"
)

copy_fixtures(
  "${TEST_FIXTURES_SOURCE_DIR}/faulty"
  "${TEST_FIXTURES_BINARY_DIR}/faulty"
  "*.flac"
  "faulty files"
)

copy_fixtures(
  "${TEST_FIXTURES_SOURCE_DIR}/uncommon"
  "${TEST_FIXTURES_BINARY_DIR}/uncommon"
  "*.flac"
  "uncommon files"
)

add_custom_target(test_fixtures ALL
    COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_fixtures.cmake"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

set(TEST_FIXTURES_DIR "${TEST_FIXTURES_BINARY_DIR}" CACHE STRING "Path set to test fixtures" FORCE)

message(STATUS "Test fixtures configured:")
message(STATUS " Source: ${TEST_FIXTURES_SOURCE_DIR}")
message(STATUS " Binary: ${TEST_FIXTURES_BINARY_DIR}")
message(STATUS " Available at: ${TEST_FIXTURES_DIR}")
