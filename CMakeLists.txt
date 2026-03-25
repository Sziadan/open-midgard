cmake_minimum_required(VERSION 3.20)
project(RagnarokHighPriest VERSION 2008.0 LANGUAGES CXX)

# ─── Toolchain requirements ────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Force Win32 (x86) target – the original binary is 32-bit
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "This project must be built as Win32 (32-bit). "
            "Add -A Win32 to CMake configuration.")
endif()

add_definitions(-DWIN32 -D_WINDOWS -DUNICODE -D_UNICODE -DNOMINMAX)
add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE)

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
    set(DX7_LIBDIR  "$ENV{DX7_SDK_DIR}/lib/x86")
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
add_executable(HighPriest WIN32 src/main/WinMain.cpp)

target_link_libraries(HighPriest PRIVATE
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
    target_link_options(HighPriest PRIVATE /NODEFAULTLIB:ddraw.lib)
endif()

if(DEFINED DX7_INCLUDE)
    target_include_directories(HighPriest PRIVATE ${DX7_INCLUDE})
    target_link_directories(HighPriest PRIVATE ${DX7_LIBDIR})
endif()

add_custom_command(TARGET HighPriest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:HighPriest>/dlls"
    COMMAND ${CMAKE_COMMAND} -E make_directory "D:/Spel/OldRO"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/dlls"
            "$<TARGET_FILE_DIR:HighPriest>/dlls"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:HighPriest>"
            "D:/Spel/OldRO/HighPriest.exe"
    COMMENT "Copying runtime DLLs to output folder"
)
