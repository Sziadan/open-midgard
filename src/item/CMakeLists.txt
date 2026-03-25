cmake_minimum_required(VERSION 3.20)

add_library(ro_item STATIC
    Item.h
    Item.cpp
)

target_include_directories(ro_item PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(ro_item PRIVATE RO_SOURCE_ROOT="${CMAKE_SOURCE_DIR}")
target_link_libraries(ro_item PUBLIC
    ro_core
)
