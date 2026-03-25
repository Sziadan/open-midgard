cmake_minimum_required(VERSION 3.20)

add_library(ro_session STATIC
    Session.h
    Session.cpp
)

target_include_directories(ro_session PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ro_session PUBLIC
    ro_core
    ro_network
)
