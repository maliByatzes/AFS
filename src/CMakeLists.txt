add_executable(ASF main.cpp fft.cpp audio_engine.cpp wave_file.cpp signal.cpp wave.cpp)

target_link_libraries(
  ASF
  PRIVATE asfproject::asfproject_options
          asfproject::asfproject_warnings)

target_link_system_libraries(
  ASF
  PRIVATE
          matplot
          fftw3
          NumCpp::NumCpp
)

target_include_directories(ASF ${WARNING_GUARD} PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
                                                       $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>)

target_compile_features(ASF PUBLIC cxx_std_23)
