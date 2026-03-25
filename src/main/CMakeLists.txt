cmake_minimum_required(VERSION 3.20)

add_library(ro_main STATIC
    WinMain.h
    WinMain.cpp
)

target_include_directories(ro_main PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ro_main PUBLIC
    ro_gamemode
    ro_session
    ro_network
    ro_core
    ro_cipher
)
