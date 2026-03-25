cmake_minimum_required(VERSION 3.20)

add_library(ro_input STATIC
    Input.h
    Input.cpp
)

target_include_directories(ro_input PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ro_input PUBLIC
    ro_core
)
