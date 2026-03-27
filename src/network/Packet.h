#pragma once
#include "Types.h"

#pragma pack(push, 1)

enum PacketId {
    // Sent to account server
    PACKETID_CA_LOGIN         = 0x0064,
    PACKETID_CA_ENTER         = 0x0065,  // also sent to char server
    PACKETID_CZ_RESTART       = 0x00B2,
    PACKETID_CH_MAKE_CHAR     = 0x0067,
    PACKETID_CZ_QUITGAME      = 0x018A,
    PACKETID_CA_LOGIN_PCBANG  = 0x0277,
    // Received from account server
    PACKETID_AC_ACCEPT_LOGIN  = 0x0069,
    PACKETID_AC_REFUSE_LOGIN  = 0x006A,
    // Received from char server
    PACKETID_HC_ACCEPT_ENTER  = 0x006B,
    PACKETID_HC_REFUSE_ENTER  = 0x006C,
    PACKETID_HC_ACCEPT_MAKECHAR = 0x006D,
    PACKETID_HC_REFUSE_MAKECHAR = 0x006E,
    PACKETID_HC_NOTIFY_ZONESVR= 0x0071,
    // Sent to zone server
    PACKETID_CZ_ENTER         = 0x0072,
    // Received from zone server
    PACKETID_ZC_ACCEPT_ENTER  = 0x0073,
    // Misc
    PACKETID_SC_NOTIFY_BAN    = 0x0081,
};

namespace PacketProfile {
constexpr int kMapServerClientPacketVersion = 23;

namespace LegacyMapServerSend {
constexpr u16 kWantToConnection = 0x0072;
constexpr u16 kNotifyActorInit = 0x007D;
constexpr u16 kTickSend = 0x007E;
constexpr u16 kWalkToXY = 0x0085;
constexpr u16 kGetCharNameRequest = 0x0094;
constexpr u16 kGlobalMessage = 0x008C;
}

namespace PacketVer23MapServerSend {
constexpr u16 kWantToConnection = 0x0436;
constexpr u16 kActionRequest = 0x0437;
constexpr u16 kUseSkillToId = 0x0438;
constexpr u16 kUseItem = 0x0439;
constexpr u16 kEquipItem = 0x00A9;
constexpr u16 kUnequipItem = 0x00AB;
constexpr u16 kWalkToXY = 0x00A7;
constexpr u16 kChangeDir = 0x0085;
constexpr u16 kTickSend = 0x0089;
constexpr u16 kGetCharNameRequest = 0x008C;
constexpr u16 kGlobalMessage = 0x00F3;
constexpr u16 kNotifyActorInit = 0x007D;
}

namespace ActiveMapServerSend {
constexpr u16 kWantToConnection = PacketVer23MapServerSend::kWantToConnection;
constexpr u16 kActionRequest = PacketVer23MapServerSend::kActionRequest;
constexpr u16 kUseSkillToId = PacketVer23MapServerSend::kUseSkillToId;
constexpr u16 kUseItem = PacketVer23MapServerSend::kUseItem;
constexpr u16 kEquipItem = PacketVer23MapServerSend::kEquipItem;
constexpr u16 kUnequipItem = PacketVer23MapServerSend::kUnequipItem;
constexpr u16 kNotifyActorInit = PacketVer23MapServerSend::kNotifyActorInit;
constexpr u16 kTickSend = PacketVer23MapServerSend::kTickSend;
constexpr u16 kWalkToXY = PacketVer23MapServerSend::kWalkToXY;
constexpr u16 kGetCharNameRequest = PacketVer23MapServerSend::kGetCharNameRequest;
constexpr u16 kGlobalMessage = PacketVer23MapServerSend::kGlobalMessage;
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

struct PACKET_CZ_TICKSEND2 {
    u16 PacketType;    // 0x0089 for packet_ver 23
    u16 padding;
    u32 ClientTick;
};

struct PACKET_CZ_REQUEST_MOVE2 {
    u16 PacketType;    // 0x00A7 for packet_ver 23
    u8   padding[3];
    u8   Dest[3];
};

struct PACKET_CZ_REQNAME2 {
    u16 PacketType;    // 0x008C for packet_ver 23
    u8   padding[5];
    u32  GID;
};

struct PACKET_CZ_ACTION_REQUEST2 {
    u16 PacketType;    // 0x0437 for packet_ver 23
    u32 TargetGID;
    u8  Action;
};

struct PACKET_CZ_USESKILLTOID2 {
    u16 PacketType;    // 0x0438 for packet_ver 23
    u16 SkillLevel;
    u16 SkillId;
    u32 TargetGID;
};

struct PACKET_CZ_USEITEM2 {
    u16 PacketType;    // 0x0439 for packet_ver 23
    u16 ItemIndex;
    u32 TargetAID;
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

// Legacy PCBANG login
struct PACKET_CA_LOGIN_PCBANG {
    short PacketType;
    unsigned long Version;
    char ID[24];
    char Passwd[24];
    unsigned char clienttype;
    char MacAdress[17];
    char IP[15];
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
static_assert(sizeof(PACKET_CA_ENTER) == 17, "PACKET_CA_ENTER size mismatch");
static_assert(sizeof(PACKET_CZ_SELECT_CHAR) == 3, "PACKET_CZ_SELECT_CHAR size mismatch");
static_assert(sizeof(PACKET_CZ_RESTART) == 3, "PACKET_CZ_RESTART size mismatch");
static_assert(sizeof(PACKET_CZ_QUITGAME) == 4, "PACKET_CZ_QUITGAME size mismatch");
static_assert(sizeof(PACKET_CZ_MAKE_CHAR) == 37, "PACKET_CZ_MAKE_CHAR size mismatch");
static_assert(sizeof(PACKET_CZ_ENTER) == 19, "PACKET_CZ_ENTER size mismatch");
static_assert(sizeof(PACKET_CZ_ENTER2) == 19, "PACKET_CZ_ENTER2 size mismatch");
static_assert(sizeof(PACKET_CZ_TICKSEND2) == 8, "PACKET_CZ_TICKSEND2 size mismatch");
static_assert(sizeof(PACKET_CZ_REQUEST_MOVE2) == 8, "PACKET_CZ_REQUEST_MOVE2 size mismatch");
static_assert(sizeof(PACKET_CZ_REQNAME2) == 11, "PACKET_CZ_REQNAME2 size mismatch");
static_assert(sizeof(PACKET_CZ_ACTION_REQUEST2) == 7, "PACKET_CZ_ACTION_REQUEST2 size mismatch");
static_assert(sizeof(PACKET_CZ_USESKILLTOID2) == 10, "PACKET_CZ_USESKILLTOID2 size mismatch");
static_assert(sizeof(PACKET_CZ_USEITEM2) == 8, "PACKET_CZ_USEITEM2 size mismatch");
static_assert(sizeof(PACKET_CZ_REQ_WEAR_EQUIP) == 6, "PACKET_CZ_REQ_WEAR_EQUIP size mismatch");
static_assert(sizeof(PACKET_CZ_REQ_TAKEOFF_EQUIP) == 4, "PACKET_CZ_REQ_TAKEOFF_EQUIP size mismatch");

#pragma pack(pop)
