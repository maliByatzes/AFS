include(cmake/SystemLink.cmake)
include(cmake/LibFuzzer.cmake)
include(CMakeDependentOption)
include(CheckCXXCompilerFlag)


include(CheckCXXSourceCompiles)


macro(afsproject_supports_sanitizers)
  if((CMAKE_CXX_COMPILER_ID MATCHES ".*Clang.*" OR CMAKE_CXX_COMPILER_ID MATCHES ".*GNU.*") AND NOT WIN32)

    message(STATUS "Sanity checking UndefinedBehaviorSanitizer, it should be supported on this platform")
    set(TEST_PROGRAM "int main() { return 0; }")

    # Check if UndefinedBehaviorSanitizer works at link time
    set(CMAKE_REQUIRED_FLAGS "-fsanitize=undefined")
    set(CMAKE_REQUIRED_LINK_OPTIONS "-fsanitize=undefined")
    check_cxx_source_compiles("${TEST_PROGRAM}" HAS_UBSAN_LINK_SUPPORT)

    if(HAS_UBSAN_LINK_SUPPORT)
      message(STATUS "UndefinedBehaviorSanitizer is supported at both compile and link time.")
      set(SUPPORTS_UBSAN ON)
    else()
      message(WARNING "UndefinedBehaviorSanitizer is NOT supported at link time.")
      set(SUPPORTS_UBSAN OFF)
    endif()
  else()
    set(SUPPORTS_UBSAN OFF)
  endif()

  if((CMAKE_CXX_COMPILER_ID MATCHES ".*Clang.*" OR CMAKE_CXX_COMPILER_ID MATCHES ".*GNU.*") AND WIN32)
    set(SUPPORTS_ASAN OFF)
  else()
    if (NOT WIN32)
      message(STATUS "Sanity checking AddressSanitizer, it should be supported on this platform")
      set(TEST_PROGRAM "int main() { return 0; }")

      # Check if AddressSanitizer works at link time
      set(CMAKE_REQUIRED_FLAGS "-fsanitize=address")
      set(CMAKE_REQUIRED_LINK_OPTIONS "-fsanitize=address")
      check_cxx_source_compiles("${TEST_PROGRAM}" HAS_ASAN_LINK_SUPPORT)

      if(HAS_ASAN_LINK_SUPPORT)
        message(STATUS "AddressSanitizer is supported at both compile and link time.")
        set(SUPPORTS_ASAN ON)
      else()
        message(WARNING "AddressSanitizer is NOT supported at link time.")
        set(SUPPORTS_ASAN OFF)
      endif()
    else()
      set(SUPPORTS_ASAN ON)
    endif()
  endif()
endmacro()

macro(afsproject_setup_options)
  option(afsproject_ENABLE_HARDENING "Enable hardening" ON)
  option(afsproject_ENABLE_COVERAGE "Enable coverage reporting" OFF)
  cmake_dependent_option(
    afsproject_ENABLE_GLOBAL_HARDENING
    "Attempt to push hardening options to built dependencies"
    ON
    afsproject_ENABLE_HARDENING
    OFF)

  afsproject_supports_sanitizers()

  if(NOT PROJECT_IS_TOP_LEVEL OR afsproject_PACKAGING_MAINTAINER_MODE)
    option(afsproject_ENABLE_IPO "Enable IPO/LTO" OFF)
    option(afsproject_WARNINGS_AS_ERRORS "Treat Warnings As Errors" OFF)
    option(afsproject_ENABLE_USER_LINKER "Enable user-selected linker" OFF)
    option(afsproject_ENABLE_SANITIZER_ADDRESS "Enable address sanitizer" OFF)
    option(afsproject_ENABLE_SANITIZER_LEAK "Enable leak sanitizer" OFF)
    option(afsproject_ENABLE_SANITIZER_UNDEFINED "Enable undefined sanitizer" OFF)
    option(afsproject_ENABLE_SANITIZER_THREAD "Enable thread sanitizer" OFF)
    option(afsproject_ENABLE_SANITIZER_MEMORY "Enable memory sanitizer" OFF)
    option(afsproject_ENABLE_UNITY_BUILD "Enable unity builds" OFF)
    option(afsproject_ENABLE_CLANG_TIDY "Enable clang-tidy" OFF)
    option(afsproject_ENABLE_CPPCHECK "Enable cpp-check analysis" OFF)
    option(afsproject_ENABLE_PCH "Enable precompiled headers" OFF)
    option(afsproject_ENABLE_CACHE "Enable ccache" OFF)
  else()
    option(afsproject_ENABLE_IPO "Enable IPO/LTO" ON)
    option(afsproject_WARNINGS_AS_ERRORS "Treat Warnings As Errors" ON)
    option(afsproject_ENABLE_USER_LINKER "Enable user-selected linker" OFF)
    option(afsproject_ENABLE_SANITIZER_ADDRESS "Enable address sanitizer" ${SUPPORTS_ASAN})
    option(afsproject_ENABLE_SANITIZER_LEAK "Enable leak sanitizer" OFF)
    option(afsproject_ENABLE_SANITIZER_UNDEFINED "Enable undefined sanitizer" ${SUPPORTS_UBSAN})
    option(afsproject_ENABLE_SANITIZER_THREAD "Enable thread sanitizer" OFF)
    option(afsproject_ENABLE_SANITIZER_MEMORY "Enable memory sanitizer" OFF)
    option(afsproject_ENABLE_UNITY_BUILD "Enable unity builds" OFF)
    option(afsproject_ENABLE_CLANG_TIDY "Enable clang-tidy" ON)
    option(afsproject_ENABLE_CPPCHECK "Enable cpp-check analysis" ON)
    option(afsproject_ENABLE_PCH "Enable precompiled headers" OFF)
    option(afsproject_ENABLE_CACHE "Enable ccache" ON)
  endif()

  if(NOT PROJECT_IS_TOP_LEVEL)
    mark_as_advanced(
      afsproject_ENABLE_IPO
      afsproject_WARNINGS_AS_ERRORS
      afsproject_ENABLE_USER_LINKER
      afsproject_ENABLE_SANITIZER_ADDRESS
      afsproject_ENABLE_SANITIZER_LEAK
      afsproject_ENABLE_SANITIZER_UNDEFINED
      afsproject_ENABLE_SANITIZER_THREAD
      afsproject_ENABLE_SANITIZER_MEMORY
      afsproject_ENABLE_UNITY_BUILD
      afsproject_ENABLE_CLANG_TIDY
      afsproject_ENABLE_CPPCHECK
      afsproject_ENABLE_COVERAGE
      afsproject_ENABLE_PCH
      afsproject_ENABLE_CACHE)
  endif()

  afsproject_check_libfuzzer_support(LIBFUZZER_SUPPORTED)
  if(LIBFUZZER_SUPPORTED AND (afsproject_ENABLE_SANITIZER_ADDRESS OR afsproject_ENABLE_SANITIZER_THREAD OR afsproject_ENABLE_SANITIZER_UNDEFINED))
    set(DEFAULT_FUZZER ON)
  else()
    set(DEFAULT_FUZZER OFF)
  endif()

  option(afsproject_BUILD_FUZZ_TESTS "Enable fuzz testing executable" ${DEFAULT_FUZZER})

endmacro()

macro(afsproject_global_options)
  if(afsproject_ENABLE_IPO)
    include(cmake/InterproceduralOptimization.cmake)
    afsproject_enable_ipo()
  endif()

  afsproject_supports_sanitizers()

  if(afsproject_ENABLE_HARDENING AND afsproject_ENABLE_GLOBAL_HARDENING)
    include(cmake/Hardening.cmake)
    if(NOT SUPPORTS_UBSAN 
       OR afsproject_ENABLE_SANITIZER_UNDEFINED
       OR afsproject_ENABLE_SANITIZER_ADDRESS
       OR afsproject_ENABLE_SANITIZER_THREAD
       OR afsproject_ENABLE_SANITIZER_LEAK)
      set(ENABLE_UBSAN_MINIMAL_RUNTIME FALSE)
    else()
      set(ENABLE_UBSAN_MINIMAL_RUNTIME TRUE)
    endif()
    message("${afsproject_ENABLE_HARDENING} ${ENABLE_UBSAN_MINIMAL_RUNTIME} ${afsproject_ENABLE_SANITIZER_UNDEFINED}")
    afsproject_enable_hardening(afsproject_options ON ${ENABLE_UBSAN_MINIMAL_RUNTIME})
  endif()
endmacro()

macro(afsproject_local_options)
  if(PROJECT_IS_TOP_LEVEL)
    include(cmake/StandardProjectSettings.cmake)
  endif()

  add_library(afsproject_warnings INTERFACE)
  add_library(afsproject_options INTERFACE)

  include(cmake/CompilerWarnings.cmake)
  afsproject_set_project_warnings(
    afsproject_warnings
    ${afsproject_WARNINGS_AS_ERRORS}
    ""
    ""
    ""
    "")

  if(afsproject_ENABLE_USER_LINKER)
    include(cmake/Linker.cmake)
    afsproject_configure_linker(afsproject_options)
  endif()

  include(cmake/Sanitizers.cmake)
  afsproject_enable_sanitizers(
    afsproject_options
    ${afsproject_ENABLE_SANITIZER_ADDRESS}
    ${afsproject_ENABLE_SANITIZER_LEAK}
    ${afsproject_ENABLE_SANITIZER_UNDEFINED}
    ${afsproject_ENABLE_SANITIZER_THREAD}
    ${afsproject_ENABLE_SANITIZER_MEMORY})

  set_target_properties(afsproject_options PROPERTIES UNITY_BUILD ${afsproject_ENABLE_UNITY_BUILD})

  if(afsproject_ENABLE_PCH)
    target_precompile_headers(
      afsproject_options
      INTERFACE
      <vector>
      <string>
      <utility>)
  endif()

  if(afsproject_ENABLE_CACHE)
    include(cmake/Cache.cmake)
    afsproject_enable_cache()
  endif()

  include(cmake/StaticAnalyzers.cmake)
  if(afsproject_ENABLE_CLANG_TIDY)
    afsproject_enable_clang_tidy(afsproject_options ${afsproject_WARNINGS_AS_ERRORS})
  endif()

  if(afsproject_ENABLE_CPPCHECK)
    afsproject_enable_cppcheck(${afsproject_WARNINGS_AS_ERRORS} "" # override cppcheck options
    )
  endif()

  if(afsproject_ENABLE_COVERAGE)
    include(cmake/Tests.cmake)
    afsproject_enable_coverage(afsproject_options)
  endif()

  if(afsproject_WARNINGS_AS_ERRORS)
    check_cxx_compiler_flag("-Wl,--fatal-warnings" LINKER_FATAL_WARNINGS)
    if(LINKER_FATAL_WARNINGS)
      # This is not working consistently, so disabling for now
      # target_link_options(afsproject_options INTERFACE -Wl,--fatal-warnings)
    endif()
  endif()

  if(afsproject_ENABLE_HARDENING AND NOT afsproject_ENABLE_GLOBAL_HARDENING)
    include(cmake/Hardening.cmake)
    if(NOT SUPPORTS_UBSAN 
       OR afsproject_ENABLE_SANITIZER_UNDEFINED
       OR afsproject_ENABLE_SANITIZER_ADDRESS
       OR afsproject_ENABLE_SANITIZER_THREAD
       OR afsproject_ENABLE_SANITIZER_LEAK)
      set(ENABLE_UBSAN_MINIMAL_RUNTIME FALSE)
    else()
      set(ENABLE_UBSAN_MINIMAL_RUNTIME TRUE)
    endif()
    afsproject_enable_hardening(afsproject_options OFF ${ENABLE_UBSAN_MINIMAL_RUNTIME})
  endif()

endmacro()
