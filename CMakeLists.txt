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
option(RO_ENABLE_GRANNY "Enable Granny runtime loading" ON)
option(RO_ENABLE_IJL "Enable Intel JPEG Library runtime loading" OFF)
option(RO_ENABLE_DEV_DEPLOY "Copy build output to the local OldRO folder" ON)
option(RO_BUILD_SKILL_REPLAY_TESTS "Build production-linked skill packet replay tests" OFF)
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

target_link_libraries(open_midgard PRIVATE
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
)

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

if(RO_BUILD_SKILL_REPLAY_TESTS)
    target_compile_definitions(ro_world PRIVATE RO_SKILL_REPLAY_TRACE=1)
    target_compile_definitions(ro_gamemode PRIVATE RO_SKILL_REPLAY_TRACE=1)
    target_compile_definitions(ro_audio PRIVATE RO_SKILL_REPLAY_TRACE=1)
    enable_testing()
    add_executable(skill_packet_replay_runtime_checks
        tools/tests/replay/SkillPacketRuntimeReplay.cpp)
    target_compile_features(skill_packet_replay_runtime_checks PRIVATE cxx_std_17)
    target_link_libraries(skill_packet_replay_runtime_checks PRIVATE
        ro_main ro_gamemode ro_session ro_world ro_skill ro_item ro_pathfinder
        ro_network ro_res ro_audio ro_render3d ro_render ro_ui ro_input
        ro_cipher ro_core ro_security ro_lua)
    if(WIN32)
        target_link_libraries(skill_packet_replay_runtime_checks PRIVATE
            winmm.lib dxguid.lib ole32.lib oleaut32.lib psapi.lib msimg32.lib)
    endif()
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
    add_test(NAME skill_packet_replay_runtime_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/bash5.tsv")
    add_test(NAME skill_packet_replay_double_strafe_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/double_strafe46.tsv")
    add_test(NAME skill_packet_replay_fire_bolt_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/fire_bolt19.tsv")
    add_test(NAME skill_packet_replay_heal_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/heal28.tsv")
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
    add_test(NAME skill_packet_replay_sanctuary_unit_lifecycle_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/sanctuary_unit_lifecycle.tsv")
    add_test(NAME skill_packet_replay_warp_unit_lifecycle_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/warp_unit_lifecycle.tsv")
    add_test(NAME skill_packet_replay_cast_cancel_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/cast_cancel01b9.tsv")
    add_test(NAME skill_packet_replay_trap_reveal_lifecycle_checks
        COMMAND skill_packet_replay_runtime_checks
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/skill/replay/trap_reveal_lifecycle.tsv")
    set_tests_properties(skill_packet_replay_runtime_checks PROPERTIES
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
    set_tests_properties(skill_packet_replay_sanctuary_unit_lifecycle_checks PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
    set_tests_properties(skill_packet_replay_warp_unit_lifecycle_checks PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
    set_tests_properties(skill_packet_replay_cast_cancel_checks PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
    set_tests_properties(skill_packet_replay_trap_reveal_lifecycle_checks PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skill_replay")
endif()
