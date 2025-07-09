include(cmake/SystemLink.cmake)
include(cmake/LibFuzzer.cmake)
include(CMakeDependentOption)
include(CheckCXXCompilerFlag)


include(CheckCXXSourceCompiles)


macro(asfproject_supports_sanitizers)
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

macro(asfproject_setup_options)
  option(asfproject_ENABLE_HARDENING "Enable hardening" ON)
  option(asfproject_ENABLE_COVERAGE "Enable coverage reporting" OFF)
  cmake_dependent_option(
    asfproject_ENABLE_GLOBAL_HARDENING
    "Attempt to push hardening options to built dependencies"
    ON
    asfproject_ENABLE_HARDENING
    OFF)

  asfproject_supports_sanitizers()

  if(NOT PROJECT_IS_TOP_LEVEL OR asfproject_PACKAGING_MAINTAINER_MODE)
    option(asfproject_ENABLE_IPO "Enable IPO/LTO" OFF)
    option(asfproject_WARNINGS_AS_ERRORS "Treat Warnings As Errors" OFF)
    option(asfproject_ENABLE_USER_LINKER "Enable user-selected linker" OFF)
    option(asfproject_ENABLE_SANITIZER_ADDRESS "Enable address sanitizer" OFF)
    option(asfproject_ENABLE_SANITIZER_LEAK "Enable leak sanitizer" OFF)
    option(asfproject_ENABLE_SANITIZER_UNDEFINED "Enable undefined sanitizer" OFF)
    option(asfproject_ENABLE_SANITIZER_THREAD "Enable thread sanitizer" OFF)
    option(asfproject_ENABLE_SANITIZER_MEMORY "Enable memory sanitizer" OFF)
    option(asfproject_ENABLE_UNITY_BUILD "Enable unity builds" OFF)
    option(asfproject_ENABLE_CLANG_TIDY "Enable clang-tidy" OFF)
    option(asfproject_ENABLE_CPPCHECK "Enable cpp-check analysis" OFF)
    option(asfproject_ENABLE_PCH "Enable precompiled headers" OFF)
    option(asfproject_ENABLE_CACHE "Enable ccache" OFF)
  else()
    option(asfproject_ENABLE_IPO "Enable IPO/LTO" ON)
    option(asfproject_WARNINGS_AS_ERRORS "Treat Warnings As Errors" ON)
    option(asfproject_ENABLE_USER_LINKER "Enable user-selected linker" OFF)
    option(asfproject_ENABLE_SANITIZER_ADDRESS "Enable address sanitizer" ${SUPPORTS_ASAN})
    option(asfproject_ENABLE_SANITIZER_LEAK "Enable leak sanitizer" OFF)
    option(asfproject_ENABLE_SANITIZER_UNDEFINED "Enable undefined sanitizer" ${SUPPORTS_UBSAN})
    option(asfproject_ENABLE_SANITIZER_THREAD "Enable thread sanitizer" OFF)
    option(asfproject_ENABLE_SANITIZER_MEMORY "Enable memory sanitizer" OFF)
    option(asfproject_ENABLE_UNITY_BUILD "Enable unity builds" OFF)
    option(asfproject_ENABLE_CLANG_TIDY "Enable clang-tidy" ON)
    option(asfproject_ENABLE_CPPCHECK "Enable cpp-check analysis" ON)
    option(asfproject_ENABLE_PCH "Enable precompiled headers" OFF)
    option(asfproject_ENABLE_CACHE "Enable ccache" ON)
  endif()

  if(NOT PROJECT_IS_TOP_LEVEL)
    mark_as_advanced(
      asfproject_ENABLE_IPO
      asfproject_WARNINGS_AS_ERRORS
      asfproject_ENABLE_USER_LINKER
      asfproject_ENABLE_SANITIZER_ADDRESS
      asfproject_ENABLE_SANITIZER_LEAK
      asfproject_ENABLE_SANITIZER_UNDEFINED
      asfproject_ENABLE_SANITIZER_THREAD
      asfproject_ENABLE_SANITIZER_MEMORY
      asfproject_ENABLE_UNITY_BUILD
      asfproject_ENABLE_CLANG_TIDY
      asfproject_ENABLE_CPPCHECK
      asfproject_ENABLE_COVERAGE
      asfproject_ENABLE_PCH
      asfproject_ENABLE_CACHE)
  endif()

  asfproject_check_libfuzzer_support(LIBFUZZER_SUPPORTED)
  if(LIBFUZZER_SUPPORTED AND (asfproject_ENABLE_SANITIZER_ADDRESS OR asfproject_ENABLE_SANITIZER_THREAD OR asfproject_ENABLE_SANITIZER_UNDEFINED))
    set(DEFAULT_FUZZER ON)
  else()
    set(DEFAULT_FUZZER OFF)
  endif()

  option(asfproject_BUILD_FUZZ_TESTS "Enable fuzz testing executable" ${DEFAULT_FUZZER})

endmacro()

macro(asfproject_global_options)
  if(asfproject_ENABLE_IPO)
    include(cmake/InterproceduralOptimization.cmake)
    asfproject_enable_ipo()
  endif()

  asfproject_supports_sanitizers()

  if(asfproject_ENABLE_HARDENING AND asfproject_ENABLE_GLOBAL_HARDENING)
    include(cmake/Hardening.cmake)
    if(NOT SUPPORTS_UBSAN 
       OR asfproject_ENABLE_SANITIZER_UNDEFINED
       OR asfproject_ENABLE_SANITIZER_ADDRESS
       OR asfproject_ENABLE_SANITIZER_THREAD
       OR asfproject_ENABLE_SANITIZER_LEAK)
      set(ENABLE_UBSAN_MINIMAL_RUNTIME FALSE)
    else()
      set(ENABLE_UBSAN_MINIMAL_RUNTIME TRUE)
    endif()
    message("${asfproject_ENABLE_HARDENING} ${ENABLE_UBSAN_MINIMAL_RUNTIME} ${asfproject_ENABLE_SANITIZER_UNDEFINED}")
    asfproject_enable_hardening(asfproject_options ON ${ENABLE_UBSAN_MINIMAL_RUNTIME})
  endif()
endmacro()

macro(asfproject_local_options)
  if(PROJECT_IS_TOP_LEVEL)
    include(cmake/StandardProjectSettings.cmake)
  endif()

  add_library(asfproject_warnings INTERFACE)
  add_library(asfproject_options INTERFACE)

  include(cmake/CompilerWarnings.cmake)
  asfproject_set_project_warnings(
    asfproject_warnings
    ${asfproject_WARNINGS_AS_ERRORS}
    ""
    ""
    ""
    "")

  if(asfproject_ENABLE_USER_LINKER)
    include(cmake/Linker.cmake)
    asfproject_configure_linker(asfproject_options)
  endif()

  include(cmake/Sanitizers.cmake)
  asfproject_enable_sanitizers(
    asfproject_options
    ${asfproject_ENABLE_SANITIZER_ADDRESS}
    ${asfproject_ENABLE_SANITIZER_LEAK}
    ${asfproject_ENABLE_SANITIZER_UNDEFINED}
    ${asfproject_ENABLE_SANITIZER_THREAD}
    ${asfproject_ENABLE_SANITIZER_MEMORY})

  set_target_properties(asfproject_options PROPERTIES UNITY_BUILD ${asfproject_ENABLE_UNITY_BUILD})

  if(asfproject_ENABLE_PCH)
    target_precompile_headers(
      asfproject_options
      INTERFACE
      <vector>
      <string>
      <utility>)
  endif()

  if(asfproject_ENABLE_CACHE)
    include(cmake/Cache.cmake)
    asfproject_enable_cache()
  endif()

  include(cmake/StaticAnalyzers.cmake)
  if(asfproject_ENABLE_CLANG_TIDY)
    asfproject_enable_clang_tidy(asfproject_options ${asfproject_WARNINGS_AS_ERRORS})
  endif()

  if(asfproject_ENABLE_CPPCHECK)
    asfproject_enable_cppcheck(${asfproject_WARNINGS_AS_ERRORS} "" # override cppcheck options
    )
  endif()

  if(asfproject_ENABLE_COVERAGE)
    include(cmake/Tests.cmake)
    asfproject_enable_coverage(asfproject_options)
  endif()

  if(asfproject_WARNINGS_AS_ERRORS)
    check_cxx_compiler_flag("-Wl,--fatal-warnings" LINKER_FATAL_WARNINGS)
    if(LINKER_FATAL_WARNINGS)
      # This is not working consistently, so disabling for now
      # target_link_options(asfproject_options INTERFACE -Wl,--fatal-warnings)
    endif()
  endif()

  if(asfproject_ENABLE_HARDENING AND NOT asfproject_ENABLE_GLOBAL_HARDENING)
    include(cmake/Hardening.cmake)
    if(NOT SUPPORTS_UBSAN 
       OR asfproject_ENABLE_SANITIZER_UNDEFINED
       OR asfproject_ENABLE_SANITIZER_ADDRESS
       OR asfproject_ENABLE_SANITIZER_THREAD
       OR asfproject_ENABLE_SANITIZER_LEAK)
      set(ENABLE_UBSAN_MINIMAL_RUNTIME FALSE)
    else()
      set(ENABLE_UBSAN_MINIMAL_RUNTIME TRUE)
    endif()
    asfproject_enable_hardening(asfproject_options OFF ${ENABLE_UBSAN_MINIMAL_RUNTIME})
  endif()

endmacro()
