#pragma once

#include "Types.h"

#include <string_view>

namespace ro::net {

enum class PacketVersionId : u8 {
    PacketVer23 = 0,
    PacketVer22 = 1,
};

enum class MapGameplaySendProfileId : u8 {
    PacketVer23 = 0,
    PacketVer22 = 1,
    Legacy = 2,
};

enum class ZoneConnectPacketLayout : u8 {
    Compact19 = 0,
    Legacy26 = 1,
    Padded22 = 2,
};

struct AccountLoginPacketProfile {
    PacketVersionId id;
    const char* name;
    u32 clientDate;
    u16 accountLogin;
    u16 accountLoginChannel;
    u16 requestPasswordHash;
    u16 passwordHashChallenge;
    u16 passwordHashLogin;
    u16 connectInfoChanged;
    u16 exeHashCheck;
    u16 notifyError;
};

struct CharacterPacketProfile {
    PacketVersionId id;
    const char* name;
    u16 charServerEnter;
    u16 selectCharacter;
    u16 makeCharacter;
    u16 deleteCharacter;
};

struct ZonePacketProfile {
    PacketVersionId id;
    const char* name;
    u16 wantToConnection;
    ZoneConnectPacketLayout wantToConnectionLayout;
};

struct MapGameplaySendProfile {
    MapGameplaySendProfileId id;
    const char* name;
    u16 actionRequest;
    u16 useSkillToId;
    u16 cartOff;
    u16 changeCart;
    u16 useSkillToPos;
    u16 useSkillToPosInfo;
    u16 useSkillMap;
    u16 useItem;
    u16 takeItem;
    u16 dropItem;
    u16 itemCompositionList;
    u16 itemComposition;
    u16 itemIdentify;
    u16 skillUp;
    u16 equipItem;
    u16 unequipItem;
    u16 walkToXY;
    u16 changeDir;
    u16 tickSend;
    u16 notifyActorInit;
    u16 getCharNameRequest;
    u16 whisper;
    u16 globalMessage;
};

struct MapReceiveProfile {
    PacketVersionId id;
    const char* name;
    bool deferProactiveNameRequests;
    bool usesLegacyActorStream;
    u16 acceptEnterLegacy;
    u16 acceptEnterModern;
    u16 notifyTime;
    u16 mapChangeBasic;
    u16 mapChangeServerMove;
    u16 actorActionNotifyBasic;
    u16 actorActionNotifyExtended;
    u16 actorSetPositionBasic;
    u16 actorSetPositionHighJump;
    u16 broadcastBasic;
    u16 broadcastColored;
    u16 groundItemEntryExisting;
    u16 groundItemEntryDropped;
    u16 itemPickupAckBasic;
    u16 itemPickupAckExtended;
    u16 normalInventoryListBasic;
    u16 normalInventoryListCardSlots;
    u16 normalInventoryListTimed;
    u16 equipInventoryListBasic;
    u16 equipInventoryListTimed;
    u16 equipInventoryListTimedOwned;
    u16 normalStorageListBasic;
    u16 normalStorageListCardSlots;
    u16 normalStorageListTimed;
    u16 equipStorageListBasic;
    u16 equipStorageListTimedOwned;
    u16 storageItemAddedBasic;
    u16 storageItemAddedTyped;
    u16 useItemAckBasic;
    u16 useItemAckExtended;
    u16 itemRemoveBasic;
    u16 itemRemoveExtended;
    u16 partyInviteAckBasic;
    u16 partyInviteAckExtended;
    u16 partyInviteRequestBasic;
    u16 partyInviteRequestExtended;
    u16 skillDamagePositionNotify;
    u16 groundSkillNotify;
    u16 skillNoDamageNotify;
    u16 skillUnitSetBasic;
    u16 skillUnitSetExtended;
    u16 notifyEffectBasic;
    u16 notifyEffectDirect;
    u16 actorSpawnLegacyIdle;
    u16 actorSpawnLegacySpawn;
    u16 actorSpawnLegacyAlt;
    u16 actorSpawnLegacyNpc;
    u16 actorSpawnLegacyIdleShifted;
    u16 actorSpawnLegacySpawnShifted;
    u16 actorMoveLegacy;
    u16 actorMoveLegacyShifted;
    u16 actorSpawnVariableIdle;
    u16 actorSpawnVariableSpawn;
    u16 actorSpawnVariableIdleRobe;
    u16 actorSpawnVariableSpawnRobe;
    u16 actorMoveVariable;
    u16 actorMoveVariableRobe;
    u16 actorSpawnModernIdle;
    u16 actorSpawnModernSpawn;
    u16 actorSpawnModernIdleFont;
    u16 actorSpawnModernSpawnFont;
    u16 actorMoveModern;
    u16 actorMoveModernFont;
    u16 actorNameAckBasic;
    u16 actorNameAckParty;
    u16 actorNameAckFull;
    u16 actorStateChangeBasic;
    u16 actorStateChangeExtended;
    u16 partyMemberAddedBasic;
    u16 partyMemberAddedExtended;
    u16 partyHpUpdateBasic;
    u16 partyHpUpdateExtended;
    u16 skillDamageNotifyBasic;
    u16 skillDamageNotifyExtended;
};

struct PacketProfileSet {
    PacketVersionId id;
    const char* name;
    AccountLoginPacketProfile login;
    CharacterPacketProfile character;
    ZonePacketProfile zone;
    MapGameplaySendProfile mapSend;
    MapReceiveProfile mapReceive;
};

const PacketProfileSet& GetActivePacketProfile();
const AccountLoginPacketProfile& GetActiveAccountLoginPacketProfile();
const CharacterPacketProfile& GetActiveCharacterPacketProfile();
const ZonePacketProfile& GetActiveZonePacketProfile();
const MapReceiveProfile& GetActiveMapReceiveProfile();

const MapGameplaySendProfile& GetActiveMapGameplaySendProfile();
bool IsPacketVer22MapGameplaySendProfile();
bool IsLegacyMapGameplaySendProfile();
bool IsPacketVer22PacketProfile();
bool IsActiveAcceptEnterPacket(u16 packetId);
bool IsActiveMapChangePacket(u16 packetId);
bool IsActiveServerMoveMapChangePacket(u16 packetId);
bool IsActiveActorActionNotifyExtendedPacket(u16 packetId);
bool IsActiveActorSetPositionPacket(u16 packetId);
bool IsActiveBroadcastPacket(u16 packetId);
bool IsActiveColoredBroadcastPacket(u16 packetId);
bool IsActiveGroundItemEntryPacket(u16 packetId);
bool IsActiveGroundItemDroppedPacket(u16 packetId);
bool IsActiveItemPickupAckPacket(u16 packetId);
bool IsActiveNormalInventoryListPacket(u16 packetId);
bool IsActiveEquipInventoryListPacket(u16 packetId);
bool IsActiveNormalStorageListPacket(u16 packetId);
bool IsActiveEquipStorageListPacket(u16 packetId);
bool IsActiveStorageItemAddedPacket(u16 packetId);
bool IsActiveStorageItemAddedTypedPacket(u16 packetId);
bool IsActiveUseItemAckPacket(u16 packetId);
bool IsActiveExtendedUseItemAckPacket(u16 packetId);
bool IsActiveItemRemovePacket(u16 packetId);
bool IsActiveExtendedItemRemovePacket(u16 packetId);
bool IsActivePartyInviteAckPacket(u16 packetId);
bool IsActiveExtendedPartyInviteAckPacket(u16 packetId);
bool IsActivePartyInviteRequestPacket(u16 packetId);
bool IsActiveSkillUnitSetPacket(u16 packetId);
int GetActiveStackableItemListEntrySize(u16 packetId);
int GetActiveEquipmentItemListEntrySize(u16 packetId);
bool IsActiveActorSpawnPacket(u16 packetId);
bool IsActiveLegacyActorSpawnPacket(u16 packetId);
bool IsActiveActorMovePacket(u16 packetId);
bool IsActiveLegacyActorMovePacket(u16 packetId);
bool IsActiveActorNameAckPacket(u16 packetId);
bool IsActiveActorNameAckFullPacket(u16 packetId);
bool IsActiveActorStateChangeExtendedPacket(u16 packetId);
bool IsActivePartyMemberAddedPacket(u16 packetId);
bool IsActivePartyMemberAddedExtendedPacket(u16 packetId);
bool IsActivePartyHpUpdateExtendedPacket(u16 packetId);
bool IsActiveSkillDamageNotifyExtendedPacket(u16 packetId);
bool BuildActiveWantToConnectionPacket(u32 aid,
    u32 gid,
    u32 authCode,
    u32 clientTick,
    u8 sex,
    void* outBuffer,
    int outBufferSize,
    int* outPacketLength);
bool BuildActiveActionRequestPacket(u32 targetGid,
    u8 action,
    void* outBuffer,
    int outBufferSize,
    int* outPacketLength);
bool BuildActiveUseSkillToIdPacket(u16 skillId,
    u16 skillLevel,
    u32 targetGid,
    void* outBuffer,
    int outBufferSize,
    int* outPacketLength);
bool BuildActiveCartOffPacket(void* outBuffer,
    int outBufferSize,
    int* outPacketLength);
bool BuildActiveChangeCartPacket(u16 type,
    void* outBuffer,
    int outBufferSize,
    int* outPacketLength);
bool BuildActiveUseSkillMapPacket(u16 skillId,
    const char* mapName,
    void* outBuffer,
    int outBufferSize,
    int* outPacketLength);
bool BuildActiveUseSkillToPosInfoPacket(u16 skillId,
    u16 skillLevel,
    u16 cellX,
    u16 cellY,
    std::string_view contents,
    void* outBuffer,
    int outBufferSize,
    int* outPacketLength);
bool BuildActiveUseItemPacket(u16 itemIndex,
    u32 targetAid,
    void* outBuffer,
    int outBufferSize,
    int* outPacketLength);
bool BuildActiveTakeItemPacket(u32 objectAid,
    void* outBuffer,
    int outBufferSize,
    int* outPacketLength);

} // namespace ro::net
