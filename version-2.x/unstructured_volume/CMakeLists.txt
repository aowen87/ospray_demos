cmake_minimum_required(VERSION 3.7)

project(us_volume)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# May need to set these
set(ospcommon_DIR path/goes/here)
set(openvkl_DIR path/goes/here)
set(ispc_DIR path/goes/here)
set(embree_DIR path/goes/here)
set(OSPCOMMON_TBB_ROOT path/goes/here)

find_package(embree 3.2.0 REQUIRED
             PATHS
             #PATH TO EMBREE
            )

find_package(ospray 1.6.1 REQUIRED
             PATHS
             #PATH TO OSPRAY
            )

add_executable(us_volume unstructuredVolume.cpp) 

target_link_libraries(us_volume ospray::ospray)

