#pragma once
#include "Types.h"
#include <vector>
#include <memory>
class CTexture;
class CGameMode;

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
    u32 guildId=0, emblemVersion=0;
};

// Shared by all flag instances owned by this guild. A new version replaces the
// cached texture; missing data never exposes the model's green placeholder.
CTexture* Gr2GuildEmblemTexture(u32 guildId,u32 version,const std::vector<unsigned char>* bitmap=nullptr);
u32 PrepareGuildEmblem(CGameMode& mode,u32 guildId,u32 advertisedVersion);

bool RenderGr2Monster(const Gr2MonsterState& state, const matrix& view);
bool GetGr2MonsterBounds(u32 gid, RECT* bounds);
bool Gr2MonsterMotionFinished(u32 gid, int state, u32 stateStartTick);
void ReleaseGr2Monster(u32 gid);
void ClearGr2Monsters();

void PreloadGr2Monsters();
void WaitForGr2Monsters();

// A corpse owns an animation/mesh instance independently of the actor-ID cache.
// Shared immutable model/texture resources remain pinned across cache teardown.
class Gr2CorpseModel {
public:
    explicit Gr2CorpseModel(int job);
    ~Gr2CorpseModel();
    Gr2CorpseModel(const Gr2CorpseModel&) = delete;
    Gr2CorpseModel& operator=(const Gr2CorpseModel&) = delete;
    bool Render(const vector3d& position, float rotation, u8 alpha, const matrix& view, double elapsedSeconds);
    float Clock() const;
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
float Gr2MonsterCurrentRotation(u32 gid, float actorRotation);
