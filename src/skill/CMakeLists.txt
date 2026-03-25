cmake_minimum_required(VERSION 3.20)

add_library(ro_skill STATIC
    Skill.h
    Skill.cpp
)

target_include_directories(ro_skill PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ro_skill PUBLIC
    ro_core
    ro_session
)
