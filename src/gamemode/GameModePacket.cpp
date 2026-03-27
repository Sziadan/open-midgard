#include "GameModePacket.h"

#include "network/GronPacket.h"
#include "network/Packet.h"
#include "GameMode.h"
#include "core/Globals.h"
#include "core/File.h"
#include "pathfinder/PathFinder.h"
#include "session/Session.h"
#include "item/Item.h"
#include "Mode.h"
#include "DebugLog.h"
#include "audio/Audio.h"
#include "ui/UIWindowMgr.h"
#include "world/GameActor.h"
#include "world/World.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdio>
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

PendingDisconnectAction g_pendingDisconnectAction = PendingDisconnectAction::None;
u32 g_lastLocalLevelUpEffectId = 0;
u32 g_lastLocalLevelUpEffectTick = 0;
u32 g_lastSelfLevelUpStatusTick = 0;
u32 g_lastSelfLevelUpStatusType = 0;

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
constexpr u32 kStatusJobLevel = 55;
constexpr u32 kNotifyEffectBaseLevelUp = 0;
constexpr u32 kNotifyEffectJobLevelUp = 1;
constexpr u32 kNotifyEffectBaseLevelUpSuperNovice = 7;
constexpr u32 kNotifyEffectJobLevelUpSuperNovice = 8;
constexpr u32 kNotifyEffectBaseLevelUpTaekwon = 9;
constexpr u32 kEffectIdBaseLevelUp = 371;
constexpr u32 kEffectIdJobLevelUp = 158;
constexpr u32 kEffectIdBaseLevelUpSuperNovice = 338;
constexpr u32 kEffectIdJobLevelUpSuperNovice = 337;
constexpr u32 kEffectIdBaseLevelUpTaekwon = 582;
constexpr u32 kLocalLevelUpNotifySuppressMs = 1200;
constexpr u32 kDeathFadeDurationMs = 510;
constexpr u32 kDeathCorpseHoldMs = 1290;

u16 ReadLE16(const u8* data);
u32 ReadLE32(const u8* data);

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

bool ShouldSuppressSelfNotifyEffect(u32 actorId, u32 effectId)
{
    if (actorId != g_session.m_aid && actorId != g_session.m_gid) {
        return false;
    }

    const u32 elapsed = GetTickCount() - g_lastLocalLevelUpEffectTick;
    return g_lastLocalLevelUpEffectId == effectId && elapsed <= kLocalLevelUpNotifySuppressMs;
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
    LaunchLevelUpEffect(actor, effectId);
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

    actor->SendMsg(actor, 84, 0, 0, 0);
    actor->SendMsg(actor, 87, 0, 0, 0);
    actor->SendMsg(actor, 83, 0, 0, 0);
}

void HandleIgnorePacket(CGameMode&, const PacketView&)
{
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

bool ApplySelfStatusUpdateToSession(u32 statusType, u32 value)
{
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

void HandleItemRemove(CGameMode&, const PacketView& packet)
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
                    LaunchLevelUpEffect(mode.m_world->m_player, effectId);
                    RecordLocalLevelUpEffect(effectId);
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
                    LaunchLevelUpEffect(mode.m_world->m_player, effectId);
                    RecordLocalLevelUpEffect(effectId);
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
    }
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

constexpr int kUiChatEventMsg = 101;

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

constexpr u8 kNotifyActDamage = 0;
constexpr u8 kNotifyActEndureDamage = 4;
constexpr u8 kNotifyActMultiHitDamage = 8;
constexpr u8 kNotifyActMultiHitEndure = 9;
constexpr u8 kNotifyActCriticalDamage = 10;
constexpr u8 kNotifyActLuckyDodge = 11;
constexpr int kDefaultAttackMotionTime = 1440;

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

    const char* waveName = nullptr;
    if (targetActor->m_isPc != 0) {
        waveName = g_session.GetJobHitWaveName(targetActor->m_job);
    } else if (sourceActor) {
        int weaponType = -1;
        if (const CPc* pcActor = dynamic_cast<const CPc*>(sourceActor)) {
            weaponType = pcActor->m_weapon;
        } else if (const CGrannyPc* grannyPcActor = dynamic_cast<const CGrannyPc*>(sourceActor)) {
            weaponType = grannyPcActor->m_weapon;
        }
        waveName = g_session.GetWeaponHitWaveName((weaponType >= 0 && weaponType < 31) ? weaponType : -1);
    } else {
        waveName = g_session.GetWeaponHitWaveName(-1);
    }

    if (waveName && *waveName) {
        std::snprintf(hitInfo.waveName, sizeof(hitInfo.waveName), "%s", waveName);
    }

    targetActor->QueueWillBeAttacked(hitInfo);
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

    const auto it = mode.m_runtimeActors.find(actorId);
    if (it != mode.m_runtimeActors.end()) {
        return it->second;
    }

    return EnsureRuntimeActor(mode, actorId, preferPc);
}

void StartAttackAnimation(CGameActor* actor, CGameActor* target, int attackMT)
{
    if (!actor) {
        return;
    }

    const int action = actor->m_isPc ? 80 : 16;
    actor->m_targetGid = target ? target->m_gid : 0;
    StopActorMovementForAction(actor);
    FaceActorTowardTarget(actor, target);
    actor->m_stateStartTick = timeGetTime();
    actor->SetModifyFactorOfmotionSpeed(attackMT);
    actor->SetAction(action, 4, 1);
    actor->m_stateId = kGameActorAttackStateId;
    actor->ProcessMotion();
    actor->m_attackMotion = static_cast<float>(actor->GetAttackMotion());

    if (CPc* pcActor = dynamic_cast<CPc*>(actor)) {
        pcActor->InvalidateBillboard();
    }
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

    if (serverTick != 0) {
        g_session.SetServerTime(serverTick);
    }

    if (attackMT == 0) {
        attackMT = kDefaultAttackMotionTime;
    }
    (void)attackedMT;

    mode.m_aidList[srcGid] = GetTickCount();
    mode.m_aidList[dstGid] = GetTickCount();

    if (!IsAttackNotifyType(actionType)) {
        return;
    }

    CGameActor* sourceActor = ResolveCombatActor(mode, srcGid, true);
    CGameActor* targetActor = ResolveCombatActor(mode, dstGid, true);
    if (!sourceActor) {
        return;
    }

    DbgLog("[GameMode] act notify stage=resolved src=%u dst=%u srcPtr=%p dstPtr=%p srcPc=%d dstPc=%d\n",
        srcGid,
        dstGid,
        static_cast<void*>(sourceActor),
        static_cast<void*>(targetActor),
        sourceActor->m_isPc,
        targetActor ? targetActor->m_isPc : 0);

    StartAttackAnimation(sourceActor, targetActor, attackMT);
    DbgLog("[GameMode] act notify stage=started src=%u action=%d motionSpeed=%.3f atkMotion=%.1f\n",
        srcGid,
        sourceActor->m_curAction,
        sourceActor->m_motionSpeed,
        sourceActor->m_attackMotion);
    QueueCombatHitReaction(sourceActor, targetActor, attackedMT, damage, actionType);
    EmitCombatNumber(sourceActor, targetActor, damage, actionType);
    DbgLog("[GameMode] act notify stage=number src=%u dst=%u dmg=%d\n",
        srcGid,
        dstGid,
        damage);

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

float ResolveActorHeight(const CWorld* world, float worldX, float worldZ)
{
    if (world && world->m_attr) {
        return world->m_attr->GetHeight(worldX, worldZ);
    }
    return 0.0f;
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
    actor->m_speed = 150;
    actor->m_job = 0;
    actor->m_sex = 0;
    actor->m_bodyState = 0;
    actor->m_healthState = 0;
    actor->m_effectState = 0;
    actor->m_pkState = 0;
    actor->m_clevel = 0;
    actor->m_xSize = 0;
    actor->m_ySize = 0;
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

    if (!IsLocalPlayerActor(mode, gid)) {
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
    if (CPc* pc = dynamic_cast<CPc*>(actor)) {
        pc->m_headDir = headDir;
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

    actor->m_bodyState = bodyState;
    actor->m_healthState = healthState;
    actor->m_effectState = effectState;
    actor->m_pkState = pkState;

    if (CPc* pcActor = dynamic_cast<CPc*>(actor)) {
        pcActor->InvalidateBillboard();
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

    if (actor->m_isMoving) {
        actor->ProcessState();
    }

    actor->UnRegisterPos();
    const bool isLocalPlayer = IsLocalPlayerActor(mode, gid);
    actor->m_moveSrcX = srcX >= 0 ? srcX : actor->m_moveDestX;
    actor->m_moveSrcY = srcY >= 0 ? srcY : actor->m_moveDestY;
    actor->m_moveDestX = tileX;
    actor->m_moveDestY = tileY;
    actor->m_lastTlvertX = tileX;
    actor->m_lastTlvertY = tileY;
    actor->m_path.Reset();

    actor->m_moveStartPos = actor->m_pos;
    if (isLocalPlayer && srcX >= 0 && srcY >= 0 && mode.m_world) {
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
    if (!actor->m_isMoving) {
        actor->m_pos = actor->m_moveEndPos;
    } else if (isLocalPlayer && actor->m_path.m_cells.size() >= 2) {
        if (!InterpolateRuntimeActorPathPosition(mode.m_world, *actor, g_session.GetServerTime(), &actor->m_pos)) {
            actor->m_pos = actor->m_moveStartPos;
        }
    } else {
        actor->m_pos = actor->m_moveStartPos;
    }
    actor->RegisterPos();
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

const char* ChannelTag(u8 channel)
{
    switch (channel) {
    case kChatChannelPlayer: return "player";
    case kChatChannelWhisper: return "whisper";
    case kChatChannelParty: return "party";
    case kChatChannelBroadcast: return "broadcast";
    case kChatChannelBattlefield: return "battlefield";
    case kChatChannelSystem: return "system";
    default: return "normal";
    }
}

ChatEntry BuildChatEntry(const std::string& text, u32 color, u8 channel)
{
    ChatEntry entry{};
    entry.color = color & 0x00FFFFFFu;
    entry.channel = channel;

    char prefix[64] = {};
    std::snprintf(prefix, sizeof(prefix), "[%s #%06X] ", ChannelTag(channel), entry.color);
    entry.text = prefix;
    entry.text += text;
    return entry;
}

void PropagateChatToUi(const ChatEntry& entry)
{
    if (entry.text.empty()) {
        return;
    }

    const int meta = static_cast<int>((entry.color & 0x00FFFFFFu) | (static_cast<u32>(entry.channel) << 24));
    g_windowMgr.SendMsg(kUiChatEventMsg, reinterpret_cast<int>(entry.text.c_str()), meta);
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
    RecordChat(mode, ExtractPacketString(packet, 8), 0x00FFFFFF, kChatChannelNormal);
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

void HandleMapChange(CGameMode&, const PacketView& packet)
{
    MapChangeInfo info;
    if (!ParseMapChangePacket(packet, info)) {
        return;
    }

    ApplyMapChangeSessionState(info);
    g_modeMgr.PresentLoadingScreen("Loading new map...", 0.01f);

    DbgLog("[GameMode] map change packet=0x%04X map='%s' pos=%d,%d serverMove=%d\n",
        packet.packetId,
        g_session.m_curMap,
        info.x,
        info.y,
        info.hasServerMove ? 1 : 0);

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
    router.Register(0x009C, HandleActorDirection);
    router.Register(0x0119, HandleActorStateChange);
    router.Register(0x011A, HandleIgnorePacket);
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
    router.Register(0x01EE, HandleNormalInventoryList);
    router.Register(0x01EF, HandleEquipInventoryList);
    router.Register(0x02D0, HandleEquipInventoryList);
    router.Register(0x02E8, HandleNormalInventoryList);
    router.Register(0x07FA, HandleItemRemove);

    // Quit/return-to-login transitions.
    router.Register(0x0081, HandleBanDisconnect);
    router.Register(0x00B3, HandleDisconnectCharacterAck);

    // Common server-side status/config packets that are safe to ignore for now,
    // but must stay in sync so later actor packets are framed correctly.
    router.Register(0x00B0, HandleSelfStatusParam);
    router.Register(0x00B1, HandleSelfStatusParam);
    router.Register(0x00BD, HandleIgnorePacket);
    router.Register(0x00C0, HandleIgnorePacket);
    router.Register(0x013E, HandleIgnorePacket);
    router.Register(0x01B0, HandleIgnorePacket);
    router.Register(0x0106, HandleIgnorePacket);
    router.Register(0x0104, HandleIgnorePacket);
    router.Register(0x010F, HandleIgnorePacket);
    router.Register(0x011F, HandleIgnorePacket);
    router.Register(0x0120, HandleIgnorePacket);
    router.Register(0x0131, HandleIgnorePacket);
    router.Register(0x0132, HandleIgnorePacket);
    router.Register(0x0141, HandleSelfStatInfo);
    router.Register(0x0148, HandleActorResurrection);
    router.Register(0x019B, HandleNotifyEffect);
    router.Register(0x01B9, HandleSkillCastCancel);
    router.Register(0x0192, HandleIgnorePacket);
    router.Register(0x01C9, HandleIgnorePacket);
    router.Register(0x01CF, HandleIgnorePacket);
    router.Register(0x01D0, HandleIgnorePacket);
    router.Register(0x01D7, HandleActorSpriteChange);
    router.Register(0x01DE, HandleIgnorePacket);
    router.Register(0x01E1, HandleIgnorePacket);
    router.Register(0x01F3, HandleIgnorePacket);
    router.Register(0x0201, HandleIgnorePacket);
    router.Register(0x0209, HandleIgnorePacket);
    router.Register(0x02DD, HandleIgnorePacket);
    router.Register(0x0283, HandleIgnorePacket);
    router.Register(0x02C9, HandleIgnorePacket);
    router.Register(0x02B9, HandleIgnorePacket);
    router.Register(0x02D1, HandleIgnorePacket);
    router.Register(0x02D2, HandleIgnorePacket);
    router.Register(0x02D3, HandleIgnorePacket);
    router.Register(0x02D5, HandleIgnorePacket);
    router.Register(0x02D7, HandleIgnorePacket);
    router.Register(0x02DA, HandleIgnorePacket);
    router.Register(0x02E1, HandleActorActionNotify);
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
