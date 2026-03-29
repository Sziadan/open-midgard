cmake_minimum_required(VERSION 3.20)

add_library(ro_audio STATIC
    Audio.h
    AudioBackend.h
    Audio.cpp
    MilesAudioBackend.cpp
    MiniaudioBackend.cpp
    Video.h
    Video.cpp
)

target_include_directories(ro_audio PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)
target_link_libraries(ro_audio PUBLIC
    ro_core
)
