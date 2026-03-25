cmake_minimum_required(VERSION 3.20)

add_library(ro_render STATIC
    Renderer.h
    Renderer.cpp
    DrawUtil.h
    DrawUtil.cpp
    DC.h
    DC.cpp
    Prim.h
    Prim.cpp
)

target_include_directories(ro_render PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ro_render PUBLIC
    ro_core
    ro_res
    ro_render3d
)
