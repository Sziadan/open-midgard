cmake_minimum_required(VERSION 3.20)

add_library(ro_network STATIC
    GronPacket.h
    GronPacket.cpp
    PacketQueue.h
    PacketQueue.cpp
    Connection.h
    Connection.cpp
)

target_include_directories(ro_network PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ro_network PUBLIC
    ro_core
)
