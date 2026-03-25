cmake_minimum_required(VERSION 3.20)

add_library(ro_core STATIC
    Timer.h     Timer.cpp
    Hash.h      Hash.cpp
    GPak.h      GPak.cpp
    File.h      File.cpp
    Globals.h   Globals.cpp
    ClientInfoLocale.h Locale.cpp
    Xml.h       Xml.cpp
    DllMgr.h
    DllMgr.cpp
)

target_include_directories(ro_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/..)

# Force-include Types.h as the very first header in every translation unit so
# that <locale.h> and <windows.h> are set up before any STL or project header
# can pull in <clocale> (which does 'using ::lconv' in MSVC 14.44+).
if(MSVC)
    target_compile_options(ro_core PRIVATE
        /FI "${CMAKE_CURRENT_SOURCE_DIR}/../Types.h")
endif()


include(FetchContent)
FetchContent_Declare(
    ZLIB
    URL https://github.com/madler/zlib/archive/refs/tags/v1.3.1.tar.gz
)
FetchContent_MakeAvailable(ZLIB)

target_link_libraries(ro_core PRIVATE zlibstatic winmm.lib)
