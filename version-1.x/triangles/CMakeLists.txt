cmake_minimum_required(VERSION 3.7)

project(triangles)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


find_package(ospray 1.8.5 REQUIRED
             PATHS
             #Path to ospray
            )

add_executable(triangles triangles.cpp) 

target_link_libraries(triangles ospray::ospray)

