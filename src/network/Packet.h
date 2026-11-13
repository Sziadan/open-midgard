#pragma once
#include "Types.h"
#include <cstddef>

#pragma pack(push, 1)

enum PacketId {
    // Sent to account server
    PACKETID_CA_LOGIN         = 0x0064,
    PACKETID_CA_ENTER         = 0x0065,  // also sent to char server
    PACKETID_CA_REQ_HASH      = 0x01DB,
    PACKETID_CA_LOGIN_HASH    = 0x01DD,
    PACKETID_CA_LOGIN_HASH_CLIENTINFO = 0x01FA,
    PACKETID_CA_CONNECT_INFO_CHANGED = 0x0200,
    PACKETID_CA_EXE_HASHCHECK = 0x0204,
    PACKETID_CZ_RESTART       = 0x00B2,
    PACKETID_CH_MAKE_CHAR     = 0x0067,
    PACKETID_CH_DELETE_CHAR   = 0x0068,
    PACKETID_CZ_QUITGAME      = 0x018A,
    PACKETID_CA_LOGIN_PCBANG  = 0x0277,
    PACKETID_CA_LOGIN4        = 0x027C,
    PACKETID_CA_LOGIN_CHANNEL = 0x02B0,
    // Received from account server
    PACKETID_AC_ACK_HASH      = 0x01DC,
    PACKETID_AC_ACCEPT_LOGIN  = 0x0069,
    PACKETID_AC_REFUSE_LOGIN  = 0x006A,
    // Received from char server
    PACKETID_HC_ACCEPT_ENTER  = 0x006B,
    PACKETID_HC_REFUSE_ENTER  = 0x006C,
    PACKETID_HC_ACCEPT_MAKECHAR = 0x006D,
    PACKETID_HC_REFUSE_MAKECHAR = 0x006E,
    PACKETID_HC_ACCEPT_DELETECHAR = 0x006F,
    PACKETID_HC_REFUSE_DELETECHAR = 0x0070,
    PACKETID_HC_NOTIFY_ZONESVR= 0x0071,
    // Sent to zone server
    PACKETID_CZ_ENTER         = 0x0072,
    // Received from zone server
    PACKETID_ZC_ACCEPT_ENTER  = 0x0073,
    // Misc
    PACKETID_SC_NOTIFY_BAN    = 0x0081,
    PACKETID_AC_NOTIFY_ERROR  = 0x01F1,
};

namespace PacketProfile {
constexpr int kMapServerClientPacketVersion = 23;

namespace PacketVer23LoginChain {
// 2008-09-10aSakexe keeps the classic account/char opcodes, but the account login
// version/date should still identify the 2008-era client profile when clientinfo.xml
// does not override it.
constexpr u32 kClientDate = 20080910;
constexpr u16 kAccountLogin = PACKETID_CA_LOGIN;
constexpr u16 kAccountLoginChannel = PACKETID_CA_LOGIN_CHANNEL;
constexpr u16 kExeHashCheck = PACKETID_CA_EXE_HASHCHECK;
constexpr u16 kRequestPasswordHash = PACKETID_CA_REQ_HASH;
constexpr u16 kPasswordHashChallenge = PACKETID_AC_ACK_HASH;
constexpr u16 kPasswordHashLogin = PACKETID_CA_LOGIN_HASH;
constexpr u16 kPasswordHashLoginClientInfo = PACKETID_CA_LOGIN_HASH_CLIENTINFO;
constexpr u16 kCharServerEnter = PACKETID_CA_ENTER;
constexpr u16 kSelectCharacter = 0x0066;
constexpr u16 kMakeCharacter = PACKETID_CH_MAKE_CHAR;
constexpr u16 kDeleteCharacter = 0x01FB;
constexpr u16 kNotifyError = PACKETID_AC_NOTIFY_ERROR;
}

namespace ActiveLoginChain {
constexpr u32 kClientDate = PacketVer23LoginChain::kClientDate;
constexpr u16 kAccountLogin = PacketVer23LoginChain::kAccountLogin;
constexpr u16 kAccountLoginChannel = PacketVer23LoginChain::kAccountLoginChannel;
constexpr u16 kExeHashCheck = PacketVer23LoginChain::kExeHashCheck;
constexpr u16 kRequestPasswordHash = PacketVer23LoginChain::kRequestPasswordHash;
constexpr u16 kPasswordHashChallenge = PacketVer23LoginChain::kPasswordHashChallenge;
constexpr u16 kPasswordHashLogin = PacketVer23LoginChain::kPasswordHashLogin;
constexpr u16 kPasswordHashLoginClientInfo = PacketVer23LoginChain::kPasswordHashLoginClientInfo;
constexpr u16 kCharServerEnter = PacketVer23LoginChain::kCharServerEnter;
constexpr u16 kSelectCharacter = PacketVer23LoginChain::kSelectCharacter;
constexpr u16 kMakeCharacter = PacketVer23LoginChain::kMakeCharacter;
constexpr u16 kDeleteCharacter = PacketVer23LoginChain::kDeleteCharacter;
constexpr u16 kNotifyError = PacketVer23LoginChain::kNotifyError;
}

namespace LegacyMapServerSend {
constexpr u16 kWantToConnection = 0x0072;
constexpr u16 kNotifyActorInit = 0x007D;
constexpr u16 kTickSend = 0x007E;
constexpr u16 kWalkToXY = 0x0085;
constexpr u16 kGetCharNameRequest = 0x0094;
constexpr u16 kWhisper = 0x0096;
constexpr u16 kGlobalMessage = 0x008C;
constexpr u16 kSkillUp = 0x0112;
}

namespace PacketVer22MapServerSend {
constexpr u16 kWantToConnection = 0x009B;
constexpr u16 kActionRequest = 0x0190;
constexpr u16 kUseSkillToId = 0x0072;
constexpr u16 kCartOff = 0x012A;
constexpr u16 kChangeCart = 0x01AF;
constexpr u16 kItemCompositionList = 0x017A;
constexpr u16 kItemComposition = 0x017C;
constexpr u16 kItemIdentify = 0x0178;
constexpr u16 kUseSkillToPos = 0x0113;
constexpr u16 kDropItem = 0x0116;
constexpr u16 kUseSkillMap = 0x011B;
constexpr u16 kUseItem = 0x009F;
constexpr u16 kSkillUp = 0x0112;
constexpr u16 kTakeItem = 0x00F5;
constexpr u16 kEquipItem = 0x00A9;
constexpr u16 kUnequipItem = 0x00AB;
constexpr u16 kWalkToXY = 0x00A7;
constexpr u16 kChangeDir = 0x0085;
constexpr u16 kTickSend = 0x0089;
constexpr u16 kGetCharNameRequest = 0x008C;
constexpr u16 kWhisper = 0x0096;
constexpr u16 kGlobalMessage = 0x00F3;
constexpr u16 kNotifyActorInit = 0x007D;
}

namespace PacketVer23MapServerSend {
constexpr u16 kWantToConnection = 0x0436;
constexpr u16 kActionRequest = 0x0437;
constexpr u16 kUseSkillToId = 0x0438;
constexpr u16 kCartOff = 0x012A;
constexpr u16 kChangeCart = 0x01AF;
constexpr u16 kItemCompositionList = 0x017A;
constexpr u16 kItemComposition = 0x017C;
constexpr u16 kItemIdentify = 0x0178;
// Ground skill (CZ_USE_SKILL_TOGROUND): from packet_ver 22 onward this lives at 0x0113 with
// padding; 0x0116 is repurposed (e.g. dropitem). See Ref/RunningServer/packet_db.txt.
constexpr u16 kUseSkillToPos = 0x0113;
constexpr u16 kDropItem = 0x0116;
constexpr u16 kUseSkillToPosInfo = 0x007E;
constexpr u16 kUseSkillMap = 0x011B;
constexpr u16 kUseItem = 0x0439;
constexpr u16 kSkillUp = 0x0112;
constexpr u16 kTakeItem = 0x00F5;
constexpr u16 kEquipItem = 0x00A9;
constexpr u16 kUnequipItem = 0x00AB;
constexpr u16 kWalkToXY = 0x00A7;
constexpr u16 kChangeDir = 0x0085;
constexpr u16 kTickSend = 0x0089;
constexpr u16 kGetCharNameRequest = 0x008C;
constexpr u16 kWhisper = 0x0096;
constexpr u16 kGlobalMessage = 0x00F3;
constexpr u16 kNotifyActorInit = 0x007D;
}

namespace ActiveMapServerSend {
constexpr u16 kWantToConnection = PacketVer23MapServerSend::kWantToConnection;
constexpr u16 kActionRequest = PacketVer23MapServerSend::kActionRequest;
constexpr u16 kUseSkillToId = PacketVer23MapServerSend::kUseSkillToId;
constexpr u16 kCartOff = PacketVer23MapServerSend::kCartOff;
constexpr u16 kChangeCart = PacketVer23MapServerSend::kChangeCart;
constexpr u16 kItemCompositionList = PacketVer23MapServerSend::kItemCompositionList;
constexpr u16 kItemComposition = PacketVer23MapServerSend::kItemComposition;
constexpr u16 kItemIdentify = PacketVer23MapServerSend::kItemIdentify;
constexpr u16 kUseSkillToPos = PacketVer23MapServerSend::kUseSkillToPos;
constexpr u16 kDropItem = PacketVer23MapServerSend::kDropItem;
constexpr u16 kUseSkillToPosInfo = PacketVer23MapServerSend::kUseSkillToPosInfo;
constexpr u16 kUseSkillMap = PacketVer23MapServerSend::kUseSkillMap;
constexpr u16 kUseItem = PacketVer23MapServerSend::kUseItem;
constexpr u16 kSkillUp = PacketVer23MapServerSend::kSkillUp;
constexpr u16 kTakeItem = PacketVer23MapServerSend::kTakeItem;
constexpr u16 kEquipItem = PacketVer23MapServerSend::kEquipItem;
constexpr u16 kUnequipItem = PacketVer23MapServerSend::kUnequipItem;
constexpr u16 kNotifyActorInit = PacketVer23MapServerSend::kNotifyActorInit;
constexpr u16 kTickSend = PacketVer23MapServerSend::kTickSend;
constexpr u16 kWalkToXY = PacketVer23MapServerSend::kWalkToXY;
constexpr u16 kChangeDir = PacketVer23MapServerSend::kChangeDir;
constexpr u16 kGetCharNameRequest = PacketVer23MapServerSend::kGetCharNameRequest;
constexpr u16 kWhisper = PacketVer23MapServerSend::kWhisper;
constexpr u16 kGlobalMessage = PacketVer23MapServerSend::kGlobalMessage;
}

namespace LegacyNpcScriptSend {
constexpr u16 kContactNpc = 0x0090;
constexpr u16 kSelectMenu = 0x00B8;
constexpr u16 kNextClick = 0x00B9;
constexpr u16 kInputNumber = 0x0143;
constexpr u16 kInputString = 0x01D5;
constexpr int kInputStringMaxBytes = 70;
constexpr u16 kCloseDialog = 0x0146;
}

namespace LegacyNpcShopSend {
constexpr u16 kSelectDealType = 0x00C5;
constexpr u16 kPurchaseItemList = 0x00C8;
constexpr u16 kSellItemList = 0x00C9;
}

namespace PacketVer23StorageSend {
// packet_ver 23 inherits the pre-renewal storage packet family from packet_ver 22.
constexpr u16 kMoveToStorage = 0x0094;
constexpr u16 kMoveFromStorage = 0x00F7;
constexpr u16 kCloseStorage = 0x0193;
}

namespace ActiveStorageSend {
constexpr u16 kMoveToStorage = PacketVer23StorageSend::kMoveToStorage;
constexpr u16 kMoveFromStorage = PacketVer23StorageSend::kMoveFromStorage;
constexpr u16 kCloseStorage = PacketVer23StorageSend::kCloseStorage;
}

namespace LegacyPartySend {
constexpr u16 kInviteParty = 0x00FC;
constexpr u16 kReplyPartyInvite = 0x00FF;
constexpr u16 kCreateParty = 0x00F9;
constexpr u16 kLeaveParty = 0x0100;
constexpr u16 kChangePartyOption = 0x0102;
constexpr u16 kRemovePartyMember = 0x0103;
}

namespace ActivePartySend {
constexpr u16 kInviteParty = LegacyPartySend::kInviteParty;
constexpr u16 kReplyPartyInvite = LegacyPartySend::kReplyPartyInvite;
constexpr u16 kCreateParty = LegacyPartySend::kCreateParty;
constexpr u16 kLeaveParty = LegacyPartySend::kLeaveParty;
constexpr u16 kChangePartyOption = LegacyPartySend::kChangePartyOption;
constexpr u16 kRemovePartyMember = LegacyPartySend::kRemovePartyMember;
}

namespace LegacyShortcutSend {
constexpr u16 kKeyChange = 0x02BA;
}
}

// CA_LOGIN: sent to account server  [55 bytes]
struct PACKET_CA_LOGIN {
    u16  PacketType;   // 0x0064
    u32  Version;      // client version
    char ID[24];       // username
    char Passwd[24];   // password
    u8   clienttype;   // 0 = normal
};

struct PACKET_CA_REQ_HASH {
    u16 PacketType;    // 0x01DB
};

struct PACKET_CA_LOGIN_HASH {
    u16 PacketType;    // 0x01DD
    u32 Version;       // client version
    char ID[24];       // username
    u8  PasswdMD5[16]; // MD5(challenge + password) on the normal service path
    u8  clienttype;    // GetAccountType()
};

struct PACKET_CA_LOGIN_HASH_CLIENTINFO {
    u16 PacketType;    // 0x01FA
    u32 Version;
    char ID[24];
    u8  PasswdMD5[16];
    u8  clienttype;
    u8  clientinfo;
};

struct PACKET_CA_CONNECT_INFO_CHANGED {
    u16 PacketType;    // 0x0200
    char ID[24];
};

struct PACKET_CA_EXE_HASHCHECK {
    u16 PacketType;    // 0x0204
    u8  HashValue[16];
};

struct PACKET_CA_LOGIN_CHANNEL {
    u16  PacketType;   // 0x02B0
    u32  Version;
    char ID[24];
    char Passwd[24];
    u8   clienttype;
    char IP[16];
    char Mac[13];
    u8   IsGravity;
};

// CA_ENTER: sent to char server  [17 bytes]
struct PACKET_CA_ENTER {
    u16 PacketType;    // 0x0065
    u32 AID;           // account ID
    u32 AuthCode;      // auth code from AC_ACCEPT_LOGIN
    u32 UserLevel;     // user level from AC_ACCEPT_LOGIN
    u16 unused;        // = 0
    u8  Sex;           // sex from AC_ACCEPT_LOGIN
};

// CZ_SELECT_CHAR: select character slot  [3 bytes] on classic packet versions
struct PACKET_CZ_SELECT_CHAR {
    u16 PacketType;    // 0x0066
    u8  CharNum;       // character slot number
};

struct PACKET_CZ_RESTART {
    u16 PacketType;    // 0x00B2
    u8  Type;          // 0 = respawn, 1 = return to character select
};

struct PACKET_CZ_QUITGAME {
    u16 PacketType;    // 0x018A
    u16 Type;          // 0 = quit to windows
};

// CH_MAKE_CHAR: classic character creation request [37 bytes]
struct PACKET_CZ_MAKE_CHAR {
    u16 PacketType;    // 0x0067
    char name[24];
    u8   Str;
    u8   Agi;
    u8   Vit;
    u8   Int;
    u8   Dex;
    u8   Luk;
    u8   CharNum;
    u16  hairColor;
    u16  hairStyle;
};

struct PACKET_CH_DELETE_CHAR {
    u16 PacketType;    // 0x0068
    u32 GID;
    char key[40];
};

struct PACKET_CH_DELETE_CHAR_01FB {
    u16 PacketType;
    u32 GID;
    char key[50];
};
static_assert(sizeof(PACKET_CH_DELETE_CHAR_01FB) == 56);

// CZ_ENTER: sent to zone/map server  [19 bytes]
struct PACKET_CZ_ENTER {
    u16 PacketType;    // 0x0072
    u32 AID;           // account ID
    u32 GID;           // character GID
    u32 AuthCode;      // auth code
    u32 ClientTick;    // GetTickCount()
    u8  Sex;           // sex (0=M, 1=F)
};

struct PACKET_CZ_ENTER2 {
    u16 PacketType;    // 0x0436 for packet_ver 23
    u32 AID;           // account ID
    u32 GID;           // character GID
    u32 AuthCode;      // auth code
    u32 ClientTick;    // GetTickCount()
    u8  Sex;           // sex (0=M, 1=F)
};

struct PACKET_CZ_ENTER_PACKETVER22 {
    u16 PacketType;    // 0x009B for packet_ver 22
    u8  padding0[2];
    u32 AID;
    u8  padding1;
    u32 GID;
    u8  padding2[4];
    u32 AuthCode;
    u32 ClientTick;
    u8  Sex;
};

struct PACKET_CZ_ENTER_PADDED22 {
    u16 PacketType;    // 0x0072 with 2004-era padded layout
    u8  padding[3];
    u32 AID;
    u32 GID;
    u32 AuthCode;
    u32 ClientTick;
    u8  Sex;
};

struct PACKET_CZ_TICKSEND2 {
    u16 PacketType;    // 0x0089 for packet_ver 23
    u16 padding;
    u32 ClientTick;
};

struct PACKET_CZ_TICKSEND_LEGACY {
    u16 PacketType;    // 0x007E for legacy packet_ver 5 family
    u32 ClientTick;
};

struct PACKET_CZ_REQUEST_MOVE2 {
    u16 PacketType;    // 0x00A7 for packet_ver 23
    u8   padding[3];
    u8   Dest[3];
};

struct PACKET_CZ_REQUEST_MOVE_LEGACY {
    u16 PacketType;    // 0x0085 for legacy packet_ver 5 family
    u8   Dest[3];
};

struct PACKET_CZ_CHANGE_DIRECTION2 {
    u16 PacketType;    // 0x0085 for packet_ver 23
    u8   padding0[5];
    u16  HeadDir;
    u8   padding1;
    u8   Dir;
};

struct PACKET_CZ_CHANGE_DIRECTION_LEGACY {
    u16 PacketType;    // 0x009B for legacy packet_ver 5 family
    u16 HeadDir;
    u8  Dir;
};

struct PACKET_CZ_REQNAME2 {
    u16 PacketType;    // 0x008C for packet_ver 23
    u8   padding[5];
    u32  GID;
};

struct PACKET_CZ_REQNAME_LEGACY {
    u16 PacketType;    // 0x0094 for legacy packet_ver 5 family
    u32 GID;
};

struct PACKET_CZ_ACTION_REQUEST_PACKETVER22 {
    u16 PacketType;    // 0x0190 for packet_ver 22
    u8  padding0[3];
    u32 TargetGID;
    u8  padding1[9];
    u8  Action;
};

struct PACKET_CZ_ACTION_REQUEST2 {
    u16 PacketType;    // 0x0437 for packet_ver 23
    u32 TargetGID;
    u8  Action;
};

struct PACKET_CZ_USESKILLTOID_PACKETVER22 {
    u16 PacketType;    // 0x0072 for packet_ver 22
    u8  padding0[3];
    u16 SkillLevel;
    u8  padding1[2];
    u16 SkillId;
    u8  padding2[9];
    u32 TargetGID;
    u8  padding3;
};

struct PACKET_CZ_USESKILLTOID2 {
    u16 PacketType;    // 0x0438 for packet_ver 23
    u16 SkillLevel;
    u16 SkillId;
    u32 TargetGID;
};

// 22 bytes, field offsets 5:9:12:20 — skill lv, skill id, x, y (Ref clif_parse_UseSkillToPos).
struct PACKET_CZ_USESKILLTOPOS {
    u16 PacketType;    // 0x0113 for packet_ver 22+ / 23 Sakexe chain
    u8  padding0[3];
    u16 SkillLevel;
    u8  padding1[2];
    u16 SkillId;
    u8  padding2;
    u16 X;
    u8  padding3[6];
    u16 Y;
};

struct PACKET_CZ_USESKILLTOPOSINFO {
    u16 PacketType;    // 0x007E, selected PACKETVER 20080910
    u8  padding0[3];
    u16 SkillLevel;    // byte offset 5
    u8  padding1[2];
    u16 SkillId;       // byte offset 9
    u8  padding2;
    s16 X;             // byte offset 12; server reads signed int16
    u8  padding3[6];
    s16 Y;             // byte offset 20; server reads signed int16
    char Contents[80]; // byte offset 22
};

static_assert(sizeof(PACKET_CZ_USESKILLTOPOSINFO) == 102, "selected 0x007E MoreInfo packet must be 102 bytes");
static_assert(offsetof(PACKET_CZ_USESKILLTOPOSINFO, SkillLevel) == 5, "0x007E skill level offset differs from server registry");
static_assert(offsetof(PACKET_CZ_USESKILLTOPOSINFO, SkillId) == 9, "0x007E skill ID offset differs from server registry");
static_assert(offsetof(PACKET_CZ_USESKILLTOPOSINFO, X) == 12, "0x007E X offset differs from server registry");
static_assert(offsetof(PACKET_CZ_USESKILLTOPOSINFO, Y) == 20, "0x007E Y offset differs from server registry");
static_assert(offsetof(PACKET_CZ_USESKILLTOPOSINFO, Contents) == 22, "0x007E text offset differs from server registry");

struct PACKET_CZ_ITEM_THROW {
    u16 PacketType;    // 0x0116 for packet_ver 22/23
    u8  padding0[3];
    u16 ItemIndex;
    u8  padding1;
    u16 Count;
};

struct PACKET_CZ_USESKILLMAP {
    u16 PacketType;    // 0x011B
    u16 SkillId;
    char MapName[16];
};

struct PACKET_CZ_USEITEM2 {
    u16 PacketType;    // 0x0439 for packet_ver 23
    u16 ItemIndex;
    u32 TargetAID;
};

struct PACKET_CZ_USEITEM_PACKETVER22 {
    u16 PacketType;    // 0x009F for packet_ver 22
    u8  padding0[2];
    u16 ItemIndex;
    u8  padding1[4];
    u32 TargetAID;
};

struct PACKET_CZ_REQ_CARTOFF {
    u16 PacketType;    // 0x012A
};

struct PACKET_CZ_REQ_CHANGECART {
    u16 PacketType;    // 0x01AF
    u16 Type;
};

struct PACKET_CZ_ITEM_COMPOSITION_LIST {
    u16 PacketType;    // 0x017A
    u16 CardItemIndex;
};

struct PACKET_CZ_ITEM_IDENTIFY {
    u16 PacketType;    // 0x0178
    s16 ItemIndex;     // inventory index, or -1 to cancel
};

struct PACKET_CZ_ITEM_COMPOSITION {
    u16 PacketType;       // 0x017C
    u16 CardItemIndex;
    u16 EquipItemIndex;
};

struct PACKET_CZ_SKILLUP {
    u16 PacketType;    // 0x0112
    u16 SkillId;
};

struct PACKET_CZ_STATUS_CHANGE {
    u16 PacketType;    // 0x00BB
    u16 StatusId;
    u8  Amount;
};

struct PACKET_CZ_TAKE_ITEM2 {
    u16 PacketType;    // 0x00F5 for packet_ver 23 profile
    u16 padding;
    u32 ObjectAID;
};

struct PACKET_CZ_TAKE_ITEM_LEGACY {
    u16 PacketType;    // 0x009F for legacy profile
    u32 ObjectAID;
};

struct PACKET_CZ_REQ_WEAR_EQUIP {
    u16 PacketType;    // 0x00A9
    u16 ItemIndex;
    u16 WearLocation;
};

struct PACKET_CZ_REQ_TAKEOFF_EQUIP {
    u16 PacketType;    // 0x00AB
    u16 ItemIndex;
};

struct PACKET_CZ_CONTACTNPC {
    u16 PacketType;    // 0x0090
    u32 NpcId;
    u8  Type;
};

// Homunculus / mercenary move & attack (RunningServer packet_db: 0x0232/9, 0x0233/11, 0x0234/6).
struct PACKET_CZ_REQUEST_MOVENPC {
    u16 PacketType;    // 0x0232
    u32 GID;
    u8  dest[3];
};

struct PACKET_CZ_REQUEST_ACTNPC {
    u16 PacketType;    // 0x0233
    u32 GID;
    u32 targetGID;
    u8  action;
};

struct PACKET_CZ_REQUEST_MOVETOOWNER {
    u16 PacketType;    // 0x0234
    u32 GID;
};

struct PACKET_CZ_NPC_SELECTMENU {
    u16 PacketType;    // 0x00B8
    u32 NpcId;
    u8  Choice;
};

struct PACKET_CZ_NPC_NEXT_CLICK {
    u16 PacketType;    // 0x00B9
    u32 NpcId;
};

struct PACKET_CZ_NPC_INPUT_NUMBER {
    u16 PacketType;    // 0x0143
    u32 NpcId;
    u32 Value;
};

struct PACKET_CZ_NPC_INPUT_STRING {
    u16 PacketType;    // 0x01D5
    u16 PacketLength;
    u32 NpcId;
    // Followed by a null-terminated string payload.
};

struct PACKET_CZ_NPC_CLOSE_DIALOG {
    u16 PacketType;    // 0x0146
    u32 NpcId;
};

struct PACKET_CZ_ACK_SELECT_DEALTYPE {
    u16 PacketType;    // 0x00C5
    u32 NpcId;
    u8  Type;          // 0 = buy, 1 = sell
};

struct PACKET_CZ_PC_PURCHASE_ITEMLIST {
    u16 PacketType;    // 0x00C8
    u16 PacketLength;
    // Followed by repeated { amount.W, itemId.W } rows.
};

struct PACKET_CZ_PC_SELL_ITEMLIST {
    u16 PacketType;    // 0x00C9
    u16 PacketLength;
    // Followed by repeated { index.W, amount.W } rows.
};

struct PACKET_CZ_CREATE_GROUP {
    u16 PacketType;    // 0x00F9
    char PartyName[24];
};

struct PACKET_CZ_REQ_JOIN_GROUP {
    u16 PacketType;    // 0x00FC
    u32 AccountId;
};

struct PACKET_CZ_JOIN_GROUP {
    u16 PacketType;    // 0x00FF
    u32 PartyId;
    u32 Flag;
};

struct PACKET_CZ_REQ_LEAVE_GROUP {
    u16 PacketType;    // 0x0100
};

struct PACKET_CZ_CHANGE_GROUP_MASTER {
    u16 PacketType;    // 0x0102
    u16 ExpOption;
    u16 ItemOption;
};

struct PACKET_CZ_REQ_EXPEL_GROUP_MEMBER {
    u16 PacketType;    // 0x0103
    u32 AccountId;
    char CharacterName[24];
};

struct PACKET_CZ_MOVE_ITEM_TO_STORE {
    u16 PacketType;    // 0x0094 for packet_ver 22/23 pre-renewal storage family
    u8  padding0[5];
    u16 ItemIndex;
    u8  padding1;
    u32 Count;
};

struct PACKET_CZ_MOVE_ITEM_FROM_STORE {
    u16 PacketType;    // 0x00F7 for packet_ver 22/23 pre-renewal storage family
    u8  padding0[12];
    u16 ItemIndex;
    u8  padding1[2];
    u32 Count;
};

struct PACKET_CZ_CLOSE_STORE {
    u16 PacketType;    // 0x0193 for packet_ver 22/23 pre-renewal storage family
};

struct PACKET_CZ_SHORTCUT_KEY_CHANGE {
    u16 PacketType;    // 0x02BA
    u16 Index;
    u8  IsSkill;       // 0 = item, 1 = skill
    u32 Id;            // item or skill id
    u16 Count;         // skill level or item placeholder
};

// Legacy PCBANG login
struct PACKET_CA_LOGIN_PCBANG {
    u16 PacketType;
    u32 Version;
    char ID[24];
    char Passwd[24];
    u8 clienttype;
    char IP[16];
    char Mac[13];
};

struct PACKET_CA_LOGIN4 {
    u16 PacketType;
    u32 Version;
    char ID[24];
    u8 PasswdMD5[16];
    u8 clienttype;
    char Mac[13];
};

// AC_ACCEPT_LOGIN: header portion  [47 bytes + variable server list]
struct PACKET_AC_ACCEPT_LOGIN {
    u16 PacketType;       // 0x0069
    u16 PacketLength;     // total packet size
    u32 AuthCode;         // session auth code
    u32 AID;              // account ID
    u32 UserLevel;        // privilege level
    u32 lastLoginIP;      // previous login IP
    char lastLoginTime[26]; // previous login time string
    u8  sex;              // 0=M, 1=F (may be +10 for some regions)
    // followed by SERVER_ADDR entries (32 bytes each)
};

// AC_REFUSE_LOGIN  [23 bytes]
struct PACKET_AC_REFUSE_LOGIN {
    u16 PacketType;       // 0x006A
    u8  ErrorCode;
    char BlockDate[20];
};

// HC_ACCEPT_MAKECHAR: accepted new character [110 bytes]
struct PACKET_HC_ACCEPT_MAKECHAR {
    u16 PacketType;       // 0x006D
    CHARACTER_INFO character;
};

// HC_REFUSE_MAKECHAR: denied new character [3 bytes]
struct PACKET_HC_REFUSE_MAKECHAR {
    u16 PacketType;       // 0x006E
    u8  ErrorCode;
};

static_assert(sizeof(PACKET_CA_LOGIN) == 55, "PACKET_CA_LOGIN size mismatch");
static_assert(sizeof(PACKET_CA_LOGIN_PCBANG) == 84, "PACKET_CA_LOGIN_PCBANG size mismatch");
static_assert(sizeof(PACKET_CA_LOGIN4) == 60, "PACKET_CA_LOGIN4 size mismatch");
static_assert(sizeof(PACKET_CA_REQ_HASH) == 2, "PACKET_CA_REQ_HASH size mismatch");
static_assert(sizeof(PACKET_CA_LOGIN_HASH) == 47, "PACKET_CA_LOGIN_HASH size mismatch");
static_assert(sizeof(PACKET_CA_LOGIN_HASH_CLIENTINFO) == 48, "PACKET_CA_LOGIN_HASH_CLIENTINFO size mismatch");
static_assert(sizeof(PACKET_CA_CONNECT_INFO_CHANGED) == 26, "PACKET_CA_CONNECT_INFO_CHANGED size mismatch");
static_assert(sizeof(PACKET_CA_ENTER) == 17, "PACKET_CA_ENTER size mismatch");
static_assert(sizeof(PACKET_CZ_SELECT_CHAR) == 3, "PACKET_CZ_SELECT_CHAR size mismatch");
static_assert(sizeof(PACKET_CZ_STATUS_CHANGE) == 5, "PACKET_CZ_STATUS_CHANGE size mismatch");
static_assert(sizeof(PACKET_CZ_RESTART) == 3, "PACKET_CZ_RESTART size mismatch");
static_assert(sizeof(PACKET_CZ_QUITGAME) == 4, "PACKET_CZ_QUITGAME size mismatch");
static_assert(sizeof(PACKET_CZ_MAKE_CHAR) == 37, "PACKET_CZ_MAKE_CHAR size mismatch");
static_assert(sizeof(PACKET_CH_DELETE_CHAR) == 46, "PACKET_CH_DELETE_CHAR size mismatch");
static_assert(sizeof(PACKET_CZ_ENTER) == 19, "PACKET_CZ_ENTER size mismatch");
static_assert(sizeof(PACKET_CZ_ENTER2) == 19, "PACKET_CZ_ENTER2 size mismatch");
static_assert(sizeof(PACKET_CZ_ENTER_PACKETVER22) == 26, "PACKET_CZ_ENTER_PACKETVER22 size mismatch");
static_assert(sizeof(PACKET_CZ_ENTER_PADDED22) == 22, "PACKET_CZ_ENTER_PADDED22 size mismatch");
static_assert(sizeof(PACKET_CZ_TICKSEND2) == 8, "PACKET_CZ_TICKSEND2 size mismatch");
static_assert(sizeof(PACKET_CZ_TICKSEND_LEGACY) == 6, "PACKET_CZ_TICKSEND_LEGACY size mismatch");
static_assert(sizeof(PACKET_CZ_REQUEST_MOVE2) == 8, "PACKET_CZ_REQUEST_MOVE2 size mismatch");
static_assert(sizeof(PACKET_CZ_REQUEST_MOVE_LEGACY) == 5, "PACKET_CZ_REQUEST_MOVE_LEGACY size mismatch");
static_assert(sizeof(PACKET_CZ_CHANGE_DIRECTION2) == 11, "PACKET_CZ_CHANGE_DIRECTION2 size mismatch");
static_assert(sizeof(PACKET_CZ_CHANGE_DIRECTION_LEGACY) == 5, "PACKET_CZ_CHANGE_DIRECTION_LEGACY size mismatch");
static_assert(sizeof(PACKET_CZ_REQNAME2) == 11, "PACKET_CZ_REQNAME2 size mismatch");
static_assert(sizeof(PACKET_CZ_REQNAME_LEGACY) == 6, "PACKET_CZ_REQNAME_LEGACY size mismatch");
static_assert(sizeof(PACKET_CZ_ACTION_REQUEST_PACKETVER22) == 19, "PACKET_CZ_ACTION_REQUEST_PACKETVER22 size mismatch");
static_assert(sizeof(PACKET_CZ_ACTION_REQUEST2) == 7, "PACKET_CZ_ACTION_REQUEST2 size mismatch");
static_assert(sizeof(PACKET_CZ_USESKILLTOID_PACKETVER22) == 25, "PACKET_CZ_USESKILLTOID_PACKETVER22 size mismatch");
static_assert(sizeof(PACKET_CZ_USESKILLTOID2) == 10, "PACKET_CZ_USESKILLTOID2 size mismatch");
static_assert(sizeof(PACKET_CZ_USESKILLTOPOS) == 22, "PACKET_CZ_USESKILLTOPOS size mismatch");
static_assert(sizeof(PACKET_CZ_USESKILLTOPOSINFO) == 102, "PACKET_CZ_USESKILLTOPOSINFO size mismatch");
static_assert(sizeof(PACKET_CZ_ITEM_THROW) == 10, "PACKET_CZ_ITEM_THROW size mismatch");
static_assert(sizeof(PACKET_CZ_USESKILLMAP) == 20, "PACKET_CZ_USESKILLMAP size mismatch");
static_assert(sizeof(PACKET_CZ_USEITEM2) == 8, "PACKET_CZ_USEITEM2 size mismatch");
static_assert(sizeof(PACKET_CZ_USEITEM_PACKETVER22) == 14, "PACKET_CZ_USEITEM_PACKETVER22 size mismatch");
static_assert(sizeof(PACKET_CZ_REQ_CARTOFF) == 2, "PACKET_CZ_REQ_CARTOFF size mismatch");
static_assert(sizeof(PACKET_CZ_REQ_CHANGECART) == 4, "PACKET_CZ_REQ_CHANGECART size mismatch");
static_assert(sizeof(PACKET_CZ_ITEM_COMPOSITION_LIST) == 4, "PACKET_CZ_ITEM_COMPOSITION_LIST size mismatch");
static_assert(sizeof(PACKET_CZ_ITEM_IDENTIFY) == 4, "PACKET_CZ_ITEM_IDENTIFY size mismatch");
static_assert(sizeof(PACKET_CZ_ITEM_COMPOSITION) == 6, "PACKET_CZ_ITEM_COMPOSITION size mismatch");
static_assert(sizeof(PACKET_CZ_SKILLUP) == 4, "PACKET_CZ_SKILLUP size mismatch");
static_assert(sizeof(PACKET_CZ_TAKE_ITEM2) == 8, "PACKET_CZ_TAKE_ITEM2 size mismatch");
static_assert(sizeof(PACKET_CZ_TAKE_ITEM_LEGACY) == 6, "PACKET_CZ_TAKE_ITEM_LEGACY size mismatch");
static_assert(sizeof(PACKET_CZ_REQ_WEAR_EQUIP) == 6, "PACKET_CZ_REQ_WEAR_EQUIP size mismatch");
static_assert(sizeof(PACKET_CZ_REQ_TAKEOFF_EQUIP) == 4, "PACKET_CZ_REQ_TAKEOFF_EQUIP size mismatch");
static_assert(sizeof(PACKET_CZ_CONTACTNPC) == 7, "PACKET_CZ_CONTACTNPC size mismatch");
static_assert(sizeof(PACKET_CZ_REQUEST_MOVENPC) == 9, "PACKET_CZ_REQUEST_MOVENPC size mismatch");
static_assert(sizeof(PACKET_CZ_REQUEST_ACTNPC) == 11, "PACKET_CZ_REQUEST_ACTNPC size mismatch");
static_assert(sizeof(PACKET_CZ_REQUEST_MOVETOOWNER) == 6, "PACKET_CZ_REQUEST_MOVETOOWNER size mismatch");
static_assert(sizeof(PACKET_CZ_NPC_SELECTMENU) == 7, "PACKET_CZ_NPC_SELECTMENU size mismatch");
static_assert(sizeof(PACKET_CZ_NPC_NEXT_CLICK) == 6, "PACKET_CZ_NPC_NEXT_CLICK size mismatch");
static_assert(sizeof(PACKET_CZ_NPC_INPUT_NUMBER) == 10, "PACKET_CZ_NPC_INPUT_NUMBER size mismatch");
static_assert(sizeof(PACKET_CZ_NPC_INPUT_STRING) == 8, "PACKET_CZ_NPC_INPUT_STRING size mismatch");
static_assert(sizeof(PACKET_CZ_NPC_CLOSE_DIALOG) == 6, "PACKET_CZ_NPC_CLOSE_DIALOG size mismatch");
static_assert(sizeof(PACKET_CZ_ACK_SELECT_DEALTYPE) == 7, "PACKET_CZ_ACK_SELECT_DEALTYPE size mismatch");
static_assert(sizeof(PACKET_CZ_PC_PURCHASE_ITEMLIST) == 4, "PACKET_CZ_PC_PURCHASE_ITEMLIST size mismatch");
static_assert(sizeof(PACKET_CZ_PC_SELL_ITEMLIST) == 4, "PACKET_CZ_PC_SELL_ITEMLIST size mismatch");
static_assert(sizeof(PACKET_CZ_CREATE_GROUP) == 26, "PACKET_CZ_CREATE_GROUP size mismatch");
static_assert(sizeof(PACKET_CZ_REQ_JOIN_GROUP) == 6, "PACKET_CZ_REQ_JOIN_GROUP size mismatch");
static_assert(sizeof(PACKET_CZ_JOIN_GROUP) == 10, "PACKET_CZ_JOIN_GROUP size mismatch");
static_assert(sizeof(PACKET_CZ_REQ_LEAVE_GROUP) == 2, "PACKET_CZ_REQ_LEAVE_GROUP size mismatch");
static_assert(sizeof(PACKET_CZ_CHANGE_GROUP_MASTER) == 6, "PACKET_CZ_CHANGE_GROUP_MASTER size mismatch");
static_assert(sizeof(PACKET_CZ_REQ_EXPEL_GROUP_MEMBER) == 30, "PACKET_CZ_REQ_EXPEL_GROUP_MEMBER size mismatch");
static_assert(sizeof(PACKET_CZ_MOVE_ITEM_TO_STORE) == 14, "PACKET_CZ_MOVE_ITEM_TO_STORE size mismatch");
static_assert(sizeof(PACKET_CZ_MOVE_ITEM_FROM_STORE) == 22, "PACKET_CZ_MOVE_ITEM_FROM_STORE size mismatch");
static_assert(sizeof(PACKET_CZ_CLOSE_STORE) == 2, "PACKET_CZ_CLOSE_STORE size mismatch");
static_assert(sizeof(PACKET_CZ_SHORTCUT_KEY_CHANGE) == 11, "PACKET_CZ_SHORTCUT_KEY_CHANGE size mismatch");

#pragma pack(pop)
