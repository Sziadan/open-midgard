cmake_minimum_required(VERSION 3.20)
project(OpenMidgard VERSION 2008.0 LANGUAGES C CXX)

if(NOT WIN32)
    include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/win32)
endif()

# ─── Toolchain requirements ────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(RO_TARGET_ARCH "x64")
else()
    set(RO_TARGET_ARCH "Win32")
endif()

option(RO_ENABLE_MILES_AUDIO "Enable Miles Sound System runtime loading" ON)
option(RO_ENABLE_BINK_VIDEO "Enable Bink video runtime loading" ON)
set(RO_ENABLE_GRANNY OFF CACHE BOOL "Legacy Granny SDK loading is retired; the client uses its independent GR2 reader" FORCE)
option(RO_ENABLE_IJL "Enable Intel JPEG Library runtime loading" OFF)
# Off by default: only the documented preset flow (`-DRO_ENABLE_DEV_DEPLOY=ON`)
# may overwrite the playable exe in OldRO. Ad-hoc build directories (Win32,
# replay, test probes) must never deploy implicitly.
option(RO_ENABLE_DEV_DEPLOY "Copy build output to the local OldRO folder" OFF)
option(RO_BUILD_SKILL_REPLAY_TESTS "Build production-linked skill packet replay tests" OFF)
option(RO_BUILD_PACKET_REPLAY_TESTS "Build offline production packet stream replay tests" OFF)
option(RO_ENABLE_QT6_UI "Enable the Qt 6 / QML GPU UI runtime" OFF)
option(RO_ENABLE_QT6_UI_SPIKE "Deprecated alias for RO_ENABLE_QT6_UI" OFF)
option(RO_QT_UI_DEFAULT_ENABLED "Enable the Qt UI runtime by default unless OPEN_MIDGARD_QT_UI overrides it" OFF)
option(RO_QT_UI_SPIKE_DEFAULT_ENABLED "Deprecated alias for RO_QT_UI_DEFAULT_ENABLED" OFF)
option(RO_ENABLE_NATIVE_D3D11 "Enable the native Direct3D 11 backend when supported" ON)
option(RO_ENABLE_NATIVE_D3D12 "Enable the native Direct3D 12 backend when supported" ON)
option(RO_LOG_VULKAN_STATS "Log vulkan performance stats" OFF)
set(RO_QT_ROOT "" CACHE PATH "Compiler-compatible Qt 6 kit root (for example C:/Qt/6.11.0/msvc2022_64)")
set(RO_QT_WINDEPLOYQT_EXECUTABLE "" CACHE FILEPATH "Path to the matching windeployqt.exe")

if(NOT WIN32)
    set(RO_ENABLE_NATIVE_D3D11 OFF CACHE BOOL "Enable the native Direct3D 11 backend when supported" FORCE)
    set(RO_ENABLE_NATIVE_D3D12 OFF CACHE BOOL "Enable the native Direct3D 12 backend when supported" FORCE)
    set(RO_ENABLE_QT6_UI ON CACHE BOOL "Enable the Qt 6 / QML GPU UI runtime" FORCE)
    set(RO_ENABLE_DEV_DEPLOY OFF CACHE BOOL "Copy build output to the local OldRO folder" FORCE)
endif()

if(RO_ENABLE_QT6_UI_SPIKE AND NOT RO_ENABLE_QT6_UI)
    set(RO_ENABLE_QT6_UI ON CACHE BOOL "Enable the Qt 6 / QML GPU UI runtime" FORCE)
endif()
if(RO_ENABLE_QT6_UI AND NOT RO_ENABLE_QT6_UI_SPIKE)
    set(RO_ENABLE_QT6_UI_SPIKE ON CACHE BOOL "Deprecated alias for RO_ENABLE_QT6_UI" FORCE)
endif()
if(RO_QT_UI_SPIKE_DEFAULT_ENABLED AND NOT RO_QT_UI_DEFAULT_ENABLED)
    set(RO_QT_UI_DEFAULT_ENABLED ON CACHE BOOL "Enable the Qt UI runtime by default unless OPEN_MIDGARD_QT_UI overrides it" FORCE)
endif()
if(RO_QT_UI_DEFAULT_ENABLED AND NOT RO_QT_UI_SPIKE_DEFAULT_ENABLED)
    set(RO_QT_UI_SPIKE_DEFAULT_ENABLED ON CACHE BOOL "Deprecated alias for RO_QT_UI_DEFAULT_ENABLED" FORCE)
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(RO_X64_FIRST_MILESTONE_BUILD ON)
    set(RO_ENABLE_MILES_AUDIO OFF CACHE BOOL "Enable Miles Sound System runtime loading" FORCE)
    set(RO_ENABLE_BINK_VIDEO OFF CACHE BOOL "Enable Bink video runtime loading" FORCE)
    set(RO_ENABLE_GRANNY OFF CACHE BOOL "Enable Granny runtime loading" FORCE)
    set(RO_ENABLE_IJL OFF CACHE BOOL "Enable Intel JPEG Library runtime loading" FORCE)
else()
    set(RO_X64_FIRST_MILESTONE_BUILD OFF)
endif()

message(STATUS "Target architecture : ${RO_TARGET_ARCH}")

if(WIN32)
    add_definitions(-DWIN32 -D_WINDOWS -DUNICODE -D_UNICODE -DNOMINMAX)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE)
endif()

set(RO_PLATFORM_WINDOWS 0)
set(RO_PLATFORM_MACOS 0)
set(RO_PLATFORM_LINUX 0)
set(RO_VULKAN_USE_WIN32_SURFACE 0)

if(WIN32)
    set(RO_PLATFORM_WINDOWS 1)
    set(RO_VULKAN_USE_WIN32_SURFACE 1)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(RO_PLATFORM_LINUX 1)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(RO_PLATFORM_MACOS 1)
endif()

add_compile_definitions(
    RO_PLATFORM_WINDOWS=${RO_PLATFORM_WINDOWS}
    RO_PLATFORM_MACOS=${RO_PLATFORM_MACOS}
    RO_PLATFORM_LINUX=${RO_PLATFORM_LINUX}
    RO_VULKAN_USE_WIN32_SURFACE=${RO_VULKAN_USE_WIN32_SURFACE}
    RO_ENABLE_MILES_AUDIO=$<BOOL:${RO_ENABLE_MILES_AUDIO}>
    RO_ENABLE_BINK_VIDEO=$<BOOL:${RO_ENABLE_BINK_VIDEO}>
    RO_ENABLE_GRANNY=$<BOOL:${RO_ENABLE_GRANNY}>
    RO_ENABLE_IJL=$<BOOL:${RO_ENABLE_IJL}>
    RO_X64_FIRST_MILESTONE_BUILD=$<BOOL:${RO_X64_FIRST_MILESTONE_BUILD}>
    RO_ENABLE_QT6_UI=$<BOOL:${RO_ENABLE_QT6_UI}>
    RO_ENABLE_QT6_UI_SPIKE=$<BOOL:${RO_ENABLE_QT6_UI_SPIKE}>
    RO_QT_UI_DEFAULT_ENABLED=$<BOOL:${RO_QT_UI_DEFAULT_ENABLED}>
    RO_QT_UI_SPIKE_DEFAULT_ENABLED=$<BOOL:${RO_QT_UI_SPIKE_DEFAULT_ENABLED}>
    RO_LOG_VULKAN_STATS=$<BOOL:${RO_LOG_VULKAN_STATS}>
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(QtSetup)
ro_configure_qt6()
if(RO_ENABLE_QT6_UI)
    if(WIN32 AND RO_HAS_NATIVE_D3D12)
        find_package(Qt6 REQUIRED COMPONENTS Core Gui GuiPrivate Qml Quick)
    else()
        find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick)
    endif()
    # UI headers include Qt types. Those headers are reachable from libraries
    # that do not themselves link Qt, so the kit include paths are global.
    include_directories(
        $<TARGET_PROPERTY:Qt6::Core,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:Qt6::Gui,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:Qt6::Qml,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:Qt6::Quick,INTERFACE_INCLUDE_DIRECTORIES>
    )
endif()

# ─── Compiler options ─────────────────────────────────────────────────────
if(MSVC)
    add_compile_options(/W3 /MP /Zc:__cplusplus)
    # Disable warnings that are unavoidable in a decompiler-origin codebase
    add_compile_options(/wd4100 /wd4189 /wd4996)
endif()

if(WIN32 AND MSVC AND RO_ENABLE_NATIVE_D3D11)
    set(RO_HAS_NATIVE_D3D11 1)
else()
    set(RO_HAS_NATIVE_D3D11 0)
endif()

if(WIN32 AND MSVC AND RO_ENABLE_NATIVE_D3D12)
    set(RO_HAS_NATIVE_D3D12 1)
else()
    set(RO_HAS_NATIVE_D3D12 0)
endif()

add_compile_definitions(
    RO_HAS_NATIVE_D3D11=${RO_HAS_NATIVE_D3D11}
    RO_HAS_NATIVE_D3D12=${RO_HAS_NATIVE_D3D12}
)

# ─── Find DirectX 7 SDK ───────────────────────────────────────────────────
# The original client uses DirectX 7 (IDirect3DDevice7 / IDirectDraw4).
# Set DX7_SDK_DIR to your DXSDK path, or it falls back to the Windows SDK.
if(DEFINED ENV{DX7_SDK_DIR})
    set(DX7_INCLUDE "$ENV{DX7_SDK_DIR}/include")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(DX7_LIBDIR "$ENV{DX7_SDK_DIR}/lib/x64")
    else()
        set(DX7_LIBDIR "$ENV{DX7_SDK_DIR}/lib/x86")
    endif()
    message(STATUS "DirectX 7 SDK : $ENV{DX7_SDK_DIR}")
else()
    message(STATUS "DX7_SDK_DIR not set – falling back to system headers")
endif()

# ─── Source subdirectories ────────────────────────────────────────────────
add_subdirectory(src/core)
add_subdirectory(src/cipher)
add_subdirectory(src/network)
add_subdirectory(src/res)
add_subdirectory(src/audio)
add_subdirectory(src/input)
add_subdirectory(src/session)
add_subdirectory(src/gamemode)
add_subdirectory(src/world)
add_subdirectory(src/render)
add_subdirectory(src/render3d)
add_subdirectory(src/ui)
if(RO_ENABLE_QT6_UI)
    add_subdirectory(src/qtui)
endif()
add_subdirectory(src/skill)
add_subdirectory(src/item)
add_subdirectory(src/pathfinder)
add_subdirectory(src/lua)
add_subdirectory(src/security)
add_subdirectory(src/main)

# ─── Final executable ─────────────────────────────────────────────────────
set(RO_APP_MAIN_SOURCE src/main/WinMain.cpp)
if(WIN32)
    add_executable(open_midgard WIN32 ${RO_APP_MAIN_SOURCE} src/main/OpenMidgard.rc)
else()
    set(RO_APP_MAIN_SOURCE src/main/AppMain_stub.cpp)
    add_executable(open_midgard ${RO_APP_MAIN_SOURCE})
endif()
set_target_properties(open_midgard PROPERTIES OUTPUT_NAME "open-midgard")

if(RO_ENABLE_DEV_DEPLOY)
    target_compile_definitions(open_midgard PRIVATE RO_DEV_DEPLOY_DIR="D:/Spel/OldRO")
endif()


set(RO_CLIENT_RUNTIME_LIBRARIES
    ro_main
    ro_gamemode
    ro_session
    ro_world
    ro_render3d
    ro_render
    ro_ui
    ro_skill
    ro_item
    ro_pathfinder
    ro_network
    ro_res
    ro_audio
    ro_input
    ro_cipher
    ro_core
    ro_security
    ro_lua
    lua51
)
target_link_libraries(open_midgard PRIVATE ${RO_CLIENT_RUNTIME_LIBRARIES})

if(WIN32)
    target_link_libraries(open_midgard PRIVATE
        winmm.lib
        dxguid.lib
        ole32.lib
        oleaut32.lib
        psapi.lib
    )

    if(MINGW)
        target_link_libraries(open_midgard PRIVATE msimg32)
    else()
        target_link_libraries(open_midgard PRIVATE msimg32.lib)
    endif()
endif()

if(RO_ENABLE_QT6_UI)
    target_link_libraries(open_midgard PRIVATE ro_qtui)
endif()

if(MSVC)
    # ddraw.h can auto-link ddraw.lib via pragma; we resolve DirectDraw dynamically.
    target_link_options(open_midgard PRIVATE /NODEFAULTLIB:ddraw.lib)
endif()

if(DEFINED DX7_INCLUDE)
    target_include_directories(open_midgard PRIVATE ${DX7_INCLUDE})
    target_link_directories(open_midgard PRIVATE ${DX7_LIBDIR})
endif()

ro_add_qt_runtime_deploy(open_midgard "$<TARGET_FILE_DIR:open_midgard>")

if(RO_ENABLE_DEV_DEPLOY)
    add_custom_command(TARGET open_midgard POST_BUILD
        COMMAND ${CMAKE_COMMAND}
                -DRO_SRC="$<TARGET_FILE:open_midgard>"
                -DRO_DST="D:/Spel/OldRO/open-midgard.exe"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/BestEffortCopy.cmake"
        COMMENT "Copying executable to local OldRO folder"
    )
    ro_add_qt_runtime_deploy(open_midgard "D:/Spel/OldRO")
endif()

if(RO_BUILD_PACKET_REPLAY_TESTS)
    target_compile_definitions(ro_gamemode PRIVATE RO_PACKET_REPLAY_TESTS=1)
    enable_testing()
    find_package(Threads REQUIRED)
    find_package(Git QUIET)

    set(RO_PACKET_REPLAY_SOURCE_COMMIT_AT_CONFIGURE "unknown")
    set(RO_PACKET_REPLAY_SOURCE_TREE_STATUS_AT_CONFIGURE "unknown")
    if(GIT_FOUND)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" rev-parse --verify HEAD
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            OUTPUT_VARIABLE RO_PACKET_REPLAY_GIT_COMMIT
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE RO_PACKET_REPLAY_GIT_COMMIT_RESULT)
        if(RO_PACKET_REPLAY_GIT_COMMIT_RESULT EQUAL 0 AND RO_PACKET_REPLAY_GIT_COMMIT)
            set(RO_PACKET_REPLAY_SOURCE_COMMIT_AT_CONFIGURE "${RO_PACKET_REPLAY_GIT_COMMIT}")
            execute_process(
                COMMAND "${GIT_EXECUTABLE}" status --porcelain --untracked-files=normal
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                OUTPUT_VARIABLE RO_PACKET_REPLAY_GIT_STATUS
                OUTPUT_STRIP_TRAILING_WHITESPACE
                RESULT_VARIABLE RO_PACKET_REPLAY_GIT_STATUS_RESULT)
            if(RO_PACKET_REPLAY_GIT_STATUS_RESULT EQUAL 0)
                if(RO_PACKET_REPLAY_GIT_STATUS STREQUAL "")
                    set(RO_PACKET_REPLAY_SOURCE_TREE_STATUS_AT_CONFIGURE "clean")
                else()
                    set(RO_PACKET_REPLAY_SOURCE_TREE_STATUS_AT_CONFIGURE "dirty")
                endif()
            endif()
        endif()
    endif()

    add_subdirectory(tools/tests/recovery_delta)

    add_executable(packet_stream_replay_checks
        tools/tests/replay/PacketStreamReplay.cpp)
    target_compile_features(packet_stream_replay_checks PRIVATE cxx_std_17)
    target_compile_definitions(packet_stream_replay_checks PRIVATE
        RO_PACKET_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
        RO_PACKET_REPLAY_SOURCE_COMMIT_AT_CONFIGURE="${RO_PACKET_REPLAY_SOURCE_COMMIT_AT_CONFIGURE}"
        RO_PACKET_REPLAY_SOURCE_TREE_STATUS_AT_CONFIGURE="${RO_PACKET_REPLAY_SOURCE_TREE_STATUS_AT_CONFIGURE}")
    set(RO_PACKET_REPLAY_RUNTIME_LIBRARIES ${RO_CLIENT_RUNTIME_LIBRARIES})
    list(REMOVE_ITEM RO_PACKET_REPLAY_RUNTIME_LIBRARIES ro_main)
    list(APPEND RO_PACKET_REPLAY_RUNTIME_LIBRARIES ro_main)
    target_link_libraries(packet_stream_replay_checks PRIVATE
        ${RO_PACKET_REPLAY_RUNTIME_LIBRARIES}
        Threads::Threads)
    if(RO_ENABLE_QT6_UI)
        target_link_libraries(packet_stream_replay_checks PRIVATE ro_qtui)
    endif()
    if(WIN32)
        target_link_libraries(packet_stream_replay_checks PRIVATE
            ws2_32 winmm.lib dxguid.lib ole32.lib oleaut32.lib psapi.lib)
        if(MINGW)
            target_link_libraries(packet_stream_replay_checks PRIVATE msimg32)
        else()
            target_link_libraries(packet_stream_replay_checks PRIVATE msimg32.lib)
        endif()
    endif()
    add_test(NAME packet_stream_replay_checks COMMAND packet_stream_replay_checks)
    add_test(NAME packet_corpse_lifecycle_checks COMMAND packet_stream_replay_checks --corpse-only)
    add_test(NAME packet_npc_class_change_checks COMMAND packet_stream_replay_checks --npc-class-change-only)
    add_test(NAME packet_compass_checks COMMAND packet_stream_replay_checks --compass-only)
    set_tests_properties(packet_compass_checks PROPERTIES TIMEOUT 30
        ENVIRONMENT "OPEN_MIDGARD_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/packet-compass-settings")
    add_test(NAME packet_chat_state_checks COMMAND packet_stream_replay_checks --chat-state-only)
    add_test(NAME packet_chat_notice_checks COMMAND packet_stream_replay_checks --chat-notice-only)
    set_tests_properties(packet_chat_notice_checks PROPERTIES TIMEOUT 30
        ENVIRONMENT "OPEN_MIDGARD_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/packet-chat-notice-settings")
    set_tests_properties(packet_chat_state_checks PROPERTIES
        TIMEOUT 30
        ENVIRONMENT "OPEN_MIDGARD_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/packet-chat-state-settings")
    add_executable(chat_window_state_checks tools/tests/ChatWindowStateTests.cpp)
    target_compile_features(chat_window_state_checks PRIVATE cxx_std_17)
    add_test(NAME chat_window_state_checks COMMAND chat_window_state_checks)
    add_executable(chat_notice_checks tools/tests/ChatNoticeTests.cpp)
    target_compile_features(chat_notice_checks PRIVATE cxx_std_17)
    add_test(NAME chat_notice_checks COMMAND chat_notice_checks)
    add_executable(chat_history_view_checks tools/tests/ChatHistoryViewTests.cpp)
    target_compile_features(chat_history_view_checks PRIVATE cxx_std_17)
    add_test(NAME chat_history_view_checks COMMAND chat_history_view_checks)
    add_executable(chat_history_wrap_checks tools/tests/ChatHistoryWrapTests.cpp)
    target_compile_features(chat_history_wrap_checks PRIVATE cxx_std_17)
    add_test(NAME chat_history_wrap_checks COMMAND chat_history_wrap_checks)
    add_executable(chat_battle_options_checks tools/tests/ChatBattleOptionsTests.cpp)
    target_compile_features(chat_battle_options_checks PRIVATE cxx_std_17)
    add_test(NAME chat_battle_options_checks COMMAND chat_battle_options_checks)
    add_test(NAME packet_chat_room_checks COMMAND packet_stream_replay_checks --chat-room-only)
    set_tests_properties(packet_chat_room_checks PROPERTIES TIMEOUT 30
        ENVIRONMENT "OPEN_MIDGARD_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/packet-chat-room-settings")
    add_executable(whisper_names_checks tools/tests/WhisperNamesTests.cpp)
    target_compile_features(whisper_names_checks PRIVATE cxx_std_17)
    target_include_directories(whisper_names_checks PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(NAME whisper_names_checks COMMAND whisper_names_checks)
    add_executable(source_chat_command_checks tools/tests/SourceChatCommandTests.cpp)
    target_compile_features(source_chat_command_checks PRIVATE cxx_std_17)
    target_include_directories(source_chat_command_checks PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(NAME source_chat_command_checks COMMAND source_chat_command_checks)
    add_test(NAME packet_auto_message_checks COMMAND packet_stream_replay_checks --auto-message-only)
    add_executable(source_chat_routing_checks tools/tests/SourceChatRoutingTests.cpp)
    target_compile_features(source_chat_routing_checks PRIVATE cxx_std_17)
    target_include_directories(source_chat_routing_checks PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(NAME source_chat_routing_checks COMMAND source_chat_routing_checks)
    add_test(NAME packet_chat_submission_checks COMMAND packet_stream_replay_checks --chat-submission-only)
    add_test(NAME packet_pet_lifecycle_checks COMMAND packet_stream_replay_checks --pet-lifecycle-only)
    set_tests_properties(packet_pet_lifecycle_checks PROPERTIES TIMEOUT 30
        ENVIRONMENT "OPEN_MIDGARD_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/packet-pet-lifecycle-settings")
    set_tests_properties(packet_chat_submission_checks PROPERTIES TIMEOUT 30
        ENVIRONMENT "OPEN_MIDGARD_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/packet-chat-submission-settings")
    add_test(NAME packet_friend_options_checks COMMAND packet_stream_replay_checks --friend-options-only)
    set_tests_properties(packet_friend_options_checks PROPERTIES TIMEOUT 30
        ENVIRONMENT "OPEN_MIDGARD_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/packet-friend-options-settings")
    add_test(NAME packet_world_entry_handoff_checks COMMAND packet_stream_replay_checks --world-entry-handoff-only)
    set_tests_properties(packet_world_entry_handoff_checks PROPERTIES TIMEOUT 30
        ENVIRONMENT "OPEN_MIDGARD_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/packet-world-entry-handoff-settings")
    set_tests_properties(packet_auto_message_checks PROPERTIES TIMEOUT 30
        ENVIRONMENT "OPEN_MIDGARD_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/packet-auto-message-settings")
    add_test(NAME packet_whisper_checks COMMAND packet_stream_replay_checks --whisper-only)
    set_tests_properties(packet_whisper_checks PROPERTIES TIMEOUT 30
        ENVIRONMENT "OPEN_MIDGARD_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/packet-whisper-settings")
    add_test(NAME packet_whisper_preferences_write_checks COMMAND packet_stream_replay_checks --whisper-preferences-write)
    add_test(NAME packet_whisper_preferences_read_checks COMMAND packet_stream_replay_checks --whisper-preferences-read)
    set_tests_properties(packet_whisper_preferences_write_checks PROPERTIES FIXTURES_SETUP source_whisper_preferences
        ENVIRONMENT "OPEN_MIDGARD_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/packet-whisper-preference-settings")
    set_tests_properties(packet_whisper_preferences_read_checks PROPERTIES FIXTURES_REQUIRED source_whisper_preferences
        ENVIRONMENT "OPEN_MIDGARD_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/packet-whisper-preference-settings")
    add_test(NAME packet_whisper_preferences_default_checks COMMAND packet_stream_replay_checks --whisper-preferences-defaults)
    set_tests_properties(packet_whisper_preferences_default_checks PROPERTIES
        ENVIRONMENT "OPEN_MIDGARD_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/packet-whisper-default-settings")
    add_executable(insult_words_checks tools/tests/InsultWordsTests.cpp)
    target_compile_features(insult_words_checks PRIVATE cxx_std_17)
    target_include_directories(insult_words_checks PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(NAME insult_words_checks COMMAND insult_words_checks)
    add_executable(chat_room_packet_checks tools/tests/ChatRoomPacketTests.cpp)
    target_compile_features(chat_room_packet_checks PRIVATE cxx_std_17)
    add_test(NAME chat_room_packet_checks COMMAND chat_room_packet_checks)
    add_executable(chat_scrollbar_checks tools/tests/ChatScrollBarTests.cpp)
    target_compile_features(chat_scrollbar_checks PRIVATE cxx_std_17)
    add_test(NAME chat_scrollbar_checks COMMAND chat_scrollbar_checks)
    add_test(NAME packet_chat_restore_checks COMMAND packet_stream_replay_checks --chat-restore-only)
    add_test(NAME packet_chat_labels_checks COMMAND packet_stream_replay_checks --chat-labels-only)
    set_tests_properties(packet_chat_restore_checks packet_chat_labels_checks PROPERTIES TIMEOUT 30
        ENVIRONMENT "OPEN_MIDGARD_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/packet-chat-restore-settings")
    add_test(NAME packet_chat_window_write_checks COMMAND packet_stream_replay_checks --chat-window-write)
    add_test(NAME packet_chat_window_read_checks COMMAND packet_stream_replay_checks --chat-window-read)
    set_tests_properties(packet_chat_window_write_checks packet_chat_window_read_checks PROPERTIES TIMEOUT 30
        ENVIRONMENT "OPEN_MIDGARD_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/packet-chat-window-restart-settings")
    set_tests_properties(packet_chat_window_write_checks PROPERTIES FIXTURES_SETUP chat_window_preferences)
    set_tests_properties(packet_chat_window_read_checks PROPERTIES FIXTURES_REQUIRED chat_window_preferences)
    add_test(NAME packet_chat_scrollbar_checks COMMAND packet_stream_replay_checks --chat-scroll-only)
    set_tests_properties(packet_chat_scrollbar_checks PROPERTIES TIMEOUT 30
        ENVIRONMENT "OPEN_MIDGARD_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/packet-chat-scroll-settings")
    add_test(NAME packet_chat_options_checks COMMAND packet_stream_replay_checks --chat-options-only)
    set_tests_properties(packet_chat_options_checks PROPERTIES TIMEOUT 30
        ENVIRONMENT "OPEN_MIDGARD_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/packet-chat-options-settings")
    add_test(NAME packet_chat_options_write_checks COMMAND packet_stream_replay_checks --chat-options-write)
    add_test(NAME packet_chat_options_read_checks COMMAND packet_stream_replay_checks --chat-options-read)
    set_tests_properties(packet_chat_options_write_checks packet_chat_options_read_checks PROPERTIES TIMEOUT 30
        ENVIRONMENT "OPEN_MIDGARD_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/packet-chat-options-restart-settings")
    set_tests_properties(packet_chat_options_write_checks PROPERTIES FIXTURES_SETUP chat_options_preferences)
    set_tests_properties(packet_chat_options_read_checks PROPERTIES FIXTURES_REQUIRED chat_options_preferences)
    add_test(NAME packet_chat_detached_checks COMMAND packet_stream_replay_checks --chat-detached-only)
    set_tests_properties(packet_chat_detached_checks PROPERTIES TIMEOUT 30
        ENVIRONMENT "OPEN_MIDGARD_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/packet-chat-detached-settings")
    add_test(NAME packet_chat_primary_checks COMMAND packet_stream_replay_checks --chat-primary-only)
    set_tests_properties(packet_chat_primary_checks PROPERTIES TIMEOUT 30
        ENVIRONMENT "OPEN_MIDGARD_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/packet-chat-primary-settings")
    set_tests_properties(packet_stream_replay_checks PROPERTIES
        TIMEOUT 30
        ENVIRONMENT "OPEN_MIDGARD_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/packet-replay-settings")
    set_tests_properties(packet_corpse_lifecycle_checks PROPERTIES TIMEOUT 30
        ENVIRONMENT "OPEN_MIDGARD_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/packet-corpse-settings")
    set_tests_properties(packet_npc_class_change_checks PROPERTIES TIMEOUT 30
        ENVIRONMENT "OPEN_MIDGARD_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/packet-npc-class-change-settings")
    add_executable(party_position_state_checks tools/tests/PartyPositionTests.cpp)
    target_compile_features(party_position_state_checks PRIVATE cxx_std_17)
    target_include_directories(party_position_state_checks PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    if(MSVC)
        target_compile_options(party_position_state_checks PRIVATE /UNDEBUG)
    else()
        target_compile_options(party_position_state_checks PRIVATE -UNDEBUG)
    endif()
    add_test(NAME party_position_state_checks COMMAND party_position_state_checks)
    if(WIN32 AND RO_ENABLE_QT6_UI)
        get_filename_component(RO_PACKET_QT_PREFIX "${Qt6_DIR}/../../.." ABSOLUTE)
        target_compile_definitions(packet_stream_replay_checks PRIVATE
        RO_PACKET_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
            RO_PACKET_QT_PLATFORM_PLUGINS="${RO_PACKET_QT_PREFIX}/plugins/platforms")
        add_executable(packet_minimap_render_checks tools/tests/replay/MinimapRenderChecks.cpp)
        target_compile_features(packet_minimap_render_checks PRIVATE cxx_std_17)
        target_compile_definitions(packet_minimap_render_checks PRIVATE
            RO_PACKET_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
            RO_PACKET_QT_PLATFORM_PLUGINS="${RO_PACKET_QT_PREFIX}/plugins/platforms")
        target_link_libraries(packet_minimap_render_checks PRIVATE
            ${RO_PACKET_REPLAY_RUNTIME_LIBRARIES} ro_qtui Qt6::Quick Qt6::Qml
            Threads::Threads ws2_32 winmm dxguid ole32 oleaut32 psapi msimg32)
        add_test(NAME packet_minimap_render_checks COMMAND packet_minimap_render_checks
            "${CMAKE_CURRENT_BINARY_DIR}/packet-minimap-renders")
        set_tests_properties(packet_minimap_render_checks PROPERTIES TIMEOUT 45
            ENVIRONMENT "OPEN_MIDGARD_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/packet-minimap-settings;QT_QPA_PLATFORM_PLUGIN_PATH=${RO_PACKET_QT_PREFIX}/plugins/platforms")
        add_executable(packet_item_render_checks tools/tests/replay/ItemColourRenderChecks.cpp)
        target_compile_features(packet_item_render_checks PRIVATE cxx_std_17)
        target_compile_definitions(packet_item_render_checks PRIVATE
            RO_PACKET_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
            RO_PACKET_QT_PLATFORM_PLUGINS="${RO_PACKET_QT_PREFIX}/plugins/platforms")
        target_link_libraries(packet_item_render_checks PRIVATE
            ${RO_PACKET_REPLAY_RUNTIME_LIBRARIES} ro_qtui Qt6::Quick Qt6::Qml
            Threads::Threads ws2_32 winmm dxguid ole32 oleaut32 psapi msimg32)
        add_test(NAME packet_item_render_checks COMMAND packet_item_render_checks
            "${CMAKE_CURRENT_BINARY_DIR}/packet-item-renders")
        set_tests_properties(packet_item_render_checks PROPERTIES TIMEOUT 45
            ENVIRONMENT "OPEN_MIDGARD_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/packet-item-settings;QT_QPA_PLATFORM_PLUGIN_PATH=${RO_PACKET_QT_PREFIX}/plugins/platforms")
        add_test(NAME packet_special_item_name_render_checks COMMAND packet_item_render_checks
            "${CMAKE_CURRENT_BINARY_DIR}/packet-special-name-renders" "special-names")
        set_tests_properties(packet_special_item_name_render_checks PROPERTIES TIMEOUT 45
            ENVIRONMENT "OPEN_MIDGARD_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/packet-special-name-settings;QT_QPA_PLATFORM_PLUGIN_PATH=${RO_PACKET_QT_PREFIX}/plugins/platforms")
        add_test(NAME packet_rental_item_render_checks COMMAND packet_item_render_checks
            "${CMAKE_CURRENT_BINARY_DIR}/packet-rental-renders" "rental-items")
        set_tests_properties(packet_rental_item_render_checks PROPERTIES TIMEOUT 45
            ENVIRONMENT "OPEN_MIDGARD_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/packet-rental-settings;QT_QPA_PLATFORM_PLUGIN_PATH=${RO_PACKET_QT_PREFIX}/plugins/platforms")
        add_test(NAME packet_commerce_item_detail_render_checks COMMAND packet_item_render_checks
            "${CMAKE_CURRENT_BINARY_DIR}/packet-commerce-detail-renders" "commerce-details")
        set_tests_properties(packet_commerce_item_detail_render_checks PROPERTIES TIMEOUT 45
            ENVIRONMENT "OPEN_MIDGARD_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/packet-commerce-detail-settings;QT_QPA_PLATFORM_PLUGIN_PATH=${RO_PACKET_QT_PREFIX}/plugins/platforms")
        add_test(NAME packet_item_category_render_checks COMMAND packet_item_render_checks
            "${CMAKE_CURRENT_BINARY_DIR}/packet-item-category-renders" "item-categories")
        set_tests_properties(packet_item_category_render_checks PROPERTIES TIMEOUT 45
            ENVIRONMENT "OPEN_MIDGARD_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/packet-item-category-settings;QT_QPA_PLATFORM_PLUGIN_PATH=${RO_PACKET_QT_PREFIX}/plugins/platforms")
        add_test(NAME packet_item_detail_toggle_render_checks COMMAND packet_item_render_checks
            "${CMAKE_CURRENT_BINARY_DIR}/packet-item-detail-toggle-renders" "item-detail-toggle")
        set_tests_properties(packet_item_detail_toggle_render_checks PROPERTIES TIMEOUT 45
            ENVIRONMENT "OPEN_MIDGARD_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/packet-item-detail-toggle-settings;QT_QPA_PLATFORM_PLUGIN_PATH=${RO_PACKET_QT_PREFIX}/plugins/platforms")
        add_test(NAME packet_npc_shop_detail_render_checks COMMAND packet_item_render_checks
            "${CMAKE_CURRENT_BINARY_DIR}/packet-npc-shop-detail-renders" "npc-shop-details")
        set_tests_properties(packet_npc_shop_detail_render_checks PROPERTIES TIMEOUT 45
            ENVIRONMENT "OPEN_MIDGARD_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/packet-npc-shop-detail-settings;QT_QPA_PLATFORM_PLUGIN_PATH=${RO_PACKET_QT_PREFIX}/plugins/platforms")
        add_test(NAME packet_npc_shop_amount_render_checks COMMAND packet_item_render_checks
            "${CMAKE_CURRENT_BINARY_DIR}/packet-npc-shop-amount-renders" "npc-shop-amounts")
        set_tests_properties(packet_npc_shop_amount_render_checks PROPERTIES TIMEOUT 45
            ENVIRONMENT "OPEN_MIDGARD_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/packet-npc-shop-amount-settings;QT_QPA_PLATFORM_PLUGIN_PATH=${RO_PACKET_QT_PREFIX}/plugins/platforms")
        add_test(NAME packet_npc_shop_quantity_render_checks COMMAND packet_item_render_checks
            "${CMAKE_CURRENT_BINARY_DIR}/packet-npc-shop-quantity-renders" "npc-shop-quantity")
        set_tests_properties(packet_npc_shop_quantity_render_checks PROPERTIES TIMEOUT 45
            ENVIRONMENT "OPEN_MIDGARD_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/packet-npc-shop-quantity-settings;QT_QPA_PLATFORM_PLUGIN_PATH=${RO_PACKET_QT_PREFIX}/plugins/platforms")
        add_test(NAME packet_inventory_tab_startup_render_checks COMMAND packet_item_render_checks
            "${CMAKE_CURRENT_BINARY_DIR}/packet-inventory-tab-startup-renders" "item-tabs-startup")
        set_tests_properties(packet_inventory_tab_startup_render_checks PROPERTIES TIMEOUT 120
            ENVIRONMENT "OPEN_MIDGARD_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/packet-inventory-tab-startup-settings;QT_QPA_PLATFORM_PLUGIN_PATH=${RO_PACKET_QT_PREFIX}/plugins/platforms")
        add_test(NAME packet_inventory_geometry_render_checks COMMAND packet_item_render_checks
            "${CMAKE_CURRENT_BINARY_DIR}/packet-inventory-geometry-renders" "item-geometry")
        set_tests_properties(packet_inventory_geometry_render_checks PROPERTIES TIMEOUT 120
            ENVIRONMENT "OPEN_MIDGARD_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/packet-inventory-geometry-settings;QT_QPA_PLATFORM_PLUGIN_PATH=${RO_PACKET_QT_PREFIX}/plugins/platforms")
        add_test(NAME packet_inventory_mutation_render_checks COMMAND packet_item_render_checks
            "${CMAKE_CURRENT_BINARY_DIR}/packet-inventory-mutation-renders" "inventory-mutations")
        set_tests_properties(packet_inventory_mutation_render_checks PROPERTIES TIMEOUT 60
            ENVIRONMENT "OPEN_MIDGARD_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/packet-inventory-mutation-settings;QT_QPA_PLATFORM_PLUGIN_PATH=${RO_PACKET_QT_PREFIX}/plugins/platforms")
        add_test(NAME packet_inventory_show_render_checks COMMAND packet_item_render_checks
            "${CMAKE_CURRENT_BINARY_DIR}/packet-inventory-show-renders" "inventory-show")
        set_tests_properties(packet_inventory_show_render_checks PROPERTIES TIMEOUT 120
            ENVIRONMENT "OPEN_MIDGARD_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/packet-inventory-show-settings;QT_QPA_PLATFORM_PLUGIN_PATH=${RO_PACKET_QT_PREFIX}/plugins/platforms")
        add_test(NAME packet_item_card_detail_render_checks COMMAND packet_item_render_checks
            "${CMAKE_CURRENT_BINARY_DIR}/packet-item-card-detail-renders" "item-card-detail")
        set_tests_properties(packet_item_card_detail_render_checks PROPERTIES TIMEOUT 120
            ENVIRONMENT "OPEN_MIDGARD_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/packet-item-card-detail-settings;QT_QPA_PLATFORM_PLUGIN_PATH=${RO_PACKET_QT_PREFIX}/plugins/platforms")
        add_test(NAME packet_recovery_delta_render_checks COMMAND packet_item_render_checks
            "${CMAKE_CURRENT_BINARY_DIR}/packet-recovery-delta-renders" "recovery-delta")
        set_tests_properties(packet_recovery_delta_render_checks PROPERTIES TIMEOUT 60
            ENVIRONMENT "OPEN_MIDGARD_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/packet-recovery-delta-settings;QT_QPA_PLATFORM_PLUGIN_PATH=${RO_PACKET_QT_PREFIX}/plugins/platforms")
        add_test(NAME packet_standing_resurrection_render_checks COMMAND packet_item_render_checks
            "${CMAKE_CURRENT_BINARY_DIR}/packet-standing-resurrection-renders" "standing-resurrection")
        set_tests_properties(packet_standing_resurrection_render_checks PROPERTIES TIMEOUT 60
            ENVIRONMENT "OPEN_MIDGARD_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/packet-standing-resurrection-settings;QT_QPA_PLATFORM_PLUGIN_PATH=${RO_PACKET_QT_PREFIX}/plugins/platforms")
        add_test(NAME packet_chat_state_render_checks COMMAND packet_item_render_checks
            "${CMAKE_CURRENT_BINARY_DIR}/packet-chat-state-renders" "chat-state")
        set_tests_properties(packet_chat_state_render_checks PROPERTIES TIMEOUT 60
            ENVIRONMENT "OPEN_MIDGARD_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/packet-chat-render-settings;QT_QPA_PLATFORM_PLUGIN_PATH=${RO_PACKET_QT_PREFIX}/plugins/platforms")
        add_test(NAME packet_tip_window_render_checks COMMAND packet_item_render_checks
            "${CMAKE_CURRENT_BINARY_DIR}/packet-tip-window-renders" "tip-window")
        set_tests_properties(packet_tip_window_render_checks PROPERTIES TIMEOUT 60
            ENVIRONMENT "OPEN_MIDGARD_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/packet-tip-window-settings;QT_QPA_PLATFORM_PLUGIN_PATH=${RO_PACKET_QT_PREFIX}/plugins/platforms")
        add_test(NAME packet_command_window_render_checks COMMAND packet_item_render_checks
            "${CMAKE_CURRENT_BINARY_DIR}/packet-command-window-renders" "command-window")
        set_tests_properties(packet_command_window_render_checks PROPERTIES TIMEOUT 60
            ENVIRONMENT "OPEN_MIDGARD_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/packet-command-window-settings;QT_QPA_PLATFORM_PLUGIN_PATH=${RO_PACKET_QT_PREFIX}/plugins/platforms")
        add_test(NAME packet_pet_window_render_checks COMMAND packet_item_render_checks
            "${CMAKE_CURRENT_BINARY_DIR}/packet-pet-window-renders" "pet-window")
        set_tests_properties(packet_pet_window_render_checks PROPERTIES TIMEOUT 60
            ENVIRONMENT "OPEN_MIDGARD_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/packet-pet-window-settings;QT_QPA_PLATFORM_PLUGIN_PATH=${RO_PACKET_QT_PREFIX}/plugins/platforms")
        add_test(NAME packet_item_use_feedback_render_checks COMMAND packet_item_render_checks
            "${CMAKE_CURRENT_BINARY_DIR}/packet-item-use-feedback-renders" "item-use-feedback")
        set_tests_properties(packet_item_use_feedback_render_checks PROPERTIES TIMEOUT 60
            ENVIRONMENT "OPEN_MIDGARD_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/packet-item-use-feedback-settings;QT_QPA_PLATFORM_PLUGIN_PATH=${RO_PACKET_QT_PREFIX}/plugins/platforms")
        add_test(NAME packet_item_obtain_render_checks COMMAND packet_item_render_checks
            "${CMAKE_CURRENT_BINARY_DIR}/packet-item-obtain-renders" "item-obtain")
        set_tests_properties(packet_item_obtain_render_checks PROPERTIES TIMEOUT 60
            ENVIRONMENT "OPEN_MIDGARD_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/packet-item-obtain-settings;QT_QPA_PLATFORM_PLUGIN_PATH=${RO_PACKET_QT_PREFIX}/plugins/platforms")
        add_test(NAME packet_item_detail_geometry_render_checks COMMAND packet_item_render_checks
            "${CMAKE_CURRENT_BINARY_DIR}/packet-item-detail-geometry-renders" "item-detail-geometry")
        set_tests_properties(packet_item_detail_geometry_render_checks PROPERTIES TIMEOUT 120
            ENVIRONMENT "OPEN_MIDGARD_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/packet-item-detail-geometry-settings;QT_QPA_PLATFORM_PLUGIN_PATH=${RO_PACKET_QT_PREFIX}/plugins/platforms")
    endif()
endif()

if(RO_BUILD_SKILL_REPLAY_TESTS)
    target_compile_definitions(ro_world PRIVATE RO_SKILL_REPLAY_TRACE=1)
    target_compile_definitions(ro_gamemode PRIVATE RO_SKILL_REPLAY_TRACE=1)
    target_compile_definitions(ro_audio PRIVATE RO_SKILL_REPLAY_TRACE=1)
    enable_testing()
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_test(NAME ground_scope_source_check
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/tests/check_ground_scope_source.py")
    add_test(NAME skill_unit_model_asset_check
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/tests/check_skill_unit_model_assets.py")
    add_executable(use_skill_to_pos_info_packet_checks tools/tests/UseSkillToPosInfoPacketTests.cpp)
    target_compile_features(use_skill_to_pos_info_packet_checks PRIVATE cxx_std_17)
    target_link_libraries(use_skill_to_pos_info_packet_checks PRIVATE ro_network ro_core)
    add_test(NAME use_skill_to_pos_info_packet_checks COMMAND use_skill_to_pos_info_packet_checks)
    add_executable(skill_packet_replay_runtime_checks
        tools/tests/replay/SkillPacketRuntimeReplay.cpp)
    target_compile_features(skill_packet_replay_runtime_checks PRIVATE cxx_std_17)
    target_link_libraries(skill_packet_replay_runtime_checks PRIVATE
        ro_main ro_gamemode ro_session ro_world ro_skill ro_item ro_pathfinder
        ro_network ro_res ro_audio ro_render3d ro_render ro_ui ro_input
        ro_cipher ro_core ro_security ro_lua)
    if(RO_ENABLE_QT6_UI)
        target_link_libraries(skill_packet_replay_runtime_checks PRIVATE ro_qtui)
    endif()
    if(WIN32)
        target_link_libraries(skill_packet_replay_runtime_checks PRIVATE
            winmm.lib dxguid.lib ole32.lib oleaut32.lib psapi.lib msimg32.lib)
    endif()
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay/data/texture/effect")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/Ref/GRF-Content/data/texture/effect/alpha_down.tga"
        "${CMAKE_CURRENT_BINARY_DIR}/skill_replay/data/texture/effect/alpha_down.tga"
        COPYONLY)
    file(GLOB_RECURSE _trap_job143_model CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/Ref/GRF-Content/data/model/*03_3.rsm")
    list(LENGTH _trap_job143_model _trap_job143_model_count)
    if(NOT _trap_job143_model_count EQUAL 1)
        message(FATAL_ERROR "Expected one bundled job143 trap model ending in 03_3.rsm, found ${_trap_job143_model_count}")
    endif()
    list(GET _trap_job143_model 0 _trap_job143_model_path)
    get_filename_component(_trap_job143_model_directory "${_trap_job143_model_path}" DIRECTORY)
    file(GLOB _trap_model_assets CONFIGURE_DEPENDS "${_trap_job143_model_directory}/*.rsm")
    foreach(_trap_model_asset IN LISTS _trap_model_assets)
        file(RELATIVE_PATH _trap_model_relative "${CMAKE_CURRENT_SOURCE_DIR}/Ref/GRF-Content/data/model" "${_trap_model_asset}")
        get_filename_component(_trap_model_relative_dir "${_trap_model_relative}" DIRECTORY)
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay/data/model/${_trap_model_relative_dir}")
        configure_file("${_trap_model_asset}"
            "${CMAKE_CURRENT_BINARY_DIR}/skill_replay/data/model/${_trap_model_relative}" COPYONLY)
    endforeach()
    file(GLOB_RECURSE _trap_job143_bitmap CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/Ref/GRF-Content/data/texture/*/onetyel copy.bmp")
    list(LENGTH _trap_job143_bitmap _trap_job143_bitmap_count)
    if(NOT _trap_job143_bitmap_count EQUAL 1)
        message(FATAL_ERROR "Expected one bundled job143 material BMP, found ${_trap_job143_bitmap_count}")
    endif()
    list(GET _trap_job143_bitmap 0 _trap_job143_bitmap_path)
    get_filename_component(_trap_job143_bitmap_directory "${_trap_job143_bitmap_path}" DIRECTORY)
    file(GLOB _trap_bitmap_assets CONFIGURE_DEPENDS "${_trap_job143_bitmap_directory}/*.bmp")
    foreach(_trap_bitmap_asset IN LISTS _trap_bitmap_assets)
        file(RELATIVE_PATH _trap_bitmap_relative "${CMAKE_CURRENT_SOURCE_DIR}/Ref/GRF-Content/data/texture" "${_trap_bitmap_asset}")
        get_filename_component(_trap_bitmap_relative_dir "${_trap_bitmap_relative}" DIRECTORY)
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay/data/texture/${_trap_bitmap_relative_dir}")
        configure_file("${_trap_bitmap_asset}"
            "${CMAKE_CURRENT_BINARY_DIR}/skill_replay/data/texture/${_trap_bitmap_relative}" COPYONLY)
    endforeach()
    add_test(NAME skill_packet_replay_runtime_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/bash5.tsv")
    add_test(NAME skill_ground_scope_render_checks
        COMMAND skill_packet_replay_runtime_checks --ground-scope-render)
    add_test(NAME skill_packet_replay_trap_model_resource_checks
        COMMAND skill_packet_replay_runtime_checks --trap-model-resource
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/trap_reveal_lifecycle.tsv")
    add_test(NAME skill_packet_replay_double_strafe_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/double_strafe46.tsv")
    add_test(NAME skill_packet_replay_fire_bolt_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/fire_bolt19.tsv")
    add_test(NAME skill_packet_replay_heal_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/heal28.tsv")
    add_test(NAME skill_packet_replay_heal200_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/heal28_200.tsv")
    add_test(NAME skill_packet_replay_heal2000_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/heal28_2000.tsv")
    add_test(NAME skill_packet_replay_heal4000_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/heal28_4000.tsv")
    add_test(NAME skill_packet_replay_blessing_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/blessing34.tsv")
    add_test(NAME skill_packet_replay_sonic_blow_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/sonic_blow136.tsv")
    add_test(NAME skill_packet_replay_asura_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/asura_strike271.tsv")
    add_test(NAME skill_packet_replay_grimtooth_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/grimtooth137.tsv")
    add_test(NAME skill_packet_replay_storm_gust_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/storm_gust89.tsv")
    add_test(NAME skill_packet_replay_meteor_storm_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/meteor_storm83.tsv")
    add_test(NAME skill_packet_replay_lord_of_vermilion_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/lord_of_vermilion85.tsv")
    add_test(NAME skill_packet_replay_monster_arrow_shower47_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/monster_arrow_shower47.tsv")
    add_test(NAME skill_packet_replay_missing_actor_arrow_shower47_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/missing_actor_arrow_shower47.tsv")
    add_test(NAME skill_packet_replay_sanctuary_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/sanctuary70.tsv")
    add_test(NAME skill_packet_replay_firewall_unit_lifecycle_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/firewall_unit_lifecycle.tsv")
    add_test(NAME skill_packet_replay_safety_wall_lifecycle_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/safety_wall_unit_lifecycle.tsv")
    add_test(NAME skill_packet_replay_ice_wall_unit_lifecycle_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/ice_wall_unit_lifecycle.tsv")
    add_test(NAME skill_packet_replay_magnus_unit_lifecycle_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/magnus_unit_lifecycle.tsv")
    add_test(NAME skill_packet_replay_demonstration_unit_lifecycle_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/demonstration_unit_lifecycle.tsv")
    add_test(NAME skill_packet_replay_quagmire_unit_lifecycle_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/quagmire_unit_lifecycle.tsv")
    add_test(NAME skill_packet_replay_basilica_unit_lifecycle_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/basilica_unit_lifecycle.tsv")
    add_test(NAME skill_packet_replay_gospel_unit_lifecycle_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/gospel_unit_lifecycle.tsv")
    add_test(NAME skill_packet_replay_volcano_unit_lifecycle_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/volcano_unit_lifecycle.tsv")
    add_test(NAME skill_packet_replay_deluge_unit_lifecycle_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/deluge_unit_lifecycle.tsv")
    add_test(NAME skill_packet_replay_violent_gale_unit_lifecycle_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/violent_gale_unit_lifecycle.tsv")
    add_test(NAME skill_packet_replay_land_protector_unit_lifecycle_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/land_protector_unit_lifecycle.tsv")
    add_test(NAME skill_packet_replay_spider_web_unit_lifecycle_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/spider_web_unit_lifecycle.tsv")
    add_test(NAME skill_packet_replay_sanctuary_unit_lifecycle_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/sanctuary_unit_lifecycle.tsv")
    add_test(NAME skill_packet_replay_warp_unit_lifecycle_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/warp_unit_lifecycle.tsv")
    add_test(NAME skill_packet_replay_warp_map_list_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/warp_map_list.tsv")
    add_test(NAME skill_packet_replay_cast_cancel_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/cast_cancel01b9.tsv")
    add_test(NAME skill_packet_replay_skill_fail_ack_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/skill_fail_ack0110.tsv")
    add_test(NAME skill_packet_replay_trap_reveal_lifecycle_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/trap_reveal_lifecycle.tsv")
    add_test(NAME skill_packet_replay_land_mine_trigger_lifecycle_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/land_mine_trigger_lifecycle.tsv")
    add_test(NAME skill_packet_replay_graffiti_lifecycle_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/graffiti_lifecycle.tsv")
    add_test(NAME skill_packet_replay_blast_mine_trigger_lifecycle_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/blast_mine_trigger_lifecycle.tsv")
    add_test(NAME skill_packet_replay_claymore_trap_trigger_lifecycle_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/claymore_trap_trigger_lifecycle.tsv")
    add_test(NAME skill_packet_replay_sandman_trap_trigger_lifecycle_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/sandman_trap_trigger_lifecycle.tsv")
    add_test(NAME skill_packet_replay_freezing_trap_trigger_lifecycle_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/freezing_trap_trigger_lifecycle.tsv")
    add_test(NAME skill_packet_replay_flasher_trap_trigger_lifecycle_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/flasher_trap_trigger_lifecycle.tsv")
    add_test(NAME skill_packet_replay_ankle_snare_trap_trigger_lifecycle_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/ankle_snare_trap_trigger_lifecycle.tsv")
    add_test(NAME skill_packet_replay_shockwave_trap_trigger_lifecycle_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/shockwave_trap_trigger_lifecycle.tsv")
    add_test(NAME skill_packet_replay_skid_trap_trigger_lifecycle_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/skid_trap_trigger_lifecycle.tsv")
    add_test(NAME skill_packet_replay_multi_ground_unit_world_update_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/multi_ground_unit_world_update.tsv")
    add_test(NAME skill_packet_replay_multi_ground_unit_resource_world_update_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/multi_ground_unit_resource_world_update.tsv")
    add_test(NAME skill_packet_replay_venom_dust_trigger_lifecycle_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/venom_dust_trigger_lifecycle.tsv")
    add_test(NAME skill_packet_replay_talkie_box_trigger_lifecycle_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/talkie_box_trigger_lifecycle.tsv")
    add_test(NAME skill_packet_replay_cast_ack013e_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/cast_ack013e.tsv")
    add_test(NAME skill_packet_replay_cast_announcement_checks
        COMMAND skill_packet_replay_runtime_checks --cast-announcements
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/cast_ack013e.tsv")
    add_test(NAME skill_packet_replay_arrow_shower47_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/arrow_shower47.tsv")
    add_test(NAME skill_packet_replay_skill_damage2_662_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/skill_damage2_662.tsv")
    add_test(NAME skill_packet_replay_skill_damage2_141_levelFFFF_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/skill_damage2_141_levelFFFF.tsv")
    add_test(NAME skill_packet_replay_skill_damage_position_0115_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/skill_damage_position_0115.tsv")
    add_test(NAME skill_packet_replay_ground_skill_wave362_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/ground_skill_wave362.tsv")
    add_test(NAME skill_packet_replay_ground_skill_wave369_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/ground_skill_wave369.tsv")
    set_tests_properties(skill_packet_replay_runtime_checks PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
    set_tests_properties(skill_ground_scope_render_checks skill_packet_replay_trap_model_resource_checks PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
    set_tests_properties(skill_packet_replay_double_strafe_checks PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
    set_tests_properties(skill_packet_replay_fire_bolt_checks PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
    set_tests_properties(skill_packet_replay_heal_checks PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
    set_tests_properties(skill_packet_replay_blessing_checks PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
    set_tests_properties(skill_packet_replay_sonic_blow_checks PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
    set_tests_properties(skill_packet_replay_asura_checks PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
    set_tests_properties(skill_packet_replay_grimtooth_checks PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
    set_tests_properties(skill_packet_replay_storm_gust_checks skill_packet_replay_sanctuary_checks PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
    set_tests_properties(skill_packet_replay_firewall_unit_lifecycle_checks PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
    set_tests_properties(skill_packet_replay_safety_wall_lifecycle_checks PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
    set_tests_properties(skill_packet_replay_ice_wall_unit_lifecycle_checks PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
    set_tests_properties(skill_packet_replay_magnus_unit_lifecycle_checks PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
    set_tests_properties(skill_packet_replay_demonstration_unit_lifecycle_checks PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
    set_tests_properties(skill_packet_replay_quagmire_unit_lifecycle_checks PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
    set_tests_properties(skill_packet_replay_basilica_unit_lifecycle_checks PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
    set_tests_properties(skill_packet_replay_gospel_unit_lifecycle_checks PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
    set_tests_properties(skill_packet_replay_volcano_unit_lifecycle_checks
        skill_packet_replay_deluge_unit_lifecycle_checks
        skill_packet_replay_violent_gale_unit_lifecycle_checks
        skill_packet_replay_land_protector_unit_lifecycle_checks PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
    set_tests_properties(skill_packet_replay_spider_web_unit_lifecycle_checks PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
    set(_skill_packet_replay_song_scenarios
        lullaby richman_kim eternal_chaos drum_battle_field ring_nibelungen lokis_wail into_abyss siegfried
        dissonance whistle assassin_cross poem_of_bragi apple_of_idun ugly_dance humming dont_forget_me
        fortunes_kiss service_for_you)
    foreach(_scenario IN LISTS _skill_packet_replay_song_scenarios)
        add_test(NAME "skill_packet_replay_${_scenario}_unit_lifecycle_checks"
            COMMAND skill_packet_replay_runtime_checks
                "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/${_scenario}_unit_lifecycle.tsv")
        set_tests_properties("skill_packet_replay_${_scenario}_unit_lifecycle_checks" PROPERTIES
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
    endforeach()
    set(_skill_packet_replay_other_ground_scenarios gravitation suiton tatamigaeshi)
    foreach(_scenario IN LISTS _skill_packet_replay_other_ground_scenarios)
        add_test(NAME "skill_packet_replay_${_scenario}_unit_lifecycle_checks"
            COMMAND skill_packet_replay_runtime_checks
                "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/${_scenario}_unit_lifecycle.tsv")
        set_tests_properties("skill_packet_replay_${_scenario}_unit_lifecycle_checks" PROPERTIES
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
    endforeach()
    add_test(NAME skill_packet_replay_pneuma_default_unit_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/pneuma_default_unit.tsv")
    set_tests_properties(skill_packet_replay_pneuma_default_unit_checks PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
    set(_skill_packet_replay_periodic_ground_scenarios
        kaen ground_drift_wind ground_drift_dark ground_drift_poison ground_drift_water ground_drift_fire)
    foreach(_scenario IN LISTS _skill_packet_replay_periodic_ground_scenarios)
        add_test(NAME "skill_packet_replay_${_scenario}_unit_lifecycle_checks"
            COMMAND skill_packet_replay_runtime_checks
                "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/${_scenario}_unit_lifecycle.tsv")
        set_tests_properties("skill_packet_replay_${_scenario}_unit_lifecycle_checks" PROPERTIES
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
    endforeach()
    set(_skill_packet_replay_remaining_unit_scenarios
        job130 job136 job146 job178 job182 job185 job195 job196 job197 job199 job200 job201)
    foreach(_scenario IN LISTS _skill_packet_replay_remaining_unit_scenarios)
        add_test(NAME "skill_packet_replay_${_scenario}_unit_lifecycle_checks"
            COMMAND skill_packet_replay_runtime_checks
                "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/${_scenario}_unit_lifecycle.tsv")
        set_tests_properties("skill_packet_replay_${_scenario}_unit_lifecycle_checks" PROPERTIES
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
    endforeach()
    set_tests_properties(skill_packet_replay_sanctuary_unit_lifecycle_checks PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
    set_tests_properties(skill_packet_replay_warp_unit_lifecycle_checks PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
    set_tests_properties(skill_packet_replay_warp_map_list_checks PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
    set_tests_properties(skill_packet_replay_lord_of_vermilion_checks PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
    set_tests_properties(skill_packet_replay_cast_cancel_checks PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
    set_tests_properties(skill_packet_replay_skill_fail_ack_checks PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
    set_tests_properties(skill_packet_replay_trap_reveal_lifecycle_checks PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
    set_tests_properties(skill_packet_replay_land_mine_trigger_lifecycle_checks PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
    set_tests_properties(skill_packet_replay_graffiti_lifecycle_checks PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
    set_tests_properties(skill_packet_replay_blast_mine_trigger_lifecycle_checks PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
    set_tests_properties(skill_packet_replay_claymore_trap_trigger_lifecycle_checks PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
    set_tests_properties(skill_packet_replay_sandman_trap_trigger_lifecycle_checks PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
    set_tests_properties(skill_packet_replay_freezing_trap_trigger_lifecycle_checks PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
    set_tests_properties(skill_packet_replay_flasher_trap_trigger_lifecycle_checks PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
    set_tests_properties(skill_packet_replay_ankle_snare_trap_trigger_lifecycle_checks PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
    set_tests_properties(skill_packet_replay_shockwave_trap_trigger_lifecycle_checks PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
    set_tests_properties(skill_packet_replay_skid_trap_trigger_lifecycle_checks PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
    set_tests_properties(skill_packet_replay_multi_ground_unit_world_update_checks PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
    set_tests_properties(skill_packet_replay_multi_ground_unit_resource_world_update_checks PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
    set_tests_properties(skill_packet_replay_venom_dust_trigger_lifecycle_checks
        skill_packet_replay_talkie_box_trigger_lifecycle_checks
        skill_packet_replay_cast_ack013e_checks
        skill_packet_replay_cast_announcement_checks
        skill_packet_replay_arrow_shower47_checks
        skill_packet_replay_skill_damage2_662_checks
        skill_packet_replay_skill_damage2_141_levelFFFF_checks
        skill_packet_replay_ground_skill_wave362_checks
        skill_packet_replay_ground_skill_wave369_checks PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
endif()
