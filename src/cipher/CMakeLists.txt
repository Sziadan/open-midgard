cmake_minimum_required(VERSION 3.20)

add_library(ro_cipher STATIC
    CDec.h
    CDec.cpp
)

target_include_directories(ro_cipher PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ro_cipher PRIVATE ro_core)
