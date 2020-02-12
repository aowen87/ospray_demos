cmake_minimum_required(VERSION 3.7)

project(ospCube)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(VTK COMPONENTS 
  vtkCommonColor
  vtkCommonCore
  vtkCommonDataModel
  vtkInteractionStyle
  vtkRenderingCore
  vtkRenderingOSPRay
  vtkRenderingFreeType
  vtkRenderingOpenGL2 QUIET)
if (NOT VTK_FOUND)
  message("Skipping ospCube: ${VTK_NOT_FOUND_MESSAGE}")
  return ()
endif()
message (STATUS "VTK_VERSION: ${VTK_VERSION}")
if (VTK_VERSION VERSION_LESS "8.90.0")
  # old system
  include(${VTK_USE_FILE})
  add_executable(ospCube MACOSX_BUNDLE ospCube.cpp )
  target_link_libraries(ospCube PRIVATE ${VTK_LIBRARIES})
else ()
  # include all components
  add_executable(ospCube MACOSX_BUNDLE ospCube.cpp )
  target_link_libraries(ospCube PRIVATE ${VTK_LIBRARIES}) 
  # vtk_module_autoinit is needed
  vtk_module_autoinit(
    TARGETS ospCube
    )
endif()

