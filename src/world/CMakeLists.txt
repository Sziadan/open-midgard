cmake_minimum_required(VERSION 3.20)

add_library(ro_world STATIC
    3dActor.h
    3dActor.cpp
    World.h
    World.cpp
    GameActor.h
    GameActor.cpp
    RagEffect.h
    RagEffect.cpp
    SkillSpecialEffect.cpp
    ArrowEffect.cpp
    GroundScope.cpp
    SkillUnit.cpp
    GraffiSkill.cpp
    GameActorMsgHandler.cpp
    MsgEffect.h
    MsgEffect.cpp
    Granny.h
    Granny.cpp
)

target_include_directories(ro_world PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ro_world PUBLIC
    ro_core
    ro_res
    ro_ui
    ro_pathfinder
)
