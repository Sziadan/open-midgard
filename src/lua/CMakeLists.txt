cmake_minimum_required(VERSION 3.20)

add_library(ro_lua STATIC
    LuaBridge.h
    LuaBridge.cpp
)

target_include_directories(ro_lua PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ro_lua PUBLIC
    ro_core
)
