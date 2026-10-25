#include "world/GameActor.h"
#include "audio/Audio.h"

#include "RagEffectDurationTable.generated.inc"

#include "gamemode/GameMode.h"
#include "gamemode/Mode.h"
#include "res/ModelRes.h"
#include "res/Res.h"
#include "session/Session.h"
#include "world/3dActor.h"
#include "world/RagEffect.h"
#include "world/World.h"

#include <cstddef>
#include <cstdlib>
#include <cstring>

#include <windows.h>

namespace {

constexpr int kDoubleAttackTerm = 200;

const char kTrap01[] = "\xBF\xDC\xBA\xCE\xBC\xD2\xC7\xB0\\\xC6\xAE\xB7\xA6" "01.rsm";
const char kTrap02[] = "\xBF\xDC\xBA\xCE\xBC\xD2\xC7\xB0\\\xC6\xAE\xB7\xA6" "02.rsm";
const char kTrap03[] = "\xBF\xDC\xBA\xCE\xBC\xD2\xC7\xB0\\\xC6\xAE\xB7\xA6" "03.rsm";
const char kTrap03_2[] = "\xBF\xDC\xBA\xCE\xBC\xD2\xC7\xB0\\\xC6\xAE\xB7\xA6" "03_2.rsm";
const char kTrap03_3[] = "\xBF\xDC\xBA\xCE\xBC\xD2\xC7\xB0\\\xC6\xAE\xB7\xA6" "03_3.rsm";
const char kTrap03_4[] = "\xBF\xDC\xBA\xCE\xBC\xD2\xC7\xB0\\\xC6\xAE\xB7\xA6" "03_4.rsm";
const char kTrap03_5[] = "\xBF\xDC\xBA\xCE\xBC\xD2\xC7\xB0\\\xC6\xAE\xB7\xA6" "03_5.rsm";
const char kTrap03_6[] = "\xBF\xDC\xBA\xCE\xBC\xD2\xC7\xB0\\\xC6\xAE\xB7\xA6" "03_6.rsm";
const char kTrap04[] = "\xBF\xDC\xBA\xCE\xBC\xD2\xC7\xB0\\\xC6\xAE\xB7\xA6" "04.rsm";
const char kTrap05[] = "\xBF\xDC\xBA\xCE\xBC\xD2\xC7\xB0\\\xC6\xAE\xB7\xA6" "05.rsm";
const char kFlameWave[] = "effect\\\xC8\xAD\xBF\xB0\xC1\xF8.wav";

struct SkillHitBlock {
    int skillId;
    int damage;
    int action;
    u32 targetGid;
    u32 startTime;
    int level;
    int count;
    int cellX;
    int cellY;
    int attackMT;
    int attackedMT;
};

static_assert(offsetof(SkillStandEntry, isVisible) == 20, "stand entry matches the 2008 stack block");
static_assert(offsetof(SkillHitBlock, attackedMT) == 40, "skill hit block matches SKILLSTRUCT");

bool CreatorIsPartyMember(u32 aid)
{
    for (const FRIEND_INFO& member : g_session.m_partyList) {
        if (member.AID == aid && member.isValid != 0) {
            return true;
        }
    }
    return false;
}

CGameActor* FindActorByGid(CWorld* world, u32 gid)
{
    if (world == nullptr) {
        return nullptr;
    }
    for (CGameActor* actor : world->m_actorList) {
        if (actor != nullptr && actor->m_gid == gid) {
            return actor;
        }
    }
    return nullptr;
}

int UnitHitEffectId(int skillId)
{
    int effectId = 0;
    if (skillId >= 493) {
        effectId = -1;
    }
    if (skillId > 85) {
        if (skillId == 89) {
            effectId = 51;
        } else if (skillId > 427 && skillId <= 430) {
            effectId = 583;
        }
    } else {
        switch (skillId) {
        case 85:
            effectId = 52;
            break;
        case 18:
            effectId = 49;
            break;
        case 79:
            effectId = 152;
            break;
        default:
            break;
        }
    }
    return effectId;
}

} // namespace

CSkill::CSkill()
    : m_launchCnt(36000)
    , m_SkillStartTime(timeGetTime())
    , m_aid(0)
    , m_3dactor(nullptr)
    , m_LoopEffect(nullptr)
    , m_effectId(-1)
{
    m_job = 126;
    m_shouldAddPickInfo = 0;
    m_isVisibleBody = 0;
    m_balloon = nullptr;
}

CSkill::~CSkill()
{
    if (m_3dactor != nullptr) {
        HideModel();
    }
    m_balloon = nullptr;
}

void CSkill::HideModel()
{
    delete m_3dactor;
    m_3dactor = nullptr;
    if (m_LoopEffect != nullptr) {
        m_LoopEffect->SendMsg(m_LoopEffect, 24, 0, 0, 0);
        m_LoopEffect = nullptr;
    }
}

const char* CSkill::TrapModelName() const
{
    switch (m_job) {
    case 143:
        return kTrap03_3;
    case 144:
        return kTrap02;
    case 145:
        return kTrap01;
    case 147:
        return kTrap03;
    case 148:
        return kTrap03_6;
    case 149:
        return kTrap03_4;
    case 150:
        return kTrap03_5;
    case 151:
        return kTrap03_2;
    case 152:
        return kTrap04;
    case 153:
        return kTrap05;
    default:
        return nullptr;
    }
}

void CSkill::ShowModel(const char* modelName)
{
    if (modelName == nullptr || m_isVisibleBody == 0) {
        return;
    }

    if (m_3dactor != nullptr) {
        HideModel();
    }

    C3dModelRes* model = g_resMgr.GetAs<C3dModelRes>(modelName);
    if (model == nullptr) {
        return;
    }

    C3dActor* actor = new C3dActor();
    if (!actor->AssignModel(*model)) {
        delete actor;
        return;
    }

    actor->m_animSpeed = 0.33333334f;
    actor->m_pos = m_pos;
    actor->m_isMatrixNeedUpdate = 1;
    std::strncpy(actor->m_name, modelName, sizeof(actor->m_name) - 1);
    actor->m_name[sizeof(actor->m_name) - 1] = '\0';
    actor->SetFrame(3);
    actor->m_blockType = 0;
    actor->m_isHideCheck = 0;
    actor->m_animType = 0;
    const vector3d lightDir{ 0.5f, 0.69999999f, 0.5f };
    const vector3d diffuseCol{ 1.0f, 1.0f, 1.0f };
    const vector3d ambientCol{ 0.5f, 0.5f, 0.5f };
    actor->UpdateVertexColor(lightDir, diffuseCol, ambientCol);
    actor->UpdateMatrix();
    m_3dactor = actor;
}

void CSkill::PlayUnitWave(const char* name) const
{
    if (name != nullptr && g_world.m_player != nullptr) {
        const vector3d& player = g_world.m_player->m_pos;
        PlayWave(name, m_pos.x - player.x, 0.0f, m_pos.z - player.z);
    }
}

void CSkill::LaunchPeriodic(int effectId, int subtract)
{
    const int duration = (effectId >= 0 && effectId < kRagEffectDurationCount)
        ? kRagEffectDurationTable[effectId]
        : 0;
    if (static_cast<int>(m_launchCnt) >= duration - subtract) {
        m_launchCnt = 0;
        LaunchEffect(effectId, vector3d{}, 0.0f);
    }
    ++m_launchCnt;
}

void CSkill::LaunchLoop(int effectId)
{
    if (m_LoopEffect == nullptr) {
        m_LoopEffect = LaunchEffect(effectId, vector3d{}, 0.0f);
    }
    if (m_LoopEffect != nullptr) {
        m_LoopEffect->AddDuration(21);
    }
    ++m_launchCnt;
}

void CSkill::ApplyStandEntry(const SkillStandEntry& entry)
{
    m_aid = entry.aid;
    m_gid = entry.aid;
    m_job = entry.job;
    m_isVisibleBody = entry.isVisible;
    if (m_isVisibleBody == 0
        && (entry.creatorAid == g_session.m_aid || CreatorIsPartyMember(entry.creatorAid))) {
        m_isVisibleBody = 1;
    }

    CGameMode* gameMode = g_modeMgr.GetCurrentGameMode();
    if (gameMode != nullptr) {
        float clientX = 0.0f;
        float clientZ = 0.0f;
        gameMode->GetClientCoor(entry.cellX, entry.cellY, clientX, clientZ);
        m_pos.x = clientX;
        m_pos.z = clientZ;
        m_pos.y = (gameMode->m_world != nullptr && gameMode->m_world->m_attr != nullptr)
            ? gameMode->m_world->m_attr->GetHeight(clientX, clientZ)
            : 0.0f;
    }
    m_effectId = -1;

    switch (m_job) {
    case 127:
        PlayUnitWave("effect\\EF_FireWall.wav");
        m_shouldAddPickInfo = 0;
        break;
    case 138:
        m_shouldAddPickInfo = 1;
        ShowModel(TrapModelName());
        break;
    case 141:
        m_shouldAddPickInfo = 1;
        return;
    case 143:
        m_shouldAddPickInfo = 1;
        m_effectId = 106;
        ShowModel(TrapModelName());
        break;
    case 144:
    case 145:
    case 147:
    case 153:
        m_shouldAddPickInfo = 1;
        ShowModel(TrapModelName());
        break;
    case 148:
        m_shouldAddPickInfo = 1;
        ShowModel(TrapModelName());
        if (!EffectExists(145)) {
            LaunchEffect(145, vector3d{}, 0.0f);
        }
        return;
    case 149:
        m_shouldAddPickInfo = 1;
        m_effectId = 139;
        ShowModel(TrapModelName());
        break;
    case 150:
        m_shouldAddPickInfo = 1;
        m_effectId = 99;
        ShowModel(TrapModelName());
        break;
    case 151:
        m_shouldAddPickInfo = 1;
        m_effectId = 108;
        ShowModel(TrapModelName());
        break;
    case 152:
        m_shouldAddPickInfo = 1;
        m_effectId = 107;
        ShowModel(TrapModelName());
        break;
    case 188:
    case 190:
    case 191:
    case 192:
    case 193:
    case 194:
        m_shouldAddPickInfo = 0;
        break;
    case 189:
        if ((std::rand() & 7) == 0) {
            PlayUnitWave(kFlameWave);
        }
        m_shouldAddPickInfo = 0;
        break;
    default:
        return;
    }
}

bool CSkill::EffectExists(int effectId) const
{
    for (const CRagEffect* effect : m_effectList) {
        if (effect != nullptr && effect->GetEffectType() == effectId) {
            return true;
        }
    }
    return false;
}

u8 CSkill::OnProcess()
{
    ProcessWillBeAttacked();
    if (m_3dactor != nullptr && m_3dactor->m_animType != 0) {
        m_3dactor->AdvanceFrame();
    }

    switch (m_job) {
    case 126:
        LaunchPeriodic(315, 7);
        return 1;
    case 127:
        if (m_SkillStartTime + 3000u < timeGetTime()) {
            return 0;
        }
        LaunchPeriodic(25, 7);
        return 1;
    case 128:
        if (g_session.m_isEffectOn) {
            LaunchPeriodic(317, 8);
        } else {
            LaunchPeriodic(36, 8);
        }
        return 1;
    case 129:
        LaunchPeriodic(316, 8);
        return 1;
    case 130:
        LaunchPeriodic(91, 8);
        return 1;
    case 131:
        LaunchLoop(319);
        return 1;
    case 132:
        LaunchLoop(318);
        return 1;
    case 135:
    case 197:
        LaunchLoop(138);
        return 1;
    case 136:
        LaunchPeriodic(97, 8);
        return 1;
    case 141:
    case 195:
    case 196:
        LaunchLoop(74);
        return 1;
    case 142:
        LaunchPeriodic(95, 20);
        return 1;
    case 146:
        LaunchLoop(171);
        return 1;
    case 154:
        LaunchLoop(239);
        return 1;
    case 155:
        LaunchLoop(240);
        return 1;
    case 156:
        LaunchLoop(241);
        return 1;
    case 157:
        LaunchLoop(242);
        return 1;
    case 158:
        LaunchLoop(278);
        return 1;
    case 159:
        LaunchLoop(279);
        return 1;
    case 160:
        LaunchLoop(280);
        return 1;
    case 161:
        LaunchLoop(281);
        return 1;
    case 162:
        LaunchLoop(282);
        return 1;
    case 163:
        LaunchLoop(283);
        return 1;
    case 164:
        LaunchLoop(284);
        return 1;
    case 165:
        LaunchLoop(285);
        return 1;
    case 166:
        LaunchLoop(277);
        return 1;
    case 167:
        LaunchLoop(286);
        return 1;
    case 168:
        LaunchLoop(287);
        return 1;
    case 169:
        LaunchLoop(288);
        return 1;
    case 170:
        LaunchLoop(289);
        return 1;
    case 171:
        LaunchLoop(290);
        return 1;
    case 172:
        LaunchLoop(291);
        return 1;
    case 173:
        LaunchLoop(292);
        return 1;
    case 174:
        LaunchLoop(293);
        return 1;
    case 175:
        LaunchLoop(294);
        return 1;
    case 177:
        LaunchLoop(302);
        return 1;
    case 178:
        LaunchPeriodic(341, 8);
        return 1;
    case 179:
        LaunchLoop(370);
        return 1;
    case 180:
        LaunchLoop(374);
        return 1;
    case 182:
        LaunchLoop(405);
        return 1;
    case 183:
        LaunchLoop(404);
        return 1;
    case 184:
        LaunchLoop(522);
        return 1;
    case 185:
        LaunchLoop(517);
        return 1;
    case 187:
        LaunchLoop(620);
        return 1;
    case 188:
        LaunchLoop(631);
        return 1;
    case 189:
        LaunchPeriodic(634, 7);
        return 1;
    case 190:
        LaunchPeriodic(638, 7);
        return 1;
    case 191:
        LaunchPeriodic(639, 7);
        return 1;
    case 192:
        LaunchPeriodic(640, 7);
        return 1;
    case 193:
        LaunchPeriodic(641, 7);
        return 1;
    case 194:
        LaunchPeriodic(642, 7);
        return 1;
    case 199:
        LaunchLoop(674);
        return 1;
    case 200:
        LaunchLoop(671);
        return 1;
    case 201:
        LaunchLoop(672);
        return 1;
    default:
        ++m_launchCnt;
        return 1;
    }
}

void CSkill::Render(matrix* viewMatrix)
{
    if (m_3dactor == nullptr || m_isVisibleBody == 0 || viewMatrix == nullptr) {
        return;
    }
    m_3dactor->m_pos = m_pos;
    m_3dactor->UpdateMatrix();
    m_3dactor->Render(*viewMatrix);
}

void CSkill::SendMsg(CGameObject* sender, int message, msgparam_t arg1, msgparam_t arg2, msgparam_t arg3)
{
    switch (message) {
    case 37: {
        if (arg1 == 0) {
            return;
        }
        const SkillHitBlock* hit = reinterpret_cast<const SkillHitBlock*>(arg1);
        int count = hit->count != 0 ? hit->count : 1;
        const int perHit = hit->damage / count;
        CGameMode* gameMode = g_modeMgr.GetCurrentGameMode();
        CWorld* world = gameMode != nullptr ? gameMode->m_world : nullptr;
        CGameActor* target = nullptr;
        if (world != nullptr && hit->targetGid == g_session.m_aid) {
            target = world->m_player;
        } else {
            target = FindActorByGid(world, hit->targetGid);
        }
        if (target != nullptr) {
            WBA wba;
            wba.targetGid = m_aid;
            wba.time = timeGetTime();
            wba.tCellX = hit->cellX;
            wba.tCellY = hit->cellY;
            wba.effectId1 = UnitHitEffectId(hit->skillId);
            wba.effectLevel = hit->level;
            wba.attackedSpeedFactor = static_cast<float>(hit->attackedMT) * 0.0034722222f;
            wba.message = 26;
            wba.damage = perHit;
            wba.effectId2 = -1;
            int running = perHit;
            target->SendMsg(this, 27, reinterpret_cast<msgparam_t>(&wba), running, 0);
            for (int extra = count - 1; extra > 0; --extra) {
                running += perHit;
                wba.time += kDoubleAttackTerm;
                target->SendMsg(this, 27, reinterpret_cast<msgparam_t>(&wba), running, 0);
            }
        }
        if (m_3dactor != nullptr) {
            m_3dactor->m_animType = 1;
        }
        return;
    }
    case 45:
        if (arg1 != 0) {
            ApplyStandEntry(*reinterpret_cast<const SkillStandEntry*>(arg1));
        }
        return;
    case 47: {
        m_job = static_cast<int>(arg1);
        if (m_job == 128) {
            PlayUnitWave("effect\\EF_Portal.wav");
        } else if (m_job == 140 && m_effectId != -1) {
            CRagEffect* effect = LaunchEffect(m_effectId, vector3d{}, 0.0f);
            if (effect != nullptr) {
                effect->SendMsg(effect, 14, reinterpret_cast<msgparam_t>(&m_pos), 0, 0);
            }
        }
        m_launchCnt = 36000;
        ShowModel(TrapModelName());
        if (m_3dactor != nullptr) {
            m_3dactor->m_animType = 1;
        }
        return;
    }
    case 119:
        if (m_isVisibleBody == 0) {
            m_isVisibleBody = 1;
            ShowModel(TrapModelName());
        }
        return;
    default:
        CGameActor::SendMsg(sender, message, arg1, arg2, arg3);
        return;
    }
}
