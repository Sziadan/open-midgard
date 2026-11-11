cmake_minimum_required(VERSION 3.20)

add_library(ro_core STATIC
    Timer.h     Timer.cpp
    MainLoopWatchdog.h MainLoopWatchdog.cpp
    Hash.h      Hash.cpp
    SettingsIni.h SettingsIni.cpp
    UiMessageStrings.h UiMessageStrings.cpp
    TipOfTheDay.h TipOfTheDay.cpp
    SourceMapNames.h SourceMapNames.cpp
    SourceMapTypeTable.h SourceMapTypeTable.cpp
    InsultWords.h MannerFilter.h MannerFilter.cpp
    SourceTextCodec.h SourceTextCodec.cpp
    GPak.h      GPak.cpp
    File.h      File.cpp
    ZipEntry.h ZipEntry.cpp
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

add_library(ro_minizip_reader STATIC
    ${zlib_SOURCE_DIR}/contrib/minizip/ioapi.c
    ${zlib_SOURCE_DIR}/contrib/minizip/unzip.c
)
target_include_directories(ro_minizip_reader PUBLIC ${zlib_SOURCE_DIR}/contrib/minizip)
target_link_libraries(ro_minizip_reader PRIVATE zlibstatic)

target_link_libraries(ro_core PRIVATE zlibstatic ro_minizip_reader)

if(WIN32)
    target_link_libraries(ro_core PRIVATE winmm.lib)
else()
    find_package(Iconv REQUIRED)
    target_link_libraries(ro_core PRIVATE Iconv::Iconv)
endif()
