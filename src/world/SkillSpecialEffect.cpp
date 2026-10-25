// Ref CSession::LaunchSkillSpecialEffect — Ref/Session2.cpp:6673 (0x67B250).
#include "session/Session.h"

#include "audio/Audio.h"
#include "gamemode/GameMode.h"
#include "gamemode/Mode.h"
#include "world/GameActor.h"
#include "world/RagEffect.h"
#include "world/World.h"

#include <cstdio>
#include <cstring>

namespace {

constexpr const char* kAsuraWave = "effect\\mon_\xBE\xC6\xBC\xF6\xB6\xF3 \xC6\xD0\xC8\xB2\xB1\xC7.wav";
constexpr const char* kNinjaThrowWave = "effect\\\xB4\xD1\xC0\xDA_\xB4\xF8\xC1\xF6\xB1\xE2.wav";
constexpr const char* kFireWallWave = "effect\\EF_FireWall.wav";
constexpr const char* kStoneCurseWave = "_stonecurse.wav";
constexpr const char* kAmonRaDieWave = "amon_ra_die01.wav";
constexpr const char* kWideSleepWave = "effect\\widesleep.wav";

void PlaySkillWave(const char* name)
{
    PlayWave(name, 0.0f, 0.0f, 0.0f);
}

CRagEffect* LaunchOn(CGameActor* actor, int effectId)
{
    if (actor == nullptr) {
        return nullptr;
    }
    return actor->LaunchEffect(effectId);
}

void AimEffect(CRagEffect* effect, const vector3d& pos)
{
    if (effect == nullptr) {
        return;
    }
    effect->SendMsg(nullptr, 14, reinterpret_cast<msgparam_t>(&pos), 0, 0);
}

void SetEffectCount(CRagEffect* effect, int count)
{
    if (effect == nullptr) {
        return;
    }
    effect->SendMsg(nullptr, 44, count, 0, 0);
}

CRagEffect* LaunchAimed(CGameActor* actor, int effectId, const vector3d& pos)
{
    CRagEffect* effect = LaunchOn(actor, effectId);
    AimEffect(effect, pos);
    return effect;
}

void SetEffectDuration(CRagEffect* effect, int duration)
{
    if (effect == nullptr) {
        return;
    }
    effect->SendMsg(nullptr, 80, duration, 0, 0);
}

CGameActor* LocalPlayer()
{
    CGameMode* gameMode = g_modeMgr.GetCurrentGameMode();
    if (gameMode == nullptr || gameMode->m_world == nullptr) {
        return nullptr;
    }
    return gameMode->m_world->m_player;
}

void ShakeCamera()
{
    CGameMode* gameMode = g_modeMgr.GetCurrentGameMode();
    if (gameMode != nullptr) {
        gameMode->SendMsg(135, 0, 0, 0);
    }
}

} // namespace

void CSession::LaunchSkillSpecialEffect(int skillId, int level, CGameActor* source, CGameActor* target)
{
    if (source == nullptr || target == nullptr) {
        return;
    }

    switch (skillId) {
    case 14:
        SetEffectCount(LaunchOn(target, 26), level);
        break;
    case 15:
        AimEffect(LaunchOn(source, 27), target->m_pos);
        break;
    case 17:
        AimEffect(LaunchOn(source, 24), target->m_pos);
        break;
    case 19:
        SetEffectCount(LaunchOn(target, 31), level);
        break;
    case 20:
        SetEffectCount(LaunchOn(target, 29), level);
        break;
    case 21:
        LaunchOn(target, 30);
        break;
    case 28:
        LaunchOn(target, 320);
        break;
    case 47: {
        CGameMode* gameMode = g_modeMgr.GetCurrentGameMode();
        if (gameMode == nullptr || gameMode->m_world == nullptr || gameMode->m_world->m_attr == nullptr) {
            break;
        }
        gameMode->MakeArrowEffectPos(source->m_pos, target->m_pos, 18.0f);
        int cellX = 0;
        int cellY = 0;
        gameMode->GetServerCoor(target->m_pos.x, target->m_pos.z, cellX, cellY);
        // Eight cells around the target. Order matches the qword table in Ref/Session2.cpp:7364.
        static const int kNeighborCells[8][2] = {
            { 1, 0 },
            { 1, 1 },
            { 1, -1 },
            { -1, 0 },
            { -1, 1 },
            { -1, -1 },
            { 0, 1 },
            { 0, -1 },
        };
        for (int index = 0; index < 8; ++index) {
            float cellClientX = 0.0f;
            float cellClientZ = 0.0f;
            gameMode->GetClientCoor(cellX + kNeighborCells[index][0],
                cellY + kNeighborCells[index][1],
                cellClientX,
                cellClientZ);
            const vector3d cellPos{
                cellClientX,
                gameMode->m_world->m_attr->GetHeight(cellClientX, cellClientZ),
                cellClientZ,
            };
            gameMode->MakeArrowEffectPos(source->m_pos, cellPos, 18.0f);
        }
        break;
    }
    case 56: {
        CRagEffect* effect = LaunchOn(source, 148);
        SetEffectCount(effect, level);
        AimEffect(effect, source->m_pos);
        break;
    }
    case 57:
        AimEffect(LaunchOn(target, 70), target->m_pos);
        break;
    case 58:
        AimEffect(LaunchOn(source, 150), target->m_pos);
        break;
    case 59:
        AimEffect(LaunchOn(source, 80), target->m_pos);
        AimEffect(LaunchOn(source, 151), source->m_pos);
        break;
    case 62:
        AimEffect(LaunchOn(source, 149), source->m_pos);
        break;
    case 77:
        AimEffect(LaunchOn(source, 82), target->m_pos);
        break;
    case 84:
    case 341:
        AimEffect(LaunchOn(source, 93), target->m_pos);
        break;
    case 86:
        AimEffect(LaunchOn(source, 117), target->m_pos);
        break;
    case 90:
        AimEffect(LaunchOn(target, 79), target->m_pos);
        break;
    case 136:
        AimEffect(LaunchOn(source, 121), target->m_pos);
        AimEffect(LaunchOn(source, 143), source->m_pos);
        break;
    case 137:
        AimEffect(LaunchOn(source, 123), target->m_pos);
        break;
    case 149:
        LaunchOn(target, 310);
        break;
    case 152:
        AimEffect(LaunchOn(source, 308), target->m_pos);
        break;
    case 153:
        LaunchOn(source, 170);
        break;
    case 160:
        AimEffect(LaunchOn(source, 80), target->m_pos);
        break;
    case 212:
        AimEffect(LaunchOn(source, 275), target->m_pos);
        break;
    case 219:
        AimEffect(LaunchOn(LocalPlayer(), 227), source->m_pos);
        break;
    case 230:
        AimEffect(LaunchOn(source, 298), target->m_pos);
        break;
    case 231:
        SetEffectCount(LaunchAimed(source, 299, target->m_pos), level);
        source->SetState(7);
        break;
    case 250:
        LaunchOn(target, 246);
        break;
    case 251:
        AimEffect(LaunchOn(source, 249), target->m_pos);
        break;
    case 253:
    case 338:
        LaunchOn(target, 245);
        break;
    case 254:
        LaunchOn(source, 226);
        break;
    case 263:
        AimEffect(LaunchOn(source, 329), target->m_pos);
        break;
    case 266:
        AimEffect(LaunchOn(source, 264), target->m_pos);
        AimEffect(LaunchOn(source, 267), target->m_pos);
        break;
    case 267:
        SetEffectCount(LaunchAimed(source, 265, target->m_pos), level);
        break;
    case 271:
        ShakeCamera();
        PlaySkillWave(kAsuraWave);
        LaunchOn(target, 266);
        break;
    case 337:
        AimEffect(LaunchOn(source, 494), target->m_pos);
        break;
    case 339:
        LaunchOn(source, 450);
        break;
    case 344:
        LaunchOn(target, 271);
        break;
    case 345:
        LaunchOn(target, 272);
        break;
    case 346:
        LaunchOn(target, 270);
        break;
    case 365:
        LaunchOn(target, 380);
        break;
    case 367:
        LaunchOn(target, 365);
        break;
    case 368:
        LaunchOn(source, 366);
        break;
    case 379:
        AimEffect(LaunchOn(source, 361), target->m_pos);
        ShakeCamera();
        PlaySkillWave(kAsuraWave);
        break;
    case 381:
        LaunchOn(target, 387);
        break;
    case 382:
        AimEffect(LaunchOn(source, 388), target->m_pos);
        break;
    case 394:
        AimEffect(LaunchOn(source, 393), target->m_pos);
        break;
    case 397:
        LaunchOn(target, 339);
        break;
    case 398:
        LaunchOn(source, 399);
        break;
    case 399:
        LaunchOn(source, 400);
        break;
    case 400:
        LaunchOn(target, 401);
        break;
    case 415:
        LaunchOn(target, 413);
        LaunchOn(target, 431);
        break;
    case 417:
        LaunchOn(target, 466);
        LaunchOn(target, 429);
        break;
    case 419:
        LaunchOn(target, 415);
        AimEffect(LaunchOn(source, 430), source->m_pos);
        break;
    case 421:
        LaunchOn(source, 457);
        AimEffect(LaunchOn(source, 439), target->m_pos);
        LaunchOn(target, 458);
        break;
    case 467:
        AimEffect(LaunchOn(source, 547), target->m_pos);
        LaunchOn(target, 550);
        break;
    case 468:
        AimEffect(LaunchOn(source, 555), target->m_pos);
        LaunchOn(target, 429);
        break;
    case 469:
        LaunchOn(target, 425);
        AimEffect(LaunchOn(source, 553), target->m_pos);
        LaunchOn(target, 431);
        LaunchOn(target, 554);
        break;
    case 480:
        LaunchOn(target, 520);
        break;
    case 485:
        LaunchOn(target, 518);
        break;
    case 490:
        SetEffectDuration(LaunchOn(target, 537), 100);
        break;
    case 501:
        LaunchOn(target, 548);
        break;
    case 502:
        LaunchOn(target, 648);
        break;
    case 503:
        LaunchOn(target, 649);
        break;
    case 507:
        LaunchOn(target, 644);
        break;
    case 512:
        LaunchOn(target, 647);
        break;
    case 514:
        AimEffect(LaunchOn(source, 623), target->m_pos);
        break;
    case 515:
        LaunchOn(target, 643);
        break;
    case 518:
        LaunchOn(target, 628);
        break;
    case 519:
        AimEffect(LaunchOn(source, 584), target->m_pos);
        break;
    case 520:
        AimEffect(LaunchOn(source, 645), target->m_pos);
        break;
    case 523:
        if (LaunchAimed(source, 613, target->m_pos) != nullptr) {
            PlaySkillWave(kNinjaThrowWave);
        }
        break;
    case 524:
        if (LaunchAimed(source, 614, target->m_pos) != nullptr) {
            PlaySkillWave(kNinjaThrowWave);
        }
        break;
    case 525:
        if (LaunchAimed(source, 615, target->m_pos) != nullptr) {
            PlaySkillWave(kNinjaThrowWave);
        }
        break;
    case 526:
        if (LaunchAimed(source, 616, target->m_pos) != nullptr) {
            PlaySkillWave(kNinjaThrowWave);
        }
        break;
    case 528:
        LaunchOn(target, 632);
        break;
    case 530:
        LaunchOn(target, 630);
        break;
    case 534:
        LaunchOn(target, 618);
        break;
    case 537:
        AimEffect(LaunchOn(target, 619), target->m_pos);
        break;
    case 540:
        SetEffectCount(LaunchAimed(source, 621, target->m_pos), level);
        break;
    case 542:
        AimEffect(LaunchOn(source, 624), target->m_pos);
        break;
    case 544:
        LaunchOn(target, 633);
        break;
    case 653:
        LaunchOn(source, 666);
        break;
    case 654:
        PlaySkillWave(kFireWallWave);
        break;
    case 659:
        LaunchOn(source, 668);
        break;
    case 661:
        LaunchOn(source, 409);
        break;
    case 665:
        LaunchOn(source, 669);
        break;
    case 666:
        PlaySkillWave(kStoneCurseWave);
        break;
    case 667:
        PlaySkillWave(kAmonRaDieWave);
        LaunchOn(source, 670);
        break;
    case 668:
        PlaySkillWave(kWideSleepWave);
        break;
    case 669:
        LaunchOn(source, 22);
        break;
    case 672:
        LaunchOn(target, 676);
        break;
    case 673:
        LaunchOn(target, 677);
        break;
    case 674:
        AimEffect(LaunchOn(LocalPlayer(), 227), source->m_pos);
        break;
    case 675:
        LaunchOn(source, 214);
        break;
    case 676:
        LaunchOn(source, 675);
        break;
    case 1004:
        AimEffect(LaunchOn(source, 600), target->m_pos);
        break;
    case 8009:
        if (level == 1) {
            LaunchOn(target, 565);
        } else if (level == 2) {
            LaunchOn(target, 566);
        } else {
            LaunchOn(target, 567);
        }
        break;
    case 8012:
        LaunchOn(target, 567);
        LaunchOn(target, 425);
        break;
    default:
        break;
    }
}
