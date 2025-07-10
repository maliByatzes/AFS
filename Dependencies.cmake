include(cmake/CPM.cmake)

function(asfproject_setup_dependencies)

  if(NOT TARGET matplot::matplot)
    cpmaddpackage("gh:alandefreitas/matplotplusplus@1.2.2")
  endif()

endfunction()
