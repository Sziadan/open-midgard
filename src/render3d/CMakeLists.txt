cmake_minimum_required(VERSION 3.20)

add_library(ro_render3d STATIC
    D3dutil.h
    D3dutil.cpp
    Device.h
    Device.cpp
)

target_include_directories(ro_render3d PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ro_render3d PUBLIC
    ro_render
    ro_core
)
