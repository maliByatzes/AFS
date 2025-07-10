add_executable(ASF main.cpp process_audio.cpp)

target_link_libraries(
  ASF
  PRIVATE asfproject::asfproject_options
          asfproject::asfproject_warnings)

target_link_system_libraries(
  ASF
  PRIVATE
          matplot
)

target_include_directories(ASF ${WARNING_GUARD} PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
                                                       $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>)

target_compile_features(ASF PUBLIC cxx_std_23)
