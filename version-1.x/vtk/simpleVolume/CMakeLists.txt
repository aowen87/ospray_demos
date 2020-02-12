cmake_minimum_required(VERSION 3.7)

project(ospVol)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(VTK COMPONENTS 
  vtkCommonCore
  vtkCommonDataModel
  vtkIOXML
  vtkImagingCore
  vtkImagingHybrid
  vtkInteractionStyle
  vtkRenderingCore
  vtkRenderingFreeType
  vtkRenderingOSPRay
  vtkRenderingOpenGL2 QUIET)
if (NOT VTK_FOUND)
  message("Skipping ospVol: ${VTK_NOT_FOUND_MESSAGE}")
  return ()
endif()
message (STATUS "VTK_VERSION: ${VTK_VERSION}")
if (VTK_VERSION VERSION_LESS "8.90.0")
  # old system
  include(${VTK_USE_FILE})
  add_executable(ospVol MACOSX_BUNDLE ospVol.cpp )
  target_link_libraries(ospVol PRIVATE ${VTK_LIBRARIES})
else ()
  # include all components
  add_executable(ospVol MACOSX_BUNDLE ospVol.cpp )
  target_link_libraries(ospVol PRIVATE ${VTK_LIBRARIES}) 
  # vtk_module_autoinit is needed
  vtk_module_autoinit(
    TARGETS ospVol
    )
endif()

