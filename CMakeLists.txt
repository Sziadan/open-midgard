cmake_minimum_required(VERSION 3.20)
project(OpenMidgard VERSION 2008.0 LANGUAGES CXX)

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

add_definitions(-DWIN32 -D_WINDOWS -DUNICODE -D_UNICODE -DNOMINMAX)
add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE)
add_compile_definitions(
    RO_ENABLE_MILES_AUDIO=$<BOOL:${RO_ENABLE_MILES_AUDIO}>
    RO_ENABLE_BINK_VIDEO=$<BOOL:${RO_ENABLE_BINK_VIDEO}>
    RO_ENABLE_GRANNY=$<BOOL:${RO_ENABLE_GRANNY}>
    RO_ENABLE_IJL=$<BOOL:${RO_ENABLE_IJL}>
    RO_X64_FIRST_MILESTONE_BUILD=$<BOOL:${RO_X64_FIRST_MILESTONE_BUILD}>
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# ─── Compiler options ─────────────────────────────────────────────────────
if(MSVC)
    add_compile_options(/W3 /MP /Zc:__cplusplus)
    # Disable warnings that are unavoidable in a decompiler-origin codebase
    add_compile_options(/wd4100 /wd4189 /wd4996)
endif()

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
add_subdirectory(src/skill)
add_subdirectory(src/item)
add_subdirectory(src/pathfinder)
add_subdirectory(src/lua)
add_subdirectory(src/security)
add_subdirectory(src/main)

# ─── Final executable ─────────────────────────────────────────────────────
add_executable(open_midgard WIN32 src/main/WinMain.cpp)
set_target_properties(open_midgard PROPERTIES OUTPUT_NAME "open-midgard")

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
    # Windows system libraries
    winmm.lib
    dxguid.lib
    ole32.lib
    oleaut32.lib
    psapi.lib
)

if(MSVC)
    # ddraw.h can auto-link ddraw.lib via pragma; we resolve DirectDraw dynamically.
    target_link_options(open_midgard PRIVATE /NODEFAULTLIB:ddraw.lib)
endif()

if(DEFINED DX7_INCLUDE)
    target_include_directories(open_midgard PRIVATE ${DX7_INCLUDE})
    target_link_directories(open_midgard PRIVATE ${DX7_LIBDIR})
endif()

add_custom_command(TARGET open_midgard POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:open_midgard>/dlls"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/dlls"
            "$<TARGET_FILE_DIR:open_midgard>/dlls"
    COMMENT "Copying runtime DLLs to output folder"
)

if(RO_ENABLE_DEV_DEPLOY)
    add_custom_command(TARGET open_midgard POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "D:/Spel/OldRO"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:open_midgard>"
                "D:/Spel/OldRO/open-midgard.exe"
        COMMENT "Copying executable to local OldRO folder"
    )
endif()
