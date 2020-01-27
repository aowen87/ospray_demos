cmake_minimum_required(VERSION 3.7)

project(volume)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(embree 3.2.0 REQUIRED
             PATHS
             #PATH TO EMBREE
            )

find_package(ospray 1.6.1 REQUIRED
             PATHS
             #PATH TO OSPRAY
            )

add_executable(s_volume structuredVolume.cpp) 

target_link_libraries(s_volume ospray::ospray)

