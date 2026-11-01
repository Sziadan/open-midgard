cmake_minimum_required(VERSION 3.20)

set(RO_MAIN_SOURCE WinMain.cpp)
if(NOT WIN32)
    set(RO_MAIN_SOURCE AppMain_stub.cpp)
else()
    list(APPEND RO_MAIN_SOURCE HealRenderPreview.cpp)
endif()

add_library(ro_main STATIC
    WinMain.h
    ${RO_MAIN_SOURCE}
)

target_include_directories(ro_main PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ro_main PUBLIC
    ro_gamemode
    ro_session
    ro_network
    ro_core
    ro_cipher
)
