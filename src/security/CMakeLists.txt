cmake_minimum_required(VERSION 3.20)

add_library(ro_security STATIC
    Security.h
    Security.cpp
)

target_include_directories(ro_security PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ro_security PUBLIC
    ro_cipher
    ro_core
)
