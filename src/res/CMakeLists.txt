cmake_minimum_required(VERSION 3.20)

add_library(ro_res STATIC
    Bitmap.h Bitmap.cpp
    EzEffectRes.h EzEffectRes.cpp
    GndRes.h GndRes.cpp
    ImfRes.h ImfRes.cpp
    ModelRes.h ModelRes.cpp
    PaletteRes.h PaletteRes.cpp
    Res.h Res.cpp
    Sprite.h Sprite.cpp
    Texture.h Texture.cpp
    WorldRes.h WorldRes.cpp
    Ijl.h Ijl.cpp
)

target_include_directories(ro_res PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(ro_res PUBLIC
    ro_core
)

if(RO_ENABLE_QT6_UI)
    target_link_libraries(ro_res PUBLIC
        Qt6::Gui
    )
endif()
