add_library(ro_gamemode STATIC
    Mode.h
    Mode.cpp
    CursorRenderer.h
    CursorRenderer.cpp
    LoginMode.h
    LoginMode.cpp
    GameMode.h
    GameMode.cpp
    View.h
    View.cpp
    GameModePacket.h
    GameModePacket.cpp
)

target_include_directories(ro_gamemode PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(ro_gamemode PUBLIC
    ro_core
    ro_network
    ro_world
    ro_audio
    ro_ui
)
