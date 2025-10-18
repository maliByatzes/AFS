add_library(afsproject_lib 
  fft.cpp
  audio_engine.cpp
  wave_file.cpp
  flac_file.cpp
  signal.cpp
  wave.cpp
  spectrum.cpp
  spectrogram.cpp
  afs.cpp
  db.cpp
)

target_link_libraries(afsproject_lib
  PRIVATE afsproject::afsproject_options
          afsproject::afsproject_warnings
          etl
)

target_link_system_libraries(
  afsproject_lib
  PRIVATE
          matplot
          fftw3
          NumCpp::NumCpp
          sqlite3
)

target_include_directories(afsproject_lib 
  ${WARNING_GUARD} PUBLIC 
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
)

target_compile_features(afsproject_lib PUBLIC cxx_std_23)

add_library(afsproject::afsproject_lib ALIAS afsproject_lib)

add_executable(AFS
  main.cpp
)

target_link_libraries(AFS
  PRIVATE afsproject::afsproject_lib
)
