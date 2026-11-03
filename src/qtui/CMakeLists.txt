set(RO_QTUI_SOURCES
    QtUiRuntime.h
    QtUiSpikeBridge.h
    QtUiSpikeBridge.cpp
    QtChatInput.cpp
    QtUiState.h
    QtUiSpikeState.h
    QtUiStateAdapter.h
    QtUiStateAdapter.cpp
    QtUiSpikeAssets.qrc
    QtPlatformWindow.h
    QtPlatformWindow.cpp
)

add_library(ro_qtui STATIC ${RO_QTUI_SOURCES})
if(WIN32)
    target_link_libraries(ro_qtui PUBLIC imm32)
endif()

set_target_properties(ro_qtui PROPERTIES
    AUTOMOC ON
    AUTORCC ON
    AUTOUIC OFF
)

target_include_directories(ro_qtui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(ro_qtui PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    ro_core
    ro_gamemode
    ro_session
    ro_ui
    ro_world
)

if(WIN32 AND RO_HAS_NATIVE_D3D12)
    target_link_libraries(ro_qtui PUBLIC
        Qt6::GuiPrivate
    )
endif()
