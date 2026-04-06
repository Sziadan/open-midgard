#include "GameModePacket.h"

#include "network/GronPacket.h"
#include "network/Packet.h"
#include "GameMode.h"
#include "core/Globals.h"
#include "Types.h"
#include "core/File.h"
#include "pathfinder/PathFinder.h"
#include "session/Session.h"
#include "session/SkillActionInfo.h"
#include "session/SkillActionInfo2.h"
#include "skill/Skill.h"
#include "item/Item.h"
#include "Mode.h"
#include "DebugLog.h"
#include "audio/Audio.h"
#include "lua/LuaBridge.h"
#include "ui/UIBasicInfoWnd.h"
#include "ui/UISayDialogWnd.h"
#include "ui/UINpcMenuWnd.h"
#include "ui/UINpcInputWnd.h"

#include <cmath>
#include "ui/UIChooseSellBuyWnd.h"
#include "ui/UIItemPurchaseWnd.h"
#include "ui/UIItemSellWnd.h"
#include "ui/UIItemShopWnd.h"
#include "ui/UIStatusWnd.h"
#include "ui/UIWindowMgr.h"
#include "world/GameActor.h"
#include "world/RagEffect.h"
#include "world/World.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <cstdio>
#include <initializer_list>
#include <map>
#include <string>
#include <vector>

class CConnection {
public:
    bool Connect(const char* ip, int port);
    void Disconnect();
};

class CRagConnection : public CConnection {
public:
    static CRagConnection* instance();
    bool SendPacket(const char* data, int len);
};

namespace {

constexpr int kUiChatEventMsg = 101;
constexpr u32 kSystemNoticeColor = 0x0080C0FFu;

PendingDisconnectAction g_pendingDisconnectAction = PendingDisconnectAction::None;
u32 g_lastLocalLevelUpEffectId = 0;
u32 g_lastLocalLevelUpEffectTick = 0;
u32 g_lastSelfLevelUpStatusTick = 0;
u32 g_lastSelfLevelUpStatusType = 0;

void StartLocalPickupAnimation(CGameMode& mode, u32 objectAid);

constexpr u32 kStatusSpeed = 0;
constexpr u32 kStatusBaseExp = 1;
constexpr u32 kStatusJobExp = 2;
constexpr u32 kStatusHp = 5;
constexpr u32 kStatusMaxHp = 6;
constexpr u32 kStatusSp = 7;
constexpr u32 kStatusMaxSp = 8;
constexpr u32 kStatusStatusPoint = 9;
constexpr u32 kStatusBaseLevel = 11;
constexpr u32 kStatusSkillPoint = 12;
constexpr u32 kStatusZeny = 20;
constexpr u32 kStatusNextBaseExp = 22;
constexpr u32 kStatusNextJobExp = 23;
constexpr u32 kStatusStr = 13;
constexpr u32 kStatusAgi = 14;
constexpr u32 kStatusVit = 15;
constexpr u32 kStatusInt = 16;
constexpr u32 kStatusDex = 17;
constexpr u32 kStatusLuk = 18;
constexpr u32 kStatusNeedStr = 32;
constexpr u32 kStatusNeedAgi = 33;
constexpr u32 kStatusNeedVit = 34;
constexpr u32 kStatusNeedInt = 35;
constexpr u32 kStatusNeedDex = 36;
constexpr u32 kStatusNeedLuk = 37;
constexpr u32 kStatusAtk = 41;
constexpr u32 kStatusRefineAtk = 42;
constexpr u32 kStatusMatkMax = 43;
constexpr u32 kStatusMatkMin = 44;
constexpr u32 kStatusDef = 45;
constexpr u32 kStatusDefBonus = 46;
constexpr u32 kStatusMdef = 47;
constexpr u32 kStatusMdefBonus = 48;
constexpr u32 kStatusHit = 49;
constexpr u32 kStatusFlee = 50;
constexpr u32 kStatusFleeBonus = 51;
constexpr u32 kStatusCritical = 52;
constexpr u32 kStatusAspd = 53;
constexpr u32 kStatusPlusAspd = 54;
constexpr u32 kStatusJobLevel = 55;
constexpr u32 kNotifyEffectBaseLevelUp = 0;
constexpr int kEffectStateSightMask = 0x0001;
constexpr int kEffectStateRuwachMask = 0x2000;
constexpr int kEffectStateHidingMask = 0x0004;
constexpr int kEffectStateSpecialHidingMask = 0x0040;
constexpr int kEffectStateBurrowMask = 0x0002;
constexpr int kSightStateEffectId = 601;

bool HasEffectStateFlag(int effectState, int mask)
{
    return (effectState & mask) != 0;
}

bool ActorHasAttachedEffect(const CGameActor* actor, int effectId)
{
    if (!actor) {
        return false;
    }

    for (const CRagEffect* effect : actor->m_effectList) {
        if (effect && effect->GetEffectType() == effectId) {
            return true;
        }
    }
    return false;
}

void RefreshActorEffectStatePresentation(CGameActor* actor, int oldEffectState)
{
    if (!actor) {
        return;
    }

    const int effectState = actor->m_effectState;
    const bool hiding = HasEffectStateFlag(effectState, kEffectStateHidingMask);
    const bool specialHiding = HasEffectStateFlag(effectState, kEffectStateSpecialHidingMask);
    const bool burrow = HasEffectStateFlag(effectState, kEffectStateBurrowMask);
    const bool bodyVisible = !(hiding || specialHiding || burrow);
    const bool fullyVisible = !(hiding || specialHiding);

    actor->m_isVisibleBody = bodyVisible ? 1 : 0;
    actor->m_isVisible = fullyVisible ? 1 : 0;
    actor->m_shadowOn = bodyVisible ? 1 : 0;

    if (HasEffectStateFlag(effectState, kEffectStateSightMask) && !ActorHasAttachedEffect(actor, kSightStateEffectId)) {
        actor->LaunchEffect(kSightStateEffectId, vector3d{}, 0.0f);
    }

    const bool hadRuwach = HasEffectStateFlag(oldEffectState, kEffectStateRuwachMask);
    const bool hasRuwach = HasEffectStateFlag(effectState, kEffectStateRuwachMask);
    if (!hadRuwach && hasRuwach) {
        actor->m_effectLaunchCnt = 0;
        if (CRagEffect* effect = actor->LaunchEffect(22, vector3d{}, 0.0f)) {
            effect->SendMsg(effect, 44, (timeGetTime() >> 4) % 10 + 1, 0, 0);
        }
    }

    if (CPc* pcActor = dynamic_cast<CPc*>(actor)) {
        pcActor->InvalidateBillboard();
    }
}
constexpr u32 kNotifyEffectJobLevelUp = 1;
constexpr u32 kNotifyEffectBaseLevelUpSuperNovice = 7;
constexpr u32 kNotifyEffectJobLevelUpSuperNovice = 8;
constexpr u32 kNotifyEffectBaseLevelUpTaekwon = 9;
constexpr u32 kEffectIdRecovery = 78;
constexpr u32 kEffectIdBaseLevelUp = 371;
constexpr u32 kEffectIdJobLevelUp = 158;
constexpr u32 kEffectIdBaseLevelUpSuperNovice = 338;
constexpr u32 kEffectIdJobLevelUpSuperNovice = 337;
constexpr u32 kEffectIdBaseLevelUpTaekwon = 582;
constexpr u32 kLocalLevelUpNotifySuppressMs = 1200;
constexpr u32 kDeathFadeDurationMs = 510;
constexpr u32 kDeathCorpseHoldMs = 1290;

struct RemoteMoveApplyTrace {
    u32 lastClientTick = 0;
    u32 lastServerTick = 0;
};

std::map<u32, RemoteMoveApplyTrace> g_remoteMoveApplyTraceByGid;

u16 ReadLE16(const u8* data);
u32 ReadLE32(const u8* data);
float TileToWorldCoordX(const CWorld* world, int tileX);
float TileToWorldCoordZ(const CWorld* world, int tileY);
float ResolveActorHeight(const CWorld* world, float worldX, float worldZ);
float PacketDirToRotationDegrees(int dir);
bool ApplySelfStatusUpdate(CGameMode& mode, u32 statusType, u32 value);
void ClearAttachedSkillEffects(CGameActor* actor);

std::string ToLowerAsciiStatus(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

const char* ResolveLevelUpWavePath()
{
    static bool s_initialized = false;
    static std::string s_path;
    if (s_initialized) {
        return s_path.empty() ? nullptr : s_path.c_str();
    }
    s_initialized = true;

    const char* directCandidates[] = {
        "wav\\levelup.wav",
        "data\\wav\\levelup.wav",
        "wav\\LEVELUP.WAV",
        "data\\wav\\LEVELUP.WAV",
        nullptr
    };
    for (int index = 0; directCandidates[index]; ++index) {
        if (g_fileMgr.IsDataExist(directCandidates[index])) {
            s_path = directCandidates[index];
            return s_path.c_str();
        }
    }

    std::vector<std::string> wavNames;
    g_fileMgr.CollectDataNamesByExtension("wav", wavNames);
    for (const std::string& candidate : wavNames) {
        if (ToLowerAsciiStatus(candidate).find("levelup") != std::string::npos) {
            s_path = candidate;
            break;
        }
    }

    return s_path.empty() ? nullptr : s_path.c_str();
}

void PlayBaseLevelUpPresentation(CGameMode& mode)
{
    if (const char* wavePath = ResolveLevelUpWavePath()) {
        if (CAudio* audio = CAudio::GetInstance()) {
            audio->PlaySound(wavePath);
        }
    }

    g_windowMgr.MakeWindow(UIWindowMgr::WID_NOTIFYLEVELUPWND);
}

void PlayJobLevelUpPresentation(CGameMode& mode)
{
    g_windowMgr.MakeWindow(UIWindowMgr::WID_NOTIFYJOBLEVELUPWND);
}

void RecordLocalLevelUpEffect(u32 effectId)
{
    g_lastLocalLevelUpEffectId = effectId;
    g_lastLocalLevelUpEffectTick = GetTickCount();
}

// Deduplicate base/job level-up VFX+SFX when both ZC_NOTIFY_EFFECT and self status update spawn the same effect
// (e.g. notify effect=371 then local base level-up effect=371 — second spawn replays angel.str startup audio).
static bool ShouldSuppressDuplicateLevelUpEffect(u32 effectId)
{
    const u32 elapsed = GetTickCount() - g_lastLocalLevelUpEffectTick;
    return g_lastLocalLevelUpEffectId == effectId && elapsed <= kLocalLevelUpNotifySuppressMs;
}

bool ShouldSuppressSelfNotifyEffect(u32 actorId, u32 effectId)
{
    if (actorId != g_session.m_aid && actorId != g_session.m_gid) {
        return false;
    }

    return ShouldSuppressDuplicateLevelUpEffect(effectId);
}

u32 ResolveLocalBaseLevelUpEffectId(const CGameMode& mode)
{
    const CPlayer* player = mode.m_world ? mode.m_world->m_player : nullptr;
    const int job = player ? player->m_job : g_session.m_playerJob;
    if (job == 23 || job == 4046) {
        return kEffectIdBaseLevelUpSuperNovice;
    }
    if (job == 4047 || job == 4048 || job == 4049 || job == 4050 || job == 4051) {
        return kEffectIdBaseLevelUpTaekwon;
    }
    return kEffectIdBaseLevelUp;
}

u32 ResolveLocalJobLevelUpEffectId(const CGameMode& mode)
{
    const CPlayer* player = mode.m_world ? mode.m_world->m_player : nullptr;
    const int job = player ? player->m_job : g_session.m_playerJob;
    if (job == 23 || job == 4046) {
        return kEffectIdJobLevelUpSuperNovice;
    }
    return kEffectIdJobLevelUp;
}

void SyncSessionAppearanceToLocalPlayer(CGameMode& mode)
{
    if (!mode.m_world || !mode.m_world->m_player) {
        return;
    }

    CPlayer* player = mode.m_world->m_player;
    player->m_job = g_session.m_playerJob;
    player->m_headType = g_session.m_playerHead;
    player->m_bodyPalette = g_session.m_playerBodyPalette;

    if (CPc* pcActor = dynamic_cast<CPc*>(player)) {
        pcActor->m_head = g_session.m_playerHead;
        pcActor->m_headPalette = g_session.m_playerHeadPalette;
        pcActor->m_weapon = g_session.m_playerWeapon;
        pcActor->m_shield = g_session.m_playerShield;
        pcActor->m_accessory = g_session.m_playerAccessory;
        pcActor->m_accessory2 = g_session.m_playerAccessory2;
        pcActor->m_accessory3 = g_session.m_playerAccessory3;
        pcActor->InvalidateBillboard();
    }
}

CGameActor* ResolveNotifyEffectActor(CGameMode& mode, u32 actorId)
{
    if (mode.m_world && mode.m_world->m_player) {
        CPlayer* player = mode.m_world->m_player;
        if (actorId == g_session.m_aid || actorId == g_session.m_gid || actorId == player->m_gid) {
            return player;
        }
    }

    const auto it = mode.m_runtimeActors.find(actorId);
    if (it != mode.m_runtimeActors.end()) {
        return it->second;
    }

    return nullptr;
}

void HandleNotifyEffect(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 10) {
        return;
    }

    const u32 actorId = ReadLE32(packet.data + 2);
    const u32 effectType = ReadLE32(packet.data + 6);
    CGameActor* actor = ResolveNotifyEffectActor(mode, actorId);
    if (!actor) {
        DbgLog("[GameMode] notify effect unresolved actor=%u type=%u\n",
            static_cast<unsigned int>(actorId),
            static_cast<unsigned int>(effectType));
        return;
    }

    u32 effectId = 0;
    switch (effectType) {
    case kNotifyEffectBaseLevelUp:
        effectId = kEffectIdBaseLevelUp;
        break;
    case kNotifyEffectBaseLevelUpSuperNovice:
        effectId = kEffectIdBaseLevelUpSuperNovice;
        break;
    case kNotifyEffectBaseLevelUpTaekwon:
        effectId = kEffectIdBaseLevelUpTaekwon;
        break;
    case kNotifyEffectJobLevelUp:
        effectId = kEffectIdJobLevelUp;
        break;
    case kNotifyEffectJobLevelUpSuperNovice:
        effectId = kEffectIdJobLevelUpSuperNovice;
        break;
    default:
        DbgLog("[GameMode] unhandled notify effect type=%u actor=%u\n",
            static_cast<unsigned int>(effectType),
            static_cast<unsigned int>(actorId));
        return;
    }

    if (ShouldSuppressSelfNotifyEffect(actorId, effectId)) {
        DbgLog("[GameMode] notify effect suppressed actor=%u type=%u effect=%u\n",
            static_cast<unsigned int>(actorId),
            static_cast<unsigned int>(effectType),
            static_cast<unsigned int>(effectId));
        return;
    }

    DbgLog("[GameMode] notify effect actor=%u type=%u effect=%u self=%d\n",
        static_cast<unsigned int>(actorId),
        static_cast<unsigned int>(effectType),
        static_cast<unsigned int>(effectId),
        actor == (mode.m_world ? static_cast<CGameActor*>(mode.m_world->m_player) : nullptr));
    if (!actor->LaunchEffect(static_cast<int>(effectId), vector3d{ 0.0f, 0.0f, 0.0f }, 0.0f)) {
        LaunchLevelUpEffect(actor, effectId);
    }
    if (actorId == g_session.m_aid || actorId == g_session.m_gid) {
        RecordLocalLevelUpEffect(effectId);
    }
}

void HandleNotifyEffect2(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 10) {
        return;
    }

    const u32 actorId = ReadLE32(packet.data + 2);
    const u32 effectId = ReadLE32(packet.data + 6);
    CGameActor* actor = ResolveNotifyEffectActor(mode, actorId);
    if (!actor) {
        DbgLog("[GameMode] notify effect2 unresolved actor=%u effect=%u\n",
            static_cast<unsigned int>(actorId),
            static_cast<unsigned int>(effectId));
        return;
    }

    DbgLog("[GameMode] notify effect2 actor=%u effect=%u self=%d\n",
        static_cast<unsigned int>(actorId),
        static_cast<unsigned int>(effectId),
        actor == (mode.m_world ? static_cast<CGameActor*>(mode.m_world->m_player) : nullptr));

    if (!actor->LaunchEffect(static_cast<int>(effectId), vector3d{ 0.0f, 0.0f, 0.0f }, 0.0f)) {
        DbgLog("[GameMode] notify effect2 launch failed actor=%u effect=%u\n",
            static_cast<unsigned int>(actorId),
            static_cast<unsigned int>(effectId));
    }
}

void HandleSkillCastCancel(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 6) {
        return;
    }

    const u32 actorId = ReadLE32(packet.data + 2);
    CGameActor* actor = ResolveNotifyEffectActor(mode, actorId);
    DbgLog("[GameMode] skill cast cancel actor=%u resolved=%d recentLevelStatus=%u recentType=%u\n",
        static_cast<unsigned int>(actorId),
        actor ? 1 : 0,
        static_cast<unsigned int>(GetTickCount() - g_lastSelfLevelUpStatusTick),
        static_cast<unsigned int>(g_lastSelfLevelUpStatusType));

    if (!actor) {
        return;
    }

    ClearAttachedSkillEffects(actor);
    actor->m_isMotionFreezed = 0;
    actor->m_freezeEndTick = 0;
    actor->m_attackMotion = -1.0f;
    if (actor->m_stateId == kGameActorAttackStateId) {
        actor->m_stateId = 0;
    }
    actor->SendMsg(actor, 84, 0, 0, 0);
    actor->SendMsg(actor, 87, 0, 0, 0);
    actor->SendMsg(actor, 83, 0, 0, 0);
}

void HandleIgnorePacket(CGameMode&, const PacketView&)
{
}

void HandlePacket043F(CGameMode&, const PacketView& packet)
{
    if (!packet.data || packet.packetLength <= 0) {
        return;
    }

    char hexBytes[3 * 32 + 1]{};
    const int bytesToDump = (std::min)(static_cast<int>(packet.packetLength), 32);
    int writeOffset = 0;
    for (int index = 0; index < bytesToDump && writeOffset + 4 < static_cast<int>(sizeof(hexBytes)); ++index) {
        const int written = std::snprintf(
            hexBytes + writeOffset,
            sizeof(hexBytes) - static_cast<size_t>(writeOffset),
            index == 0 ? "%02X" : " %02X",
            static_cast<unsigned int>(packet.data[index]));
        if (written <= 0) {
            break;
        }
        writeOffset += written;
    }

    DbgLog("[GameMode] pkt043F len=%u bytes=%s\n",
        static_cast<unsigned int>(packet.packetLength),
        hexBytes);
}

int ResolvePotionEffectId(unsigned int itemId)
{
    switch (itemId) {
    case 501:
    case 507:
    case 545:
    case 569:
    case 578:
    case 598:
        return 204;
    case 502:
    case 579:
    case 599:
        return 205;
    case 503:
    case 508:
    case 546:
    case 11500:
        return 206;
    case 504:
    case 509:
    case 547:
    case 555:
    case 556:
    case 557:
    case 11501:
    case 11503:
        return 207;
    case 505:
    case 510:
    case 11504:
        return 208;
    case 506:
    case 511:
        return 209;
    case 512:
    case 513:
    case 515:
    case 516:
    case 531:
    case 532:
    case 534:
    case 535:
    case 548:
    case 549:
    case 550:
    case 574:
    case 582:
    case 682:
        return 210;
    case 514:
    case 533:
    case 568:
    case 680:
    case 683:
        return 211;
    case 517:
    case 518:
    case 519:
    case 520:
    case 521:
    case 526:
    case 528:
    case 529:
    case 530:
    case 536:
    case 537:
    case 538:
    case 539:
    case 540:
    case 541:
    case 542:
    case 543:
    case 544:
    case 551:
    case 552:
    case 553:
    case 562:
    case 563:
    case 564:
    case 565:
    case 566:
    case 567:
    case 570:
    case 571:
    case 572:
    case 575:
    case 576:
    case 577:
    case 580:
    case 581:
    case 583:
    case 584:
    case 585:
    case 586:
        return 7;
    default:
        return -1;
    }
}

void MakePotionEffect(CGameMode& mode, unsigned int itemId, u32 actorId)
{
    CGameActor* actor = ResolveNotifyEffectActor(mode, actorId);
    if (!actor) {
        return;
    }

    const int effectId = ResolvePotionEffectId(itemId);
    if (effectId <= 0) {
        return;
    }

    if (!actor->LaunchEffect(effectId, vector3d{ 0.0f, 0.0f, 0.0f }, 0.0f)) {
        DbgLog("[GameMode] potion effect launch failed itemId=%u actor=%u effect=%d\n",
            static_cast<unsigned int>(itemId),
            static_cast<unsigned int>(actorId),
            effectId);
        return;
    }

    DbgLog("[GameMode] potion effect itemId=%u actor=%u effect=%d\n",
        static_cast<unsigned int>(itemId),
        static_cast<unsigned int>(actorId),
        effectId);
}

void HandleUseItemAck2(CGameMode&, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 13) {
        return;
    }

    const unsigned int itemIndex = static_cast<unsigned int>(ReadLE16(packet.data + 2));
    const unsigned int itemId = static_cast<unsigned int>(ReadLE16(packet.data + 4));
    const unsigned int actorId = ReadLE32(packet.data + 6);
    const int remainingAmount = static_cast<int>(ReadLE16(packet.data + 10));
    const bool ok = packet.data[12] != 0;

    if (const ITEM_INFO* existing = g_session.GetInventoryItemByIndex(itemIndex)) {
        ITEM_INFO updated = *existing;
        updated.SetItemId(itemId);
        updated.m_num = (std::max)(0, remainingAmount);
        g_session.SetInventoryItem(updated);
    }

    if (ok) {
        if (CGameMode* gameMode = g_modeMgr.GetCurrentGameMode()) {
            MakePotionEffect(*gameMode, itemId, actorId);
        }
    }

    DbgLog("[GameMode] use item ack2 index=%u itemId=%u actor=%u amount=%d ok=%d\n",
        itemIndex,
        itemId,
        actorId,
        remainingAmount,
        ok ? 1 : 0);
}

void HandleRecovery(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 6) {
        return;
    }

    const u32 statusType = ReadLE16(packet.data + 2);
    const u32 amount = ReadLE16(packet.data + 4);
    if (amount == 0) {
        return;
    }

    u32 currentValue = 0;
    u32 maxValue = 0;
    if (mode.m_world && mode.m_world->m_player) {
        const CPlayer* player = mode.m_world->m_player;
        if (statusType == kStatusHp) {
            currentValue = player->m_Hp;
            maxValue = player->m_MaxHp;
        } else if (statusType == kStatusSp) {
            currentValue = player->m_Sp;
            maxValue = player->m_MaxSp;
        }
    } else if (const CHARACTER_INFO* info = g_session.GetSelectedCharacterInfo()) {
        if (statusType == kStatusHp) {
            currentValue = static_cast<u32>((std::max)(0, static_cast<int>(info->hp)));
            maxValue = static_cast<u32>((std::max)(0, static_cast<int>(info->maxhp)));
        } else if (statusType == kStatusSp) {
            currentValue = static_cast<u32>((std::max)(0, static_cast<int>(info->sp)));
            maxValue = static_cast<u32>((std::max)(0, static_cast<int>(info->maxsp)));
        }
    }

    if (statusType == kStatusHp || statusType == kStatusSp) {
        const u32 healedValue = maxValue > 0 ? (std::min)(maxValue, currentValue + amount) : (currentValue + amount);
        ApplySelfStatusUpdate(mode, statusType, healedValue);
    }

    if (mode.m_world && mode.m_world->m_player) {
        CGameActor* actor = mode.m_world->m_player;
        if (!actor->LaunchEffect(static_cast<int>(kEffectIdRecovery), vector3d{ 0.0f, 0.0f, 0.0f }, 0.0f)) {
            DbgLog("[GameMode] recovery effect launch failed status=%u amount=%u\n",
                static_cast<unsigned int>(statusType),
                static_cast<unsigned int>(amount));
        }
    }

    DbgLog("[GameMode] recovery status=%u amount=%u\n",
        static_cast<unsigned int>(statusType),
        static_cast<unsigned int>(amount));
}

void HandleAttackRange(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 4) {
        return;
    }

    mode.m_attackChaseRange = (std::max)(1, static_cast<int>(ReadLE16(packet.data + 2)));
}

void HandleAttackFailureForDistance(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 16) {
        return;
    }

    const u32 targetGid = ReadLE32(packet.data + 2);
    const int targetX = static_cast<int>(static_cast<s16>(ReadLE16(packet.data + 6)));
    const int targetY = static_cast<int>(static_cast<s16>(ReadLE16(packet.data + 8)));
    const int sourceX = static_cast<int>(static_cast<s16>(ReadLE16(packet.data + 10)));
    const int sourceY = static_cast<int>(static_cast<s16>(ReadLE16(packet.data + 12)));
    const int attackRange = (std::max)(1, static_cast<int>(ReadLE16(packet.data + 14)));

    mode.m_lastAttackChaseHintTick = GetTickCount();
    mode.m_attackChaseTargetGid = targetGid;
    mode.m_attackChaseTargetCellX = targetX;
    mode.m_attackChaseTargetCellY = targetY;
    mode.m_attackChaseSourceCellX = sourceX;
    mode.m_attackChaseSourceCellY = sourceY;
    mode.m_attackChaseRange = attackRange;
    mode.m_hasAttackChaseHint = 1;
}

u16 ClampToU16(u32 value)
{
    return static_cast<u16>((std::min)(value, static_cast<u32>(0xFFFFu)));
}

int ConvertRawAspdToDisplay(int rawMotion)
{
    const int clampedMotion = (std::max)(0, (std::min)(rawMotion, 2000));
    return (std::max)(0, (2000 - clampedMotion) / 10);
}

void SyncLocalPlayerPrimaryStatsFromSession(CGameMode& mode)
{
    CPlayer* player = mode.m_world ? mode.m_world->m_player : nullptr;
    if (!player) {
        return;
    }

    const CHARACTER_INFO* info = g_session.GetSelectedCharacterInfo();
    if (!info) {
        return;
    }

    player->m_Str = ClampToU16(static_cast<u32>(info->Str) + static_cast<u32>((std::max)(0, g_session.m_plusStr)));
    player->m_Agi = ClampToU16(static_cast<u32>(info->Agi) + static_cast<u32>((std::max)(0, g_session.m_plusAgi)));
    player->m_Vit = ClampToU16(static_cast<u32>(info->Vit) + static_cast<u32>((std::max)(0, g_session.m_plusVit)));
    player->m_Int = ClampToU16(static_cast<u32>(info->Int) + static_cast<u32>((std::max)(0, g_session.m_plusInt)));
    player->m_Dex = ClampToU16(static_cast<u32>(info->Dex) + static_cast<u32>((std::max)(0, g_session.m_plusDex)));
    player->m_Luk = ClampToU16(static_cast<u32>(info->Luk) + static_cast<u32>((std::max)(0, g_session.m_plusLuk)));
}

bool ApplyCombatStatusParamToSession(u32 statusType, int value, bool aspdIsRawMotion)
{
    switch (statusType) {
    case kStatusAtk:
        g_session.m_attPower = value;
        return true;
    case kStatusRefineAtk:
        g_session.m_refiningPower = value;
        return true;
    case kStatusMatkMax:
        g_session.m_maxMatkPower = value;
        return true;
    case kStatusMatkMin:
        g_session.m_minMatkPower = value;
        return true;
    case kStatusDef:
        g_session.m_itemDefPower = value;
        return true;
    case kStatusDefBonus:
        g_session.m_plusDefPower = value;
        return true;
    case kStatusMdef:
        g_session.m_mdefPower = value;
        return true;
    case kStatusMdefBonus:
        g_session.m_plusMdefPower = value;
        return true;
    case kStatusHit:
        g_session.m_hitSuccessValue = value;
        return true;
    case kStatusFlee:
        g_session.m_avoidSuccessValue = value;
        return true;
    case kStatusFleeBonus:
        g_session.m_plusAvoidSuccessValue = value;
        return true;
    case kStatusCritical:
        g_session.m_criticalSuccessValue = value;
        return true;
    case kStatusAspd:
        g_session.m_aspd = aspdIsRawMotion ? ConvertRawAspdToDisplay(value) : value;
        return true;
    case kStatusPlusAspd:
        g_session.m_plusAspd = value;
        return true;
    default:
        return false;
    }
}

void ApplyStatusBaseAndCostSummary(CGameMode& mode,
    int statusPoint,
    const std::array<int, 6>& baseStats,
    const std::array<int, 6>& statCosts)
{
    if (CHARACTER_INFO* info = g_session.GetMutableSelectedCharacterInfo()) {
        if (statusPoint >= 0) {
            info->sppoint = static_cast<s16>(ClampToU16(static_cast<u32>(statusPoint)));
        }
        info->Str = static_cast<u8>((std::min)(baseStats[0], 0xFF));
        info->Agi = static_cast<u8>((std::min)(baseStats[1], 0xFF));
        info->Vit = static_cast<u8>((std::min)(baseStats[2], 0xFF));
        info->Int = static_cast<u8>((std::min)(baseStats[3], 0xFF));
        info->Dex = static_cast<u8>((std::min)(baseStats[4], 0xFF));
        info->Luk = static_cast<u8>((std::min)(baseStats[5], 0xFF));
    }

    g_session.m_standardStr = (std::max)(0, statCosts[0]);
    g_session.m_standardAgi = (std::max)(0, statCosts[1]);
    g_session.m_standardVit = (std::max)(0, statCosts[2]);
    g_session.m_standardInt = (std::max)(0, statCosts[3]);
    g_session.m_standardDex = (std::max)(0, statCosts[4]);
    g_session.m_standardLuk = (std::max)(0, statCosts[5]);
    SyncLocalPlayerPrimaryStatsFromSession(mode);
}

void ApplyCombatStatusSummary(CGameMode& mode,
    int attack,
    int refineAttack,
    int matkMax,
    int matkMin,
    int itemDef,
    int plusDef,
    int mdef,
    int plusMdef,
    int hit,
    int flee,
    int plusFlee,
    int critical,
    int aspdDisplay,
    int plusAspd)
{
    g_session.m_attPower = (std::max)(0, attack);
    g_session.m_refiningPower = (std::max)(0, refineAttack);
    g_session.m_maxMatkPower = (std::max)(0, matkMax);
    g_session.m_minMatkPower = (std::max)(0, matkMin);
    g_session.m_itemDefPower = (std::max)(0, itemDef);
    g_session.m_plusDefPower = (std::max)(0, plusDef);
    g_session.m_mdefPower = (std::max)(0, mdef);
    g_session.m_plusMdefPower = (std::max)(0, plusMdef);
    g_session.m_hitSuccessValue = (std::max)(0, hit);
    g_session.m_avoidSuccessValue = (std::max)(0, flee);
    g_session.m_plusAvoidSuccessValue = (std::max)(0, plusFlee);
    g_session.m_criticalSuccessValue = (std::max)(0, critical);
    g_session.m_aspd = (std::max)(0, aspdDisplay);
    g_session.m_plusAspd = (std::max)(0, plusAspd);
    SyncLocalPlayerPrimaryStatsFromSession(mode);
}

bool ApplySelfStatusUpdateToSession(u32 statusType, u32 value)
{
    if (ApplyCombatStatusParamToSession(statusType, static_cast<int>(value), true)) {
        return true;
    }

    CHARACTER_INFO* info = g_session.GetMutableSelectedCharacterInfo();
    switch (statusType) {
    case kStatusBaseExp:
        g_session.SetBaseExpValue(static_cast<int>(value));
        return true;
    case kStatusJobExp:
        g_session.SetJobExpValue(static_cast<int>(value));
        return true;
    case kStatusNextBaseExp:
        g_session.SetNextBaseExpValue(static_cast<int>(value));
        return true;
    case kStatusNextJobExp:
        g_session.SetNextJobExpValue(static_cast<int>(value));
        return true;
    default:
        break;
    }

    if (!info) {
        return false;
    }

    switch (statusType) {
    case kStatusHp:
        info->hp = static_cast<s16>(ClampToU16(value));
        if (info->maxhp < info->hp) {
            info->maxhp = info->hp;
        }
        return true;
    case kStatusMaxHp:
        info->maxhp = static_cast<s16>(ClampToU16(value));
        if (info->hp > info->maxhp) {
            info->hp = info->maxhp;
        }
        return true;
    case kStatusSp:
        info->sp = static_cast<s16>(ClampToU16(value));
        if (info->maxsp < info->sp) {
            info->maxsp = info->sp;
        }
        return true;
    case kStatusMaxSp:
        info->maxsp = static_cast<s16>(ClampToU16(value));
        if (info->sp > info->maxsp) {
            info->sp = info->maxsp;
        }
        return true;
    case kStatusBaseLevel:
        info->level = static_cast<s16>(ClampToU16(value));
        return true;
    case kStatusJobLevel:
        info->joblevel = static_cast<int>(value);
        return true;
    case kStatusZeny:
        info->money = static_cast<int>(value);
        return true;
    case kStatusStatusPoint:
        info->sppoint = static_cast<s16>(ClampToU16(value));
        return true;
    case kStatusSkillPoint:
        info->jobpoint = static_cast<s16>(ClampToU16(value));
        return true;
    default:
        return false;
    }
}

bool BuildNormalInventoryItem(const PacketView& packet, const u8* entry, ITEM_INFO& outItem)
{
    if (!entry) {
        return false;
    }

    outItem = ITEM_INFO{};
    outItem.m_itemIndex = ReadLE16(entry + 0);
    outItem.SetItemId(ReadLE16(entry + 2));
    outItem.m_itemType = entry[4];
    outItem.m_isIdentified = entry[5];
    outItem.m_num = ReadLE16(entry + 6);
    outItem.m_wearLocation = ReadLE16(entry + 8);

    size_t entrySize = 0;
    switch (packet.packetId) {
    case 0x00A3:
        entrySize = 10;
        break;
    case 0x01EE:
        entrySize = 18;
        break;
    case 0x02E8:
        entrySize = 22;
        break;
    default:
        return false;
    }

    if (entrySize >= 18) {
        outItem.m_slot[0] = ReadLE16(entry + 10);
        outItem.m_slot[1] = ReadLE16(entry + 12);
        outItem.m_slot[2] = ReadLE16(entry + 14);
        outItem.m_slot[3] = ReadLE16(entry + 16);
    }
    if (entrySize >= 22) {
        outItem.m_deleteTime = static_cast<int>(ReadLE32(entry + 18));
    }

    return outItem.m_itemIndex != 0;
}

bool BuildEquipInventoryItem(const PacketView& packet, const u8* entry, ITEM_INFO& outItem)
{
    if (!entry) {
        return false;
    }

    outItem = ITEM_INFO{};
    outItem.m_itemIndex = ReadLE16(entry + 0);
    outItem.SetItemId(ReadLE16(entry + 2));
    outItem.m_itemType = entry[4];
    outItem.m_isIdentified = entry[5];
    outItem.m_location = ReadLE16(entry + 6);
    outItem.m_wearLocation = ReadLE16(entry + 8);
    outItem.m_isDamaged = entry[10];
    outItem.m_refiningLevel = entry[11];
    outItem.m_slot[0] = ReadLE16(entry + 12);
    outItem.m_slot[1] = ReadLE16(entry + 14);
    outItem.m_slot[2] = ReadLE16(entry + 16);
    outItem.m_slot[3] = ReadLE16(entry + 18);
    outItem.m_num = 1;

    size_t entrySize = 0;
    switch (packet.packetId) {
    case 0x00A4:
        entrySize = 20;
        break;
    case 0x01EF:
        entrySize = 24;
        break;
    case 0x02D0:
        entrySize = 26;
        break;
    default:
        return false;
    }

    if (entrySize >= 24) {
        outItem.m_deleteTime = static_cast<int>(ReadLE32(entry + 20));
    }
    if (entrySize >= 26) {
        outItem.m_isYours = ReadLE16(entry + 24);
    }

    if (outItem.m_wearLocation != 0) {
        DbgLog("[GameMode] equip inventory entry pkt=0x%04X index=%u item=%u location=0x%04X wear=0x%04X damaged=%u refine=%u yours=%u\n",
            static_cast<unsigned int>(packet.packetId),
            outItem.m_itemIndex,
            outItem.GetItemId(),
            static_cast<unsigned int>(outItem.m_location),
            static_cast<unsigned int>(outItem.m_wearLocation),
            static_cast<unsigned int>(outItem.m_isDamaged),
            static_cast<unsigned int>(outItem.m_refiningLevel),
            static_cast<unsigned int>(outItem.m_isYours));
    }

    return outItem.m_itemIndex != 0;
}

void ApplySkillMetadata(PLAYER_SKILL_INFO& skillInfo)
{
    g_skillMgr.EnsureLoaded();
    const SkillMetadata* metadata = g_skillMgr.GetSkillMetadata(skillInfo.SKID);
    if (!metadata) {
        if (skillInfo.skillName.empty()) {
            skillInfo.skillName = "Unknown Skill";
        }
        return;
    }

    skillInfo.skillIdName = metadata->skillIdName;
    if (skillInfo.skillName.empty()) {
        skillInfo.skillName = metadata->displayName.empty() ? metadata->skillIdName : metadata->displayName;
    }
    if (skillInfo.descriptionLines.empty()) {
        skillInfo.descriptionLines = metadata->descriptionLines;
    }
    if (skillInfo.skillMaxLv <= 0) {
        skillInfo.skillMaxLv = static_cast<int>(metadata->levelSpCosts.size());
    }
    if (skillInfo.spcost <= 0 &&
        skillInfo.level > 0 &&
        skillInfo.level <= static_cast<int>(metadata->levelSpCosts.size())) {
        skillInfo.spcost = metadata->levelSpCosts[skillInfo.level - 1];
    }
}

bool BuildPlayerSkillListEntry(const u8* entry, PLAYER_SKILL_INFO& outSkill)
{
    if (!entry) {
        return false;
    }

    outSkill = {};
    outSkill.m_isValid = 1;
    outSkill.SKID = ReadLE16(entry + 0);
    outSkill.type = static_cast<int>(ReadLE32(entry + 2));
    outSkill.level = static_cast<int>(ReadLE16(entry + 6));
    outSkill.spcost = static_cast<int>(ReadLE16(entry + 8));
    outSkill.attackRange = static_cast<int>(ReadLE16(entry + 10));
    outSkill.upgradable = static_cast<int>(entry[36]);
    ApplySkillMetadata(outSkill);
    return outSkill.SKID != 0;
}

// Matches Ref Zc_Skillinfo_List filtering for ZC_SKILLINFO_LIST (0x010F) player skills.
static bool IsPlayerSkillTreeSkid(int skid)
{
    return skid > 0 && skid < 0x40F;
}

static bool IsHomunSkillTreeSkid(int skid)
{
    return skid >= 0x1F40 && skid < 0x1F51;
}

static bool IsMercSkillTreeSkid(int skid)
{
    return skid >= 0x2008 && skid < 0x202E;
}

enum class SkillInfoListTarget {
    Player,
    Homun,
    Merc,
};

static void HandleSkillInfoListByTarget(CGameMode&, const PacketView& packet, SkillInfoListTarget target)
{
    if (!packet.data || packet.packetLength < 4) {
        return;
    }

    DbgLog("[GameMode] skill list pkt=0x%04X len=%u target=%d\n",
        static_cast<unsigned int>(packet.packetId),
        static_cast<unsigned int>(packet.packetLength),
        static_cast<int>(target));

    switch (target) {
    case SkillInfoListTarget::Player:
        g_session.ClearSkillItems();
        break;
    case SkillInfoListTarget::Homun:
        g_session.ClearHomunSkillItems();
        break;
    case SkillInfoListTarget::Merc:
        g_session.ClearMercSkillItems();
        break;
    }

    for (size_t offset = 4; offset + 37 <= static_cast<size_t>(packet.packetLength); offset += 37) {
        PLAYER_SKILL_INFO skillInfo;
        if (!BuildPlayerSkillListEntry(packet.data + offset, skillInfo)) {
            continue;
        }
        const int skid = skillInfo.SKID;
        if (target == SkillInfoListTarget::Player) {
            if (!IsPlayerSkillTreeSkid(skid)) {
                continue;
            }
            g_session.SetSkillItem(skillInfo);
        } else if (target == SkillInfoListTarget::Homun) {
            if (!IsHomunSkillTreeSkid(skid)) {
                continue;
            }
            g_session.SetHomunSkillItem(skillInfo);
        } else {
            if (!IsMercSkillTreeSkid(skid)) {
                continue;
            }
            g_session.SetMercSkillItem(skillInfo);
        }
        DbgLog("[GameMode] skill list entry skid=%d level=%d sp=%d upgradable=%d\n",
            skillInfo.SKID,
            skillInfo.level,
            skillInfo.spcost,
            skillInfo.upgradable);
    }
}

bool TryGetExistingPlayerSkillInfo(int skillId, PLAYER_SKILL_INFO& outSkill)
{
    for (const PLAYER_SKILL_INFO& skillInfo : g_session.GetSkillItems()) {
        if (skillInfo.SKID == skillId) {
            outSkill = skillInfo;
            return true;
        }
    }
    return false;
}

static bool TryGetExistingHomunSkillInfo(int skillId, PLAYER_SKILL_INFO& outSkill)
{
    for (const PLAYER_SKILL_INFO& skillInfo : g_session.GetHomunSkillItems()) {
        if (skillInfo.SKID == skillId) {
            outSkill = skillInfo;
            return true;
        }
    }
    return false;
}

static bool TryGetExistingMercSkillInfo(int skillId, PLAYER_SKILL_INFO& outSkill)
{
    for (const PLAYER_SKILL_INFO& skillInfo : g_session.GetMercSkillItems()) {
        if (skillInfo.SKID == skillId) {
            outSkill = skillInfo;
            return true;
        }
    }
    return false;
}

void HandlePlayerSkillList(CGameMode& mode, const PacketView& packet)
{
    HandleSkillInfoListByTarget(mode, packet, SkillInfoListTarget::Player);
}

void HandleHomunSkillList(CGameMode& mode, const PacketView& packet)
{
    HandleSkillInfoListByTarget(mode, packet, SkillInfoListTarget::Homun);
}

void HandleMercSkillList(CGameMode& mode, const PacketView& packet)
{
    HandleSkillInfoListByTarget(mode, packet, SkillInfoListTarget::Merc);
}

static void ApplySkillUpdatePayload(const u8* data, PLAYER_SKILL_INFO& skillInfo, void (CSession::*setItem)(const PLAYER_SKILL_INFO&))
{
    skillInfo.level = static_cast<int>(ReadLE16(data + 4));
    skillInfo.spcost = static_cast<int>(ReadLE16(data + 6));
    skillInfo.attackRange = static_cast<int>(ReadLE16(data + 8));
    skillInfo.upgradable = static_cast<int>(data[10]);
    ApplySkillMetadata(skillInfo);
    (g_session.*setItem)(skillInfo);
}

void HandlePlayerSkillUpdate(CGameMode&, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 11) {
        return;
    }

    if (CGameMode* gameMode = g_modeMgr.GetCurrentGameMode()) {
        gameMode->m_isReqUpgradeSkillLevel = 0;
    }

    DbgLog("[GameMode] skill update pkt=0x%04X len=%u skid=%u level=%u sp=%u range=%u upgradable=%u\n",
        static_cast<unsigned int>(packet.packetId),
        static_cast<unsigned int>(packet.packetLength),
        static_cast<unsigned int>(ReadLE16(packet.data + 2)),
        static_cast<unsigned int>(ReadLE16(packet.data + 4)),
        static_cast<unsigned int>(ReadLE16(packet.data + 6)),
        static_cast<unsigned int>(ReadLE16(packet.data + 8)),
        static_cast<unsigned int>(packet.data[10]));
    PLAYER_SKILL_INFO skillInfo;
    skillInfo.SKID = static_cast<int>(ReadLE16(packet.data + 2));
    if (!TryGetExistingPlayerSkillInfo(skillInfo.SKID, skillInfo)) {
        skillInfo = {};
        skillInfo.SKID = static_cast<int>(ReadLE16(packet.data + 2));
        skillInfo.m_isValid = 1;
    }

    ApplySkillUpdatePayload(packet.data, skillInfo, &CSession::SetSkillItem);
}

void HandleHomunSkillUpdate(CGameMode&, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 11) {
        return;
    }

    if (CGameMode* gameMode = g_modeMgr.GetCurrentGameMode()) {
        gameMode->m_isReqUpgradeSkillLevel = 0;
    }

    DbgLog("[GameMode] homun skill update pkt=0x%04X skid=%u level=%u\n",
        static_cast<unsigned int>(packet.packetId),
        static_cast<unsigned int>(ReadLE16(packet.data + 2)),
        static_cast<unsigned int>(ReadLE16(packet.data + 4)));
    PLAYER_SKILL_INFO skillInfo;
    skillInfo.SKID = static_cast<int>(ReadLE16(packet.data + 2));
    if (!TryGetExistingHomunSkillInfo(skillInfo.SKID, skillInfo)) {
        skillInfo = {};
        skillInfo.SKID = static_cast<int>(ReadLE16(packet.data + 2));
        skillInfo.m_isValid = 1;
    }

    ApplySkillUpdatePayload(packet.data, skillInfo, &CSession::SetHomunSkillItem);
}

void HandleMercSkillUpdate(CGameMode&, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 11) {
        return;
    }

    if (CGameMode* gameMode = g_modeMgr.GetCurrentGameMode()) {
        gameMode->m_isReqUpgradeSkillLevel = 0;
    }

    DbgLog("[GameMode] merc skill update pkt=0x%04X skid=%u level=%u\n",
        static_cast<unsigned int>(packet.packetId),
        static_cast<unsigned int>(ReadLE16(packet.data + 2)),
        static_cast<unsigned int>(ReadLE16(packet.data + 4)));
    PLAYER_SKILL_INFO skillInfo;
    skillInfo.SKID = static_cast<int>(ReadLE16(packet.data + 2));
    if (!TryGetExistingMercSkillInfo(skillInfo.SKID, skillInfo)) {
        skillInfo = {};
        skillInfo.SKID = static_cast<int>(ReadLE16(packet.data + 2));
        skillInfo.m_isValid = 1;
    }

    ApplySkillUpdatePayload(packet.data, skillInfo, &CSession::SetMercSkillItem);
}

void HandlePlayerSkillAdd(CGameMode&, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 39) {
        return;
    }

    if (CGameMode* gameMode = g_modeMgr.GetCurrentGameMode()) {
        gameMode->m_isReqUpgradeSkillLevel = 0;
    }

    DbgLog("[GameMode] add skill pkt=0x%04X len=%u\n",
        static_cast<unsigned int>(packet.packetId),
        static_cast<unsigned int>(packet.packetLength));
    PLAYER_SKILL_INFO skillInfo;
    if (!BuildPlayerSkillListEntry(packet.data + 2, skillInfo)) {
        return;
    }
    g_session.SetSkillItem(skillInfo);
}

void HandleShortcutKeyList(CGameMode&, const PacketView& packet)
{
    constexpr size_t kShortcutEntrySize = 7;
    constexpr size_t kShortcutPayloadSize = static_cast<size_t>(kShortcutSlotCount) * kShortcutEntrySize;
    constexpr size_t kExpectedPacketSize = 2 + kShortcutPayloadSize;
    if (!packet.data || packet.packetLength < kExpectedPacketSize) {
        return;
    }

    g_session.ClearShortcutSlots();
    for (int index = 0; index < kShortcutSlotCount; ++index) {
        const size_t offset = 2 + static_cast<size_t>(index) * kShortcutEntrySize;
        const unsigned char isSkill = packet.data[offset + 0];
        const unsigned int id = ReadLE32(packet.data + offset + 1);
        const unsigned short count = ReadLE16(packet.data + offset + 5);
        g_session.SetShortcutSlotByAbsoluteIndex(index, isSkill, id, count);
    }

    DbgLog("[GameMode] shortcut key list synced slots=%d\n", kShortcutSlotCount);
}

void HandleNormalInventoryList(CGameMode&, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 4) {
        return;
    }

    size_t entrySize = 0;
    switch (packet.packetId) {
    case 0x00A3:
        entrySize = 10;
        break;
    case 0x01EE:
        entrySize = 18;
        break;
    case 0x02E8:
        entrySize = 22;
        break;
    default:
        return;
    }

    const size_t payloadSize = static_cast<size_t>(packet.packetLength - 4);
    if (entrySize == 0 || payloadSize < entrySize) {
        return;
    }

    g_session.ClearInventoryItems();
    for (size_t offset = 4; offset + entrySize <= static_cast<size_t>(packet.packetLength); offset += entrySize) {
        ITEM_INFO itemInfo;
        if (BuildNormalInventoryItem(packet, packet.data + offset, itemInfo)) {
            g_session.SetInventoryItem(itemInfo);
        }
    }
}

void HandleEquipInventoryList(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 4) {
        return;
    }

    size_t entrySize = 0;
    switch (packet.packetId) {
    case 0x00A4:
        entrySize = 20;
        break;
    case 0x01EF:
        entrySize = 24;
        break;
    case 0x02D0:
        entrySize = 26;
        break;
    default:
        return;
    }

    g_session.ClearEquipmentInventoryItems();
    for (size_t offset = 4; offset + entrySize <= static_cast<size_t>(packet.packetLength); offset += entrySize) {
        ITEM_INFO itemInfo;
        if (!BuildEquipInventoryItem(packet, packet.data + offset, itemInfo)) {
            continue;
        }
        g_session.AddInventoryItem(itemInfo);
    }
    g_session.RebuildPlayerEquipmentAppearanceFromInventory();
    SyncSessionAppearanceToLocalPlayer(mode);
}

void HandleItemPickupAck(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 23) {
        return;
    }

    const int result = packet.data[22];
    if (result != 0) {
        return;
    }

    ITEM_INFO itemInfo;
    itemInfo.m_itemIndex = ReadLE16(packet.data + 2);
    itemInfo.m_num = ReadLE16(packet.data + 4);
    itemInfo.SetItemId(ReadLE16(packet.data + 6));
    itemInfo.m_isIdentified = packet.data[8];
    itemInfo.m_isDamaged = packet.data[9];
    itemInfo.m_refiningLevel = packet.data[10];
    itemInfo.m_slot[0] = ReadLE16(packet.data + 11);
    itemInfo.m_slot[1] = ReadLE16(packet.data + 13);
    itemInfo.m_slot[2] = ReadLE16(packet.data + 15);
    itemInfo.m_slot[3] = ReadLE16(packet.data + 17);
    itemInfo.m_location = ReadLE16(packet.data + 19);
    itemInfo.m_wearLocation = 0;
    itemInfo.m_itemType = packet.data[21];
    if (itemInfo.m_itemType == 10 || itemInfo.m_itemType == 16 || itemInfo.m_itemType == 17) {
        itemInfo.m_wearLocation = 0x8000;
    }
    if (packet.packetLength >= 27) {
        itemInfo.m_deleteTime = static_cast<int>(ReadLE32(packet.data + 23));
    }
    if (packet.packetLength >= 29) {
        itemInfo.m_isYours = ReadLE16(packet.data + 27);
    }

    DbgLog("[GameMode] pickup ack pkt=0x%04X index=%u amount=%d item=%u type=%u location=0x%04X wear=0x%04X result=%d expire=%d yours=%u\n",
        static_cast<unsigned int>(packet.packetId),
        static_cast<unsigned int>(itemInfo.m_itemIndex),
        itemInfo.m_num,
        itemInfo.GetItemId(),
        static_cast<unsigned int>(itemInfo.m_itemType),
        static_cast<unsigned int>(itemInfo.m_location),
        static_cast<unsigned int>(itemInfo.m_wearLocation),
        result,
        itemInfo.m_deleteTime,
        static_cast<unsigned int>(itemInfo.m_isYours));

    const u32 pickedObjectAid = mode.m_pickupReqItemNaidList.empty()
        ? 0
        : mode.m_pickupReqItemNaidList.front();
    if (pickedObjectAid != 0) {
        StartLocalPickupAnimation(mode, pickedObjectAid);
    }

    g_session.AddInventoryItem(itemInfo);
    mode.m_pickupReqItemNaidList.clear();
    mode.m_lastPickupRequestTick = 0;
}

void UpsertGroundItem(CGameMode& mode,
    u32 objectId,
    u16 itemId,
    u8 identified,
    u16 tileX,
    u16 tileY,
    u16 amount,
    u8 subX,
    u8 subY,
    bool playDropAnimation)
{
    GroundItemState& item = mode.m_groundItemList[objectId];
    item.objectId = objectId;
    item.itemId = itemId;
    item.identified = identified;
    item.tileX = tileX;
    item.tileY = tileY;
    item.amount = amount;
    item.subX = subX;
    item.subY = subY;
    item.pendingDropAnimation = playDropAnimation ? 1 : 0;
}

void HandleGroundItemEntry(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 17) {
        return;
    }

    const u32 objectId = ReadLE32(packet.data + 2);
    const u16 itemId = ReadLE16(packet.data + 6);
    const u8 identified = packet.data[8];
    const u16 tileX = ReadLE16(packet.data + 9);
    const u16 tileY = ReadLE16(packet.data + 11);

    if (packet.packetId == 0x009D) {
        const u16 amount = ReadLE16(packet.data + 13);
        const u8 subX = packet.data[15];
        const u8 subY = packet.data[16];
        UpsertGroundItem(mode, objectId, itemId, identified, tileX, tileY, amount, subX, subY, false);
        return;
    }

    const u8 subX = packet.data[13];
    const u8 subY = packet.data[14];
    const u16 amount = ReadLE16(packet.data + 15);
    UpsertGroundItem(mode, objectId, itemId, identified, tileX, tileY, amount, subX, subY, true);
}

void HandleGroundItemDisappear(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 6) {
        return;
    }

    const u32 objectId = ReadLE32(packet.data + 2);
    mode.m_groundItemList.erase(objectId);
    mode.m_pickupReqItemNaidList.remove(objectId);
    if (mode.m_pickupReqItemNaidList.empty()) {
        mode.m_lastPickupRequestTick = 0;
    }
}

void HandleItemRemove(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data) {
        return;
    }

    unsigned int itemIndex = 0;
    int amount = 0;
    switch (packet.packetId) {
    case 0x00AF:
        if (packet.packetLength < 6) {
            return;
        }
        itemIndex = ReadLE16(packet.data + 2);
        amount = static_cast<int>(ReadLE16(packet.data + 4));
        break;
    case 0x07FA:
        if (packet.packetLength < 8) {
            return;
        }
        itemIndex = ReadLE16(packet.data + 4);
        amount = static_cast<int>(ReadLE16(packet.data + 6));
        break;
    default:
        return;
    }

    if (mode.m_waitingItemThrowAck == static_cast<int>(itemIndex)) {
        mode.m_waitingItemThrowAck = 0;
    }

    g_session.RemoveInventoryItem(itemIndex, amount);
}

void HandleEquipItemAck(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 7) {
        return;
    }

    const unsigned int itemIndex = ReadLE16(packet.data + 2);
    const int wearLocation = static_cast<int>(ReadLE16(packet.data + 4));
    const int result = packet.data[6];
    if (mode.m_waitingWearEquipAck == itemIndex) {
        mode.m_waitingWearEquipAck = 0;
    }

    if (result == 0 || itemIndex == 0 || wearLocation == 0) {
        DbgLog("[GameMode] equip ack index=%u wear=0x%04X ok=%d\n",
            itemIndex,
            static_cast<unsigned int>(wearLocation),
            result);
        return;
    }

    g_session.ClearInventoryWearLocationMask(wearLocation, itemIndex);
    g_session.SetInventoryItemWearLocation(itemIndex, wearLocation);
    g_session.RebuildPlayerEquipmentAppearanceFromInventory();
    SyncSessionAppearanceToLocalPlayer(mode);
    DbgLog("[GameMode] equip ack index=%u wear=0x%04X ok=%d\n",
        itemIndex,
        static_cast<unsigned int>(wearLocation),
        result);
}

void HandleUnequipItemAck(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 7) {
        return;
    }

    const unsigned int itemIndex = ReadLE16(packet.data + 2);
    const int wearLocation = static_cast<int>(ReadLE16(packet.data + 4));
    const int result = packet.data[6];
    if (mode.m_waitingTakeoffEquipAck == itemIndex) {
        mode.m_waitingTakeoffEquipAck = 0;
    }

    if (result == 0 || itemIndex == 0) {
        DbgLog("[GameMode] unequip ack index=%u wear=0x%04X ok=%d\n",
            itemIndex,
            static_cast<unsigned int>(wearLocation),
            result);
        return;
    }

    g_session.SetInventoryItemWearLocation(itemIndex, 0);
    g_session.RebuildPlayerEquipmentAppearanceFromInventory();
    SyncSessionAppearanceToLocalPlayer(mode);
    DbgLog("[GameMode] unequip ack index=%u wear=0x%04X ok=%d\n",
        itemIndex,
        static_cast<unsigned int>(wearLocation),
        result);
}

bool ApplySelfStatusUpdate(CGameMode& mode, u32 statusType, u32 value)
{
    const bool updatedSession = ApplySelfStatusUpdateToSession(statusType, value);
    if (!mode.m_world || !mode.m_world->m_player) {
        return updatedSession;
    }

    CPlayer* player = mode.m_world->m_player;
    switch (statusType) {
    case kStatusSpeed:
        player->m_speed = value;
        return true;
    case kStatusHp:
        player->m_Hp = ClampToU16(value);
        return true;
    case kStatusMaxHp:
        player->m_MaxHp = ClampToU16(value);
        if (player->m_Hp > player->m_MaxHp) {
            player->m_Hp = player->m_MaxHp;
        }
        return true;
    case kStatusSp:
        player->m_Sp = ClampToU16(value);
        return true;
    case kStatusMaxSp:
        player->m_MaxSp = ClampToU16(value);
        if (player->m_Sp > player->m_MaxSp) {
            player->m_Sp = player->m_MaxSp;
        }
        return true;
    case kStatusBaseLevel:
        player->m_clevel = ClampToU16(value);
        return true;
    case kStatusJobLevel:
        return updatedSession;
    case kStatusBaseExp:
    case kStatusJobExp:
        player->m_Exp = static_cast<int>(value);
        return true;
    default:
        return updatedSession;
    }
}

bool ApplySelfStatUpdate(CGameMode& mode, u32 statusType, u32 baseValue, u32 plusValue)
{
    switch (statusType) {
    case kStatusStr:
        g_session.m_plusStr = static_cast<int>(plusValue);
        break;
    case kStatusAgi:
        g_session.m_plusAgi = static_cast<int>(plusValue);
        break;
    case kStatusVit:
        g_session.m_plusVit = static_cast<int>(plusValue);
        break;
    case kStatusInt:
        g_session.m_plusInt = static_cast<int>(plusValue);
        break;
    case kStatusDex:
        g_session.m_plusDex = static_cast<int>(plusValue);
        break;
    case kStatusLuk:
        g_session.m_plusLuk = static_cast<int>(plusValue);
        break;
    default:
        break;
    }

    CHARACTER_INFO* info = g_session.GetMutableSelectedCharacterInfo();
    if (info) {
        switch (statusType) {
        case kStatusStr:
            info->Str = static_cast<u8>((std::min)(baseValue, static_cast<u32>(0xFFu)));
            break;
        case kStatusAgi:
            info->Agi = static_cast<u8>((std::min)(baseValue, static_cast<u32>(0xFFu)));
            break;
        case kStatusVit:
            info->Vit = static_cast<u8>((std::min)(baseValue, static_cast<u32>(0xFFu)));
            break;
        case kStatusInt:
            info->Int = static_cast<u8>((std::min)(baseValue, static_cast<u32>(0xFFu)));
            break;
        case kStatusDex:
            info->Dex = static_cast<u8>((std::min)(baseValue, static_cast<u32>(0xFFu)));
            break;
        case kStatusLuk:
            info->Luk = static_cast<u8>((std::min)(baseValue, static_cast<u32>(0xFFu)));
            break;
        default:
            break;
        }
    }

    if (!mode.m_world || !mode.m_world->m_player) {
        return info != nullptr;
    }

    const u16 totalValue = ClampToU16(baseValue + plusValue);
    CPlayer* player = mode.m_world->m_player;
    switch (statusType) {
    case kStatusStr:
        player->m_Str = totalValue;
        return true;
    case kStatusAgi:
        player->m_Agi = totalValue;
        return true;
    case kStatusVit:
        player->m_Vit = totalValue;
        return true;
    case kStatusInt:
        player->m_Int = totalValue;
        return true;
    case kStatusDex:
        player->m_Dex = totalValue;
        return true;
    case kStatusLuk:
        player->m_Luk = totalValue;
        return true;
    default:
        return false;
    }
}

void InvalidateStatusWindows()
{
    if (g_windowMgr.m_statusWnd) {
        g_windowMgr.m_statusWnd->Invalidate();
    }
    if (g_windowMgr.m_basicInfoWnd) {
        g_windowMgr.m_basicInfoWnd->Invalidate();
    }
}

void UpdateStatusPointCost(u32 statusType, int value)
{
    const int clamped = (std::max)(0, value);
    switch (statusType) {
    case kStatusNeedStr:
        g_session.m_standardStr = clamped;
        break;
    case kStatusNeedAgi:
        g_session.m_standardAgi = clamped;
        break;
    case kStatusNeedVit:
        g_session.m_standardVit = clamped;
        break;
    case kStatusNeedInt:
        g_session.m_standardInt = clamped;
        break;
    case kStatusNeedDex:
        g_session.m_standardDex = clamped;
        break;
    case kStatusNeedLuk:
        g_session.m_standardLuk = clamped;
        break;
    default:
        break;
    }
}

void UpdateStatusSummaryFromPacket214(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 42) {
        return;
    }

    const std::array<int, 6> baseStats = {
        static_cast<int>(packet.data[2]),
        static_cast<int>(packet.data[4]),
        static_cast<int>(packet.data[6]),
        static_cast<int>(packet.data[8]),
        static_cast<int>(packet.data[10]),
        static_cast<int>(packet.data[12]),
    };
    const std::array<int, 6> statCosts = {
        static_cast<int>(packet.data[3]),
        static_cast<int>(packet.data[5]),
        static_cast<int>(packet.data[7]),
        static_cast<int>(packet.data[9]),
        static_cast<int>(packet.data[11]),
        static_cast<int>(packet.data[13]),
    };

    ApplyStatusBaseAndCostSummary(mode, -1, baseStats, statCosts);
    ApplyCombatStatusSummary(mode,
        static_cast<int>(ReadLE16(packet.data + 14)),
        static_cast<int>(ReadLE16(packet.data + 16)),
        static_cast<int>(ReadLE16(packet.data + 18)),
        static_cast<int>(ReadLE16(packet.data + 20)),
        static_cast<int>(ReadLE16(packet.data + 22)),
        static_cast<int>(ReadLE16(packet.data + 24)),
        static_cast<int>(ReadLE16(packet.data + 26)),
        static_cast<int>(ReadLE16(packet.data + 28)),
        static_cast<int>(ReadLE16(packet.data + 30)),
        static_cast<int>(ReadLE16(packet.data + 32)),
        static_cast<int>(ReadLE16(packet.data + 34)),
        static_cast<int>(ReadLE16(packet.data + 36)),
        static_cast<int>(ReadLE16(packet.data + 38)),
        static_cast<int>(ReadLE16(packet.data + 40)));

    InvalidateStatusWindows();
}

void UpdateStatusSummaryFromPacket00BD(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 44) {
        return;
    }

    const std::array<int, 6> baseStats = {
        static_cast<int>(packet.data[4]),
        static_cast<int>(packet.data[6]),
        static_cast<int>(packet.data[8]),
        static_cast<int>(packet.data[10]),
        static_cast<int>(packet.data[12]),
        static_cast<int>(packet.data[14]),
    };
    const std::array<int, 6> statCosts = {
        static_cast<int>(packet.data[5]),
        static_cast<int>(packet.data[7]),
        static_cast<int>(packet.data[9]),
        static_cast<int>(packet.data[11]),
        static_cast<int>(packet.data[13]),
        static_cast<int>(packet.data[15]),
    };

    ApplyStatusBaseAndCostSummary(mode, static_cast<int>(ReadLE16(packet.data + 2)), baseStats, statCosts);
    ApplyCombatStatusSummary(mode,
        static_cast<int>(ReadLE16(packet.data + 16)),
        static_cast<int>(ReadLE16(packet.data + 18)),
        static_cast<int>(ReadLE16(packet.data + 20)),
        static_cast<int>(ReadLE16(packet.data + 22)),
        static_cast<int>(ReadLE16(packet.data + 24)),
        static_cast<int>(ReadLE16(packet.data + 26)),
        static_cast<int>(ReadLE16(packet.data + 28)),
        static_cast<int>(ReadLE16(packet.data + 30)),
        static_cast<int>(ReadLE16(packet.data + 32)),
        static_cast<int>(ReadLE16(packet.data + 34)),
        static_cast<int>(ReadLE16(packet.data + 36)),
        static_cast<int>(ReadLE16(packet.data + 38)),
        ConvertRawAspdToDisplay(static_cast<int>(ReadLE16(packet.data + 40))),
        static_cast<int>(ReadLE16(packet.data + 42)));

    InvalidateStatusWindows();
}

void HandleSelfStatusParam(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 8) {
        return;
    }

    const u32 statusType = static_cast<u32>(packet.data[2])
        | (static_cast<u32>(packet.data[3]) << 8);
    const u32 value = static_cast<u32>(packet.data[4])
        | (static_cast<u32>(packet.data[5]) << 8)
        | (static_cast<u32>(packet.data[6]) << 16)
        | (static_cast<u32>(packet.data[7]) << 24);

    u32 previousValue = 0;
    bool hadPreviousValue = false;
    if (statusType == kStatusBaseLevel || statusType == kStatusJobLevel) {
        if (const CHARACTER_INFO* info = g_session.GetSelectedCharacterInfo()) {
            previousValue = (statusType == kStatusBaseLevel)
                ? static_cast<u32>((std::max)(0, static_cast<int>(info->level)))
                : static_cast<u32>((std::max)(0, info->joblevel));
            hadPreviousValue = previousValue > 0;
        }
    }

    if (ApplySelfStatusUpdate(mode, statusType, value)) {
        CPlayer* player = mode.m_world ? mode.m_world->m_player : nullptr;
        DbgLog("[GameMode] self status type=%u value=%u hp=%u/%u sp=%u/%u\n",
            statusType,
            value,
            static_cast<unsigned int>(player ? player->m_Hp : 0),
            static_cast<unsigned int>(player ? player->m_MaxHp : 0),
            static_cast<unsigned int>(player ? player->m_Sp : 0),
            static_cast<unsigned int>(player ? player->m_MaxSp : 0));
        InvalidateStatusWindows();

        if (hadPreviousValue && value > previousValue) {
            if (statusType == kStatusBaseLevel) {
                g_lastSelfLevelUpStatusTick = GetTickCount();
                g_lastSelfLevelUpStatusType = statusType;
                PlayBaseLevelUpPresentation(mode);
                if (mode.m_world && mode.m_world->m_player) {
                    const u32 effectId = ResolveLocalBaseLevelUpEffectId(mode);
                    DbgLog("[GameMode] local base level-up old=%u new=%u effect=%u\n",
                        static_cast<unsigned int>(previousValue),
                        static_cast<unsigned int>(value),
                        static_cast<unsigned int>(effectId));
                    if (!ShouldSuppressDuplicateLevelUpEffect(effectId)) {
                        LaunchLevelUpEffect(mode.m_world->m_player, effectId);
                        RecordLocalLevelUpEffect(effectId);
                    } else {
                        DbgLog("[GameMode] local base level-up VFX skipped (duplicate of recent notify effect=%u)\n",
                            static_cast<unsigned int>(effectId));
                    }
                }
            } else if (statusType == kStatusJobLevel) {
                g_lastSelfLevelUpStatusTick = GetTickCount();
                g_lastSelfLevelUpStatusType = statusType;
                PlayJobLevelUpPresentation(mode);
                if (mode.m_world && mode.m_world->m_player) {
                    const u32 effectId = ResolveLocalJobLevelUpEffectId(mode);
                    DbgLog("[GameMode] local job level-up old=%u new=%u effect=%u\n",
                        static_cast<unsigned int>(previousValue),
                        static_cast<unsigned int>(value),
                        static_cast<unsigned int>(effectId));
                    if (!ShouldSuppressDuplicateLevelUpEffect(effectId)) {
                        LaunchLevelUpEffect(mode.m_world->m_player, effectId);
                        RecordLocalLevelUpEffect(effectId);
                    } else {
                        DbgLog("[GameMode] local job level-up VFX skipped (duplicate of recent notify effect=%u)\n",
                            static_cast<unsigned int>(effectId));
                    }
                }
            }
        }
    }
}

void HandleSelfStatInfo(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 14) {
        return;
    }

    const u32 statusType = ReadLE32(packet.data + 2);
    const u32 baseValue = ReadLE32(packet.data + 6);
    const u32 plusValue = ReadLE32(packet.data + 10);
    if (ApplySelfStatUpdate(mode, statusType, baseValue, plusValue)) {
        CPlayer* player = mode.m_world ? mode.m_world->m_player : nullptr;
        DbgLog("[GameMode] self stat type=%u base=%u plus=%u str=%u agi=%u vit=%u int=%u dex=%u luk=%u\n",
            statusType,
            baseValue,
            plusValue,
            static_cast<unsigned int>(player ? player->m_Str : 0),
            static_cast<unsigned int>(player ? player->m_Agi : 0),
            static_cast<unsigned int>(player ? player->m_Vit : 0),
            static_cast<unsigned int>(player ? player->m_Int : 0),
            static_cast<unsigned int>(player ? player->m_Dex : 0),
            static_cast<unsigned int>(player ? player->m_Luk : 0));
        InvalidateStatusWindows();
    }
}

void HandleStatusSummary(CGameMode& mode, const PacketView& packet)
{
    UpdateStatusSummaryFromPacket214(mode, packet);
}

void HandleInitialStatusSummary(CGameMode& mode, const PacketView& packet)
{
    UpdateStatusSummaryFromPacket00BD(mode, packet);
}

void HandleStatusChangeAck(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 6) {
        return;
    }

    const u32 statusType = ReadLE16(packet.data + 2);
    const bool ok = packet.data[4] != 0;
    const u32 value = static_cast<u32>(packet.data[5]);
    if (!ok) {
        return;
    }

    if (ApplyCombatStatusParamToSession(statusType, static_cast<int>(value), true)) {
        InvalidateStatusWindows();
        return;
    }

    CHARACTER_INFO* info = g_session.GetMutableSelectedCharacterInfo();
    if (info) {
        switch (statusType) {
        case kStatusStr:
            info->Str = static_cast<u8>(value);
            break;
        case kStatusAgi:
            info->Agi = static_cast<u8>(value);
            break;
        case kStatusVit:
            info->Vit = static_cast<u8>(value);
            break;
        case kStatusInt:
            info->Int = static_cast<u8>(value);
            break;
        case kStatusDex:
            info->Dex = static_cast<u8>(value);
            break;
        case kStatusLuk:
            info->Luk = static_cast<u8>(value);
            break;
        default:
            break;
        }
    }

    SyncLocalPlayerPrimaryStatsFromSession(mode);
    InvalidateStatusWindows();
}

void HandleStatusPointCostUpdate(CGameMode& mode, const PacketView& packet)
{
    (void)mode;
    if (!packet.data || packet.packetLength < 5) {
        return;
    }

    const u32 statusType = ReadLE16(packet.data + 2);
    const int value = static_cast<int>(packet.data[4]);
    UpdateStatusPointCost(statusType, value);
    InvalidateStatusWindows();
}

void HandleNotifyTime(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 6) {
        return;
    }

    const u32 serverTick = static_cast<u32>(packet.data[2])
        | (static_cast<u32>(packet.data[3]) << 8)
        | (static_cast<u32>(packet.data[4]) << 16)
        | (static_cast<u32>(packet.data[5]) << 24);
    g_session.SetServerTime(serverTick);
    mode.m_receiveSyneRequestTime = static_cast<int>(timeGetTime());
    ++mode.m_numNotifyTime;
}

u16 ReadLE16(const u8* p)
{
    return static_cast<u16>(p[0] | (static_cast<u16>(p[1]) << 8));
}

u32 ReadLE32(const u8* p)
{
    return static_cast<u32>(p[0]) |
           (static_cast<u32>(p[1]) << 8) |
           (static_cast<u32>(p[2]) << 16) |
           (static_cast<u32>(p[3]) << 24);
}

bool EndsWithMapExtension(const std::string& value, const char* extension)
{
    if (!extension) {
        return false;
    }

    const size_t extensionLen = std::strlen(extension);
    if (value.size() < extensionLen) {
        return false;
    }

    const size_t offset = value.size() - extensionLen;
    for (size_t i = 0; i < extensionLen; ++i) {
        const char lhs = static_cast<char>(std::tolower(static_cast<unsigned char>(value[offset + i])));
        const char rhs = static_cast<char>(std::tolower(static_cast<unsigned char>(extension[i])));
        if (lhs != rhs) {
            return false;
        }
    }

    return true;
}

std::string NormalizeMapName(std::string mapName)
{
    while (!mapName.empty()) {
        if (EndsWithMapExtension(mapName, ".rsw") ||
            EndsWithMapExtension(mapName, ".gat") ||
            EndsWithMapExtension(mapName, ".gnd")) {
            mapName.resize(mapName.size() - 4);
            continue;
        }
        break;
    }

    return mapName;
}

std::string ExtractPacketString(const PacketView& packet, size_t offset)
{
    if (!packet.data || packet.packetLength <= offset) {
        return {};
    }

    const size_t maxLen = static_cast<size_t>(packet.packetLength) - offset;
    size_t len = 0;
    while (len < maxLen && packet.data[offset + len] != 0) {
        ++len;
    }

    return std::string(reinterpret_cast<const char*>(packet.data + offset), len);
}

std::string ExtractFixedPacketString(const PacketView& packet, size_t offset, size_t fieldSize)
{
    if (!packet.data || packet.packetLength <= offset || fieldSize == 0) {
        return std::string();
    }

    const size_t available = static_cast<size_t>(packet.packetLength) - offset;
    const size_t maxLen = (std::min)(fieldSize, available);
    size_t len = 0;
    while (len < maxLen && packet.data[offset + len] != 0) {
        ++len;
    }

    return std::string(reinterpret_cast<const char*>(packet.data + offset), len);
}

std::vector<std::string> SplitNpcMenuOptions(const std::string& text)
{
    std::vector<std::string> options;
    size_t start = 0;
    while (start <= text.size()) {
        const size_t separator = text.find(':', start);
        const size_t end = (separator == std::string::npos) ? text.size() : separator;
        if (end > start) {
            options.push_back(text.substr(start, end - start));
        }
        if (separator == std::string::npos) {
            break;
        }
        start = separator + 1;
    }
    return options;
}

void CloseNpcShopWindows()
{
    g_windowMgr.CloseNpcShopWindows();
}

void PushShopResultMessage(const char* text, u32 color)
{
    if (!text || *text == '\0') {
        return;
    }
    g_windowMgr.PushChatEvent(text, color, 6);
}

void HandleNpcDialogText(CGameMode&, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 8) {
        return;
    }

    const u32 npcId = ReadLE32(packet.data + 4);
    auto* sayWnd = static_cast<UISayDialogWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_SAYDIALOGWND));
    if (!sayWnd) {
        return;
    }
    sayWnd->AppendText(npcId, ExtractPacketString(packet, 8));
}

void HandleNpcDialogNext(CGameMode&, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 6) {
        return;
    }

    const u32 npcId = ReadLE32(packet.data + 2);
    if (g_windowMgr.m_npcMenuWnd) {
        g_windowMgr.m_npcMenuWnd->HideMenu();
    }
    if (g_windowMgr.m_npcInputWnd) {
        g_windowMgr.m_npcInputWnd->HideInput();
    }

    auto* sayWnd = static_cast<UISayDialogWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_SAYDIALOGWND));
    if (sayWnd) {
        sayWnd->ShowNext(npcId);
    }
}

void HandleNpcDialogClose(CGameMode&, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 6) {
        return;
    }

    const u32 npcId = ReadLE32(packet.data + 2);
    if (g_windowMgr.m_npcMenuWnd) {
        g_windowMgr.m_npcMenuWnd->HideMenu();
    }
    if (g_windowMgr.m_npcInputWnd) {
        g_windowMgr.m_npcInputWnd->HideInput();
    }

    if (g_windowMgr.m_sayDialogWnd && g_windowMgr.m_sayDialogWnd->m_show != 0) {
        g_windowMgr.m_sayDialogWnd->ShowClose(npcId);
        return;
    }

    g_windowMgr.CloseNpcDialogWindows();
}

void HandleNpcDialogMenu(CGameMode&, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 8) {
        return;
    }

    const u32 npcId = ReadLE32(packet.data + 4);
    const std::vector<std::string> options = SplitNpcMenuOptions(ExtractPacketString(packet, 8));
    if (g_windowMgr.m_sayDialogWnd) {
        g_windowMgr.m_sayDialogWnd->ClearAction();
    }
    if (g_windowMgr.m_npcInputWnd) {
        g_windowMgr.m_npcInputWnd->HideInput();
    }

    auto* menuWnd = static_cast<UINpcMenuWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_NPCMENUWND));
    if (menuWnd) {
        menuWnd->SetMenu(npcId, options);
    }
}

void HandleNpcDialogNumberInput(CGameMode&, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 6) {
        return;
    }

    const u32 npcId = ReadLE32(packet.data + 2);
    if (g_windowMgr.m_sayDialogWnd) {
        g_windowMgr.m_sayDialogWnd->ClearAction();
    }
    if (g_windowMgr.m_npcMenuWnd) {
        g_windowMgr.m_npcMenuWnd->HideMenu();
    }

    auto* inputWnd = static_cast<UINpcInputWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_NPCINPUTWND));
    if (inputWnd) {
        inputWnd->OpenNumber(npcId);
    }
}

void HandleNpcDialogStringInput(CGameMode&, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 6) {
        return;
    }

    const u32 npcId = ReadLE32(packet.data + 2);
    if (g_windowMgr.m_sayDialogWnd) {
        g_windowMgr.m_sayDialogWnd->ClearAction();
    }
    if (g_windowMgr.m_npcMenuWnd) {
        g_windowMgr.m_npcMenuWnd->HideMenu();
    }

    auto* inputWnd = static_cast<UINpcInputWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_NPCINPUTWND));
    if (inputWnd) {
        inputWnd->OpenString(npcId);
    }
}

void HandleNpcShopDealType(CGameMode&, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 6) {
        return;
    }

    const u32 npcId = ReadLE32(packet.data + 2);
    g_session.SetNpcShopChoice(npcId);
    CloseNpcShopWindows();
    g_windowMgr.MakeWindow(UIWindowMgr::WID_CHOOSESELLBUYWND);
}

void HandleNpcShopBuyList(CGameMode&, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 4) {
        return;
    }

    std::vector<NPC_SHOP_ROW> rows;
    const size_t payloadLength = static_cast<size_t>(packet.packetLength - 4);
    rows.reserve(payloadLength / 11);
    for (size_t offset = 4; offset + 11 <= static_cast<size_t>(packet.packetLength); offset += 11) {
        NPC_SHOP_ROW row{};
        row.price = static_cast<int>(ReadLE32(packet.data + offset + 0));
        row.secondaryPrice = static_cast<int>(ReadLE32(packet.data + offset + 4));
        row.itemInfo.m_itemType = packet.data[offset + 8];
        row.itemInfo.SetItemId(ReadLE16(packet.data + offset + 9));
        row.itemInfo.m_isIdentified = 1;
        rows.push_back(std::move(row));
    }

    g_session.SetNpcShopRows(g_session.m_shopNpcId, NpcShopMode::Buy, rows);
    if (g_windowMgr.m_chooseSellBuyWnd) {
        g_windowMgr.DeleteWindow(g_windowMgr.m_chooseSellBuyWnd);
    }
    g_windowMgr.MakeWindow(UIWindowMgr::WID_ITEMSHOPWND);
    g_windowMgr.MakeWindow(UIWindowMgr::WID_ITEMPURCHASEWND);
}

void HandleNpcShopSellList(CGameMode&, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 4) {
        return;
    }

    std::vector<NPC_SHOP_ROW> rows;
    const size_t payloadLength = static_cast<size_t>(packet.packetLength - 4);
    rows.reserve(payloadLength / 10);
    for (size_t offset = 4; offset + 10 <= static_cast<size_t>(packet.packetLength); offset += 10) {
        const unsigned int itemIndex = ReadLE16(packet.data + offset + 0);
        const ITEM_INFO* itemInfo = g_session.GetInventoryItemByIndex(itemIndex);
        if (!itemInfo || itemInfo->m_wearLocation != 0) {
            continue;
        }

        NPC_SHOP_ROW row{};
        row.itemInfo = *itemInfo;
        row.sourceItemIndex = itemIndex;
        row.price = static_cast<int>(ReadLE32(packet.data + offset + 2));
        row.secondaryPrice = static_cast<int>(ReadLE32(packet.data + offset + 6));
        row.availableCount = itemInfo->m_num;
        rows.push_back(std::move(row));
    }

    g_session.SetNpcShopRows(g_session.m_shopNpcId, NpcShopMode::Sell, rows);
    if (g_windowMgr.m_chooseSellBuyWnd) {
        g_windowMgr.DeleteWindow(g_windowMgr.m_chooseSellBuyWnd);
    }
    g_windowMgr.MakeWindow(UIWindowMgr::WID_ITEMSHOPWND);
    g_windowMgr.MakeWindow(UIWindowMgr::WID_ITEMSELLWND);
}

void HandleNpcShopBuyResult(CGameMode&, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 3) {
        return;
    }

    switch (packet.data[2]) {
    case 0:
        PushShopResultMessage("The deal has successfully completed.", 0x00FFFF00u);
        break;
    case 1:
        PushShopResultMessage("You do not have enough zeny.", 0x000000FFu);
        break;
    case 2:
        PushShopResultMessage("You cannot carry more items because you are overweight.", 0x000000FFu);
        break;
    case 3:
        PushShopResultMessage("You cannot carry any more items.", 0x000000FFu);
        break;
    default:
        break;
    }

    CloseNpcShopWindows();
    g_session.ClearNpcShopState();
}

void HandleNpcShopSellResult(CGameMode&, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 3) {
        return;
    }

    if (packet.data[2] == 0) {
        PushShopResultMessage("The deal has successfully completed.", 0x00FFFF00u);
    } else if (packet.data[2] == 1) {
        PushShopResultMessage("The deal has failed.", 0x000000FFu);
    }

    CloseNpcShopWindows();
    g_session.ClearNpcShopState();
}

std::string SanitizeNpcDisplayName(std::string name)
{
    const size_t hashPos = name.find('#');
    if (hashPos != std::string::npos) {
        name.resize(hashPos);
    }
    return name;
}

enum : u8 {
    kChatChannelNormal = 0,
    kChatChannelPlayer = 1,
    kChatChannelWhisper = 2,
    kChatChannelParty = 3,
    kChatChannelBroadcast = 4,
    kChatChannelBattlefield = 5,
    kChatChannelSystem = 6,
};

struct ChatEntry {
    std::string text;
    u32 color;
    u8 channel;
};

struct BroadcastPayload {
    std::string text;
    u32 color;
};

struct MapChangeInfo {
    std::string mapName;
    int x = 0;
    int y = 0;
    u32 ip = 0;
    u16 port = 0;
    bool hasPosition = false;
    bool hasServerMove = false;
};

bool ParseHexColor(const std::string& src, size_t offset, u32& outColor)
{
    if (src.size() < offset + 6) {
        return false;
    }

    char hex[7] = {};
    std::memcpy(hex, src.data() + offset, 6);
    unsigned int parsed = 0;
    if (std::sscanf(hex, "%x", &parsed) != 1) {
        return false;
    }

    outColor = parsed & 0x00FFFFFFu;
    return true;
}

bool ParseMapChangePacket(const PacketView& packet, MapChangeInfo& outInfo)
{
    outInfo = {};

    if (!packet.data || packet.packetLength < 22) {
        return false;
    }

    constexpr size_t kMapNameBytes = 16;
    const char* rawMapName = reinterpret_cast<const char*>(packet.data + 2);
    size_t mapNameLen = 0;
    while (mapNameLen < kMapNameBytes && rawMapName[mapNameLen] != '\0') {
        ++mapNameLen;
    }
    outInfo.mapName = NormalizeMapName(std::string(rawMapName, mapNameLen));
    outInfo.x = static_cast<int>(ReadLE16(packet.data + 18));
    outInfo.y = static_cast<int>(ReadLE16(packet.data + 20));
    outInfo.hasPosition = true;

    if (packet.packetId == 0x0092 && packet.packetLength >= 28) {
        outInfo.ip = ReadLE32(packet.data + 22);
        outInfo.port = ReadLE16(packet.data + 26);
        outInfo.hasServerMove = outInfo.ip != 0 && outInfo.port != 0;
    }

    return !outInfo.mapName.empty();
}

bool SendLoadEndAckPacket()
{
    const u16 opcode = PacketProfile::ActiveMapServerSend::kNotifyActorInit;
    u8 packet[2] = {
        static_cast<u8>(opcode & 0xFFu),
        static_cast<u8>((opcode >> 8) & 0xFFu)
    };

    const bool sent = CRagConnection::instance()->SendPacket(
        reinterpret_cast<const char*>(packet),
        static_cast<int>(sizeof(packet)));
    DbgLog("[GameMode] load end ack opcode=0x%04X sent=%d\n",
        opcode,
        sent ? 1 : 0);
    return sent;
}

bool IsSameMapAlreadyLoaded(const CGameMode& mode, const MapChangeInfo& info)
{
    if (info.mapName.empty() || !mode.m_world || !mode.m_world->m_player || mode.m_rswName[0] == '\0') {
        return false;
    }

    const std::string currentMap = ToLowerAsciiStatus(NormalizeMapName(std::string(mode.m_rswName)));
    const std::string targetMap = ToLowerAsciiStatus(info.mapName);
    return !currentMap.empty() && currentMap == targetMap;
}

void PrepareSameMapWarpReuse(CGameMode& mode)
{
    CPlayer* const player = mode.m_world ? mode.m_world->m_player : nullptr;
    if (!player) {
        return;
    }

    for (auto it = mode.m_runtimeActors.begin(); it != mode.m_runtimeActors.end(); ) {
        CGameActor* actor = it->second;
        if (!actor || actor == player || it->first == g_session.m_gid || it->first == g_session.m_aid) {
            ++it;
            continue;
        }

        actor->UnRegisterPos();
        delete actor;
        it = mode.m_runtimeActors.erase(it);
    }

    mode.m_actorPosList.clear();
    mode.m_aidList.clear();
    mode.m_actorNameList.clear();
    mode.m_actorNameReqTimer.clear();
    mode.m_actorNameListByGID.clear();
    mode.m_actorNameByGIDReqTimer.clear();
    mode.m_partyPosList.clear();
    mode.m_guildPosList.clear();
    mode.m_compassPosList.clear();
    mode.m_pickupReqItemNaidList.clear();
    mode.m_groundItemList.clear();

    if (mode.m_world) {
        for (CItem* item : mode.m_world->m_itemList) {
            delete item;
        }
        mode.m_world->m_itemList.clear();
    }

    player->m_targetGid = 0;
    player->m_proceedTargetGid = 0;
    player->m_zoom = 1.0f;
    player->m_shadowOn = 1;
    player->m_shadowZoom = 1.0f;
    player->m_isMoving = 0;
    player->m_isWaitingMoveAck = 0;
    player->m_path.Reset();
    player->m_moveStartTime = 0;
    player->m_moveEndTime = 0;
    player->m_moveSrcX = g_session.m_playerPosX;
    player->m_moveSrcY = g_session.m_playerPosY;
    player->m_moveDestX = g_session.m_playerPosX;
    player->m_moveDestY = g_session.m_playerPosY;
    player->m_lastTlvertX = g_session.m_playerPosX;
    player->m_lastTlvertY = g_session.m_playerPosY;
    player->m_pos.x = TileToWorldCoordX(mode.m_world, g_session.m_playerPosX);
    player->m_pos.z = TileToWorldCoordZ(mode.m_world, g_session.m_playerPosY);
    player->m_pos.y = mode.m_world && mode.m_world->m_attr
        ? mode.m_world->m_attr->GetHeight(player->m_pos.x, player->m_pos.z)
        : 0.0f;
    player->m_moveStartPos = player->m_pos;
    player->m_moveEndPos = player->m_pos;
    player->m_roty = PacketDirToRotationDegrees(g_session.m_playerDir);
    if (CPc* pc = dynamic_cast<CPc*>(player)) {
        pc->InvalidateBillboard();
    }

    mode.m_runtimeActors[g_session.m_gid] = player;
    mode.m_actorPosList[g_session.m_gid] = CellPos{ g_session.m_playerPosX, g_session.m_playerPosY };
    mode.m_aidList[g_session.m_gid] = GetTickCount();
    mode.m_lastPcGid = g_session.m_gid;
    mode.m_lastMonGid = 0;
    mode.m_lastLockOnMonGid = 0;
    mode.m_lastMoveRequestCellX = -1;
    mode.m_lastMoveRequestCellY = -1;
    mode.m_heldMoveTargetCellX = -1;
    mode.m_heldMoveTargetCellY = -1;
    mode.m_hasHeldMoveTarget = 0;
    mode.m_lastMoveRequestTick = 0;
    mode.m_lastAttackRequestTick = 0;
    mode.m_lastPickupRequestTick = 0;
    mode.m_attackChaseTargetGid = 0;
    mode.m_attackChaseTargetCellX = 0;
    mode.m_attackChaseTargetCellY = 0;
    mode.m_attackChaseSourceCellX = 0;
    mode.m_attackChaseSourceCellY = 0;
    mode.m_attackChaseRange = 0;
    mode.m_hasAttackChaseHint = 0;
    mode.m_isLeftButtonHeld = 0;
    mode.m_sentLoadEndAck = 0;
    mode.m_mapLoadingStage = CGameMode::MapLoading_None;
    mode.m_mapLoadingStartTick = 0;
    mode.m_mapLoadingAckTick = 0;
    mode.m_lastActorBootstrapPacketTick = GetTickCount();
}

constexpr u8 kNotifyActDamage = 0;
constexpr u8 kNotifyActEndureDamage = 4;
constexpr u8 kNotifyActMultiHitDamage = 8;
constexpr u8 kNotifyActMultiHitEndure = 9;
constexpr u8 kNotifyActCriticalDamage = 10;
constexpr u8 kNotifyActLuckyDodge = 11;
constexpr int kDefaultAttackMotionTime = 1440;
constexpr u32 kRefDoubleAttackTermMs = 200;

struct ScheduledAttackHit {
    int damage = 0;
    u32 delayMs = 0;
};

bool IsAttackNotifyType(u8 actionType)
{
    switch (actionType) {
    case kNotifyActDamage:
    case kNotifyActEndureDamage:
    case kNotifyActMultiHitDamage:
    case kNotifyActMultiHitEndure:
    case kNotifyActCriticalDamage:
    case kNotifyActLuckyDodge:
        return true;
    default:
        return false;
    }
}

u32 ResolveCombatNumberColor(int damage, u8 actionType)
{
    if (damage < 0) {
        return 0xFF00FF00u;
    }

    switch (actionType) {
    case kNotifyActCriticalDamage:
        return 0xFFFFFF00u;
    default:
        return 0xFFFFFFFFu;
    }
}

int ResolveCombatNumberKind(int damage, u8 actionType)
{
    if (damage < 0) {
        return 22;
    }

    switch (actionType) {
    case kNotifyActCriticalDamage:
        return 16;
    default:
        return 14;
    }
}

u16 ResolveAttackRepeatHitCount(u8 actionType, u16 div)
{
    if (actionType == kNotifyActMultiHitDamage || actionType == kNotifyActMultiHitEndure) {
        return static_cast<u16>((std::max)(1, (std::min)(36, static_cast<int>(div))));
    }

    return 1;
}

u32 ResolveAttackImpactDelayMs(const CGameActor& actor)
{
    u32 delayMs = static_cast<u32>((std::max)(0.0f, actor.m_attackMotion * actor.m_motionSpeed * 24.0f));
    switch (actor.m_job) {
    case 1016:
    case 1420:
        delayMs += 192;
        break;
    case 1285:
    case 1830:
        delayMs += 912;
        break;
    case 1286:
    case 1287:
    case 1829:
        delayMs += 408;
        break;
    default:
        break;
    }
    return delayMs;
}

bool IsDualWeaponPcJob(int job)
{
    return job == 12 || job == 4013 || job == 4035;
}

int ResolveCombatHitWaveWeaponType(const CGameActor& sourceActor)
{
    int weaponValue = 0;

    if (const CPc* pcActor = dynamic_cast<const CPc*>(&sourceActor)) {
        weaponValue = pcActor->m_weapon & 0xFFFF;
        if (IsDualWeaponPcJob(sourceActor.m_job) && pcActor->m_shield != 0) {
            weaponValue = (pcActor->m_weapon & 0xFFFF) | ((pcActor->m_shield & 0xFFFF) << 16);
        }
    } else if (const CGrannyPc* grannyPcActor = dynamic_cast<const CGrannyPc*>(&sourceActor)) {
        weaponValue = grannyPcActor->m_weapon & 0xFFFF;
        if (IsDualWeaponPcJob(sourceActor.m_job) && grannyPcActor->m_shield != 0) {
            weaponValue = (grannyPcActor->m_weapon & 0xFFFF) | ((grannyPcActor->m_shield & 0xFFFF) << 16);
        }
    }

    const bool isLocalPlayer = sourceActor.m_gid != 0
        && (sourceActor.m_gid == g_session.m_gid || sourceActor.m_gid == g_session.m_aid);
    if (isLocalPlayer) {
        const int localWeaponValue = g_session.GetCurrentPlayerWeaponValue();
        if (localWeaponValue != 0) {
            weaponValue = localWeaponValue;
        }
    }

    if (weaponValue == 0) {
        return -1;
    }

    if (IsDualWeaponPcJob(sourceActor.m_job)) {
        const int primaryWeaponValue = weaponValue & 0xFFFF;
        if (primaryWeaponValue == 0) {
            return -1;
        }

        int primaryWeaponType = g_session.ResolvePackedWeaponType(sourceActor.m_job, primaryWeaponValue);
        if (primaryWeaponType <= 0 || primaryWeaponType >= 31) {
            primaryWeaponType = g_session.ResolvePackedWeaponType(0, primaryWeaponValue);
        }

        return (primaryWeaponType > 0 && primaryWeaponType < 31) ? primaryWeaponType : -1;
    }

    int weaponType = g_session.ResolvePackedWeaponType(sourceActor.m_job, weaponValue);
    if (weaponType <= 0 || weaponType >= 31) {
        weaponType = g_session.ResolvePackedWeaponType(sourceActor.m_job, weaponValue & 0xFFFF);
    }

    return (weaponType > 0 && weaponType < 31) ? weaponType : -1;
}

void EmitCombatNumber(CGameActor* sourceActor, CGameActor* targetActor, int damage, u8 actionType)
{
    if (!targetActor || damage == 0 || actionType == kNotifyActLuckyDodge) {
        return;
    }

    const int numberValue = damage < 0 ? -damage : damage;
    if (numberValue <= 0) {
        return;
    }

    const bool isLocalPlayerTarget = targetActor->m_gid == g_session.m_gid || targetActor->m_gid == g_session.m_aid;
    const bool isLocalPlayerSource = sourceActor && (sourceActor->m_gid == g_session.m_gid || sourceActor->m_gid == g_session.m_aid);
    const u32 numberColor = (isLocalPlayerTarget && !isLocalPlayerSource)
        ? 0xFFFF4040u
        : ResolveCombatNumberColor(damage, actionType);

    targetActor->SendMsg(sourceActor,
        88,
        numberValue,
        static_cast<int>(numberColor),
        ResolveCombatNumberKind(damage, actionType));
}

void AppendCombatDamageHits(std::vector<int>& hitDamages, int damage, u16 hitCountRaw)
{
    if (damage == 0) {
        return;
    }

    const int hitCount = (std::max)(1, static_cast<int>(hitCountRaw));
    if (hitCount <= 1) {
        hitDamages.push_back(damage);
        return;
    }

    const int absDamage = damage < 0 ? -damage : damage;
    const int sign = damage < 0 ? -1 : 1;
    const int basePerHit = absDamage / hitCount;
    int remainder = absDamage % hitCount;
    for (int hitIndex = 0; hitIndex < hitCount; ++hitIndex) {
        int perHit = basePerHit;
        if (remainder > 0) {
            ++perHit;
            --remainder;
        }
        hitDamages.push_back(perHit * sign);
    }
}

std::vector<int> ResolveAttackHitDamages(int damage, int leftDamage, u16 hitCountRaw)
{
    std::vector<int> hitDamages;
    hitDamages.reserve((std::max)(1, static_cast<int>(hitCountRaw)) + (leftDamage != 0 ? 1 : 0));
    AppendCombatDamageHits(hitDamages, damage, hitCountRaw);
    if (leftDamage != 0) {
        hitDamages.push_back(leftDamage);
    }
    return hitDamages;
}

std::vector<ScheduledAttackHit> ResolveAttackHitSchedule(const CGameActor& sourceActor,
    int damage,
    int leftDamage,
    u16 div,
    u8 actionType)
{
    std::vector<ScheduledAttackHit> scheduledHits;
    const u16 repeatHitCount = ResolveAttackRepeatHitCount(actionType, div);
    const u32 impactDelayMs = ResolveAttackImpactDelayMs(sourceActor);
    const bool hasRefOffhandFollowup = leftDamage != 0
        && (sourceActor.m_job < 1000 || sourceActor.m_job > 4000);

    if (damage != 0) {
        if (repeatHitCount <= 1) {
            scheduledHits.push_back({ damage, impactDelayMs });
        } else {
            const int perHitDamage = damage / static_cast<int>(repeatHitCount);
            scheduledHits.reserve(static_cast<size_t>(repeatHitCount) + (hasRefOffhandFollowup ? 1u : 0u));
            scheduledHits.push_back({ perHitDamage, impactDelayMs });

            if (hasRefOffhandFollowup) {
                const u32 followupDelayMs = impactDelayMs + kRefDoubleAttackTermMs / 2;
                for (u16 hitIndex = 1; hitIndex < repeatHitCount; ++hitIndex) {
                    scheduledHits.push_back({ perHitDamage, followupDelayMs });
                }
            } else {
                for (u16 hitIndex = 1; hitIndex < repeatHitCount; ++hitIndex) {
                    scheduledHits.push_back({ perHitDamage, impactDelayMs + kRefDoubleAttackTermMs * hitIndex });
                }
            }
        }
    }

    if (hasRefOffhandFollowup) {
        const u32 offhandDelayMs = impactDelayMs + kRefDoubleAttackTermMs / 4 + kRefDoubleAttackTermMs + kRefDoubleAttackTermMs / 2;
        scheduledHits.push_back({ leftDamage, offhandDelayMs });
    }

    return scheduledHits;
}

void PopulateCombatHitWaveName(CGameActor* sourceActor, CGameActor* targetActor, char* waveName, size_t waveNameSize)
{
    if (!waveName || waveNameSize == 0) {
        return;
    }

    const char* resolvedWaveName = nullptr;
    if (targetActor && targetActor->m_isPc != 0) {
        resolvedWaveName = g_session.GetJobHitWaveName(targetActor->m_job);
    } else if (sourceActor) {
        const int weaponType = ResolveCombatHitWaveWeaponType(*sourceActor);
        resolvedWaveName = g_session.GetWeaponHitWaveName(weaponType);
    } else {
        resolvedWaveName = g_session.GetWeaponHitWaveName(-1);
    }

    if (resolvedWaveName && *resolvedWaveName) {
        std::snprintf(waveName, waveNameSize, "%s", resolvedWaveName);
    }
}

void EmitCombatNumbers(CGameActor* sourceActor, CGameActor* targetActor, const std::vector<int>& hitDamages, u8 actionType)
{
    for (int hitDamage : hitDamages) {
        EmitCombatNumber(sourceActor, targetActor, hitDamage, actionType);
    }
}

u16 ResolveDisplayedSkillHitCount(u16 skillId, u16 level, u16 div)
{
    if (div > 1) {
        return div;
    }

    switch (skillId) {
    case 17: // MG_FIREBOLT
    case 19: // MG_COLDBOLT
    case 20: // MG_LIGHTNINGBOLT
        return level > 1 ? level : 1;
    default:
        return div > 0 ? div : 1;
    }
}

void EmitCombatNumbers(CGameActor* sourceActor, CGameActor* targetActor, int damage, u8 actionType, u16 hitCountRaw)
{
    std::vector<int> hitDamages;
    hitDamages.reserve((std::max)(1, static_cast<int>(hitCountRaw)));
    AppendCombatDamageHits(hitDamages, damage, hitCountRaw);
    EmitCombatNumbers(sourceActor, targetActor, hitDamages, actionType);
}

void EmitSkillImpactEffect(CGameActor* targetActor, u16 skillId)
{
    if (!targetActor) {
        return;
    }

    LuaSkillEffectInfo luaInfo;
    if (g_buabridge.GetSkillEffectInfoBySkillId(static_cast<int>(skillId), &luaInfo) && !luaInfo.effectIds.empty()) {
        for (int effectId : luaInfo.effectIds) {
            if (effectId > 0) {
                targetActor->LaunchEffect(effectId, vector3d{}, 0.0f);
            }
        }
        return;
    }

    switch (skillId) {
    case 17: // MG_FIREBOLT
        targetActor->LaunchEffect(49, vector3d{}, 0.0f);
        break;
    default:
        break;
    }
}

void EmitSkillImpactEffects(CGameActor* targetActor, u16 skillId, u16 hitCountRaw)
{
    const int hitCount = (std::max)(1, static_cast<int>(hitCountRaw));
    for (int hitIndex = 0; hitIndex < hitCount; ++hitIndex) {
        EmitSkillImpactEffect(targetActor, skillId);
    }
}

void QueueCombatHitReaction(CGameActor* sourceActor, CGameActor* targetActor, int attackedMT, int damage, u8 actionType)
{
    if (!targetActor || damage == 0 || actionType == kNotifyActLuckyDodge) {
        return;
    }

    WBA hitInfo{};
    hitInfo.gid = sourceActor ? sourceActor->m_gid : 0;
    hitInfo.time = timeGetTime();
    hitInfo.message = 133;
    hitInfo.attackedMotionTime = attackedMT;
    hitInfo.damageDestX = targetActor->m_pos.x;
    hitInfo.damageDestZ = targetActor->m_pos.z;
    PopulateCombatHitWaveName(sourceActor, targetActor, hitInfo.waveName, sizeof(hitInfo.waveName));

    targetActor->QueueWillBeAttacked(hitInfo);
}

void QueueTimedAttackHit(CGameActor* sourceActor,
    CGameActor* targetActor,
    int attackedMT,
    int damage,
    u8 actionType,
    u32 queueTime)
{
    if (!targetActor || damage == 0 || actionType == kNotifyActLuckyDodge) {
        return;
    }

    WBA hitInfo{};
    hitInfo.gid = sourceActor ? sourceActor->m_gid : 0;
    hitInfo.time = queueTime;
    hitInfo.message = 133;
    hitInfo.attackedMotionTime = attackedMT;
    hitInfo.damage = damage < 0 ? -damage : damage;
    const bool isLocalPlayerTarget = targetActor->m_gid == g_session.m_gid || targetActor->m_gid == g_session.m_aid;
    const bool isLocalPlayerSource = sourceActor && (sourceActor->m_gid == g_session.m_gid || sourceActor->m_gid == g_session.m_aid);
    hitInfo.damageColor = (isLocalPlayerTarget && !isLocalPlayerSource)
        ? 0xFFFF4040u
        : ResolveCombatNumberColor(damage, actionType);
    hitInfo.damageKind = ResolveCombatNumberKind(damage, actionType);
    hitInfo.damageDestX = targetActor->m_pos.x;
    hitInfo.damageDestZ = targetActor->m_pos.z;
    PopulateCombatHitWaveName(sourceActor, targetActor, hitInfo.waveName, sizeof(hitInfo.waveName));
    targetActor->QueueWillBeAttacked(hitInfo);
}

void QueueAttackHitReactions(CGameActor* sourceActor,
    CGameActor* targetActor,
    int attackedMT,
    const std::vector<ScheduledAttackHit>& scheduledHits,
    u8 actionType)
{
    const u32 now = timeGetTime();
    for (const ScheduledAttackHit& scheduledHit : scheduledHits) {
        QueueTimedAttackHit(sourceActor,
            targetActor,
            attackedMT,
            scheduledHit.damage,
            actionType,
            now + scheduledHit.delayMs);
    }
}

void FaceActorTowardTarget(CGameActor* actor, CGameActor* target)
{
    if (!actor || !target) {
        return;
    }

    const float dx = target->m_pos.x - actor->m_pos.x;
    const float dz = target->m_pos.z - actor->m_pos.z;
    if (dx == 0.0f && dz == 0.0f) {
        return;
    }

    actor->m_roty = std::atan2(dx, -dz) * (180.0f / 3.14159265f);
    if (actor->m_roty < 0.0f) {
        actor->m_roty += 360.0f;
    }
    if (actor->m_roty >= 360.0f) {
        actor->m_roty -= 360.0f;
    }
}

void FaceActorTowardPosition(CGameActor* actor, const vector3d& targetPos)
{
    if (!actor) {
        return;
    }

    const float dx = targetPos.x - actor->m_pos.x;
    const float dz = targetPos.z - actor->m_pos.z;
    if (dx == 0.0f && dz == 0.0f) {
        return;
    }

    actor->m_roty = std::atan2(dx, -dz) * (180.0f / 3.14159265f);
    if (actor->m_roty < 0.0f) {
        actor->m_roty += 360.0f;
    }
    if (actor->m_roty >= 360.0f) {
        actor->m_roty -= 360.0f;
    }
}

void StopActorMovementForAction(CGameActor* actor)
{
    if (!actor) {
        return;
    }

    actor->m_isMoving = 0;
    actor->m_path.Reset();
    actor->m_moveStartPos = actor->m_pos;
    actor->m_moveEndPos = actor->m_pos;
    actor->m_moveStartTime = 0;
    actor->m_moveEndTime = 0;
}

CGameActor* EnsureRuntimeActor(CGameMode& mode, u32 gid, bool preferPc);

CGameActor* ResolveCombatActor(CGameMode& mode, u32 actorId, bool preferPc)
{
    if (mode.m_world && mode.m_world->m_player) {
        CPlayer* player = mode.m_world->m_player;
        if (actorId == g_session.m_aid || actorId == g_session.m_gid || actorId == player->m_gid) {
            return player;
        }
    }
    return EnsureRuntimeActor(mode, actorId, preferPc);
}

void StartAttackAnimation(CGameActor* actor, CGameActor* target, int attackMT)
{
    if (!actor) {
        return;
    }

    actor->m_isSitting = 0;
    actor->m_targetGid = target ? target->m_gid : 0;
    StopActorMovementForAction(actor);
    FaceActorTowardTarget(actor, target);
    actor->SetModifyFactorOfmotionSpeed(attackMT);
    DbgLog("[ActorTrace2] attack before actor=%p gid=%u state=%d base=%d cur=%d motion=%d isPc=%d isCPc=%d target=%u attackMT=%d roty=%.1f\n",
        static_cast<void*>(actor),
        actor->m_gid,
        actor->m_stateId,
        actor->m_baseAction,
        actor->m_curAction,
        actor->m_curMotion,
        actor->m_isPc,
        dynamic_cast<CPc*>(actor) != nullptr,
        actor->m_targetGid,
        attackMT,
        actor->m_roty);
    actor->SetState(kGameActorAttackStateId);
    DbgLog("[ActorTrace2] attack after SetState actor=%p gid=%u state=%d base=%d cur=%d motion=%d atkMotion=%.1f\n",
        static_cast<void*>(actor),
        actor->m_gid,
        actor->m_stateId,
        actor->m_baseAction,
        actor->m_curAction,
        actor->m_curMotion,
        actor->m_attackMotion);
    actor->ProcessMotion();
    DbgLog("[ActorTrace2] attack after ProcessMotion actor=%p gid=%u state=%d base=%d cur=%d motion=%d finished=%d atkMotion=%.1f\n",
        static_cast<void*>(actor),
        actor->m_gid,
        actor->m_stateId,
        actor->m_baseAction,
        actor->m_curAction,
        actor->m_curMotion,
        actor->m_isMotionFinished,
        actor->m_attackMotion);

    if (CPc* pcActor = dynamic_cast<CPc*>(actor)) {
        pcActor->InvalidateBillboard();
    }
}

void StartPickupAnimation(CGameMode& mode, CGameActor* actor, u32 objectAid)
{
    if (!mode.m_world || !actor || objectAid == 0 || !actor->m_isPc) {
        return;
    }

    actor->m_isSitting = 0;
    actor->m_targetGid = 0;
    StopActorMovementForAction(actor);

    for (CItem* item : mode.m_world->m_itemList) {
        if (!item || item->m_aid != objectAid) {
            continue;
        }
        FaceActorTowardPosition(actor, item->m_pos);
        break;
    }

    DbgLog("[ActorTrace2] pickup before actor=%p gid=%u state=%d base=%d cur=%d motion=%d isPc=%d isCPc=%d item=%u roty=%.1f\n",
        static_cast<void*>(actor),
        actor->m_gid,
        actor->m_stateId,
        actor->m_baseAction,
        actor->m_curAction,
        actor->m_curMotion,
        actor->m_isPc,
        dynamic_cast<CPc*>(actor) != nullptr,
        objectAid,
        actor->m_roty);
    actor->SetState(kGameActorPickupStateId);
    DbgLog("[ActorTrace2] pickup after SetState actor=%p gid=%u state=%d base=%d cur=%d motion=%d atkMotion=%.1f\n",
        static_cast<void*>(actor),
        actor->m_gid,
        actor->m_stateId,
        actor->m_baseAction,
        actor->m_curAction,
        actor->m_curMotion,
        actor->m_attackMotion);
    actor->ProcessMotion();
    DbgLog("[ActorTrace2] pickup after ProcessMotion actor=%p gid=%u state=%d base=%d cur=%d motion=%d finished=%d atkMotion=%.1f\n",
        static_cast<void*>(actor),
        actor->m_gid,
        actor->m_stateId,
        actor->m_baseAction,
        actor->m_curAction,
        actor->m_curMotion,
        actor->m_isMotionFinished,
        actor->m_attackMotion);

    if (CPc* pcActor = dynamic_cast<CPc*>(actor)) {
        pcActor->InvalidateBillboard();
    }
}

void StartLocalPickupAnimation(CGameMode& mode, u32 objectAid)
{
    StartPickupAnimation(mode,
        mode.m_world ? static_cast<CGameActor*>(mode.m_world->m_player) : nullptr,
        objectAid);
}

void StartSitStandAnimation(CGameActor* actor, bool sitting)
{
    if (!actor) {
        return;
    }

    StopActorMovementForAction(actor);
    actor->m_targetGid = 0;
    actor->m_attackMotion = -1.0f;
    actor->m_isSitting = sitting ? 1 : 0;
    actor->m_stateId = 0;
    actor->m_stateStartTick = 0;
    actor->m_isMotionFinished = 0;
    actor->m_isMotionFreezed = 0;
    actor->m_motionType = 0;

    // Posture updates should snap directly to the steady sit/idle pose instead
    // of entering a transient animation state like attack or pickup.
    const int baseAction = (actor->m_isPc && sitting) ? 16 : 0;
    const int resolvedAction = baseAction + actor->Get8Dir(actor->m_roty);
    actor->m_baseAction = baseAction;
    actor->m_curAction = resolvedAction;
    actor->m_curMotion = 0;
    actor->m_oldBaseAction = baseAction;
    actor->m_oldMotion = 0;

    if (CPc* pcActor = dynamic_cast<CPc*>(actor)) {
        const int headMotion = (std::max)(0, (std::min)(pcActor->m_headDir, 2));
        pcActor->m_curMotion = headMotion;
        pcActor->m_oldMotion = headMotion;
        pcActor->InvalidateBillboard();
    }
    actor->m_stateId = 0;
}

const char* ResolveSkillFailMessage(u16 skillId, u32 btype, u8 cause)
{
    if (skillId == 1 && cause == 0) {
        switch (btype) {
        case 0:
            return "Basic skill failed.";
        case 1:
            return "Cannot use emotions.";
        case 2:
            return "Cannot sit.";
        case 3:
            return "Cannot chat.";
        case 4:
            return "Cannot form a party.";
        case 5:
            return "Cannot shout.";
        case 6:
            return "Cannot PK.";
        case 7:
            return "Cannot align.";
        default:
            break;
        }
    }

    if (cause == 0) {
        return "Action failed.";
    }

    switch (cause) {
    case 1:
        return "Not enough SP.";
    case 2:
        return "Not enough HP.";
    case 4:
        return "Action is still on cooldown.";
    case 5:
        return "Not enough Zeny.";
    case 9:
        return "Too much weight.";
    default:
        return "Action failed.";
    }
}

void HandleSkillFailAck(CGameMode& mode, const PacketView& packet)
{
    (void)mode;
    if (!packet.data || packet.packetLength < 10) {
        return;
    }

    const u16 skillId = ReadLE16(packet.data + 2);
    const u32 btype = ReadLE32(packet.data + 4);
    const u8 result = packet.data[8];
    const u8 cause = packet.data[9];
    const char* message = ResolveSkillFailMessage(skillId, btype, cause);

    DbgLog("[GameMode] skill fail ack opcode=0x%04X skill=%u num=%u result=%u cause=%u msg='%s'\n",
        packet.packetId,
        static_cast<unsigned int>(skillId),
        static_cast<unsigned int>(btype),
        static_cast<unsigned int>(result),
        static_cast<unsigned int>(cause),
        message);

    g_windowMgr.SendMsg(kUiChatEventMsg,
        reinterpret_cast<msgparam_t>(message),
        static_cast<int>(kSystemNoticeColor));
}

void HandleActorActionNotify(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data) {
        return;
    }

    const bool isNotifyAct2 = packet.packetId == 0x02E1;
    const size_t minLength = isNotifyAct2 ? 33u : 29u;
    if (packet.packetLength < minLength) {
        return;
    }

    const u32 srcGid = ReadLE32(packet.data + 2);
    const u32 dstGid = ReadLE32(packet.data + 6);
    const u32 serverTick = ReadLE32(packet.data + 10);
    int attackMT = static_cast<int>(ReadLE32(packet.data + 14));
    const int attackedMT = static_cast<int>(ReadLE32(packet.data + 18));
    const int damage = isNotifyAct2
        ? static_cast<int>(ReadLE32(packet.data + 22))
        : static_cast<int>(static_cast<int16_t>(ReadLE16(packet.data + 22)));
    const u16 div = isNotifyAct2
        ? ReadLE16(packet.data + 26)
        : ReadLE16(packet.data + 24);
    const u8 actionType = packet.data[isNotifyAct2 ? 28 : 26];
    const int leftDamage = isNotifyAct2
        ? static_cast<int>(ReadLE32(packet.data + 29))
        : static_cast<int>(static_cast<int16_t>(ReadLE16(packet.data + 27)));

    if (attackMT == 0) {
        attackMT = kDefaultAttackMotionTime;
    }
    (void)attackedMT;

    mode.m_aidList[srcGid] = GetTickCount();
    if (dstGid != 0) {
        mode.m_aidList[dstGid] = GetTickCount();
    }

    if (!IsAttackNotifyType(actionType) && actionType != 1 && actionType != 2 && actionType != 3) {
        static std::map<u32, bool> s_loggedUnsupportedActNotifyTypes;
        const u32 unsupportedKey = (static_cast<u32>(packet.packetId) << 8) | static_cast<u32>(actionType);
        if (!s_loggedUnsupportedActNotifyTypes[unsupportedKey]) {
            s_loggedUnsupportedActNotifyTypes[unsupportedKey] = true;
            DbgLog("[GameMode] unsupported act notify opcode=0x%04X type=%u src=%u dst=%u rawTick=%u\n",
                packet.packetId,
                static_cast<unsigned int>(actionType),
                srcGid,
                dstGid,
                static_cast<unsigned int>(serverTick));
        }
        return;
    }

    if (serverTick != 0) {
        g_session.SetServerTime(serverTick);
    }

    CGameActor* sourceActor = ResolveCombatActor(mode, srcGid, true);
    CGameActor* targetActor = (dstGid != 0 && IsAttackNotifyType(actionType))
        ? ResolveCombatActor(mode, dstGid, true)
        : nullptr;
    if (!sourceActor) {
        return;
    }

    DbgLog("[GameMode] act notify stage=resolved src=%u dst=%u srcPtr=%p dstPtr=%p srcPc=%d dstPc=%d srcLocal=%d srcCPc=%d dstCPc=%d\n",
        srcGid,
        dstGid,
        static_cast<void*>(sourceActor),
        static_cast<void*>(targetActor),
        sourceActor->m_isPc,
        targetActor ? targetActor->m_isPc : 0,
        mode.m_world && sourceActor == static_cast<CGameActor*>(mode.m_world->m_player),
        dynamic_cast<CPc*>(sourceActor) != nullptr,
        dynamic_cast<CPc*>(targetActor) != nullptr);

    if (actionType == 1) {
        StartPickupAnimation(mode, sourceActor, dstGid);
        DbgLog("[GameMode] act notify pickup opcode=0x%04X src=%u item=%u action=%d\n",
            packet.packetId,
            srcGid,
            dstGid,
            sourceActor->m_curAction);
        return;
    }

    if (actionType == 2 || actionType == 3) {
        StartSitStandAnimation(sourceActor, actionType == 2);
        DbgLog("[GameMode] act notify posture opcode=0x%04X src=%u type=%u sitting=%d\n",
            packet.packetId,
            srcGid,
            static_cast<unsigned int>(actionType),
            actionType == 2 ? 1 : 0);
        return;
    }

    StartAttackAnimation(sourceActor, targetActor, attackMT);
    DbgLog("[GameMode] act notify stage=started src=%u action=%d motionSpeed=%.3f atkMotion=%.1f\n",
        srcGid,
        sourceActor->m_curAction,
        sourceActor->m_motionSpeed,
        sourceActor->m_attackMotion);
    const std::vector<ScheduledAttackHit> scheduledHits = ResolveAttackHitSchedule(*sourceActor, damage, leftDamage, div, actionType);
    QueueAttackHitReactions(sourceActor, targetActor, attackedMT, scheduledHits, actionType);
    DbgLog("[GameMode] act notify stage=number src=%u dst=%u dmg=%d left=%d hits=%u\n",
        srcGid,
        dstGid,
        damage,
        leftDamage,
        static_cast<unsigned int>(scheduledHits.size()));

    DbgLog("[GameMode] act notify opcode=0x%04X src=%u dst=%u type=%u attackMT=%d attackedMT=%d dmg=%d left=%d div=%u action=%d motionSpeed=%.3f atkMotion=%.1f\n",
        packet.packetId,
        srcGid,
        dstGid,
        static_cast<unsigned int>(actionType),
        attackMT,
        attackedMT,
        damage,
        leftDamage,
        static_cast<unsigned int>(div),
        sourceActor->m_curAction,
        sourceActor->m_motionSpeed,
        sourceActor->m_attackMotion);
}

void ApplyMapChangeSessionState(const MapChangeInfo& info)
{
    constexpr size_t kMaxMapName = sizeof(g_session.m_curMap) - 1;
    const size_t copyLen = (std::min)(kMaxMapName, info.mapName.size());
    std::memset(g_session.m_curMap, 0, sizeof(g_session.m_curMap));
    if (copyLen > 0) {
        std::memcpy(g_session.m_curMap, info.mapName.data(), copyLen);
        g_session.m_curMap[copyLen] = '\0';
    }

    if (info.hasPosition) {
        g_session.SetPlayerPosDir(info.x, info.y, g_session.m_playerDir);
    }
}

bool ReconnectForServerMove(const MapChangeInfo& info)
{
    if (!info.hasServerMove) {
        return true;
    }

    char serverHost[64] = {};
    std::snprintf(serverHost,
        sizeof(serverHost),
        "%u.%u.%u.%u",
        static_cast<unsigned int>(info.ip & 0xFFu),
        static_cast<unsigned int>((info.ip >> 8) & 0xFFu),
        static_cast<unsigned int>((info.ip >> 16) & 0xFFu),
        static_cast<unsigned int>((info.ip >> 24) & 0xFFu));

    CRagConnection::instance()->Disconnect();
    if (!CRagConnection::instance()->Connect(serverHost, static_cast<int>(info.port))) {
        DbgLog("[GameMode] map change reconnect failed zone=%s:%u map='%s'\n",
            serverHost,
            static_cast<unsigned int>(info.port),
            info.mapName.c_str());
        return false;
    }

    PACKET_CZ_ENTER2 enterPacket{};
    enterPacket.PacketType = PacketProfile::ActiveMapServerSend::kWantToConnection;
    enterPacket.AID = g_session.m_aid;
    enterPacket.GID = g_session.m_gid;
    enterPacket.AuthCode = g_session.m_authCode;
    enterPacket.ClientTick = GetTickCount();
    enterPacket.Sex = g_session.m_sex;

    const bool sent = CRagConnection::instance()->SendPacket(
        reinterpret_cast<const char*>(&enterPacket),
        static_cast<int>(sizeof(enterPacket)));
    DbgLog("[GameMode] map change reconnect zone=%s:%u map='%s' opcode=0x%04X sentEnter=%d\n",
        serverHost,
        static_cast<unsigned int>(info.port),
        info.mapName.c_str(),
        enterPacket.PacketType,
        sent ? 1 : 0);
    return sent;
}

BroadcastPayload ParseBroadcastPayload(const PacketView& packet)
{
    BroadcastPayload payload{};
    payload.color = 0x0000FFFF;

    if (!packet.data || packet.packetLength < 5) {
        return payload;
    }

    if (packet.packetId == 0x01C3) {
        if (packet.packetLength >= 16) {
            payload.color = static_cast<u32>(packet.data[4]) |
                            (static_cast<u32>(packet.data[5]) << 8) |
                            (static_cast<u32>(packet.data[6]) << 16);
            payload.text = ExtractPacketString(packet, 16);
        }
        return payload;
    }

    const std::string raw = ExtractPacketString(packet, 4);
    if (raw.empty()) {
        return payload;
    }

    if (raw.size() >= 34 && raw.compare(0, 4, "micc") == 0) {
        std::string talker = raw.substr(4, 24);
        const size_t talkerNul = talker.find('\0');
        if (talkerNul != std::string::npos) {
            talker.resize(talkerNul);
        }
        ParseHexColor(raw, 28, payload.color);

        if (raw.size() > 34) {
            const std::string body = raw.substr(34);
            if (!talker.empty()) {
                payload.text = talker + " : " + body;
            } else {
                payload.text = body;
            }
        } else {
            payload.text = talker;
        }
        return payload;
    }

    if (raw.size() > 10 && raw.compare(0, 4, "tool") == 0) {
        ParseHexColor(raw, 4, payload.color);
        payload.text = raw.substr(10);
        return payload;
    }
    if (raw.size() > 4 && (raw.compare(0, 4, "blue") == 0 || raw.compare(0, 4, "ssss") == 0)) {
        payload.color = 0x00FFFF00;
        payload.text = raw.substr(4);
        return payload;
    }

    const size_t hashPos = raw.find('#');
    if (hashPos != std::string::npos && hashPos > 0) {
        payload.text = raw.substr(0, hashPos);
        const size_t colonPos = raw.find(':', hashPos);
        if (colonPos != std::string::npos && colonPos < raw.size()) {
            payload.text += raw.substr(colonPos);
        }
        return payload;
    }

    payload.text = raw;
    return payload;
}

float TileToWorldCoordX(const CWorld* world, int tileX)
{
    const int width = world && world->m_attr ? world->m_attr->m_width : (world && world->m_ground ? world->m_ground->m_width : 0);
    const float zoom = world && world->m_attr ? static_cast<float>(world->m_attr->m_zoom) : (world && world->m_ground ? world->m_ground->m_zoom : 5.0f);
    return (static_cast<float>(tileX) - static_cast<float>(width) * 0.5f) * zoom + zoom * 0.5f;
}

float TileToWorldCoordZ(const CWorld* world, int tileY)
{
    const int height = world && world->m_attr ? world->m_attr->m_height : (world && world->m_ground ? world->m_ground->m_height : 0);
    const float zoom = world && world->m_attr ? static_cast<float>(world->m_attr->m_zoom) : (world && world->m_ground ? world->m_ground->m_zoom : 5.0f);
    return (static_cast<float>(tileY) - static_cast<float>(height) * 0.5f) * zoom + zoom * 0.5f;
}

struct UseSkillPacketInfo {
    int beginEffectId = 16;
    int motionType = 7;
    int targetEffectId = -1;
    std::vector<int> impactEffectIds;
    bool useTargetSlot = false;
    bool launchUseSkill = true;
};

UseSkillPacketInfo ResolveUseSkillPacketInfo(u16 skillId)
{
    UseSkillPacketInfo info;
    GetSkillActionInfo(static_cast<int>(skillId), info.beginEffectId, info.motionType);

    LuaSkillEffectInfo luaInfo;
    if (g_buabridge.GetSkillEffectInfoBySkillId(static_cast<int>(skillId), &luaInfo)) {
        if (luaInfo.hasTargetEffectId) {
            info.targetEffectId = luaInfo.targetEffectId;
        }
        info.impactEffectIds = luaInfo.effectIds;
        if (luaInfo.hasOnTarget) {
            info.useTargetSlot = luaInfo.onTarget;
        }
        if (luaInfo.hasLaunchUseSkill) {
            info.launchUseSkill = luaInfo.launchUseSkill;
        }
    }

    return info;
}

int ResolveGroundSkillEffectId(u16 skillId, u16 level);

int ResolveSkillVisibleEffectId(u16 skillId, const UseSkillPacketInfo& packetInfo, u16 levelOrAmount)
{
    if (packetInfo.targetEffectId >= 0) {
        return packetInfo.targetEffectId;
    }

    switch (skillId) {
    case 24: // AL_RUWACH
        return 24;
    case 33: // AL_ANGELUS
        return 41;
    case 35: // AL_CURE
        return 66;
    default: {
        return packetInfo.beginEffectId >= 0 ? packetInfo.beginEffectId : ResolveGroundSkillEffectId(skillId, levelOrAmount);
    }
    }
}

void InvalidateActorBillboard(CGameActor* actor)
{
    if (CPc* pcActor = dynamic_cast<CPc*>(actor)) {
        pcActor->InvalidateBillboard();
    }
}

void ClearAttachedSkillEffects(CGameActor* actor)
{
    if (!actor) {
        return;
    }

    if (actor->m_beginSpellEffect) {
        actor->m_beginSpellEffect->DetachFromMaster();
        actor->m_beginSpellEffect = nullptr;
    }
    if (actor->m_magicTargetEffect) {
        actor->m_magicTargetEffect->DetachFromMaster();
        actor->m_magicTargetEffect = nullptr;
    }
}

void StartSkillSourceAnimation(CGameActor* actor, const vector3d* targetPos, int attackMotionMs)
{
    if (!actor) {
        return;
    }

    actor->m_isSitting = 0;
    actor->m_targetGid = 0;
    StopActorMovementForAction(actor);
    if (targetPos) {
        FaceActorTowardPosition(actor, *targetPos);
    }
    actor->SetModifyFactorOfmotionSpeed(attackMotionMs > 0 ? attackMotionMs : kDefaultAttackMotionTime);
    actor->SetState(kGameActorAttackStateId);
    actor->ProcessMotion();
    InvalidateActorBillboard(actor);
}

void StartSkillStateAnimation(CGameActor* actor, int stateId, u32 targetGid, const vector3d* targetPos)
{
    if (!actor) {
        return;
    }

    actor->m_isSitting = 0;
    actor->m_targetGid = targetGid;
    StopActorMovementForAction(actor);
    if (targetPos) {
        FaceActorTowardPosition(actor, *targetPos);
    }
    actor->SetState(stateId);
    InvalidateActorBillboard(actor);
}

vector3d ResolveSkillCellWorldPosition(const CWorld* world, int cellX, int cellY)
{
    const float worldX = TileToWorldCoordX(world, cellX);
    const float worldZ = TileToWorldCoordZ(world, cellY);
    return vector3d{
        worldX,
        ResolveActorHeight(world, worldX, worldZ),
        worldZ
    };
}

int ResolveGroundSkillEffectId(u16 skillId, u16 level)
{
    LuaSkillEffectInfo luaInfo;
    if (g_buabridge.GetSkillEffectInfoBySkillId(static_cast<int>(skillId), &luaInfo) && luaInfo.hasGroundEffectId) {
        return luaInfo.groundEffectId;
    }

    switch (skillId) {
    case 21: return 30;
    case 25: return 141;
    case 69: return 91;
    case 70: return 83;
    case 79: return 113;
    case 83: return 92;
    case 85: return 90;
    case 87: return 74;
    case 89: return 89;
    case 91: return 142;
    case 92: return 95;
    case 110: return 102;
    case 111: return 98;
    case 286: return 236;
    case 287: return 237;
    case 288: return 238;
    case 478:
        if (level >= 10) {
            return 499;
        }
        if (level >= 6) {
            return 498;
        }
        return 497;
    case 694: return 91;
    default:
        return static_cast<int>(skillId);
    }
}

static std::string LowercaseAsciiSkillIdName(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        if (ch >= 'A' && ch <= 'Z') {
            return static_cast<char>(ch - 'A' + 'a');
        }
        return static_cast<char>(ch);
    });
    return value;
}

// GRF layout: data\wav\effect\wizard_<stem>.wav (Ref RagEffect Lord/MeteorStorm/StormGust use aEffectWizard* / HunterBl).
static std::string CompactAlnumLower(std::string value)
{
    std::string out;
    out.reserve(value.size());
    for (unsigned char ch : value) {
        if (std::isalnum(ch)) {
            out.push_back(static_cast<char>(std::tolower(ch)));
        }
    }
    return out;
}

static std::string WizardWaveStemFromSkillIdName(std::string idName)
{
    idName = LowercaseAsciiSkillIdName(std::move(idName));
    static constexpr const char* kStripPrefixes[] = {
        "mg_", "wz_", "pr_", "am_", "mo_", "npc_", "tk_", "sl_", "gs_", "cr_", "cg_", "nj_", "lk_", "rms_",
        "st_", "sa_", "pf_", "we_", "bs_", "mc_", "all_", "ba_", "nc_", "sr_", "al_", "ht_", "kn_", "tf_",
    };
    for (const char* pre : kStripPrefixes) {
        const size_t len = std::strlen(pre);
        if (idName.size() > len && idName.compare(0, len, pre) == 0) {
            idName = idName.substr(len);
            break;
        }
    }
    return CompactAlnumLower(idName);
}

static bool TryPlayEffectWaveAtPosition(const vector3d& soundPos, const std::string& relativePath)
{
    if (relativePath.empty()) {
        return false;
    }

    CAudio* audio = CAudio::GetInstance();
    CGameMode* gameMode = g_modeMgr.GetCurrentGameMode();
    if (!audio || !gameMode || !gameMode->m_world || !gameMode->m_world->m_player) {
        return false;
    }

    const vector3d listenerPos = gameMode->m_world->m_player->m_pos;
    const std::array<std::string, 3> candidates = {
        relativePath,
        std::string("wav\\") + relativePath,
        std::string("data\\wav\\") + relativePath,
    };

    for (const std::string& candidate : candidates) {
        if (audio->PlaySound3D(candidate.c_str(), soundPos, listenerPos, 250, 40, 1.0f)) {
            return true;
        }
    }
    return false;
}

static bool TryPlayFirstMatchingEffectWave(const vector3d& pos, std::initializer_list<const char*> paths)
{
    for (const char* path : paths) {
        if (path && TryPlayEffectWaveAtPosition(pos, path)) {
            return true;
        }
    }
    return false;
}

// EzStr world effects do not emit audio from keyframes; timed casts no longer attach begin-spell STR (which
// often carried cast SFX). Play impact SFX when server-driven packets spawn the ground/cell effect.
static void PlayGroundSkillImpactSound(u16 skillId, const vector3d& worldPos)
{
    if (!g_soundMode || !g_isSoundOn) {
        return;
    }

    switch (skillId) {
    case 21: // MG_THUNDERSTORM — client ground effect id 30 (thunderstorm.str)
        if (TryPlayFirstMatchingEffectWave(worldPos, {
                "effect\\wizard_thunderstorm.wav",
                "effect\\ef_thunderstorm.wav",
                "effect\\ef_thunder.wav",
            })) {
            return;
        }
        break;
    case 83: // WZ_METEOR — client ground effect id 92 (Meteor*.str); Ref PlayWave aEffectWizardMe
        if (TryPlayFirstMatchingEffectWave(worldPos, {
                "effect\\wizard_meteor.wav",
                "effect\\wizard_meteorstorm.wav",
                "effect\\ef_meteorstorm.wav",
                "effect\\ef_meteor.wav",
            })) {
            return;
        }
        break;
    case 85: // WZ_VERMILION — client ground effect id 90 (lord.str); Ref Lord() uses aEffectWizardMe_0 + aEffectHunterBl
        if (TryPlayFirstMatchingEffectWave(worldPos, {
                "effect\\wizard_vermilion.wav",
                "effect\\wizard_lord.wav",
                "effect\\wizard_meteor.wav",
                "effect\\hunter_blitz.wav",
                "effect\\ef_vermillion.wav",
                "effect\\ef_lord.wav",
                "effect\\ef_lov.wav",
            })) {
            return;
        }
        break;
    case 89: // WZ_STORMGUST — client ground effect id 89 (storm gust.str); Ref PlayWave aEffectWizardSt
        if (TryPlayFirstMatchingEffectWave(worldPos, {
                "effect\\wizard_stormgust.wav",
                "effect\\ef_stormgust.wav",
                "effect\\ef_storm.wav",
                "effect\\ef_wz_stormgust.wav",
            })) {
            return;
        }
        break;
    default:
        break;
    }

    g_skillMgr.EnsureLoaded();
    const SkillMetadata* md = g_skillMgr.GetSkillMetadata(static_cast<int>(skillId));
    if (!md || md->skillIdName.empty()) {
        return;
    }

    const std::string n = LowercaseAsciiSkillIdName(md->skillIdName);
    const std::string wizardStem = WizardWaveStemFromSkillIdName(md->skillIdName);
    if (!wizardStem.empty() && TryPlayEffectWaveAtPosition(worldPos, std::string("effect\\wizard_") + wizardStem + ".wav")) {
        return;
    }

    const std::string compactUnderscore = CompactAlnumLower(n);
    if (!compactUnderscore.empty()
        && TryPlayEffectWaveAtPosition(worldPos, std::string("effect\\wizard_") + compactUnderscore + ".wav")) {
        return;
    }

    if (TryPlayEffectWaveAtPosition(worldPos, std::string("effect\\ef_") + n + ".wav")) {
        return;
    }

    if (!compactUnderscore.empty()
        && TryPlayEffectWaveAtPosition(worldPos, std::string("effect\\ef_") + compactUnderscore + ".wav")) {
        return;
    }

    static constexpr const char* kStripPrefixes[] = {
        "mg_",
        "wz_",
        "pr_",
        "am_",
        "mo_",
        "npc_",
        "tk_",
        "sl_",
        "gs_",
        "cr_",
        "cg_",
        "nj_",
        "lk_",
        "rms_",
        "st_",
        "sa_",
        "pf_",
        "we_",
        "bs_",
        "mc_",
        "all_",
        "ba_",
        "nc_",
        "sr_",
    };

    for (const char* pre : kStripPrefixes) {
        const size_t len = std::strlen(pre);
        if (n.size() > len && n.compare(0, len, pre) == 0) {
            const std::string tail = n.substr(len);
            if (TryPlayEffectWaveAtPosition(worldPos, std::string("effect\\wizard_") + CompactAlnumLower(tail) + ".wav")) {
                return;
            }
            if (TryPlayEffectWaveAtPosition(worldPos, std::string("effect\\ef_") + tail + ".wav")) {
                return;
            }
        }
    }
}

CRagEffect* SpawnWorldSkillEffect(CGameMode& mode, int effectId, const vector3d& position)
{
    if (effectId < 0 || !mode.m_world) {
        return nullptr;
    }

    CRagEffect* effect = new CRagEffect();
    if (!effect) {
        return nullptr;
    }

    effect->InitAtWorldPosition(effectId, position);
    mode.m_world->m_gameObjectList.push_back(effect);
    return effect;
}

float ResolveActorHeight(const CWorld* world, float worldX, float worldZ)
{
    if (world && world->m_attr) {
        return world->m_attr->GetHeight(worldX, worldZ);
    }
    return 0.0f;
}

void HandleSkillCastAck(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 24) {
        return;
    }

    const u32 srcGid = ReadLE32(packet.data + 2);
    const u32 dstGid = ReadLE32(packet.data + 6);
    const int cellX = static_cast<int>(ReadLE16(packet.data + 10));
    const int cellY = static_cast<int>(ReadLE16(packet.data + 12));
    const u16 skillId = ReadLE16(packet.data + 14);
    const PLAYER_SKILL_INFO* castSkillInfo = g_session.GetSkillItemBySkillId(static_cast<int>(skillId));
    const u16 castLevel = static_cast<u16>(castSkillInfo && castSkillInfo->level > 0 ? castSkillInfo->level : 1);
    const u32 property = ReadLE32(packet.data + 16);
    const int castTimeMs = static_cast<int>(ReadLE32(packet.data + 20));

    CGameActor* sourceActor = ResolveCombatActor(mode, srcGid, true);
    CGameActor* targetActor = dstGid != 0 ? ResolveCombatActor(mode, dstGid, true) : nullptr;
    if (!sourceActor) {
        DbgLog("[GameMode] skill cast ack unresolved src=%u dst=%u skill=%u\n",
            srcGid,
            dstGid,
            static_cast<unsigned int>(skillId));
        return;
    }

    const vector3d targetPos = targetActor
        ? targetActor->m_pos
        : ResolveSkillCellWorldPosition(mode.m_world, cellX, cellY);

    int beginEffectId = 16;
    int motionType = kGameActorCastingStateId;
    GetSkillActionInfo2(static_cast<int>(skillId), beginEffectId, motionType, static_cast<int>(property), sourceActor->m_job);
    LuaSkillEffectInfo luaInfo;
    if (g_buabridge.GetSkillEffectInfoBySkillId(static_cast<int>(skillId), &luaInfo)) {
        if (luaInfo.hasBeginEffectId) {
            beginEffectId = luaInfo.beginEffectId;
        }
        if (luaInfo.hasBeginMotionType) {
            motionType = luaInfo.beginMotionType;
        }
    }

    int motion = motionType;
    if (motion == 0 && castTimeMs > 0) {
        motion = kGameActorCastingStateId;
    } else if ((motion == 5 || motion == 11 || motion == 12) && castTimeMs > 0) {
        motion = kGameActorCastingStateId;
    }

    const bool frozenCast = (skillId == 57 || skillId == 62);

    sourceActor->m_isSitting = 0;
    StopActorMovementForAction(sourceActor);
    FaceActorTowardPosition(sourceActor, targetPos);

    if (frozenCast) {
        ClearAttachedSkillEffects(sourceActor);
        sourceActor->SetModifyFactorOfmotionSpeed(castTimeMs > 0 ? castTimeMs : kDefaultAttackMotionTime);
        sourceActor->SetState(kGameActorAttackStateId);
        sourceActor->m_isMotionFreezed = 1;
        sourceActor->m_freezeEndTick = timeGetTime() + static_cast<u32>((std::max)(0, castTimeMs));
    } else {
        ClearAttachedSkillEffects(sourceActor);
        if (motion == kGameActorAttackStateId && castTimeMs > 0) {
            StartSkillSourceAnimation(sourceActor, &targetPos, castTimeMs);
        } else {
            sourceActor->m_targetGid = dstGid;
            int motionToApply = motion;
            // State 8 never calls SetAction; timed casts need the loop pose (13) so motion/granny state matches Ref.
            if (castTimeMs > 0 && motionToApply == kGameActorCastingStateId) {
                motionToApply = kGameActorCastingLoopStateId;
            }
            sourceActor->SetState(motionToApply);
        }
        if (beginEffectId >= 0) {
            sourceActor->SendMsg(sourceActor, 85, beginEffectId, castTimeMs > 0 ? (castTimeMs >> 4) : 0, 0);
        }
        if (castTimeMs > 0) {
            sourceActor->SendMsg(sourceActor, 82, castTimeMs, 0, 0);
        }
    }

    if (dstGid != 0 && targetActor && targetActor != sourceActor) {
        sourceActor->SendMsg(sourceActor, 81, dstGid, 0, 0);
        targetActor->SendMsg(targetActor, 86, 60, castTimeMs >> 4, 0);
    }

    // Ground timed casts: defer cell effect until server sends damage / ZC_SKILL_ENTRY / etc., not cast-start ack.
    if (dstGid == 0 && castTimeMs <= 0) {
        const int groundFx = ResolveGroundSkillEffectId(skillId, castLevel);
        if (groundFx > 0) {
            SpawnWorldSkillEffect(mode, groundFx, targetPos);
        }
    }

    if (skillId == 17 && targetActor && targetActor != sourceActor) { // MG_FIREBOLT
        if (CRagEffect* effect = sourceActor->LaunchEffect(24, vector3d{}, 0.0f)) {
            effect->SendMsg(effect, 44, castLevel > 0 ? castLevel : 1, 0, 0);
            effect->SendMsg(effect, 14, reinterpret_cast<msgparam_t>(&targetActor->m_pos), 0, 0);
        }
    }

    DbgLog("[GameMode] skill cast ack src=%u dst=%u skill=%u property=%u castMs=%d cell=(%d,%d)\n",
        srcGid,
        dstGid,
        static_cast<unsigned int>(skillId),
        static_cast<unsigned int>(property),
        castTimeMs,
        cellX,
        cellY);
}

void HandleSkillDamageNotify(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || (packet.packetId == 0x0114 && packet.packetLength < 31) || (packet.packetId == 0x01DE && packet.packetLength < 33)) {
        return;
    }

    const bool isLongDamage = packet.packetId == 0x01DE;
    const u16 skillId = ReadLE16(packet.data + 2);
    const u32 srcGid = ReadLE32(packet.data + 4);
    const u32 dstGid = ReadLE32(packet.data + 8);
    const int attackMT = static_cast<int>(ReadLE32(packet.data + 16));
    const int attackedMT = static_cast<int>(ReadLE32(packet.data + 20));
    const int damage = isLongDamage
        ? static_cast<int>(ReadLE32(packet.data + 24))
        : static_cast<int>(static_cast<int16_t>(ReadLE16(packet.data + 24)));
    const u16 level = ReadLE16(packet.data + (isLongDamage ? 28 : 26));
    const u16 div = ReadLE16(packet.data + (isLongDamage ? 30 : 28));
    const u16 displayHitCount = ResolveDisplayedSkillHitCount(skillId, level, div);
    const u8 actionType = packet.data[isLongDamage ? 32 : 30];

    CGameActor* sourceActor = ResolveCombatActor(mode, srcGid, true);
    CGameActor* targetActor = ResolveCombatActor(mode, dstGid, true);
    if (!sourceActor || !targetActor) {
        DbgLog("[GameMode] skill damage unresolved src=%u dst=%u skill=%u opcode=0x%04X\n",
            srcGid,
            dstGid,
            static_cast<unsigned int>(skillId),
            packet.packetId);
        return;
    }

    ClearAttachedSkillEffects(sourceActor);
    StartSkillSourceAnimation(sourceActor, &targetActor->m_pos, attackMT);
    QueueCombatHitReaction(sourceActor, targetActor, attackedMT, damage, actionType);
    EmitSkillImpactEffects(targetActor, skillId, displayHitCount);
    EmitCombatNumbers(sourceActor, targetActor, damage, actionType, displayHitCount);

    DbgLog("[GameMode] skill damage opcode=0x%04X src=%u dst=%u skill=%u level=%u damage=%d div=%u type=%u\n",
        packet.packetId,
        srcGid,
        dstGid,
        static_cast<unsigned int>(skillId),
        static_cast<unsigned int>(level),
        damage,
        static_cast<unsigned int>(div),
        static_cast<unsigned int>(actionType));
}

void HandleSkillDamagePositionNotify(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 35) {
        return;
    }

    const u16 skillId = ReadLE16(packet.data + 2);
    const u32 srcGid = ReadLE32(packet.data + 4);
    const u32 dstGid = ReadLE32(packet.data + 8);
    const int attackMT = static_cast<int>(ReadLE32(packet.data + 16));
    const int attackedMT = static_cast<int>(ReadLE32(packet.data + 20));
    const int cellX = static_cast<int>(ReadLE16(packet.data + 24));
    const int cellY = static_cast<int>(ReadLE16(packet.data + 26));
    const int damage = static_cast<int>(static_cast<int16_t>(ReadLE16(packet.data + 28)));
    const u16 level = ReadLE16(packet.data + 30);
    const u16 div = ReadLE16(packet.data + 32);
    const u16 displayHitCount = ResolveDisplayedSkillHitCount(skillId, level, div);
    const u8 actionType = packet.data[34];

    CGameActor* sourceActor = ResolveCombatActor(mode, srcGid, true);
    CGameActor* targetActor = ResolveCombatActor(mode, dstGid, true);
    const vector3d impactPos = ResolveSkillCellWorldPosition(mode.m_world, cellX, cellY);

    if (sourceActor) {
        ClearAttachedSkillEffects(sourceActor);
        StartSkillSourceAnimation(sourceActor, &impactPos, attackMT);
    }
    if (targetActor) {
        QueueCombatHitReaction(sourceActor, targetActor, attackedMT, damage, actionType);
        EmitSkillImpactEffects(targetActor, skillId, displayHitCount);
        EmitCombatNumbers(sourceActor, targetActor, damage, actionType, displayHitCount);
    }

    SpawnWorldSkillEffect(mode, ResolveGroundSkillEffectId(skillId, level), impactPos);
    PlayGroundSkillImpactSound(skillId, impactPos);
    DbgLog("[GameMode] skill damage pos src=%u dst=%u skill=%u level=%u damage=%d div=%u cell=(%d,%d)\n",
        srcGid,
        dstGid,
        static_cast<unsigned int>(skillId),
        static_cast<unsigned int>(level),
        damage,
        static_cast<unsigned int>(div),
        cellX,
        cellY);
}

void HandleGroundSkillNotify(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 18 || !mode.m_world) {
        return;
    }

    const u16 skillId = ReadLE16(packet.data + 2);
    const u32 srcGid = ReadLE32(packet.data + 4);
    const u16 level = ReadLE16(packet.data + 8);
    const int cellX = static_cast<int>(ReadLE16(packet.data + 10));
    const int cellY = static_cast<int>(ReadLE16(packet.data + 12));

    CGameActor* sourceActor = ResolveCombatActor(mode, srcGid, true);
    const vector3d worldPos = ResolveSkillCellWorldPosition(mode.m_world, cellX, cellY);
    const UseSkillPacketInfo packetInfo = ResolveUseSkillPacketInfo(skillId);
    if (sourceActor) {
        ClearAttachedSkillEffects(sourceActor);
        if (packetInfo.motionType == kGameActorAttackStateId) {
            StartSkillSourceAnimation(sourceActor, &worldPos, kDefaultAttackMotionTime);
        } else if (packetInfo.motionType >= 0) {
            StartSkillStateAnimation(sourceActor, packetInfo.motionType, 0, &worldPos);
        }
    }
    SpawnWorldSkillEffect(mode, ResolveGroundSkillEffectId(skillId, level), worldPos);
    PlayGroundSkillImpactSound(skillId, worldPos);

    DbgLog("[GameMode] ground skill src=%u skill=%u level=%u cell=(%d,%d)\n",
        srcGid,
        static_cast<unsigned int>(skillId),
        static_cast<unsigned int>(level),
        cellX,
        cellY);
}

void HandleSkillNoDamageNotify(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 15) {
        return;
    }

    const u16 skillId = ReadLE16(packet.data + 2);
    const u16 levelOrAmount = ReadLE16(packet.data + 4);
    const u32 dstGid = ReadLE32(packet.data + 6);
    const u32 srcGid = ReadLE32(packet.data + 10);
    const u8 result = packet.data[14];

    CGameActor* sourceActor = srcGid != 0 ? ResolveCombatActor(mode, srcGid, true) : nullptr;
    CGameActor* targetActor = ResolveCombatActor(mode, dstGid, true);
    const UseSkillPacketInfo packetInfo = ResolveUseSkillPacketInfo(skillId);
    if (sourceActor && targetActor && packetInfo.motionType >= 0) {
        ClearAttachedSkillEffects(sourceActor);
        StartSkillStateAnimation(sourceActor, packetInfo.motionType, dstGid, &targetActor->m_pos);
    }
    if (targetActor && result != 0 && packetInfo.launchUseSkill) {
        const int effectId = ResolveSkillVisibleEffectId(skillId, packetInfo, levelOrAmount);
        const bool selfCast = sourceActor && sourceActor == targetActor;
        const bool useTargetSlot = !selfCast
            && (packetInfo.useTargetSlot || packetInfo.targetEffectId >= 0);
        if (selfCast && effectId >= 0) {
            targetActor->SendMsg(targetActor, 85, effectId, 0, 0);
        } else {
            ClearAttachedSkillEffects(targetActor);
            targetActor->SendMsg(targetActor, 86,
                effectId >= 0 ? effectId : ResolveGroundSkillEffectId(skillId, levelOrAmount),
                0,
                0);
        }
    }

    DbgLog("[GameMode] skill nodamage src=%u dst=%u skill=%u value=%u result=%u\n",
        srcGid,
        dstGid,
        static_cast<unsigned int>(skillId),
        static_cast<unsigned int>(levelOrAmount),
        static_cast<unsigned int>(result));
}

void HandleSkillUnitSet(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 16 || !mode.m_world) {
        return;
    }

    const u32 unitAid = ReadLE32(packet.data + 2);
    const u32 srcGid = ReadLE32(packet.data + 6);
    const int cellX = static_cast<int>(ReadLE16(packet.data + 10));
    const int cellY = static_cast<int>(ReadLE16(packet.data + 12));
    const u8 unitId = packet.data[14];

    const vector3d worldPos = ResolveSkillCellWorldPosition(mode.m_world, cellX, cellY);
    SpawnWorldSkillEffect(mode, static_cast<int>(unitId), worldPos);

    DbgLog("[GameMode] skill unit opcode=0x%04X unitAid=%u src=%u unitId=%u cell=(%d,%d)\n",
        packet.packetId,
        unitAid,
        srcGid,
        static_cast<unsigned int>(unitId),
        cellX,
        cellY);
}

void DecodePosDir(const u8* src, int& posX, int& posY, int& dir)
{
    if (!src) {
        posX = posY = dir = 0;
        return;
    }

    posX = (static_cast<int>(src[0]) << 2) | (src[1] >> 6);
    posY = ((src[1] & 0x3F) << 4) | (src[2] >> 4);
    dir = src[2] & 0x0F;
}

bool IsKnownObjectType(u8 objectType)
{
    return objectType <= 9;
}

enum LookType {
    kLookBase = 0,
    kLookHair = 1,
    kLookWeapon = 2,
    kLookHeadBottom = 3,
    kLookHeadTop = 4,
    kLookHeadMid = 5,
    kLookHairColor = 6,
    kLookClothesColor = 7,
    kLookShield = 8,
    kLookRobe = 12,
};

bool IsPlayerLikeObjectType(u8 objectType)
{
    return objectType <= 1;
}

bool IsPotentialPcObjectType(u8 objectType)
{
    return IsPlayerLikeObjectType(objectType);
}

bool IsLikelyPlayerGid(u32 gid)
{
    return gid >= 2000000u && gid <= 100000000u;
}

bool IsNpcLikeJob(int job)
{
    return job >= 45 && job < 4000;
}

bool IsMonsterLikeJob(int job)
{
    return job >= 1000 && (job < 6001 || job > 6047);
}

bool IsHomunOrMercenaryJob(int job)
{
    return job >= 6001 && job <= 6047;
}

bool ShouldTreatActorAsPc(u8 objectType, int job)
{
    if (!IsPotentialPcObjectType(objectType)) {
        return false;
    }

    if (objectType == 9) {
        return false;
    }

    if (IsNpcLikeJob(job)) {
        return false;
    }

    if (IsMonsterLikeJob(job)) {
        return false;
    }

    return true;
}

bool ShouldUseSpriteBillboardActor(u8 objectType, int job)
{
    if (ShouldTreatActorAsPc(objectType, job)) {
        return true;
    }

    if (IsNpcLikeJob(job) || IsMonsterLikeJob(job) || IsHomunOrMercenaryJob(job)) {
        return true;
    }

    return objectType >= 5 && objectType <= 9;
}

float PacketDirToRotationDegrees(int dir)
{
    return static_cast<float>(45 * ((dir & 7) + 4));
}

void DecodePos2MoveData(const u8* src, int& srcX, int& srcY, int& dstX, int& dstY, int& cellX, int& cellY)
{
    if (!src) {
        srcX = srcY = dstX = dstY = cellX = cellY = 0;
        return;
    }

    srcX = (static_cast<int>(src[0]) << 2) | (src[1] >> 6);
    srcY = ((src[1] & 0x3F) << 4) | (src[2] >> 4);
    dstX = ((src[2] & 0x0F) << 6) | (src[3] >> 2);
    dstY = ((src[3] & 0x03) << 8) | src[4];
    cellX = (src[5] >> 4) & 0x0F;
    cellY = src[5] & 0x0F;
}

struct RuntimeActorState {
    u16 packetId = 0;
    u32 gid = 0;
    u32 speed = 0;
    int job = 0;
    int sex = 0;
    int hairStyle = 0;
    int weapon = 0;
    int shield = 0;
    int headBottom = 0;
    int headTop = 0;
    int headMid = 0;
    int hairColor = 0;
    int clothColor = 0;
    int headDir = 0;
    int manner = 0;
    int karma = 0;
    int bodyState = 0;
    int healthState = 0;
    int effectState = 0;
    int pkState = 0;
    int clevel = 0;
    int xSize = 0;
    int ySize = 0;
    int state = 0;
    int headType = 0;
    int guildId = 0;
    int emblemVersion = 0;
    int charfont = 0;
    int tileX = 0;
    int tileY = 0;
    int dir = 0;
    u8 objectType = 0;
    bool hasPosition = false;
};

CGameActor* EnsureRuntimeActor(CGameMode& mode, u32 gid, bool preferPc = false);

int ScoreLegacyActorState(const RuntimeActorState& state, bool hasObjectType)
{
    int score = 0;

    if (state.gid != 0) {
        score += 2;
    }
    if (state.job >= 0 && state.job < 6000) {
        score += 4;
    } else {
        score -= 8;
    }
    if (state.speed > 0 && state.speed < 1000) {
        score += 2;
    } else {
        score -= 2;
    }
    if (state.sex >= 0 && state.sex <= 1) {
        score += 2;
    } else {
        score -= 2;
    }
    if (state.tileX > 0 && state.tileX < 512) {
        score += 2;
    } else {
        score -= 2;
    }
    if (state.tileY > 0 && state.tileY < 512) {
        score += 2;
    } else {
        score -= 2;
    }
    if (state.dir >= 0 && state.dir < 8) {
        score += 1;
    } else {
        score -= 1;
    }
    if (state.clevel >= 0 && state.clevel < 200) {
        score += 1;
    } else {
        score -= 1;
    }

    if (hasObjectType) {
        score += IsKnownObjectType(state.objectType) ? 2 : -4;
    }

    return score;
}

void UpdateRuntimeActorPosition(CGameMode& mode, u32 gid, int tileX, int tileY, int srcX = -1, int srcY = -1);

    bool FindActivePathSegmentForPacketView(const CPathInfo& path, u32 now, size_t* outStartIndex)
    {
        if (!outStartIndex || path.m_cells.size() < 2) {
            return false;
        }

        for (size_t index = 0; index + 1 < path.m_cells.size(); ++index) {
            if (now < path.m_cells[index + 1].arrivalTime) {
                *outStartIndex = index;
                return true;
            }
        }

        return false;
    }

    bool InterpolateRuntimeActorPathPosition(const CWorld* world, const CGameActor& actor, u32 now, vector3d* outPos)
    {
        if (!world || !outPos || actor.m_path.m_cells.empty()) {
            return false;
        }

        if (actor.m_path.m_cells.size() == 1) {
            const PathCell& cell = actor.m_path.m_cells.front();
            outPos->x = TileToWorldCoordX(world, cell.x);
            outPos->z = TileToWorldCoordZ(world, cell.y);
            outPos->y = ResolveActorHeight(world, outPos->x, outPos->z);
            return true;
        }

        if (now >= actor.m_path.m_cells.back().arrivalTime) {
            const PathCell& cell = actor.m_path.m_cells.back();
            outPos->x = TileToWorldCoordX(world, cell.x);
            outPos->z = TileToWorldCoordZ(world, cell.y);
            outPos->y = ResolveActorHeight(world, outPos->x, outPos->z);
            return true;
        }

        size_t startIndex = 0;
        if (!FindActivePathSegmentForPacketView(actor.m_path, now, &startIndex)) {
            return false;
        }

        const PathCell& startCell = actor.m_path.m_cells[startIndex];
        const PathCell& endCell = actor.m_path.m_cells[startIndex + 1];
        const u32 startTime = startCell.arrivalTime;
        const u32 endTime = endCell.arrivalTime;
        const float duration = static_cast<float>((std::max)(1u, endTime - startTime));
        const float ratio = static_cast<float>(now - startTime) / duration;
        const float clamped = (std::max)(0.0f, (std::min)(1.0f, ratio));

        const float startX = startIndex == 0 ? actor.m_moveStartPos.x : TileToWorldCoordX(world, startCell.x);
        const float startZ = startIndex == 0 ? actor.m_moveStartPos.z : TileToWorldCoordZ(world, startCell.y);
        const float endX = TileToWorldCoordX(world, endCell.x);
        const float endZ = TileToWorldCoordZ(world, endCell.y);

        outPos->x = startX + (endX - startX) * clamped;
        outPos->z = startZ + (endZ - startZ) * clamped;
        outPos->y = ResolveActorHeight(world, outPos->x, outPos->z);
        return true;
    }

void InitializeRuntimeActorDefaults(CGameActor* actor, u32 gid)
{
    if (!actor) {
        return;
    }

    actor->m_gid = gid;
    actor->m_isVisible = 1;
    actor->m_isPc = 0;
    actor->m_roty = 0.0f;
    actor->m_zoom = 1.0f;
    actor->m_speed = 150;
    actor->m_job = 0;
    actor->m_sex = 0;
    actor->m_bodyState = 0;
    actor->m_healthState = 0;
    actor->m_effectState = 0;
    actor->m_pkState = 0;
    actor->m_isSitting = 0;
    actor->m_clevel = 0;
    actor->m_xSize = 0;
    actor->m_ySize = 0;
    actor->m_shadowOn = 1;
    actor->m_shadowZoom = 1.0f;
    actor->m_headType = 0;
    actor->m_gdid = 0;
    actor->m_emblemVersion = 0;
    actor->m_charfont = 0;
    actor->m_objectType = 0;
    actor->m_actorType = 0;
    actor->m_bodyPalette = 0;
    actor->m_birdEffect = nullptr;
    actor->m_moveSrcX = 0;
    actor->m_moveSrcY = 0;
    actor->m_moveDestX = 0;
    actor->m_moveDestY = 0;
    actor->m_moveStartTime = 0;
    actor->m_moveEndTime = 0;
    actor->m_isMoving = 0;
    actor->m_dist = 0.0f;
    actor->m_Hp = 0;
    actor->m_MaxHp = 0;
    actor->m_Sp = 0;
    actor->m_MaxSp = 0;
    actor->m_targetGid = 0;
    actor->m_willBeDead = 0;
    actor->m_vanishTime = 0;
    actor->m_stateId = 0;
    actor->m_oldstateId = 0;
    actor->m_isLieOnGround = 0;
    actor->m_isMotionFinished = 0;
    actor->m_isMotionFreezed = 0;
    actor->m_stateStartTick = timeGetTime();
    actor->m_motionType = 0;
    actor->m_motionSpeed = 1.0f;
    actor->m_modifyFactorOfmotionSpeed = 1.0f;
    actor->m_modifyFactorOfmotionSpeed2 = 1.0f;
    actor->m_attackMotion = -1.0f;
    actor->m_curAction = 0;
    actor->m_baseAction = 0;
    actor->m_curMotion = 0;
    actor->m_oldBaseAction = 0;
    actor->m_oldMotion = 0;
    actor->m_forceAct = 0;
    actor->m_forceMot = 0;
    actor->m_forceMaxMot = 0;
    actor->m_forceAnimSpeed = 0;
    actor->m_forceFinishedAct = 0;
    actor->m_forceFinishedMot = 0;
    actor->m_forceStartMot = 0;
    actor->m_isForceState = 0;
    actor->m_isForceAnimLoop = 0;
    actor->m_isForceAnimation = 0;
    actor->m_isForceAnimFinish = 0;
    actor->m_isForceState2 = 0;
    actor->m_isForceState3 = 0;
    actor->m_forceStateCnt = 0;
    actor->m_forceStateEndTick = 0;
    actor->m_sprRes = nullptr;
    actor->m_actRes = nullptr;
    actor->m_pos = vector3d{ 0.0f, 0.0f, 0.0f };
    actor->m_moveStartPos = actor->m_pos;
    actor->m_moveEndPos = actor->m_pos;
    actor->m_path.Reset();
    actor->m_msgEffectList.clear();

    if (CPc* pcActor = dynamic_cast<CPc*>(actor)) {
        pcActor->m_honor = 0;
        pcActor->m_virtue = 0;
        pcActor->m_headDir = 0;
        pcActor->m_head = 0;
        pcActor->m_headPalette = 0;
        pcActor->m_weapon = 0;
        pcActor->m_accessory = 0;
        pcActor->m_accessory2 = 0;
        pcActor->m_accessory3 = 0;
        pcActor->m_shield = 0;
        pcActor->InvalidateBillboard();
    }
}

bool IsLocalPlayerActor(const CGameMode& mode, u32 gid)
{
    if (gid == g_session.m_gid || gid == g_session.m_aid) {
        return true;
    }

    return mode.m_world && mode.m_world->m_player && mode.m_world->m_player->m_gid == gid;
}

CGameActor* FindRuntimeActorForVanish(CGameMode& mode, u32 gid)
{
    const auto it = mode.m_runtimeActors.find(gid);
    if (it != mode.m_runtimeActors.end()) {
        return it->second;
    }

    if (IsLocalPlayerActor(mode, gid) && mode.m_world && mode.m_world->m_player) {
        return mode.m_world->m_player;
    }

    return nullptr;
}

void BeginActorDeath(CGameMode& mode, CGameActor& actor, u32 gid)
{
    actor.m_isVisible = 1;
    actor.m_vanishTime = 0;
    actor.m_willBeDead = 0;
    actor.SendMsg(&actor, 28, 0, 0, 0);

    if (mode.m_world && mode.m_world->m_player && mode.m_world->m_player->m_proceedTargetGid == gid) {
        mode.m_world->m_player->m_proceedTargetGid = 0;
    }

    if (CPc* pcActor = dynamic_cast<CPc*>(&actor)) {
        pcActor->InvalidateBillboard();
    }

    // Monsters keep the existing corpse hold + fade timing, but PCs should
    // remain visible on the ground until a later vanish/remove packet arrives.
    if (!IsLocalPlayerActor(mode, gid) && actor.m_isPc == 0) {
        actor.m_willBeDead = 1;
    }
}

void HandleActorResurrection(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 8) {
        return;
    }

    const u32 gid = ReadLE32(packet.data + 2);
    if (gid == 0) {
        return;
    }

    if (CGameActor* actor = FindRuntimeActorForVanish(mode, gid)) {
        actor->SendMsg(actor, 98, 0, 0, 0);
    }

    if (IsLocalPlayerActor(mode, gid)) {
        mode.m_isOnQuest = 0;
        mode.m_isPlayerDead = 0;
        mode.m_waitingUseItemAck = 0;
    }

    DbgLog("[GameMode] resurrection gid=%u self=%d\n",
        gid,
        IsLocalPlayerActor(mode, gid) ? 1 : 0);
}

void LogActorPacketSample(const char* reason, const PacketView& packet)
{
    if (!packet.data || !reason) {
        return;
    }

    static std::map<u16, bool> loggedPacketIds;
    if (loggedPacketIds[packet.packetId]) {
        return;
    }
    loggedPacketIds[packet.packetId] = true;

    char hexBytes[3 * 24 + 1] = {};
    const int sampleBytes = (std::min)(static_cast<int>(packet.packetLength), 24);
    int cursor = 0;
    for (int i = 0; i < sampleBytes && cursor < static_cast<int>(sizeof(hexBytes)) - 4; ++i) {
        cursor += std::snprintf(hexBytes + cursor, sizeof(hexBytes) - cursor, "%02X%s",
            packet.data[i], (i + 1 < sampleBytes) ? " " : "");
    }

    DbgLog("[GamePacket] %s id=0x%04X len=%u sample=%s\n",
        reason, packet.packetId, packet.packetLength, hexBytes);
}

void LogDecodedActorOnce(const RuntimeActorState& state, bool isPc)
{
    static std::map<u32, bool> loggedActorIds;
    if (loggedActorIds[state.gid]) {
        return;
    }
    loggedActorIds[state.gid] = true;

    DbgLog("[GameMode] decoded actor pkt=0x%04X gid=%u job=%d objType=%u pc=%d pos=%d,%d dir=%d\n",
        state.packetId,
        state.gid,
        state.job,
        static_cast<unsigned int>(state.objectType),
        isPc ? 1 : 0,
        state.tileX,
        state.tileY,
        state.dir);
}

void LogActorPacketSeenOnce(const PacketView& packet)
{
    static std::map<u16, bool> loggedPacketIds;
    if (!packet.data || loggedPacketIds[packet.packetId]) {
        return;
    }
    loggedPacketIds[packet.packetId] = true;

    DbgLog("[GameMode] actor packet seen id=0x%04X len=%u\n",
        packet.packetId,
        packet.packetLength);
}

void ApplyRuntimeActorState(CGameMode& mode, const RuntimeActorState& state)
{
    const bool isPc = ShouldTreatActorAsPc(state.objectType, state.job);
    CGameActor* actor = EnsureRuntimeActor(mode, state.gid, ShouldUseSpriteBillboardActor(state.objectType, state.job));
    if (!actor) {
        return;
    }

    const int oldEffectState = actor->m_effectState;

    actor->m_speed = state.speed;
    actor->m_job = state.job;
    actor->m_sex = state.sex;
    actor->m_bodyState = state.bodyState;
    actor->m_healthState = state.healthState;
    actor->m_effectState = state.effectState;
    actor->m_pkState = state.pkState;
    actor->m_clevel = static_cast<u16>(state.clevel);
    actor->m_xSize = state.xSize;
    actor->m_ySize = state.ySize;
    actor->m_headType = state.headType;
    actor->m_gdid = state.guildId;
    actor->m_emblemVersion = state.emblemVersion;
    actor->m_charfont = static_cast<u16>(state.charfont);
    actor->m_objectType = state.objectType;
    actor->m_actorType = state.objectType;
    actor->m_isPc = isPc ? 1 : 0;
    actor->m_roty = PacketDirToRotationDegrees(state.dir);
    RefreshActorEffectStatePresentation(actor, oldEffectState);

    if (isPc) {
        mode.m_lastPcGid = state.gid;
    } else {
        mode.m_lastMonGid = state.gid;
    }

    if (CPc* pcActor = dynamic_cast<CPc*>(actor)) {
        pcActor->m_honor = state.manner;
        pcActor->m_virtue = state.karma;
        pcActor->m_headDir = state.headDir;
        pcActor->m_head = isPc ? state.hairStyle : 0;
        pcActor->m_headPalette = isPc ? state.hairColor : 0;
        pcActor->m_weapon = isPc ? state.weapon : 0;
        pcActor->m_shield = isPc ? state.shield : 0;
        pcActor->m_accessory = isPc ? state.headBottom : 0;
        pcActor->m_accessory2 = isPc ? state.headTop : 0;
        pcActor->m_accessory3 = isPc ? state.headMid : 0;
        pcActor->m_bodyPalette = isPc ? state.clothColor : 0;
    }

    if (state.state == 1) {
        actor->SendMsg(actor, 28, 0, 0, 0);
        if (CPc* pcActor = dynamic_cast<CPc*>(actor)) {
            pcActor->InvalidateBillboard();
        }
    } else if (state.state == 2) {
        StartSitStandAnimation(actor, true);
    } else if (actor->m_isSitting != 0) {
        StartSitStandAnimation(actor, false);
    } else if (CPc* pcActor = dynamic_cast<CPc*>(actor)) {
        pcActor->InvalidateBillboard();
    }

    if (state.hasPosition) {
        mode.m_actorPosList[state.gid] = CellPos{state.tileX, state.tileY};
        UpdateRuntimeActorPosition(mode, state.gid, state.tileX, state.tileY);
    }
}

void SeedMoveOnlyRemotePcAppearance(CGameActor* actor)
{
    CPc* pcActor = dynamic_cast<CPc*>(actor);
    if (!actor || !pcActor) {
        return;
    }

    actor->m_isPc = 1;
    actor->m_job = 0;
    actor->m_sex = 0;
    actor->m_bodyState = 0;
    actor->m_healthState = 0;
    actor->m_effectState = 0;
    actor->m_pkState = 0;
    actor->m_clevel = 1;
    actor->m_xSize = 5;
    actor->m_ySize = 5;
    actor->m_headType = 0;
    actor->m_objectType = 0;
    actor->m_actorType = 0;
    actor->m_bodyPalette = 0;

    pcActor->m_honor = 0;
    pcActor->m_virtue = 0;
    pcActor->m_headDir = 0;
    pcActor->m_head = 0;
    pcActor->m_headPalette = 0;
    pcActor->m_weapon = 0;
    pcActor->m_shield = 0;
    pcActor->m_accessory = 0;
    pcActor->m_accessory2 = 0;
    pcActor->m_accessory3 = 0;
}

bool DecodeActorIdleOrSpawnPacket(const PacketView& packet, RuntimeActorState& outState)
{
    if (!packet.data) {
        return false;
    }

    if (packet.packetId == 0x007C) {
        if (packet.packetLength < 42u) {
            LogActorPacketSample("short legacy non-player spawn packet", packet);
            return false;
        }

        outState = {};
        outState.packetId = packet.packetId;
        outState.objectType = packet.data[2];
        outState.gid = ReadLE32(packet.data + 3);
        outState.speed = ReadLE16(packet.data + 7);
        outState.bodyState = ReadLE16(packet.data + 9);
        outState.healthState = ReadLE16(packet.data + 11);
        outState.effectState = ReadLE16(packet.data + 13);
        outState.hairStyle = ReadLE16(packet.data + 15);
        outState.weapon = ReadLE16(packet.data + 17);
        outState.headBottom = ReadLE16(packet.data + 19);
        outState.job = ReadLE16(packet.data + 21);
        outState.shield = ReadLE16(packet.data + 23);
        outState.headTop = ReadLE16(packet.data + 25);
        outState.headMid = ReadLE16(packet.data + 27);
        outState.hairColor = ReadLE16(packet.data + 29);
        outState.clothColor = ReadLE16(packet.data + 31);
        outState.headDir = ReadLE16(packet.data + 33);
        outState.headType = outState.hairStyle;
        outState.karma = packet.data[35];
        outState.pkState = packet.data[35];
        outState.sex = packet.data[36];
        DecodePosDir(packet.data + 37, outState.tileX, outState.tileY, outState.dir);
        outState.hasPosition = true;
        return true;
    }

    if (packet.packetId == 0x0078 || packet.packetId == 0x0079 || packet.packetId == 0x01D8 || packet.packetId == 0x01D9) {
        const bool isIdle = packet.packetId == 0x0078 || packet.packetId == 0x01D8;
        const size_t minimumLength = isIdle ? 54u : 53u;
        if (packet.packetLength < minimumLength) {
            LogActorPacketSample("short legacy player idle/spawn packet", packet);
            return false;
        }

        auto decodeLegacyCandidate = [&](size_t shift, bool hasObjectType, RuntimeActorState& state) {
            const size_t levelOffset = isIdle ? (52u + shift) : (51u + shift);
            const size_t stateOffset = isIdle ? (51u + shift) : 0u;
            const u16 option = ReadLE16(packet.data + 12 + shift);
            const u16 opt3 = ReadLE16(packet.data + 42 + shift);

            state = {};
            state.packetId = packet.packetId;
            state.objectType = hasObjectType ? packet.data[2] : 0;
            state.gid = ReadLE32(packet.data + 2 + shift);
            state.speed = ReadLE16(packet.data + 6 + shift);
            state.bodyState = ReadLE16(packet.data + 8 + shift);
            state.healthState = ReadLE16(packet.data + 10 + shift);
            state.effectState = static_cast<int>(option != 0 ? option : opt3);
            state.job = ReadLE16(packet.data + 14 + shift);
            state.hairStyle = ReadLE16(packet.data + 16 + shift);
            state.weapon = ReadLE16(packet.data + 18 + shift);
            state.headBottom = ReadLE16(packet.data + 20 + shift);
            state.shield = ReadLE16(packet.data + 22 + shift);
            state.headTop = ReadLE16(packet.data + 24 + shift);
            state.headMid = ReadLE16(packet.data + 26 + shift);
            state.hairColor = ReadLE16(packet.data + 28 + shift);
            state.clothColor = ReadLE16(packet.data + 30 + shift);
            state.headDir = ReadLE16(packet.data + 32 + shift);
            state.headType = state.hairStyle;
            state.guildId = static_cast<int>(ReadLE32(packet.data + 34 + shift));
            state.emblemVersion = ReadLE16(packet.data + 38 + shift);
            state.manner = ReadLE16(packet.data + 40 + shift);
            state.karma = packet.data[44 + shift];
            state.pkState = packet.data[44 + shift];
            state.sex = packet.data[45 + shift];
            state.xSize = packet.data[49 + shift];
            state.ySize = packet.data[50 + shift];
            state.state = isIdle ? packet.data[stateOffset] : 0;
            state.clevel = ReadLE16(packet.data + levelOffset);
            DecodePosDir(packet.data + 46 + shift, state.tileX, state.tileY, state.dir);
            state.hasPosition = true;
        };

        RuntimeActorState classicState;
        decodeLegacyCandidate(0, false, classicState);

        const bool canUseShiftedLegacyLayout =
            (packet.packetId == 0x0078 || packet.packetId == 0x0079) &&
            packet.packetLength >= (minimumLength + 1u) &&
            IsKnownObjectType(packet.data[2]);

        if (!canUseShiftedLegacyLayout) {
            outState = classicState;
            return true;
        }

        RuntimeActorState shiftedState;
        decodeLegacyCandidate(1, true, shiftedState);

        if (ScoreLegacyActorState(shiftedState, true) >= ScoreLegacyActorState(classicState, false)) {
            outState = shiftedState;
        } else {
            outState = classicState;
        }
        return true;
    }

    if (packet.packetId == 0x07F8 || packet.packetId == 0x07F9 || packet.packetId == 0x0858 || packet.packetId == 0x0857) {
        const bool hasRobe = packet.packetId == 0x0858 || packet.packetId == 0x0857;
        const bool isIdle = packet.packetId == 0x07F9 || packet.packetId == 0x0857;
        const size_t minimumLength = hasRobe ? (isIdle ? 65u : 64u) : (isIdle ? 63u : 62u);
        if (packet.packetLength < minimumLength) {
            LogActorPacketSample("short variable player idle/spawn packet", packet);
            return false;
        }

        const size_t guildOffset = hasRobe ? 41u : 39u;
        const size_t emblemOffset = hasRobe ? 45u : 43u;
        const size_t mannerOffset = hasRobe ? 47u : 45u;
        const size_t opt3Offset = hasRobe ? 49u : 47u;
        const size_t karmaOffset = hasRobe ? 53u : 51u;
        const size_t sexOffset = hasRobe ? 54u : 52u;
        const size_t posOffset = hasRobe ? 55u : 53u;
        const size_t xSizeOffset = hasRobe ? 58u : 56u;
        const size_t ySizeOffset = hasRobe ? 59u : 57u;
        const size_t stateOffset = hasRobe ? 60u : 58u;
        const size_t levelOffset = hasRobe ? (isIdle ? 61u : 60u) : (isIdle ? 59u : 58u);

        const u32 option = ReadLE32(packet.data + 15);
        const u32 opt3 = ReadLE32(packet.data + opt3Offset);

        outState = {};
        outState.packetId = packet.packetId;
        outState.objectType = packet.data[4];
        outState.gid = ReadLE32(packet.data + 5);
        outState.speed = ReadLE16(packet.data + 9);
        outState.bodyState = ReadLE16(packet.data + 11);
        outState.healthState = ReadLE16(packet.data + 13);
        outState.effectState = static_cast<int>(option != 0 ? option : opt3);
        outState.job = ReadLE16(packet.data + 19);
        outState.hairStyle = ReadLE16(packet.data + 21);
        outState.weapon = ReadLE16(packet.data + 23);
        outState.shield = ReadLE16(packet.data + 25);
        outState.headBottom = ReadLE16(packet.data + 27);
        outState.headTop = ReadLE16(packet.data + 29);
        outState.headMid = ReadLE16(packet.data + 31);
        outState.hairColor = ReadLE16(packet.data + 33);
        outState.clothColor = ReadLE16(packet.data + 35);
        outState.headDir = ReadLE16(packet.data + 37);
        outState.headType = outState.hairStyle;
        outState.guildId = static_cast<int>(ReadLE32(packet.data + guildOffset));
        outState.emblemVersion = ReadLE16(packet.data + emblemOffset);
        outState.manner = ReadLE16(packet.data + mannerOffset);
        outState.karma = packet.data[karmaOffset];
        outState.pkState = packet.data[karmaOffset];
        outState.sex = packet.data[sexOffset];
        outState.clevel = ReadLE16(packet.data + levelOffset);
        outState.state = isIdle ? packet.data[stateOffset] : 0;
        outState.xSize = packet.data[xSizeOffset];
        outState.ySize = packet.data[ySizeOffset];
        DecodePosDir(packet.data + posOffset, outState.tileX, outState.tileY, outState.dir);
        outState.hasPosition = true;
        return true;
    }

    if (packet.packetId != 0x022A && packet.packetId != 0x022B && packet.packetId != 0x02ED && packet.packetId != 0x02EE) {
        return false;
    }

    const bool hasFont = packet.packetId == 0x02ED || packet.packetId == 0x02EE;
    const bool isIdle = packet.packetId == 0x022A || packet.packetId == 0x02EE;
    const size_t gidOffset = 2;
    const size_t speedOffset = 6;
    const size_t opt1Offset = 8;
    const size_t opt2Offset = 10;
    const size_t optionOffset = 12;
    const size_t jobOffset = 16;
    const size_t hairStyleOffset = 18;
    const size_t weaponOffset = 20;
    const size_t shieldOffset = 22;
    const size_t headBottomOffset = 24;
    const size_t headTopOffset = 26;
    const size_t headMidOffset = 28;
    const size_t hairColorOffset = 30;
    const size_t clothColorOffset = 32;
    const size_t headDirOffset = 34;
    const size_t guildOffset = 36;
    const size_t emblemOffset = 40;
    const size_t mannerOffset = 42;
    const size_t opt3Offset = 44;
    const size_t karmaOffset = 48;
    const size_t sexOffset = 49;
    const size_t posOffset = 50;
    const size_t xSizeOffset = 53;
    const size_t ySizeOffset = 54;
    const size_t stateOffset = 55;
    const size_t levelOffset = isIdle ? 56 : 55;
    const size_t fontOffset = isIdle ? 58 : 57;

    const size_t minimumLength = hasFont ? (fontOffset + 2) : (levelOffset + 2);
    if (packet.packetLength < minimumLength) {
        LogActorPacketSample("short player idle/spawn packet", packet);
        return false;
    }

    const u32 option = ReadLE32(packet.data + optionOffset);
    const u32 opt3 = ReadLE32(packet.data + opt3Offset);

    outState = {};
    outState.packetId = packet.packetId;
    outState.objectType = 0;
    outState.gid = ReadLE32(packet.data + gidOffset);
    outState.speed = ReadLE16(packet.data + speedOffset);
    outState.bodyState = ReadLE16(packet.data + opt1Offset);
    outState.healthState = ReadLE16(packet.data + opt2Offset);
    outState.effectState = static_cast<int>(option != 0 ? option : opt3);
    outState.pkState = packet.data[karmaOffset];
    outState.job = ReadLE16(packet.data + jobOffset);
    outState.hairStyle = ReadLE16(packet.data + hairStyleOffset);
    outState.weapon = ReadLE16(packet.data + weaponOffset);
    outState.shield = ReadLE16(packet.data + shieldOffset);
    outState.headBottom = ReadLE16(packet.data + headBottomOffset);
    outState.headTop = ReadLE16(packet.data + headTopOffset);
    outState.headMid = ReadLE16(packet.data + headMidOffset);
    outState.hairColor = ReadLE16(packet.data + hairColorOffset);
    outState.clothColor = ReadLE16(packet.data + clothColorOffset);
    outState.headDir = ReadLE16(packet.data + headDirOffset);
    outState.headType = outState.hairStyle;
    outState.sex = packet.data[sexOffset];
    outState.guildId = static_cast<int>(ReadLE32(packet.data + guildOffset));
    outState.emblemVersion = ReadLE16(packet.data + emblemOffset);
    outState.manner = ReadLE16(packet.data + mannerOffset);
    outState.karma = packet.data[karmaOffset];
    outState.clevel = ReadLE16(packet.data + levelOffset);
    outState.state = isIdle ? packet.data[stateOffset] : 0;
    outState.charfont = hasFont ? ReadLE16(packet.data + fontOffset) : 0;
    outState.xSize = packet.data[xSizeOffset];
    outState.ySize = packet.data[ySizeOffset];
    DecodePosDir(packet.data + posOffset, outState.tileX, outState.tileY, outState.dir);
    outState.hasPosition = true;
    return true;
}

bool DecodeActorWalkingPacket(const PacketView& packet, RuntimeActorState& outState)
{
    if (!packet.data) {
        return false;
    }

    if (packet.packetId == 0x007B || packet.packetId == 0x01DA) {
        if (packet.packetLength < 60u) {
            LogActorPacketSample("short legacy player walking packet", packet);
            return false;
        }

        auto decodeLegacyMoveCandidate = [&](size_t shift, bool hasObjectType, RuntimeActorState& state) {
            const u16 option = ReadLE16(packet.data + 12 + shift);

            state = {};
            state.packetId = packet.packetId;
            state.objectType = hasObjectType ? packet.data[2] : 0;
            state.gid = ReadLE32(packet.data + 2 + shift);
            state.speed = ReadLE16(packet.data + 6 + shift);
            state.bodyState = ReadLE16(packet.data + 8 + shift);
            state.healthState = ReadLE16(packet.data + 10 + shift);
            state.effectState = option;
            state.job = ReadLE16(packet.data + 14 + shift);
            state.hairStyle = ReadLE16(packet.data + 16 + shift);
            state.weapon = ReadLE16(packet.data + 18 + shift);
            state.headBottom = ReadLE16(packet.data + 20 + shift);
            state.shield = ReadLE16(packet.data + 26 + shift);
            state.headTop = ReadLE16(packet.data + 28 + shift);
            state.headMid = ReadLE16(packet.data + 30 + shift);
            state.hairColor = ReadLE16(packet.data + 32 + shift);
            state.clothColor = ReadLE16(packet.data + 34 + shift);
            state.headDir = ReadLE16(packet.data + 36 + shift);
            state.headType = state.hairStyle;
            state.guildId = static_cast<int>(ReadLE32(packet.data + 38 + shift));
            state.emblemVersion = ReadLE16(packet.data + 42 + shift);
            state.manner = ReadLE16(packet.data + 44 + shift);
            state.karma = packet.data[48 + shift];
            state.pkState = packet.data[48 + shift];
            state.sex = packet.data[49 + shift];
            state.xSize = packet.data[56 + shift];
            state.ySize = packet.data[57 + shift];
            state.clevel = ReadLE16(packet.data + 58 + shift);

            int srcX = 0;
            int srcY = 0;
            int dstX = 0;
            int dstY = 0;
            int srcDir = 0;
            int dstDir = 0;
            DecodeSrcDst(packet.data + 50 + shift, srcX, srcY, dstX, dstY, srcDir, dstDir);
            state.tileX = dstX;
            state.tileY = dstY;
            state.dir = dstDir;
            state.hasPosition = true;
        };

        RuntimeActorState classicState;
        decodeLegacyMoveCandidate(0, false, classicState);

        const bool canUseShiftedLegacyLayout =
            packet.packetId == 0x007B &&
            packet.packetLength >= 61u &&
            IsKnownObjectType(packet.data[2]);

        if (!canUseShiftedLegacyLayout) {
            outState = classicState;
            return true;
        }

        RuntimeActorState shiftedState;
        decodeLegacyMoveCandidate(1, true, shiftedState);
        if (ScoreLegacyActorState(shiftedState, true) >= ScoreLegacyActorState(classicState, false)) {
            outState = shiftedState;
        } else {
            outState = classicState;
        }
        return true;
    }

    if (packet.packetId == 0x07F7 || packet.packetId == 0x0856) {
        const bool hasRobe = packet.packetId == 0x0856;
        const size_t minimumLength = hasRobe ? 71u : 69u;
        if (packet.packetLength < minimumLength) {
            LogActorPacketSample("short variable player walking packet", packet);
            return false;
        }

        const size_t guildOffset = hasRobe ? 45u : 43u;
        const size_t emblemOffset = hasRobe ? 49u : 47u;
        const size_t mannerOffset = hasRobe ? 51u : 49u;
        const size_t opt3Offset = hasRobe ? 53u : 51u;
        const size_t karmaOffset = hasRobe ? 57u : 55u;
        const size_t sexOffset = hasRobe ? 58u : 56u;
        const size_t moveOffset = hasRobe ? 59u : 57u;
        const size_t xSizeOffset = hasRobe ? 65u : 63u;
        const size_t ySizeOffset = hasRobe ? 66u : 64u;
        const size_t levelOffset = hasRobe ? 67u : 65u;

        const u32 option = ReadLE32(packet.data + 15);
        const u32 opt3 = ReadLE32(packet.data + opt3Offset);

        outState = {};
        outState.packetId = packet.packetId;
        outState.objectType = packet.data[4];
        outState.gid = ReadLE32(packet.data + 5);
        outState.speed = ReadLE16(packet.data + 9);
        outState.bodyState = ReadLE16(packet.data + 11);
        outState.healthState = ReadLE16(packet.data + 13);
        outState.effectState = static_cast<int>(option != 0 ? option : opt3);
        outState.job = ReadLE16(packet.data + 19);
        outState.hairStyle = ReadLE16(packet.data + 21);
        outState.weapon = ReadLE16(packet.data + 23);
        outState.shield = ReadLE16(packet.data + 25);
        outState.headBottom = ReadLE16(packet.data + 27);
        outState.headTop = ReadLE16(packet.data + 33);
        outState.headMid = ReadLE16(packet.data + 35);
        outState.hairColor = ReadLE16(packet.data + 37);
        outState.clothColor = ReadLE16(packet.data + 39);
        outState.headDir = ReadLE16(packet.data + 41);
        outState.headType = outState.hairStyle;
        outState.guildId = static_cast<int>(ReadLE32(packet.data + guildOffset));
        outState.emblemVersion = ReadLE16(packet.data + emblemOffset);
        outState.manner = ReadLE16(packet.data + mannerOffset);
        outState.karma = packet.data[karmaOffset];
        outState.pkState = packet.data[karmaOffset];
        outState.sex = packet.data[sexOffset];
        outState.xSize = packet.data[xSizeOffset];
        outState.ySize = packet.data[ySizeOffset];
        outState.clevel = ReadLE16(packet.data + levelOffset);

        int srcX = 0;
        int srcY = 0;
        int dstX = 0;
        int dstY = 0;
        int srcDir = 0;
        int dstDir = 0;
        DecodeSrcDst(packet.data + moveOffset, srcX, srcY, dstX, dstY, srcDir, dstDir);
        outState.tileX = dstX;
        outState.tileY = dstY;
        outState.dir = dstDir;
        outState.hasPosition = true;
        return true;
    }

    if (packet.packetId != 0x022C && packet.packetId != 0x02EC) {
        return false;
    }

    const bool hasFont = packet.packetId == 0x02EC;
    const size_t minimumLength = hasFont ? 67 : 65;
    if (packet.packetLength < minimumLength) {
        if (packet.packetId == 0x022C || packet.packetId == 0x02EC) {
            LogActorPacketSample("short player walking packet", packet);
        }
        return false;
    }

    outState = {};
    outState.packetId = packet.packetId;
    outState.objectType = packet.data[2];
    outState.gid = ReadLE32(packet.data + 3);
    outState.speed = ReadLE16(packet.data + 7);
    outState.bodyState = ReadLE16(packet.data + 9);
    outState.healthState = ReadLE16(packet.data + 11);
    const u32 option = ReadLE32(packet.data + 13);
    const u32 opt3 = ReadLE32(packet.data + 49);
    outState.effectState = static_cast<int>(option != 0 ? option : opt3);
    outState.job = ReadLE16(packet.data + 17);
    outState.hairStyle = ReadLE16(packet.data + 19);
    outState.weapon = ReadLE16(packet.data + 21);
    outState.shield = ReadLE16(packet.data + 23);
    outState.headBottom = ReadLE16(packet.data + 25);
    outState.headTop = ReadLE16(packet.data + 31);
    outState.headMid = ReadLE16(packet.data + 33);
    outState.hairColor = ReadLE16(packet.data + 35);
    outState.clothColor = ReadLE16(packet.data + 37);
    outState.headDir = ReadLE16(packet.data + 39);
    outState.headType = outState.hairStyle;
    outState.guildId = static_cast<int>(ReadLE32(packet.data + 41));
    outState.emblemVersion = ReadLE16(packet.data + 45);
    outState.manner = ReadLE16(packet.data + 47);
    outState.karma = packet.data[53];
    outState.pkState = packet.data[53];
    outState.sex = packet.data[54];
    outState.xSize = packet.data[61];
    outState.ySize = packet.data[62];
    outState.clevel = ReadLE16(packet.data + 63);
    outState.charfont = hasFont ? ReadLE16(packet.data + 65) : 0;

    int srcX = 0;
    int srcY = 0;
    int dstX = 0;
    int dstY = 0;
    int srcDir = 0;
    int dstDir = 0;
    DecodeSrcDst(packet.data + 55, srcX, srcY, dstX, dstY, srcDir, dstDir);
    outState.tileX = dstX;
    outState.tileY = dstY;
    outState.dir = dstDir;
    outState.hasPosition = true;
    return true;
}

void HandleActorDirection(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 9) {
        return;
    }

    const u32 gid = ReadLE32(packet.data + 2);
    const u16 headDir = ReadLE16(packet.data + 6);
    const u8 dir = packet.data[8] & 7;

    mode.m_aidList[gid] = GetTickCount();

    CGameActor* actor = EnsureRuntimeActor(mode, gid);
    if (!actor) {
        return;
    }

    actor->m_roty = PacketDirToRotationDegrees(dir);
    if (actor->m_isSitting != 0) {
        const int baseAction = (actor->m_isPc != 0) ? 16 : 0;
        const int resolvedAction = baseAction + actor->Get8Dir(actor->m_roty);
        actor->m_baseAction = baseAction;
        actor->m_curAction = resolvedAction;
        actor->m_oldBaseAction = baseAction;
        actor->m_oldMotion = actor->m_curMotion;
    }

    if (CPc* pc = dynamic_cast<CPc*>(actor)) {
        pc->m_headDir = (std::max)(0, (std::min)(static_cast<int>(headDir), 2));
        if (!pc->m_isMoving && (pc->m_isSitting != 0 || pc->m_baseAction == 0 || pc->m_baseAction == 16)) {
            pc->m_curMotion = pc->m_headDir;
            pc->m_oldMotion = pc->m_headDir;
        }
        pc->InvalidateBillboard();
    }

    if (IsLocalPlayerActor(mode, gid)) {
        g_session.SetPlayerPosDir(g_session.m_playerPosX, g_session.m_playerPosY, dir);
    }
}

void HandleActorStateChange(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data) {
        return;
    }

    if (mode.m_mapLoadingStage != CGameMode::MapLoading_None) {
        mode.m_lastActorBootstrapPacketTick = GetTickCount();
    }

    u32 gid = 0;
    int bodyState = 0;
    int healthState = 0;
    int effectState = 0;
    int pkState = 0;

    if (packet.packetId == 0x0119) {
        if (packet.packetLength < 13) {
            return;
        }

        gid = ReadLE32(packet.data + 2);
        bodyState = ReadLE16(packet.data + 6);
        healthState = ReadLE16(packet.data + 8);
        effectState = ReadLE16(packet.data + 10);
        pkState = packet.data[12];
    } else if (packet.packetId == 0x0229) {
        if (packet.packetLength < 15) {
            return;
        }

        gid = ReadLE32(packet.data + 2);
        bodyState = ReadLE16(packet.data + 6);
        healthState = ReadLE16(packet.data + 8);
        effectState = static_cast<int>(ReadLE32(packet.data + 10));
        pkState = packet.data[14];
    } else {
        return;
    }

    mode.m_aidList[gid] = GetTickCount();

    CGameActor* actor = EnsureRuntimeActor(mode, gid);
    if (!actor) {
        return;
    }

    const int oldEffectState = actor->m_effectState;

    actor->m_bodyState = bodyState;
    actor->m_healthState = healthState;
    actor->m_effectState = effectState;
    actor->m_pkState = pkState;
    RefreshActorEffectStatePresentation(actor, oldEffectState);
}

CGameActor* EnsureRuntimeActor(CGameMode& mode, u32 gid, bool preferPc)
{
    if (IsLocalPlayerActor(mode, gid) && mode.m_world && mode.m_world->m_player) {
        return mode.m_world->m_player;
    }

    const auto it = mode.m_runtimeActors.find(gid);
    if (it != mode.m_runtimeActors.end()) {
        if (preferPc && !dynamic_cast<CPc*>(it->second)) {
            CGameActor* existing = it->second;
            CPc* upgraded = new CPc();
            if (!upgraded) {
                return existing;
            }

            static_cast<CGameActor&>(*upgraded) = *existing;
            upgraded->m_birdEffect = nullptr;
            upgraded->m_msgEffectList.clear();
            existing->UnRegisterPos();
            delete existing;

            it->second = upgraded;
            upgraded->RegisterPos();
            DbgLog("[GameMode] upgraded runtime actor gid=%u to CPc\n", gid);
            return upgraded;
        }
        return it->second;
    }

    CGameActor* actor = preferPc
        ? static_cast<CGameActor*>(new CPc())
        : static_cast<CGameActor*>(new CGameActor());
    if (!actor) {
        return nullptr;
    }

    InitializeRuntimeActorDefaults(actor, gid);
    mode.m_runtimeActors.emplace(gid, actor);
    if (preferPc) {
        DbgLog("[GameMode] created runtime CPc gid=%u\n", gid);
    }
    return actor;
}

void UpdateRuntimeActorPosition(CGameMode& mode, u32 gid, int tileX, int tileY, int srcX, int srcY)
{
    CGameActor* actor = EnsureRuntimeActor(mode, gid);
    if (!actor) {
        return;
    }

    actor->m_isVisible = 1;

    const bool traceRemoteMove = actor->m_isPc == 0;
    const u32 clientTickNow = GetTickCount();
    const u32 serverTickNow = g_session.GetServerTime();
    const int prevSrcX = actor->m_moveSrcX;
    const int prevSrcY = actor->m_moveSrcY;
    const int prevDstX = actor->m_moveDestX;
    const int prevDstY = actor->m_moveDestY;
    const u32 prevMoveStart = actor->m_moveStartTime;
    const u32 prevMoveEnd = actor->m_moveEndTime;
    const int prevIsMoving = actor->m_isMoving;
    const vector3d prevPos = actor->m_pos;

    if (actor->m_isMoving) {
        actor->ProcessState();
    }

    actor->UnRegisterPos();
    const bool isLocalPlayer = IsLocalPlayerActor(mode, gid);
    const bool keepInterpolatedMoveStart = isLocalPlayer && actor->m_isMoving;
    actor->m_moveSrcX = srcX >= 0 ? srcX : actor->m_moveDestX;
    actor->m_moveSrcY = srcY >= 0 ? srcY : actor->m_moveDestY;
    actor->m_moveDestX = tileX;
    actor->m_moveDestY = tileY;
    actor->m_lastTlvertX = tileX;
    actor->m_lastTlvertY = tileY;
    actor->m_path.Reset();

    actor->m_moveStartPos = actor->m_pos;
    if (!keepInterpolatedMoveStart && isLocalPlayer && srcX >= 0 && srcY >= 0 && mode.m_world) {
        const float srcWorldX = TileToWorldCoordX(mode.m_world, actor->m_moveSrcX);
        const float srcWorldZ = TileToWorldCoordZ(mode.m_world, actor->m_moveSrcY);
        actor->m_moveStartPos.x = srcWorldX;
        actor->m_moveStartPos.z = srcWorldZ;
        actor->m_moveStartPos.y = ResolveActorHeight(mode.m_world, srcWorldX, srcWorldZ);
    }
    const float worldX = TileToWorldCoordX(mode.m_world, tileX);
    const float worldZ = TileToWorldCoordZ(mode.m_world, tileY);
    actor->m_moveEndPos.x = worldX;
    actor->m_moveEndPos.z = worldZ;
    actor->m_moveEndPos.y = ResolveActorHeight(mode.m_world, worldX, worldZ);
    if (actor->m_moveStartPos.x == 0.0f && actor->m_moveStartPos.y == 0.0f && actor->m_moveStartPos.z == 0.0f) {
        actor->m_moveStartPos = actor->m_moveEndPos;
    }
    actor->m_moveStartTime = g_session.GetServerTime();
    actor->m_moveEndTime = actor->m_moveStartTime;

    if (mode.m_world && mode.m_world->m_attr) {
        g_pathFinder.SetMap(mode.m_world->m_attr);
        const int speed = static_cast<int>(actor->m_speed != 0 ? actor->m_speed : 150u);
        if (g_pathFinder.FindPath(actor->m_moveStartTime,
                actor->m_moveSrcX,
                actor->m_moveSrcY,
                tileX,
                tileY,
                0,
                0,
                speed,
                &actor->m_path)
            && actor->m_path.m_cells.size() >= 2) {
            actor->m_moveEndTime = actor->m_path.m_cells.back().arrivalTime;
        }
    }

    actor->m_isMoving = actor->m_moveEndTime > actor->m_moveStartTime;
    if (actor->m_isMoving) {
        actor->m_isSitting = 0;
        actor->SetState(1);
        if (CPc* pcActor = dynamic_cast<CPc*>(actor)) {
            pcActor->m_headDir = 0;
            pcActor->m_curMotion = 0;
            pcActor->m_oldMotion = 0;
            pcActor->InvalidateBillboard();
        }
    }
    if (!actor->m_isMoving) {
        actor->m_pos = actor->m_moveEndPos;
        actor->SetState(0);
    } else if (isLocalPlayer && actor->m_path.m_cells.size() >= 2) {
        if (!InterpolateRuntimeActorPathPosition(mode.m_world, *actor, g_session.GetServerTime(), &actor->m_pos)) {
            actor->m_pos = actor->m_moveStartPos;
        }
    } else {
        actor->m_pos = actor->m_moveStartPos;
    }
    actor->RegisterPos();

    if (traceRemoteMove) {
        RemoteMoveApplyTrace& trace = g_remoteMoveApplyTraceByGid[gid];
        const u32 clientDelta = trace.lastClientTick != 0 ? (clientTickNow - trace.lastClientTick) : 0;
        const u32 serverDelta = trace.lastServerTick != 0 ? (serverTickNow - trace.lastServerTick) : 0;
        DbgLog("[ActorMove] apply gid=%u pktSrc=%d,%d pktDst=%d,%d prevSrc=%d,%d prevDst=%d,%d prevMove=[%u..%u] prevMoving=%d prevPos=(%.2f,%.2f) serverNow=%u serverDelta=%u clientDelta=%u newMove=[%u..%u] newMoving=%d pathCells=%zu newPos=(%.2f,%.2f)\n",
            gid,
            srcX,
            srcY,
            tileX,
            tileY,
            prevSrcX,
            prevSrcY,
            prevDstX,
            prevDstY,
            static_cast<unsigned int>(prevMoveStart),
            static_cast<unsigned int>(prevMoveEnd),
            prevIsMoving,
            prevPos.x,
            prevPos.z,
            static_cast<unsigned int>(serverTickNow),
            static_cast<unsigned int>(serverDelta),
            static_cast<unsigned int>(clientDelta),
            static_cast<unsigned int>(actor->m_moveStartTime),
            static_cast<unsigned int>(actor->m_moveEndTime),
            actor->m_isMoving,
            actor->m_path.m_cells.size(),
            actor->m_pos.x,
            actor->m_pos.z);
        trace.lastClientTick = clientTickNow;
        trace.lastServerTick = serverTickNow;
    }
}

void ApplyRuntimeActorFixPosition(CGameMode& mode, u32 gid, int tileX, int tileY)
{
    CGameActor* actor = EnsureRuntimeActor(mode, gid);
    if (!actor) {
        return;
    }

    actor->m_isVisible = 1;

    actor->UnRegisterPos();
    actor->m_moveSrcX = tileX;
    actor->m_moveSrcY = tileY;
    actor->m_moveDestX = tileX;
    actor->m_moveDestY = tileY;
    actor->m_lastTlvertX = tileX;
    actor->m_lastTlvertY = tileY;
    actor->m_path.Reset();

    const float worldX = TileToWorldCoordX(mode.m_world, tileX);
    const float worldZ = TileToWorldCoordZ(mode.m_world, tileY);
    actor->m_moveStartPos.x = worldX;
    actor->m_moveStartPos.z = worldZ;
    actor->m_moveStartPos.y = ResolveActorHeight(mode.m_world, worldX, worldZ);
    actor->m_moveEndPos = actor->m_moveStartPos;
    actor->m_moveStartTime = g_session.GetServerTime();
    actor->m_moveEndTime = actor->m_moveStartTime;
    actor->m_isMoving = 0;
    actor->m_pos = actor->m_moveEndPos;
    actor->SetState(0);
    actor->RegisterPos();
}

bool FindPathCellIndex(const CPathInfo& path, int tileX, int tileY, size_t* outIndex)
{
    if (!outIndex) {
        return false;
    }

    for (size_t index = 0; index < path.m_cells.size(); ++index) {
        if (path.m_cells[index].x == tileX && path.m_cells[index].y == tileY) {
            *outIndex = index;
            return true;
        }
    }

    return false;
}

bool ShouldIgnoreRedundantLocalFixPosition(const CGameActor& actor, int tileX, int tileY, size_t* outFixIndex, size_t* outActiveIndex)
{
    if (!actor.m_isMoving || actor.m_path.m_cells.size() < 2) {
        return false;
    }

    size_t fixIndex = 0;
    if (!FindPathCellIndex(actor.m_path, tileX, tileY, &fixIndex)) {
        return false;
    }

    size_t activeIndex = 0;
    if (!FindActivePathSegmentForPacketView(actor.m_path, g_session.GetServerTime(), &activeIndex)) {
        return false;
    }

    if (outFixIndex) {
        *outFixIndex = fixIndex;
    }
    if (outActiveIndex) {
        *outActiveIndex = activeIndex;
    }

    // Treat fixpos as correction only when it is meaningfully ahead of the
    // client's current path progress. If the packet lands on or behind the
    // current segment, keep local motion to avoid visible step-back jitter.
    return fixIndex <= activeIndex + 1;
}

void RemoveRuntimeActor(CGameMode& mode, u32 gid)
{
    mode.m_actorPosList.erase(gid);
    mode.m_aidList.erase(gid);
    mode.m_actorNameList.erase(gid);
    mode.m_actorNameReqTimer.erase(gid);
    mode.m_actorNameListByGID.erase(gid);
    mode.m_actorNameByGIDReqTimer.erase(gid);

    if (mode.m_lastPcGid == gid) {
        mode.m_lastPcGid = 0;
    }
    if (mode.m_lastMonGid == gid) {
        mode.m_lastMonGid = 0;
    }
    if (mode.m_lastLockOnMonGid == gid) {
        mode.m_lastLockOnMonGid = 0;
    }
    if (mode.m_attackChaseTargetGid == gid) {
        mode.m_attackChaseTargetGid = 0;
    }
    if (mode.m_world && mode.m_world->m_player && mode.m_world->m_player->m_proceedTargetGid == gid) {
        mode.m_world->m_player->m_proceedTargetGid = 0;
    }

    const auto it = mode.m_runtimeActors.find(gid);
    if (it == mode.m_runtimeActors.end()) {
        return;
    }

    if (mode.m_world && mode.m_world->m_player == it->second) {
        mode.m_world->m_player = nullptr;
    }

    if (it->second) {
        it->second->UnRegisterPos();
        delete it->second;
    }
    mode.m_runtimeActors.erase(it);
}

ChatEntry BuildChatEntry(const std::string& text, u32 color, u8 channel)
{
    ChatEntry entry{};
    entry.color = color & 0x00FFFFFFu;
    entry.channel = channel;
    entry.text = text;
    return entry;
}

std::string ExtractChatTextWithAid(const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 8) {
        return {};
    }

    std::string text = ExtractPacketString(packet, 8);
    if (!text.empty()) {
        return text;
    }

    // Some older traces suggest chat payloads may occasionally arrive without
    // the actor-id field populated as expected, so keep a legacy fallback.
    return ExtractPacketString(packet, 4);
}

void PropagateChatToUi(const ChatEntry& entry)
{
    if (entry.text.empty()) {
        return;
    }

    g_windowMgr.PushChatEvent(entry.text.c_str(), entry.color, entry.channel, GetTickCount());
}

void RecordChat(CGameMode& mode, const std::string& text, u32 color, u8 channel)
{
    if (text.empty()) {
        return;
    }

    const ChatEntry entry = BuildChatEntry(text, color, channel);
    PropagateChatToUi(entry);

    if (mode.m_lastChat == entry.text) {
        ++mode.m_sameChatRepeatCnt;
    } else {
        mode.m_sameChatRepeatCnt = 0;
    }
    mode.m_lastChat = entry.text;

    int slot = 0;
    if (mode.m_recordChatNum < 11) {
        slot = mode.m_recordChatNum;
        ++mode.m_recordChatNum;
    } else {
        for (int i = 1; i < 11; ++i) {
            mode.m_recordChat[i - 1] = mode.m_recordChat[i];
            mode.m_recordChatTime[i - 1] = mode.m_recordChatTime[i];
        }
        slot = 10;
    }

    mode.m_recordChat[slot] = entry.text;
    mode.m_recordChatTime[slot] = GetTickCount();
}

void HandleNotifyChat(CGameMode& mode, const PacketView& packet)
{
    // Ref: Zc_Notify_Chat (0x008D)
    if (!packet.data || packet.packetLength < 8) {
        return;
    }

    const u32 aid = ReadLE32(packet.data + 4);
    mode.m_aidList[aid] = GetTickCount();
    RecordChat(mode, ExtractChatTextWithAid(packet), 0x00FFFFFF, kChatChannelNormal);
}

void HandleNotifyPlayerChat(CGameMode& mode, const PacketView& packet)
{
    // Ref: Zc_Notify_Playerchat (0x008E)
    if (!packet.data || packet.packetLength < 5) {
        return;
    }

    RecordChat(mode, ExtractPacketString(packet, 4), 0x0000FF00, kChatChannelPlayer);
}

void HandleWhisper(CGameMode& mode, const PacketView& packet)
{
    // Ref: Zc_Whisper (0x0097)
    if (!packet.data || packet.packetLength < 29) {
        return;
    }

    std::string name(reinterpret_cast<const char*>(packet.data + 4), 24);
    const size_t nulPos = name.find('\0');
    if (nulPos != std::string::npos) {
        name.resize(nulPos);
    }

    const std::string msg = ExtractPacketString(packet, 28);
    mode.m_lastWhisperName = name;
    mode.m_lastWhisper = msg;

    if (!name.empty() && !msg.empty()) {
        RecordChat(mode, name + " : " + msg, 0x00222222, kChatChannelWhisper);
    } else if (!msg.empty()) {
        RecordChat(mode, msg, 0x00222222, kChatChannelWhisper);
    }
}

void HandleNotifyChatParty(CGameMode& mode, const PacketView& packet)
{
    // Ref: Zc_Notify_Chat_Party (0x0109)
    if (!packet.data || packet.packetLength < 8) {
        return;
    }

    const u32 aid = ReadLE32(packet.data + 4);
    mode.m_aidList[aid] = GetTickCount();

    const std::string msg = ExtractPacketString(packet, 8);
    if (!msg.empty()) {
        RecordChat(mode, msg, 0x00C8C8FF, kChatChannelParty);
    }
}

void HandleBattlefieldChat(CGameMode& mode, const PacketView& packet)
{
    // Ref: Zc_Battlefield_Chat (0x02DC)
    if (!packet.data || packet.packetLength < 32) {
        return;
    }

    const u32 aid = ReadLE32(packet.data + 4);
    mode.m_aidList[aid] = GetTickCount();

    const std::string msg = ExtractPacketString(packet, 32);
    if (!msg.empty()) {
        RecordChat(mode, msg, 0x009B07FF, kChatChannelBattlefield);
    }
}

void HandleAckWhisper(CGameMode& mode, const PacketView& packet)
{
    // Ref: Zc_Ack_Whisper (0x0098)
    if (!packet.data || packet.packetLength < 3) {
        return;
    }

    const u8 status = packet.data[2];
    if (status == 0) {
        if (!mode.m_lastWhisper.empty()) {
            RecordChat(mode, std::string("(to ") + mode.m_lastWhisperName + ") " + mode.m_lastWhisper,
                0x00FFFF00, kChatChannelWhisper);
        }
    } else {
        RecordChat(mode, "Whisper delivery failed.", 0x00FF0000, kChatChannelSystem);
    }
}

void HandleBroadcast(CGameMode& mode, const PacketView& packet)
{
    // Ref: Zc_Broadcast (0x009A), Zc_Broadcast2 (0x01C3)
    if (!packet.data || packet.packetLength < 5) {
        return;
    }

    const BroadcastPayload payload = ParseBroadcastPayload(packet);
    if (payload.text.empty()) {
        return;
    }

    mode.m_broadCastTick = GetTickCount();
    RecordChat(mode, payload.text, payload.color, kChatChannelBroadcast);
}

void ReturnToLoginMode()
{
    CRagConnection::instance()->Disconnect();
    g_modeMgr.Switch(0, "");
}

} // namespace

void SetPendingDisconnectAction(PendingDisconnectAction action)
{
    g_pendingDisconnectAction = action;
}

PendingDisconnectAction GetPendingDisconnectAction()
{
    return g_pendingDisconnectAction;
}

void ClearPendingDisconnectAction()
{
    g_pendingDisconnectAction = PendingDisconnectAction::None;
}

namespace {

void HandleBanDisconnect(CGameMode&, const PacketView& packet)
{
    // Ref: Sc_Notify_Ban (0x0081)
    if (!packet.data || packet.packetLength < 3) {
        return;
    }
    ReturnToLoginMode();
}

void HandleDisconnectCharacterAck(CGameMode&, const PacketView& packet)
{
    // Ref: Zc_Ack_Disconnect_Character flow (0x00B3)
    if (!packet.data || packet.packetLength < 3) {
        return;
    }

    const bool accepted = (packet.data[2] == 1);
    const PendingDisconnectAction pendingAction = GetPendingDisconnectAction();
    ClearPendingDisconnectAction();

    if (accepted) {
        if (pendingAction == PendingDisconnectAction::ReturnToCharSelect) {
            g_session.m_pendingReturnToCharSelect = 1;
        } else {
            g_session.m_pendingReturnToCharSelect = 0;
        }
        ReturnToLoginMode();
    } else {
        g_session.m_pendingReturnToCharSelect = 0;
    }
}

void HandleLoginAccept(CGameMode&, const PacketView& packet)
{
    // AC_ACCEPT_LOGIN (0x0069)
    // [0..1] type, [2..3] len, [4..7] authCode, [8..11] aid, [38] sex
    if (!packet.data || packet.packetLength < 39) {
        return;
    }

    g_session.m_authCode = ReadLE32(packet.data + 4);
    g_session.m_aid      = ReadLE32(packet.data + 8);
    g_session.m_sex      = packet.data[38];
}

void HandleLoginRefuse(CGameMode&, const PacketView&)
{
    // AC_REFUSE_LOGIN (0x006A)
    g_session.m_authCode = 0;
    g_session.m_aid = 0;
}

void HandleMapChange(CGameMode& mode, const PacketView& packet)
{
    MapChangeInfo info;
    if (!ParseMapChangePacket(packet, info)) {
        return;
    }

    const bool sameMapAlreadyLoaded = !info.hasServerMove && IsSameMapAlreadyLoaded(mode, info);
    ApplyMapChangeSessionState(info);

    DbgLog("[GameMode] map change packet=0x%04X map='%s' pos=%d,%d serverMove=%d\n",
        packet.packetId,
        g_session.m_curMap,
        info.x,
        info.y,
        info.hasServerMove ? 1 : 0);

    if (sameMapAlreadyLoaded) {
        PrepareSameMapWarpReuse(mode);
        const bool sentLoadAck = SendLoadEndAckPacket();
        mode.m_sentLoadEndAck = sentLoadAck ? 1 : 0;
        mode.m_mapLoadingAckTick = GetTickCount();
        mode.m_lastActorBootstrapPacketTick = mode.m_mapLoadingAckTick;
        DbgLog("[GameMode] reused loaded map='%s' pos=%d,%d sentLoadAck=%d\n",
            g_session.m_curMap,
            info.x,
            info.y,
            sentLoadAck ? 1 : 0);
        return;
    }

    g_modeMgr.PresentLoadingScreen("Loading new map...", 0.01f);

    if (!ReconnectForServerMove(info)) {
        ReturnToLoginMode();
        return;
    }

    char worldName[40] = {};
    std::snprintf(worldName, sizeof(worldName), "%s.rsw", g_session.m_curMap);
    g_modeMgr.Switch(1, worldName);
}

void HandleAcceptEnter(CGameMode&, const PacketView& packet)
{
    // Ref: Zc_Accept_Enter (packet type 0x0073)
    // [2..5] server time, [6..8] packed x/y/dir
    if (!packet.data || packet.packetLength < 9) {
        return;
    }

    g_session.SetServerTime(ReadLE32(packet.data + 2));

    const int posX = (packet.data[7] >> 6) | (4 * packet.data[6]);
    const int posY = (packet.data[8] >> 4) | (16 * (packet.data[7] & 0x3F));
    const int dir  = packet.data[8] & 0x0F;
    g_session.SetPlayerPosDir(posX, posY, dir);

    char worldName[40] = {};
    std::snprintf(worldName, sizeof(worldName), "%s.rsw", g_session.m_curMap);
    g_modeMgr.Switch(1, worldName);
}

void HandleAcceptEnter2(CGameMode&, const PacketView& packet)
{
    // Ref: Zc_Accept_Enter2 (packet type 0x02EB)
    // Same core layout as 0x0073 (+ additional fields).
    if (!packet.data || packet.packetLength < 9) {
        return;
    }

    g_session.SetServerTime(ReadLE32(packet.data + 2));

    const int posX = (packet.data[7] >> 6) | (4 * packet.data[6]);
    const int posY = (packet.data[8] >> 4) | (16 * (packet.data[7] & 0x3F));
    const int dir  = packet.data[8] & 0x0F;
    g_session.SetPlayerPosDir(posX, posY, dir);

    char worldName[40] = {};
    std::snprintf(worldName, sizeof(worldName), "%s.rsw", g_session.m_curMap);
    g_modeMgr.Switch(1, worldName);
}

void HandleActorSpawnSkeleton(CGameMode& mode, const PacketView& packet)
{
    LogActorPacketSeenOnce(packet);
    if (mode.m_mapLoadingStage != CGameMode::MapLoading_None) {
        mode.m_lastActorBootstrapPacketTick = GetTickCount();
    }

    RuntimeActorState actorState;
    if (DecodeActorIdleOrSpawnPacket(packet, actorState)) {
        LogDecodedActorOnce(actorState, ShouldTreatActorAsPc(actorState.objectType, actorState.job));
        mode.m_aidList[actorState.gid] = GetTickCount();
        ApplyRuntimeActorState(mode, actorState);
        return;
    }

    if (packet.packetId == 0x0078 || packet.packetId == 0x0079 || packet.packetId == 0x007A || packet.packetId == 0x007C || packet.packetId == 0x01D8 || packet.packetId == 0x01D9) {
        LogActorPacketSample("legacy actor spawn fallback", packet);
    }

    // Skeleton parse for older actor-entry packets.
    if (!packet.data || packet.packetLength < 6) {
        return;
    }

    const u32 gid = ReadLE32(packet.data + 2);
    mode.m_lastPcGid = gid;
    mode.m_aidList[gid] = GetTickCount();

    if (packet.packetLength >= 12) {
        int sx = 0, sy = 0, dx = 0, dy = 0, sdir = 0, ddir = 0;
        DecodeSrcDst(packet.data + 6, sx, sy, dx, dy, sdir, ddir);
        mode.m_actorPosList[gid] = CellPos{dx, dy};
        UpdateRuntimeActorPosition(mode, gid, dx, dy, sx, sy);
    }
}

void HandleActorMoveSkeleton(CGameMode& mode, const PacketView& packet)
{
    LogActorPacketSeenOnce(packet);
    if (mode.m_mapLoadingStage != CGameMode::MapLoading_None) {
        mode.m_lastActorBootstrapPacketTick = GetTickCount();
    }

    RuntimeActorState actorState;
    if (DecodeActorWalkingPacket(packet, actorState)) {
        LogDecodedActorOnce(actorState, ShouldTreatActorAsPc(actorState.objectType, actorState.job));
        mode.m_aidList[actorState.gid] = GetTickCount();
        ApplyRuntimeActorState(mode, actorState);
        return;
    }

    if (packet.packetId == 0x007B || packet.packetId == 0x01DA) {
        LogActorPacketSample("legacy actor move fallback", packet);
    }

    if (!packet.data || packet.packetLength < 12) {
        return;
    }

    const u32 gid = ReadLE32(packet.data + 2);
    int sx = 0, sy = 0, dx = 0, dy = 0, sdir = 0, ddir = 0;
    DecodeSrcDst(packet.data + 6, sx, sy, dx, dy, sdir, ddir);

    mode.m_lastPcGid = gid;
    mode.m_aidList[gid] = GetTickCount();
    mode.m_actorPosList[gid] = CellPos{dx, dy};
    if (IsLocalPlayerActor(mode, gid)) {
        g_session.SetPlayerPosDir(dx, dy, ddir & 7);
        mode.m_attackChaseSourceCellX = dx;
        mode.m_attackChaseSourceCellY = dy;
    }
    UpdateRuntimeActorPosition(mode, gid, dx, dy, sx, sy);
}

void HandleActorMoveUpdate(CGameMode& mode, const PacketView& packet)
{
    LogActorPacketSeenOnce(packet);
    if (mode.m_mapLoadingStage != CGameMode::MapLoading_None) {
        mode.m_lastActorBootstrapPacketTick = GetTickCount();
    }

    // Ref switch case 134 (0x86): movement update with server time and packed src/dst.
    if (!packet.data || packet.packetLength < 12) {
        return;
    }

    const u32 gid = ReadLE32(packet.data + 2);
    int sx = 0, sy = 0, dx = 0, dy = 0, cellX = 0, cellY = 0;
    DecodePos2MoveData(packet.data + 6, sx, sy, dx, dy, cellX, cellY);

    const bool likelyPlayer = IsLikelyPlayerGid(gid);

    // eAthena uses 0x0086 for any visible walking object. Keep a billboard-capable
    // shell for move-only actors, but only seed player appearance when the id falls
    // inside the account-id range used for PCs.
    CGameActor* actor = EnsureRuntimeActor(mode, gid, true);
    static u32 s_loggedMoveUpdateCount = 0;
    if (s_loggedMoveUpdateCount < 24) {
        ++s_loggedMoveUpdateCount;
        DbgLog("[GameMode] move update gid=%u likelyPlayer=%d src=%d,%d dst=%d,%d dir=%d actor=%p pc=%d job=%d\n",
            gid,
            likelyPlayer ? 1 : 0,
            sx,
            sy,
            dx,
            dy,
            -1,
            static_cast<void*>(actor),
            actor && actor->m_isPc ? 1 : 0,
            actor ? actor->m_job : -1);
    }
    if (likelyPlayer && actor && actor->m_isPc == 0 && actor->m_job == 0) {
        SeedMoveOnlyRemotePcAppearance(actor);
    }
    if (actor) {
        if (CPc* pcActor = dynamic_cast<CPc*>(actor)) {
            pcActor->InvalidateBillboard();
        }
    }

    mode.m_lastPcGid = gid;
    mode.m_aidList[gid] = GetTickCount();
    mode.m_actorPosList[gid] = CellPos{dx, dy};

    if (IsLocalPlayerActor(mode, gid)) {
        g_session.SetPlayerPosDir(dx, dy, g_session.m_playerDir);
        mode.m_attackChaseSourceCellX = dx;
        mode.m_attackChaseSourceCellY = dy;
    }
    UpdateRuntimeActorPosition(mode, gid, dx, dy, sx, sy);
}

void HandleSelfMoveAck(CGameMode& mode, const PacketView& packet)
{
    // Classic eAthena self walk acknowledgment (0x0087): start tick + packed src/dst.
    if (!packet.data || packet.packetLength < 12 || g_session.m_gid == 0) {
        return;
    }

    g_session.SetServerTime(ReadLE32(packet.data + 2));

    int sx = 0, sy = 0, dx = 0, dy = 0, cellX = 0, cellY = 0;
    DecodePos2MoveData(packet.data + 6, sx, sy, dx, dy, cellX, cellY);

    mode.m_lastPcGid = g_session.m_gid;
    mode.m_aidList[g_session.m_gid] = GetTickCount();
    mode.m_actorPosList[g_session.m_gid] = CellPos{dx, dy};
    g_session.SetPlayerPosDir(dx, dy, g_session.m_playerDir);
    mode.m_attackChaseSourceCellX = dx;
    mode.m_attackChaseSourceCellY = dy;
    if (mode.m_world && mode.m_world->m_player) {
        mode.m_world->m_player->m_isWaitingMoveAck = 0;
    }
    UpdateRuntimeActorPosition(mode, g_session.m_gid, dx, dy, sx, sy);
}

void HandleActorSetPosition(CGameMode& mode, const PacketView& packet)
{
    LogActorPacketSeenOnce(packet);
    if (mode.m_mapLoadingStage != CGameMode::MapLoading_None) {
        mode.m_lastActorBootstrapPacketTick = GetTickCount();
    }

    // Ref switch case 136 (0x88): direct actor position update with short x/y.
    if (!packet.data || packet.packetLength < 10) {
        return;
    }

    const u32 gid = ReadLE32(packet.data + 2);
    const int x = static_cast<s16>(ReadLE16(packet.data + 6));
    const int y = static_cast<s16>(ReadLE16(packet.data + 8));

    CGameActor* actor = EnsureRuntimeActor(mode, gid);
    if (!actor) {
        return;
    }

    if (IsLocalPlayerActor(mode, gid)) {
        size_t fixIndex = 0;
        size_t activeIndex = 0;
        if (ShouldIgnoreRedundantLocalFixPosition(*actor, x, y, &fixIndex, &activeIndex)) {
            return;
        }
    }

    mode.m_lastPcGid = gid;
    mode.m_aidList[gid] = GetTickCount();
    mode.m_actorPosList[gid] = CellPos{x, y};
    if (IsLocalPlayerActor(mode, gid)) {
        g_session.SetPlayerPosDir(x, y, g_session.m_playerDir);
        mode.m_attackChaseSourceCellX = x;
        mode.m_attackChaseSourceCellY = y;
        if (mode.m_world && mode.m_world->m_player) {
            mode.m_world->m_player->m_isWaitingMoveAck = 0;
        }
    }
    ApplyRuntimeActorFixPosition(mode, gid, x, y);
}

void HandleActorVanish(CGameMode& mode, const PacketView& packet)
{
    LogActorPacketSeenOnce(packet);
    if (mode.m_mapLoadingStage != CGameMode::MapLoading_None) {
        mode.m_lastActorBootstrapPacketTick = GetTickCount();
    }

    // Ref switch case 128 (0x80): actor vanish/disappear.
    if (!packet.data || packet.packetLength < 7) {
        return;
    }

    const u32 gid = ReadLE32(packet.data + 2);
    const u8 reason = packet.data[6];
    DbgLog("[GameMode] vanish gid=%u reason=%u\n", gid, static_cast<unsigned int>(reason));
    mode.m_actorPosList.erase(gid);
    mode.m_aidList.erase(gid);

    if (reason == 0) {
        DbgLog("[GameMode] remove vanish gid=%u reason=0\n", gid);
        RemoveRuntimeActor(mode, gid);
    } else if (reason == 1) {
        if (CGameActor* actor = FindRuntimeActorForVanish(mode, gid)) {
            BeginActorDeath(mode, *actor, gid);
            if (IsLocalPlayerActor(mode, gid)) {
                mode.m_isOnQuest = 1;
                mode.m_isPlayerDead = 1;
            }

            DbgLog("[GameMode] death vanish gid=%u self=%d despawnAt=%u\n",
                gid,
                IsLocalPlayerActor(mode, gid) ? 1 : 0,
                static_cast<unsigned int>(actor->m_vanishTime));
        } else {
            RemoveRuntimeActor(mode, gid);
        }
    } else {
        RemoveRuntimeActor(mode, gid);
    }

    if (mode.m_lastPcGid == gid) {
        mode.m_lastPcGid = 0;
    }
    if (mode.m_lastMonGid == gid) {
        mode.m_lastMonGid = 0;
    }
    if (mode.m_lastLockOnMonGid == gid) {
        mode.m_lastLockOnMonGid = 0;
    }
}

} // namespace

void CleanupPendingActorDespawns(CGameMode& mode)
{
    const u32 now = GetTickCount();
    std::vector<u32> expiredActors;
    expiredActors.reserve(mode.m_runtimeActors.size());

    for (const auto& entry : mode.m_runtimeActors) {
        const u32 gid = entry.first;
        CGameActor* actor = entry.second;
        if (!actor) {
            continue;
        }

        if (actor->m_willBeDead == 1 && actor->m_vanishTime == 0) {
            const u32 deathFadeStart = actor->m_stateStartTick + kDeathCorpseHoldMs;
            if (now >= deathFadeStart) {
                actor->m_vanishTime = now + kDeathFadeDurationMs;
                actor->m_willBeDead = 2;
                if (CPc* pcActor = dynamic_cast<CPc*>(actor)) {
                    pcActor->InvalidateBillboard();
                }
                DbgLog("[GameMode] begin death fade gid=%u vanishAt=%u\n",
                    gid,
                    static_cast<unsigned int>(actor->m_vanishTime));
            }
            continue;
        }

        if (actor->m_vanishTime == 0 || actor->m_vanishTime > now) {
            continue;
        }

        expiredActors.push_back(gid);
    }

    for (u32 gid : expiredActors) {
        DbgLog("[GameMode] despawn expired dead actor gid=%u\n", gid);
        RemoveRuntimeActor(mode, gid);
    }
}

namespace {

void HandleActorFont(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 8) {
        return;
    }

    const u32 gid = ReadLE32(packet.data + 2);
    const u16 fontId = ReadLE16(packet.data + 6);

    mode.m_aidList[gid] = GetTickCount();
    if (CGameActor* actor = EnsureRuntimeActor(mode, gid)) {
        actor->m_charfont = fontId;
    }
}

void HandleActorSpriteChange(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 11) {
        return;
    }

    const u32 gid = ReadLE32(packet.data + 2);
    const u8 type = packet.data[6];
    const u32 value = ReadLE32(packet.data + 7);
    if (gid == 0) {
        return;
    }

    mode.m_aidList[gid] = GetTickCount();

    CGameActor* actor = EnsureRuntimeActor(mode, gid, IsLikelyPlayerGid(gid));
    if (!actor) {
        return;
    }

    bool billboardDirty = false;
    switch (type) {
    case kLookBase:
        actor->m_job = static_cast<int>(value);
        actor->m_isPc = ShouldTreatActorAsPc(actor->m_objectType, actor->m_job) ? 1 : 0;
        billboardDirty = true;
        break;
    case kLookHair:
        actor->m_headType = static_cast<int>(value);
        if (CPc* pcActor = dynamic_cast<CPc*>(actor)) {
            pcActor->m_head = static_cast<int>(value);
        }
        billboardDirty = true;
        break;
    case kLookWeapon:
        if (CPc* pcActor = dynamic_cast<CPc*>(actor)) {
            pcActor->m_weapon = static_cast<int>(value & 0xFFFFu);
            pcActor->m_shield = static_cast<int>((value >> 16) & 0xFFFFu);
        }
        billboardDirty = true;
        break;
    case kLookHeadBottom:
        if (CPc* pcActor = dynamic_cast<CPc*>(actor)) {
            pcActor->m_accessory = static_cast<int>(value);
        }
        billboardDirty = true;
        break;
    case kLookHeadTop:
        if (CPc* pcActor = dynamic_cast<CPc*>(actor)) {
            pcActor->m_accessory2 = static_cast<int>(value);
        }
        billboardDirty = true;
        break;
    case kLookHeadMid:
        if (CPc* pcActor = dynamic_cast<CPc*>(actor)) {
            pcActor->m_accessory3 = static_cast<int>(value);
        }
        billboardDirty = true;
        break;
    case kLookHairColor:
        if (CPc* pcActor = dynamic_cast<CPc*>(actor)) {
            pcActor->m_headPalette = static_cast<int>(value);
        }
        billboardDirty = true;
        break;
    case kLookClothesColor:
        actor->m_bodyPalette = static_cast<int>(value);
        billboardDirty = true;
        break;
    case kLookShield:
        if (CPc* pcActor = dynamic_cast<CPc*>(actor)) {
            pcActor->m_shield = static_cast<int>(value);
        }
        billboardDirty = true;
        break;
    case kLookRobe:
        billboardDirty = true;
        break;
    default:
        return;
    }

    if (billboardDirty) {
        if (gid == g_session.m_gid || gid == g_session.m_aid) {
            CHARACTER_INFO* info = g_session.GetMutableSelectedCharacterInfo();
            switch (type) {
            case kLookBase:
                g_session.m_playerJob = static_cast<int>(value);
                if (info) {
                    info->job = static_cast<s16>(value & 0xFFFFu);
                }
                break;
            case kLookHair:
                g_session.m_playerHead = static_cast<int>(value);
                if (info) {
                    info->head = static_cast<s16>(value & 0xFFFFu);
                }
                break;
            case kLookWeapon:
                g_session.m_playerWeapon = static_cast<int>(value & 0xFFFFu);
                g_session.m_playerShield = static_cast<int>((value >> 16) & 0xFFFFu);
                if (info) {
                    info->weapon = static_cast<s16>(g_session.m_playerWeapon & 0xFFFF);
                    info->shield = static_cast<s16>(g_session.m_playerShield & 0xFFFF);
                }
                break;
            case kLookHeadBottom:
                g_session.m_playerAccessory = static_cast<int>(value);
                if (info) {
                    info->accessory = static_cast<s16>(value & 0xFFFFu);
                }
                break;
            case kLookHeadTop:
                g_session.m_playerAccessory2 = static_cast<int>(value);
                if (info) {
                    info->accessory2 = static_cast<s16>(value & 0xFFFFu);
                }
                break;
            case kLookHeadMid:
                g_session.m_playerAccessory3 = static_cast<int>(value);
                if (info) {
                    info->accessory3 = static_cast<s16>(value & 0xFFFFu);
                }
                break;
            case kLookHairColor:
                g_session.m_playerHeadPalette = static_cast<int>(value);
                if (info) {
                    info->headpalette = static_cast<s16>(value & 0xFFFFu);
                }
                break;
            case kLookClothesColor:
                g_session.m_playerBodyPalette = static_cast<int>(value);
                if (info) {
                    info->bodypalette = static_cast<s16>(value & 0xFFFFu);
                }
                break;
            case kLookShield:
                g_session.m_playerShield = static_cast<int>(value);
                if (info) {
                    info->shield = static_cast<s16>(value & 0xFFFFu);
                }
                break;
            default:
                break;
            }
        }

        if (CPc* pcActor = dynamic_cast<CPc*>(actor)) {
            pcActor->InvalidateBillboard();
        }

        DbgLog("[GameMode] sprite change gid=%u type=%u value=%u job=%d pc=%d\n",
            gid,
            static_cast<unsigned int>(type),
            static_cast<unsigned int>(value),
            actor->m_job,
            actor->m_isPc);
    }
}

void HandleActorNameAck(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 30) {
        return;
    }

    const u32 gid = ReadLE32(packet.data + 2);
    std::string name = ExtractFixedPacketString(packet, 6, 24);
    if (gid == 0 || name.empty()) {
        return;
    }

    if (CGameActor* actor = EnsureRuntimeActor(mode, gid)) {
        if (!actor->m_isPc && actor->m_objectType == 6) {
            name = SanitizeNpcDisplayName(std::move(name));
        }
    }

    if (name.empty()) {
        return;
    }

    NamePair& entry = mode.m_actorNameListByGID[gid];
    entry.name = name;
    if (packet.packetId == 0x0195 && packet.packetLength >= 54) {
        entry.nick = ExtractFixedPacketString(packet, 30, 24);
    }
    mode.m_actorNameByGIDReqTimer.erase(gid);

    DbgLog("[GameMode] name ack gid=%u name='%s' nick='%s'\n",
        gid,
        entry.name.c_str(),
        entry.nick.c_str());
}

} // namespace

bool TryReadPacket(const u8* stream, int availableBytes, PacketView& outPacket, int& consumedBytes)
{
    consumedBytes = 0;
    outPacket = {};

    if (!stream || availableBytes < 2) {
        return false;
    }

    const u16 packetId = ReadLE16(stream);
    const s16 knownSize = ro::net::GetPacketSize(packetId);

    if (knownSize == 0) {
        return false;
    }

    int packetSize = knownSize;
    if (knownSize == ro::net::kVariablePacketSize) {
        if (availableBytes < 4) {
            return false;
        }
        packetSize = ReadLE16(stream + 2);
        if (packetSize < 4 || packetSize > 0x7FFF) {
            return false;
        }
    }

    if (availableBytes < packetSize) {
        return false;
    }

    outPacket.packetId = packetId;
    outPacket.packetLength = static_cast<u16>(packetSize);
    outPacket.data = stream;
    consumedBytes = packetSize;
    return true;
}

void DecodeSrcDst(const u8* src, int& srcX, int& srcY, int& dstX, int& dstY, int& srcDir, int& dstDir)
{
    if (!src) {
        srcX = srcY = dstX = dstY = srcDir = dstDir = 0;
        return;
    }

    // Movement packets pack coordinates/directions into 6 bytes.
    srcX   = (static_cast<int>(src[0]) << 2) | (src[1] >> 6);
    srcY   = ((src[1] & 0x3F) << 4) | (src[2] >> 4);
    dstX   = ((src[2] & 0x0F) << 6) | (src[3] >> 2);
    dstY   = ((src[3] & 0x03) << 8) | src[4];
    srcDir = (src[5] >> 4) & 0x0F;
    dstDir = src[5] & 0x0F;
}

void CGameModePacketRouter::Register(u16 packetId, Handler handler)
{
    if (!handler) {
        m_handlers.erase(packetId);
        return;
    }
    m_handlers[packetId] = std::move(handler);
}

bool CGameModePacketRouter::Dispatch(CGameMode& gameMode, const PacketView& packet) const
{
    const auto it = m_handlers.find(packet.packetId);
    if (it == m_handlers.end()) {
        return false;
    }

    it->second(gameMode, packet);
    return true;
}

void CGameModePacketRouter::Clear()
{
    m_handlers.clear();
}

void RegisterDefaultGameModePacketHandlers(CGameModePacketRouter& router)
{
    router.Clear();

    // Character enter/map-ready chain.
    router.Register(0x0073, HandleAcceptEnter);
    router.Register(0x007F, HandleNotifyTime);
    router.Register(0x02EB, HandleAcceptEnter2);

    // Login/session bootstrap.
    router.Register(0x0069, HandleLoginAccept);
    router.Register(0x006A, HandleLoginRefuse);

    // Map-change skeleton hooks.
    router.Register(0x0091, HandleMapChange);
    router.Register(0x0092, HandleMapChange);

    // Actor spawn/move skeleton hooks.
    router.Register(0x0078, HandleActorSpawnSkeleton);
    router.Register(0x0079, HandleActorSpawnSkeleton);
    router.Register(0x007A, HandleActorSpawnSkeleton);
    router.Register(0x007C, HandleActorSpawnSkeleton);
    router.Register(0x01D8, HandleActorSpawnSkeleton);
    router.Register(0x01D9, HandleActorSpawnSkeleton);
    router.Register(0x01DA, HandleActorMoveSkeleton);
    router.Register(0x007B, HandleActorMoveSkeleton);
    router.Register(0x0080, HandleActorVanish);
    router.Register(0x0086, HandleActorMoveUpdate);
    router.Register(0x0087, HandleSelfMoveAck);
    router.Register(0x008A, HandleActorActionNotify);
    router.Register(0x0088, HandleActorSetPosition);
    router.Register(0x01FF, HandleActorSetPosition); // ZC_HIGHJUMP: same id/x/y layout as 0x0088
    router.Register(0x009C, HandleActorDirection);
    router.Register(0x010E, HandlePlayerSkillUpdate);
    router.Register(0x010F, HandlePlayerSkillList);
    router.Register(0x0110, HandleSkillFailAck);
    router.Register(0x0111, HandlePlayerSkillAdd);
    router.Register(0x0235, HandleHomunSkillList);
    router.Register(0x0239, HandleHomunSkillUpdate);
    router.Register(0x029D, HandleMercSkillList);
    router.Register(0x029E, HandleMercSkillUpdate);
    router.Register(0x0114, HandleSkillDamageNotify);
    router.Register(0x0115, HandleSkillDamagePositionNotify);
    router.Register(0x0117, HandleGroundSkillNotify);
    router.Register(0x0119, HandleActorStateChange);
    router.Register(0x011A, HandleSkillNoDamageNotify);
    router.Register(0x0139, HandleAttackFailureForDistance);
    router.Register(0x013A, HandleAttackRange);
    router.Register(0x0229, HandleActorStateChange);

    // Chat/system text pathways.
    router.Register(0x008D, HandleNotifyChat);
    router.Register(0x008E, HandleNotifyPlayerChat);
    router.Register(0x0109, HandleNotifyChatParty);
    router.Register(0x0097, HandleWhisper);
    router.Register(0x0098, HandleAckWhisper);
    router.Register(0x009A, HandleBroadcast);
    router.Register(0x00B4, HandleNpcDialogText);
    router.Register(0x00B5, HandleNpcDialogNext);
    router.Register(0x00B6, HandleNpcDialogClose);
    router.Register(0x00B7, HandleNpcDialogMenu);
    router.Register(0x00C4, HandleNpcShopDealType);
    router.Register(0x00C6, HandleNpcShopBuyList);
    router.Register(0x00C7, HandleNpcShopSellList);
    router.Register(0x00CA, HandleNpcShopBuyResult);
    router.Register(0x00CB, HandleNpcShopSellResult);
    router.Register(0x0142, HandleNpcDialogNumberInput);
    router.Register(0x01D4, HandleNpcDialogStringInput);
    router.Register(0x01C3, HandleBroadcast);
    router.Register(0x02DC, HandleBattlefieldChat);
    router.Register(0x0095, HandleActorNameAck);
    router.Register(0x0195, HandleActorNameAck);

    router.Register(0x009D, HandleGroundItemEntry);
    router.Register(0x009E, HandleGroundItemEntry);
    router.Register(0x00A0, HandleItemPickupAck);
    router.Register(0x02D4, HandleItemPickupAck);
    router.Register(0x00A1, HandleGroundItemDisappear);
    router.Register(0x00A3, HandleNormalInventoryList);
    router.Register(0x00A4, HandleEquipInventoryList);
    router.Register(0x00AA, HandleEquipItemAck);
    router.Register(0x00AF, HandleItemRemove);
    router.Register(0x00AC, HandleUnequipItemAck);
    router.Register(0x01C8, HandleUseItemAck2);
    router.Register(0x01EE, HandleNormalInventoryList);
    router.Register(0x01EF, HandleEquipInventoryList);
    router.Register(0x01F8, HandleIgnorePacket);
    router.Register(0x02D0, HandleEquipInventoryList);
    router.Register(0x02E8, HandleNormalInventoryList);
    router.Register(0x07FA, HandleItemRemove);
    router.Register(0x0100, HandleIgnorePacket);

    // Quit/return-to-login transitions.
    router.Register(0x0081, HandleBanDisconnect);
    router.Register(0x00B3, HandleDisconnectCharacterAck);

    // Common server-side status/config packets that are safe to ignore for now,
    // but must stay in sync so later actor packets are framed correctly.
    router.Register(0x00B0, HandleSelfStatusParam);
    router.Register(0x00B1, HandleSelfStatusParam);
    router.Register(0x00BC, HandleStatusChangeAck);
    router.Register(0x00BD, HandleInitialStatusSummary);
    router.Register(0x00BE, HandleStatusPointCostUpdate);
    router.Register(0x00C0, HandleIgnorePacket);
    router.Register(0x013D, HandleRecovery);
    router.Register(0x013E, HandleSkillCastAck);
    router.Register(0x01B0, HandleIgnorePacket);
    router.Register(0x0106, HandleIgnorePacket);
    router.Register(0x0107, HandleIgnorePacket); // party minimap position (account id); framing only
    router.Register(0x0104, HandleIgnorePacket);
    router.Register(0x011F, HandleSkillUnitSet);
    router.Register(0x0120, HandleIgnorePacket);
    router.Register(0x0131, HandleIgnorePacket);
    router.Register(0x0132, HandleIgnorePacket);
    router.Register(0x0141, HandleSelfStatInfo);
    router.Register(0x0148, HandleActorResurrection);
    router.Register(0x019B, HandleNotifyEffect);
    router.Register(0x01B9, HandleSkillCastCancel);
    router.Register(0x0192, HandleIgnorePacket);
    router.Register(0x01C9, HandleSkillUnitSet);
    router.Register(0x01CF, HandleIgnorePacket);
    router.Register(0x01D0, HandleIgnorePacket);
    router.Register(0x01D7, HandleActorSpriteChange);
    router.Register(0x01DE, HandleSkillDamageNotify);
    router.Register(0x01E1, HandleIgnorePacket);
    router.Register(0x01F3, HandleNotifyEffect2);
    router.Register(0x0201, HandleIgnorePacket);
    router.Register(0x0209, HandleIgnorePacket);
    router.Register(0x0214, HandleStatusSummary);
    router.Register(0x02DD, HandleIgnorePacket);
    router.Register(0x0283, HandleIgnorePacket);
    router.Register(0x02C9, HandleIgnorePacket);
    router.Register(0x02B9, HandleShortcutKeyList);
    router.Register(0x02D1, HandleIgnorePacket);
    router.Register(0x02D2, HandleIgnorePacket);
    router.Register(0x02D3, HandleIgnorePacket);
    router.Register(0x02D5, HandleIgnorePacket);
    router.Register(0x02D7, HandleIgnorePacket);
    router.Register(0x02DA, HandleIgnorePacket);
    router.Register(0x02E1, HandleActorActionNotify);
    router.Register(0x043F, HandlePacket043F);
    router.Register(0x0814, HandleIgnorePacket);
    router.Register(0x0816, HandleIgnorePacket);

    // Newer actor entry packet family from Ref switch (v4 set).
    router.Register(0x022A, HandleActorSpawnSkeleton);
    router.Register(0x022B, HandleActorSpawnSkeleton);
    router.Register(0x022C, HandleActorMoveSkeleton);
    router.Register(0x02EC, HandleActorMoveSkeleton);
    router.Register(0x02ED, HandleActorSpawnSkeleton);
    router.Register(0x02EE, HandleActorSpawnSkeleton);
    router.Register(0x02EF, HandleActorFont);
    router.Register(0x07F7, HandleActorMoveSkeleton);
    router.Register(0x07F8, HandleActorSpawnSkeleton);
    router.Register(0x07F9, HandleActorSpawnSkeleton);
    router.Register(0x0856, HandleActorMoveSkeleton);
    router.Register(0x0857, HandleActorSpawnSkeleton);
    router.Register(0x0858, HandleActorSpawnSkeleton);
}
