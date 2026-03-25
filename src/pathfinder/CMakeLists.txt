cmake_minimum_required(VERSION 3.20)

add_library(ro_pathfinder STATIC
    PathFinder.h
    PathFinder.cpp
)

target_include_directories(ro_pathfinder PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ro_pathfinder PUBLIC
    ro_core
)
