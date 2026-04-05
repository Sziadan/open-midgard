cmake_minimum_required(VERSION 3.20)

include(FetchContent)

FetchContent_Declare(lua51
    URL https://www.lua.org/ftp/lua-5.1.5.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_GetProperties(lua51)
if(NOT lua51_POPULATED)
    FetchContent_Populate(lua51)
endif()

add_library(lua51 STATIC
    ${lua51_SOURCE_DIR}/src/lapi.c
    ${lua51_SOURCE_DIR}/src/lauxlib.c
    ${lua51_SOURCE_DIR}/src/lbaselib.c
    ${lua51_SOURCE_DIR}/src/lcode.c
    ${lua51_SOURCE_DIR}/src/ldblib.c
    ${lua51_SOURCE_DIR}/src/ldebug.c
    ${lua51_SOURCE_DIR}/src/ldo.c
    ${lua51_SOURCE_DIR}/src/ldump.c
    ${lua51_SOURCE_DIR}/src/lfunc.c
    ${lua51_SOURCE_DIR}/src/lgc.c
    ${lua51_SOURCE_DIR}/src/linit.c
    ${lua51_SOURCE_DIR}/src/liolib.c
    ${lua51_SOURCE_DIR}/src/llex.c
    ${lua51_SOURCE_DIR}/src/lmathlib.c
    ${lua51_SOURCE_DIR}/src/lmem.c
    ${lua51_SOURCE_DIR}/src/loadlib.c
    ${lua51_SOURCE_DIR}/src/lobject.c
    ${lua51_SOURCE_DIR}/src/lopcodes.c
    ${lua51_SOURCE_DIR}/src/loslib.c
    ${lua51_SOURCE_DIR}/src/lparser.c
    ${lua51_SOURCE_DIR}/src/lstate.c
    ${lua51_SOURCE_DIR}/src/lstring.c
    ${lua51_SOURCE_DIR}/src/lstrlib.c
    ${lua51_SOURCE_DIR}/src/ltable.c
    ${lua51_SOURCE_DIR}/src/ltablib.c
    ${lua51_SOURCE_DIR}/src/ltm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lua51_lundump_compat.c
    ${lua51_SOURCE_DIR}/src/lvm.c
    ${lua51_SOURCE_DIR}/src/lzio.c
)

target_include_directories(lua51 PUBLIC
    ${lua51_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(MSVC)
    target_compile_definitions(lua51 PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS)
endif()

add_library(ro_lua STATIC
    LuaBridge.h
    LuaBridge.cpp
)

target_include_directories(ro_lua PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ro_lua PUBLIC
    ro_core
    lua51
)
