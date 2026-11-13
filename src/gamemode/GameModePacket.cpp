#include "GameModePacket.h"
#include "ItemSnapshot.h"
#include "world/Corpse.h"
#include "world/GrannyCorpse.h"
#include "world/Gr2Monster.h"
#include "Commerce.h"
#include "LoginMode.h"

#include "network/Connection.h"
#include "network/GronPacket.h"
#include "network/MapSendProfile.h"
#include "network/Packet.h"
#include "GameMode.h"
#include "GuildCreation.h"
#include "PacketPadding.h"
#include "core/Globals.h"
#include "core/ClientInfoLocale.h"
#include "Types.h"
#include "core/File.h"
#include "pathfinder/PathFinder.h"
#include "session/Session.h"
#include "session/SkillActionInfo.h"
#include "session/SkillActionInfo2.h"
#include "skill/Skill.h"
#include "item/Item.h"
#include "ui/UIItemInfoWnd.h"
#include "ui/UIPetEggListWnd.h"
#include "ui/UIPetTamingDeceiveWnd.h"
#include "item/ItemUseFeedback.h"
#include "Mode.h"
#include "DebugLog.h"
#include "audio/Audio.h"
#include "lua/LuaBridge.h"
#include "ui/UIBasicInfoWnd.h"
#include "ui/UIChatRoomWnd.h"
#include "ui/UIChatRoomSettingsWnd.h"
#include "core/UiMessageStrings.h"
#include "core/SourceWeight.h"
#include "ui/UIJoinPartyAcceptWnd.h"
#include "ui/UISayDialogWnd.h"
#include "ui/UINpcMenuWnd.h"
#include "ui/UINpcInputWnd.h"

#include <cmath>
#include "ui/UIChooseSellBuyWnd.h"
#include "ui/UIEquipWnd.h"
#include "ui/UIItemCompositionWnd.h"
#include "ui/UIItemIdentifyWnd.h"
#include "ui/UISkillSelectWnd.h"
#include "ui/UIMonsterInfoWnd.h"
#include "ui/UIItemInfoWnd.h"
#include "ui/UIItemWnd.h"
#include "ui/UIStorageWnd.h"
#include "ui/UIItemPurchaseWnd.h"
#include "ui/UIItemSellWnd.h"
#include "ui/UIItemShopWnd.h"
#include "ui/UIShortCutWnd.h"
#include "ui/UIStatusWnd.h"
#include "ui/UIWindowMgr.h"
#include "ui/UICompanionStatusWnd.h"
#include "ui/UIPetInfoWnd.h"
#include "world/GameActor.h"
#include "world/RagEffect.h"
#include "world/RagEffect.h"
#include "world/World.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <initializer_list>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <zlib.h>

namespace {

void RecordChat(CGameMode& mode, const std::string& text, u32 color, u8 channel, ro::chat::NoticeSource source = {});

constexpr int kUiChatEventMsg = 101;
constexpr u32 kSystemNoticeColor = 0x0080C0FFu;
constexpr u32 kActorChatBubbleBaseLifetimeMs = 3200;
constexpr u32 kActorChatBubblePerCharLifetimeMs = 90;
constexpr u32 kActorChatBubbleMinLifetimeMs = 4500;
constexpr u32 kActorChatBubbleMaxLifetimeMs = 12000;
constexpr u32 kActorNameRequestCooldownMs = 1000;
constexpr u32 kDisconnectAckTimeoutMs = 15000;

PendingDisconnectAction g_pendingDisconnectAction = PendingDisconnectAction::None;
u32 g_pendingDisconnectStartTick = 0;
u32 g_lastLocalLevelUpEffectId = 0;
u32 g_lastLocalLevelUpEffectTick = 0;
u32 g_lastSelfLevelUpStatusTick = 0;
u32 g_lastSelfLevelUpStatusType = 0;
bool g_loggedPacketVer22NameRequestDefer = false;
bool g_localCartInfoKnown = false;
int g_localCartEffectStateMask = 0;

void RegisterHandlerIfValid(CGameModePacketRouter& router, u16 packetId, CGameModePacketRouter::Handler handler)
{
    if (packetId != 0) {
        router.Register(packetId, std::move(handler));
    }
}

void SendActorNameRequestIfNeeded(CGameMode& mode, u32 gid)
{
    if (gid == 0 || gid == g_session.m_gid) {
        return;
    }

    const ro::net::MapGameplaySendProfile& profile = ro::net::GetActiveMapGameplaySendProfile();
    if (!ro::net::IsLegacyMapGameplaySendProfile() && ro::net::GetActiveMapReceiveProfile().deferProactiveNameRequests) {
        if (!g_loggedPacketVer22NameRequestDefer) {
            DbgLog("[GameMode] deferring proactive actor name requests for packetver22 profile\n");
            g_loggedPacketVer22NameRequestDefer = true;
        }
        return;
    }

    const u32 now = GetTickCount();
    const auto timerIt = mode.m_actorNameByGIDReqTimer.find(gid);
    if (timerIt != mode.m_actorNameByGIDReqTimer.end()
        && now - timerIt->second < kActorNameRequestCooldownMs) {
        return;
    }

    bool sent = false;
    if (ro::net::IsLegacyMapGameplaySendProfile()) {
        PACKET_CZ_REQNAME_LEGACY packet{};
        packet.PacketType = profile.getCharNameRequest;
        packet.GID = gid;
        sent = CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(&packet), static_cast<int>(sizeof(packet)));
    } else {
        PACKET_CZ_REQNAME2 packet{};
        packet.PacketType = profile.getCharNameRequest;
        FillPacketPadding(packet.padding, static_cast<int>(sizeof(packet.padding)));
        packet.GID = gid;
        sent = CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(&packet), static_cast<int>(sizeof(packet)));
    }
    DbgLog("[GameMode] actor name request opcode=0x%04X gid=%u sent=%d\n",
        profile.getCharNameRequest,
        gid,
        sent ? 1 : 0);
    if (sent) {
        mode.m_actorNameByGIDReqTimer[gid] = now;
    }
}

void StartLocalPickupAnimation(CGameMode& mode, u32 objectAid);
bool EffectExists(CAbleToMakeEffect* actor, int effectId);

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
constexpr u32 kStatusCartInfo = 99;
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
constexpr int kEffectStateFalconMask = 0x0010;
constexpr int kEffectStatePushCartMask = 0x0008 | 0x0080 | 0x0100 | 0x0200 | 0x0400;
constexpr u16 kSkillIdHiding = 51;
constexpr u16 kSkillIdCloaking = 135;
constexpr u32 kPendingLocalHideSkillWindowMs = 1200;

struct PendingLocalHideSkillRequest {
    int mask = 0;
    bool expectedActive = false;
    u32 requestTick = 0;
};

PendingLocalHideSkillRequest g_pendingLocalHideSkillRequest;

int ResolveHideSkillEffectStateMask(u16 skillId)
{
    switch (skillId) {
    case kSkillIdHiding:
        // rAthena OPTION_HIDE is 0x02, which this client calls burrow.
        return kEffectStateBurrowMask;
    case kSkillIdCloaking:
        // rAthena OPTION_CLOAK is 0x04, which this client calls hiding.
        return kEffectStateHidingMask;
    default:
        return 0;
    }
}

bool IsLocalActorId(u32 actorId)
{
    if (actorId == 0) {
        return false;
    }
    if (actorId == g_session.m_gid || actorId == g_session.m_aid) {
        return true;
    }

    CGameMode* gameMode = g_modeMgr.GetCurrentGameMode();
    return gameMode && gameMode->m_world && gameMode->m_world->m_player && gameMode->m_world->m_player->m_gid == actorId;
}

bool IsPendingLocalHideSkillRequestActive(u32 now)
{
    return g_pendingLocalHideSkillRequest.mask != 0
        && g_pendingLocalHideSkillRequest.requestTick != 0
        && now - g_pendingLocalHideSkillRequest.requestTick <= kPendingLocalHideSkillWindowMs;
}

void RefreshActorEffectStatePresentation(CGameActor* actor, int oldEffectState);

void InvalidateCartUi()
{
    if (g_windowMgr.m_equipWnd) {
        g_windowMgr.m_equipWnd->Invalidate();
    }
}

void UpdateKnownLocalCartState(bool active, int effectState = 0)
{
    g_localCartInfoKnown = true;
    ro::commerce::state.hasCart = active;
    if (!active) {
        g_localCartEffectStateMask = 0;
        return;
    }

    const int resolvedMask = effectState & kEffectStatePushCartMask;
    if (resolvedMask != 0) {
        g_localCartEffectStateMask = resolvedMask;
    } else if (g_localCartEffectStateMask == 0) {
        g_localCartEffectStateMask = 0x0008;
    }
}

void ApplyLocalCartStateToActor(CGameActor* actor)
{
    if (!actor || !g_localCartInfoKnown) {
        return;
    }

    const int oldEffectState = actor->m_effectState;
    int newEffectState = oldEffectState;
    const bool localCartActive = (g_localCartEffectStateMask & kEffectStatePushCartMask) != 0;
    bool needsPresentationRefresh = false;
    if (localCartActive) {
        newEffectState &= ~kEffectStatePushCartMask;
        newEffectState |= g_localCartEffectStateMask;
        needsPresentationRefresh = actor->m_cartEffect == nullptr;
    } else {
        newEffectState &= ~kEffectStatePushCartMask;
        needsPresentationRefresh = actor->m_cartEffect != nullptr;
    }

    if (newEffectState == oldEffectState && !needsPresentationRefresh) {
        return;
    }

    actor->m_effectState = newEffectState;
    const int presentationOldEffectState = (newEffectState != oldEffectState || !needsPresentationRefresh)
        ? oldEffectState
        : (localCartActive
            ? (oldEffectState & ~kEffectStatePushCartMask)
            : (oldEffectState | kEffectStatePushCartMask));
    RefreshActorEffectStatePresentation(actor, presentationOldEffectState);
    InvalidateCartUi();
    DbgLog("[GameMode] local cart fallback active=%d mask=0x%X old=0x%X new=0x%X\n",
        localCartActive ? 1 : 0,
        g_localCartEffectStateMask,
        oldEffectState,
        newEffectState);
}

void ClearPendingLocalHideSkillRequest()
{
    g_pendingLocalHideSkillRequest = {};
}


bool HasEffectStateFlag(int effectState, int mask)
{
    return (effectState & mask) != 0;
}

bool IsEffectStatePushCart(int effectState)
{
    return (effectState & kEffectStatePushCartMask) != 0;
}

bool IsEffectStatePushCartChanged(int effectState, int oldEffectState)
{
    return ((effectState ^ oldEffectState) & kEffectStatePushCartMask) != 0;
}

bool IsBabyCartJob(int job)
{
    return job == 4028 || job == 4033 || job == 4041;
}

int ResolvePushCartEffectType(const CGameActor& actor)
{
    const int effectState = actor.m_effectState;
    if ((effectState & 0x0008) != 0) {
        if (actor.m_job == 23) {
            return 0x15;
        }
        if (actor.m_job == 4045) {
            return 0x1E;
        }
        return IsBabyCartJob(actor.m_job) ? 0x19 : 1;
    }
    if ((effectState & 0x0080) != 0) {
        return IsBabyCartJob(actor.m_job) ? 0x1A : 0x10;
    }
    if ((effectState & 0x0100) != 0) {
        return IsBabyCartJob(actor.m_job) ? 0x1B : 0x11;
    }
    if ((effectState & 0x0200) != 0) {
        return IsBabyCartJob(actor.m_job) ? 0x1C : 0x12;
    }
    if ((effectState & 0x0400) != 0) {
        return IsBabyCartJob(actor.m_job) ? 0x1D : 0x13;
    }
    return 0;
}

void RecordLocalHideSkillRequest(u16 skillId, u32 targetGid)
{
    const int mask = ResolveHideSkillEffectStateMask(skillId);
    if (mask == 0 || !IsLocalActorId(targetGid)) {
        return;
    }

    CGameMode* gameMode = g_modeMgr.GetCurrentGameMode();
    CGameActor* actor = gameMode && gameMode->m_world ? gameMode->m_world->m_player : nullptr;
    const bool currentlyActive = actor && (actor->m_effectState & mask) != 0;

    g_pendingLocalHideSkillRequest.mask = mask;
    g_pendingLocalHideSkillRequest.expectedActive = !currentlyActive;
    g_pendingLocalHideSkillRequest.requestTick = timeGetTime();
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
    const bool pushCartChanged = IsEffectStatePushCartChanged(effectState, oldEffectState);
    const bool hasPushCart = IsEffectStatePushCart(effectState);
    const bool bodyVisible = !(hiding || specialHiding || burrow);
    const bool fullyVisible = !(hiding || specialHiding);
    const bool suppressPushCart = hiding || specialHiding || burrow;
    const bool suppressFalcon = hiding || specialHiding || burrow;
    if (!actor->IsHideFadeActive(timeGetTime())) {
        actor->m_isVisibleBody = bodyVisible ? 1 : 0;
        actor->m_isVisible = fullyVisible ? 1 : 0;
        actor->m_shadowOn = bodyVisible ? 1 : 0;
    }

    // Ref SetAttrState only plays the sight wave. The orbit is effect 22,
    // relaunched by ProcessEffectState. Effect 601 is a separate skill visual.
    if ((pushCartChanged || suppressPushCart || !hasPushCart) && actor->m_cartEffect) {
        actor->SendMsg(actor, 52, 0, 0, 0);
    }

    if (hasPushCart && !suppressPushCart && !actor->m_cartEffect) {
        const int cartEffectType = ResolvePushCartEffectType(*actor);
        if (cartEffectType != 0) {
            actor->SendMsg(actor, 51, cartEffectType, 0, 0);
        }
    }

    // Ref CGameActor::SetAttrState sends messages101/102 when the Falcon
    // option changes. Runtime actor bootstrap applies state directly, so also
    // reconcile the attached Falcon when that path first creates the actor.
    const bool hasFalcon = HasEffectStateFlag(effectState, kEffectStateFalconMask);
    if ((!hasFalcon || suppressFalcon) && actor->m_birdEffect) {
        actor->SendMsg(actor, 102, 0, 0, 0);
    }
    if (hasFalcon && !suppressFalcon && !actor->m_birdEffect) {
        actor->SendMsg(actor, 101, 0, 0, 0);
    }

    const bool hadRuwach = HasEffectStateFlag(oldEffectState, kEffectStateRuwachMask);
    const bool hasRuwach = HasEffectStateFlag(effectState, kEffectStateRuwachMask);
    if (!hadRuwach && hasRuwach) {
        // Ref only plays the gain sound here. The orbit itself is the periodic effect-state relaunch of effect 33.
    }

    if (CPc* pcActor = dynamic_cast<CPc*>(actor)) {
        pcActor->InvalidateBillboard();
    }
}
constexpr u32 kNotifyEffectJobLevelUp = 1;
constexpr u32 kNotifyEffectBaseLevelUpSuperNovice = 7;
constexpr u32 kNotifyEffectJobLevelUpSuperNovice = 8;
constexpr u32 kNotifyEffectBaseLevelUpTaekwon = 9;
constexpr u32 kEffectIdRecoveryHp = 331;
constexpr u32 kEffectIdRecoverySp = 332;
constexpr int kRecoveryNumberKind = 114;
constexpr u32 kRecoveryHpNumberColor = 0xFF00FF00u;
constexpr u32 kRecoverySpNumberColor = 0xFF0000FFu;
constexpr u32 kEffectIdBaseLevelUp = 371;
constexpr u32 kEffectIdJobLevelUp = 158;
constexpr u32 kEffectIdBaseLevelUpSuperNovice = 338;
constexpr u32 kEffectIdJobLevelUpSuperNovice = 337;
constexpr u32 kEffectIdBaseLevelUpTaekwon = 582;
constexpr u32 kLocalLevelUpNotifySuppressMs = 1200;

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
void InvalidateStatusWindows();
void InvalidateBasicInfoWindow();


int ResolveRecoveryEffectId(u32 statusType)
{
    switch (statusType) {
    case kStatusHp:
        return static_cast<int>(kEffectIdRecoveryHp);
    case kStatusSp:
        return static_cast<int>(kEffectIdRecoverySp);
    default:
        return -1;
    }
}

u32 ResolveRecoveryNumberColor(u32 statusType)
{
    return statusType == kStatusSp ? kRecoverySpNumberColor : kRecoveryHpNumberColor;
}

void EmitRecoveryNumber(CGameActor* sourceActor, CGameActor* targetActor, int amount, u32 color)
{
    if (!targetActor) {
        return;
    }

    targetActor->SendMsg(sourceActor ? sourceActor : targetActor,
        88,
        static_cast<int>(amount),
        static_cast<int>(color),
        kRecoveryNumberKind);
}

std::string ToLowerAsciiStatus(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

void PlayBaseLevelUpPresentation(CGameMode& mode)
{
    // Ref/GameModePacket.cpp Zc_Par_Change (0x596E03).
    PlayWave("levelup.wav", 0.0f, 0.0f, 0.0f);

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

CGameActor* ResolveSkillPacketActor(CGameMode& mode, u32 aid)
{
    if (mode.m_world == nullptr) {
        return nullptr;
    }
    CGameActor* player = mode.m_world->m_player;
    if (aid == g_session.m_aid || aid == g_session.m_gid
        || (player != nullptr && aid == player->m_gid)) {
        return player;
    }
    return mode.m_world->GetGameActorByAID(aid);
}

void HandleNpcShowEfstUpdate(CGameMode& mode, const PacketView& packet)
{
    // Ref ZC_NPC_SHOWEFST_UPDATE (0x028A): id.L@2, effectState.L@6,
    // level.L@10, showEFST.L@14. OPTION remains owned by the attribute-state path.
    if (!packet.data || packet.packetLength < 18) {
        return;
    }

    const u32 actorId = ReadLE32(packet.data + 2);
    const int effectState = static_cast<int>(ReadLE32(packet.data + 6));
    const int level = static_cast<int>(ReadLE32(packet.data + 10));
    const u32 showEfst = ReadLE32(packet.data + 14);
    CGameActor* actor = ResolveSkillPacketActor(mode, actorId);
    if (!actor) {
        return;
    }

    if (level == 99 && !HasEffectStateFlag(effectState, kEffectStateSpecialHidingMask)) {
        actor->SendMsg(actor, 126, 0, 0, 0);
    }
    actor->SetShowEfst(showEfst);

}

CSkill* ResolveSkillPacketUnit(CGameMode& mode, u32 aid)
{
    return mode.m_world ? mode.m_world->GetSkillByAID(aid) : nullptr;
}

void HandleNotifyEffect(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 10) {
        return;
    }

    if (packet.packetId != ro::net::GetActiveMapReceiveProfile().notifyEffectBasic) {
        return;
    }

    const u32 actorId = ReadLE32(packet.data + 2);
    const u32 effectType = ReadLE32(packet.data + 6);
    CGameActor* actor = ResolveSkillPacketActor(mode, actorId);
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

    if (packet.packetId != ro::net::GetActiveMapReceiveProfile().notifyEffectDirect) {
        return;
    }

    const u32 actorId = ReadLE32(packet.data + 2);
    const u32 effectId = ReadLE32(packet.data + 6);
    CGameActor* actor = ResolveSkillPacketActor(mode, actorId);
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

void HandleSpirits(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 8 || packet.packetId != 0x01D0 || !g_session.m_isEffectOn) {
        return;
    }

    const u32 aid = ReadLE32(packet.data + 2);
    CGameActor* actor = ResolveSkillPacketActor(mode, aid);
    if (!actor) {
        return;
    }

    // Canonical GetJob transforms other job IDs, never 4038 or 24; these two
    // comparisons are therefore equivalent while all other jobs use the default.
    const int job = actor->m_job;
    const int effectId = job == 4038 ? 504 : (job == 24 ? 629 : 228);
    actor->SendMsg(nullptr, 109, effectId, 0, 0);

    if (aid == 0) {
        return;
    }

    const bool hidden = HasEffectStateFlag(actor->m_effectState, kEffectStateBurrowMask)
        || HasEffectStateFlag(actor->m_effectState, kEffectStateHidingMask);
    if (hidden) {
        actor->SendMsg(nullptr, 109, effectId, 0, 0);
        return;
    }

    CRagEffect* effect = actor->LaunchEffect(effectId, vector3d{ 0.0f, 0.0f, 0.0f }, 0.0f);
    const vector3d position{ static_cast<float>(static_cast<s16>(ReadLE16(packet.data + 6))), 0.0f, 0.0f };
    if (effect) {
        // The executable initializes only X before sending this vector. The
        // reconstructed client uses deterministic zeroes for the untouched axes.
        effect->SendMsg(nullptr, 14, reinterpret_cast<msgparam_t>(&position), 0, 0);
    }
}

void HandleSpirits2(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 8 || packet.packetId != 0x01E1 || !g_session.m_isEffectOn) {
        return;
    }

    const u32 aid = ReadLE32(packet.data + 2);
    CGameActor* actor = ResolveSkillPacketActor(mode, aid);
    if (!actor) {
        return;
    }

    // Canonical GetJob transforms other job IDs, never 4038 or 24; these two
    // comparisons are therefore equivalent while all other jobs use the default.
    const int job = actor->m_job;
    const int effectId = job == 4038 ? 504 : (job == 24 ? 629 : 228);
    const bool hidden = HasEffectStateFlag(actor->m_effectState, kEffectStateBurrowMask)
        || HasEffectStateFlag(actor->m_effectState, kEffectStateHidingMask);
    if (hidden) {
        actor->SendMsg(nullptr, 109, effectId, 0, 0);
        return;
    }

    if (!EffectExists(actor, effectId) && aid != 0) {
        vector3d position{};
        const vector3d offset{};
        CRagEffect* effect = actor->LaunchEffect(effectId, offset, 0.0f);
        position.x = static_cast<float>(static_cast<s16>(ReadLE16(packet.data + 6)));
        if (effect) {
            effect->SendMsg(nullptr, 14, reinterpret_cast<msgparam_t>(&position), 0, 0);
        }
    }
}

void HandleDevotionList(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 28 || packet.packetId != 0x01CF || !g_session.m_isEffectOn) {
        return;
    }

    const u32 devoterAid = ReadLE32(packet.data + 2);
    CGameActor* devoter = ResolveSkillPacketActor(mode, devoterAid);

    if (devoter) {
        for (int index = 0; index < 5; ++index) {
            devoter->SendMsg(nullptr, 109, 232, 0, 0);
        }
    }

    for (int index = 0; index < 5; ++index) {
        const u32 devoteeAid = ReadLE32(packet.data + 6 + index * 4);
        if (devoteeAid == 0) {
            continue;
        }

        const vector3d position{ static_cast<float>(devoterAid), static_cast<float>(devoteeAid), 0.0f };
        if (devoter) {
            const vector3d offset{};
            CRagEffect* effect = devoter->LaunchEffect(232, offset, 0.0f);
            if (effect) {
                effect->SendMsg(nullptr, 14, reinterpret_cast<msgparam_t>(&position), 0, 0);
            }
        }
    }
}

void InvalidateWindowTree(UIWindow* window)
{
    if (!window) {
        return;
    }
    window->Invalidate();
    for (UIWindow* child : window->m_children) {
        InvalidateWindowTree(child);
    }
}

void InvalidateBasicInfoWindow()
{
    InvalidateWindowTree(g_windowMgr.m_basicInfoWnd);
}

void AdvanceEffectStatusRandomState()
{
    // The canonical ChangeXorValue uses this number of rand() calls to rotate
    // encoded HP/SP slots. This client keeps those values directly on CPlayer,
    // so preserve the shared RNG sequence while using the direct representation.
    static bool initialized = false;
    if (!initialized) {
        initialized = true;
        for (int index = 0; index < 5; ++index) {
            std::rand();
        }
    }
    for (int index = 0; index < 5; ++index) {
        std::rand();
    }
    for (int index = 0; index < 12; ++index) {
        std::rand();
    }
}

void HandleNotifyEffect3(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 14 || !mode.m_world) {
        return;
    }

    const u32 actorId = ReadLE32(packet.data + 2);
    const u32 effectId = ReadLE32(packet.data + 6);
    const u32 value = ReadLE32(packet.data + 10);
    CGameActor* actor = ResolveSkillPacketActor(mode, actorId);
    if (!actor) {
        return;
    }

    if (actorId == g_session.m_aid && (effectId == 331 || effectId == 332)) {
        AdvanceEffectStatusRandomState();
        const u32 previousValue = effectId == 331 ? actor->m_Hp : actor->m_Sp;
        const u32 updatedValue = AddVital(previousValue, value);
        if (updatedValue != previousValue) {
            if (effectId == 331) {
                actor->m_Hp = updatedValue;
                g_session.UpdateCharacterVital(5, updatedValue);
            } else {
                actor->m_Sp = updatedValue;
                g_session.UpdateCharacterVital(7, updatedValue);
            }
            InvalidateBasicInfoWindow();
        }
    }

    u32 color = 0xFF0000FFu;
    switch (effectId) {
    case 331:
        color = 0xFF00FF00u;
        break;
    case 332:
        break;
    case 657:
        color = 0xFF00FF00u;
        break;
    case 658:
        break;
    case 659:
        color = 0xFFFF0000u;
        break;
    case 660:
        color = 0xFFFF00FFu;
        break;
    case 661:
        color = 0xFF000000u;
        break;
    case 662:
        color = 0xFFFFFFFFu;
        break;
    case 663:
        color = 0xFFFFFF00u;
        break;
    case 664:
        color = 0xFFFF55B1u;
        break;
    default:
        break;
    }

    actor->SendMsg(actor, 88, static_cast<s32>(value), static_cast<s32>(color), 114);
    actor->LaunchEffect(static_cast<int>(effectId), vector3d{0.0f, 0.0f, 0.0f}, 0.0f);
}

void HandleSkillCastCancel(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 6 || mode.m_world == nullptr) {
        return;
    }

    const u32 actorId = ReadLE32(packet.data + 2);
    CGameActor* actor = ResolveSkillPacketActor(mode, actorId);
    if (actor == nullptr) {
        return;
    }

    actor->SendMsg(actor, 84, 0, 0, 0);
    actor->SendMsg(actor, 87, 0, 0, 0);
    actor->SendMsg(actor, 83, 0, 0, 0);
    if (actor->m_stateId == kGameActorCastingStateId || actor->m_stateId == kGameActorCastingLoopStateId) {
        actor->m_isMotionFreezed = 0;
        actor->SetState(0);
    }
}

// Ref/GameModePacket.cpp:25976-26125 (0x5A85D0), clif.c:7976.
// Ref runtime-wave: 0x5A8845 0x5A88C7 (name bytes from packet + 2).
// TEST_BODY_BEGIN server_sound_packet_family
void HandleSound(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 35) {
        return;
    }
    const char* bytes = reinterpret_cast<const char*>(packet.data + 2);
    size_t length = 0;
    while (length < 24 && bytes[length] != '\0') {
        ++length;
    }
    const std::string name(bytes, length);
    const u8 action = packet.data[26];
    const u32 aid = ReadLE32(packet.data + 31);
    if (action == 0) {
        CGameActor* actor = ResolveSkillPacketActor(mode, aid);
        if (actor && mode.m_world->m_player) {
            const vector3d& player = mode.m_world->m_player->m_pos;
            // The reference sound packet uses player - actor, with y = 0.
            PlayWave(name.c_str(), player.x - actor->m_pos.x, 0.0f, player.z - actor->m_pos.z);
        } else {
            PlayWave(name.c_str(), 0.0f, 0.0f, 0.0f);
        }
    } else if (action == 1) {
        PLAY_WAVE_INFO info{};
        info.wavName = name;
        info.term = ReadLE32(packet.data + 27);
        info.nAID = aid;
        info.endTick = timeGetTime() + info.term;
        mode.m_playWaveList.push_back(info);
        PlayWave(name.c_str(), 0.0f, 0.0f, 0.0f);
    } else if (action == 2) {
        const auto found = std::find_if(mode.m_playWaveList.begin(), mode.m_playWaveList.end(),
            [&](const PLAY_WAVE_INFO& info) { return info.wavName == name; });
        if (found != mode.m_playWaveList.end()) {
            mode.m_playWaveList.erase(found);
        }
    }
}

// Ref/GameModePacket.cpp:10258-10273 (0x5A8900).
void HandleComboDelay(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 10) {
        return;
    }
    const u32 aid = ReadLE32(packet.data + 2);
    const u32 tick = ReadLE32(packet.data + 6);
    g_session.SetForceAnimEndTick(aid, tick);
    CGameActor* actor = ResolveSkillPacketActor(mode, aid);
    if (actor) {
        actor->SetForceAnimEndTick(tick);
    }
}

// Ref/GameModePacket.cpp:8808-8876 (0x5A8400).
void HandleBladeStop(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 14 || !mode.m_world) {
        return;
    }
    auto actorByAid = [&](u32 aid) -> CGameActor* { return ResolveSkillPacketActor(mode, aid); };
    CGameActor* source = actorByAid(ReadLE32(packet.data + 2));
    CGameActor* target = actorByAid(ReadLE32(packet.data + 6));
    if (ReadLE32(packet.data + 10)) {
        if (source && target) {
            source->SetForceAnimation(96, 4, 4, 96, 4, 1, 0x5F5E0FFu, 0);
            source->m_isSprArgbFixed = 1;
            source->SetArgb(-1, 64, 64, 64);
            target->m_isMotionFreezed = 1;
            target->m_isSprArgbFixed = 1;
            target->SetArgb(-1, 64, 64, 64);
        }
    } else {
        if (source) {
            source->SetForceAnimation(96, 4, 4, 96, 4, 1, 1u, 0);
            source->m_isSprArgbFixed = 0;
            source->SetArgb(-1, 255, 255, 255);
        }
        if (target) {
            target->m_isMotionFreezed = 0;
            target->m_isSprArgbFixed = 0;
            target->SetArgb(-1, 255, 255, 255);
        }
    }
}

// TEST_BODY_END server_sound_packet_family
void HandleIgnorePacket(CGameMode&, const PacketView&)
{
}

void ApplyActorStatusVisual(CGameMode& mode, int statusType, u32 actorId, int flag)
{
    if (!mode.m_world || actorId == 0) {
        return;
    }

    // Ref Zc_Msg_State_Change sends actor message 110 for every other actor.
    // That message is what starts Energy Coat's blue blur (effect 166). The
    // 2008 handler skipped the local aid, so the caster never saw the aura.
    CGameActor* actor = nullptr;
    if (actorId == g_session.m_aid || actorId == g_session.m_gid) {
        actor = mode.m_world->m_player;
    } else {
        actor = mode.m_world->GetGameActorByAID(actorId);
    }
    if (!actor) {
        return;
    }

    actor->SendMsg(actor, 110, statusType, flag, 0);
}

void ApplySelfStatusIconPacket(CGameMode& mode, int statusType, u32 actorId, bool active, u32 remainingMs, int flag)
{
    ApplyActorStatusVisual(mode, statusType, actorId, flag);

    const bool matchesLocalPlayer = actorId != 0
        && (actorId == g_session.m_gid || actorId == g_session.m_aid);
    if (!matchesLocalPlayer) {
        return;
    }

    g_session.SetActiveStatusIcon(statusType, active, remainingMs);
    if (statusType==34) {
        DbgLog("[Status] local Hallucination active=%d duration=%u ms (damage display values are supplied by the server)\n",
            active?1:0,remainingMs);
    }
}

void HandlePacket0196(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 9) {
        return;
    }

    const int statusType = static_cast<int>(ReadLE16(packet.data + 2));
    const u32 actorId = ReadLE32(packet.data + 4);
    const int flag = static_cast<int>(packet.data[8]);
    ApplySelfStatusIconPacket(mode, statusType, actorId, flag != 0, 0, flag);
}

void HandlePacket043F(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 25) {
        return;
    }

    const int statusType = static_cast<int>(ReadLE16(packet.data + 2));
    const u32 actorId = ReadLE32(packet.data + 4);
    const int flag = static_cast<int>(packet.data[8]);
    const u32 remainingMs = ReadLE32(packet.data + 9);
    ApplySelfStatusIconPacket(mode, statusType, actorId, flag != 0, remainingMs, flag);
}

void MakePotionEffect(CGameMode& mode, unsigned int itemId, u32 actorId)
{
    CGameActor* actor=ResolveSkillPacketActor(mode,actorId);
    if(!actor)return;
    const auto feedback=ro::item::GetUseFeedback(itemId);
    // Primary only announces EXP-item notices to the exact local AID with a player.
    if(actorId==g_session.m_aid&&mode.m_world&&mode.m_world->m_player) {
        const auto notice=g_ttemmgr.GetItemUseNotice(itemId);
        if(!notice.empty())g_windowMgr.PushSourceChatEvent(notice.c_str(),0x0000ffffu,6, ro::chat::NoticeRoute::Normal, ro::chat::NoticeType::Status);
    }
    if(feedback.sound)PlayWave(feedback.sound,0.0f,0.0f,0.0f);
    if(feedback.extraEffectId>0)actor->LaunchEffect(feedback.extraEffectId,vector3d{},0.0f);
    if(feedback.effectId<=0)return;
    // Message109 removes one matching owner entry; repeat until all old copies end.
    while(std::any_of(actor->m_effectList.begin(),actor->m_effectList.end(),[&](const CRagEffect* effect){
        return effect&&effect->GetEffectType()==feedback.effectId;
    }))actor->SendMsg(actor,109,feedback.effectId,0,0);
    if(!actor->LaunchEffect(feedback.effectId,vector3d{},0.0f))
        DbgLog("[GameMode] potion effect launch failed itemId=%u actor=%u effect=%d\n",itemId,actorId,feedback.effectId);
}

void ApplyInventoryRemainingAmount(unsigned int itemIndex,int remainingAmount)
{
    const ITEM_INFO* existing=g_session.GetInventoryItemByIndex(itemIndex);
    if (!existing) return;
    if (remainingAmount==0) {
        g_session.RemoveInventoryItem(itemIndex,0);
        return;
    }
    ITEM_INFO updated=*existing;
    updated.m_num=remainingAmount;
    g_session.SetInventoryItem(updated);
}

void HandleUseItemAck2(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 13) {
        return;
    }

    if (!ro::net::IsActiveExtendedUseItemAckPacket(packet.packetId)) {
        return;
    }

    const unsigned int itemIndex = static_cast<unsigned int>(ReadLE16(packet.data + 2));
    const unsigned int itemId = static_cast<unsigned int>(ReadLE16(packet.data + 4));
    const unsigned int actorId = ReadLE32(packet.data + 6);
    const int remainingAmount = static_cast<int>(ReadLE16(packet.data + 10));
    const bool ok = packet.data[12] != 0;

    if (IsLocalActorId(actorId)) {
        mode.m_waitingUseItemAck = 0;
        mode.m_lastCardItemIndex = ok && g_ttemmgr.IsCardItem(itemId) ? static_cast<int>(itemIndex) : 0;
        if (ok) ApplyInventoryRemainingAmount(itemIndex,remainingAmount);
    }

    if (ok) {
        MakePotionEffect(mode, itemId, actorId);
    }

    DbgLog("[GameMode] use item ack2 index=%u itemId=%u actor=%u amount=%d ok=%d self=%d\n",
        itemIndex,
        itemId,
        actorId,
        remainingAmount,
        ok ? 1 : 0,
        IsLocalActorId(actorId) ? 1 : 0);
}

void HandleUseItemAck(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 7) {
        return;
    }

    if (packet.packetId != ro::net::GetActiveMapReceiveProfile().useItemAckBasic) {
        return;
    }

    const unsigned int itemIndex = static_cast<unsigned int>(ReadLE16(packet.data + 2));
    const int remainingAmount = static_cast<int>(ReadLE16(packet.data + 4));
    const bool ok = packet.data[6] != 0;
    const ITEM_INFO* item = g_session.GetInventoryItemByIndex(itemIndex);

    mode.m_waitingUseItemAck = 0;
    mode.m_lastCardItemIndex = ok && item && g_ttemmgr.IsCardItem(item->GetItemId()) ? static_cast<int>(itemIndex) : 0;

    if (ok) {
        const bool recovery=item&&item->m_itemIndex!=0&&item->GetItemId()>500&&item->GetItemId()<600;
        ApplyInventoryRemainingAmount(itemIndex,remainingAmount);
        if(recovery&&mode.m_world&&mode.m_world->m_player)
            mode.m_world->m_player->SendMsg(mode.m_world->m_player,20,0,0,0);
    }

    DbgLog("[GameMode] use item ack index=%u amount=%d ok=%d\n",
        itemIndex,
        remainingAmount,
        ok ? 1 : 0);
}

void RefreshInventoryDependentWindows()
{
    if (g_windowMgr.m_itemInfoWnd) {
        g_windowMgr.m_itemInfoWnd->Invalidate();
    }
    if (g_windowMgr.m_itemWnd) {
        g_windowMgr.m_itemWnd->Invalidate();
    }
    if (g_windowMgr.m_equipWnd) {
        g_windowMgr.m_equipWnd->Invalidate();
    }
    if (g_windowMgr.m_shortCutWnd) {
        g_windowMgr.m_shortCutWnd->Invalidate();
    }
}

void ShowItemCompositionWindow(const std::vector<unsigned int>& itemIndices)
{
    UIItemCompositionWnd* const window = dynamic_cast<UIItemCompositionWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_ITEMCOMPOSITIONWND));
    if (!window) {
        return;
    }

    unsigned int cardItemIndex = 0;
    if (const CGameMode* gameMode = g_modeMgr.GetCurrentGameMode()) {
        cardItemIndex = static_cast<unsigned int>((std::max)(0, gameMode->m_lastCardItemIndex));
    }
    window->SetCardItemIndex(cardItemIndex);
    window->SetEquipmentItemIndices(itemIndices);
}

void ShowIdentifyWindow(const std::vector<unsigned int>& itemIndices)
{
    UIItemIdentifyWnd* const window = dynamic_cast<UIItemIdentifyWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_ITEMIDENTIFYWND));
    if (!window) {
        return;
    }
    window->SetIdentifyItemIndices(itemIndices);
}

void HandleItemIdentifyList(CGameMode&, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 4 || ((packet.packetLength - 4) % 2) != 0) {
        return;
    }

    std::vector<unsigned int> itemIndices;
    const int count = (packet.packetLength - 4) / 2;
    itemIndices.reserve(static_cast<size_t>(count));
    for (int index = 0; index < count; ++index) {
        const unsigned int itemIndex = static_cast<unsigned int>(ReadLE16(packet.data + 4 + index * 2));
        if (itemIndex == 0) {
            continue;
        }
        itemIndices.push_back(itemIndex);
    }

    ShowIdentifyWindow(itemIndices);
    DbgLog("[GameMode] item identify list count=%d\n", static_cast<int>(itemIndices.size()));
}

void HandleItemIdentifyAck(CGameMode&, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 5) {
        return;
    }

    const unsigned int itemIndex = static_cast<unsigned int>(ReadLE16(packet.data + 2));
    const unsigned int result = static_cast<unsigned int>(packet.data[4]);
    if (result == 0) {
        if (const ITEM_INFO* existing = g_session.GetInventoryItemByIndex(itemIndex)) {
            ITEM_INFO updated = *existing;
            updated.m_isIdentified = 1;
            g_session.SetInventoryItem(updated);
        }
        RefreshInventoryDependentWindows();
        g_windowMgr.PushChatEvent("Item has been identified.", kSystemNoticeColor, 6);
    } else {
        g_windowMgr.PushChatEvent("Item identification failed.", 0x00FF4040u, 6);
    }

    DbgLog("[GameMode] item identify ack index=%u result=%u\n", itemIndex, result);
}

void HandleItemCompositionList(CGameMode&, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 4 || ((packet.packetLength - 4) % 2) != 0) {
        return;
    }

    std::vector<unsigned int> itemIndices;
    const int count = (packet.packetLength - 4) / 2;
    itemIndices.reserve(static_cast<size_t>(count));
    for (int index = 0; index < count; ++index) {
        const unsigned int itemIndex = static_cast<unsigned int>(ReadLE16(packet.data + 4 + index * 2));
        if (itemIndex == 0) {
            continue;
        }
        itemIndices.push_back(itemIndex);
    }

    ShowItemCompositionWindow(itemIndices);
    DbgLog("[GameMode] item composition list count=%d cardIndex=%d\n",
        static_cast<int>(itemIndices.size()),
        g_modeMgr.GetCurrentGameMode() ? g_modeMgr.GetCurrentGameMode()->m_lastCardItemIndex : 0);
}

void HandleItemCompositionAck(CGameMode&, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 7) {
        return;
    }

    const unsigned int equipItemIndex = static_cast<unsigned int>(ReadLE16(packet.data + 2));
    const unsigned int cardItemIndex = static_cast<unsigned int>(ReadLE16(packet.data + 4));
    const unsigned int result = static_cast<unsigned int>(packet.data[6]);

    if (CGameMode* gameMode = g_modeMgr.GetCurrentGameMode()) {
        gameMode->m_lastCardItemIndex = 0;
    }

    if (result == 0) {
        const ITEM_INFO* cardItem = g_session.GetInventoryItemByIndex(cardItemIndex);
        const ITEM_INFO* equipItem = g_session.GetInventoryItemByIndex(equipItemIndex);
        if (cardItem && equipItem && cardItem->m_itemIndex != 0 && equipItem->m_itemIndex != 0) {
            ITEM_INFO updatedEquipItem = *equipItem;
            for (int slotIndex = 0; slotIndex < 4; ++slotIndex) {
                if (updatedEquipItem.m_slot[slotIndex] == 0) {
                    updatedEquipItem.m_slot[slotIndex] = static_cast<int>(cardItem->GetItemId());
                    break;
                }
            }
            g_session.RemoveInventoryItem(cardItemIndex, 1);
            g_session.SetInventoryItem(updatedEquipItem);
        }
        RefreshInventoryDependentWindows();
        g_windowMgr.PushChatEvent("Item compounding succeeded.", kSystemNoticeColor, 6);
    } else if (result == 1) {
        g_windowMgr.PushChatEvent("Item compounding failed.", 0x00FF4040u, 6);
    }

    DbgLog("[GameMode] item composition ack equipIndex=%u cardIndex=%u result=%u\n",
        equipItemIndex,
        cardItemIndex,
        result);
}

void HandleRecovery(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 6) return;
    const u32 statusType=ReadLE16(packet.data+2);
    const int effectId=ResolveRecoveryEffectId(statusType);
    if(effectId<0)return;

    // Selected PACKET_ZC_RECOVERY.amount is int16. Both original PE branches
    // use movswl and an unchecked 32-bit add, not a maximum-HP clamp.
    const u16 rawAmount=ReadLE16(packet.data+4);
    const int amount=rawAmount<=0x7fff ? int(rawAmount) : int(rawAmount)-0x10000;
    AdvanceEffectStatusRandomState();
    const auto& vitals=g_session.GetCharacterVitals();
    const u32 previousValue=statusType==kStatusHp ? vitals.hp : vitals.sp;
    const u32 updatedValue=previousValue+static_cast<u32>(amount);
    ApplySelfStatusUpdate(mode,statusType,updatedValue);
    if(updatedValue!=previousValue)InvalidateBasicInfoWindow();

    if(mode.m_world&&mode.m_world->m_player) {
        CGameActor* actor=mode.m_world->m_player;
        // Zero and negative deltas still send the source number message. The
        // existing Am_Make_Number path clamps a negative display value to zero.
        EmitRecoveryNumber(actor,actor,amount,ResolveRecoveryNumberColor(statusType));
        if(!actor->LaunchEffect(effectId,vector3d{},0.0f))
            DbgLog("[GameMode] recovery effect launch failed status=%u amount=%d\n",unsigned(statusType),amount);
    }
    DbgLog("[GameMode] recovery status=%u amount=%d\n",unsigned(statusType),amount);
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

u16 ClampPrimaryStatTotal(s32 base, s32 bonus)
{
    const auto total = static_cast<std::int64_t>(base) + bonus;
    return static_cast<u16>(std::clamp<std::int64_t>(total, 0, std::numeric_limits<u16>::max()));
}

struct PrimaryStatFields {
    u8 CHARACTER_INFO::* cardBase;
    int CSession::* bonus;
    u16 CGameActor::* actorTotal;
};

constexpr std::array<PrimaryStatFields, 6> kPrimaryStatFields{{
    {&CHARACTER_INFO::Str, &CSession::m_plusStr, &CGameActor::m_Str},
    {&CHARACTER_INFO::Agi, &CSession::m_plusAgi, &CGameActor::m_Agi},
    {&CHARACTER_INFO::Vit, &CSession::m_plusVit, &CGameActor::m_Vit},
    {&CHARACTER_INFO::Int, &CSession::m_plusInt, &CGameActor::m_Int},
    {&CHARACTER_INFO::Dex, &CSession::m_plusDex, &CGameActor::m_Dex},
    {&CHARACTER_INFO::Luk, &CSession::m_plusLuk, &CGameActor::m_Luk},
}};

const PrimaryStatFields* FindPrimaryStat(u32 statusType)
{
    if (statusType < kStatusStr || statusType > kStatusLuk) return nullptr;
    return &kPrimaryStatFields[statusType - kStatusStr];
}

void ApplyPrimaryStatBase(CGameMode& mode, const PrimaryStatFields& fields, s32 base)
{
    if (auto* character = g_session.GetMutableSelectedCharacterInfo()) {
        character->*fields.cardBase = static_cast<u8>(std::clamp<s32>(base, 0, 0xff));
    }
    if (auto* player = mode.m_world ? mode.m_world->m_player : nullptr) {
        player->*fields.actorTotal = ClampPrimaryStatTotal(base, g_session.*fields.bonus);
    }
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

    for (const auto& fields : kPrimaryStatFields) {
        player->*fields.actorTotal = ClampPrimaryStatTotal(info->*fields.cardBase, g_session.*fields.bonus);
    }
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
    g_session.SetStatusPoints(statusPoint);
    if (CHARACTER_INFO* info = g_session.GetMutableSelectedCharacterInfo()) {
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

bool ApplySelfStatusUpdateToSession(u32 statusType, u32 value)
{
    const bool updatedVital = g_session.UpdateCharacterVital(statusType, value);
    if (ApplyCombatStatusParamToSession(statusType, static_cast<int>(value), true)) {
        return true;
    }

    CHARACTER_INFO* info = g_session.GetMutableSelectedCharacterInfo();
    switch (statusType) {
    case kStatusBaseExp:
        g_session.SetBaseExpValue(value);
        return true;
    case kStatusJobExp:
        g_session.SetJobExpValue(value);
        return true;
    case kStatusNextBaseExp:
        g_session.SetNextBaseExpValue(value);
        return true;
    case kStatusNextJobExp:
        g_session.SetNextJobExpValue(value);
        return true;
    case kStatusStatusPoint:
        g_session.SetStatusPoints(static_cast<s32>(value));
        return true;
    default:
        break;
    }

    if (!info) {
        return updatedVital;
    }

    switch (statusType) {
    case kStatusHp:
        info->hp = static_cast<s16>((std::min)(value, 32767u));
        return true;
    case kStatusMaxHp:
        info->maxhp = static_cast<s16>((std::min)(value, 32767u));
        return true;
    case kStatusSp:
        info->sp = static_cast<s16>((std::min)(value, 32767u));
        return true;
    case kStatusMaxSp:
        info->maxsp = static_cast<s16>((std::min)(value, 32767u));
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
    // clif_item_normal sends item_data::equip here (allowed slot), not the
    // item's actual equipped state. Ref tracks equipped ammo via 0x013C.
    outItem.m_location = ReadLE16(entry + 8);
    outItem.m_wearLocation = 0;
    // Stackable ammunition still has an equipment destination (EQP_AMMO).
    if (outItem.m_itemType == 10 || outItem.m_itemType == 16 || outItem.m_itemType == 17) {
        outItem.m_location = 0x8000;
    }

    const int entrySizeValue = ro::net::GetActiveStackableItemListEntrySize(packet.packetId);
    if (entrySizeValue <= 0) {
        return false;
    }
    const size_t entrySize = static_cast<size_t>(entrySizeValue);

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

    const int entrySizeValue = ro::net::GetActiveEquipmentItemListEntrySize(packet.packetId);
    if (entrySizeValue <= 0) {
        return false;
    }
    const size_t entrySize = static_cast<size_t>(entrySizeValue);

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

// Ref Zc_autorun_skill stores a temporary row before invoking mode126.
void HandleAutoRunSkill(CGameMode& mode, const PacketView& packet)
{
    if(!packet.data || packet.packetId!=0x0147 || packet.packetLength!=39 || packet.data[0]!=0x47 || packet.data[1]!=1)return;
    PLAYER_SKILL_INFO skill{};skill.m_isValid=1;skill.SKID=ReadLE16(packet.data+2);
    ApplySkillMetadata(skill);
    skill.type=static_cast<int>(ReadLE32(packet.data+4));
    skill.level=static_cast<s16>(ReadLE16(packet.data+8));skill.spcost=static_cast<s16>(ReadLE16(packet.data+10));
    skill.attackRange=static_cast<s16>(ReadLE16(packet.data+12));skill.upgradable=packet.data[38];
    skill.skillName=skill.skillIdName;
    g_session.AddTemporarySkillItem(skill);
    mode.SendMsg(126,reinterpret_cast<msgparam_t>(&skill),skill.level,0);
}

// Matches Ref Zc_Skillinfo_List filtering for ZC_SKILLINFO_LIST (0x010F) player skills.
static bool IsPlayerSkillTreeSkid(int skid)
{
    // Selected rAthena grants WE_CHEERUP to adopted characters and includes it
    // in the next full skill snapshot.
    return (skid > 0 && skid < 0x40F) || skid == 5065;
}

static bool IsHomunSkillTreeSkid(int skid)
{
    return skid >= 0x1F40 && skid < 0x1F51;
}

static bool IsMercSkillTreeSkid(int skid)
{
    return SKILL_INFO::IsMercenarySkill(skid);
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

    constexpr size_t kSkillInfoListEntrySize = 37;
    const size_t payloadSize = static_cast<size_t>(packet.packetLength - 4);
    if (payloadSize % kSkillInfoListEntrySize != 0) {
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

    for (size_t offset = 4; offset + kSkillInfoListEntrySize <= static_cast<size_t>(packet.packetLength); offset += kSkillInfoListEntrySize) {
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
        DbgLog("[GameMode] skill list entry skid=%d type=%d level=%d sp=%d range=%d upgradable=%d\n",
            skillInfo.SKID,
            skillInfo.type,
            skillInfo.level,
            skillInfo.spcost,
            skillInfo.attackRange,
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

std::string ExtractFixedPacketString(const PacketView& packet, size_t offset, size_t fieldSize);

int ReadCompanionI32(const u8* data)
{
    const u32 bits = ReadLE32(data);
    s32 value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

void ReadCompanionCombat(CompanionCombatStatus& status, const u8* data)
{
    status.attack = static_cast<s16>(ReadLE16(data));
    status.magicAttack = static_cast<s16>(ReadLE16(data + 2));
    status.hit = static_cast<s16>(ReadLE16(data + 4));
    status.critical = static_cast<s16>(ReadLE16(data + 6));
    status.defense = static_cast<s16>(ReadLE16(data + 8));
    status.magicDefense = static_cast<s16>(ReadLE16(data + 10));
    status.flee = static_cast<s16>(ReadLE16(data + 12));
    status.attackSpeed = static_cast<s16>(ReadLE16(data + 14));
}

void CacheOwnedCompanionActorName(CGameMode& mode, u32 gid, const std::string& name)
{
    if (gid == 0 || name.empty()) {
        return;
    }
    mode.m_actorNameListByGID[gid].name = name;
}

void HandleHomunProperty(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength != 71) return;
    auto& status = g_session.m_homunStatus;
    const auto* data = packet.data;
    const auto name = ExtractFixedPacketString(packet, 2, 24);
    // Ref only uses bit0 of the flags byte as the rename lock.
    const u8 renameFlag = static_cast<u8>(data[26] & 0x1);
    // Selected server flags report rest at bit 1 and positive HP at bit 2.
    if ((data[26] & 0x2) != 0 || (data[26] & 0x4) == 0) {
        mode.DeactivateHomunculus();
    }
    const bool nameChanged = status.name != name || status.nameModified != renameFlag;
    status.name = name;
    status.nameModified = renameFlag;
    CacheOwnedCompanionActorName(mode, g_session.m_homunGID, status.name);
    if (nameChanged && g_windowMgr.m_homunInfoWnd) g_windowMgr.m_homunInfoWnd->RefreshReceivedName();
    status.level = static_cast<s16>(ReadLE16(data + 27));
    status.hunger = static_cast<s16>(ReadLE16(data + 29));
    status.intimacy = static_cast<s16>(ReadLE16(data + 31));
    status.accessory = ReadLE16(data + 33);
    ReadCompanionCombat(status.combat, data + 35);
    status.combat.hp = static_cast<s16>(ReadLE16(data + 51));
    status.combat.maxHp = static_cast<s16>(ReadLE16(data + 53));
    status.combat.sp = static_cast<s16>(ReadLE16(data + 55));
    status.combat.maxSp = static_cast<s16>(ReadLE16(data + 57));
    status.experience = ReadCompanionI32(data + 59);
    status.nextExperience = ReadCompanionI32(data + 63);
    status.skillPoints = static_cast<s16>(ReadLE16(data + 67));
    status.combat.attackRange = static_cast<s16>(ReadLE16(data + 69));
    if (g_windowMgr.m_homunInfoWnd) {
        g_windowMgr.m_homunInfoWnd->Invalidate();
    }
    const int hunger = status.hunger;
    if ((hunger == 1 || hunger == 5 || hunger == 10) && hunger != status.lastWarnedHunger) {
        ro::UiMessageStrings strings;
        strings.Load();
        const auto message = strings.Get(1018, "Your homunculus is hungry.");
        g_windowMgr.SendMsg(1, reinterpret_cast<msgparam_t>(message.c_str()), 0xff);
        status.lastWarnedHunger = hunger;
    }
}

void ShowMissingPetFood(u16 foodId)
{
    ITEM_INFO food{};
    food.SetItemId(foodId);
    ro::UiMessageStrings strings;
    strings.Load();
    std::string message = strings.Get(591, "You have no '%s'.");
    if (const auto argument = message.find("%s"); argument != std::string::npos)
        message.replace(argument, 2, food.GetItemName());
    constexpr u32 color = 0x1e1ef5;
    g_windowMgr.SendMsg(1, reinterpret_cast<msgparam_t>(message.c_str()), color);
    g_windowMgr.SendMsg(20, reinterpret_cast<msgparam_t>(message.c_str()), color);
}

void HandleHomunFeedResult(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength != 5) return;
    if (packet.data[2] != 0) {
        g_session.m_homunStatus.stateId = 29;
        return;
    }

    ShowMissingPetFood(ReadLE16(packet.data + 3));
    if (auto* actor = mode.m_world ? mode.m_world->GetGameActorByAID(g_session.m_homunGID) : nullptr)
        actor->SendMsg(nullptr, 21, 4, 0, 0);
}

void HandleHomunStatus(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength != 12) return;
    const auto gid = ReadLE32(packet.data + 4);
    const auto* actor = mode.m_world && gid ? mode.m_world->GetGameActorByAID(gid) : nullptr;
    auto& status = g_session.m_homunStatus;
    switch (packet.data[3]) {
    case 0:
        if (gid != g_session.m_homunGID) {
            mode.DeactivateHomunculus();
            status.job = -1;
        }
        g_session.m_homunGID = gid;
        if (actor) status.alive = true;
        CacheOwnedCompanionActorName(mode, gid, status.name);
        break;
    case 1: status.intimacy = ReadCompanionI32(packet.data + 8); break;
    case 2: status.hunger = ReadCompanionI32(packet.data + 8); break;
    case 3:
        if (actor) status.accessory = ReadLE32(packet.data + 8);
        return;
    default: return;
    }
    if (actor) status.job = actor->m_job;
}

void HandleNotifyMapProperty(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength != 4) return;
    const int property = static_cast<s16>(ReadLE16(packet.data + 2));
    g_session.m_isMinEffect = g_session.m_isMinEffectOld;
    if (mode.m_world) {
        if (property == 1 || property == 5) mode.m_world->m_isPKZone = 1;
        else if (property == 2) mode.m_world->m_isEventPVPMode = 1;
        else if (property == 3) {
            mode.m_world->m_isSiegeMode = 1;
            g_session.m_isMinEffect = true;
        }
    }
    if (property == 4) {
        ro::UiMessageStrings strings;
        strings.Load();
        const auto text = strings.Get(855, "This is a PK Zone.");
        g_windowMgr.SendMsg(1, reinterpret_cast<msgparam_t>(text.c_str()), 0x9b9bff);
    }
    if (g_serviceType != ServiceKorea) g_session.m_mapTypeInfo.Clear();
}

void HandleNotifyMapProperty2(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength != 4) return;
    const int type = static_cast<s16>(ReadLE16(packet.data + 2));
    g_session.m_curMapType = type;
    g_session.m_isMinEffect = g_session.m_isMinEffectOld;
    g_session.m_mapTypeInfo.EnsureLoaded();
    if (!mode.m_world) return;

    if (type == 5 || type == 6) {
        mode.m_world->m_isPKZone = 1;
    } else if (type != 11 && type != 12) {
        if (type == 8 || type == 18 || g_session.m_mapTypeInfo.Enabled(type, ro::SourceMapTypeTable::Siege)) {
            mode.m_world->m_isSiegeMode = 1;
            g_session.m_isMinEffect = true;
        } else if (type == 19) {
            mode.m_world->m_isBattleFieldMode = 1;
            mode.m_world->m_isPKZone = 1;
        }
    }
}

void HandleMapPropertyRowUpdate(CGameMode&, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 6 || ReadLE16(packet.data + 2) != packet.packetLength) return;
    const int type = static_cast<s16>(ReadLE16(packet.data + 4));
    g_session.m_mapTypeInfo.ApplyRowUpdate(type, std::string_view(
        reinterpret_cast<const char*>(packet.data + 6), static_cast<std::size_t>(packet.packetLength - 6)));
}

void ApplyEquipmentVisibility(bool visible)
{
    g_session.m_showEquipmentToOthers = visible;
    ro::UiMessageStrings strings;
    strings.Load();
    const auto notice = strings.Get(visible ? 1359 : 1358, visible
        ? "Open your equipment information to the public."
        : "Do not open your equipment information to the public.");
    g_windowMgr.SendMsg(1, reinterpret_cast<msgparam_t>(notice.c_str()), 6619135);
    if (g_windowMgr.m_equipWnd) g_windowMgr.m_equipWnd->Invalidate();
}

void HandlePrivacyConfiguration(CGameMode&, const PacketView& packet)
{
    if (!packet.data || packet.packetLength != 10) return;
    const u32 type = ReadLE32(packet.data + 2);
    const u32 enabled = ReadLE32(packet.data + 6);
    if (enabled > 1) return;
    if (type == 0) ApplyEquipmentVisibility(enabled != 0);
    if (type == 1) {
        g_session.m_refuseRecall = enabled != 0;
        const char* notice = enabled ? "Recall requests are blocked." : "Recall requests are allowed.";
        g_windowMgr.SendMsg(1, reinterpret_cast<msgparam_t>(notice), 6619135);
    }
}

void HandleEquipmentVisibility(CGameMode&, const PacketView& packet)
{
    if (packet.data && packet.packetLength == 3 && packet.data[2] <= 1) {
        ApplyEquipmentVisibility(packet.data[2] != 0);
    }
}

void HandlePartyInvitePrivacy(CGameMode&, const PacketView& packet)
{
    if (!packet.data || packet.packetLength != 3 || packet.data[2] > 1) return;
    const bool refuse = packet.data[2] != 0;
    g_session.m_refusePartyInvites = refuse;
    ro::UiMessageStrings strings;
    strings.Load();
    const auto notice = strings.Get(refuse ? 1325 : 1326,
        refuse ? "Block all party invitations." : "Allow all party invitations.");
    g_windowMgr.SendMsg(1, reinterpret_cast<msgparam_t>(notice.c_str()), 6619135);
}

void HandleLocalizedSystemNotice(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength != 4) return;

    const u16 messageId = ReadLE16(packet.data + 2);
    const char* fallback = "";
    switch (messageId) {
    case 1221: fallback = "The party member was not summoned because you are not the party leader."; break;
    case 1222: fallback = "There is no party member to summon in the current map."; break;
    case 1266: fallback = "The mercenary contract has expired."; break;
    case 1267: fallback = "The mercenary has died."; break;
    case 1268: fallback = "You have released the mercenary."; break;
    case 1269: fallback = "The mercenary has run away."; break;
    case 1357: fallback = "This character's equipment information is not open to the public."; break;
    default: break;
    }
    // Selected mercenary_delete sends these owner-only notices before the
    // ordinary out-of-sight removal, which also happens during movement.
    if (messageId == 1266 || messageId == 1267) {
        mode.RetireMercenaryContract();
    }
    ro::UiMessageStrings strings;
    strings.Load();
    const auto text = strings.Get(messageId, fallback);
    if (text.empty()) return;

    // Selected Zc_Msg at 0x5B0A20 shows chat and forwards the same notice twice.
    const u32 color = messageId == 1221 || messageId == 1222 ? 0x000000ffu : 0x00ffffffu;
    g_windowMgr.SendMsg(3, 0, 0);
    g_windowMgr.SendMsg(1, reinterpret_cast<msgparam_t>(text.c_str()), color);
    g_windowMgr.SendMsg(20, reinterpret_cast<msgparam_t>(text.c_str()), color);
}

void HandleMercenaryInit(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength != 80) return;
    const auto* data = packet.data;
    const u32 gid = ReadLE32(data + 2);
    if (gid == 0) return;
    if (gid != g_session.m_merGID) {
        mode.RetireMercenaryContract();
    }
    auto& status = g_session.m_merStatus;
    g_session.ClearMercSkillItems();
    g_session.m_merGID = gid;
    status.alive = true;
    if (const auto* actor = mode.m_world ? mode.m_world->GetGameActorByAID(g_session.m_merGID) : nullptr)
        status.job = actor->m_job;
    ReadCompanionCombat(status.combat, data + 6);
    status.combat.magicAttack = ReadLE16(data + 8);
    status.name = ExtractFixedPacketString(packet, 22, 24);
    CacheOwnedCompanionActorName(mode, g_session.m_merGID, status.name);
    status.level = static_cast<s16>(ReadLE16(data + 46));
    status.combat.hp = ReadCompanionI32(data + 48);
    status.combat.maxHp = ReadCompanionI32(data + 52);
    status.combat.sp = ReadCompanionI32(data + 56);
    status.combat.maxSp = ReadCompanionI32(data + 60);
    status.expiresAt = ReadCompanionI32(data + 64);
    status.faith = static_cast<s16>(ReadLE16(data + 68));
    status.calls = ReadCompanionI32(data + 70);
    status.kills = ReadCompanionI32(data + 74);
    status.combat.attackRange = static_cast<s16>(ReadLE16(data + 78));
}

void HandleMercenaryParameter(CGameMode&, const PacketView& packet)
{
    auto& status = g_session.m_merStatus;
    if (!packet.data || packet.packetLength != 8 || !status.alive) return;
    const auto value = ReadCompanionI32(packet.data + 4);
    switch (ReadLE16(packet.data + 2)) {
    case 5: status.combat.hp = value; break;
    case 6: status.combat.maxHp = value; break;
    case 7: status.combat.sp = value; break;
    case 8: status.combat.maxSp = value; break;
    case 0x29: status.combat.attack = value; break;
    case 0x2b: status.combat.magicAttack = value; break;
    case 0x2d: status.combat.defense = value; break;
    case 0x2f: status.combat.magicDefense = value; break;
    case 0x31: status.combat.hit = value; break;
    case 0x34: status.combat.critical = value; break;
    case 0x35: status.combat.attackSpeed = value; break;
    case 0xa5: status.combat.flee = value; break;
    case 0xbd: status.kills = value; break;
    case 0xbe: status.faith = value; break;
    }
}

void HandleElementalInfo(CGameMode&, const PacketView& packet)
{
    if (!packet.data || packet.packetLength != 22) return;
    const auto* data = packet.data;
    const u32 gid = ReadLE32(data + 2);
    if (gid == 0) return;
    g_session.m_elementalStatus = {gid, ReadLE32(data + 6), ReadLE32(data + 10),
        ReadLE32(data + 14), ReadLE32(data + 18)};
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
    const int skid = ReadLE16(packet.data + 2);
    if (!SKILL_INFO::IsMercenarySkill(skid)) {
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
    skillInfo.SKID = skid;
    if (!TryGetExistingMercSkillInfo(skillInfo.SKID, skillInfo)) {
        skillInfo = {};
        skillInfo.SKID = skid;
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

bool StageSessionItemList(const PacketView& packet, bool equipment, bool storage, std::list<ITEM_INFO>& items)
{
    const int stride = equipment ? ro::net::GetActiveEquipmentItemListEntrySize(packet.packetId)
        : ro::net::GetActiveStackableItemListEntrySize(packet.packetId);
    const auto stageRow = [&](const u8* row) {
        ITEM_INFO item;
        if (!(equipment ? BuildEquipInventoryItem(packet, row, item) : BuildNormalInventoryItem(packet, row, item))) {
            return false;
        }
        if (storage) item.m_wearLocation = 0;
        items.push_back(std::move(item));
        return true;
    };
    using namespace ro::item_snapshot;
    return storage ? Stage<kFirstStorageIndex, kStorageSlots>(packet, stride, equipment, stageRow)
        : Stage<kFirstInventoryIndex, kInventorySlots>(packet, stride, equipment, stageRow);
}

void ApplyInventoryList(CGameMode& mode, const PacketView& packet, bool equipment)
{
    std::list<ITEM_INFO> items;
    if (!StageSessionItemList(packet, equipment, false, items)) return;
    unsigned int eggIndex = 0;
    if (equipment) {
        for (const auto& item : items) {
            if (item.GetItemId() >= 9001 && item.GetItemId() < 9050 && item.m_isDamaged) {
                eggIndex = item.m_itemIndex;
            }
        }
    }

    const bool replacedWornEquipment = g_session.ReplaceInventoryCategory(equipment, std::move(items));
    if (eggIndex) g_session.m_petStatus.eggIndex = eggIndex;
    if (equipment || replacedWornEquipment) {
        g_session.RebuildPlayerEquipmentAppearanceFromInventory();
        SyncSessionAppearanceToLocalPlayer(mode);
    }
}

void HandleNormalInventoryList(CGameMode& mode, const PacketView& packet)
{
    if (ro::net::IsActiveNormalInventoryListPacket(packet.packetId)) ApplyInventoryList(mode, packet, false);
}

void HandleEquipInventoryList(CGameMode& mode, const PacketView& packet)
{
    if (ro::net::IsActiveEquipInventoryListPacket(packet.packetId)) ApplyInventoryList(mode, packet, true);
}

bool BuildStorageItemFromAddPacket(const PacketView& packet, ITEM_INFO& outItem)
{
    if (!packet.data) {
        return false;
    }

    outItem = ITEM_INFO{};
    outItem.m_itemIndex = ReadLE16(packet.data + 2);
    outItem.m_num = static_cast<int>(ReadLE32(packet.data + 4));
    outItem.SetItemId(ReadLE16(packet.data + 8));

    if (packet.packetId == ro::net::GetActiveMapReceiveProfile().storageItemAddedBasic) {
        if (packet.packetLength < 21) {
            return false;
        }
        outItem.m_isIdentified = packet.data[10];
        outItem.m_isDamaged = packet.data[11];
        outItem.m_refiningLevel = packet.data[12];
        outItem.m_slot[0] = ReadLE16(packet.data + 13);
        outItem.m_slot[1] = ReadLE16(packet.data + 15);
        outItem.m_slot[2] = ReadLE16(packet.data + 17);
        outItem.m_slot[3] = ReadLE16(packet.data + 19);
    } else if (ro::net::IsActiveStorageItemAddedTypedPacket(packet.packetId)) {
        if (packet.packetLength < 22) {
            return false;
        }
        outItem.m_itemType = packet.data[10];
        outItem.m_isIdentified = packet.data[11];
        outItem.m_isDamaged = packet.data[12];
        outItem.m_refiningLevel = packet.data[13];
        outItem.m_slot[0] = ReadLE16(packet.data + 14);
        outItem.m_slot[1] = ReadLE16(packet.data + 16);
        outItem.m_slot[2] = ReadLE16(packet.data + 18);
        outItem.m_slot[3] = ReadLE16(packet.data + 20);
    } else {
        return false;
    }

    if (outItem.m_num <= 0) {
        outItem.m_num = 1;
    }
    return outItem.m_itemIndex != 0;
}

void HandleStorageStatus(CGameMode&, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 6) {
        return;
    }

    const int currentCount = static_cast<int>(ReadLE16(packet.data + 2));
    const int maxCount = static_cast<int>(ReadLE16(packet.data + 4));
    // An empty refresh has no category frames. The count still establishes emptiness.
    if (currentCount == 0) g_session.ClearStorageItems();
    g_session.OpenStorage(currentCount, maxCount);
    g_session.ClearNpcShopState(false);
    g_windowMgr.CloseNpcDialogWindows();
    g_windowMgr.CloseNpcShopWindows();
    g_windowMgr.MakeWindow(UIWindowMgr::WID_ITEMWND);
    g_windowMgr.MakeWindow(UIWindowMgr::WID_STORAGEWND);
    DbgLog("[GameMode] storage status current=%d max=%d\n", currentCount, maxCount);
}

void ApplyStorageList(const PacketView& packet, bool equipment)
{
    std::list<ITEM_INFO> items;
    if (StageSessionItemList(packet, equipment, true, items)) {
        g_session.ReplaceStorageCategory(equipment, std::move(items));
    }
}

void HandleNormalStorageList(CGameMode&, const PacketView& packet)
{
    if (ro::net::IsActiveNormalStorageListPacket(packet.packetId)) ApplyStorageList(packet, false);
}

void HandleEquipStorageList(CGameMode&, const PacketView& packet)
{
    if (ro::net::IsActiveEquipStorageListPacket(packet.packetId)) ApplyStorageList(packet, true);
}

void HandleStorageItemAdded(CGameMode&, const PacketView& packet)
{
    ITEM_INFO itemInfo;
    if (!BuildStorageItemFromAddPacket(packet, itemInfo)) {
        return;
    }

    g_session.AddStorageItem(itemInfo);
    DbgLog("[GameMode] storage add pkt=0x%04X index=%u amount=%d item=%u type=%u\n",
        static_cast<unsigned int>(packet.packetId),
        static_cast<unsigned int>(itemInfo.m_itemIndex),
        itemInfo.m_num,
        static_cast<unsigned int>(itemInfo.GetItemId()),
        static_cast<unsigned int>(itemInfo.m_itemType));
}

void HandleStorageItemRemoved(CGameMode&, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 8) {
        return;
    }

    const unsigned int itemIndex = ReadLE16(packet.data + 2);
    const int amount = static_cast<int>(ReadLE32(packet.data + 4));
    // The selected server uses a zero-count 0x00F6 as a failed-transfer
    // correction. It must not erase the storage row.
    if (amount <= 0) {
        DbgLog("[GameMode] storage remove ignored index=%u amount=%d\n",
            itemIndex,
            amount);
        return;
    }

    g_session.RemoveStorageItem(itemIndex, amount);
    DbgLog("[GameMode] storage remove index=%u amount=%d\n",
        static_cast<unsigned int>(itemIndex),
        amount);
}

void HandleStorageClose(CGameMode&, const PacketView&)
{
    g_session.CloseStorage();
    g_windowMgr.CloseStorageWindows();
    DbgLog("[GameMode] storage close\n");
}

void HandleStoragePasswordRequest(CGameMode&, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 4) {
        return;
    }

    const u16 info = ReadLE16(packet.data + 2);
    DbgLog("[GameMode] storage password request info=%u\n", static_cast<unsigned int>(info));
    g_windowMgr.PushChatEvent("Storage password flow is not implemented yet.", 0x000000FFu, 6);
}

void HandleStoragePasswordResult(CGameMode&, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 6) {
        return;
    }

    const u16 result = ReadLE16(packet.data + 2);
    const u16 errorCount = ReadLE16(packet.data + 4);
    DbgLog("[GameMode] storage password result=%u errors=%u\n",
        static_cast<unsigned int>(result),
        static_cast<unsigned int>(errorCount));
}

void AnnouncePickedUpItem(const ITEM_INFO& itemInfo)
{
    // Ref Zc_Item_Pickup_Ack sprintf(MsgStr(MSI_GET_ITEM), name, count).
    // msgstringtable.txt index 153: "You got %s (%d).#"
    // The color argument is COLORREF 16776960 (0x00FFFF00), which is RGB(0, 255, 255).
    // Chat stores that as R in the high byte. Type 5 is the get-item chat class;
    // the Info tab is the channel that still shows it.
    const auto message=g_ttemmgr.GetItemObtainChatText(itemInfo);
    g_windowMgr.PushSourceChatEvent(message.c_str(),0x0000FFFFu,6, ro::chat::NoticeRoute::Normal, ro::chat::NoticeType::ObtainItem);
}

void HandleItemPickupAck(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 23) {
        return;
    }

    if (!ro::net::IsActiveItemPickupAckPacket(packet.packetId)) {
        return;
    }

    const int result = packet.data[22];
    const u32 pickedObjectAid = mode.m_pickupReqItemNaidList.empty()
        ? 0 : mode.m_pickupReqItemNaidList.front();
    // Ref clears the intent before examining the result. Rejection must stop
    // PumpPendingPickupRequest from retrying until the user asks again.
    mode.m_pickupReqItemNaidList.clear();
    mode.m_lastPickupRequestTick = 0;
    if (result != 0) {
        // Source message3 shows chat for these known failures even when the
        // selected language resource deliberately contains an empty message.
        if(result==1 || result==2 || result==4 || result==5 || result==6)
            g_windowMgr.SendMsg(3, 0, 0);
        const auto notice=g_ttemmgr.GetPickupFailureNotice(static_cast<unsigned char>(result));
        if (!notice.empty()) {
            g_windowMgr.PushSourceChatEvent(notice.c_str(),0x00ff0000u,0, ro::chat::NoticeRoute::Normal, ro::chat::NoticeType::None);
        }
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
        itemInfo.m_location = 0x8000;
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

    if (pickedObjectAid != 0) {
        StartLocalPickupAnimation(mode, pickedObjectAid);
    }

    g_session.AddInventoryItem(itemInfo);
    AnnouncePickedUpItem(itemInfo);
    g_windowMgr.ShowItemObtainNotification(itemInfo);
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
    const auto existingIt = mode.m_groundItemList.find(objectId);
    const bool existed = existingIt != mode.m_groundItemList.end();
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
    DbgLog("[GroundItem] entry obj=%u item=%u amount=%u pos=%u,%u sub=%u,%u existed=%d dropAnim=%d\n",
        objectId,
        static_cast<unsigned int>(itemId),
        static_cast<unsigned int>(amount),
        static_cast<unsigned int>(tileX),
        static_cast<unsigned int>(tileY),
        static_cast<unsigned int>(subX),
        static_cast<unsigned int>(subY),
        existed ? 1 : 0,
        playDropAnimation ? 1 : 0);
}

void HandleGroundItemEntry(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 17) {
        return;
    }

    if (!ro::net::IsActiveGroundItemEntryPacket(packet.packetId)) {
        return;
    }

    const u32 objectId = ReadLE32(packet.data + 2);
    const u16 itemId = ReadLE16(packet.data + 6);
    const u8 identified = packet.data[8];
    const u16 tileX = ReadLE16(packet.data + 9);
    const u16 tileY = ReadLE16(packet.data + 11);

    if (!ro::net::IsActiveGroundItemDroppedPacket(packet.packetId)) {
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
    const auto stateIt = mode.m_groundItemList.find(objectId);
    const bool hadState = stateIt != mode.m_groundItemList.end();
    const u32 itemId = hadState ? stateIt->second.itemId : 0;
    const u16 amount = hadState ? stateIt->second.amount : 0;
    bool hadWorldItem = false;
    if (mode.m_world) {
        for (CItem* item : mode.m_world->m_itemList) {
            if (item && item->m_aid == objectId) {
                hadWorldItem = true;
                break;
            }
        }
    }
    const bool matchedActor = mode.m_runtimeActors.find(objectId) != mode.m_runtimeActors.end();
    DbgLog("[GroundItem] disappear obj=%u hadState=%d item=%u amount=%u worldItem=%d actorMatch=%d\n",
        objectId,
        hadState ? 1 : 0,
        itemId,
        static_cast<unsigned int>(amount),
        hadWorldItem ? 1 : 0,
        matchedActor ? 1 : 0);
    mode.m_groundItemList.erase(objectId);
    mode.m_pickupReqItemNaidList.remove(objectId);
    if (mode.m_pickupReqItemNaidList.empty()) {
        mode.m_lastPickupRequestTick = 0;
    }
}

void HandleRentalItem(CGameMode&, const PacketView& packet)
{
    if (!packet.data) return;
    const auto& profile = ro::net::GetActiveMapReceiveProfile();
    std::vector<std::string> messages;
    if (packet.packetId == profile.rentalTimeCounter) {
        if (packet.packetLength != 8) return;
        const u16 itemId = ReadLE16(packet.data + 2);
        if (!itemId) return;
        messages = g_ttemmgr.GetRentalTimeNotices(itemId, ReadLE32(packet.data + 4));
    } else if (packet.packetId == profile.rentalItemDelete) {
        if (packet.packetLength != 6) return;
        const u16 index = ReadLE16(packet.data + 2), itemId = ReadLE16(packet.data + 4);
        if (!index || !itemId) return;
        // Primary Zc_Cash_Item_Delete decrements one. Selected pc_delitem then
        // sends 00AF with the full stack amount; an absent row stays absent.
        g_session.RemoveInventoryItem(index, 1);
        if (g_windowMgr.m_itemWnd) g_windowMgr.m_itemWnd->Invalidate();
        if (g_windowMgr.m_equipWnd) g_windowMgr.m_equipWnd->Invalidate();
        if (g_windowMgr.m_shortCutWnd) g_windowMgr.m_shortCutWnd->Invalidate();
        if (g_windowMgr.m_itemInfoWnd) g_windowMgr.m_itemInfoWnd->RefreshBoundItem();
        messages.push_back(g_ttemmgr.GetRentalDeleteNotice(itemId));
    } else return;
    // One system event fans out to the modern All/Info tabs. No actor/world state
    // or absolute expiry timestamp is inferred from the warning's seconds field.
    g_windowMgr.EnsureChatWindowVisible();
    for (const auto& message : messages) g_windowMgr.PushChatEvent(message.c_str(), 0x00ffff00u, 6);
}

void HandleItemRemove(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data) {
        return;
    }

    if (!ro::net::IsActiveItemRemovePacket(packet.packetId)) {
        return;
    }

    unsigned int itemIndex = 0;
    int amount = 0;
    if (!ro::net::IsActiveExtendedItemRemovePacket(packet.packetId)) {
        if (packet.packetLength < 6) {
            return;
        }
        itemIndex = ReadLE16(packet.data + 2);
        amount = static_cast<int>(ReadLE16(packet.data + 4));
    } else {
        if (packet.packetLength < 8) {
            return;
        }
        itemIndex = ReadLE16(packet.data + 4);
        amount = static_cast<int>(ReadLE16(packet.data + 6));
    }

    if (mode.m_waitingItemThrowAck == static_cast<int>(itemIndex)) {
        mode.m_waitingItemThrowAck = 0;
    }

    // The selected pre-20091117 server reports a rejected drop as a basic
    // 0x00AF acknowledgement with count zero. It is not an inventory removal.
    if (packet.packetId == 0x00AF && amount == 0) {
        return;
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

    if (result != 1 || itemIndex == 0 || wearLocation == 0) {
        if (result==0 || result==2) {
            const auto notice=g_ttemmgr.GetEquipFailureNotice();
            g_windowMgr.PushSourceChatEvent(notice.c_str(),0x00ff0000u,6, ro::chat::NoticeRoute::Normal, ro::chat::NoticeType::Equip);
        }
        DbgLog("[GameMode] equip ack index=%u wear=0x%04X ok=%d\n",
            itemIndex,
            static_cast<unsigned int>(wearLocation),
            result);
        return;
    }

    // The primary GetItemInfoByIndex guard runs before moving any equip mask.
    if (!g_session.GetInventoryItemByIndex(itemIndex)) return;
    g_session.ClearInventoryWearLocationMask(wearLocation, itemIndex);
    g_session.SetInventoryItemWearLocation(itemIndex, wearLocation);
    g_session.RebuildPlayerEquipmentAppearanceFromInventory();
    SyncSessionAppearanceToLocalPlayer(mode);
    DbgLog("[GameMode] equip ack index=%u wear=0x%04X ok=%d\n",
        itemIndex,
        static_cast<unsigned int>(wearLocation),
        result);
}

void HandleEquipAmmo(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 4) return;
    const unsigned int itemIndex = ReadLE16(packet.data + 2);
    g_session.ClearInventoryWearLocationMask(0x8000, itemIndex);
    if (itemIndex != 0) g_session.SetInventoryItemWearLocation(itemIndex, 0x8000);
    mode.m_waitingWearEquipAck = 0;
    DbgLog("[GameMode] ammunition equip ack index=%u\n", itemIndex);
}

void RetirePlayerSkillProceed(CGameMode& mode, CPlayer& player)
{
    if (player.m_proceedType.type == 3 || player.m_proceedType.type == 4 || player.m_proceedType.type == 6) {
        player.m_proceedType.type = 0;
        player.m_isAttackRequest = 0;
    }
    mode.SynchronizePlayerSkillProceed(&player, true);
}

void RetireActorTarget(CGameMode& mode, u32 gid)
{
    if (gid == 0) return;
    auto* player = mode.m_world ? mode.m_world->m_player : nullptr;
    if (player && player->m_proceedTargetGid == gid) {
        player->m_proceedTargetGid = 0;
        if (player->m_proceedType.type == 3 || player->m_proceedType.type == 6) {
            RetirePlayerSkillProceed(mode, *player);
        }
    }
    if (auto* window = g_windowMgr.m_petTamingDeceiveWnd;
        window && window->GetTargetGid() == gid && !window->IsSelected()) {
        g_windowMgr.DeleteWindow(window);
    }
}

void HandleActionFailure(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 4) return;
    const unsigned int type = ReadLE16(packet.data + 2);
    const char* message = nullptr;
    int color = 16777060;
    // Ref/GameModePacket.cpp Zc_action_failure, MsgStringTable 242..245.
    switch (type) {
    case 0: message = "Please equip the proper ammunition first."; color = 255; break;
    case 1: message = "You can't Attack or use Skills because your Weight Limit has been exceeded."; break;
    case 2: message = "You can't use Skills because Weight Limit has been exceeded."; break;
    case 3:
        if (g_session.m_playerJob == 12 || g_session.m_playerJob == 4035 || g_session.m_playerJob == 4013)
            message = "You have equipped throwing daggers.";
        else if (g_session.m_playerJob == 24) message = "Bullets have been equipped.";
        else if (g_session.m_playerJob != 25) message = "Ammunition has been equipped.";
        break;
    default: return;
    }
    if (type <= 2) {
        // Stop retrying a server-rejected action until the user tries again.
        mode.SendMsg(CGameMode::GameMsg_LButtonUp, 0, 0, 0);
        mode.m_lastMonGid = 0;
        mode.m_lastLockOnMonGid = 0;
        mode.m_hasAttackChaseHint = 0;
        mode.m_attackChaseTargetGid = 0;
        mode.m_hasSkillChase = 0;
        mode.m_hasGroundSkillChase = 0;
        if (mode.m_world && mode.m_world->m_player) {
            CPlayer* player = mode.m_world->m_player;
            player->m_targetGid = 0;
            player->m_proceedTargetGid = 0;
            RetirePlayerSkillProceed(mode, *player);
        }
    }
    if (message) g_windowMgr.SendMsg(kUiChatEventMsg, reinterpret_cast<msgparam_t>(message), color);
    DbgLog("[GameMode] action failure type=%u message='%s'\n", type, message ? message : "");
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

void RetimeMovePath(CGameActor& actor, u32 oldSpeed, u32 newSpeed)
{
    if (!actor.m_isMoving || oldSpeed == 0 || newSpeed == 0 || oldSpeed == newSpeed
        || actor.m_path.m_cells.size() < 2) {
        return;
    }

    const u32 now = g_session.GetServerTime();
    for (PathCell& cell : actor.m_path.m_cells) {
        if (cell.arrivalTime <= now) {
            continue;
        }
        const u32 remain = cell.arrivalTime - now;
        cell.arrivalTime = now + static_cast<u32>((static_cast<u64>(remain) * newSpeed) / oldSpeed);
    }
    actor.m_moveEndTime = actor.m_path.m_cells.back().arrivalTime;
}

bool ApplySelfStatusUpdate(CGameMode& mode, u32 statusType, u32 value)
{
    if (statusType == kStatusSpeed) {
        // Ref Zc_Par_Change case 0 stores this on the session. CPlayer pathfinding
        // reads that value, not the speed copied off a spawn packet.
        g_session.m_speed = value;
        if (mode.m_world && mode.m_world->m_player) {
            CPlayer* player = mode.m_world->m_player;
            const u32 baked = player->m_speed != 0 ? player->m_speed : 150u;
            const u32 next = value != 0 ? value : 150u;
            RetimeMovePath(*player, baked, next);
            player->m_speed = value;
        }
        return true;
    }

    const bool updatedSession = ApplySelfStatusUpdateToSession(statusType, value);
    if (statusType == kStatusCartInfo) {
        UpdateKnownLocalCartState(true);
        ApplyPendingLocalCartState(mode);
        return true;
    }

    if (!mode.m_world || !mode.m_world->m_player) {
        return updatedSession;
    }

    CPlayer* player = mode.m_world->m_player;
    switch (statusType) {
    case kStatusHp:
        player->m_Hp = value;
        return true;
    case kStatusMaxHp:
        player->m_MaxHp = value;
        return true;
    case kStatusSp:
        player->m_Sp = value;
        return true;
    case kStatusMaxSp:
        player->m_MaxSp = value;
        return true;
    case kStatusBaseLevel:
        player->m_clevel = ClampToU16(value);
        // Ref GameModeShowEFST starts the aura when it learns clevel is 99.
        // The local client learns that from this status update.
        if (value == 99) {
            player->SendMsg(player, 126, 0, 0, 0);
        }
        return true;
    case kStatusJobLevel:
        return updatedSession;
    case kStatusBaseExp:
    case kStatusJobExp:
        player->m_Exp = value;
        return true;
    default:
        return updatedSession;
    }
}

bool ApplySelfStatUpdate(CGameMode& mode, u32 statusType, s32 baseValue, s32 plusValue)
{
    const auto* fields = FindPrimaryStat(statusType);
    if (!fields) return false;

    g_session.*fields->bonus = plusValue;
    ApplyPrimaryStatBase(mode, *fields, baseValue);
    return true;
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

void HandleGmStatusSummary(CGameMode&, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 42) {
        return;
    }

    const auto combat = [&](int offset) { return static_cast<s16>(ReadLE16(packet.data + offset)); };
    const auto sendLine = [](u32 color, const char* format, auto... values) {
        char text[256];
        std::snprintf(text, sizeof(text), format, values...);
        g_windowMgr.SendMsg(1, reinterpret_cast<msgparam_t>(text), color);
    };

    // Ref Zc_Ack_Status_GM prints the inspected character's stats to chat.
    sendLine(0xffff, "             STR=%3d      AGI=%3d      VIT=%3d      INT=%3d      DEX=%3d      LUK=%d",
        packet.data[2], packet.data[4], packet.data[6], packet.data[8], packet.data[10], packet.data[12]);
    sendLine(65335, "standard STR=%3d      AGI=%3d      VIT=%3d      INT=%3d      DEX=%3d      LUK=%2d",
        packet.data[3], packet.data[5], packet.data[7], packet.data[9], packet.data[11], packet.data[13]);
    sendLine(0xffff, "    attPower=%3d    refiningPow=%3d    MAXmatPow=%3d    MINmatPower=%3d       ASPD=%3d",
        combat(14), combat(16), combat(18), combat(20), combat(38));
    sendLine(65335, "itemdefPow=%3d     plusdefPow=%3d    mdefPower=%3d    plusmdefPow=%3d    plusASPD=%3d",
        combat(22), combat(24), combat(26), combat(28), combat(40));
    sendLine(0xffff, "hitSuccessVal=%3d    avoidSuccessVal=%3d    plusAvoidSuccessValue=%3d",
        combat(30), combat(32), combat(34));
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
    constexpr std::array<u32, 14> combatTypes = { kStatusAtk, kStatusRefineAtk,
        kStatusMatkMax, kStatusMatkMin, kStatusDef, kStatusDefBonus, kStatusMdef, kStatusMdefBonus,
        kStatusHit, kStatusFlee, kStatusFleeBonus, kStatusCritical, kStatusAspd, kStatusPlusAspd };
    for (std::size_t index = 0; index < combatTypes.size(); ++index) {
        const auto value = static_cast<s16>(ReadLE16(packet.data + 16 + index * 2));
        ApplyCombatStatusParamToSession(combatTypes[index], value, true);
    }

    InvalidateStatusWindows();
}

bool ApplySourceWeightParam(u32 statusType,u32 value)
{
    if(statusType!=24 && statusType!=25)return false;
    const int oldWeight=g_session.m_weight,oldMax=g_session.m_maxWeight;
    const int before=ro::stats::SourceWeightPercent(oldWeight,oldMax);
    if(statusType==24)g_session.m_weight=static_cast<s32>(value)/10;
    else g_session.m_maxWeight=static_cast<s32>(value)/10;
    const int after=ro::stats::SourceWeightPercent(g_session.m_weight,g_session.m_maxWeight);
    const auto notice=[](unsigned id,bool released){
        ro::UiMessageStrings table;table.Load();
        const char* fallback=id==306?"You cannot recover HP or SP while overweight.":id==307?"You cannot attack while overweight."
            :id==308?"You can recover HP and SP again.":"You can attack again.";
        const auto text=table.Get(id,fallback);
        g_windowMgr.SendMsg(3,0,0,0);
        g_windowMgr.SendMsg(1,reinterpret_cast<msgparam_t>(text.c_str()),released?0xffff00:255,7,0);
    };
    if(oldMax){
        if(statusType==24 && after>before){
            if(after>=50 && before<50)notice(306,false);
            if(after>=90 && before<90)notice(307,false);
        }else if((statusType==24 && after<before) || (statusType==25 && oldMax<g_session.m_maxWeight)){
            if(after<50 && before>=50)notice(308,true);
            if(after<90 && before>=90)notice(309,true);
        }
    }
    InvalidateStatusWindows();return true;
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

    if(packet.packetId==0x00b0 && ApplySourceWeightParam(statusType,value))return;

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
    const auto baseValue = static_cast<s32>(ReadLE32(packet.data + 6));
    const auto plusValue = static_cast<s32>(ReadLE32(packet.data + 10));
    if (ApplySelfStatUpdate(mode, statusType, baseValue, plusValue)) {
        CPlayer* player = mode.m_world ? mode.m_world->m_player : nullptr;
        DbgLog("[GameMode] self stat type=%u base=%d plus=%d str=%u agi=%u vit=%u int=%u dex=%u luk=%u\n",
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

    if (const auto* fields = FindPrimaryStat(statusType)) {
        ApplyPrimaryStatBase(mode, *fields, value);
    }
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
    if (mode.m_numNotifyTime != 0) {
        g_session.UpdateServerTime(serverTick);
    } else {
        g_session.SetServerTime(serverTick);
    }
    mode.m_receiveSyneRequestTime = 1;
    ++mode.m_numNotifyTime;
    const u32 receiveTick = timeGetTime();
    g_session.RecalcAveragePingTime(receiveTick - mode.m_syncRequestTime);
    DbgLog("[GameMode] notify time tick=%u count=%u\n",
        static_cast<unsigned int>(serverTick),
        static_cast<unsigned int>(mode.m_numNotifyTime));
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

void HandleUpdateMapInfo(CGameMode& mode, const PacketView& packet)
{
    if (!mode.m_world || !packet.data || packet.packetLength != 24) {
        return;
    }
    const std::string map = ToLowerAsciiStatus(NormalizeMapName(ExtractFixedPacketString(packet, 8, 16)));
    const std::string currentMap = ToLowerAsciiStatus(NormalizeMapName(mode.m_rswName));
    if (map.empty() || map != currentMap) {
        return;
    }
    mode.m_world->ApplyMapCellType(static_cast<s16>(ReadLE16(packet.data + 2)),
        static_cast<s16>(ReadLE16(packet.data + 4)), static_cast<s16>(ReadLE16(packet.data + 6)));
}

struct NpcMenuOption {
    std::string text;
    u8 choice = 0;
};

std::vector<NpcMenuOption> SplitNpcMenuOptions(const std::string& text)
{
    std::vector<NpcMenuOption> options;
    size_t start = 0;
    int choice = 1;
    while (start <= text.size()) {
        const size_t separator = text.find(':', start);
        const size_t end = (separator == std::string::npos) ? text.size() : separator;
        if (end > start && choice <= 0xFE) {
            options.push_back(NpcMenuOption{ text.substr(start, end - start), static_cast<u8>(choice) });
        }
        if (separator == std::string::npos) {
            break;
        }
        start = separator + 1;
        ++choice;
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

void RememberNpcConversation(CGameMode& mode, u32 npcId)
{
    if (npcId == 0) {
        return;
    }
    mode.m_lastNaid = npcId;
    mode.m_isOnQuest = 1;
}

void ReleaseNpcConversationLock(CGameMode& mode)
{
    mode.m_isOnQuest = 0;
}

void HandleNpcDialogText(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 8) {
        return;
    }

    const u32 npcId = ReadLE32(packet.data + 4);
    RememberNpcConversation(mode, npcId);
    auto* sayWnd = static_cast<UISayDialogWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_SAYDIALOGWND));
    if (!sayWnd) {
        return;
    }
    sayWnd->AppendText(npcId, ExtractPacketString(packet, 8));
}

void HandleNpcDialogNext(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 6) {
        return;
    }

    const u32 npcId = ReadLE32(packet.data + 2);
    RememberNpcConversation(mode, npcId);
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

void HandleNpcDialogClose(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 6) {
        return;
    }

    const u32 npcId = ReadLE32(packet.data + 2);
    RememberNpcConversation(mode, npcId);
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

    ReleaseNpcConversationLock(mode);
    g_windowMgr.CloseNpcDialogWindows();
}

void HandleNpcDialogMenu(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 8) {
        return;
    }

    const u32 npcId = ReadLE32(packet.data + 4);
    RememberNpcConversation(mode, npcId);
    const std::string menuText = ExtractPacketString(packet, 8);
    const std::vector<NpcMenuOption> parsedOptions = SplitNpcMenuOptions(menuText);
    std::vector<std::string> options;
    std::vector<u8> optionChoices;
    options.reserve(parsedOptions.size());
    optionChoices.reserve(parsedOptions.size());
    for (const NpcMenuOption& option : parsedOptions) {
        options.push_back(option.text);
        optionChoices.push_back(option.choice);
    }
    if (g_windowMgr.m_sayDialogWnd) {
        g_windowMgr.m_sayDialogWnd->ClearAction();
    }
    if (g_windowMgr.m_npcInputWnd) {
        g_windowMgr.m_npcInputWnd->HideInput();
    }

    DbgLog("[GameMode] npc menu recv npc=%u raw='%s' visible=%u\n",
        static_cast<unsigned int>(npcId),
        menuText.c_str(),
        static_cast<unsigned int>(options.size()));

    auto* menuWnd = static_cast<UINpcMenuWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_NPCMENUWND));
    if (menuWnd) {
        menuWnd->SetMenu(npcId, options, optionChoices);
    }
}

void HandleNpcDialogNumberInput(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 6) {
        return;
    }

    const u32 npcId = ReadLE32(packet.data + 2);
    RememberNpcConversation(mode, npcId);
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

void HandleNpcDialogStringInput(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 6) {
        return;
    }

    const u32 npcId = ReadLE32(packet.data + 2);
    RememberNpcConversation(mode, npcId);
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

void HandleNpcShopDealType(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 6) {
        return;
    }

    const u32 npcId = ReadLE32(packet.data + 2);
    mode.m_isOnQuest = 1;
    g_session.SetNpcShopChoice(npcId);
    CloseNpcShopWindows();
    g_windowMgr.MakeWindow(UIWindowMgr::WID_CHOOSESELLBUYWND);
}

void HandleNpcShopBuyList(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 4) {
        return;
    }

    const size_t payloadLength = static_cast<size_t>(packet.packetLength - 4);
    if (payloadLength % 11 != 0) {
        return;
    }

    mode.m_isOnQuest = 1;

    std::vector<NPC_SHOP_ROW> rows;
    rows.reserve(payloadLength / 11);
    for (size_t offset = 4; offset + 11 <= static_cast<size_t>(packet.packetLength); offset += 11) {
        NPC_SHOP_ROW row{};
        row.price = ReadLE32(packet.data + offset + 0);
        row.secondaryPrice = ReadLE32(packet.data + offset + 4);
        row.itemInfo.m_itemType = packet.data[offset + 8];
        row.itemInfo.SetItemId(ReadLE16(packet.data + offset + 9));
        row.itemInfo.m_isIdentified = 1;
        rows.push_back(std::move(row));
    }

    g_windowMgr.CloseNpcShopQuantityPrompt();
    g_session.SetNpcShopRows(g_session.m_shopNpcId, NpcShopMode::Buy, rows);
    if (g_windowMgr.m_chooseSellBuyWnd) {
        g_windowMgr.DeleteWindow(g_windowMgr.m_chooseSellBuyWnd);
    }
    g_windowMgr.MakeWindow(UIWindowMgr::WID_ITEMSHOPWND);
    g_windowMgr.MakeWindow(UIWindowMgr::WID_ITEMPURCHASEWND);
}

void HandleNpcShopSellList(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 4) {
        return;
    }

    const size_t payloadLength = static_cast<size_t>(packet.packetLength - 4);
    if (payloadLength % 10 != 0) {
        return;
    }

    mode.m_isOnQuest = 1;

    std::vector<NPC_SHOP_ROW> rows;
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
        row.price = ReadLE32(packet.data + offset + 2);
        row.secondaryPrice = ReadLE32(packet.data + offset + 6);
        row.availableCount = itemInfo->m_num;
        rows.push_back(std::move(row));
    }

    g_windowMgr.CloseNpcShopQuantityPrompt();
    g_session.SetNpcShopRows(g_session.m_shopNpcId, NpcShopMode::Sell, rows);
    if (g_windowMgr.m_chooseSellBuyWnd) {
        g_windowMgr.DeleteWindow(g_windowMgr.m_chooseSellBuyWnd);
    }
    g_windowMgr.MakeWindow(UIWindowMgr::WID_ITEMSHOPWND);
    g_windowMgr.MakeWindow(UIWindowMgr::WID_ITEMSELLWND);
}

void HandleNpcShopBuyResult(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 3) {
        return;
    }

    if (!g_session.ConsumeNpcShopResult(NpcShopMode::Buy)) return;

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

}

void HandleNpcShopSellResult(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 3) {
        return;
    }

    if (!g_session.ConsumeNpcShopResult(NpcShopMode::Sell)) return;

    if (packet.data[2] == 0) {
        PushShopResultMessage("The deal has successfully completed.", 0x00FFFF00u);
    } else if (packet.data[2] == 1) {
        PushShopResultMessage("The deal has failed.", 0x000000FFu);
    }

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

void HandleNpcProgress(CGameMode& mode, const PacketView& packet)
{
    if (packet.data && packet.packetLength == 10) {
        // Zc_Progress uses the duration and leaves the gauge's original colors in place.
        mode.StartNpcProgress(ReadLE32(packet.data + 6), timeGetTime());
    }
}

void HandleNpcProgressCancel(CGameMode& mode, const PacketView& packet)
{
    if (packet.data && packet.packetLength == 2) {
        mode.FinishNpcProgress(*CRagConnection::instance());
    }
}

void HandleCompass(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength != 23) {
        return;
    }

    // Zc_compass ignores the NPC ID and keys the marker by the byte at 18.
    const u32 type = ReadLE32(packet.data + 6);
    const u32 id = packet.data[18];
    if (type == 2) {
        mode.m_compassPosList.erase(id);
        return;
    }

    mode.m_compassPosList[id] = CompassMarker{
        static_cast<int>(ReadLE32(packet.data + 10)),
        static_cast<int>(ReadLE32(packet.data + 14)),
        ReadLE32(packet.data + 19) | 0xFF000000u,
        type,
        timeGetTime(),
    };
}

void HandleNotifyMapInfo(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength != 4) {
        return;
    }

    const u16 type = ReadLE16(packet.data + 2);
    const char* message = nullptr;
    // Ref supplies Win32 COLORREF 0x00FFFF00; chat stores packed RGB.
    u32 color = 0x0000FFFFu;
    switch (type) {
    case 0:
        message = "Teleport is not allowed in this area.";
        color = 0x00FF0000u;
        break;
    case 1:
        message = "Saved point cannot be memorized.";
        break;
    case 2:
        message = "Skills cannot be used in this area.";
        break;
    case 3:
        mode.m_waitingUseItemAck = 0;
        message = "Items cannot be used in this area.";
        break;
    default:
        return;
    }

    RecordChat(mode, message, color, kChatChannelSystem,
        {ro::chat::NoticeRoute::NormalAndCommand, ro::chat::NoticeType::None, {}});
}

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

std::string StripInlineColorCodes(const std::string& text)
{
    if (text.empty()) {
        return text;
    }

    std::string cleaned;
    cleaned.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        u32 ignoredColor = 0;
        if (text[i] == '^' && ParseHexColor(text, i + 1, ignoredColor)) {
            i += 6;
            continue;
        }
        cleaned.push_back(text[i]);
    }
    return cleaned;
}

bool ParseMapChangePacket(const PacketView& packet, MapChangeInfo& outInfo)
{
    outInfo = {};

    if (!packet.data || packet.packetLength < 22) {
        return false;
    }

    if (!ro::net::IsActiveMapChangePacket(packet.packetId)) {
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

    if (ro::net::IsActiveServerMoveMapChangePacket(packet.packetId) && packet.packetLength >= 28) {
        outInfo.ip = ReadLE32(packet.data + 22);
        outInfo.port = ReadLE16(packet.data + 26);
        outInfo.hasServerMove = outInfo.ip != 0 && outInfo.port != 0;
    }

    return !outInfo.mapName.empty();
}

bool SendLoadEndAckPacket()
{
    const u16 opcode = ro::net::GetActiveMapGameplaySendProfile().notifyActorInit;
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
    if (info.mapName.empty()
        || mode.m_mapLoadingStage != CGameMode::MapLoading_None
        || !mode.m_world
        || !mode.m_world->m_player
        || !mode.m_world->m_ground
        || !mode.m_view
        || mode.m_rswName[0] == '\0') {
        return false;
    }

    const std::string currentMap = ToLowerAsciiStatus(NormalizeMapName(std::string(mode.m_rswName)));
    const std::string targetMap = ToLowerAsciiStatus(info.mapName);
    return !currentMap.empty() && currentMap == targetMap;
}

void PrepareSameMapWarpReuse(CGameMode& mode)
{
    mode.m_recallPartnerName.clear();
    mode.SendMsg(78, 0, 0, 0);
    g_session.m_petStatus.ResetMapState();
    mode.ResetCompanionMapState();
    g_windowMgr.CloseCompanionWindows(true);
    g_windowMgr.CloseCompanionWindows(false);
    const int draggedSkill = mode.m_dragInfo.skillId;
    if (mode.m_dragType == static_cast<int>(DragType::ShortcutSkill)
        && ((draggedSkill >= 8000 && draggedSkill < 8017) || SKILL_INFO::IsMercenarySkill(draggedSkill))) {
        mode.m_dragType = 0;
        mode.m_dragInfo = {};
    }
    if (auto* box = g_windowMgr.m_roomMessageBox; box && box->HasCancel()
        && (box->GetMessageId() == 601 || box->GetMessageId() == 356))
        box->Dismiss(false);
    if (g_windowMgr.m_petInfoWnd) g_windowMgr.DeleteWindow(g_windowMgr.m_petInfoWnd);
    if (g_windowMgr.m_petTamingDeceiveWnd) g_windowMgr.DeleteWindow(g_windowMgr.m_petTamingDeceiveWnd);
    if (g_windowMgr.m_petEggListWnd) g_windowMgr.DeleteWindow(g_windowMgr.m_petEggListWnd);
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

        if (mode.m_world) {
            mode.m_world->NotifyActorDeleted(actor);
            mode.m_world->UnregisterActor(actor);
        }
        actor->UnRegisterPos();
        delete actor;
        it = mode.m_runtimeActors.erase(it);
    }

    mode.m_actorPosList.clear();
    mode.m_aidList.clear();
    mode.m_chatRoomOwners.clear();
    if(g_windowMgr.m_chatRoomWnd)g_windowMgr.DeleteWindow(g_windowMgr.m_chatRoomWnd);
    mode.m_actorNameList.clear();
    mode.m_actorNameReqTimer.clear();
    mode.m_actorNameListByGID.clear();
    mode.m_actorNameByGIDReqTimer.clear();
    mode.m_characterNameCache.clear();
    mode.m_characterNameReqTimer.clear();
    mode.m_partyPosList.clear();
    mode.m_guildPosList.clear();
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
    // The Ref same-map reload constructs a new player. The reused player
    // must retire the action queued against actors from the old scene.
    player->m_isAttackRequest = 0;
    player->m_proceedType.type = 0;
    mode.SynchronizePlayerSkillProceed(player, true);
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
    mode.m_isOnQuest = 0;
    mode.m_isPlayerDead = 0;
    mode.m_sentLoadEndAck = 0;
    mode.m_mapLoadingStage = CGameMode::MapLoading_None;
    mode.m_mapLoadingStartTick = 0;
    mode.m_mapLoadingAckTick = 0;
    mode.m_lastActorBootstrapPacketTick = GetTickCount();
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

bool IsLikelyPlayerGid(u32 gid);

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

int ResolveWorldActionDirFromRotation(float rotationDegrees)
{
    const int roundedDir = static_cast<int>(std::floor((rotationDegrees + 22.5f) / 45.0f));
    return roundedDir & 7;
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
    const int resolvedAction = baseAction + ResolveWorldActionDirFromRotation(actor->m_roty);
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

struct SkillFailNotice
{
    int messageId = -1;
    std::string message;
    int color = kSystemNoticeColor;
};

struct SkillFailNoticeSet
{
    std::array<SkillFailNotice, 2> notices{};
    std::size_t count = 0;
};

SkillFailNoticeSet ResolveSkillFailNotices(u16 skillId, s16 reason, u16 itemId, u8 cause, bool skillFailEnabled)
{
    SkillFailNoticeSet result;
    if (cause == 0) {
        if (skillId == 1) {
            static constexpr int basicSkillMessageIds[] = { 0x9F, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5 };
            static constexpr const char* basicSkillMessages[] = {
                "You haven't learned enough Basic Skills to Trade.",
                "You haven't learned enough Basic Skills to use Emotion icons.",
                "You haven't learned enough Basic Skills to Sit.",
                "You haven't learned enough Basic Skills to create a chat room.",
                "You haven't learned enough Basic Skills to Party.",
                "You haven't learned enough skills to Shout.",
                "You haven't learned enough skills for Pking.",
            };
            if (reason >= 0 && static_cast<std::size_t>(reason) < std::size(basicSkillMessageIds)) {
                result.notices[result.count++] = { basicSkillMessageIds[reason], basicSkillMessages[reason] };
            } else if (reason == 7) {
                result.notices[result.count++] = { 0x17F, "You haven't learned enough skills for aligning.", 255 };
                result.notices[result.count++] = { 0x518, "Insufficient Skill Level for joining a Party." };
            } else if (reason == 8) {
                result.notices[result.count++] = { 0x518, "Insufficient Skill Level for joining a Party." };
            }
        } else if (skillId == 27) {
            result.notices[result.count++] = { 0xD6, "Skill Level is not high enough." };
        } else if (skillId == 50) {
            result.notices[result.count++] = { 0xCD, "Steal has failed." };
        } else if (skillId == 52) {
            result.notices[result.count++] = { 0xCF, "Envenom skill has failed." };
        } else if (skillFailEnabled) {
            result.notices[result.count++] = { 0xCC, "Skill has failed." };
        }
        return result;
    }

    switch (cause) {
    case 1:
        result.notices[0] = { 0xCA, "Not Enough SP" };
        break;
    case 2:
        result.notices[0] = { 0xCB, "Not Enough HP" };
        break;
    case 3:
        result.notices[0] = { 0x328, "You do not have enough ingredients." };
        break;
    case 4:
        if (!skillFailEnabled) return result;
        result.notices[0] = { 0xDB, "There is a Delay after using a Skill." };
        break;
    case 5:
        result.notices[0] = { 0xE9, "Skill has failed because you do not have enough zeny." };
        break;
    case 6:
        result.notices[0] = { 0xEF, "The skill cannot be used with this weapon." };
        break;
    case 7:
        result.notices[0] = { 0xF6, "Red Gemstone required." };
        break;
    case 8:
        result.notices[0] = { 0xF7, "Blue Gemstone required." };
        break;
    case 9:
        result.notices[0] = { 0x244, "You can't use this Skill because you are over your Weight Limit." };
        break;
    case 10:
        if (!skillFailEnabled) return result;
        result.notices[0] = { 0x11D, "Skill has failed." };
        break;
    case 71: // rAthena USESKILL_FAIL_NEED_ITEM supplies a quantity and item ID.
        if (itemId == 0 || reason <= 0) return result;
        result.notices[0] = { -1, "[" + g_ttemmgr.GetDisplayName(itemId, true, false)
            + "] required '" + std::to_string(reason) + "' amount." };
        break;
    default:
        return result;
    }
    result.count = 1;
    return result;
}

void HandleSkillFailAck(CGameMode& mode, const PacketView& packet)
{
    ro::commerce::Receive(mode,packet);
    if (!packet.data || packet.packetLength < 10) {
        return;
    }

    const u16 skillId = ReadLE16(packet.data + 2);
    const s16 reason = static_cast<s16>(ReadLE16(packet.data + 4));
    const u16 itemId = ReadLE16(packet.data + 6);
    const u8 result = packet.data[8];
    const u8 cause = packet.data[9];
    // Ref Zc_Ack_Touseskill releases the local motion freeze for every result.
    if (mode.m_world != nullptr && mode.m_world->m_player != nullptr) {
        mode.m_world->m_player->m_isMotionFreezed = 0;
    }
    if (result != 0) {
        return;
    }

    if (mode.m_world != nullptr && mode.m_world->m_player != nullptr) {
        CPlayer* player = mode.m_world->m_player;
        // A rejected skill must not leave the swing or cast pose running.
        // The server did not spend SP and will not send a hit.
        const int state = player->m_stateId;
        if (state == 2 || state == 5 || state == 7 || state == 8 || state == 9 || state == 12) {
            player->SetState(0);
        }
    }
    if (cause == 0 && skillId == 1 && reason == 1) {
        mode.m_isReqEmotion = 0;
    }
    const SkillFailNoticeSet notices = ResolveSkillFailNotices(skillId, reason, itemId, cause, g_session.m_isSkillFail == 1);

    DbgLog("[GameMode] skill fail ack opcode=0x%04X skill=%u reason=%d item=%u result=%u cause=%u noticeCount=%u\n",
        packet.packetId,
        static_cast<unsigned int>(skillId),
        static_cast<int>(reason),
        static_cast<unsigned int>(itemId),
        static_cast<unsigned int>(result),
        static_cast<unsigned int>(cause),
        static_cast<unsigned int>(notices.count));
    for (std::size_t index = 0; index < notices.count; ++index) {
        const SkillFailNotice& notice = notices.notices[index];
        DbgLog("[GameMode] skill fail notice msgId=%d msg='%s'\n", notice.messageId, notice.message.c_str());
        g_windowMgr.SendMsg(kUiChatEventMsg,
            reinterpret_cast<msgparam_t>(notice.message.c_str()),
            notice.color);
    }
}

void HandleSkillMessage(CGameMode&, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 6) {
        return;
    }

    const char* message = nullptr;
    switch (ReadLE32(packet.data + 2)) {
    case 0x15:
        message = "All abnormal status effects have been removed.";
        break;
    case 0x16:
        message = "You will be immune to abnormal status effects for the next minute.";
        break;
    case 0x17:
        message = "Your Max HP will stay increased for the next minute.";
        break;
    case 0x18:
        message = "Your Max SP will stay increased for the next minute.";
        break;
    case 0x19:
        message = "All of your Stats will stay increased for the next minute.";
        break;
    case 0x1C:
        message = "Your weapon will remain blessed with Holy power for the next minute.";
        break;
    case 0x1D:
        message = "Your armor will remain blessed with Holy power for the next minute.";
        break;
    case 0x1E:
        message = "Your Defense will stay increased for the next 10 seconds.";
        break;
    case 0x1F:
        message = "Your Attack strength will be increased for the next minute.";
        break;
    case 0x20:
        message = "Your Accuracy and Flee Rate will be increased for the next minute.";
        break;
    case 0x28:
        // Decoded from the canonical executable's CP949 MSI_FULLSTRIP resource:
        // "완벽하게 코팅되어있어서 풀스트립이 통하지 않습니다."
        message = "This target is fully coated, so Full Strip will not work.";
        break;
    default:
        return;
    }

    g_windowMgr.PushChatEvent(message, 10198015u, 7);
}

bool SessionIsAttack(int action)
{
    return action == 0 || action == 4;
}

CGameActor* FindActorByPacketGid(CWorld& world, u32 gid)
{
    for (CGameActor* actor : world.m_actorList) {
        if (actor != nullptr && actor->m_gid == gid) {
            return actor;
        }
    }
    return nullptr;
}

void HandleActorActionNotify(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || mode.m_world == nullptr) {
        return;
    }

    const bool isNotifyAct2 = ro::net::IsActiveActorActionNotifyExtendedPacket(packet.packetId);
    const size_t minLength = isNotifyAct2 ? 33u : 29u;
    if (packet.packetLength < minLength) {
        return;
    }

    const u32 srcGid = ReadLE32(packet.data + 2);
    const u32 targetGid = ReadLE32(packet.data + 6);
    const u32 startTime = ReadLE32(packet.data + 10);
    int attackMT = static_cast<int>(ReadLE32(packet.data + 14));
    int attackedMT = static_cast<int>(ReadLE32(packet.data + 18));
    const int damage = isNotifyAct2
        ? static_cast<int>(ReadLE32(packet.data + 22))
        : static_cast<int>(static_cast<int16_t>(ReadLE16(packet.data + 22)));
    const int count = isNotifyAct2
        ? static_cast<int>(static_cast<int16_t>(ReadLE16(packet.data + 26)))
        : static_cast<int>(static_cast<int16_t>(ReadLE16(packet.data + 24)));
    const int action = packet.data[isNotifyAct2 ? 28 : 26];
    const int leftDamage = isNotifyAct2
        ? static_cast<int>(ReadLE32(packet.data + 29))
        : static_cast<int>(static_cast<int16_t>(ReadLE16(packet.data + 27)));

    if (damage == -30000) {
        return;
    }
    if (attackMT == 0) {
        attackMT = 1440;
    }
    if (attackedMT == 0 && !SessionIsAttack(action)) {
        attackedMT = 1440;
    }

    int actionBlock[10] = {};
    actionBlock[0] = damage;
    actionBlock[1] = leftDamage;
    actionBlock[2] = action;
    actionBlock[3] = static_cast<int>(targetGid);
    actionBlock[4] = static_cast<int>(startTime);
    actionBlock[5] = count;
    actionBlock[6] = -1;
    actionBlock[7] = -1;
    actionBlock[8] = attackMT;
    actionBlock[9] = attackedMT;

    CGameActor* actor = srcGid == g_session.m_aid
        ? static_cast<CGameActor*>(mode.m_world->m_player)
        : FindActorByPacketGid(*mode.m_world, srcGid);
    if (actor != nullptr) {
        actor->SendMsg(actor, 11, reinterpret_cast<msgparam_t>(actionBlock), 0, 0);
    }
}

void ApplyMapChangeSessionState(const MapChangeInfo& info)
{
    g_session.InvalidatePartyPositions();
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

    std::array<u8, sizeof(PACKET_CZ_ENTER_PACKETVER22)> enterPacket{};
    int enterPacketLength = 0;
    if (!ro::net::BuildActiveWantToConnectionPacket(
            g_session.m_aid,
            g_session.m_gid,
            g_session.m_authCode,
            timeGetTime(),
            g_session.m_sex,
            enterPacket.data(),
            static_cast<int>(enterPacket.size()),
            &enterPacketLength)) {
        DbgLog("[GameMode] map change reconnect failed to build wanttoconnection packet map='%s'\n",
            info.mapName.c_str());
        return false;
    }

    const u16 enterOpcode = static_cast<u16>(enterPacket[0]) | (static_cast<u16>(enterPacket[1]) << 8);
    const bool sent = CRagConnection::instance()->SendPacket(
        reinterpret_cast<const char*>(enterPacket.data()),
        enterPacketLength);
    DbgLog("[GameMode] map change reconnect zone=%s:%u map='%s' opcode=0x%04X sentEnter=%d\n",
        serverHost,
        static_cast<unsigned int>(info.port),
        info.mapName.c_str(),
        enterOpcode,
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

    if (!ro::net::IsActiveBroadcastPacket(packet.packetId)) {
        return payload;
    }

    if (ro::net::IsActiveColoredBroadcastPacket(packet.packetId)) {
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


static bool TryPlayEffectWaveAtPosition(const vector3d& soundPos, const std::string& relativePath)
{
    CGameMode* mode = g_modeMgr.GetCurrentGameMode();
    if (!mode || !mode->m_world || !mode->m_world->m_player) {
        return false;
    }
    const vector3d& listener = mode->m_world->m_player->m_pos;
    return PlayWave(relativePath.c_str(), soundPos.x - listener.x, soundPos.y - listener.y,
        soundPos.z - listener.z);
}


float ResolveActorHeight(const CWorld* world, float worldX, float worldZ)
{
    if (world && world->m_attr) {
        return world->m_attr->GetHeight(worldX, worldZ);
    }
    return 0.0f;
}

bool IsMonsterSkillActor(const CGameActor* actor);

void HandleSkillCastAck(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 24) {
        return;
    }

    const u32 srcGid = ReadLE32(packet.data + 2);
    const u32 dstGid = ReadLE32(packet.data + 6);
    const int cellX = static_cast<int>(static_cast<int16_t>(ReadLE16(packet.data + 10)));
    const int cellY = static_cast<int>(static_cast<int16_t>(ReadLE16(packet.data + 12)));
    const u16 skillId = ReadLE16(packet.data + 14);
    const u32 property = ReadLE32(packet.data + 16);
    const int castTimeMs = static_cast<int>(ReadLE32(packet.data + 20));

    CGameActor* sourceActor = ResolveSkillPacketActor(mode, srcGid);
    if (sourceActor == nullptr) {
        DbgLog("[GameMode] skill cast ack unresolved src=%u dst=%u skill=%u\n",
            srcGid,
            dstGid,
            static_cast<unsigned int>(skillId));
        return;
    }

    // Modern servers can announce a zero-time cast before checking ammunition.
    // The cast announcement is not a successful use: wait for the skill result
    // to animate instant local skills. Real timed casts keep their wind-up.
    if (castTimeMs <= 0 && mode.m_world && sourceActor == mode.m_world->m_player) {
        DbgLog("[GameMode] instant cast announcement deferred src=%u skill=%u\n", srcGid, skillId);
        return;
    }

    int beginEffectId = 16;
    int motionType = 8;
    GetSkillActionInfo2(static_cast<int>(skillId), beginEffectId, motionType, static_cast<int>(property), sourceActor->m_job);

    if (skillId == 62 || skillId == 57) {
        sourceActor->SetState(motionType);
        sourceActor->m_isMotionFreezed = 1;
        sourceActor->m_freezeEndTick = static_cast<u32>(castTimeMs) + timeGetTime();
    } else {
        sourceActor->SetState(motionType);
        sourceActor->SendMsg(sourceActor, 85, beginEffectId, castTimeMs >> 4, 0);
        sourceActor->SendMsg(sourceActor, 82, castTimeMs, 0, 0);
        if (IsMonsterSkillActor(sourceActor)) {
            int act = -1;
            int loopType = 0;
            const char* jobName = g_session.GetJobName(sourceActor->m_job);
            g_session.GetMonsterSkillInfo(jobName != nullptr ? jobName : "", skillId, act, loopType);
            if (act != -1) {
                sourceActor->SetAction(8 * act - 8, 1, loopType == 0 ? 1 : 0);
                if (skillId == 193) {
                    sourceActor->m_motionSpeed = static_cast<float>(castTimeMs / 300);
                }
            }
        } else {
            // Ref Zc_useskill_ack: only wedding announcements use the target AID.
            mode.SendMsg(99,
                static_cast<msgparam_t>(skillId),
                reinterpret_cast<msgparam_t>(sourceActor),
                static_cast<msgparam_t>((skillId == 334 || skillId == 335) ? dstGid : 0));
        }
    }

    if (dstGid != 0) {
        CGameActor* targetActor = ResolveSkillPacketActor(mode, dstGid);
        if (targetActor != nullptr && targetActor != sourceActor) {
            const bool mercenaryOrHomun = targetActor->m_job > 6000 && targetActor->m_job < 6048;
            const bool pkOrSiege = mode.m_world != nullptr
                && (mode.m_world->m_isPKZone != 0 || mode.m_world->m_isSiegeMode != 0);
            if ((!mercenaryOrHomun && targetActor->m_objectType != 9)
                || !SKILL_INFO::IsGoodSkillForActor(skillId)
                || pkOrSiege) {
                sourceActor->SendMsg(sourceActor, 81, dstGid, 0, 0);
            }
            FaceActorTowardPosition(sourceActor, targetActor->m_pos);
            targetActor->SendMsg(targetActor, 86, 60, castTimeMs >> 4, 0);
            if (skillId == 490) {
                if (CRagEffect* effect = sourceActor->LaunchEffect(539, vector3d{}, 0.0f)) {
                    effect->SendMsg(effect, 14, reinterpret_cast<msgparam_t>(&targetActor->m_pos), 0, 0);
                }
            }
        }
    } else if (mode.m_world != nullptr) {
        float destX = 0.0f;
        float destZ = 0.0f;
        mode.GetClientCoor(cellX, cellY, destX, destZ);
        FaceActorTowardPosition(sourceActor, vector3d{ destX, sourceActor->m_pos.y, destZ });
        CGroundScope* scope = new CGroundScope();
        scope->SendMsg(nullptr, 22, static_cast<msgparam_t>(srcGid), 0, 0);
        scope->SendMsg(nullptr, 14, cellX, cellY, 0);
        scope->SendMsg(nullptr, 115, static_cast<msgparam_t>(static_cast<u32>(castTimeMs) + timeGetTime()), 0, 0);
        scope->SendMsg(nullptr, 117, skillId, 0, 0);
        mode.m_world->m_gameObjectList.push_back(scope);
    }

    if (skillId == 401) {
        sourceActor->SetForceAnimation(96, 0, 0, 96, 0, 0, 0, 0);
        sourceActor->LaunchEffect(402, vector3d{}, 0.0f);
    } else if (skillId == 268) {
        sourceActor->SetForceAnimation(96, 0, 0, 96, 0, 1, static_cast<u32>(castTimeMs) + 1000u, 0);
    } else if (skillId == 261 || skillId == 1015) {
        sourceActor->SetForceAnimation(96, 0, 0, 96, 0, 0, 0, 0);
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

struct SkillUseArgs {
    int skillId = 0;
    int damage = 0;
    int action = 0;
    u32 targetGid = 0;
    u32 startTime = 0;
    int level = 0;
    int count = 0;
    int cellX = -1;
    int cellY = -1;
    int attackMT = 0;
    int attackedMT = 0;
};

struct GroundSkillArgs {
    int skillId = 0;
    int startTime = 0;
    int cellX = 0;
    int cellY = 0;
    int level = 0;
};

struct AttrStateArgs {
    int bodyState = 0;
    int effectState = 0;
    int healthState = 0;
    int pkState = 0;
};

bool IsMonsterSkillActor(const CGameActor* actor)
{
    return actor != nullptr
        && actor->m_objectType != 9
        && actor->m_job > 1000
        && actor->m_job < 4000;
}

void ApplyMonsterSkillMotion(CGameActor* source, int skillId)
{
    if (!IsMonsterSkillActor(source)) {
        return;
    }

    int act = -1;
    int loopType = 0;
    const char* jobName = g_session.GetJobName(source->m_job);
    g_session.GetMonsterSkillInfo(jobName != nullptr ? jobName : "", skillId, act, loopType);
    if (act == -1 || source->m_gid == g_session.m_aid) {
        return;
    }
    source->SetAction(8 * act - 8, 1, loopType == 0 ? 1 : 0);
}

void DeliverSkillUse(CGameMode& mode, const SkillUseArgs& args, u32 sourceGid, bool skill2Extras)
{
    CGameActor* source = ResolveSkillPacketActor(mode, sourceGid);
    CGameActor* target = args.targetGid != 0 ? ResolveSkillPacketActor(mode, args.targetGid) : nullptr;
    if (source != nullptr) {
        source->SendMsg(source, 37, reinterpret_cast<msgparam_t>(&args), 0, 0);
    } else if (mode.m_world != nullptr) {
        if (CSkill* skill = ResolveSkillPacketUnit(mode, sourceGid)) {
            skill->SendMsg(skill, 37, reinterpret_cast<msgparam_t>(&args), 0, 0);
        }
    }
    if (skill2Extras && source != nullptr && target != nullptr) {
        if (args.skillId == 141 && static_cast<u16>(args.level) == 0xFFFFu) {
            target->LaunchEffect(129, vector3d{}, 0.0f);
        } else if (args.skillId == 662 && source != target) {
            target->LaunchEffect(372, vector3d{}, 0.0f);
        }
    }
    if (IsMonsterSkillActor(source)) {
        ApplyMonsterSkillMotion(source, args.skillId);
    } else {
        mode.SendMsg(99,
            static_cast<msgparam_t>(args.skillId),
            reinterpret_cast<msgparam_t>(source),
            static_cast<msgparam_t>(args.level));
    }
}

void LaunchGroundArrowShower(CGameMode& mode, CGameActor* source, int cellX, int cellY)
{
    if (source == nullptr || mode.m_world == nullptr || mode.m_world->m_attr == nullptr) {
        return;
    }

    mode.MakeArrowEffectPos(source->m_pos, source->m_pos, 18.0f);
    static const int kNeighborCells[8][2] = {
        { 1, 0 }, { 1, 1 }, { 1, -1 }, { -1, 0 },
        { -1, 1 }, { -1, -1 }, { 0, 1 }, { 0, -1 },
    };
    for (int index = 0; index < 8; ++index) {
        float clientX = 0.0f;
        float clientZ = 0.0f;
        mode.GetClientCoor(cellX + kNeighborCells[index][0], cellY + kNeighborCells[index][1], clientX, clientZ);
        const vector3d cellPos{
            clientX,
            mode.m_world->m_attr->GetHeight(clientX, clientZ),
            clientZ,
        };
        mode.MakeArrowEffectPos(source->m_pos, cellPos, 18.0f);
    }
}

void HandleSkillDamageNotify(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data
        || (!ro::net::IsActiveSkillDamageNotifyExtendedPacket(packet.packetId) && packet.packetLength < 31)
        || (ro::net::IsActiveSkillDamageNotifyExtendedPacket(packet.packetId) && packet.packetLength < 33)) {
        return;
    }

    const bool isLongDamage = ro::net::IsActiveSkillDamageNotifyExtendedPacket(packet.packetId);
    const u16 skillId = ReadLE16(packet.data + 2);
    const u32 srcGid = ReadLE32(packet.data + 4);
    const u32 dstGid = ReadLE32(packet.data + 8);
    const int attackMT = static_cast<int>(ReadLE32(packet.data + 16));
    const int attackedMT = static_cast<int>(ReadLE32(packet.data + 20));
    const int damage = isLongDamage
        ? static_cast<int>(ReadLE32(packet.data + 24))
        : static_cast<int>(static_cast<int16_t>(ReadLE16(packet.data + 24)));
    const int level = static_cast<int>(static_cast<int16_t>(ReadLE16(packet.data + (isLongDamage ? 28 : 26))));
    const int div = static_cast<int>(static_cast<int16_t>(ReadLE16(packet.data + (isLongDamage ? 30 : 28))));
    const u8 actionType = packet.data[isLongDamage ? 32 : 30];

    SkillUseArgs args{};
    args.skillId = skillId;
    args.damage = damage;
    args.action = actionType;
    args.targetGid = dstGid;
    args.startTime = ReadLE32(packet.data + 12);
    args.level = level;
    args.count = div;
    args.cellX = -1;
    args.cellY = -1;
    args.attackMT = attackMT;
    args.attackedMT = attackedMT;
    DeliverSkillUse(mode, args, srcGid, isLongDamage);

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

    if (packet.packetId != ro::net::GetActiveMapReceiveProfile().skillDamagePositionNotify) {
        return;
    }

    const u16 skillId = ReadLE16(packet.data + 2);
    const u32 srcGid = ReadLE32(packet.data + 4);
    const u32 dstGid = ReadLE32(packet.data + 8);
    const int attackMT = static_cast<int>(ReadLE32(packet.data + 16));
    const int attackedMT = static_cast<int>(ReadLE32(packet.data + 20));
    const int cellX = static_cast<int>(static_cast<int16_t>(ReadLE16(packet.data + 24)));
    const int cellY = static_cast<int>(static_cast<int16_t>(ReadLE16(packet.data + 26)));
    const int damage = static_cast<int>(static_cast<int16_t>(ReadLE16(packet.data + 28)));
    const u16 level = ReadLE16(packet.data + 30);
    const u16 div = ReadLE16(packet.data + 32);
    const u8 actionType = packet.data[34];

    SkillUseArgs args{};
    args.skillId = skillId;
    args.damage = damage;
    args.action = actionType;
    args.targetGid = dstGid;
    args.startTime = ReadLE32(packet.data + 12);
    args.level = static_cast<int>(static_cast<int16_t>(level));
    args.count = static_cast<int>(static_cast<int16_t>(div));
    args.cellX = cellX;
    args.cellY = cellY;
    args.attackMT = attackMT;
    args.attackedMT = attackedMT;
    DeliverSkillUse(mode, args, srcGid, false);

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

    if (packet.packetId != ro::net::GetActiveMapReceiveProfile().groundSkillNotify) {
        return;
    }

    const u16 skillId = ReadLE16(packet.data + 2);
    const u32 srcGid = ReadLE32(packet.data + 4);
    const int level = static_cast<int>(static_cast<int16_t>(ReadLE16(packet.data + 8)));
    const int cellX = static_cast<int>(static_cast<int16_t>(ReadLE16(packet.data + 10)));
    const int cellY = static_cast<int>(static_cast<int16_t>(ReadLE16(packet.data + 12)));

    GroundSkillArgs args{};
    args.skillId = skillId;
    args.startTime = static_cast<int>(ReadLE32(packet.data + 14));
    args.level = level;
    args.cellX = cellX;
    args.cellY = cellY;

    CGameActor* sourceActor = ResolveSkillPacketActor(mode, srcGid);
    if (sourceActor != nullptr) {
        sourceActor->SendMsg(sourceActor, 38, reinterpret_cast<msgparam_t>(&args), 0, 0);
    }
    if (!IsMonsterSkillActor(sourceActor)) {
        mode.SendMsg(99,
            static_cast<msgparam_t>(skillId),
            reinterpret_cast<msgparam_t>(sourceActor),
            static_cast<msgparam_t>(level));
        if (skillId == 47) {
            LaunchGroundArrowShower(mode, sourceActor, cellX, cellY);
        } else if (skillId == 362 || skillId == 369) {
            const vector3d origin = mode.m_world->m_player != nullptr ? mode.m_world->m_player->m_pos : vector3d{};
            TryPlayEffectWaveAtPosition(origin,
                skillId == 362 ? "effect\\\xB0\xA1\xBD\xBA\xC6\xE7.wav" : "effect\\\xB9\xD9\xBD\xC7\xB8\xAE\xC4\xAB.wav");
        }
    } else {
        int act = -1;
        int loopType = 0;
        const char* jobName = g_session.GetJobName(sourceActor->m_job);
        g_session.GetMonsterSkillInfo(jobName != nullptr ? jobName : "", skillId, act, loopType);
    }

    DbgLog("[GameMode] ground skill src=%u skill=%u level=%u cell=(%d,%d)\n",
        srcGid,
        static_cast<unsigned int>(skillId),
        static_cast<unsigned int>(level),
        cellX,
        cellY);
}

bool EffectExists(CAbleToMakeEffect* actor, int effectId)
{
    if (actor == nullptr) {
        return false;
    }
    for (const CRagEffect* effect : actor->m_effectList) {
        if (effect != nullptr && effect->GetEffectType() == effectId) {
            return true;
        }
    }
    return false;
}

CRagEffect* LaunchOn(CAbleToMakeEffect* actor, int effectId)
{
    if (actor == nullptr) {
        return nullptr;
    }
    return actor->LaunchEffect(effectId, vector3d{}, 0.0f);
}

void PlayUseSkillWave(const char* name, float offsetX, float offsetY, float offsetZ)
{
    vector3d origin{};
    if (g_modeMgr.GetCurrentGameMode() != nullptr
        && g_modeMgr.GetCurrentGameMode()->m_world != nullptr
        && g_modeMgr.GetCurrentGameMode()->m_world->m_player != nullptr) {
        origin = g_modeMgr.GetCurrentGameMode()->m_world->m_player->m_pos;
    }
    origin.x += offsetX;
    origin.y += offsetY;
    origin.z += offsetZ;
    TryPlayEffectWaveAtPosition(origin, name != nullptr ? name : "");
}

void PlayUseSkillWaveAt(const vector3d& position, const char* name)
{
    TryPlayEffectWaveAtPosition(position, name != nullptr ? name : "");
}

#include "UseSkillEffect.generated.inc"

bool UseSkillFacesCell(int skillId)
{
    switch (skillId) {
    case 12:
    case 18:
    case 21:
    case 70:
    case 79:
    case 83:
    case 85:
    case 89:
    case 110:
    case 478:
        return true;
    default:
        return false;
    }
}

void DeliverUseSkill(CGameMode& mode, u32 sourceGid, u32 targetGid, int skillId, int rawLevel, bool success)
{
    CGameActor* source = sourceGid != 0 ? ResolveSkillPacketActor(mode, sourceGid) : nullptr;
    bool monsterAction = false;
    if (source != nullptr) {
        if (skillId == 249 || skillId == 8220) {
            source->SetState(2);
        }
        if (!IsMonsterSkillActor(source) && skillId != 9) {
            const msgparam_t levelArg = (skillId == 334 || skillId == 335)
                ? static_cast<msgparam_t>(targetGid)
                : static_cast<msgparam_t>(rawLevel);
            mode.SendMsg(99,
                static_cast<msgparam_t>(skillId),
                reinterpret_cast<msgparam_t>(source),
                levelArg);
        } else {
            int act = -1;
            int loopType = 0;
            const char* jobName = g_session.GetJobName(source->m_job);
            g_session.GetMonsterSkillInfo(jobName != nullptr ? jobName : "", skillId, act, loopType);
            if (act != -1) {
                source->SetAction(8 * act - 8, 1, loopType == 0 ? 1 : 0);
                monsterAction = true;
            }
        }
    }
    // Skill 135 is Cloaking. Ref still returns motion 7, the same cast pose
    // Hide uses, and state 7 blocks walking until that pose finishes. Cloak
    // only changes the option bit; it does not play that pose or its wave.
    if (!monsterAction && skillId != 135 && skillId != 263 && skillId != 272 && skillId != 273 && skillId != 371 && source != nullptr) {
        int beginEffectId = 16;
        int motion = 7;
        GetSkillActionInfo(skillId, beginEffectId, motion);
        if (source->m_stateId == 1 || motion != 1) {
            source->SetState(motion);
        } else {
            source->SetState(7);
        }
    }

    CGameActor* target = nullptr;
    if (UseSkillFacesCell(skillId)) {
        if (source != nullptr) {
            float cellX = 0.0f;
            float cellZ = 0.0f;
            mode.GetClientCoor(static_cast<int>(static_cast<unsigned short>(targetGid >> 16)),
                static_cast<int>(static_cast<unsigned short>(targetGid)),
                cellX,
                cellZ);
            FaceActorTowardPosition(source, vector3d{ cellX, source->m_pos.y, cellZ });
        }
    } else if (skillId == 124 || skillId == 131) {
        if (source != nullptr && mode.m_world != nullptr) {
            if (CSkill* skill = ResolveSkillPacketUnit(mode, targetGid)) {
                FaceActorTowardPosition(source, skill->m_pos);
            }
        }
    } else {
        if (targetGid != 0) {
            target = ResolveSkillPacketActor(mode, targetGid);
        }
        if (source != nullptr && target != nullptr && source != target) {
            FaceActorTowardPosition(source, target->m_pos);
        }
    }

    if (success) {
        ApplyUseSkillEffects(mode, source, target, skillId, rawLevel, sourceGid, targetGid);
    }
}

void HandleSkillNoDamageNotify(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 15) {
        return;
    }

    if (packet.packetId != ro::net::GetActiveMapReceiveProfile().skillNoDamageNotify) {
        return;
    }

    const u16 skillId = ReadLE16(packet.data + 2);
    const int rawLevel = static_cast<int>(static_cast<int16_t>(ReadLE16(packet.data + 4)));
    const u32 dstGid = ReadLE32(packet.data + 6);
    const u32 srcGid = ReadLE32(packet.data + 10);
    const u8 result = packet.data[14];

    if (skillId == 244 && srcGid == g_session.m_aid) {
        mode.DeactivateHomunculus();
    }

    DeliverUseSkill(mode, srcGid, dstGid, skillId, rawLevel, result != 0);

    DbgLog("[GameMode] skill nodamage src=%u dst=%u skill=%u value=%d result=%u\n",
        srcGid,
        dstGid,
        static_cast<unsigned int>(skillId),
        rawLevel,
        static_cast<unsigned int>(result));
}

void RememberVisibleTrap(CGameMode& mode, u32 unitAid, u32 creatorAid, u8& visible)
{
    visible = 1;
    std::list<u32>& traps = mode.m_visibleTrapList;
    if (creatorAid == g_session.m_aid || traps.empty()) {
        traps.push_back(unitAid);
        return;
    }
    for (auto it = traps.begin(); it != traps.end(); ++it) {
        if (*it == unitAid) {
            traps.erase(it);
            visible = 0;
            return;
        }
    }
    traps.push_back(unitAid);
}

void PlaceSkillUnit(CGameMode& mode, u32 unitAid, u32 creatorAid, int job, int cellX, int cellY, u8 visible)
{
    if (mode.m_world == nullptr) {
        return;
    }
    CSkill* skill = ResolveSkillPacketUnit(mode, unitAid);
    if (skill == nullptr) {
        if (job < 0x2D) {
            return;
        }
        skill = new CSkill();
        mode.m_world->m_gameObjectList.push_back(skill);
        mode.m_world->m_skillList.push_back(skill);
    }
    // Modern rAthena owns this unit's lifetime through ZC_SKILL_DISAPPEAR.
    // The legacy three-second fire-wall timer applies only to synthetic units.
    skill->m_serverOwnedUnit = true;
    SkillStandEntry entry;
    entry.aid = unitAid;
    entry.creatorAid = creatorAid;
    entry.job = job;
    entry.cellX = cellX;
    entry.cellY = cellY;
    entry.isVisible = visible;
    skill->SendMsg(skill, 45, reinterpret_cast<msgparam_t>(&entry), 0, 0);
}

void HandleSkillUnitSet(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 16 || mode.m_world == nullptr) {
        return;
    }
    if (packet.packetId != ro::net::GetActiveMapReceiveProfile().skillUnitSetBasic) {
        return;
    }

    const u32 unitAid = ReadLE32(packet.data + 2);
    const u32 creatorAid = ReadLE32(packet.data + 6);
    const int cellX = static_cast<int>(static_cast<int16_t>(ReadLE16(packet.data + 10)));
    const int cellY = static_cast<int>(static_cast<int16_t>(ReadLE16(packet.data + 12)));
    const int job = packet.data[14];
    u8 visible = 1;
    RememberVisibleTrap(mode, unitAid, creatorAid, visible);
    PlaceSkillUnit(mode, unitAid, creatorAid, job, cellX, cellY, visible);
}

void HandleSkillUnitSet2(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 97 || mode.m_world == nullptr) {
        return;
    }
    if (packet.packetId != ro::net::GetActiveMapReceiveProfile().skillUnitSetExtended) {
        return;
    }
    if (packet.data[16] == 1) {
        const u32 unitAid = ReadLE32(packet.data + 2);
        const int cellX = static_cast<int>(static_cast<int16_t>(ReadLE16(packet.data + 10)));
        const int cellY = static_cast<int>(static_cast<int16_t>(ReadLE16(packet.data + 12)));
        char text[80];
        std::memcpy(text, packet.data + 17, 79);
        text[79] = '\0';
        CGraffiSkill* graffiti = new CGraffiSkill();
        mode.m_world->m_gameObjectList.push_back(graffiti);
        mode.m_world->m_skillList.push_back(graffiti);
        graffiti->SendMsg(graffiti, 22, static_cast<msgparam_t>(unitAid), 0, 0);
        graffiti->SendMsg(graffiti, 117, reinterpret_cast<msgparam_t>(text), cellX, cellY);
        return;
    }

    const u32 unitAid = ReadLE32(packet.data + 2);
    const u32 creatorAid = ReadLE32(packet.data + 6);
    const int cellX = static_cast<int>(static_cast<int16_t>(ReadLE16(packet.data + 10)));
    const int cellY = static_cast<int>(static_cast<int16_t>(ReadLE16(packet.data + 12)));
    const int job = packet.data[14];
    u8 visible = 1;
    RememberVisibleTrap(mode, unitAid, creatorAid, visible);
    PlaceSkillUnit(mode, unitAid, creatorAid, job, cellX, cellY, visible);
}

void CloseSkillSelectWindow()
{
    if (g_windowMgr.m_skillSelectWnd) {
        g_windowMgr.DeleteWindow(g_windowMgr.m_skillSelectWnd);
    }
}

bool IsRandomOnlyTeleport(u16 skillId, const std::vector<std::string>& destinations)
{
    if (skillId != 26 || destinations.size() != 1) {
        return false;
    }
    const std::string& name = destinations.front();
    return name.size() == 6
        && (name[0] == 'R' || name[0] == 'r')
        && (name[1] == 'A' || name[1] == 'a')
        && (name[2] == 'N' || name[2] == 'n')
        && (name[3] == 'D' || name[3] == 'd')
        && (name[4] == 'O' || name[4] == 'o')
        && (name[5] == 'M' || name[5] == 'm');
}

void ShowWarpDestinationWindow(CGameMode& mode)
{
    if (IsRandomOnlyTeleport(mode.m_warpSkillId, mode.m_warpMapDestinations)) {
        const std::string randomName = mode.m_warpMapDestinations.front();
        if (mode.SendWarpMapSelection(randomName)) {
            CloseSkillSelectWindow();
            DbgLog("[GameMode] teleport level 1 random warp sent map='%s'\n", randomName.c_str());
            return;
        }
    }

    UISkillSelectWnd* const window = dynamic_cast<UISkillSelectWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_SKILLSELECTWND));
    if (!window) {
        return;
    }
    window->SetWarpDestinations(mode.m_warpSkillId, mode.m_warpMapDestinations);
    DbgLog("[GameMode] warp list window skill=%u destinations=%u\n",
        static_cast<unsigned int>(mode.m_warpSkillId),
        static_cast<unsigned int>(mode.m_warpMapDestinations.size()));
}

void HandleWarpMapList(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetId != 0x011C || packet.packetLength < 68) {
        return;
    }

    mode.m_warpSkillId = ReadLE16(packet.data + 2);
    mode.m_warpMapDestinations.clear();
    for (int index = 0; index < 4; ++index) {
        const char* field = reinterpret_cast<const char*>(packet.data + 4 + index * 16);
        std::size_t length = 0;
        while (length < 16 && field[length] != '\0') {
            ++length;
        }
        if (length != 0) {
            mode.m_warpMapDestinations.emplace_back(field, length);
        }
    }

    if (mode.m_warpMapDestinations.empty()) {
        mode.m_warpSkillId = 0;
        if (g_windowMgr.m_skillSelectWnd && g_windowMgr.m_skillSelectWnd->IsWarpSelection()) {
            CloseSkillSelectWindow();
        }
        return;
    }
    ShowWarpDestinationWindow(mode);
}

// 0x011E ZC_ACK_REMEMBER_WARPPOINT, selected 20080910 layout: type at byte 2.
void HandleMemoResult(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetId != 0x011E || packet.packetLength < 3) {
        return;
    }

    unsigned int messageId = 0;
    u32 colorRef = 0;
    const char* fallback = "";
    switch (packet.data[2]) {
    case 0: // rAthena WARPPOINT_SUCCESS / Ref MSI_WARPPOINTSTORED
        messageId = 217;
        colorRef = 0x00FFFF00u; // RGB(0, 255, 255), original cyan
        fallback = "Saved location as a Memo Point for Warp Skill.";
        break;
    case 1: // rAthena WARPPOINT_LOW_LEVEL / Ref MSI_NOTENOUGHSKILLLEVEL
        messageId = 214;
        colorRef = 0x000000FFu; // RGB(255, 0, 0), original red
        fallback = "Skill Level is not high enough.";
        break;
    case 2: // rAthena WARPPOINT_NOT_LEARNED / Ref MSI_NOWARPSKILL
        messageId = 216;
        colorRef = 0x000000FFu;
        fallback = "You haven't learned Warp.";
        break;
    default:
        return;
    }

    ro::UiMessageStrings strings;
    strings.Load();
    const std::string message = strings.Get(messageId, fallback);
    RecordChat(mode, message, ro::chat::ColorRefToUiRgb(colorRef), kChatChannelSystem);
}

void HandleMonsterEstimate(CGameMode&, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 29) return;

    UIMonsterInfoWnd::Estimate estimate;
    estimate.job = ReadLE16(packet.data + 2);
    if ((estimate.job >= 1285 && estimate.job <= 1287) || estimate.job == 1829 || estimate.job == 1830) return;
    estimate.level = ReadLE16(packet.data + 4);
    estimate.size = ReadLE16(packet.data + 6);
    estimate.hp = ReadLE32(packet.data + 8);
    estimate.defense = static_cast<s16>(ReadLE16(packet.data + 12));
    estimate.race = ReadLE16(packet.data + 14);
    estimate.magicDefense = static_cast<s16>(ReadLE16(packet.data + 16));
    estimate.element = ReadLE16(packet.data + 18);
    std::copy_n(packet.data + 20, estimate.rates.size(), estimate.rates.begin());

    auto* window = static_cast<UIMonsterInfoWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_MONSTERINFOWND));
    if (window) window->SetEstimate(estimate);
}

void HandleMakingArrowList(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetId != 0x01AD || packet.packetLength < 4 || ((packet.packetLength - 4) % 2) != 0) {
        return;
    }

    std::vector<unsigned int> itemIds;
    const int count = (packet.packetLength - 4) / 2;
    itemIds.reserve(static_cast<size_t>(count));
    for (int index = 0; index < count; ++index) {
        const unsigned int itemId = static_cast<unsigned int>(ReadLE16(packet.data + 4 + index * 2));
        if (itemId != 0) {
            itemIds.push_back(itemId);
        }
    }

    if (itemIds.empty()) {
        if (g_windowMgr.m_skillSelectWnd && g_windowMgr.m_skillSelectWnd->IsMakingArrowSelection()) {
            CloseSkillSelectWindow();
        }
        return;
    }

    mode.m_warpSkillId = 0;
    mode.m_warpMapDestinations.clear();
    UISkillSelectWnd* const window = dynamic_cast<UISkillSelectWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_SKILLSELECTWND));
    if (!window) {
        return;
    }
    window->SetMakingArrowItems(itemIds);
    DbgLog("[GameMode] making arrow list count=%d\n", static_cast<int>(itemIds.size()));
}

void HandleSkillDisappear(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 6 || mode.m_world == nullptr) {
        return;
    }
    mode.m_world->RemoveSkill(ReadLE32(packet.data + 2));
}

void HandleSkillUpdate(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 6 || mode.m_world == nullptr) {
        return;
    }
    CSkill* skill = ResolveSkillPacketUnit(mode, ReadLE32(packet.data + 2));
    if (skill != nullptr && skill->m_job > 138 && skill->m_job <= 153) {
        skill->SendMsg(skill, 119, 0, 0, 0);
    }
}

void HandleTalkboxChatContents(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 86 || mode.m_world == nullptr) {
        return;
    }

    CSkill* skill = ResolveSkillPacketUnit(mode, ReadLE32(packet.data + 2));
    if (skill != nullptr) {
        std::array<char, 81> text{};
        std::memcpy(text.data(), packet.data + 6, text.size() - 1);
        skill->SendMsg(skill, 7, reinterpret_cast<msgparam_t>(text.data()), 0, 0);
    }
}

void HandleGuildEmblem(CGameMode& mode, const PacketView& packet)
{
    // ZC_GUILD_EMBLEM_IMG: header, guild id, version, zlib-compressed BMP.
    if(!packet.data || packet.packetLength<=12)return;
    const u32 guild=ReadLE32(packet.data+4),version=ReadLE32(packet.data+8);
    if(!guild)return;
    std::vector<unsigned char> bitmap(65536);
    uLongf size=static_cast<uLongf>(bitmap.size());
    if(uncompress(bitmap.data(),&size,packet.data+12,packet.packetLength-12)!=Z_OK
        || size<54 || bitmap[0]!='B' || bitmap[1]!='M'
        || ReadLE32(bitmap.data()+18)!=24
        || (ReadLE32(bitmap.data()+22)!=24 && ReadLE32(bitmap.data()+22)!=static_cast<u32>(-24)))return;
    bitmap.resize(size);
    mode.m_guildEmblems[guild]={version,std::move(bitmap)};
    DbgLog("[GuildEmblem] received guild=%u version=%u bytes=%u\n",guild,version,static_cast<unsigned>(size));
    mode.m_emblemRequestTicks.erase(guild);
    if(mode.m_pendingEmblemGuild==guild && version!=mode.m_pendingEmblemVersion
        && mode.m_pendingEmblemData.size()==packet.packetLength-12
        && std::equal(mode.m_pendingEmblemData.begin(),mode.m_pendingEmblemData.end(),packet.data+12)) {
        mode.m_pendingEmblemGuild=0;mode.m_pendingEmblemData.clear();
        g_windowMgr.PushChatEvent("Guild emblem updated by the server.",0x0080C0FFu,6);
        DbgLog("[GuildEmblem] confirmed guild=%u version=%u\n",guild,version);
    }
}

void HandleGuildBelong(CGameMode& mode, const PacketView& packet)
{
    // ZC_UPDATE_GDID supplies the local guild even before hovering anyone.
    if(!packet.data || packet.packetLength<43)return;
    auto& guild=g_session.m_localGuild;
    const u32 guildId=ReadLE32(packet.data+2);
    if(guild.characterId!=g_session.m_gid || guild.guildId!=guildId){guild={};g_session.m_guildWindow={};g_session.ClearGuildSkillItems();}
    guild.characterId=g_session.m_gid;
    guild.guildId=guildId;
    guild.emblemVersion=ReadLE32(packet.data+6);
    guild.name=ExtractFixedPacketString(packet,19,24);
    guild.permissions=ReadLE32(packet.data+10);guild.isMaster=packet.data[14]!=0;
    if(!mode.m_world || !mode.m_world->m_player)return;
    auto* player=mode.m_world->m_player;
    player->m_gdid=static_cast<int>(ReadLE32(packet.data+2));
    player->m_emblemVersion=static_cast<int>(ReadLE32(packet.data+6));
    mode.m_actorNameListByGID[player->m_gid].guildName=ExtractFixedPacketString(packet,19,24);
}

void HandleGuildWindowPacket(CGameMode& mode,const PacketView& packet)
{
    if(!ro::guild::ReadWindowPacket(g_session.m_guildWindow,packet.packetId,packet.data,packet.packetLength))return;
    if((packet.packetId==0x16d || packet.packetId==0x1f2) && ReadLE32(packet.data+10)==0)mode.m_guildPosList.erase(ReadLE32(packet.data+2));
    if(packet.packetId==0x150 || packet.packetId==0x1b6) {
        auto& guild=g_session.m_localGuild;guild.characterId=g_session.m_gid;
        guild.guildId=g_session.m_guildWindow.id;guild.name=g_session.m_guildWindow.name;guild.emblemVersion=g_session.m_guildWindow.emblemVersion;
    }
    u32 permissions=g_session.m_localGuild.permissions;
    std::string title=g_session.m_localGuild.title;
    if(ro::guild::FindLocalPosition(g_session.m_guildWindow,g_session.m_gid,permissions,title)) {
        g_session.m_localGuild.permissions=permissions;
        g_session.m_localGuild.title=std::move(title);
    }
}

void InvalidatePetInfo()
{
    for(auto* window:g_windowMgr.m_children)if(window && window->m_id==88)window->Invalidate();
}

void HandlePetStatus(CGameMode&, const PacketView& packet)
{
    auto next=g_session.m_petStatus;
    if(!ro::pet::ReadStatus(next,packet.data,packet.packetLength))return;
    if(next.level>=10000){
        // Source factory12 is the shared item detail, named ItemInfo in this
        // port. Port ItemCollection is the separate illustration preview.
        auto* detail=static_cast<UIItemInfoWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_ITEMINFOWND));
        if(detail)detail->SetPetSummary(next);
        return;
    }
    g_session.m_petStatus=std::move(next);
    if(g_windowMgr.m_petInfoWnd)g_windowMgr.m_petInfoWnd->RefreshReceivedName();
    InvalidatePetInfo();
}

void HandlePetCaptureArm(CGameMode& mode,const PacketView& packet)
{
    if(!packet.data || packet.packetId!=0x019E || packet.packetLength!=2
        || packet.data[0]!=0x9e || packet.data[1]!=0x01)return;
    // This replaces pending mode intent only, not a queued player proceed.
    mode.m_skillUseInfo={10000,0,5,2};
}

void HandlePetCaptureResult(CGameMode&,const PacketView& packet)
{
    if(!packet.data || packet.packetId!=0x01A0 || packet.packetLength!=3
        || packet.data[0]!=0xa0 || packet.data[1]!=0x01)return;
    if (!CRagConnection::instance()->ConsumePetCaptureResult()) return;
    if(auto* window=g_windowMgr.m_petTamingDeceiveWnd)
        window->ApplyCaptureResult(packet.data[2] != 0);
}

void HandlePetEggList(CGameMode&,const PacketView& packet)
{
    if(!packet.data || packet.packetId!=0x01a6 || packet.packetLength<4
        || packet.packetLength>0x7fff || ro::pet::ReadU16(packet.data)!=0x01a6
        || ro::pet::ReadU16(packet.data+2)!=packet.packetLength)return;
    auto* window=static_cast<UIPetEggListWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_SELECTPETEGGWND));
    if(!window)return;
    window->SendMsg(nullptr,75,0,0,0); // Selected native recipient deliberately ignores75.
    const int count=(packet.packetLength-4)/2;
    for(int i=0;i<count;++i){
        const auto index=static_cast<unsigned int>(ro::pet::ReadI16(packet.data+4+2*i));
        if(const auto* item=g_session.GetInventoryItemByIndex(index)){
            if(item->m_itemIndex)window->SendMsg(nullptr,31,reinterpret_cast<msgparam_t>(item->m_itemName.c_str()),item->m_itemIndex,0);
        }
    }
    window->SendMsg(nullptr,60,0,0,0);
}

void HandlePetStatusChange(CGameMode& mode,const PacketView& packet)
{
    if(!packet.data || packet.packetLength!=11 || packet.packetId!=0x01a4)return;
    const u32 actorId=ReadLE32(packet.data+3),bits=ReadLE32(packet.data+7);
    s32 value;std::memcpy(&value,&bits,sizeof(value));
    auto* actor=mode.m_world?mode.m_world->GetGameActorByAID(actorId):nullptr;
    auto& pet=g_session.m_petStatus;
    switch(packet.data[2]){
    case 0:
        pet.actorId=actorId;if(actor)pet.job=actor->m_job;break;
    case 1:
        pet.relationship=value;if(actor)pet.job=actor->m_job;
        InvalidatePetInfo();
        if(g_session.m_playerJob>=4050 && g_session.m_playerJob<=4052 && value<100 && pet.fullness<30){
            ro::UiMessageStrings table;table.Load();const auto message=table.Get(1188,"");
            g_windowMgr.SendMsg(1,reinterpret_cast<msgparam_t>(message.c_str()),0x3232ff,0);
        }
        break;
    case 2:
        pet.oldFullness=pet.fullness;pet.fullness=value;if(actor)pet.job=actor->m_job;
        InvalidatePetInfo();break;
    case 3:
        if(actor){
            actor->SetPetAccessoryActName(value?g_session.GetPetAccActName(value):nullptr);
            if(actorId==pet.actorId){
                pet.accessoryItemId=value;
                InvalidatePetInfo();
            }
        }
        break;
    case 4:
        if(actor){actor->m_pkState=value;actor->SetState(0);}break;
    case 5:
        if(actor)actor->m_headType=100;break;
    default:break;
    }
}

int PetReactionState(int percent)
{
    if (percent > 100) return 0;
    if (percent > 90) return 4;
    if (percent > 75) return 3;
    if (percent > 25) return 2;
    return percent > 10 ? 1 : 0;
}

void HandlePetFeedResult(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength != 5) {
        return;
    }
    if (packet.data[2] == 0) {
        ShowMissingPetFood(ReadLE16(packet.data + 3));
        return;
    }
    if (packet.data[2] != 1) {
        return;
    }

    const auto& pet = g_session.m_petStatus;
    if (pet.job < 1000 || pet.job > 0xffff) {
        return;
    }
    const int hunger = PetReactionState(pet.oldFullness);
    const int friendship = PetReactionState(pet.relationship / 10);
    // Ref CSession::InitPetTables, feeding action 0.
    constexpr int feedEmotions[5][5] = {
        {32, 32, 20, 33, 18},
        {20, 20, 33, 18, 15},
        {32, 20, 33, 18, 15},
        {32, 32, 20, 33, 18},
        {23, 32, 32, 20, 20},
    };
    const int voiceBase = pet.job < 1519 && g_session.m_sex == 1 ? 50000 : 90000;
    mode.SendMsg(161, voiceBase + (pet.job - 1000) * 100 + hunger, 0, 0);
    mode.SendMsg(161, feedEmotions[hunger][friendship], 0, 0);
}

void HandleChangeGuild(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 12 || mode.m_world == nullptr) {
        return;
    }

    CGameActor* actor = ResolveSkillPacketActor(mode, ReadLE32(packet.data + 2));
    const u32 gid=ReadLE32(packet.data+2);
    if(gid==g_session.m_gid || gid==g_session.m_aid) {
        auto& guild=g_session.m_localGuild;
        const u32 guildId=ReadLE32(packet.data+6);
        if(guild.characterId!=g_session.m_gid || guild.guildId!=guildId)guild={};
        guild.characterId=g_session.m_gid;guild.guildId=guildId;
        guild.emblemVersion=ReadLE16(packet.data+10);
    }
    if (actor != nullptr) {
        actor->SetGuildInfo(ReadLE32(packet.data + 6), static_cast<s16>(ReadLE16(packet.data + 10)));
    }
}

namespace {

constexpr const char* kGuildSkillIdNames[16] = {
    "GD_APPROVAL", "GD_KAFRACONTRACT", "GD_GUARDRESEARCH", "GD_GUARDUP",
    "GD_EXTENSION", "GD_GLORYGUILD", "GD_LEADERSHIP", "GD_GLORYWOUNDS",
    "GD_SOULCOLD", "GD_HAWKEYES", "GD_BATTLEORDER", "GD_REGENERATION",
    "GD_RESTORE", "GD_EMERGENCYCALL", "GD_DEVELOPMENT", "GD_ITEMEMERGENCYCALL",
};

const char* GetGuildSkillIdName(int skillId)
{
    if (skillId < 10000 || skillId >= 10016) {
        return "SKILL-UNKNOWN";
    }
    return kGuildSkillIdNames[skillId - 10000];
}

} // namespace

void HandleGuildSkillInfo(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 6) {
        return;
    }

    mode.m_isReqUpgradeSkillLevel = 0;
    g_session.ClearGuildSkillItems();

    const int sourceLength = static_cast<int>(static_cast<s16>(packet.packetLength));
    const int recordCount = (sourceLength - 6) / 37;
    size_t offset = 6;
    for (int index = 0; index < recordCount; ++index, offset += 37) {
        PLAYER_SKILL_INFO skillInfo{};
        skillInfo.m_isValid = 1;
        skillInfo.SKID = static_cast<int>(ReadLE16(packet.data + offset));
        skillInfo.type = static_cast<int>(ReadLE32(packet.data + offset + 2));
        skillInfo.level = static_cast<int>(static_cast<s16>(ReadLE16(packet.data + offset + 6)));
        skillInfo.spcost = static_cast<int>(static_cast<s16>(ReadLE16(packet.data + offset + 8)));
        skillInfo.upgradable = packet.data[offset + 36];

        if (skillInfo.SKID >= 10000 && skillInfo.SKID < 10016) {
            skillInfo.skillIdName = GetGuildSkillIdName(skillInfo.SKID);
            skillInfo.skillName = skillInfo.skillIdName;
            g_session.SetGuildSkillItem(skillInfo);
        }
    }

    g_session.SetGuildSkillPoint(static_cast<int>(static_cast<s16>(ReadLE16(packet.data + 4))));
    g_session.InvalidateUI();
}

void HandleShowImage(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 19) {
        return;
    }

    char imageName[17]{};
    std::memcpy(imageName, packet.data + 2, 16);
    imageName[16] = '\0';
    int imageType = packet.data[18];
    if (imageType == 255) {
        imageType = -1;
    }
    mode.SendMsg(113, reinterpret_cast<msgparam_t>(imageName), imageType, 0);
}

void HandleShowImage2(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 67) {
        return;
    }

    char imageName[65]{};
    std::memcpy(imageName, packet.data + 2, 64);
    imageName[64] = '\0';
    int imageType = packet.data[66];
    if (imageType == 255) {
        imageType = -1;
    }
    DbgLog("[GameMode] show image2 name='%s' type=%d\n", imageName, imageType);
    mode.SendMsg(113, reinterpret_cast<msgparam_t>(imageName), imageType, 0);
}

void HandleAutoSpellList(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetId != 0x01CD || packet.packetLength != 30) {
        return;
    }

    std::vector<unsigned int> skillIds;
    skillIds.reserve(7);
    for (size_t index = 0; index < 7; ++index) {
        const u32 skillId = ReadLE32(packet.data + 2 + index * 4);
        if (skillId != 0 && skillId <= 0xFFFFu) {
            skillIds.push_back(skillId);
        }
    }
    if (skillIds.empty()) {
        if (g_windowMgr.m_skillSelectWnd && g_windowMgr.m_skillSelectWnd->IsAutoSpellSelection()) {
            g_windowMgr.DeleteWindow(g_windowMgr.m_skillSelectWnd);
        }
        return;
    }

    mode.m_warpSkillId = 0;
    mode.m_warpMapDestinations.clear();
    UISkillSelectWnd* const window = dynamic_cast<UISkillSelectWnd*>(
        g_windowMgr.MakeWindow(UIWindowMgr::WID_SKILLSELECTWND));
    if (!window) {
        return;
    }
    window->SetAutoSpellSkills(skillIds);
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

    if (IsHomunOrMercenaryJob(job)) {
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
    std::optional<u32> showEfst; // OPT3 is separate from OPTION and absent on some records.

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
    int srcX = -1;
    int srcY = -1;
    int subCellX = 0;
    int subCellY = 0;
    u32 moveStartTime = 0;
    bool hasMovePath = false;
    int dir = 0;
    u8 objectType = 0;
    bool hasPosition = false;
    bool positionIsFixed = false;
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

void UpdateRuntimeActorPosition(CGameMode& mode, u32 gid, int tileX, int tileY, int srcX = -1, int srcY = -1, u32 moveStartTime = 0, int subCellX = 0, int subCellY = 0);
void ApplyRuntimeActorFixPosition(CGameMode& mode, u32 gid, int tileX, int tileY);

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
    actor->m_hasSiegeEmblem = false;
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

    RetireActorTarget(mode, gid);

    if (CPc* pcActor = dynamic_cast<CPc*>(&actor)) {
        pcActor->InvalidateBillboard();
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

void ActivateSiegeEmblem(const CGameMode& mode, CGameActor& actor)
{
    constexpr int hidden = kEffectStateHidingMask | kEffectStateBurrowMask | kEffectStateSpecialHidingMask;
    const bool guardian = actor.m_job >= 1285 && actor.m_job <= 1287;
    if (mode.m_world && mode.m_world->m_isSiegeMode && (actor.m_isPc || guardian)
        && actor.m_headType != 100 && actor.m_gdid > 0 && !IsGravityAid(actor.m_gid)
        && (IsGravityAid(g_session.m_aid) || !(actor.m_effectState & hidden))) {
        actor.m_hasSiegeEmblem = true;
    }
}

void ApplyRuntimeActorState(CGameMode& mode, const RuntimeActorState& state)
{
    const bool isPc = ShouldTreatActorAsPc(state.objectType, state.job);
    const auto& receiveProfile = ro::net::GetActiveMapReceiveProfile();
    // The dedicated legacy NPC entry has no companion-alive branch.
    const bool newCompanionEntry = state.gid != 0
        && state.packetId != receiveProfile.actorSpawnLegacyNpc
        && state.packetId != receiveProfile.actorSpawnLegacyAlt
        && (state.gid == g_session.m_homunGID || state.gid == g_session.m_merGID)
        && (IsNpcLikeJob(state.job) || IsHomunOrMercenaryJob(state.job) || state.objectType == 9)
        && !FindRuntimeActorForVanish(mode, state.gid)
        && (!mode.m_world || !mode.m_world->GetGameActorByAID(state.gid));
    CGameActor* actor = EnsureRuntimeActor(
        mode,
        state.gid,
        isPc || ShouldUseSpriteBillboardActor(state.objectType, state.job));
    if (!actor) {
        return;
    }

    if (newCompanionEntry) {
        if (state.gid == g_session.m_homunGID) g_session.m_homunStatus.alive = true;
        if (state.gid == g_session.m_merGID) g_session.m_merStatus.alive = true;
    }

    const bool hadResolvedIdentity = actor->m_job != 0 || actor->m_objectType != 0 || actor->m_isPc != 0;
    const int oldEffectState = actor->m_effectState;

    actor->m_speed = state.speed;
    actor->m_job = state.job;
    if (state.objectType == 9 && state.gid == g_session.m_merGID && g_session.m_merStatus.alive) {
        g_session.m_merStatus.job = state.job;
    }
    actor->m_sex = state.sex;
    actor->m_bodyState = state.bodyState;
    actor->m_healthState = state.healthState;
    actor->m_effectState = state.effectState;
    actor->m_pkState = state.pkState;
    actor->m_clevel = static_cast<u16>(state.clevel);
    actor->m_xSize = state.xSize;
    actor->m_ySize = state.ySize;
    // The Ref actor entry and walk handlers mark object type 7 as a pet,
    // independently of the server-configurable hair value.
    actor->m_headType = state.objectType == 7 ? 100 : state.headType;
    actor->m_gdid = state.guildId;
    actor->m_emblemVersion = state.emblemVersion;
    // Flag NPCs carry the owner in the two head accessory words and the
    // emblem version in the bottom accessory (shield on old 007c spawns).
    // Their legacy NPC packet has no ordinary guild fields.
    if(state.job==722) {
        actor->m_gdid=static_cast<int>((static_cast<u32>(state.headTop)<<16)|static_cast<u32>(state.headMid));
        actor->m_emblemVersion=state.packetId==ro::net::GetActiveMapReceiveProfile().actorSpawnLegacyNpc?state.shield:state.headBottom;
        DbgLog("[GuildFlag] actor=%u guild=%d emblemVersion=%d\n",state.gid,actor->m_gdid,actor->m_emblemVersion);
    }
    actor->m_charfont = static_cast<u16>(state.charfont);
    actor->m_objectType = state.objectType;
    actor->m_actorType = state.objectType;
    actor->m_isPc = isPc ? 1 : 0;
    actor->m_roty = PacketDirToRotationDegrees(state.dir);

    if (state.hasPosition) {
        // A walking entry's current cell is the path start. The destination is where it is going.
        const int recordedX = state.hasMovePath ? state.srcX : state.tileX;
        const int recordedY = state.hasMovePath ? state.srcY : state.tileY;
        mode.m_actorPosList[state.gid] = CellPos{recordedX, recordedY};
        if (state.positionIsFixed) {
            ApplyRuntimeActorFixPosition(mode, state.gid, state.tileX, state.tileY);
        } else {
            if (state.moveStartTime != 0) {
                g_session.UpdateServerTime(state.moveStartTime);
            }
            UpdateRuntimeActorPosition(mode,
                state.gid,
                state.tileX,
                state.tileY,
                state.hasMovePath ? state.srcX : -1,
                state.hasMovePath ? state.srcY : -1,
                state.moveStartTime,
                state.subCellX,
                state.subCellY);
        }
    }

    const bool localPlayer = IsLocalPlayerActor(mode, state.gid);

    if (localPlayer && IsEffectStatePushCart(state.effectState)) {
        UpdateKnownLocalCartState(true, state.effectState);
    }

    RefreshActorEffectStatePresentation(actor, oldEffectState);
    if (state.showEfst) actor->SetShowEfst(*state.showEfst);
    else actor->RefreshShowEfstVisuals();
    ActivateSiegeEmblem(mode, *actor);

    // Ref/GameModePacket.cpp GameModeShowEFST: clevel 99 starts message 126
    // unless the actor is in cloak. Hiding and burrow are handled inside 126.
    if (state.clevel == 99 && !HasEffectStateFlag(state.effectState, kEffectStateSpecialHidingMask)) {
        actor->SendMsg(actor, 126, 0, 0, 0);
    }
    if (localPlayer && IsEffectStatePushCartChanged(actor->m_effectState, oldEffectState)) {
        InvalidateCartUi();
    }
    if (localPlayer) {
        ApplyPendingLocalCartState(mode);
    }

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

    if (!hadResolvedIdentity && !isPc && actor->m_objectType == 6) {
        const auto cachedNameIt = mode.m_actorNameListByGID.find(state.gid);
        if (cachedNameIt == mode.m_actorNameListByGID.end() || cachedNameIt->second.name.empty()) {
            SendActorNameRequestIfNeeded(mode, state.gid);
        }
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

    const ro::net::MapReceiveProfile& receiveProfile = ro::net::GetActiveMapReceiveProfile();

    if (packet.packetId == receiveProfile.actorSpawnLegacyNpc) {
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
        outState.positionIsFixed = true;
        return true;
    }

    if (packet.packetId == receiveProfile.actorSpawnLegacyIdle
        || packet.packetId == receiveProfile.actorSpawnLegacySpawn
        || packet.packetId == receiveProfile.actorSpawnLegacyIdleShifted
        || packet.packetId == receiveProfile.actorSpawnLegacySpawnShifted) {
        const bool isIdle = packet.packetId == receiveProfile.actorSpawnLegacyIdle
            || packet.packetId == receiveProfile.actorSpawnLegacyIdleShifted;
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
            state.effectState = static_cast<int>(option);
            state.showEfst = opt3;
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
            state.positionIsFixed = true;
        };

        RuntimeActorState classicState;
        decodeLegacyCandidate(0, false, classicState);

        const bool canUseShiftedLegacyLayout =
            (packet.packetId == receiveProfile.actorSpawnLegacyIdle || packet.packetId == receiveProfile.actorSpawnLegacySpawn) &&
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

    if (packet.packetId == receiveProfile.actorSpawnVariableSpawn
        || packet.packetId == receiveProfile.actorSpawnVariableIdle
        || packet.packetId == receiveProfile.actorSpawnVariableSpawnRobe
        || packet.packetId == receiveProfile.actorSpawnVariableIdleRobe) {
        const bool hasRobe = packet.packetId == receiveProfile.actorSpawnVariableSpawnRobe
            || packet.packetId == receiveProfile.actorSpawnVariableIdleRobe;
        const bool isIdle = packet.packetId == receiveProfile.actorSpawnVariableIdle
            || packet.packetId == receiveProfile.actorSpawnVariableIdleRobe;
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
        outState.effectState = static_cast<int>(option);
        outState.showEfst = opt3;
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
        outState.positionIsFixed = true;
        return true;
    }

    if (packet.packetId != receiveProfile.actorSpawnModernIdle
        && packet.packetId != receiveProfile.actorSpawnModernSpawn
        && packet.packetId != receiveProfile.actorSpawnModernSpawnFont
        && packet.packetId != receiveProfile.actorSpawnModernIdleFont) {
        return false;
    }

    const bool hasFont = packet.packetId == receiveProfile.actorSpawnModernSpawnFont
        || packet.packetId == receiveProfile.actorSpawnModernIdleFont;
    const bool isIdle = packet.packetId == receiveProfile.actorSpawnModernIdle
        || packet.packetId == receiveProfile.actorSpawnModernIdleFont;
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
    outState.effectState = static_cast<int>(option);
    outState.showEfst = opt3;
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
    outState.positionIsFixed = true;
    return true;
}

bool DecodeActorWalkingPacket(const PacketView& packet, RuntimeActorState& outState)
{
    if (!packet.data) {
        return false;
    }

    const ro::net::MapReceiveProfile& receiveProfile = ro::net::GetActiveMapReceiveProfile();

    if (packet.packetId == receiveProfile.actorMoveLegacy || packet.packetId == receiveProfile.actorMoveLegacyShifted) {
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
            state.srcX = srcX;
            state.srcY = srcY;
            state.subCellX = srcDir;
            state.subCellY = dstDir;
            // Ref Zc_Notify_Moveentry: the walk tick is the dword at offset 22.
            // A shifted object-type byte moves that field with the rest of the packet.
            state.moveStartTime = ReadLE32(packet.data + 22 + shift);
            state.hasMovePath = true;
            state.dir = dstDir;
            state.hasPosition = true;
        };

        RuntimeActorState classicState;
        decodeLegacyMoveCandidate(0, false, classicState);

        const bool canUseShiftedLegacyLayout =
            packet.packetId == receiveProfile.actorMoveLegacy &&
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

    if (packet.packetId == receiveProfile.actorMoveVariable || packet.packetId == receiveProfile.actorMoveVariableRobe) {
        const bool hasRobe = packet.packetId == receiveProfile.actorMoveVariableRobe;
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
        outState.effectState = static_cast<int>(option);
        outState.showEfst = opt3;
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
        outState.srcX = srcX;
        outState.srcY = srcY;
        outState.subCellX = srcDir;
        outState.subCellY = dstDir;
        // The 2009+ walking entry keeps the same tick gap as 0x022C, shifted by the length header.
        outState.moveStartTime = ReadLE32(packet.data + 29);
        outState.hasMovePath = true;
        outState.dir = dstDir;
        outState.hasPosition = true;
        return true;
    }

    if (packet.packetId != receiveProfile.actorMoveModern && packet.packetId != receiveProfile.actorMoveModernFont) {
        return false;
    }

    const bool hasFont = packet.packetId == receiveProfile.actorMoveModernFont;
    const size_t minimumLength = hasFont ? 67 : 65;
    if (packet.packetLength < minimumLength) {
        if (packet.packetId == receiveProfile.actorMoveModern || packet.packetId == receiveProfile.actorMoveModernFont) {
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
    outState.effectState = static_cast<int>(option);
    outState.showEfst = opt3;
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
    outState.srcX = srcX;
    outState.srcY = srcY;
    outState.subCellX = srcDir;
    outState.subCellY = dstDir;
    // eAthena clif_set_unit_walking for 0x022C / 0x02EC: gettick() sits between
    // head bottom and head top. POS2 at +55 is the current cell, then the destination.
    outState.moveStartTime = ReadLE32(packet.data + 27);
    outState.hasMovePath = true;
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
        const int resolvedAction = baseAction + ((static_cast<int>(dir) + 4) & 7);
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

void HandleCartInfo(CGameMode& mode, const PacketView& packet)
{
    ro::commerce::Receive(mode,packet);
    if (!packet.data || packet.packetLength < 14) {
        return;
    }

    const u16 count = ReadLE16(packet.data + 2);
    const u16 maxCount = ReadLE16(packet.data + 4);
    const u32 weight = ReadLE32(packet.data + 6);
    const u32 maxWeight = ReadLE32(packet.data + 10);
    UpdateKnownLocalCartState(true);
    ApplyPendingLocalCartState(mode);
    DbgLog("[GameMode] cart info count=%u/%u weight=%u/%u\n",
        static_cast<unsigned int>(count),
        static_cast<unsigned int>(maxCount),
        static_cast<unsigned int>(weight),
        static_cast<unsigned int>(maxWeight));
}

void HandleCartOff(CGameMode& mode, const PacketView& packet)
{
    ro::commerce::Receive(mode,packet);
    if (!packet.data || packet.packetLength < 2) {
        return;
    }

    // Local actor option changes own cart presence. ZC_CARTOFF only clears contents.
    InvalidateCartUi();
    DbgLog("[GameMode] cart cleared\n");
}

void HandleEmotion(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 7 || packet.data[6] > 80) return;
    const u32 gid = ReadLE32(packet.data + 2);
    if (CGameActor* actor = ResolveSkillPacketActor(mode, gid)) {
        actor->SendMsg(actor, 21, packet.data[6], 0, 0);
        if (IsLocalPlayerActor(mode, gid)) mode.m_isReqEmotion = 0;
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

    if (!ro::net::IsActiveActorStateChangeExtendedPacket(packet.packetId)) {
        if (packet.packetLength < 13) {
            return;
        }

        gid = ReadLE32(packet.data + 2);
        bodyState = ReadLE16(packet.data + 6);
        healthState = ReadLE16(packet.data + 8);
        effectState = ReadLE16(packet.data + 10);
        pkState = packet.data[12];
    } else {
        if (packet.packetLength < 15) {
            return;
        }

        gid = ReadLE32(packet.data + 2);
        bodyState = ReadLE16(packet.data + 6);
        healthState = ReadLE16(packet.data + 8);
        effectState = static_cast<int>(ReadLE32(packet.data + 10));
        pkState = packet.data[14];
    }

    mode.m_aidList[gid] = GetTickCount();

    CGameActor* actor = EnsureRuntimeActor(mode, gid);
    if (!actor) {
        return;
    }

    const int oldEffectState = actor->m_effectState;
    if (IsLocalPlayerActor(mode, gid) && (actor->m_healthState != healthState || actor->m_bodyState != bodyState)) {
        DbgLog("[Status] local body=%d health=0x%04X (previous body=%d health=0x%04X)\n", bodyState, healthState, actor->m_bodyState, actor->m_healthState);
    }
    AttrStateArgs state{};
    state.bodyState = bodyState;
    state.effectState = effectState;
    state.healthState = healthState;
    state.pkState = pkState;
    actor->SendMsg(actor, 43, reinterpret_cast<msgparam_t>(&state), 0, 0);

    constexpr int hidden = kEffectStateHidingMask | kEffectStateBurrowMask | kEffectStateSpecialHidingMask;
    if ((oldEffectState ^ actor->m_effectState) & hidden) {
        if (actor->m_effectState & hidden) {
            actor->m_hasSiegeEmblem = false;
        } else if (g_serviceType != ServiceChina && g_serviceType != ServiceMalaysia
            && g_serviceType != ServiceSingapore) {
            ActivateSiegeEmblem(mode, *actor);
        }
    }

    const bool localPlayer = IsLocalPlayerActor(mode, gid);
    if (localPlayer) {
        UpdateKnownLocalCartState(IsEffectStatePushCart(effectState), effectState);
    }
    if (localPlayer && IsEffectStatePushCartChanged(actor->m_effectState, oldEffectState)) {
        InvalidateCartUi();
    }
    if (localPlayer) {
        ApplyPendingLocalCartState(mode);
    }

    if (IsLocalActorId(gid)
        && IsPendingLocalHideSkillRequestActive(timeGetTime())
        && HasEffectStateFlag(actor->m_effectState, g_pendingLocalHideSkillRequest.mask) == g_pendingLocalHideSkillRequest.expectedActive) {
        ClearPendingLocalHideSkillRequest();
    }
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

            const std::list<CMsgEffect*> preservedMsgEffects = existing->m_msgEffectList;
            CMsgEffect* const preservedBirdEffect = existing->m_birdEffect;
            CMsgEffect* const preservedCartEffect = existing->m_cartEffect;
            CMsgEffect* const preservedLastDamageNumber = existing->m_lastDamageNumberEffect;
            static_cast<CGameActor&>(*upgraded) = *existing;
            upgraded->m_birdEffect = preservedBirdEffect;
            upgraded->m_cartEffect = preservedCartEffect;
            upgraded->m_lastDamageNumberEffect = preservedLastDamageNumber;
            upgraded->m_effectList.clear();
            upgraded->m_beginSpellEffect = nullptr;
            upgraded->m_magicTargetEffect = nullptr;
            upgraded->m_msgEffectList = preservedMsgEffects;
            for (CMsgEffect* effect : upgraded->m_msgEffectList) {
                if (!effect) {
                    continue;
                }

                effect->m_masterActor = upgraded;
                effect->m_removedFromOwner = 0;
                if (effect == upgraded->m_cartEffect) {
                    effect->m_followInitialized = 0;
                    effect->m_followRenderInitialized = 0;
                    effect->m_followLastRenderFrame = 0;
                    effect->m_followRenderTick = 0;
                }
            }
            existing->m_birdEffect = nullptr;
            existing->m_cartEffect = nullptr;
            existing->m_lastDamageNumberEffect = nullptr;
            existing->m_msgEffectList.clear();
            if (mode.m_world) {
                mode.m_world->UnregisterActor(existing);
            }
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

void UpdateRuntimeActorPosition(CGameMode& mode, u32 gid, int tileX, int tileY, int srcX, int srcY, u32 moveStartTime, int subCellX, int subCellY)
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
    // Ref Zc_Notify_Move only replaces the path. It does not sample the new
    // path onto m_pos, and it does not run ProcessState on the old path after
    // the clock jump. Both of those land as a visible pop on the next packet.
    const bool alreadyPlaced = prevIsMoving != 0
        || actor->m_stateId != 0
        || actor->m_pos.x != 0.0f
        || actor->m_pos.y != 0.0f
        || actor->m_pos.z != 0.0f;

    actor->UnRegisterPos();
    actor->m_moveSrcX = srcX >= 0 ? srcX : actor->m_moveDestX;
    actor->m_moveSrcY = srcY >= 0 ? srcY : actor->m_moveDestY;
    actor->m_moveDestX = tileX;
    actor->m_moveDestY = tileY;
    actor->m_lastTlvertX = tileX;
    actor->m_lastTlvertY = tileY;
    actor->m_path.Reset();

    const float srcWorldX = TileToWorldCoordX(mode.m_world, actor->m_moveSrcX);
    const float srcWorldZ = TileToWorldCoordZ(mode.m_world, actor->m_moveSrcY);
    actor->m_moveStartPos.x = srcWorldX;
    actor->m_moveStartPos.z = srcWorldZ;
    actor->m_moveStartPos.y = ResolveActorHeight(mode.m_world, srcWorldX, srcWorldZ);
    const float worldX = TileToWorldCoordX(mode.m_world, tileX);
    const float worldZ = TileToWorldCoordZ(mode.m_world, tileY);
    actor->m_moveEndPos.x = worldX;
    actor->m_moveEndPos.z = worldZ;
    actor->m_moveEndPos.y = ResolveActorHeight(mode.m_world, worldX, worldZ);
    if (actor->m_moveStartPos.x == 0.0f && actor->m_moveStartPos.y == 0.0f && actor->m_moveStartPos.z == 0.0f) {
        actor->m_moveStartPos = actor->m_moveEndPos;
    }
    // Ref Zc_Notify_Move uses the packet tick as the path start. Falling back
    // to "now" makes a late packet replay the whole walk from the first cell.
    actor->m_moveStartTime = moveStartTime != 0 ? moveStartTime : g_session.GetServerTime();
    actor->m_moveEndTime = actor->m_moveStartTime;

    if (mode.m_world && mode.m_world->m_attr && actor->m_moveSrcX >= 0 && actor->m_moveSrcY >= 0) {
        g_pathFinder.SetMap(mode.m_world);
        const bool localPlayer = IsLocalPlayerActor(mode, gid);
        if (localPlayer && g_session.m_speed != 0) {
            actor->m_speed = g_session.m_speed;
        }
        const int speed = static_cast<int>(actor->m_speed != 0 ? actor->m_speed : 150u);
        auto buildPath = [&](u32 startTime, int speedFactor) {
            return g_pathFinder.FindPath(startTime,
                actor->m_moveSrcX,
                actor->m_moveSrcY,
                subCellX,
                subCellY,
                tileX,
                tileY,
                speedFactor,
                &actor->m_path)
                && actor->m_path.m_cells.size() >= 2;
        };
        if (buildPath(actor->m_moveStartTime, speed)) {
            actor->m_moveEndTime = actor->m_path.m_cells.back().arrivalTime;
            // Ref CGameActor::FindPath: a path whose last cell is already due
            // is rebuilt from now at 10/13 of the step time, so the actor
            // still walks it instead of being dropped on the destination.
            if (actor->m_moveEndTime <= g_session.GetServerTime()) {
                actor->m_moveStartTime = g_session.GetServerTime();
                const int catchupSpeed = 10 * speed / 13;
                if (buildPath(actor->m_moveStartTime, catchupSpeed > 0 ? catchupSpeed : speed)) {
                    actor->m_moveEndTime = actor->m_path.m_cells.back().arrivalTime;
                }
            }
        }
    }

    actor->m_isMoving = actor->m_moveEndTime > actor->m_moveStartTime && actor->m_path.m_cells.size() >= 2;
    if (!alreadyPlaced) {
        // Ref move-entry (SendMsg 6) places a brand new actor on the start
        // sub-cell, which the server filled with the cell it is on right now.
        // Placing it on the destination is the enter-screen snap.
        int placeX = actor->m_moveSrcX;
        int placeY = actor->m_moveSrcY;
        int placeSubX = subCellX;
        int placeSubY = subCellY;
        if (srcX < 0 || srcY < 0) {
            placeX = tileX;
            placeY = tileY;
            placeSubX = 8;
            placeSubY = 8;
        }
        float placedX = TileToWorldCoordX(mode.m_world, placeX);
        float placedZ = TileToWorldCoordZ(mode.m_world, placeY);
        if (mode.m_world && mode.m_world->m_attr && mode.m_world->m_attr->m_zoom > 0) {
            const float zoom = static_cast<float>(mode.m_world->m_attr->m_zoom);
            placedX += zoom * 0.0625f * static_cast<float>(placeSubX - 8);
            placedZ += zoom * 0.0625f * static_cast<float>(placeSubY - 8);
        }
        actor->m_pos.x = placedX;
        actor->m_pos.z = placedZ;
        actor->m_pos.y = ResolveActorHeight(mode.m_world, placedX, placedZ);
    }
    if (actor->m_isMoving) {
        actor->m_isSitting = 0;
        actor->m_pathStartCell = 0;
        // The integration baseline is this server time. Folding a clock jump
        // into the first step is what makes a new or replaced path sprint.
        actor->m_lastServerTime = g_session.GetServerTime();
        if (actor->m_stateId != 1) {
            actor->SetState(1);
        }
    } else {
        // A failed or same-cell path must not pull the actor to the destination.
        // The server sends a stop when the walk is cancelled; snapping here is
        // the loot-sprint / rubber-band.
        actor->m_path.Reset();
        actor->m_moveEndTime = actor->m_moveStartTime;
        if (actor->m_stateId == 1) {
            actor->SetState(0);
        }
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
    actor->ReapplyConcealmentVisibility();
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
    actor->ReapplyConcealmentVisibility();
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
    if (mode.m_selectedPcTargetGid == gid) {
        mode.m_selectedPcTargetGid = 0;
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
    RetireActorTarget(mode, gid);

    const auto it = mode.m_runtimeActors.find(gid);
    if (it == mode.m_runtimeActors.end()) {
        return;
    }

    if (mode.m_world && mode.m_world->m_player == it->second) {
        mode.m_world->m_player = nullptr;
    }

    if (it->second) {
        CGameActor* actor = it->second;
        if (mode.m_world) {
            mode.m_world->NotifyActorDeleted(actor);
            mode.m_world->UnregisterActor(actor);
        }
        actor->UnRegisterPos();
        mode.m_deferredActorDeletes.push_back(actor);
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

std::string ExtractActorSpeechBubbleText(const std::string& chatText)
{
    if (chatText.empty()) {
        return {};
    }

    const size_t separator = chatText.find(" : ");
    if (separator == std::string::npos) {
        return chatText;
    }

    const size_t bodyOffset = separator + 3;
    if (bodyOffset >= chatText.size()) {
        return chatText;
    }

    return chatText.substr(bodyOffset);
}

u32 ComputeActorChatBubbleUntilTick(const std::string& chatText, u32 now)
{
    const u32 bubbleLengthMs = kActorChatBubbleBaseLifetimeMs
        + static_cast<u32>(chatText.size()) * kActorChatBubblePerCharLifetimeMs;
    const u32 clampedLengthMs = (std::min)(kActorChatBubbleMaxLifetimeMs,
        (std::max)(kActorChatBubbleMinLifetimeMs, bubbleLengthMs));
    return now + clampedLengthMs;
}

CGameActor* ResolveChatSpeakerActor(CGameMode& mode, u32 actorId)
{
    if (actorId == 0) {
        return nullptr;
    }

    if (mode.m_world && mode.m_world->m_player) {
        CGameActor* const player = mode.m_world->m_player;
        if (actorId == player->m_gid || actorId == g_session.m_gid || actorId == g_session.m_aid) {
            return player;
        }
    }

    const auto directIt = mode.m_runtimeActors.find(actorId);
    if (directIt != mode.m_runtimeActors.end() && directIt->second) {
        return directIt->second;
    }

    if (actorId == g_session.m_aid && g_session.m_gid != 0) {
        const auto gidIt = mode.m_runtimeActors.find(g_session.m_gid);
        if (gidIt != mode.m_runtimeActors.end() && gidIt->second) {
            return gidIt->second;
        }
    }

    if (actorId == g_session.m_gid && g_session.m_aid != 0) {
        const auto aidIt = mode.m_runtimeActors.find(g_session.m_aid);
        if (aidIt != mode.m_runtimeActors.end() && aidIt->second) {
            return aidIt->second;
        }
    }

    return nullptr;
}

void ApplyActorChatBubble(CGameMode& mode, u32 actorId, const std::string& chatText)
{
    CGameActor* const actor = ResolveChatSpeakerActor(mode, actorId);
    if (!actor) {
        return;
    }

    const std::string bubbleText = ExtractActorSpeechBubbleText(chatText);
    if (bubbleText.empty()) {
        actor->ClearChatBubbleText();
        return;
    }

    const u32 now = GetTickCount();
    actor->SetChatBubbleText(bubbleText, ComputeActorChatBubbleUntilTick(bubbleText, now));
}

void HandlePetAction(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength != 10 || !mode.m_world) {
        return;
    }
    const u32 actorId = ReadLE32(packet.data + 2);
    auto* actor = mode.m_world->GetGameActorByAID(actorId);
    if (!actor) {
        return;
    }
    const int action = ReadCompanionI32(packet.data + 6);
    if (action < 50000) {
        actor->SendMsg(nullptr, 21, action, 0, 0);
        return;
    }

    const int code = action - (action >= 90000 ? 90000 : 50000);
    const int job = 1000 + code / 100 % 1000;
    const std::string speech = g_session.GetPetSpeech(job, code % 10, code / 10 % 10);
    if (speech.empty()) {
        return;
    }
    std::string name = mode.ResolveCharacterName(actorId);
    if (name.empty() && actorId == g_session.m_petStatus.actorId) {
        name = g_session.m_petStatus.name;
    }
    if (name.empty()) {
        return;
    }
    actor->SetChatBubbleText(speech, ComputeActorChatBubbleUntilTick(speech, GetTickCount()));
    if (g_session.m_isNoTalkMsg != 1 && g_session.m_isNoTalkMsg2 != 1) {
        RecordChat(mode, name + " : " + speech, 0x00fafafau, kChatChannelNormal,
            {ro::chat::NoticeRoute::Normal, ro::chat::NoticeType::Chat, {}});
    }
}

void PropagateChatToUi(const ChatEntry& entry, const ro::chat::NoticeSource& source)
{
    if (entry.text.empty()) {
        return;
    }

    if (source.route == ro::chat::NoticeRoute::Unspecified)
        g_windowMgr.PushChatEvent(entry.text.c_str(), entry.color, entry.channel, GetTickCount());
    else g_windowMgr.PushSourceChatEvent(entry.text.c_str(), entry.color, entry.channel, source.route, source.type,
        source.talker.c_str(), GetTickCount());
}

void RecordChat(CGameMode& mode, const std::string& text, u32 color, u8 channel, ro::chat::NoticeSource source)
{
    if (text.empty()) {
        return;
    }

    const ChatEntry entry = BuildChatEntry(text, color, channel);
    PropagateChatToUi(entry, source);

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

// Guild notifications in the Ref/2008 protocol. Position removal is (-1,-1).
void HandleGuildNotification(CGameMode& mode,const PacketView& packet){
    if(!packet.data)return;
    const auto* p=packet.data;const auto n=packet.packetLength;const auto op=packet.packetId;
    if(op==0x1eb){
        if(n<10)return;const u32 aid=ReadLE32(p+2);const int x=static_cast<s16>(ReadLE16(p+6)),y=static_cast<s16>(ReadLE16(p+8));
        if(aid==g_session.m_aid)return;
        if(x<0||y<0)mode.m_guildPosList.erase(aid);else mode.m_guildPosList[aid]={x,y,0x0080FF80u};return;
    }
    if(op==0x17f){
        if(n>=5 && !g_session.m_isNoTalkMsg2) {
            auto text = ExtractPacketString(packet, 4);
            // Original forces the final payload byte to NUL, including unterminated input.
            if (text.size() == n - 4 && !text.empty()) text.pop_back();
            RecordChat(mode, text, ro::chat::ColorRefToUiRgb(11861940u), kChatChannelParty,
                {ro::chat::NoticeRoute::Normal, ro::chat::NoticeType::Guild, {}});
        }
        return;
    }
    if(op==0x171){if(n<30)return;auto& state=ro::commerce::state;state.promptKind="alliance";state.promptId=ReadLE32(p+2);state.promptName=ExtractFixedPacketString(packet,6,24);return;}
    std::string message;
    if(op==0x173){if(n<3)return;const char* messages[]={"Guilds are already allied.","Guild alliance invitation declined.","Guild alliance invitation accepted.","The other guild has too many alliances.","Your guild has too many alliances.","Guild alliances are disabled."};message=p[2]<6?messages[p[2]]:"Guild alliance request failed.";}
    else if(op==0x181){if(n<3)return;const char* messages[]={"Guild opposition registered.","Your guild has too many opposing guilds.","Guild opposition is already registered.","Guild opposition is disabled."};message=p[2]<4?messages[p[2]]:"Guild opposition request failed.";}
    else if(op==0x15f){if(n<42)return;message="Guild disbanded: "+ExtractFixedPacketString(packet,2,40);}
    else if(op==0x15e){
        if(n<6)return;const u32 result=ReadLE32(p+2);
        if(!result){
            g_session.m_localGuild={};g_session.m_guildWindow={};g_session.ClearGuildSkillItems();mode.m_guildPosList.clear();
            for(auto& entry:mode.m_actorNameListByGID){if(entry.first==g_session.m_gid||entry.first==g_session.m_aid){entry.second.guildName.clear();entry.second.guildTitle.clear();}}
            if(mode.m_world&&mode.m_world->m_player){mode.m_world->m_player->m_gdid=0;mode.m_world->m_player->m_emblemVersion=0;}
            message="Guild disbanded.";
        }else message=result==1?"Cannot disband guild: the guild name is incorrect.":"Cannot disband guild while it still has members.";
    }
    if(!message.empty())RecordChat(mode,message,0x0080C0FFu,kChatChannelSystem);
}

#include "ChatRoomPacketHandlers.inc"

void HandleNpcChat(CGameMode& mode, const PacketView& packet)
{
    // Ref: Zc_Npc_Chat (0x02C1) / rAthena ZC_NPC_CHAT
    // Layout: <len>.W <id>.L <color>.L <message>.?B
    if (!packet.data || packet.packetLength < 13) {
        return;
    }

    const u32 actorId = ReadLE32(packet.data + 4);
    if (std::find(g_session.m_exAidList.begin(), g_session.m_exAidList.end(), actorId)
        != g_session.m_exAidList.end()) {
        return;
    }

    const u32 colorRef = ReadLE32(packet.data + 8);
    const std::string text = ExtractPacketString(packet, 12);
    if (text.empty()) {
        return;
    }

    // Match Ref UIWindowMgr::SendMsg(..., 1, chatMsg, color, 0, 0).
    RecordChat(mode, text, ro::chat::ColorRefToUiRgb(colorRef), kChatChannelSystem,
        {ro::chat::NoticeRoute::Normal, ro::chat::NoticeType::None, {}});
    ApplyActorChatBubble(mode, actorId, text);
    if (g_session.m_isSaveChat) {
        mode.WriteSourceChat(text);
    }
}

void HandleNotifyChat(CGameMode& mode, const PacketView& packet)
{
    // Ref: Zc_Notify_Chat (0x008D)
    if (!packet.data || packet.packetLength < 8) {
        return;
    }

    const u32 aid = ReadLE32(packet.data + 4);
    mode.m_aidList[aid] = GetTickCount();
    const std::string text = ExtractChatTextWithAid(packet);
    if (std::find(g_session.m_exAidList.begin(), g_session.m_exAidList.end(), aid) != g_session.m_exAidList.end()) return;
    if(g_windowMgr.m_chatRoomWnd){
        mode.m_lastChat=text;g_windowMgr.SendMsg(5,reinterpret_cast<msgparam_t>(text.c_str()),2236962);return;
    }
    if (IsNameYellow(aid)) {
        const u32 colorRef = g_serviceType == ServiceThai ? 16776960u
            : g_serviceType == ServiceGermany ? 1936895u : 0x0000ffffu;
        RecordChat(mode, text, ro::chat::ColorRefToUiRgb(colorRef), kChatChannelNormal,
            {ro::chat::NoticeRoute::Normal, ro::chat::NoticeType::Chat, {}});
    } else if (const u32 colorRef = GetSpecificNameColorRef(aid)) {
        RecordChat(mode, text, ro::chat::ColorRefToUiRgb(colorRef), kChatChannelNormal,
            {ro::chat::NoticeRoute::Normal, ro::chat::NoticeType::None, {}});
    } else if (g_session.m_isNoTalkMsg != 1 && g_session.m_isNoTalkMsg2 != 1) {
        RecordChat(mode, text, 0x00ffffffu, kChatChannelNormal,
            {ro::chat::NoticeRoute::Normal, ro::chat::NoticeType::Chat, {}});
    }
    ApplyActorChatBubble(mode, aid, text);
}

void HandleNotifyPlayerChat(CGameMode& mode, const PacketView& packet)
{
    // Ref: Zc_Notify_Playerchat (0x008E)
    if (!packet.data || packet.packetLength < 5) {
        return;
    }

    const auto text = ExtractPacketString(packet, 4);
    if(g_windowMgr.m_chatRoomWnd){
        mode.m_lastChat=text;g_windowMgr.SendMsg(5,reinterpret_cast<msgparam_t>(text.c_str()),30720);return;
    }
    RecordChat(mode, text, 0x0000ff00u, kChatChannelPlayer,
        {ro::chat::NoticeRoute::Normal, ro::chat::NoticeType::Chat, {}});
    ApplyActorChatBubble(mode, g_session.m_aid, text);
}

void HandleWhisper(CGameMode& mode, const PacketView& packet)
{
    constexpr int kNameOffset = 4;
    constexpr int kNameSize = 24;
    constexpr int kTextOffset = kNameOffset + kNameSize;
    if (!packet.data || packet.packetLength <= kTextOffset
        || ReadLE16(packet.data + 2) != packet.packetLength) return;
    const auto* body = reinterpret_cast<const char*>(packet.data + kTextOffset);
    const auto* terminator = reinterpret_cast<const char*>(packet.data + packet.packetLength - 1);
    if (*terminator != '\0' || std::find(body, terminator, '\0') != terminator) return;
    const auto name = ExtractFixedPacketString(packet, kNameOffset, kNameSize);
    if (name.empty()) return;
    if(g_session.IsExName(name.c_str()))return;
    const std::string text(body, (std::min)(packet.packetLength - kTextOffset - 1, 255));
    const bool delivered=g_windowMgr.AddWhisperChatToWhisperWnd(name,name+" : "+text,0x222222);
    if(!delivered){
        const bool friendName=g_session.IsFriendName(name.c_str()),guild=g_session.IsGuildMember(name.c_str());
        if(!friendName && !guild){
            const int warning=g_session.IsCheatName(name.c_str())?917:g_session.IsCheatGuildName(name.c_str())?919:0;
            if(warning){ro::UiMessageStrings table;table.Load();const auto message=table.Get(warning,"Check the sender's identity.");
                g_windowMgr.SendMsg(1,reinterpret_cast<msgparam_t>(message.c_str()),65535);
                g_windowMgr.SendMsg(20,reinterpret_cast<msgparam_t>(message.c_str()),65535);}
        }
        if(name!=g_session.GetPlayerName()){
            const auto message=std::string(friendName?"[ Friend ":guild?"[ Member ":"( From ")+name+(friendName || guild?" ] : ":" ) : ")+text;
            g_windowMgr.SendMsg(3,0,0);
            RecordChat(mode,message,0xffff00,kChatChannelWhisper,{ro::chat::NoticeRoute::Normal,ro::chat::NoticeType::Whisper,name});
            g_windowMgr.AddWhisperCharName(name);mode.WriteSourceChat(message);
        }
    }
    // Prefix comparison suppresses auto-reply loops; an empty prefix suppresses
    // every reply. Incoming text must never replace pending outgoing metadata.
    if(g_session.m_onAutoMSG){ro::UiMessageStrings table;table.Load();const auto prefix=table.Get(941,"[Auto] ");
        if(text.compare(0,prefix.size(),prefix)!=0)mode.SendSourceWhisper(name,prefix+g_session.m_strAutoMSG);}
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
        const bool self = aid == g_session.m_aid;
        if (!self) g_windowMgr.SendMsg(3, 0, 0);
        RecordChat(mode, msg, ro::chat::ColorRefToUiRgb(self ? 51455u : 13158655u), kChatChannelParty,
            {ro::chat::NoticeRoute::Normal, ro::chat::NoticeType::Party, {}});
        ApplyActorChatBubble(mode, aid, msg);
    }
}

std::string ExtractFixedPacketString(const u8* data, size_t length)
{
    if (!data || length == 0) {
        return {};
    }

    std::string value(reinterpret_cast<const char*>(data), length);
    const size_t nulPos = value.find('\0');
    if (nulPos != std::string::npos) {
        value.resize(nulPos);
    }
    return value;
}

using NoticeArguments = std::initializer_list<std::pair<char, std::string_view>>;

std::optional<std::string> FormatLocalizedNotice(std::string_view format, NoticeArguments arguments)
{
    std::string message;
    auto argument = arguments.begin();
    for (size_t i = 0; i < format.size(); ++i) {
        if (format[i] != '%') {
            message += format[i];
            continue;
        }
        if (++i == format.size()) {
            return std::nullopt;
        }
        const char type = format[i];
        if (type == '%') {
            message += '%';
        } else if (argument != arguments.end()
            && (type == argument->first || (type == 'u' && argument->first == 'd'))) {
            message += argument->second;
            ++argument;
        } else {
            return std::nullopt;
        }
    }
    if (argument != arguments.end() || message.empty()) {
        return std::nullopt;
    }
    return message;
}

void ShowLocalizedStarNotice(CGameMode& mode, unsigned id, const std::string& fallback,
    NoticeArguments arguments)
{
    ro::UiMessageStrings strings;
    strings.Load();
    if (const auto localized = strings.Find(id)) {
        if (const auto message = FormatLocalizedNotice(*localized, arguments)) {
            mode.ShowStarSkillNotice(*message, timeGetTime());
            return;
        }
    }
    if (const auto message = FormatLocalizedNotice(fallback, arguments)) {
        mode.ShowStarSkillNotice(*message, timeGetTime());
    }
}

void HandleStarSkill(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength != 32) {
        return;
    }
    constexpr std::array<const char*, 3> celestialNames = {"Sun", "Moon", "Stars"};
    constexpr unsigned starPlaceMessageBase = 837; // MSI_STARPLACE1 through MSI_STARPLACE6
    constexpr unsigned starMonsterMessageBase = 843; // MSI_STARMONSTER1 through MSI_STARMONSTER6
    const u8 star = packet.data[30];
    const u8 result = packet.data[31];
    std::string target = ExtractFixedPacketString(packet.data + 2, 24);
    int effect = 0;
    switch (result) {
    case 0:
    case 1:
    case 10:
    case 11: {
        if (star >= celestialNames.size()) {
            return;
        }
        const bool hatred = result >= 10;
        const bool info = result == 1 || result == 11;
        const unsigned messageId = (hatred ? starMonsterMessageBase : starPlaceMessageBase) + (info ? 3 : 0) + star;
        if (hatred && target.empty()) {
            const s32 job = static_cast<s32>(ReadLE32(packet.data + 26));
            const char* name = g_session.GetJobName(job);
            target = name && *name ? name : std::to_string(job);
        }
        const auto player = ExtractFixedPacketString(reinterpret_cast<const u8*>(g_session.m_playerName),
            sizeof(g_session.m_playerName));
        const std::string designation = std::string(hatred ? "target of the " : "place of the ") + celestialNames[star];
        const std::string fallback = info ? "%s's " + designation + " is %s."
            : "%s designated %s as the " + designation + ".";
        ShowLocalizedStarNotice(mode, messageId, fallback, {{'s', player}, {'s', target}});
        if (info) {
            effect = 543;
            PlayWave("effect\\piring.wav", 0.0f, -250.0f, 0.0f);
        }
        break;
    }
    case 20: {
        const std::string progress = std::to_string(star);
        ShowLocalizedStarNotice(mode, 927, "[TaeKwon Mission] Target: %s (%d%%)", {{'s', target}, {'d', progress}}); // MSI_TAEKWON_MISSION
        effect = 543;
        break;
    }
    case 30:
        ShowLocalizedStarNotice(mode, 1033, "Your designated places and targets have been reset.", {}); // MSI_STAR_ANGEL
        effect = 432;
        break;
    default:
        return;
    }
    if (effect != 0 && mode.m_world && mode.m_world->m_player) {
        mode.m_world->m_player->LaunchEffect(effect);
    }
}

void HandleStarPlace(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength != 3 || packet.data[2] > 2) {
        return;
    }
    mode.m_warpSkillId = 0;
    mode.m_warpMapDestinations.clear();
    auto* window = static_cast<UISkillSelectWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_SKILLSELECTWND));
    if (!window) {
        return;
    }
    ro::UiMessageStrings strings;
    strings.Load();
    std::string prompt = strings.Get(1028, "Register this map permanently?"); // MSI_STARPLACE_ACCEPT
    if (prompt.empty()) {
        prompt = "Register this map permanently?";
    }
    window->SetStarPlaceConfirmation(packet.data[2], std::move(prompt));
}

void ApplyCachedPartyHpToVisibleActor(CGameMode& mode, u32 aid, const std::string& characterName)
{
    if (aid == 0 || characterName.empty()) {
        return;
    }

    const FRIEND_INFO* const member = g_session.FindPartyMemberByAid(aid);
    if (!member || member->partyMaxHp <= 0) {
        return;
    }

    auto tryMatchActor = [&](CGameActor* actor) {
        if (!actor || actor->m_isPc == 0 || actor->m_gid == g_session.m_gid || actor->m_gid == g_session.m_aid) {
            return false;
        }

        const auto nameIt = mode.m_actorNameListByGID.find(actor->m_gid);
        if (nameIt == mode.m_actorNameListByGID.end() || nameIt->second.name != characterName) {
            return false;
        }

        actor->SendMsg(actor, 34, member->partyHp, member->partyMaxHp, 0);
        return true;
    };

    if (mode.m_world && mode.m_world->m_player) {
        tryMatchActor(mode.m_world->m_player);
    }
    for (const auto& runtimeEntry : mode.m_runtimeActors) {
        if (tryMatchActor(runtimeEntry.second)) {
            break;
        }
    }
}

void HandleGuildDeparture(CGameMode& mode,const PacketView& packet)
{
    if(!packet.data || packet.packetLength<(packet.packetId==0x15c?90:66))return;
    const std::string name=ExtractFixedPacketString(packet,2,24);
    if(name.empty())return;
    auto& members=g_session.m_guildWindow.members;
    for(const auto& member:members)if(member.name==name)mode.m_guildPosList.erase(member.account);
    members.erase(std::remove_if(members.begin(),members.end(),[&](const auto& m){return m.name==name;}),members.end());
    if(name==g_session.GetPlayerName()) {
        g_session.m_localGuild={};g_session.m_guildWindow={};g_session.ClearGuildSkillItems();
        mode.m_actorNameListByGID.erase(g_session.m_gid);
        if(mode.m_world && mode.m_world->m_player){mode.m_world->m_player->m_gdid=0;mode.m_world->m_player->m_emblemVersion=0;}
        RecordChat(mode,packet.packetId==0x15c?"You were expelled from the guild.":"You have left the guild.",0x00FFFFFFu,kChatChannelSystem);
    }
}

void HandleGuildCreateAck(CGameMode& mode,const PacketView& packet)
{
    if(!packet.data || packet.packetLength<3)return;
    const u8 result=packet.data[2];
    RecordChat(mode,ro::guild::CreateResultMessage(result),result==0?0x0000C000u:0x00FF4040u,kChatChannelSystem);
    if(result==0) {
        // Membership/ID comes from ZC_UPDATE_GDID, not this acknowledgement.
        // Refresh the full name reply so self-hover gets the new guild/rank.
        mode.m_actorNameListByGID.erase(g_session.m_gid);
        mode.m_actorNameByGIDReqTimer.erase(g_session.m_gid);
    }
}

void HandlePartyCreateAck(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 3) {
        return;
    }

    const u8 result = packet.data[2];
    switch (result) {
    case 0:
        RecordChat(mode, "Party successfully organized.", 0x0000C000, kChatChannelSystem);
        if (g_session.GetNumParty() == 0 && !mode.m_pendingPartyCreateName.empty()) {
            FRIEND_INFO info{};
            info.isValid = 1;
            info.AID = g_session.m_aid;
            info.characterName = g_session.GetPlayerName();
            info.role = 0;
            info.state = 0;

            g_session.ClearParty();
            g_session.m_partyName = mode.m_pendingPartyCreateName;
            g_session.m_amIPartyMaster = true;
            g_session.m_partyExpShare = mode.m_pendingPartyCreateExpShare != 0;
            g_session.m_itemCollectType = mode.m_pendingPartyCreateItemShare != 0;
            g_session.m_itemDivType = mode.m_pendingPartyCreateItemSharingType != 0;
            g_session.AddMemberToParty(info);
            g_session.RefreshPartyUI();

            DbgLog("[Party] seeded local create state name='%s' aid=%u\n",
                mode.m_pendingPartyCreateName.c_str(),
                static_cast<unsigned int>(g_session.m_aid));
        }
        g_windowMgr.MakeWindow(UIWindowMgr::WID_MESSENGERGROUPWND);
        if (mode.m_hasPendingPartyCreateOptions != 0) {
            const unsigned int optionBits =
                (mode.m_pendingPartyCreateExpShare != 0 ? 0x01u : 0x00u)
                | (mode.m_pendingPartyCreateItemShare != 0 ? 0x02u : 0x00u)
                | (mode.m_pendingPartyCreateItemSharingType != 0 ? 0x04u : 0x00u);
            if (optionBits != 0) {
                mode.SendMsg(CGameMode::GameMsg_RequestPartyChangeOptions, optionBits, 0, 0);
            }
        }
        break;
    case 1:
        RecordChat(mode, "Party name already exists.", 0x00FF4040, kChatChannelSystem);
        break;
    case 2:
        RecordChat(mode, "You are already in a party.", 0x00FF4040, kChatChannelSystem);
        break;
    case 3:
        RecordChat(mode, "Cannot organize a party on this map.", 0x00FF4040, kChatChannelSystem);
        break;
    default:
        break;
    }

    mode.m_hasPendingPartyCreateOptions = 0;
    mode.m_pendingPartyCreateExpShare = 0;
    mode.m_pendingPartyCreateItemShare = 0;
    mode.m_pendingPartyCreateItemSharingType = 0;
    mode.m_pendingPartyCreateName.clear();
}

void HandlePartyInviteAck(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 27) {
        return;
    }

    if (!ro::net::IsActivePartyInviteAckPacket(packet.packetId)) {
        return;
    }

    const std::string nick = ExtractFixedPacketString(packet.data + 2, 24);
    const int result = ro::net::IsActiveExtendedPartyInviteAckPacket(packet.packetId)
        ? static_cast<int>(ReadLE32(packet.data + 26))
        : static_cast<int>(packet.data[26]);

    switch (result) {
    case 0:
        RecordChat(mode, "Character is already in a party.", 0x00FF4040, kChatChannelSystem);
        break;
    case 1:
        RecordChat(mode,
            nick.empty() ? "Character rejected the party invitation." : nick + " rejected the party invitation.",
            0x00FF4040,
            kChatChannelSystem);
        break;
    case 2:
        RecordChat(mode,
            nick.empty() ? "Character accepted the party invitation." : nick + " accepted the party invitation.",
            0x0000C000,
            kChatChannelSystem);
        break;
    case 3:
        RecordChat(mode, "Party is full.", 0x00FF4040, kChatChannelSystem);
        break;
    case 4:
        RecordChat(mode, "A character from the same account already joined the party.", 0x00FF4040, kChatChannelSystem);
        break;
    case 5:
        RecordChat(mode, "The character blocked party invitations.", 0x00FF4040, kChatChannelSystem);
        break;
    case 7:
        RecordChat(mode, "Character is not online or does not exist.", 0x00FF4040, kChatChannelSystem);
        break;
    case 8:
        RecordChat(mode, "Cannot invite this character to the party right now.", 0x00FF4040, kChatChannelSystem);
        break;
    case 9:
        RecordChat(mode, "This map prohibits party joining.", 0x00FF4040, kChatChannelSystem);
        break;
    default:
        break;
    }
}

void HandlePartyInviteRequest(CGameMode&, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 30) {
        return;
    }

    if (!ro::net::IsActivePartyInviteRequestPacket(packet.packetId)) {
        return;
    }

    const u32 partyId = ReadLE32(packet.data + 2);
    if (partyId == 0) {
        return;
    }

    auto* inviteWnd = static_cast<UIJoinPartyAcceptWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_JOINPARTYACCEPTWND));
    if (!inviteWnd) {
        return;
    }

    inviteWnd->OpenInvite(partyId, ExtractFixedPacketString(packet.data + 6, 24).c_str());
}

void HandlePartyList(CGameMode& mode, const PacketView& packet)
{
    constexpr int kHeaderSize = 28;
    constexpr int kMemberSize = 46;
    constexpr int kMaxPartyMembers = 12;
    if (!packet.data || packet.packetLength < kHeaderSize
        || (packet.packetLength - kHeaderSize) % kMemberSize != 0) {
        return;
    }

    const int entryCount = (packet.packetLength - kHeaderSize) / kMemberSize;
    if (entryCount > kMaxPartyMembers) {
        return;
    }

    std::vector<FRIEND_INFO> members;
    members.reserve(static_cast<size_t>(entryCount));
    for (int index = 0; index < entryCount; ++index) {
        const u8* entry = packet.data + kHeaderSize + index * kMemberSize;
        FRIEND_INFO info{};
        info.isValid = 1;
        info.AID = ReadLE32(entry + 0);
        info.characterName = ExtractFixedPacketString(entry + 4, 24);
        info.mapName = ExtractFixedPacketString(entry + 28, 16);
        info.role = static_cast<int>(entry[44]);
        info.state = static_cast<int>(entry[45]);
        const bool duplicateAid = std::any_of(members.begin(), members.end(), [&info](const FRIEND_INFO& previous) {
            return previous.AID == info.AID;
        });
        if (info.AID == 0 || duplicateAid || info.characterName.empty() || info.role > 1 || info.state > 1) {
            return;
        }
        members.push_back(std::move(info));
    }

    const std::string partyName = ExtractFixedPacketString(packet.data + 4, 24);
    if (!g_session.ReplacePartyRoster(partyName, members)) {
        return;
    }
    for (const FRIEND_INFO& info : members) {
        ApplyCachedPartyHpToVisibleActor(mode, info.AID, info.characterName);
    }

    g_session.RefreshPartyUI();
}

void HandlePartyMemberAdded(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || !ro::net::IsActivePartyMemberAddedPacket(packet.packetId) || packet.packetLength < 79) {
        return;
    }

    FRIEND_INFO info{};
    info.isValid = 1;
    info.AID = ReadLE32(packet.data + 2);
    info.role = static_cast<int>(ReadLE32(packet.data + 6));
    info.state = static_cast<int>(packet.data[14]);
    g_session.m_partyName = ExtractFixedPacketString(packet.data + 15, 24);
    info.characterName = ExtractFixedPacketString(packet.data + 39, 24);
    info.mapName = ExtractFixedPacketString(packet.data + 63, 16);
    if (const FRIEND_INFO* const existing = g_session.FindPartyMemberByAid(info.AID)) {
        info.partyHp = existing->partyHp;
        info.partyMaxHp = existing->partyMaxHp;
    }
    if (ro::net::IsActivePartyMemberAddedExtendedPacket(packet.packetId) && packet.packetLength >= 81) {
        g_session.m_itemCollectType = packet.data[79] != 0;
        g_session.m_itemDivType = packet.data[80] != 0;
    }
    g_session.AddMemberToParty(info);
    if (info.AID == g_session.m_aid && info.role == 0) {
        g_session.m_amIPartyMaster = true;
    }
    ApplyCachedPartyHpToVisibleActor(mode, info.AID, info.characterName);
    g_session.RefreshPartyUI();
}

void HandlePartyOptionChanged(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 6) {
        return;
    }

    const u32 expOption = ReadLE32(packet.data + 2);
    if (expOption == 2) {
        RecordChat(mode, "Cannot change EXP sharing right now.", 0x00FF4040, kChatChannelSystem);
    } else {
        g_session.m_partyExpShare = expOption != 0;
    }

    if (packet.packetLength >= 8) {
        g_session.m_itemCollectType = packet.data[6] != 0;
        g_session.m_itemDivType = packet.data[7] != 0;
    }

    g_session.RefreshPartyUI();
}

void HandlePartyMemberRemoved(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 31) {
        return;
    }

    const std::string characterName = ExtractFixedPacketString(packet.data + 6, 24);
    if (characterName.empty()) {
        return;
    }

    const u8 result = packet.data[30];
    if (result == 2) {
        RecordChat(mode, "Cannot leave the party on this map.", 0x00FF4040, kChatChannelSystem);
        return;
    }
    if (result == 3) {
        RecordChat(mode, "Cannot expel a party member on this map.", 0x00FF4040, kChatChannelSystem);
        return;
    }

    g_session.DeleteMemberFromParty(characterName.c_str());
    if (characterName == g_session.GetPlayerName() || (result & 0x10u) != 0) {
        g_session.ClearParty();
        if ((result & 0x10u) != 0) {
            RecordChat(mode, "Party has been disbanded.", 0x0000C000, kChatChannelSystem);
        }
    }
    g_session.RefreshPartyUI();
}

void HandlePartyHpUpdate(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data) {
        return;
    }

    const u32 aid = ReadLE32(packet.data + 2);
    long long hp = 0;
    long long maxHp = 0;
    if (ro::net::IsActivePartyHpUpdateExtendedPacket(packet.packetId)) {
        if (packet.packetLength < 14) {
            return;
        }
        hp = static_cast<long long>(ReadLE32(packet.data + 6));
        maxHp = static_cast<long long>(ReadLE32(packet.data + 10));
    } else {
        if (packet.packetLength < 10) {
            return;
        }
        hp = static_cast<int>(ReadLE16(packet.data + 6));
        maxHp = static_cast<int>(ReadLE16(packet.data + 8));
    }

    if (aid == 0 || !g_session.SetPartyMemberHp(aid, hp, maxHp)) {
        return;
    }

    const FRIEND_INFO* const member = g_session.FindPartyMemberByAid(aid);
    if (!member || member->characterName.empty()) {
        return;
    }
    ApplyCachedPartyHpToVisibleActor(mode, aid, member->characterName);
}

void HandlePartyPosition(CGameMode&,const PacketView& packet)
{
    if(!packet.data||packet.packetLength!=10)return;
    const u32 aid=ReadLE32(packet.data+2);
    const s16 x=static_cast<s16>(ReadLE16(packet.data+6));
    const s16 y=static_cast<s16>(ReadLE16(packet.data+8));
    const FRIEND_INFO* const member = g_session.FindPartyMemberByAid(aid);
    if (!member) {
        return;
    }
    const bool removeMarker = x == -1 && y == -1;
    if (!removeMarker && (x < 0 || y < 0 || !g_session.IsPartyMemberOnMap(*member, g_session.m_curMap))) {
        return;
    }
    g_session.SetPartyMemberPosition(aid,x,y);
}

void HandleFriendsList(CGameMode&, const PacketView& packet)
{
    constexpr int kHeaderSize = 4;
    constexpr int kEntrySize = 32;
    constexpr int kMaxFriends = 40;
    if (!packet.data || packet.packetLength < kHeaderSize
        || ReadLE16(packet.data + 2) != packet.packetLength) {
        return;
    }
    const int payloadSize = packet.packetLength - kHeaderSize;
    if (payloadSize % kEntrySize || payloadSize / kEntrySize > kMaxFriends) return;

    std::list<FRIEND_INFO> friends;
    for (int offset = kHeaderSize; offset < packet.packetLength; offset += kEntrySize) {
        const u8* entry = packet.data + offset;
        FRIEND_INFO info{};
        info.isValid = 1;
        info.AID = ReadLE32(entry + 0);
        info.GID = ReadLE32(entry + 4);
        info.characterName = ExtractFixedPacketString(entry + 8, 24);
        info.state = 1;
        if (!info.AID || !info.GID || info.characterName.empty()
            || std::any_of(friends.begin(), friends.end(), [&](const FRIEND_INFO& existing) {
                return existing.GID == info.GID || existing.characterName == info.characterName;
            })) return;
        friends.push_back(std::move(info));
    }
    g_session.ReplaceFriendList(std::move(friends));
}

void HandleFriendState(CGameMode&, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 11) {
        return;
    }

    if (g_session.SetFriendState(ReadLE32(packet.data + 2), ReadLE32(packet.data + 6), packet.data[10])) {
        g_session.RefreshFriendUI();
    }
}

void HandleRecallPartnerName(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength != 26) return;
    mode.m_recallPartnerName = ExtractFixedPacketString(packet, 2, 24);
}

void HandleWeddingEffect(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength != 6 || !g_session.m_isEffectOn) return;
    const u32 gid = ReadLE32(packet.data + 2);
    if (!gid || !mode.m_world) return;
    auto* actor = gid == g_session.m_aid
        ? mode.m_world->m_player : mode.m_world->GetGameActorByAID(gid);
    if (actor) actor->LaunchEffect(347);
}

void HandleDivorceNotice(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength != 26) return;
    const auto partner = ExtractFixedPacketString(packet, 2, 24);
    const std::string_view player = g_session.GetPlayerName();
    if (player.empty() || partner.empty()) return;
    if (mode.m_recallPartnerName == partner) mode.m_recallPartnerName.clear();

    constexpr const char* fallback = "%s and %s have divorced from each other.";
    const NoticeArguments names{{'s', player}, {'s', partner}};
    ro::UiMessageStrings strings;
    strings.Load();
    auto notice = FormatLocalizedNotice(strings.Get(836, fallback), names);
    if (!notice) notice = FormatLocalizedNotice(fallback, names);
    if (!notice) return;
    g_windowMgr.SendMsg(3, 0, 0, 0, 0);
    g_windowMgr.SendMsg(1, reinterpret_cast<msgparam_t>(notice->c_str()), 16776960);
}

void HandleAdoptionResult(CGameMode&, const PacketView& packet)
{
    if (!packet.data || packet.packetLength != 6) return;
    const auto result = ReadLE32(packet.data + 2);
    constexpr unsigned ids[] = {899, 900, 916};
    constexpr const char* fallback[] = {
        "You cannot adopt more than 1 child.",
        "You must be at least character level 70 in order to adopt someone.",
        "You cannot adopt a married person."
    };
    if (result >= std::size(ids)) return;
    ro::UiMessageStrings strings;
    strings.Load();
    const auto notice = strings.Get(ids[result], fallback[result]);
    g_windowMgr.SendMsg(1, reinterpret_cast<msgparam_t>(notice.c_str()), 10198015);
}

void HandleAdoptionInvitation(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength != 34) return;
    mode.ReceiveAdoptionInvitation(ReadLE32(packet.data + 2), ReadLE32(packet.data + 6),
        ExtractFixedPacketString(packet, 10, 24));
}

void HandleUserCount(CGameMode&, const PacketView& packet)
{
    if (!packet.data || packet.packetLength != 6) return;
    ro::UiMessageStrings strings;
    strings.Load();
    std::string message;
    if (g_serviceType == ServiceChina && !IsGravityAid(g_session.m_aid)) {
        const u16 count = ReadLE16(packet.data + 2);
        const unsigned index = count <= 1000 ? 0 : count <= 3000 ? 1
            : count <= 6000 ? 2 : count <= 10000 ? 3 : 4;
        constexpr const char* populations[] = {"Very low population.", "Low population.",
            "Normal population.", "High population.", "Very high population."};
        message = strings.Get(825 + index, populations[index]);
        if (message.empty()) message = populations[index];
    } else {
        const auto count = std::to_string(ReadLE32(packet.data + 2));
        const auto formatted = FormatLocalizedNotice(strings.Get(178, "There are %d Players Currently Connected."),
            {{'d', count}});
        message = formatted.value_or("There are " + count + " Players Currently Connected.");
    }
    g_windowMgr.SendMsg(3, 0, 0, 0, 0);
    g_windowMgr.SendMsg(1, reinterpret_cast<msgparam_t>(message.c_str()), 0xffff);
}

void HandleFriendAdd(CGameMode&, const PacketView& packet)
{
    if (!packet.data || packet.packetLength != 36) {
        return;
    }
    const auto result = ReadLE16(packet.data + 2);
    if (result > 3) return;
    const auto name = ExtractFixedPacketString(packet.data + 12, 24);
    static constexpr unsigned ids[] = {821, 822, 819, 820};
    static constexpr const char* fallback[] = {"You have become friends with (%s).",
        "(%s) does not want to be friends with you.", "Your Friend List is full.",
        "(%s)'s Friend List is full."};
    const NoticeArguments namedArguments{{'s', name}};
    const auto args = result == 2 ? NoticeArguments{} : namedArguments;
    ro::UiMessageStrings strings;
    strings.Load();
    auto notice = FormatLocalizedNotice(strings.Get(ids[result], fallback[result]), args);
    if (!notice) notice = FormatLocalizedNotice(fallback[result], args);
    if (notice) {
        g_windowMgr.SendMsg(3, 0, 0, 0, 0);
        g_windowMgr.SendMsg(1, reinterpret_cast<msgparam_t>(notice->c_str()), result == 0 ? 16776960 : 255);
    }
    if (result != 0) return;

    FRIEND_INFO info{};
    info.isValid = 1;
    info.AID = ReadLE32(packet.data + 4);
    info.GID = ReadLE32(packet.data + 8);
    info.characterName = name;
    info.state = 0;
    g_session.AddFriendToList(info);
    g_session.RefreshFriendUI();
}

void HandleFriendDelete(CGameMode&, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 10) {
        return;
    }

    if (g_session.DeleteFriendFromList(ReadLE32(packet.data + 2), ReadLE32(packet.data + 6))) {
        g_session.RefreshFriendUI(true);
    }
}

void HandleFriendRequest(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength != 34) return;
    const u32 aid = ReadLE32(packet.data + 2);
    const u32 cid = ReadLE32(packet.data + 6);
    const std::string friendName = ExtractFixedPacketString(packet.data + 10, 24);
    if (!aid || !cid || friendName.empty()) return;
    if (IsNameYellow(g_session.m_aid) || g_session.m_isNoTrade) {
        ro::commerce::SendFriendInvitationReply(aid, cid, false);
        return;
    }
    if (ro::commerce::Receive(mode, packet)) {
        mode.CloseFriendInvitation();
        mode.UpdateFriendInvitation();
        RecordChat(mode, std::string("Friend request from ") + friendName + ".", 0x00FFFF00, kChatChannelSystem);
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
    if(!packet.data || packet.packetLength!=3)return;
    const u8 status=packet.data[2];const auto& target=mode.m_lastWhisperName;
    if(status==0){
        const auto text=std::string(g_session.GetPlayerName())+" : "+mode.m_lastWhisper;
        if(g_windowMgr.AddWhisperChatToWhisperWnd(target,text,0x7800))return;
        const bool bracket=g_session.IsFriendName(target.c_str()) || g_session.IsGuildMember(target.c_str());
        const auto message=std::string(bracket?"[ To ":"( To ")+target+(bracket?" ] : ":" ) : ")+mode.m_lastWhisper;
        if(mode.m_hiddenWhisperAcknowledgements>0)--mode.m_hiddenWhisperAcknowledgements;
        else {g_windowMgr.SendMsg(3,0,0);
            RecordChat(mode,message,0xffff00,kChatChannelWhisper,{ro::chat::NoticeRoute::Normal,ro::chat::NoticeType::Whisper,target});}
        mode.WriteSourceChat(message);
    }else {
        if(status==1 && mode.m_hiddenWhisperAcknowledgements>0){--mode.m_hiddenWhisperAcknowledgements;return;}
        const int id=status==2?149:status==3?150:148;
        ro::UiMessageStrings table;table.Load();const auto label=table.Get(id,status==2?"Whispers are not accepted.":status==3?"All whispers are refused.":"Character not found.");
        const auto message="("+target+")"+label;
        if(!g_windowMgr.AddWhisperChatToWhisperWnd(target,message,0x7800))
            RecordChat(mode,message,0xffff00,kChatChannelWhisper,{ro::chat::NoticeRoute::Normal,ro::chat::NoticeType::Whisper,{}});
        if(status>3)mode.WriteSourceChat(message);
    }
}

#include "ChatIgnorePacketHandlers.inc"

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

void RequestTransitionMode(const MapTransitionServices* services, int modeType, const char* worldName)
{
    if (services && services->requestMode) {
        services->requestMode(modeType, worldName);
    } else {
        g_modeMgr.Switch(modeType, worldName);
    }
}

void ReturnToLoginMode(const MapTransitionServices* services = nullptr)
{
    ClearPendingDisconnectAction();
    g_session.ClearInventoryItems();
    g_windowMgr.ClearChatEvents();
    CRagConnection::instance()->Disconnect();
    RequestTransitionMode(services, 0, "");
}

} // namespace

#if defined(RO_PACKET_REPLAY_TESTS)
bool ReplaySendMapLoadEndAckPacket()
{
    return SendLoadEndAckPacket();
}
#endif

void ReturnToLoginModeWithStatus(const char* status)
{
    if (status && status[0] != '\0') {
        SetPendingLoginStatus(status);
    }
    ReturnToLoginMode();
}

bool ApplyDisconnectTimeoutFallback(PendingDisconnectAction expiredAction)
{
    if (expiredAction == PendingDisconnectAction::None) {
        return false;
    }
    ClearPendingDisconnectAction();
    g_session.m_pendingReturnToCharSelect =
        expiredAction == PendingDisconnectAction::ReturnToCharSelect ? 1 : 0;
    ReturnToLoginModeWithStatus("Login: disconnect timed out.");
    return true;
}

void ApplyPendingLocalCartState(CGameMode& mode)
{
    if (!mode.m_world || !mode.m_world->m_player) {
        return;
    }

    ApplyLocalCartStateToActor(mode.m_world->m_player);
}

void PrimeLocalCartStateFromEffectState(int effectState)
{
    UpdateKnownLocalCartState(IsEffectStatePushCart(effectState), effectState);
}

bool IsKnownLocalCartActive()
{
    return g_localCartInfoKnown && (g_localCartEffectStateMask & kEffectStatePushCartMask) != 0;
}

bool BeginPendingDisconnectAction(PendingDisconnectAction action)
{
    if (action == PendingDisconnectAction::None
        || g_pendingDisconnectAction != PendingDisconnectAction::None) {
        return false;
    }
    g_pendingDisconnectAction = action;
    g_pendingDisconnectStartTick = 0;
    return true;
}

void MarkPendingDisconnectActionSent(u32 startTick)
{
    if (g_pendingDisconnectAction != PendingDisconnectAction::None) {
        g_pendingDisconnectStartTick = startTick;
    }
}

PendingDisconnectAction GetPendingDisconnectAction()
{
    return g_pendingDisconnectAction;
}

PendingDisconnectAction GetExpiredDisconnectAction(bool receiveDrained, u32 nowTick)
{
    if (!receiveDrained || g_pendingDisconnectAction == PendingDisconnectAction::None) {
        return PendingDisconnectAction::None;
    }
    if (static_cast<u32>(nowTick - g_pendingDisconnectStartTick) < kDisconnectAckTimeoutMs) {
        return PendingDisconnectAction::None;
    }
    return g_pendingDisconnectAction;
}

bool ApplyDisconnectTimeoutAfterPacketLoop(const GameModePacketLoopResult& packetLoop, u32 nowTick)
{
    if (packetLoop.modeOperationRequested) {
        return false;
    }
    const PendingDisconnectAction expiredAction = GetExpiredDisconnectAction(
        packetLoop.receiveDrained,
        nowTick);
    return ApplyDisconnectTimeoutFallback(expiredAction);
}

void ClearPendingDisconnectAction()
{
    g_pendingDisconnectAction = PendingDisconnectAction::None;
    g_pendingDisconnectStartTick = 0;
}

DisconnectAckDecision DecideDisconnectAck(PendingDisconnectAction action, u16 result)
{
    if (result == 1 && action != PendingDisconnectAction::None) {
        return DisconnectAckDecision::Refused;
    }
    if (result == 0 && action == PendingDisconnectAction::ExitToWindows) {
        return DisconnectAckDecision::ExitToWindows;
    }
    return DisconnectAckDecision::Ignore;
}

bool IsReturnToCharSelectAck(PendingDisconnectAction action, u8 result)
{
    return action == PendingDisconnectAction::ReturnToCharSelect && result == 1;
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

    const PendingDisconnectAction pendingAction = GetPendingDisconnectAction();
    if (!IsReturnToCharSelectAck(pendingAction, packet.data[2])) {
        DbgLog("[GameMode] ignored 0x00B3 type=%u pending=%d\n",
            static_cast<unsigned int>(packet.data[2]),
            static_cast<int>(pendingAction));
        return;
    }

    ClearPendingDisconnectAction();
    g_session.m_pendingReturnToCharSelect = 1;
    ReturnToLoginMode();
}

void HandleDisconnectAck(CGameMode&, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 4) {
        DbgLog("[GameMode] ignored truncated 0x018B packet len=%u\n",
            static_cast<unsigned int>(packet.packetLength));
        return;
    }

    const u16 result = ReadLE16(packet.data + 2);
    const PendingDisconnectAction action = GetPendingDisconnectAction();
    switch (DecideDisconnectAck(action, result)) {
    case DisconnectAckDecision::Refused:
        ClearPendingDisconnectAction();
        g_windowMgr.PushChatEvent(
            "The server refused the disconnect request.",
            0x00FF4040u,
            kChatChannelSystem);
        break;
    case DisconnectAckDecision::ExitToWindows:
        ClearPendingDisconnectAction();
        g_session.m_pendingReturnToCharSelect = 0;
        CRagConnection::instance()->Disconnect();
        g_modeMgr.Quit();
        break;
    case DisconnectAckDecision::Ignore:
        DbgLog("[GameMode] ignored 0x018B result=%u pending=%d\n",
            static_cast<unsigned int>(result),
            static_cast<int>(action));
        break;
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

    mode.ClearNpcProgress();
    mode.m_compassPosList.clear();
    const bool sameMapAlreadyLoaded = !info.hasServerMove && IsSameMapAlreadyLoaded(mode, info);
    ro::commerce::CloseTransactions();
    mode.CloseFriendInvitation();
    mode.ClearAdoptionInvitation();
    ApplyMapChangeSessionState(info);

    DbgLog("[GameMode] map change packet=0x%04X map='%s' pos=%d,%d serverMove=%d\n",
        packet.packetId,
        g_session.m_curMap,
        info.x,
        info.y,
        info.hasServerMove ? 1 : 0);

    if (sameMapAlreadyLoaded) {
        ro::commerce::ResetForMapEntry();
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

    const MapTransitionServices* const services = mode.GetMapTransitionServices();
    if (services && services->presentLoading) {
        services->presentLoading("Loading new map...", 0.01f);
    } else {
        g_modeMgr.PresentLoadingScreen("Loading new map...", 0.01f);
    }

    // Pending request state belongs to the real transition, not the injected transport.
    if (info.hasServerMove) ClearPendingDisconnectAction();
    const bool reconnected = !info.hasServerMove
        || (services && services->reconnect
            ? services->reconnect(info.ip, info.port)
            : ReconnectForServerMove(info));
    if (!reconnected) {
        g_session.m_pendingReturnToCharSelect = 0;
        ReturnToLoginMode(services);
        return;
    }

    char worldName[40] = {};
    std::snprintf(worldName, sizeof(worldName), "%s.rsw", g_session.m_curMap);
    RequestTransitionMode(services, 1, worldName);
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

    if (ro::net::IsActiveLegacyActorSpawnPacket(packet.packetId)) {
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
        ApplyRuntimeActorFixPosition(mode, gid, dx, dy);
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

    if (ro::net::IsActiveLegacyActorMovePacket(packet.packetId)) {
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
    // Ref Zc_Notify_Move: the walk tick is the dword after the 6-byte pos.
    const u32 moveStartTime = packet.packetLength >= 16
        ? ReadLE32(packet.data + 12)
        : g_session.GetServerTime();
    if (moveStartTime != 0) {
        g_session.UpdateServerTime(moveStartTime);
    }

    // eAthena uses 0x0086 for any visible walking object. Keep a billboard-capable
    // shell for move-only actors, but only seed player appearance when the id falls
    // inside the account-id range used for PCs.
    const bool likelyPlayer = IsLikelyPlayerGid(gid);
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
    UpdateRuntimeActorPosition(mode, gid, dx, dy, sx, sy, moveStartTime, cellX, cellY);
}

void HandleSelfMoveAck(CGameMode& mode, const PacketView& packet)
{
    // Classic eAthena self walk acknowledgment (0x0087): start tick + packed src/dst.
    if (!packet.data || packet.packetLength < 12 || g_session.m_gid == 0) {
        return;
    }

    const u32 moveStartTime = ReadLE32(packet.data + 2);
    g_session.SetServerTime(moveStartTime);

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
    UpdateRuntimeActorPosition(mode, g_session.m_gid, dx, dy, sx, sy, moveStartTime, cellX, cellY);
}

void HandleActorSetPosition(CGameMode& mode, const PacketView& packet)
{
    LogActorPacketSeenOnce(packet);
    if (mode.m_mapLoadingStage != CGameMode::MapLoading_None) {
        mode.m_lastActorBootstrapPacketTick = GetTickCount();
    }

    if (!ro::net::IsActiveActorSetPositionPacket(packet.packetId)) {
        return;
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

    // ZC_STOPMOVE is authoritative even on the current/previous path cell.
    // rAthena sends it before casting; dropping it leaves the old walk alive.
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
    if (const auto groundItemIt = mode.m_groundItemList.find(gid); groundItemIt != mode.m_groundItemList.end()) {
        DbgLog("[GroundItem] vanish overlaps item obj=%u item=%u amount=%u reason=%u\n",
            gid,
            groundItemIt->second.itemId,
            static_cast<unsigned int>(groundItemIt->second.amount),
            static_cast<unsigned int>(reason));
    }
    mode.m_actorPosList.erase(gid);
    mode.m_aidList.erase(gid);

    if (reason == 0) {
        DbgLog("[GameMode] remove vanish gid=%u reason=0\n", gid);
        RemoveRuntimeActor(mode, gid);
    } else if (reason == 1) {
        if (CGameActor* actor = FindRuntimeActorForVanish(mode, gid)) {
            BeginActorDeath(mode, *actor, gid);
            if (IsLocalPlayerActor(mode, gid)) {
                // Selected pc_dead deletes the mercenary and elemental before
                // the owner death packet, with only visibility removals for them.
                mode.RetireMercenaryContract();
                g_session.m_elementalStatus = {};
                mode.m_isOnQuest = 1;
                mode.m_isPlayerDead = 1;
                for (auto it = mode.m_compassPosList.begin(); it != mode.m_compassPosList.end();) {
                    if (it->second.type != 0) {
                        it = mode.m_compassPosList.erase(it);
                    } else {
                        ++it;
                    }
                }
                if (g_serviceType != ServiceTaiwan && mode.CanRequestStandingResurrection()) {
                    g_windowMgr.MakeWindow(UIWindowMgr::WID_CHOOSEWND);
                }
            }

            DbgLog("[GameMode] death vanish gid=%u self=%d despawnAt=%u\n",
                gid,
                IsLocalPlayerActor(mode, gid) ? 1 : 0,
                static_cast<unsigned int>(actor->m_vanishTime));
        } else {
            if (gid && gid == g_session.m_homunGID) mode.DeactivateHomunculus();
            RemoveRuntimeActor(mode, gid);
        }
    } else {
        RemoveRuntimeActor(mode, gid);
    }

    if (mode.m_lastPcGid == gid) {
        mode.m_lastPcGid = 0;
    }
    if (mode.m_selectedPcTargetGid == gid) {
        mode.m_selectedPcTargetGid = 0;
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

        if (actor->m_vanishTime == 0 || actor->m_vanishTime > now) {
            continue;
        }

        // Firebolt and other hit effects are launched on the target. Removing
        // the actor here deletes those effects with it. Keep the faded actor
        // until its effect list drains so the remaining bolts finish.
        bool effectStillRunning = false;
        for (CRagEffect* effect : actor->m_effectList) {
            if (effect) {
                effectStillRunning = true;
                break;
            }
        }
        if (effectStillRunning) {
            actor->m_isVisible = 0;
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

void HandleNpcClassChange(CGameMode& mode, const PacketView& packet)
{
    if (!mode.m_world || !packet.data || packet.packetLength != 11) {
        return;
    }

    const u32 gid = ReadLE32(packet.data + 2);
    // The original receiver passes zero to SetSprAct and skips byte 6.
    const u32 wireJob = ReadLE32(packet.data + 7);
    if (gid == 0 || wireJob > static_cast<u32>((std::numeric_limits<int>::max)())) {
        return;
    }

    auto* actor = dynamic_cast<CPc*>(mode.m_world->GetGameActorByAID(gid));
    const int job = static_cast<int>(wireJob);
    if (!actor || actor->m_isPc || actor->m_job == job) {
        return;
    }

    actor->ReplaceNpcClass(job);
    mode.m_world->InvalidateBillboardFrameCache();
}

void HandleActorSpriteChange(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength < 11) {
        return;
    }

    const u32 gid = ReadLE32(packet.data + 2);
    const u8 type = packet.data[6];
    const u32 value = type == kLookWeapon ? ReadLE32(packet.data + 7) : ReadLE16(packet.data + 7);
    if (gid == 0) {
        return;
    }

    mode.m_aidList[gid] = GetTickCount();

    CSkill* skillUnit = ResolveSkillPacketUnit(mode, gid);
    CGameActor* actor = ResolveSkillPacketActor(mode, gid);
    if (!actor && skillUnit) {
        if (type == kLookBase) {
            skillUnit->SendMsg(skillUnit, 47, value, 0, 0);
        }
        return;
    }
    if (!actor) {
        actor = EnsureRuntimeActor(mode, gid, IsLikelyPlayerGid(gid));
    }
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
        actor->m_headType = actor->m_objectType == 7 ? 100 : static_cast<int>(value);
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

    if (skillUnit && type == kLookBase) {
        skillUnit->SendMsg(skillUnit, 47, value, 0, 0);
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

void HandleCharacterNameAck(CGameMode& mode, const PacketView& packet)
{
    if (!packet.data || packet.packetLength != 30) return;
    const u32 characterId = ReadLE32(packet.data + 2);
    const std::string name = ExtractFixedPacketString(packet, 6, 24);
    if (characterId == 0 || name.empty()) return;
    mode.m_characterNameCache[characterId] = name;
    mode.m_characterNameReqTimer.erase(characterId);
    if (g_windowMgr.m_itemWnd) g_windowMgr.m_itemWnd->Invalidate();
    if (g_windowMgr.m_storageWnd) g_windowMgr.m_storageWnd->Invalidate();
    if (g_windowMgr.m_equipWnd) g_windowMgr.m_equipWnd->Invalidate();
    if (g_windowMgr.m_itemInfoWnd) g_windowMgr.m_itemInfoWnd->Invalidate();
    if (g_windowMgr.m_shortCutWnd) g_windowMgr.m_shortCutWnd->Invalidate();
    // ZC_ACK_REQNAME_BYGID carries a character ID, not an actor's world ID.
    // Never create an actor, overwrite actor affiliations or complete its query.
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

    auto updateEntry = [&](NamePair& entry) {
        entry.name = name;
        entry.nick.clear();
        entry.partyName.clear();
        entry.guildName.clear();
        entry.guildTitle.clear();

        if (ro::net::IsActiveActorNameAckFullPacket(packet.packetId) && packet.packetLength >= 102) {
            entry.partyName = ExtractFixedPacketString(packet, 30, 24);
            entry.guildName = ExtractFixedPacketString(packet, 54, 24);
            entry.guildTitle = ExtractFixedPacketString(packet, 78, 24);
        }
    };

    updateEntry(mode.m_actorNameListByGID[gid]);
    updateEntry(mode.m_actorNameList[gid]);

    const NamePair& entry = mode.m_actorNameListByGID[gid];
    if((gid==g_session.m_gid || gid==g_session.m_aid)
        && ro::net::IsActiveActorNameAckFullPacket(packet.packetId) && packet.packetLength>=102) {
        auto& guild=g_session.m_localGuild;
        if(guild.characterId!=g_session.m_gid)guild={};
        guild.characterId=g_session.m_gid;
        guild.name=entry.guildName;guild.title=entry.guildTitle;
        if(guild.name.empty()){guild.guildId=0;guild.emblemVersion=0;}
    }
    mode.m_actorNameByGIDReqTimer.erase(gid);

    DbgLog("[GameMode] name ack gid=%u name='%s' party='%s' guild='%s' title='%s'\n",
        gid,
        entry.name.c_str(),
        entry.partyName.c_str(),
        entry.guildName.c_str(),
        entry.guildTitle.c_str());

    for (const FRIEND_INFO& member : g_session.GetPartyList()) {
        if (member.characterName == entry.name && member.partyMaxHp > 0) {
            ApplyCachedPartyHpToVisibleActor(mode, member.AID, member.characterName);
            break;
        }
    }
}

} // namespace

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
    const ro::net::MapReceiveProfile& receiveProfile = ro::net::GetActiveMapReceiveProfile();

    // Character enter/map-ready chain.
    RegisterHandlerIfValid(router, receiveProfile.acceptEnterLegacy, HandleAcceptEnter);
    RegisterHandlerIfValid(router, receiveProfile.notifyTime, HandleNotifyTime);
    RegisterHandlerIfValid(router, receiveProfile.acceptEnterModern, HandleAcceptEnter2);

    // Login/session bootstrap.
    router.Register(0x0069, HandleLoginAccept);
    router.Register(0x006A, HandleLoginRefuse);

    // Map-change skeleton hooks.
    RegisterHandlerIfValid(router, receiveProfile.mapChangeBasic, HandleMapChange);
    RegisterHandlerIfValid(router, receiveProfile.mapChangeServerMove, HandleMapChange);

    // Actor spawn/move skeleton hooks.
    RegisterHandlerIfValid(router, receiveProfile.actorSpawnLegacyIdle, HandleActorSpawnSkeleton);
    RegisterHandlerIfValid(router, receiveProfile.actorSpawnLegacySpawn, HandleActorSpawnSkeleton);
    RegisterHandlerIfValid(router, receiveProfile.actorSpawnLegacyAlt, HandleActorSpawnSkeleton);
    RegisterHandlerIfValid(router, receiveProfile.actorSpawnLegacyNpc, HandleActorSpawnSkeleton);
    RegisterHandlerIfValid(router, receiveProfile.actorSpawnLegacyIdleShifted, HandleActorSpawnSkeleton);
    RegisterHandlerIfValid(router, receiveProfile.actorSpawnLegacySpawnShifted, HandleActorSpawnSkeleton);
    RegisterHandlerIfValid(router, receiveProfile.actorMoveLegacyShifted, HandleActorMoveSkeleton);
    RegisterHandlerIfValid(router, receiveProfile.actorMoveLegacy, HandleActorMoveSkeleton);
    router.Register(0x0080, HandleActorVanish);
    router.Register(0x0086, HandleActorMoveUpdate);
    router.Register(0x0087, HandleSelfMoveAck);
    RegisterHandlerIfValid(router, receiveProfile.actorActionNotifyBasic, HandleActorActionNotify);
    RegisterHandlerIfValid(router, receiveProfile.actorSetPositionBasic, HandleActorSetPosition);
    RegisterHandlerIfValid(router, receiveProfile.actorSetPositionHighJump, HandleActorSetPosition);
    router.Register(0x009C, HandleActorDirection);
    router.Register(0x010E, HandlePlayerSkillUpdate);
    router.Register(0x010F, HandlePlayerSkillList);
    router.Register(0x0110, HandleSkillFailAck);
    router.Register(0x0111, HandlePlayerSkillAdd);
    router.Register(0x011C, HandleWarpMapList);
    router.Register(0x011E, HandleMemoResult);
    router.Register(0x018C, HandleMonsterEstimate);
    router.Register(0x01AD, HandleMakingArrowList);
    router.Register(0x01A2, HandlePetStatus);
    router.Register(0x01A0, HandlePetCaptureResult);
    router.Register(0x019E, HandlePetCaptureArm);
    router.Register(0x01A6, HandlePetEggList);
    router.Register(0x01A4, HandlePetStatusChange);
    router.Register(0x01A3, HandlePetFeedResult);
    router.Register(0x01AA, HandlePetAction);
    router.Register(0x022E, HandleHomunProperty);
    router.Register(0x022F, HandleHomunFeedResult);
    router.Register(0x0230, HandleHomunStatus);
    router.Register(0x0235, HandleHomunSkillList);
    router.Register(0x0239, HandleHomunSkillUpdate);
    router.Register(0x0291, HandleLocalizedSystemNotice);
    router.Register(0x029B, HandleMercenaryInit);
    router.Register(0x029D, HandleMercSkillList);
    router.Register(0x029E, HandleMercSkillUpdate);
    router.Register(0x02A2, HandleMercenaryParameter);
    RegisterHandlerIfValid(router, receiveProfile.skillDamageNotifyBasic, HandleSkillDamageNotify);
    RegisterHandlerIfValid(router, receiveProfile.skillDamagePositionNotify, HandleSkillDamagePositionNotify);
    RegisterHandlerIfValid(router, receiveProfile.groundSkillNotify, HandleGroundSkillNotify);
    RegisterHandlerIfValid(router, receiveProfile.actorStateChangeBasic, HandleActorStateChange);
    RegisterHandlerIfValid(router, receiveProfile.skillNoDamageNotify, HandleSkillNoDamageNotify);
    router.Register(0x0139, HandleAttackFailureForDistance);
    router.Register(0x013A, HandleAttackRange);
    router.Register(0x0215, HandleSkillMessage);
    router.Register(0x020E, HandleStarSkill);
    router.Register(0x0253, HandleStarPlace);
    RegisterHandlerIfValid(router, receiveProfile.actorStateChangeExtended, HandleActorStateChange);
    router.Register(0x0121, HandleCartInfo);
    router.Register(0x012B, HandleCartOff);

    router.Register(0x00d6,HandleRoomCreateAck);
    router.Register(0x00d7,HandleRoomListing);
    router.Register(0x00d8,HandleRoomDestroy);
    router.Register(0x00da,HandleRoomEnterRefused);
    router.Register(0x00db,HandleRoomEntry);
    router.Register(0x00dc,HandleRoomMemberEnter);
    router.Register(0x00dd,HandleRoomMemberExit);
    router.Register(0x00df,HandleRoomListing);
    router.Register(0x00e1,HandleRoomRole);

    // Chat/system text pathways.
    router.Register(0x008D, HandleNotifyChat);
    router.Register(0x008E, HandleNotifyPlayerChat);
    router.Register(0x0109, HandleNotifyChatParty);
    router.Register(0x0097, HandleWhisper);
    router.Register(0x0098, HandleAckWhisper);
    router.Register(0x00D1, HandleWhisperSettingPc);
    router.Register(0x00D2, HandleWhisperSettingState);
    router.Register(0x00D4, HandleWhisperExclusionList);
    router.Register(0x02C1, HandleNpcChat);
    RegisterHandlerIfValid(router, receiveProfile.broadcastBasic, HandleBroadcast);
    router.Register(0x00B4, HandleNpcDialogText);
    router.Register(0x00B5, HandleNpcDialogNext);
    router.Register(0x00B6, HandleNpcDialogClose);
    router.Register(0x00B7, HandleNpcDialogMenu);
    router.Register(0x00C2, HandleUserCount);
    router.Register(0x00C4, HandleNpcShopDealType);
    router.Register(0x00C6, HandleNpcShopBuyList);
    router.Register(0x00C7, HandleNpcShopSellList);
    router.Register(0x00CA, HandleNpcShopBuyResult);
    router.Register(0x00CB, HandleNpcShopSellResult);
    router.Register(0x0142, HandleNpcDialogNumberInput);
    router.Register(0x01D4, HandleNpcDialogStringInput);
    router.Register(0x0144, HandleCompass);
    router.Register(0x0189, HandleNotifyMapInfo);
    RegisterHandlerIfValid(router, receiveProfile.broadcastColored, HandleBroadcast);
    router.Register(0x02DC, HandleBattlefieldChat);
    RegisterHandlerIfValid(router, receiveProfile.actorNameAckBasic, HandleActorNameAck);
    RegisterHandlerIfValid(router, receiveProfile.characterNameAck, HandleCharacterNameAck);
    RegisterHandlerIfValid(router, receiveProfile.actorNameAckFull, HandleActorNameAck);

    RegisterHandlerIfValid(router, receiveProfile.groundItemEntryExisting, HandleGroundItemEntry);
    RegisterHandlerIfValid(router, receiveProfile.groundItemEntryDropped, HandleGroundItemEntry);
    RegisterHandlerIfValid(router, receiveProfile.itemPickupAckBasic, HandleItemPickupAck);
    RegisterHandlerIfValid(router, receiveProfile.itemPickupAckExtended, HandleItemPickupAck);
    router.Register(0x00A1, HandleGroundItemDisappear);
    RegisterHandlerIfValid(router, receiveProfile.normalInventoryListBasic, HandleNormalInventoryList);
    RegisterHandlerIfValid(router, receiveProfile.normalInventoryListCardSlots, HandleNormalInventoryList);
    RegisterHandlerIfValid(router, receiveProfile.normalInventoryListTimed, HandleNormalInventoryList);
    RegisterHandlerIfValid(router, receiveProfile.equipInventoryListBasic, HandleEquipInventoryList);
    RegisterHandlerIfValid(router, receiveProfile.equipInventoryListTimed, HandleEquipInventoryList);
    RegisterHandlerIfValid(router, receiveProfile.equipInventoryListTimedOwned, HandleEquipInventoryList);
    RegisterHandlerIfValid(router, receiveProfile.normalStorageListBasic, HandleNormalStorageList);
    RegisterHandlerIfValid(router, receiveProfile.normalStorageListCardSlots, HandleNormalStorageList);
    RegisterHandlerIfValid(router, receiveProfile.normalStorageListTimed, HandleNormalStorageList);
    RegisterHandlerIfValid(router, receiveProfile.equipStorageListBasic, HandleEquipStorageList);
    RegisterHandlerIfValid(router, receiveProfile.equipStorageListTimedOwned, HandleEquipStorageList);
    router.Register(0x00AA, HandleEquipItemAck);
    RegisterHandlerIfValid(router, receiveProfile.itemRemoveBasic, HandleItemRemove);
    RegisterHandlerIfValid(router, receiveProfile.rentalTimeCounter, HandleRentalItem);
    RegisterHandlerIfValid(router, receiveProfile.rentalItemDelete, HandleRentalItem);
    router.Register(0x00AC, HandleUnequipItemAck);
    RegisterHandlerIfValid(router, receiveProfile.useItemAckBasic, HandleUseItemAck);
    router.Register(0x0147, HandleAutoRunSkill);
    router.Register(0x017B, HandleItemCompositionList);
    router.Register(0x0177, HandleItemIdentifyList);
    router.Register(0x017D, HandleItemCompositionAck);
    router.Register(0x0179, HandleItemIdentifyAck);
    router.Register(0x00F2, HandleStorageStatus);
    RegisterHandlerIfValid(router, receiveProfile.storageItemAddedBasic, HandleStorageItemAdded);
    router.Register(0x00F6, HandleStorageItemRemoved);
    router.Register(0x00F8, HandleStorageClose);
    RegisterHandlerIfValid(router, receiveProfile.partyInviteAckBasic, HandlePartyInviteAck);
    RegisterHandlerIfValid(router, receiveProfile.partyInviteRequestBasic, HandlePartyInviteRequest);
    router.Register(0x00FA, HandlePartyCreateAck);
    router.Register(0x00FB, HandlePartyList);
    RegisterHandlerIfValid(router, receiveProfile.useItemAckExtended, HandleUseItemAck2);
    RegisterHandlerIfValid(router, receiveProfile.storageItemAddedTyped, HandleStorageItemAdded);
    router.Register(0x01F8, HandleIgnorePacket);
    router.Register(0x023A, HandleStoragePasswordRequest);
    router.Register(0x023C, HandleStoragePasswordResult);
    router.Register(0x0284, HandleNotifyEffect3);
    router.Register(0x0191, HandleTalkboxChatContents);
    router.Register(0x01B4, HandleChangeGuild);
    for(u16 id:{0x01eb,0x017f,0x0171,0x0173,0x0181,0x015e,0x015f})router.Register(id,HandleGuildNotification);
    router.Register(0x0152, HandleGuildEmblem);
    for(u16 id:{0x0122,0x0123,0x01ef,0x0297,0x02d2,0x02e9,0x0124,0x01c5,0x0125,0x012c,
        0x012d,0x0131,0x0132,0x0133,0x0135,0x0136,0x0137,0x0138,
        0x00e5,0x01f4,0x00e7,0x01f5,0x00e9,0x00ea,0x00ec,0x00ee,0x00f0,0x00f1,
        0x016a,0x0169,0x0249,0x02d7})
        router.Register(id,[](CGameMode& mode,const PacketView& packet){ro::commerce::Receive(mode,packet);});
    router.Register(0x0167, HandleGuildCreateAck);
    router.Register(0x016C, HandleGuildBelong);
    router.Register(0x015A,HandleGuildDeparture);
    router.Register(0x015C,HandleGuildDeparture);
    for(u16 id:{0x014c,0x014e,0x0150,0x0154,0x0156,0x0160,0x0163,0x0166,0x016d,0x016f,0x0174,0x01b6,0x0176,0x0182,0x0184,0x0185,0x01f2})router.Register(id,HandleGuildWindowPacket);
    router.Register(0x0145, HandleShowImage);
    router.Register(0x01B3, HandleShowImage2);
    router.Register(0x0162, HandleGuildSkillInfo);
    router.Register(0x01CD, HandleAutoSpellList);
    RegisterHandlerIfValid(router, receiveProfile.itemRemoveExtended, HandleItemRemove);
    router.Register(0x0100, HandleIgnorePacket);

    // Quit/return-to-login transitions.
    router.Register(0x0081, HandleBanDisconnect);
    router.Register(0x00B3, HandleDisconnectCharacterAck);
    router.Register(0x018B, HandleDisconnectAck);

    // Common server-side status/config packets that are safe to ignore for now,
    // but must stay in sync so later actor packets are framed correctly.
    router.Register(0x00B0, HandleSelfStatusParam);
    router.Register(0x00B1, HandleSelfStatusParam);
    router.Register(0x00BC, HandleStatusChangeAck);
    router.Register(0x00BD, HandleInitialStatusSummary);
    router.Register(0x00BE, HandleStatusPointCostUpdate);
    router.Register(0x00C0, HandleEmotion);
    router.Register(0x0101, HandlePartyOptionChanged);
    router.Register(0x013D, HandleRecovery);
    router.Register(0x013B, HandleActionFailure);
    router.Register(0x013C, HandleEquipAmmo);
    router.Register(0x013E, HandleSkillCastAck);
    router.Register(0x01B0, HandleNpcClassChange);
    RegisterHandlerIfValid(router, receiveProfile.partyHpUpdateBasic, HandlePartyHpUpdate);
    router.Register(0x0107, HandlePartyPosition);
    RegisterHandlerIfValid(router, receiveProfile.partyMemberAddedBasic, HandlePartyMemberAdded);
    router.Register(0x0105, HandlePartyMemberRemoved);
    RegisterHandlerIfValid(router, receiveProfile.partyMemberAddedExtended, HandlePartyMemberAdded);
    RegisterHandlerIfValid(router, receiveProfile.skillUnitSetBasic, HandleSkillUnitSet);
    router.Register(0x0120, HandleSkillDisappear);
    router.Register(0x0141, HandleSelfStatInfo);
    router.Register(0x0148, HandleActorResurrection);
    RegisterHandlerIfValid(router, receiveProfile.notifyEffectBasic, HandleNotifyEffect);
    router.Register(0x01B9, HandleSkillCastCancel);
    router.Register(0x0192, HandleUpdateMapInfo);
    RegisterHandlerIfValid(router, receiveProfile.skillUnitSetExtended, HandleSkillUnitSet2);
    router.Register(0x01AC, HandleSkillUpdate);
    router.Register(0x01CF, HandleDevotionList);
    router.Register(0x01D0, HandleSpirits);
    router.Register(0x01D3, HandleSound);
    router.Register(0x01D2, HandleComboDelay);
    router.Register(0x01D1, HandleBladeStop);
    router.Register(0x0199, HandleNotifyMapProperty);
    router.Register(0x01D6, HandleNotifyMapProperty2);
    router.Register(0x01D7, HandleActorSpriteChange);
    RegisterHandlerIfValid(router, receiveProfile.skillDamageNotifyExtended, HandleSkillDamageNotify);
    router.Register(0x01E1, HandleSpirits2);
    RegisterHandlerIfValid(router, receiveProfile.notifyEffectDirect, HandleNotifyEffect2);
    router.Register(0x0201, HandleFriendsList);
    router.Register(0x0205, HandleDivorceNotice);
    router.Register(0x01E6, HandleRecallPartnerName);
    router.Register(0x01EA, HandleWeddingEffect);
    router.Register(0x01F6, HandleAdoptionInvitation);
    router.Register(0x0216, HandleAdoptionResult);
    router.Register(0x0206, HandleFriendState);
    router.Register(0x0207, HandleFriendRequest);
    router.Register(0x0209, HandleFriendAdd);
    router.Register(0x020A, HandleFriendDelete);
    router.Register(0x0214, HandleGmStatusSummary);
    RegisterHandlerIfValid(router, receiveProfile.partyInviteAckExtended, HandlePartyInviteAck);
    router.Register(0x02D9, HandlePrivacyConfiguration);
    router.Register(0x02DD, HandleIgnorePacket);
    RegisterHandlerIfValid(router, receiveProfile.partyHpUpdateExtended, HandlePartyHpUpdate);
    router.Register(0x0283, HandleIgnorePacket);
    RegisterHandlerIfValid(router, receiveProfile.partyInviteRequestExtended, HandlePartyInviteRequest);
    router.Register(0x02C9, HandlePartyInvitePrivacy);
    router.Register(0x02B1, HandleIgnorePacket);
    router.Register(0x02B2, HandleIgnorePacket);
    router.Register(0x02B9, HandleShortcutKeyList);
    router.Register(0x02D3, HandleIgnorePacket);
    router.Register(0x02D5, HandleIgnorePacket);
    router.Register(0x02DA, HandleEquipmentVisibility);
    RegisterHandlerIfValid(router, receiveProfile.actorActionNotifyExtended, HandleActorActionNotify);
    router.Register(0x02E7, HandleMapPropertyRowUpdate);
    router.Register(0x02F0, HandleNpcProgress);
    router.Register(0x02F2, HandleNpcProgressCancel);
    router.Register(0x0196, HandlePacket0196);
    router.Register(0x043F, HandlePacket043F);
    router.Register(0x028A, HandleNpcShowEfstUpdate);
    router.Register(0x0814, HandleIgnorePacket);
    router.Register(0x0816, HandleIgnorePacket);
    if (receiveProfile.id == ro::net::PacketVersionId::PacketVer23) {
        router.Register(0x081D, HandleElementalInfo);
    }

    // Newer actor entry packet family from Ref switch (v4 set).
    RegisterHandlerIfValid(router, receiveProfile.actorSpawnModernIdle, HandleActorSpawnSkeleton);
    RegisterHandlerIfValid(router, receiveProfile.actorSpawnModernSpawn, HandleActorSpawnSkeleton);
    RegisterHandlerIfValid(router, receiveProfile.actorMoveModern, HandleActorMoveSkeleton);
    RegisterHandlerIfValid(router, receiveProfile.actorMoveModernFont, HandleActorMoveSkeleton);
    RegisterHandlerIfValid(router, receiveProfile.actorSpawnModernSpawnFont, HandleActorSpawnSkeleton);
    RegisterHandlerIfValid(router, receiveProfile.actorSpawnModernIdleFont, HandleActorSpawnSkeleton);
    router.Register(0x02EF, HandleActorFont);
    RegisterHandlerIfValid(router, receiveProfile.actorMoveVariable, HandleActorMoveSkeleton);
    RegisterHandlerIfValid(router, receiveProfile.actorSpawnVariableSpawn, HandleActorSpawnSkeleton);
    RegisterHandlerIfValid(router, receiveProfile.actorSpawnVariableIdle, HandleActorSpawnSkeleton);
    RegisterHandlerIfValid(router, receiveProfile.actorMoveVariableRobe, HandleActorMoveSkeleton);
    RegisterHandlerIfValid(router, receiveProfile.actorSpawnVariableIdleRobe, HandleActorSpawnSkeleton);
    RegisterHandlerIfValid(router, receiveProfile.actorSpawnVariableSpawnRobe, HandleActorSpawnSkeleton);
}

void NoteLocalHideSkillRequest(u16 skillId, u32 targetGid)
{
    RecordLocalHideSkillRequest(skillId, targetGid);
}

bool CGameMode::MakeCorpse(CGameActor& actor)
{
    if (actor.m_corpseCreated) return true;
    if (!m_world || m_world->m_player == &actor) return false;
    const auto owner = m_runtimeActors.find(actor.m_gid);
    if (owner == m_runtimeActors.end() || owner->second != &actor) return false;
    std::unique_ptr<CGameObject> corpse;
    if (Gr2MonsterModel(actor.m_job))
        corpse = std::make_unique<CGrannyCorpse3d>(actor.m_job, actor.m_pos,
            Gr2MonsterCurrentRotation(actor.m_gid, actor.m_roty));
    else
        corpse = std::make_unique<CCorpse>(actor.m_pos, actor.m_actRes, actor.m_sprRes,
            actor.m_baseAction, actor.m_curMotion, actor.m_roty);
    // The original owning game-object sweep reaches the newly appended corpse
    // in this step. Actors are a separate pass here, so initialize its frame now.
    corpse->OnProcess();
    m_world->m_gameObjectList.push_back(corpse.release());
    actor.m_corpseCreated = true;
    RemoveRuntimeActor(*this, actor.m_gid);
    return true;
}
