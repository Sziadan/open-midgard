#pragma once
#include "Types.h"

// Ref/Session.cpp uses these same GR2 resources for the original and Thor
// guardian classes, including their battlefield/instance variants.
// nType indices from HighPriest.exe.c job[] / C3dGrannyBoneRes::GetAnimation:
// 0 empelium, 1 guild flag, 2 treasure box, 7–9 guardians.
constexpr const char* Gr2MonsterModel(int job)
{
    switch (job) {
    case 1288: return "data\\model\\3dmob\\empelium90_0.gr2";
    case 722: return "data\\model\\3dmob\\guildflag90_1.gr2";
    case 1324: return "data\\model\\3dmob\\treasurebox_2.gr2";
    case 1285: case 1830: case 1900: case 1950: return "data\\model\\3dmob\\aguardian90_8.gr2";
    case 1286: case 1829: case 1899: case 1949: return "data\\model\\3dmob\\kguardian90_7.gr2";
    case 1287: return "data\\model\\3dmob\\sguardian90_9.gr2";
    default: return nullptr;
    }
}

struct Gr2MonsterState {
    u32 gid;
    int job, state;
    u32 stateStartTick;
    vector3d pos;
    float rotation, zoom;
    bool frozen;
    u32 color;
    vector3d light, diffuse, ambient;
    float attackSpeedFactor=1.0f;
};

bool RenderGr2Monster(const Gr2MonsterState& state, const matrix& view);
bool GetGr2MonsterBounds(u32 gid, RECT* bounds);
bool Gr2MonsterMotionFinished(u32 gid, int state, u32 stateStartTick);
void ReleaseGr2Monster(u32 gid);
void ClearGr2Monsters();

void PreloadGr2Monsters();
void WaitForGr2Monsters();
