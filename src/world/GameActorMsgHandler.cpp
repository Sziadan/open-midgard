#include "GameActor.h"
#include "RagEffect.h"
#include "World.h"
#include "audio/Audio.h"
#include "gamemode/GameMode.h"
#include "gamemode/Mode.h"
#include "DebugLog.h"
#include "res/ActRes.h"
#include "session/Session.h"
#include "session/SkillActionInfo.h"
#include "skill/Skill.h"

#include "RagEffectDurationTable.generated.inc"

#include <cmath>
#include <cstring>
#include <string>

namespace {

constexpr int kDoubleAttackTermMs = 200;
constexpr float kAttackedMotionFactor = 0.0034722222f;

bool IsPlayerJob(int job)
{
    return (job >= 0 && job < 28) || (job > 4000 && job < 6000);
}

CGameActor* FindActorByGid(u32 gid)
{
    if (gid == 0) {
        return nullptr;
    }
    if (g_world.m_player && (g_world.m_player->m_gid == gid || g_session.m_aid == gid)) {
        return g_world.m_player;
    }
    for (CGameActor* entry : g_world.m_actorList) {
        if (entry && entry->m_gid == gid) {
            return entry;
        }
    }
    return nullptr;
}

void FaceToward(CRenderObject& object, float x, float z)
{
    float rot = std::atan2(x - object.m_pos.x, -(z - object.m_pos.z)) / 3.14159265f * 180.0f;
    if (rot >= 360.0f) {
        rot -= 360.0f;
    }
    if (rot < 0.0f) {
        rot += 360.0f;
    }
    object.m_roty = rot;
}

void PlaySkillWaveAtPlayer(const char* waveName)
{
    PlayWave(waveName, 0.0f, 0.0f, 0.0f);
}

void PlaceGroundEffect(const vector3d& pos, int effectId)
{
    if (effectId < 0 || effectId >= kRagEffectDurationCount) {
        return;
    }
    CGameMode* gameMode = g_modeMgr.GetCurrentGameMode();
    if (!gameMode || !gameMode->m_world) {
        return;
    }
    gameMode->PlaceEffect(pos, effectId, kRagEffectDurationTable[effectId] + 10, 0);
}

CRagEffect* LaunchCounted(CAbleToMakeEffect& owner, int effectId, int count)
{
    if (effectId < 0) {
        return nullptr;
    }
    CRagEffect* effect = owner.LaunchEffect(effectId, vector3d{}, 0.0f);
    if (effect) {
        effect->SendMsg(effect, 44, count, 0, 0);
    }
    return effect;
}

int ResolveSkillAttackEffect(int skillId, int damage, int& effectId2)
{
    int effectId = -1;
    if (skillId >= 411 && skillId <= 472) {
        effectId = -1;
    } else if (skillId >= 493) {
        effectId = -1;
    }

    if (skillId == 129 || skillId == 347 || skillId == 397 || skillId == 654
        || (skillId >= 56 && skillId <= 59)
        || (skillId >= 184 && skillId <= 189)) {
        return -1;
    }
    switch (skillId) {
    case 7:
    case 17:
    case 19:
        return 49;
    case 14:
    case 15:
        return 51;
    case 20:
        return 52;
    case 54:
        if (damage != 0) {
            effectId2 = -1;
            return 152;
        }
        return effectId;
    case 90:
    case 91:
        return 147;
    case 136:
        return 0;
    case 190:
        return 180;
    case 467:
    case 468:
        return 549;
    default:
        return effectId;
    }
}

void ApplyGroundEffect(int skillId, int level, const vector3d& pos)
{
    switch (skillId) {
    case 21:
        PlaceGroundEffect(pos, 30);
        break;
    case 25:
        PlaceGroundEffect(pos, 141);
        break;
    case 69:
    case 694:
        PlaceGroundEffect(pos, 91);
        break;
    case 70:
        PlaceGroundEffect(pos, 83);
        break;
    case 79:
        PlaceGroundEffect(pos, 113);
        break;
    case 83:
        PlaceGroundEffect(pos, 92);
        break;
    case 85:
        PlaceGroundEffect(pos, 90);
        break;
    case 87:
        PlaceGroundEffect(pos, 74);
        break;
    case 89:
        PlaceGroundEffect(pos, 89);
        break;
    case 91:
        PlaceGroundEffect(pos, 142);
        break;
    case 92:
        PlaceGroundEffect(pos, 95);
        break;
    case 110:
        PlaceGroundEffect(pos, 102);
        break;
    case 111:
    case 459:
        PlaceGroundEffect(pos, 98);
        break;
    case 115:
        PlaceGroundEffect(pos, 69);
        break;
    case 130:
        PlaceGroundEffect(pos, 119);
        break;
    case 140:
        PlaceGroundEffect(pos, 124);
        break;
    case 285:
        PlaceGroundEffect(pos, 225);
        break;
    case 286:
        PlaceGroundEffect(pos, 236);
        break;
    case 287:
        PlaceGroundEffect(pos, 237);
        break;
    case 288:
        PlaceGroundEffect(pos, 238);
        break;
    case 478:
        PlayWave("assulter_attack.wav", 0.0f, -150.0f, 0.0f);
        if (level >= 10) {
            PlaceGroundEffect(pos, 499);
        } else if (level >= 6) {
            PlaceGroundEffect(pos, 498);
        } else {
            PlaceGroundEffect(pos, 497);
        }
        break;
    case 483:
        PlaySkillWaveAtPlayer("effect\\EF_FireWall.wav");
        PlaceGroundEffect(pos, 223);
        break;
    default:
        break;
    }
}

} // namespace

void CGameActor::Am_Action(const int* arg1)
{
    if (!arg1) {
        return;
    }

    const int action = arg1[2];
    const u32 targetGid = static_cast<u32>(arg1[3]);
    int count = arg1[5];
    const int cellX = arg1[6];
    const int cellY = arg1[7];
    const int attackMT = arg1[8];
    const int attackedMT = arg1[9];
    const int totalDamage = arg1[0];
    m_targetGid = targetGid;

    CGameMode* gameMode = g_modeMgr.GetCurrentGameMode();
    CWorld* world = gameMode ? gameMode->m_world : nullptr;
    CGameActor* player = world ? world->m_player : nullptr;
    CGameActor* target = nullptr;
    if (world) {
        if (g_session.m_aid == targetGid) {
            target = player;
        } else {
            target = world->GetGameActorByAID(targetGid);
            if (!target) {
                target = world->GetSkillByAID(targetGid);
            }
        }
    }
    vector3d targetPos = m_pos;
    if (target) {
        targetPos = target->m_pos;
    } else if (player) {
        const vector3d& playerPos = player->m_pos;
        targetPos.x = m_pos.x - (playerPos.x - m_pos.x);
        targetPos.z = m_pos.z - (playerPos.z - m_pos.z);
    }

    if (count <= 0 || count > 36) {
        count = 1;
    }
    if (action != 8 && action != 9) {
        count = 1;
    }
    const int perHit = count > 0 ? totalDamage / count : totalDamage;

    switch (action) {
    case 0:
    case 4:
    case 8:
    case 9:
    case 10:
    case 11:
        break;
    case 1:
        SetState(5);
        return;
    case 2:
        SetState(6);
        return;
    case 3:
        SetState(0);
        return;
    default:
        DbgLog("Unknown Action Type == %d\n", action);
        return;
    }

    if (m_isCounter) {
        if (CRagEffect* counter = LaunchEffect(131, vector3d{}, 0.0f)) {
            counter->SendMsg(this, 50, 0, 0, 0);
        }
        m_isCounter = 0;
    }

    SetModifyFactorOfmotionSpeed(attackMT);
    if (targetGid != m_gid) {
        SetState(2);
    }
    double rotation = std::atan2(static_cast<double>(targetPos.x) - m_pos.x,
        -(static_cast<double>(targetPos.z) - m_pos.z)) / 3.141592 * 180.0;
    m_roty = static_cast<float>(rotation);
    if (rotation >= 360.0) {
        m_roty = static_cast<float>(rotation - 360.0);
    }
    if (m_roty < 0.0) {
        m_roty += 360.0;
    }

    u32 delay = static_cast<u32>(m_attackMotion * m_motionSpeed * 24.0f);
    if (m_job == 1016 || m_job == 1420) {
        delay += 192;
    } else if (m_job == 1285 || m_job == 1830) {
        delay += 912;
    } else if (m_job == 1286 || m_job == 1829 || m_job == 1287) {
        delay += 408;
    }

    int message = 26;
    int effectId1 = -1;
    switch (action) {
    case 4:
    case 9:
        message = 39;
        break;
    case 10:
        message = 93;
        effectId1 = 1;
        break;
    case 11:
        message = m_isCounter ? 26 : 94;
        break;
    default:
        break;
    }

    if (!target) {
        return;
    }

    int effectId2 = -2;
    if (targetGid != m_gid) {
        effectId2 = (m_job >= 4046 && m_job <= 4048) ? 434 : 0;
    }

    WBA hit{};
    hit.time = delay + timeGetTime();
    hit.effectId1 = effectId1;
    hit.targetGid = m_gid;
    hit.tCellY = cellY;
    hit.message = message;
    hit.tCellX = cellX;
    hit.effectId2 = effectId2;
    hit.effectLevel = 1;
    hit.damage = perHit;
    hit.attackedSpeedFactor = static_cast<float>(static_cast<double>(attackedMT) * 0.0034722222);

    if (count == 1) {
        target->SendMsg(this, 27, reinterpret_cast<msgparam_t>(&hit), 0, 0);
    } else {
        target->SendMsg(this, 27, reinterpret_cast<msgparam_t>(&hit), perHit, 0);
    }

    const int leftDamage = arg1[1];
    if (leftDamage && (m_job < 1000 || m_job > 4000)) {
        int running = perHit;
        if (count > 1) {
            const int step = kDoubleAttackTermMs / 2;
            for (int extra = count - 1; extra > 0; --extra) {
                running += perHit;
                hit.time += static_cast<u32>(step);
                hit.damage = perHit;
                target->SendMsg(this, 27, reinterpret_cast<msgparam_t>(&hit), running, 0);
                hit.time -= static_cast<u32>(step);
            }
        }
        hit.message = message;
        hit.damage = leftDamage;
        hit.time += static_cast<u32>(kDoubleAttackTermMs / 4 + kDoubleAttackTermMs + kDoubleAttackTermMs / 2);
        if (count == 1) {
            target->SendMsg(this, 27, reinterpret_cast<msgparam_t>(&hit), 0, 0);
        } else {
            target->SendMsg(this, 27, reinterpret_cast<msgparam_t>(&hit), leftDamage + running, 0);
        }
    } else if (count > 1) {
        int running = perHit;
        for (int extra = count - 1; extra > 0; --extra) {
            running += perHit;
            hit.time += static_cast<u32>(kDoubleAttackTermMs);
            hit.damage = perHit;
            target->SendMsg(this, 27, reinterpret_cast<msgparam_t>(&hit), running, 0);
        }
    }
}

void CGameActor::Am_Skill(const int* arg1)
{
    if (!arg1) {
        return;
    }

    const int skillId = arg1[0];
    const int totalDamage = arg1[1];
    int action = arg1[2];
    const u32 targetGid = static_cast<u32>(arg1[3]);
    int level = arg1[5];
    int count = arg1[6];
    const int cellX = arg1[7];
    const int cellY = arg1[8];
    const int attackMT = arg1[9];
    const int attackedMT = arg1[10];

    if (count <= 0 || count > 36) {
        count = 1;
    }
    if (skillId == 56) {
        level = count;
    }
    m_targetGid = targetGid;
    const int damage = count > 0 ? totalDamage / count : totalDamage;

    CGameActor* target = FindActorByGid(targetGid);
    if (target && (skillId == 129 || skillId == 381) && m_birdEffect) {
        m_birdEffect->SendMsg(nullptr, 29, reinterpret_cast<msgparam_t>(&target->m_pos), 0, 0);
    }

    vector3d targetPos = m_pos;
    if (target) {
        targetPos = target->m_pos;
    } else if (g_world.m_player) {
        const vector3d& playerPos = g_world.m_player->m_pos;
        targetPos.x = m_pos.x - (playerPos.x - m_pos.x);
        targetPos.y = m_pos.y - (playerPos.y - m_pos.y);
        targetPos.z = m_pos.z - (playerPos.z - m_pos.z);
    }

    if (action < 8 || action > 9) {
        count = 1;
    }

    if (action == 5 && target && skillId != 47 && skillId != 11 && skillId != 413 && skillId != 273) {
        WBA hit{};
        hit.time = timeGetTime() + static_cast<u32>(m_attackMotion * m_motionSpeed * 24.0f);
        hit.targetGid = m_gid;
        hit.damage = damage;
        hit.effectId1 = -1;
        hit.effectId2 = -1;
        hit.tCellX = cellX;
        hit.tCellY = cellY;
        hit.effectLevel = level;
        hit.attackedSpeedFactor = static_cast<float>(attackedMT) * kAttackedMotionFactor;
        hit.message = 96;
        target->SendMsg(this, 27, reinterpret_cast<msgparam_t>(&hit), 0, 0);
        return;
    }

    switch (action) {
    case 4:
    case 5:
    case 6:
    case 8:
    case 9:
        break;
    default:
        DbgLog("Unknown Action Type == %d\n", action);
        return;
    }

    int beginEffectId = 16;
    int skillMotionType = 7;
    GetSkillActionInfo(skillId, beginEffectId, skillMotionType);
    SetModifyFactorOfmotionSpeed(attackMT);
    if (skillId != 143 && skillId != 263 && skillId != 272 && skillId != 273 && skillId != 271 && skillId != 371) {
        SetState(skillMotionType);
    }
    if (SKILL_INFO::IsSkillNotDelayed(skillId)) {
        LaunchCounted(*this, beginEffectId, count);
    }
    g_session.LaunchSkillSpecialEffect(skillId, count, this, target);
    FaceToward(*this, targetPos.x, targetPos.z);

    u32 delay = static_cast<u32>(m_attackMotion * m_motionSpeed * 24.0f);
    if (m_job == 1016 || m_job == 1420) {
        delay += 192;
    }
    if (!target) {
        return;
    }

    const int attackEffectId = g_session.GetAttackEffectIdBySkillId(skillId);
    int effectId2 = g_session.GetAttackedEffectIdBySkillId(skillId);
    if (effectId2 == 186) {
        switch (level) {
        case 2: effectId2 = 187; break;
        case 3: effectId2 = 188; break;
        case 4: effectId2 = 189; break;
        case 5: effectId2 = 190; break;
        default: break;
        }
    }
    if (CRagEffect* attackEffect = LaunchCounted(*this, attackEffectId, count)) {
        attackEffect->SendMsg(attackEffect, 14, reinterpret_cast<msgparam_t>(&targetPos), 0, 0);
    }

    if (skillId == 170) {
        action = 10;
    }
    const int effectId1 = ResolveSkillAttackEffect(skillId, damage, effectId2);

    int message = 96;
    switch (action) {
    case 4:
    case 9:
        message = 39;
        break;
    case 6:
        message = 97;
        break;
    case 10:
        message = 93;
        break;
    default:
        break;
    }

    WBA hit{};
    hit.damage = damage;
    hit.effectId2 = effectId2;
    hit.message = message;
    hit.tCellX = cellX;
    hit.effectId1 = effectId1;
    hit.attackedSpeedFactor = static_cast<float>(attackedMT) * kAttackedMotionFactor;
    hit.effectLevel = level;
    hit.time = delay + timeGetTime();
    hit.targetGid = m_gid;
    hit.tCellY = cellY;
    if (skillId == 77) {
        hit.time += 500;
    }

    const int savedMessage = message;
    if (skillId == 413 || skillId == 419 || skillId == 421) {
        target->SendMsg(this, 131, reinterpret_cast<msgparam_t>(&hit), damage, 0);
    } else if (skillId == 415 || skillId == 417) {
        hit.time += 300;
        target->SendMsg(this, 131, reinterpret_cast<msgparam_t>(&hit), damage, 0);
    } else if (skillId == 271 || skillId == 379) {
        target->SendMsg(this, 93, reinterpret_cast<msgparam_t>(&hit), damage, 0);
    } else if (skillId == 273) {
        target->LaunchEffect(704, vector3d{}, 0.0f);
        target->SendMsg(this, 27, reinterpret_cast<msgparam_t>(&hit), damage, 0);
    } else if (count == 1 && skillId != 371 && skillId != 199) {
        target->SendMsg(this, 27, reinterpret_cast<msgparam_t>(&hit), 0, 0);
    } else {
        target->SendMsg(this, 27, reinterpret_cast<msgparam_t>(&hit), damage, 0);
    }

    hit.effectId2 = -1;
    switch (skillId) {
    case 263:
    case 271:
        SetState(2);
        break;
    case 266:
    case 267:
        m_forceAnimSpeed = 10;
        SetForceAnimation(96, 3, 4, 96, 4, 1, 0x44Cu, 0);
        break;
    case 272:
        SetForceAnimation(96, 5, 5, 96, 5, 1, g_session.GetForceAnimEndTick(m_gid), 0);
        target->LaunchEffect(262, vector3d{}, 0.0f);
        LaunchEffect(263, vector3d{}, 0.0f);
        target->LaunchEffect(273, vector3d{}, 0.0f);
        break;
    case 273:
        m_forceAnimSpeed = 4;
        SetForceAnimation(80, 3, 4, 80, 4, 1, timeGetTime() + 100, 0);
        if (CGameMode* gameMode = g_modeMgr.GetCurrentGameMode()) {
            gameMode->SendMsg(135, 0, 0, 0);
        }
        PlaySkillWaveAtPlayer("effect\\mon_\xB8\xCD\xB7\xE6\xB0\xFA\xB0\xAD.wav");
        LaunchEffect(263, vector3d{}, 0.0f);
        LaunchEffect(330, vector3d{}, 0.0f);
        break;
    case 370:
        SetState(2);
        if (CGameMode* gameMode = g_modeMgr.GetCurrentGameMode()) {
            gameMode->SendMsg(135, 0, 0, 0);
        }
        PlaySkillWaveAtPlayer("effect\\\xB8\xCD\xC8\xA3\xB0\xE6\xC6\xC4\xBB\xEA.wav");
        target->LaunchEffect(376, vector3d{}, 0.0f);
        break;
    case 371:
        SetState(2);
        if (CGameMode* gameMode = g_modeMgr.GetCurrentGameMode()) {
            gameMode->SendMsg(135, 0, 0, 0);
        }
        LaunchEffect(377, vector3d{}, 0.0f);
        LaunchEffect(263, vector3d{}, 0.0f);
        break;
    case 372:
        LaunchEffect(263, vector3d{}, 0.0f);
        if (CRagEffect* slash = LaunchEffect(512, vector3d{}, 0.0f)) {
            vector3d aim = target->m_pos;
            slash->SendMsg(slash, 14, reinterpret_cast<msgparam_t>(&aim), 0, 0);
        }
        break;
    case 413:
        target->LaunchEffect(435, vector3d{}, 0.0f);
        break;
    default:
        break;
    }

    if (action != 6 && action != 8 && action != 9) {
        return;
    }
    int running = damage;
    for (int extra = 0; extra < count - 1; ++extra) {
        if (skillId == 56) {
            hit.time += 80;
        }
        hit.effectId2 = -1;
        hit.time += static_cast<u32>(kDoubleAttackTermMs);
        running += damage;
        hit.damage = damage;
        if (skillId == 136) {
            hit.message = 108;
            hit.effectId2 = 122;
            SendMsg(this, 27, reinterpret_cast<msgparam_t>(&hit), 0, 0);
            if (extra == 2) {
                hit.effectId1 = 1;
            }
            hit.message = savedMessage;
            target->SendMsg(this, 27, reinterpret_cast<msgparam_t>(&hit), running, 0);
            continue;
        }
        if (skillId == 272) {
            hit.effectId2 = 122;
        } else if (skillId == 372 || skillId == 394) {
            hit.message = 108;
            SendMsg(this, 27, reinterpret_cast<msgparam_t>(&hit), 0, 0);
        }
        hit.message = savedMessage;
        if (skillId == 413 || skillId == 415 || skillId == 417 || skillId == 419 || skillId == 421) {
            target->SendMsg(this, 134, reinterpret_cast<msgparam_t>(&hit), running, 0);
        } else {
            target->SendMsg(this, 27, reinterpret_cast<msgparam_t>(&hit), running, 0);
        }
    }
}

void CGameActor::Am_Groundskill(const int* arg1)
{
    if (!arg1) {
        return;
    }

    const int skillId = arg1[0];
    const int cellX = arg1[2];
    const int cellY = arg1[3];
    const int level = arg1[4];

    int beginEffectId = 16;
    int skillMotionType = 7;
    GetSkillActionInfo(skillId, beginEffectId, skillMotionType);

    CGameMode* gameMode = g_modeMgr.GetCurrentGameMode();
    float destX = m_pos.x;
    float destZ = m_pos.z;
    if (gameMode) {
        gameMode->GetClientCoor(cellX, cellY, destX, destZ);
    }
    float destY = 0.0f;
    if (gameMode && gameMode->m_world && gameMode->m_world->m_attr) {
        destY = gameMode->m_world->m_attr->GetHeight(destX, destZ);
    }
    const vector3d dest{ destX, destY, destZ };
    m_targetGid = 0;

    if (skillId == 83) {
        m_curAction = 88;
        if (m_actRes) {
            const int motions = m_actRes->GetMotionCount(88);
            if (motions > 0) {
                m_curMotion = motions - 1;
            }
        }
        ApplyGroundEffect(skillId, level, dest);
        return;
    }

    FaceToward(*this, destX, destZ);
    SetState(skillMotionType);

    if (skillId == 484) {
        PlaySkillWaveAtPlayer("effect\\wizard_earthspike.wav");
        return;
    }
    if (skillId == 491) {
        LaunchEffect(58, vector3d{}, 0.0f);
        return;
    }
    if (skillId == 521) {
        PlaySkillWaveAtPlayer("effect\\\xBC\xF6\xB5\xD0.wav");
        return;
    }
    if (skillId == 538) {
        PlaySkillWaveAtPlayer("effect\\\xB1\xD7\xB6\xF3\xBF\xEE\xB5\xE5.wav");
        return;
    }
    if (skillId == 264) {
        if (IsPlayerJob(m_job)) {
            if (CRagEffect* dash = LaunchEffect(166, vector3d{}, 0.0f)) {
                dash->SendMsg(dash, 44, 0xFFFF, 243, 100);
            }
        }
        SendMsg(this, 19, cellX, cellY, 15);
        SetAction(m_baseAction + 80, 6, 1);
        m_curMotion = 7;
        m_isMotionFreezed = 1;
        return;
    }
    if (skillId == 130) {
        ApplyGroundEffect(skillId, level, dest);
        if (m_birdEffect) {
            m_birdEffect->SendMsg(nullptr, 29, reinterpret_cast<msgparam_t>(&dest), 0, 0);
        }
        return;
    }

    ApplyGroundEffect(skillId, level, dest);
}

namespace {

int g_isPoison = 0;
int g_isBlackWorld = 0;

constexpr int kSightBit = 0x0001;
constexpr int kBurrowBit = 0x0002;
constexpr int kHidingBit = 0x0004;
constexpr int kBirdBit = 0x0010;
constexpr int kChickenBit = 0x0020;
constexpr int kCloakBit = 0x0040;
constexpr int kCartBits = 0x0008 | 0x0080 | 0x0100 | 0x0200 | 0x0400;
constexpr int kStar2Bit = 0x8000;
constexpr int kRuwachBit = 0x2000;

bool HasBit(int state, int mask)
{
    return (state & mask) != 0;
}

bool BitChanged(int state, int oldState, int mask)
{
    return ((state ^ oldState) & mask) != 0;
}

void PlayStateWave(const CGameActor& actor, const char* waveName)
{
    if (!g_world.m_player) {
        return;
    }
    const vector3d& listener = g_world.m_player->m_pos;
    PlayWave(waveName, actor.m_pos.x - listener.x, actor.m_pos.y - listener.y,
        actor.m_pos.z - listener.z);
}

bool HasLaunchedEffect(const CGameActor& actor, int effectId)
{
    for (const CRagEffect* effect : actor.m_effectList) {
        if (effect && effect->GetEffectType() == effectId) {
            return true;
        }
    }
    return false;
}

void TintActor(CGameActor& actor, u8 red, u8 green, u8 blue)
{
    actor.m_isSprArgbFixed = 1;
    actor.m_u28.m_sprColor.u.s.a = 255;
    actor.m_u28.m_sprColor.u.s.r = red;
    actor.m_u28.m_sprColor.u.s.g = green;
    actor.m_u28.m_sprColor.u.s.b = blue;
}

void ClearActorTint(CGameActor& actor)
{
    actor.m_isSprArgbFixed = 0;
    actor.m_u28.m_sprColor.u.s.a = 255;
    actor.m_u28.m_sprColor.u.s.r = 255;
    actor.m_u28.m_sprColor.u.s.g = 255;
    actor.m_u28.m_sprColor.u.s.b = 255;
}

} // namespace

void CGameActor::SetAttrState(int bodyState, int effectState, int healthState, bool changedNow)
{
    const int oldEffectState = m_effectState;
    const int oldBodyState = m_bodyState;
    m_bodyState = bodyState;
    m_effectState = effectState;
    m_healthState = healthState;
    const bool localPlayer = m_gid == g_session.m_aid;

    if (BitChanged(effectState, oldEffectState, kSightBit) && HasBit(effectState, kSightBit)) {
        PlayStateWave(*this, "effect\\EF_Sight.wav");
    }
    if (BitChanged(effectState, oldEffectState, kRuwachBit) && HasBit(effectState, kRuwachBit)) {
        PlayStateWave(*this, "effect\\EF_Sight.wav");
    }

    if (BitChanged(effectState, oldEffectState, kBurrowBit)) {
        if (HasBit(effectState, kBurrowBit)) {
            if (changedNow) {
                SendMsg(this, 63, 0, 0, 0);
                if (m_job != 25) {
                    LaunchEffect(16, vector3d{}, 0.0f);
                }
            }
            m_isVisibleBody = 0;
            if (!localPlayer) {
                m_shadowOn = 0;
                SendMsg(this, 109, 159, 0, 0);
                SendMsg(this, 35, 0, 0, 0);
            }
            if (HasBit(effectState, kCartBits)) {
                SendMsg(this, 52, 0, 0, 0);
            }
            if (HasBit(effectState, kBirdBit)) {
                SendMsg(this, 102, 0, 0, 0);
            }
        } else {
            if (changedNow && g_modeMgr.GetCurrentGameMode()) {
                SendMsg(this, 63, 0, 0, 0);
                g_modeMgr.GetCurrentGameMode()->m_waitingUseItemAck = 0;
            }
            m_isVisibleBody = 1;
            if (!localPlayer) {
                m_shadowOn = 1;
            }
            if (HasBit(effectState, kCartBits)) {
                SendMsg(this, 51, effectState, 0, 0);
            }
            if (HasBit(effectState, kBirdBit)) {
                SendMsg(this, 101, 0, 0, 0);
            }
        }
    }

    if (BitChanged(effectState, oldEffectState, kHidingBit)) {
        if (HasBit(effectState, kHidingBit)) {
            m_isVisibleBody = 0;
            m_isVisible = 0;
            if (!localPlayer) {
                m_shadowOn = 0;
                SendMsg(this, 35, 0, 0, 0);
                SendMsg(this, 109, 159, 0, 0);
            }
            if (HasBit(effectState, kCartBits)) {
                SendMsg(this, 52, 0, 0, 0);
            }
            if (HasBit(effectState, kBirdBit)) {
                SendMsg(this, 102, 0, 0, 0);
            }
        } else {
            m_isVisibleBody = 1;
            m_isVisible = 1;
            if (!localPlayer) {
                m_shadowOn = 1;
            }
            if (HasBit(effectState, kCartBits)) {
                SendMsg(this, 51, 0, 0, 0);
            }
            if (HasBit(effectState, kBirdBit)) {
                SendMsg(this, 101, 0, 0, 0);
            }
        }
    }

    if (BitChanged(effectState, oldEffectState, kCartBits)) {
        SendMsg(this, 52, 0, 0, 0);
        if (HasBit(effectState, kCartBits)) {
            SendMsg(this, 51, effectState, 0, 0);
        }
    }
    if (BitChanged(effectState, oldEffectState, kBirdBit)) {
        SendMsg(this, HasBit(effectState, kBirdBit) ? 101 : 102, 0, 0, 0);
    }
    if (BitChanged(effectState, oldEffectState, kChickenBit) || BitChanged(effectState, oldEffectState, kStar2Bit)) {
        SendMsg(this, HasBit(effectState, kChickenBit) || HasBit(effectState, kStar2Bit) ? 105 : 106, 0, 0, 0);
    }

    if (BitChanged(effectState, oldEffectState, kCloakBit)) {
        if (HasBit(effectState, kCloakBit)) {
            m_isVisibleBody = 0;
            m_isVisible = 0;
            m_shadowOn = 0;
            SendMsg(this, 109, 159, 0, 0);
            if (HasBit(effectState, kCartBits)) {
                SendMsg(this, 52, 0, 0, 0);
            }
            if (HasBit(effectState, kBirdBit)) {
                SendMsg(this, 102, 0, 0, 0);
            }
        } else {
            m_isVisibleBody = 1;
            m_isVisible = 1;
            m_shadowOn = 1;
            if (HasBit(effectState, kCartBits)) {
                SendMsg(this, 51, 0, 0, 0);
            }
            if (HasBit(effectState, kBirdBit)) {
                SendMsg(this, 101, 0, 0, 0);
            }
        }
    }

    if (oldBodyState != m_bodyState) {
        switch (oldBodyState) {
        case 1:
            PlayStateWave(*this, "_stone_explosion.wav");
            if (CGameMode* gameMode = g_modeMgr.GetCurrentGameMode()) {
                gameMode->m_waitingUseItemAck = 0;
            }
            break;
        case 2:
            SendMsg(this, 59, 0, 0, 0);
            SendMsg(this, 62, 0, 0, 0);
            PlayStateWave(*this, "_frozen_explosion.wav");
            m_isMotionFreezed = 0;
            m_isSprArgbFixed = 0;
            m_isMotionFinished = 1;
            ClearActorTint(*this);
            if (CGameMode* gameMode = g_modeMgr.GetCurrentGameMode()) {
                gameMode->m_waitingUseItemAck = 0;
            }
            break;
        case 3:
            SendMsg(this, 72, 0, 0, 0);
            if (CGameMode* gameMode = g_modeMgr.GetCurrentGameMode()) {
                gameMode->m_waitingUseItemAck = 0;
            }
            break;
        case 4:
            SendMsg(this, 70, 0, 0, 0);
            if (CGameMode* gameMode = g_modeMgr.GetCurrentGameMode()) {
                gameMode->m_waitingUseItemAck = 0;
            }
            break;
        default:
            break;
        }

        switch (m_bodyState) {
        case 1:
            m_isMotionFreezed = 1;
            m_isMotionFinished = 1;
            TintActor(*this, 64, 64, 64);
            break;
        case 2:
            SendMsg(this, 58, 0, 0, 0);
            m_isMotionFreezed = 1;
            m_isMotionFinished = 1;
            TintActor(*this, 0, 128, 255);
            if (changedNow) {
                PlayStateWave(*this, "_stonecurse.wav");
            }
            break;
        case 3:
            SendMsg(this, 71, 0, 0, 0);
            if (changedNow) {
                PlayStateWave(*this, "_stun.wav");
            }
            break;
        case 4:
            SendMsg(this, 69, 0, 0, 0);
            break;
        case 6:
            TintActor(*this, 128, 128, 128);
            break;
        default:
            break;
        }
    }

    if (HasBit(m_healthState, 0x80) || HasBit(m_healthState, 0x01)) {
        if (localPlayer) {
            g_isPoison = 1;
        }
        TintActor(*this, 255, HasBit(m_healthState, 0x80) ? 50 : 128, 255);
        if (changedNow) {
            PlayStateWave(*this, "_poison.wav");
            if (localPlayer && !HasLaunchedEffect(*this, 335)) {
                LaunchEffect(335, vector3d{}, 0.0f);
            }
        }
    } else if (localPlayer) {
        if (g_isPoison == 1) {
            SendMsg(this, 109, 335, 0, 0);
        }
        g_isPoison = 0;
    }

    if (HasBit(m_healthState, 0x02)) {
        SendMsg(this, 67, 0, 0, 0);
        TintActor(*this, 200, 50, 50);
        if (changedNow) {
            PlayStateWave(*this, "_curse.wav");
        }
    } else {
        SendMsg(this, 68, 0, 0, 0);
    }

    if (HasBit(m_healthState, 0x04)) {
        if (changedNow) {
            PlayStateWave(*this, "_silence.wav");
        }
        SendMsg(this, 73, 0, 0, 0);
    } else {
        SendMsg(this, 74, 0, 0, 0);
    }

    SendMsg(this, HasBit(m_healthState, 0x20) ? 75 : 76, 0, 0, 0);

    if (HasBit(m_healthState, 0x08) && changedNow) {
        PlayStateWave(*this, "_confusion.wav");
    }

    if (HasBit(m_healthState, 0x10)) {
        if (localPlayer) {
            g_isBlackWorld = 1;
        }
        if (changedNow) {
            PlayStateWave(*this, "_blind.wav");
            if (localPlayer && !HasLaunchedEffect(*this, 334)) {
                LaunchEffect(334, vector3d{}, 0.0f);
            }
        }
    } else if (localPlayer) {
        if (g_isBlackWorld == 1) {
            SendMsg(this, 109, 334, 0, 0);
        }
        g_isBlackWorld = 0;
    }

    if (m_bodyState == 0) {
        m_isMotionFreezed = 0;
    }
    if ((m_healthState & 0x83) == 0 && m_bodyState != 1 && m_bodyState != 2 && m_bodyState != 6) {
        ClearActorTint(*this);
    }
}
