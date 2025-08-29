add_executable(AFS
  main.cpp
  fft.cpp
  audio_engine.cpp
  wave_file.cpp
  signal.cpp
  wave.cpp
  spectrum.cpp
  spectrogram.cpp
  afs.cpp
  db.cpp
)

target_link_libraries(
  AFS
  PRIVATE afsproject::afsproject_options
          afsproject::afsproject_warnings)

target_link_system_libraries(
  AFS
  PRIVATE
          matplot
          fftw3
          NumCpp::NumCpp
          sqlite3
)

target_include_directories(AFS ${WARNING_GUARD} PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
                                                       $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>)

target_compile_features(AFS PUBLIC cxx_std_23)
