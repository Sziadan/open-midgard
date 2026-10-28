#include "MapSendProfile.h"

#include "Packet.h"
#include "core/SettingsIni.h"
#include "DebugLog.h"
#include "gamemode/PacketPadding.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <mutex>
#include <string>

namespace ro::net {
namespace {

bool MatchesPacketId(u16 packetId, std::initializer_list<u16> ids)
{
    for (const u16 id : ids) {
        if (id != 0 && packetId == id) {
            return true;
        }
    }
    return false;
}

AccountLoginPacketProfile MakeLoginProfile(PacketVersionId id, const char* name)
{
    return {
        id,
        name,
        PacketProfile::PacketVer23LoginChain::kClientDate,
        PacketProfile::PacketVer23LoginChain::kAccountLogin,
        PacketProfile::PacketVer23LoginChain::kAccountLoginChannel,
        PacketProfile::PacketVer23LoginChain::kRequestPasswordHash,
        PacketProfile::PacketVer23LoginChain::kPasswordHashChallenge,
        PacketProfile::PacketVer23LoginChain::kPasswordHashLogin,
        PACKETID_CA_CONNECT_INFO_CHANGED,
        PacketProfile::PacketVer23LoginChain::kExeHashCheck,
        PacketProfile::PacketVer23LoginChain::kNotifyError,
    };
}

CharacterPacketProfile MakeCharacterProfile(PacketVersionId id, const char* name)
{
    return {
        id,
        name,
        PacketProfile::PacketVer23LoginChain::kCharServerEnter,
        PacketProfile::PacketVer23LoginChain::kSelectCharacter,
        PacketProfile::PacketVer23LoginChain::kMakeCharacter,
        PacketProfile::PacketVer23LoginChain::kDeleteCharacter,
    };
}

ZonePacketProfile MakePacketVer23ZoneProfile()
{
    return {
        PacketVersionId::PacketVer23,
        "packetver23",
        PacketProfile::PacketVer23MapServerSend::kWantToConnection,
        ZoneConnectPacketLayout::Compact19,
    };
}

ZonePacketProfile MakePacketVer22ZoneProfile()
{
    return {
        PacketVersionId::PacketVer22,
        "packetver22",
        PacketProfile::PacketVer22MapServerSend::kWantToConnection,
        ZoneConnectPacketLayout::Legacy26,
    };
}

ZonePacketProfile MakeLegacy72ZoneProfile()
{
    return {
        PacketVersionId::PacketVer22,
        "legacy0072",
        PacketProfile::LegacyMapServerSend::kWantToConnection,
        ZoneConnectPacketLayout::Compact19,
    };
}

ZonePacketProfile MakeLegacy72Padded22ZoneProfile()
{
    return {
        PacketVersionId::PacketVer22,
        "legacy0072padded22",
        PacketProfile::LegacyMapServerSend::kWantToConnection,
        ZoneConnectPacketLayout::Padded22,
    };
}

MapGameplaySendProfile MakePacketVer23Profile()
{
    return {
        MapGameplaySendProfileId::PacketVer23,
        "packetver23",
        PacketProfile::PacketVer23MapServerSend::kActionRequest,
        PacketProfile::PacketVer23MapServerSend::kUseSkillToId,
        PacketProfile::PacketVer23MapServerSend::kCartOff,
        PacketProfile::PacketVer23MapServerSend::kChangeCart,
        PacketProfile::PacketVer23MapServerSend::kUseSkillToPos,
        PacketProfile::PacketVer23MapServerSend::kUseSkillToPosInfo,
        PacketProfile::PacketVer23MapServerSend::kUseSkillMap,
        PacketProfile::PacketVer23MapServerSend::kUseItem,
        PacketProfile::PacketVer23MapServerSend::kTakeItem,
        PacketProfile::PacketVer23MapServerSend::kDropItem,
        PacketProfile::PacketVer23MapServerSend::kItemCompositionList,
        PacketProfile::PacketVer23MapServerSend::kItemComposition,
        PacketProfile::PacketVer23MapServerSend::kItemIdentify,
        PacketProfile::PacketVer23MapServerSend::kSkillUp,
        PacketProfile::PacketVer23MapServerSend::kEquipItem,
        PacketProfile::PacketVer23MapServerSend::kUnequipItem,
        PacketProfile::PacketVer23MapServerSend::kWalkToXY,
        PacketProfile::PacketVer23MapServerSend::kChangeDir,
        PacketProfile::PacketVer23MapServerSend::kTickSend,
        PacketProfile::PacketVer23MapServerSend::kNotifyActorInit,
        PacketProfile::PacketVer23MapServerSend::kGetCharNameRequest,
        PacketProfile::PacketVer23MapServerSend::kWhisper,
        PacketProfile::PacketVer23MapServerSend::kGlobalMessage,
    };
}

MapGameplaySendProfile MakePacketVer22Profile()
{
    return {
        MapGameplaySendProfileId::PacketVer22,
        "packetver22",
        PacketProfile::PacketVer22MapServerSend::kActionRequest,
        PacketProfile::PacketVer22MapServerSend::kUseSkillToId,
        PacketProfile::PacketVer22MapServerSend::kCartOff,
        PacketProfile::PacketVer22MapServerSend::kChangeCart,
        PacketProfile::PacketVer22MapServerSend::kUseSkillToPos,
        0,
        PacketProfile::PacketVer22MapServerSend::kUseSkillMap,
        PacketProfile::PacketVer22MapServerSend::kUseItem,
        PacketProfile::PacketVer22MapServerSend::kTakeItem,
        PacketProfile::PacketVer22MapServerSend::kDropItem,
        PacketProfile::PacketVer22MapServerSend::kItemCompositionList,
        PacketProfile::PacketVer22MapServerSend::kItemComposition,
        PacketProfile::PacketVer22MapServerSend::kItemIdentify,
        PacketProfile::PacketVer22MapServerSend::kSkillUp,
        PacketProfile::PacketVer22MapServerSend::kEquipItem,
        PacketProfile::PacketVer22MapServerSend::kUnequipItem,
        PacketProfile::PacketVer22MapServerSend::kWalkToXY,
        PacketProfile::PacketVer22MapServerSend::kChangeDir,
        PacketProfile::PacketVer22MapServerSend::kTickSend,
        PacketProfile::PacketVer22MapServerSend::kNotifyActorInit,
        PacketProfile::PacketVer22MapServerSend::kGetCharNameRequest,
        PacketProfile::PacketVer22MapServerSend::kWhisper,
        PacketProfile::PacketVer22MapServerSend::kGlobalMessage,
    };
}

MapGameplaySendProfile MakeLegacyGameplayProfile()
{
    return {
        MapGameplaySendProfileId::Legacy,
        "legacy0072",
        0x0089,
        0x0113,
        PacketProfile::PacketVer23MapServerSend::kCartOff,
        PacketProfile::PacketVer23MapServerSend::kChangeCart,
        0x0116,
        0,
        0x011B,
        0x00A7,
        0x009F,
        0x00A2,
        PacketProfile::PacketVer22MapServerSend::kItemCompositionList,
        PacketProfile::PacketVer22MapServerSend::kItemComposition,
        PacketProfile::PacketVer22MapServerSend::kItemIdentify,
        PacketProfile::LegacyMapServerSend::kSkillUp,
        0x00A9,
        0x00AB,
        PacketProfile::LegacyMapServerSend::kWalkToXY,
        0x009B,
        PacketProfile::LegacyMapServerSend::kTickSend,
        PacketProfile::LegacyMapServerSend::kNotifyActorInit,
        PacketProfile::LegacyMapServerSend::kGetCharNameRequest,
        PacketProfile::LegacyMapServerSend::kWhisper,
        PacketProfile::LegacyMapServerSend::kGlobalMessage,
    };
}

MapReceiveProfile MakePacketVer23ReceiveProfile()
{
    MapReceiveProfile profile{};
    profile.id = PacketVersionId::PacketVer23;
    profile.name = "packetver23";
    profile.deferProactiveNameRequests = false;
    profile.usesLegacyActorStream = true;
    profile.acceptEnterLegacy = PACKETID_ZC_ACCEPT_ENTER;
    profile.acceptEnterModern = 0x02EB;
    profile.notifyTime = 0x007F;
    profile.mapChangeBasic = 0x0091;
    profile.mapChangeServerMove = 0x0092;
    profile.actorActionNotifyBasic = 0x008A;
    profile.actorActionNotifyExtended = 0x02E1;
    profile.actorSetPositionBasic = 0x0088;
    profile.actorSetPositionHighJump = 0x01FF;
    profile.broadcastBasic = 0x009A;
    profile.broadcastColored = 0x01C3;
    profile.groundItemEntryExisting = 0x009D;
    profile.groundItemEntryDropped = 0x009E;
    profile.itemPickupAckBasic = 0x00A0;
    profile.itemPickupAckExtended = 0x02D4;
    profile.normalInventoryListBasic = 0x00A3;
    profile.normalInventoryListCardSlots = 0x01EE;
    profile.normalInventoryListTimed = 0x02E8;
    profile.equipInventoryListBasic = 0x00A4;
    profile.equipInventoryListTimed = 0x01EF;
    profile.equipInventoryListTimedOwned = 0x02D0;
    profile.normalStorageListBasic = 0x00A5;
    profile.normalStorageListCardSlots = 0x01F0;
    profile.normalStorageListTimed = 0x02EA;
    profile.equipStorageListBasic = 0x00A6;
    profile.equipStorageListTimedOwned = 0x02D1;
    profile.storageItemAddedBasic = 0x00F4;
    profile.storageItemAddedTyped = 0x01C4;
    profile.useItemAckBasic = 0x00A8;
    profile.useItemAckExtended = 0x01C8;
    profile.itemRemoveBasic = 0x00AF;
    profile.itemRemoveExtended = 0x07FA;
    profile.partyInviteAckBasic = 0x00FD;
    profile.partyInviteAckExtended = 0x02C5;
    profile.partyInviteRequestBasic = 0x00FE;
    profile.partyInviteRequestExtended = 0x02C6;
    profile.skillDamagePositionNotify = 0x0115;
    profile.groundSkillNotify = 0x0117;
    profile.skillNoDamageNotify = 0x011A;
    profile.skillUnitSetBasic = 0x011F;
    profile.skillUnitSetExtended = 0x01C9;
    profile.notifyEffectBasic = 0x019B;
    profile.notifyEffectDirect = 0x01F3;
    profile.actorSpawnLegacyIdle = 0x0078;
    profile.actorSpawnLegacySpawn = 0x0079;
    profile.actorSpawnLegacyAlt = 0x007A;
    profile.actorSpawnLegacyNpc = 0x007C;
    profile.actorSpawnLegacyIdleShifted = 0x01D8;
    profile.actorSpawnLegacySpawnShifted = 0x01D9;
    profile.actorMoveLegacy = 0x007B;
    profile.actorMoveLegacyShifted = 0x01DA;
    profile.actorSpawnVariableIdle = 0x07F9;
    profile.actorSpawnVariableSpawn = 0x07F8;
    profile.actorSpawnVariableIdleRobe = 0x0857;
    profile.actorSpawnVariableSpawnRobe = 0x0858;
    profile.actorMoveVariable = 0x07F7;
    profile.actorMoveVariableRobe = 0x0856;
    profile.actorSpawnModernIdle = 0x022A;
    profile.actorSpawnModernSpawn = 0x022B;
    profile.actorSpawnModernIdleFont = 0x02EE;
    profile.actorSpawnModernSpawnFont = 0x02ED;
    profile.actorMoveModern = 0x022C;
    profile.actorMoveModernFont = 0x02EC;
    profile.actorNameAckBasic = 0x0095;
    profile.actorNameAckParty = 0x0194;
    profile.actorNameAckFull = 0x0195;
    profile.actorStateChangeBasic = 0x0119;
    profile.actorStateChangeExtended = 0x0229;
    profile.partyMemberAddedBasic = 0x0104;
    profile.partyMemberAddedExtended = 0x01E9;
    profile.partyHpUpdateBasic = 0x0106;
    profile.partyHpUpdateExtended = 0x080E;
    profile.skillDamageNotifyBasic = 0x0114;
    profile.skillDamageNotifyExtended = 0x01DE;
    return profile;
}

MapReceiveProfile MakePacketVer22ReceiveProfile()
{
    MapReceiveProfile profile = MakePacketVer23ReceiveProfile();
    profile.id = PacketVersionId::PacketVer22;
    profile.name = "packetver22";
    profile.deferProactiveNameRequests = true;
    return profile;
}

PacketProfileSet MakePacketVer23PacketProfileSet()
{
    return {
        PacketVersionId::PacketVer23,
        "packetver23",
        MakeLoginProfile(PacketVersionId::PacketVer23, "packetver23"),
        MakeCharacterProfile(PacketVersionId::PacketVer23, "packetver23"),
        MakePacketVer23ZoneProfile(),
        MakePacketVer23Profile(),
        MakePacketVer23ReceiveProfile(),
    };
}

PacketProfileSet MakePacketVer22PacketProfileSet()
{
    return {
        PacketVersionId::PacketVer22,
        "packetver22",
        MakeLoginProfile(PacketVersionId::PacketVer22, "packetver22"),
        MakeCharacterProfile(PacketVersionId::PacketVer22, "packetver22"),
        MakePacketVer22ZoneProfile(),
        MakePacketVer22Profile(),
        MakePacketVer22ReceiveProfile(),
    };
}

std::string NormalizeProfileToken(std::string value)
{
    value.erase(std::remove_if(value.begin(), value.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0 || ch == '_' || ch == '-';
    }), value.end());
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

const PacketProfileSet& ResolveConfiguredProfile()
{
    static const PacketProfileSet kPacketVer23 = MakePacketVer23PacketProfileSet();
    static const PacketProfileSet kPacketVer22 = MakePacketVer22PacketProfileSet();

    const std::string configured = NormalizeProfileToken(
        LoadSettingsIniString("Packets", "MapSendProfile", kPacketVer23.name));
    if (configured == "packetver22" || configured == "22" || configured == "legacy" || configured == "pre20080910") {
        return kPacketVer22;
    }
    return kPacketVer23;
}

const ZonePacketProfile& ResolveConfiguredZoneProfile(const PacketProfileSet& baseProfile)
{
    static const ZonePacketProfile kPacketVer23Zone = MakePacketVer23ZoneProfile();
    static const ZonePacketProfile kPacketVer22Zone = MakePacketVer22ZoneProfile();
    static const ZonePacketProfile kLegacy72Zone = MakeLegacy72ZoneProfile();
    static const ZonePacketProfile kLegacy72Padded22Zone = MakeLegacy72Padded22ZoneProfile();

    const std::string configured = NormalizeProfileToken(
        LoadSettingsIniString("Packets", "ZoneConnectProfile", "inherit"));
    if (configured.empty() || configured == "inherit" || configured == "active" || configured == "mapsendprofile") {
        return baseProfile.zone;
    }

    if (configured == "legacy0072padded22" || configured == "0072padded22" || configured == "packetver6"
        || configured == "22byte0072" || configured == "padded22") {
        return kLegacy72Padded22Zone;
    }

    if (configured == "legacy0072" || configured == "0072" || configured == "classic" || configured == "legacy19") {
        return kLegacy72Zone;
    }

    if (configured == "packetver22" || configured == "22" || configured == "009b" || configured == "legacy26") {
        return kPacketVer22Zone;
    }

    if (configured == "packetver23" || configured == "23" || configured == "0436" || configured == "compact19") {
        return kPacketVer23Zone;
    }

    return baseProfile.zone;
}

const MapGameplaySendProfile& ResolveConfiguredGameplaySendProfile(const PacketProfileSet& baseProfile, const ZonePacketProfile& zoneProfile)
{
    static const MapGameplaySendProfile kPacketVer23Profile = MakePacketVer23Profile();
    static const MapGameplaySendProfile kPacketVer22Profile = MakePacketVer22Profile();
    static const MapGameplaySendProfile kLegacyProfile = MakeLegacyGameplayProfile();

    const std::string configured = NormalizeProfileToken(
        LoadSettingsIniString("Packets", "MapGameplaySendProfile", "inherit"));
    if (configured == "legacy0072" || configured == "legacy005" || configured == "packetver5"
        || configured == "classic" || configured == "legacyworld") {
        return kLegacyProfile;
    }

    if (configured == "packetver22" || configured == "22") {
        return kPacketVer22Profile;
    }

    if (configured == "packetver23" || configured == "23") {
        return kPacketVer23Profile;
    }

    if (configured.empty() || configured == "inherit" || configured == "active" || configured == "mapsendprofile") {
        if (zoneProfile.wantToConnection == PacketProfile::LegacyMapServerSend::kWantToConnection
            && zoneProfile.wantToConnectionLayout == ZoneConnectPacketLayout::Compact19) {
            return kLegacyProfile;
        }
        return baseProfile.mapSend;
    }

    return baseProfile.mapSend;
}

} // namespace

const PacketProfileSet& GetActivePacketProfile()
{
    static std::once_flag once;
    static const PacketProfileSet* profile = nullptr;

    std::call_once(once, []() {
        profile = &ResolveConfiguredProfile();
        DbgLog("[PacketProfile] active packet profile=%s\n", profile->name);
    });

    return *profile;
}

const AccountLoginPacketProfile& GetActiveAccountLoginPacketProfile()
{
    return GetActivePacketProfile().login;
}

const CharacterPacketProfile& GetActiveCharacterPacketProfile()
{
    return GetActivePacketProfile().character;
}

const ZonePacketProfile& GetActiveZonePacketProfile()
{
    static std::once_flag once;
    static const ZonePacketProfile* profile = nullptr;

    std::call_once(once, []() {
        profile = &ResolveConfiguredZoneProfile(GetActivePacketProfile());
        DbgLog("[PacketProfile] active zone connect profile=%s opcode=0x%04X layout=%u\n",
            profile->name,
            static_cast<unsigned int>(profile->wantToConnection),
            static_cast<unsigned int>(profile->wantToConnectionLayout));
    });

    return *profile;
}

const MapReceiveProfile& GetActiveMapReceiveProfile()
{
    return GetActivePacketProfile().mapReceive;
}

const MapGameplaySendProfile& GetActiveMapGameplaySendProfile()
{
    static std::once_flag once;
    static const MapGameplaySendProfile* profile = nullptr;

    std::call_once(once, []() {
        const PacketProfileSet& baseProfile = GetActivePacketProfile();
        const ZonePacketProfile& zoneProfile = GetActiveZonePacketProfile();
        profile = &ResolveConfiguredGameplaySendProfile(baseProfile, zoneProfile);
        DbgLog("[PacketProfile] active map gameplay send profile=%s\n", profile->name);
    });

    return *profile;
}

bool IsPacketVer22MapGameplaySendProfile()
{
    return GetActiveMapGameplaySendProfile().id == MapGameplaySendProfileId::PacketVer22;
}

bool IsLegacyMapGameplaySendProfile()
{
    return GetActiveMapGameplaySendProfile().id == MapGameplaySendProfileId::Legacy;
}

bool IsPacketVer22PacketProfile()
{
    return GetActivePacketProfile().id == PacketVersionId::PacketVer22;
}

bool IsActiveAcceptEnterPacket(u16 packetId)
{
    const MapReceiveProfile& profile = GetActiveMapReceiveProfile();
    return MatchesPacketId(packetId, {profile.acceptEnterLegacy, profile.acceptEnterModern});
}

bool IsActiveMapChangePacket(u16 packetId)
{
    const MapReceiveProfile& profile = GetActiveMapReceiveProfile();
    return MatchesPacketId(packetId, {profile.mapChangeBasic, profile.mapChangeServerMove});
}

bool IsActiveServerMoveMapChangePacket(u16 packetId)
{
    return packetId == GetActiveMapReceiveProfile().mapChangeServerMove;
}

bool IsActiveActorActionNotifyExtendedPacket(u16 packetId)
{
    return packetId == GetActiveMapReceiveProfile().actorActionNotifyExtended;
}

bool IsActiveActorSetPositionPacket(u16 packetId)
{
    const MapReceiveProfile& profile = GetActiveMapReceiveProfile();
    return MatchesPacketId(packetId, {profile.actorSetPositionBasic, profile.actorSetPositionHighJump});
}

bool IsActiveBroadcastPacket(u16 packetId)
{
    const MapReceiveProfile& profile = GetActiveMapReceiveProfile();
    return MatchesPacketId(packetId, {profile.broadcastBasic, profile.broadcastColored});
}

bool IsActiveColoredBroadcastPacket(u16 packetId)
{
    return packetId == GetActiveMapReceiveProfile().broadcastColored;
}

bool IsActiveGroundItemEntryPacket(u16 packetId)
{
    const MapReceiveProfile& profile = GetActiveMapReceiveProfile();
    return MatchesPacketId(packetId, {profile.groundItemEntryExisting, profile.groundItemEntryDropped});
}

bool IsActiveGroundItemDroppedPacket(u16 packetId)
{
    return packetId == GetActiveMapReceiveProfile().groundItemEntryDropped;
}

bool IsActiveItemPickupAckPacket(u16 packetId)
{
    const MapReceiveProfile& profile = GetActiveMapReceiveProfile();
    return MatchesPacketId(packetId, {profile.itemPickupAckBasic, profile.itemPickupAckExtended});
}

bool IsActiveNormalInventoryListPacket(u16 packetId)
{
    const MapReceiveProfile& profile = GetActiveMapReceiveProfile();
    return MatchesPacketId(packetId,
        {profile.normalInventoryListBasic, profile.normalInventoryListCardSlots, profile.normalInventoryListTimed});
}

bool IsActiveEquipInventoryListPacket(u16 packetId)
{
    const MapReceiveProfile& profile = GetActiveMapReceiveProfile();
    return MatchesPacketId(packetId,
        {profile.equipInventoryListBasic, profile.equipInventoryListTimed, profile.equipInventoryListTimedOwned});
}

bool IsActiveNormalStorageListPacket(u16 packetId)
{
    const MapReceiveProfile& profile = GetActiveMapReceiveProfile();
    return MatchesPacketId(packetId,
        {profile.normalStorageListBasic, profile.normalStorageListCardSlots, profile.normalStorageListTimed});
}

bool IsActiveEquipStorageListPacket(u16 packetId)
{
    const MapReceiveProfile& profile = GetActiveMapReceiveProfile();
    return MatchesPacketId(packetId, {profile.equipStorageListBasic, profile.equipStorageListTimedOwned});
}

bool IsActiveStorageItemAddedPacket(u16 packetId)
{
    const MapReceiveProfile& profile = GetActiveMapReceiveProfile();
    return MatchesPacketId(packetId, {profile.storageItemAddedBasic, profile.storageItemAddedTyped});
}

bool IsActiveStorageItemAddedTypedPacket(u16 packetId)
{
    return packetId == GetActiveMapReceiveProfile().storageItemAddedTyped;
}

bool IsActiveUseItemAckPacket(u16 packetId)
{
    const MapReceiveProfile& profile = GetActiveMapReceiveProfile();
    return MatchesPacketId(packetId, {profile.useItemAckBasic, profile.useItemAckExtended});
}

bool IsActiveExtendedUseItemAckPacket(u16 packetId)
{
    return packetId == GetActiveMapReceiveProfile().useItemAckExtended;
}

bool IsActiveItemRemovePacket(u16 packetId)
{
    const MapReceiveProfile& profile = GetActiveMapReceiveProfile();
    return MatchesPacketId(packetId, {profile.itemRemoveBasic, profile.itemRemoveExtended});
}

bool IsActiveExtendedItemRemovePacket(u16 packetId)
{
    return packetId == GetActiveMapReceiveProfile().itemRemoveExtended;
}

bool IsActivePartyInviteAckPacket(u16 packetId)
{
    const MapReceiveProfile& profile = GetActiveMapReceiveProfile();
    return MatchesPacketId(packetId, {profile.partyInviteAckBasic, profile.partyInviteAckExtended});
}

bool IsActiveExtendedPartyInviteAckPacket(u16 packetId)
{
    return packetId == GetActiveMapReceiveProfile().partyInviteAckExtended;
}

bool IsActivePartyInviteRequestPacket(u16 packetId)
{
    const MapReceiveProfile& profile = GetActiveMapReceiveProfile();
    return MatchesPacketId(packetId, {profile.partyInviteRequestBasic, profile.partyInviteRequestExtended});
}

bool IsActiveSkillUnitSetPacket(u16 packetId)
{
    const MapReceiveProfile& profile = GetActiveMapReceiveProfile();
    return MatchesPacketId(packetId, {profile.skillUnitSetBasic, profile.skillUnitSetExtended});
}

int GetActiveStackableItemListEntrySize(u16 packetId)
{
    const MapReceiveProfile& profile = GetActiveMapReceiveProfile();
    if (MatchesPacketId(packetId, {profile.normalInventoryListBasic, profile.normalStorageListBasic})) {
        return 10;
    }
    if (MatchesPacketId(packetId, {profile.normalInventoryListCardSlots, profile.normalStorageListCardSlots})) {
        return 18;
    }
    if (MatchesPacketId(packetId, {profile.normalInventoryListTimed, profile.normalStorageListTimed})) {
        return 22;
    }
    return 0;
}

int GetActiveEquipmentItemListEntrySize(u16 packetId)
{
    const MapReceiveProfile& profile = GetActiveMapReceiveProfile();
    if (MatchesPacketId(packetId, {profile.equipInventoryListBasic, profile.equipStorageListBasic})) {
        return 20;
    }
    if (packetId == profile.equipInventoryListTimed) {
        return 24;
    }
    if (MatchesPacketId(packetId, {profile.equipInventoryListTimedOwned, profile.equipStorageListTimedOwned})) {
        return 26;
    }
    return 0;
}

bool IsActiveActorSpawnPacket(u16 packetId)
{
    const MapReceiveProfile& profile = GetActiveMapReceiveProfile();
    return MatchesPacketId(packetId,
        {
            profile.actorSpawnLegacyIdle,
            profile.actorSpawnLegacySpawn,
            profile.actorSpawnLegacyAlt,
            profile.actorSpawnLegacyNpc,
            profile.actorSpawnLegacyIdleShifted,
            profile.actorSpawnLegacySpawnShifted,
            profile.actorSpawnVariableIdle,
            profile.actorSpawnVariableSpawn,
            profile.actorSpawnVariableIdleRobe,
            profile.actorSpawnVariableSpawnRobe,
            profile.actorSpawnModernIdle,
            profile.actorSpawnModernSpawn,
            profile.actorSpawnModernIdleFont,
            profile.actorSpawnModernSpawnFont,
        });
}

bool IsActiveLegacyActorSpawnPacket(u16 packetId)
{
    const MapReceiveProfile& profile = GetActiveMapReceiveProfile();
    return MatchesPacketId(packetId,
        {
            profile.actorSpawnLegacyIdle,
            profile.actorSpawnLegacySpawn,
            profile.actorSpawnLegacyAlt,
            profile.actorSpawnLegacyNpc,
            profile.actorSpawnLegacyIdleShifted,
            profile.actorSpawnLegacySpawnShifted,
        });
}

bool IsActiveActorMovePacket(u16 packetId)
{
    const MapReceiveProfile& profile = GetActiveMapReceiveProfile();
    return MatchesPacketId(packetId,
        {
            profile.actorMoveLegacy,
            profile.actorMoveLegacyShifted,
            profile.actorMoveVariable,
            profile.actorMoveVariableRobe,
            profile.actorMoveModern,
            profile.actorMoveModernFont,
        });
}

bool IsActiveLegacyActorMovePacket(u16 packetId)
{
    const MapReceiveProfile& profile = GetActiveMapReceiveProfile();
    return MatchesPacketId(packetId, {profile.actorMoveLegacy, profile.actorMoveLegacyShifted});
}

bool IsActiveActorNameAckPacket(u16 packetId)
{
    const MapReceiveProfile& profile = GetActiveMapReceiveProfile();
    return MatchesPacketId(packetId, {profile.actorNameAckBasic, profile.actorNameAckParty, profile.actorNameAckFull});
}

bool IsActiveActorNameAckFullPacket(u16 packetId)
{
    return packetId == GetActiveMapReceiveProfile().actorNameAckFull;
}

bool IsActiveActorStateChangeExtendedPacket(u16 packetId)
{
    return packetId == GetActiveMapReceiveProfile().actorStateChangeExtended;
}

bool IsActivePartyMemberAddedPacket(u16 packetId)
{
    const MapReceiveProfile& profile = GetActiveMapReceiveProfile();
    return MatchesPacketId(packetId, {profile.partyMemberAddedBasic, profile.partyMemberAddedExtended});
}

bool IsActivePartyMemberAddedExtendedPacket(u16 packetId)
{
    return packetId == GetActiveMapReceiveProfile().partyMemberAddedExtended;
}

bool IsActivePartyHpUpdateExtendedPacket(u16 packetId)
{
    return packetId == GetActiveMapReceiveProfile().partyHpUpdateExtended;
}

bool IsActiveSkillDamageNotifyExtendedPacket(u16 packetId)
{
    return packetId == GetActiveMapReceiveProfile().skillDamageNotifyExtended;
}

bool BuildActiveWantToConnectionPacket(u32 aid,
    u32 gid,
    u32 authCode,
    u32 clientTick,
    u8 sex,
    void* outBuffer,
    int outBufferSize,
    int* outPacketLength)
{
    if (!outBuffer || !outPacketLength || outBufferSize <= 0) {
        return false;
    }

    const ZonePacketProfile& profile = GetActiveZonePacketProfile();
    if (profile.wantToConnectionLayout == ZoneConnectPacketLayout::Legacy26) {
        if (outBufferSize < static_cast<int>(sizeof(PACKET_CZ_ENTER_PACKETVER22))) {
            return false;
        }

        PACKET_CZ_ENTER_PACKETVER22 packet{};
        packet.PacketType = profile.wantToConnection;
        FillPacketPadding(packet.padding0, static_cast<int>(sizeof(packet.padding0)));
        packet.AID = aid;
        FillPacketPadding(&packet.padding1, 1);
        packet.GID = gid;
        FillPacketPadding(packet.padding2, static_cast<int>(sizeof(packet.padding2)));
        packet.AuthCode = authCode;
        packet.ClientTick = clientTick;
        packet.Sex = sex;
        std::memcpy(outBuffer, &packet, sizeof(packet));
        *outPacketLength = static_cast<int>(sizeof(packet));
        return true;
    }

    if (profile.wantToConnectionLayout == ZoneConnectPacketLayout::Padded22) {
        if (outBufferSize < static_cast<int>(sizeof(PACKET_CZ_ENTER_PADDED22))) {
            return false;
        }

        PACKET_CZ_ENTER_PADDED22 packet{};
        packet.PacketType = profile.wantToConnection;
        FillPacketPadding(packet.padding, static_cast<int>(sizeof(packet.padding)));
        packet.AID = aid;
        packet.GID = gid;
        packet.AuthCode = authCode;
        packet.ClientTick = clientTick;
        packet.Sex = sex;
        std::memcpy(outBuffer, &packet, sizeof(packet));
        *outPacketLength = static_cast<int>(sizeof(packet));
        return true;
    }

    if (outBufferSize < static_cast<int>(sizeof(PACKET_CZ_ENTER2))) {
        return false;
    }

    PACKET_CZ_ENTER2 packet{};
    packet.PacketType = profile.wantToConnection;
    packet.AID = aid;
    packet.GID = gid;
    packet.AuthCode = authCode;
    packet.ClientTick = clientTick;
    packet.Sex = sex;
    std::memcpy(outBuffer, &packet, sizeof(packet));
    *outPacketLength = static_cast<int>(sizeof(packet));
    return true;
}

bool BuildActiveActionRequestPacket(u32 targetGid,
    u8 action,
    void* outBuffer,
    int outBufferSize,
    int* outPacketLength)
{
    if (!outBuffer || !outPacketLength || outBufferSize <= 0) {
        return false;
    }

    const MapGameplaySendProfile& profile = GetActiveMapGameplaySendProfile();
    if (profile.id == MapGameplaySendProfileId::PacketVer22) {
        if (outBufferSize < static_cast<int>(sizeof(PACKET_CZ_ACTION_REQUEST_PACKETVER22))) {
            return false;
        }

        PACKET_CZ_ACTION_REQUEST_PACKETVER22 packet{};
        packet.PacketType = profile.actionRequest;
        FillPacketPadding(packet.padding0, static_cast<int>(sizeof(packet.padding0)));
        packet.TargetGID = targetGid;
        FillPacketPadding(packet.padding1, static_cast<int>(sizeof(packet.padding1)));
        packet.Action = action;
        std::memcpy(outBuffer, &packet, sizeof(packet));
        *outPacketLength = static_cast<int>(sizeof(packet));
        return true;
    }

    if (outBufferSize < static_cast<int>(sizeof(PACKET_CZ_ACTION_REQUEST2))) {
        return false;
    }

    PACKET_CZ_ACTION_REQUEST2 packet{};
    packet.PacketType = profile.actionRequest;
    packet.TargetGID = targetGid;
    packet.Action = action;
    std::memcpy(outBuffer, &packet, sizeof(packet));
    *outPacketLength = static_cast<int>(sizeof(packet));
    return true;
}

bool BuildActiveUseSkillToIdPacket(u16 skillId,
    u16 skillLevel,
    u32 targetGid,
    void* outBuffer,
    int outBufferSize,
    int* outPacketLength)
{
    if (!outBuffer || !outPacketLength || outBufferSize <= 0) {
        return false;
    }

    const MapGameplaySendProfile& profile = GetActiveMapGameplaySendProfile();
    if (profile.id == MapGameplaySendProfileId::PacketVer22) {
        if (outBufferSize < static_cast<int>(sizeof(PACKET_CZ_USESKILLTOID_PACKETVER22))) {
            return false;
        }

        PACKET_CZ_USESKILLTOID_PACKETVER22 packet{};
        packet.PacketType = profile.useSkillToId;
        FillPacketPadding(packet.padding0, static_cast<int>(sizeof(packet.padding0)));
        packet.SkillId = skillId;
        packet.SkillLevel = skillLevel;
        FillPacketPadding(packet.padding1, static_cast<int>(sizeof(packet.padding1)));
        FillPacketPadding(packet.padding2, static_cast<int>(sizeof(packet.padding2)));
        packet.TargetGID = targetGid;
        FillPacketPadding(&packet.padding3, 1);
        std::memcpy(outBuffer, &packet, sizeof(packet));
        *outPacketLength = static_cast<int>(sizeof(packet));
        return true;
    }

    if (outBufferSize < static_cast<int>(sizeof(PACKET_CZ_USESKILLTOID2))) {
        return false;
    }

    PACKET_CZ_USESKILLTOID2 packet{};
    packet.PacketType = profile.useSkillToId;
    packet.SkillId = skillId;
    packet.SkillLevel = skillLevel;
    packet.TargetGID = targetGid;
    std::memcpy(outBuffer, &packet, sizeof(packet));
    *outPacketLength = static_cast<int>(sizeof(packet));
    return true;
}

bool BuildActiveCartOffPacket(void* outBuffer,
    int outBufferSize,
    int* outPacketLength)
{
    if (!outBuffer || !outPacketLength || outBufferSize < static_cast<int>(sizeof(PACKET_CZ_REQ_CARTOFF))) {
        return false;
    }

    const MapGameplaySendProfile& profile = GetActiveMapGameplaySendProfile();
    PACKET_CZ_REQ_CARTOFF packet{};
    packet.PacketType = profile.cartOff;
    std::memcpy(outBuffer, &packet, sizeof(packet));
    *outPacketLength = static_cast<int>(sizeof(packet));
    return true;
}

bool BuildActiveChangeCartPacket(u16 type,
    void* outBuffer,
    int outBufferSize,
    int* outPacketLength)
{
    if (!outBuffer || !outPacketLength || outBufferSize < static_cast<int>(sizeof(PACKET_CZ_REQ_CHANGECART))) {
        return false;
    }

    const MapGameplaySendProfile& profile = GetActiveMapGameplaySendProfile();
    PACKET_CZ_REQ_CHANGECART packet{};
    packet.PacketType = profile.changeCart;
    packet.Type = type;
    std::memcpy(outBuffer, &packet, sizeof(packet));
    *outPacketLength = static_cast<int>(sizeof(packet));
    return true;
}

bool BuildActiveUseSkillMapPacket(u16 skillId,
    const char* mapName,
    void* outBuffer,
    int outBufferSize,
    int* outPacketLength)
{
    if (!outBuffer || !outPacketLength || outBufferSize < static_cast<int>(sizeof(PACKET_CZ_USESKILLMAP)) || !mapName) {
        return false;
    }

    const MapGameplaySendProfile& profile = GetActiveMapGameplaySendProfile();
    PACKET_CZ_USESKILLMAP packet{};
    packet.PacketType = profile.useSkillMap;
    packet.SkillId = skillId;
    std::strncpy(packet.MapName, mapName, sizeof(packet.MapName) - 1);
    std::memcpy(outBuffer, &packet, sizeof(packet));
    *outPacketLength = static_cast<int>(sizeof(packet));
    return true;
}

bool BuildActiveUseSkillToPosInfoPacket(u16 skillId,
    u16 skillLevel,
    u16 cellX,
    u16 cellY,
    std::string_view contents,
    void* outBuffer,
    int outBufferSize,
    int* outPacketLength)
{
    if (!outBuffer || !outPacketLength || contents.size() > sizeof(PACKET_CZ_USESKILLTOPOSINFO::Contents)
        || outBufferSize < static_cast<int>(sizeof(PACKET_CZ_USESKILLTOPOSINFO))) {
        return false;
    }

    const MapGameplaySendProfile& profile = GetActiveMapGameplaySendProfile();
    if (profile.id != MapGameplaySendProfileId::PacketVer23) {
        return false;
    }

    PACKET_CZ_USESKILLTOPOSINFO packet{};
    packet.PacketType = profile.useSkillToPosInfo;
    packet.SkillLevel = skillLevel;
    packet.SkillId = skillId;
    packet.X = cellX;
    packet.Y = cellY;
    if (!contents.empty()) {
        std::memcpy(packet.Contents, contents.data(), contents.size());
    }
    std::memcpy(outBuffer, &packet, sizeof(packet));
    *outPacketLength = static_cast<int>(sizeof(packet));
    return true;
}

bool BuildActiveUseItemPacket(u16 itemIndex,
    u32 targetAid,
    void* outBuffer,
    int outBufferSize,
    int* outPacketLength)
{
    if (!outBuffer || !outPacketLength || outBufferSize <= 0) {
        return false;
    }

    const MapGameplaySendProfile& profile = GetActiveMapGameplaySendProfile();
    if (profile.id == MapGameplaySendProfileId::PacketVer22) {
        if (outBufferSize < static_cast<int>(sizeof(PACKET_CZ_USEITEM_PACKETVER22))) {
            return false;
        }

        PACKET_CZ_USEITEM_PACKETVER22 packet{};
        packet.PacketType = profile.useItem;
        FillPacketPadding(packet.padding0, static_cast<int>(sizeof(packet.padding0)));
        packet.ItemIndex = itemIndex;
        FillPacketPadding(packet.padding1, static_cast<int>(sizeof(packet.padding1)));
        packet.TargetAID = targetAid;
        std::memcpy(outBuffer, &packet, sizeof(packet));
        *outPacketLength = static_cast<int>(sizeof(packet));
        return true;
    }

    if (outBufferSize < static_cast<int>(sizeof(PACKET_CZ_USEITEM2))) {
        return false;
    }

    PACKET_CZ_USEITEM2 packet{};
    packet.PacketType = profile.useItem;
    packet.ItemIndex = itemIndex;
    packet.TargetAID = targetAid;
    std::memcpy(outBuffer, &packet, sizeof(packet));
    *outPacketLength = static_cast<int>(sizeof(packet));
    return true;
}

bool BuildActiveTakeItemPacket(u32 objectAid,
    void* outBuffer,
    int outBufferSize,
    int* outPacketLength)
{
    if (!outBuffer || !outPacketLength || outBufferSize <= 0 || objectAid == 0) {
        return false;
    }

    const MapGameplaySendProfile& profile = GetActiveMapGameplaySendProfile();
    if (profile.id == MapGameplaySendProfileId::Legacy) {
        if (outBufferSize < static_cast<int>(sizeof(PACKET_CZ_TAKE_ITEM_LEGACY))) {
            return false;
        }

        PACKET_CZ_TAKE_ITEM_LEGACY packet{};
        packet.PacketType = profile.takeItem;
        packet.ObjectAID = objectAid;
        std::memcpy(outBuffer, &packet, sizeof(packet));
        *outPacketLength = static_cast<int>(sizeof(packet));
        return true;
    }

    if (outBufferSize < static_cast<int>(sizeof(PACKET_CZ_TAKE_ITEM2))) {
        return false;
    }

    PACKET_CZ_TAKE_ITEM2 packet{};
    packet.PacketType = profile.takeItem;
    packet.padding = 0;
    packet.ObjectAID = objectAid;
    std::memcpy(outBuffer, &packet, sizeof(packet));
    *outPacketLength = static_cast<int>(sizeof(packet));
    return true;
}

} // namespace ro::net
