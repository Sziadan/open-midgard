#include <array>
#include <cassert>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using s16 = std::int16_t;
using msgparam_t = std::intptr_t;

struct vector3d { float x=0, y=0, z=0; };
struct PacketView { u16 packetId=0, packetLength=0; const u8* data=nullptr; };
struct CHARACTER_INFO { s16 hp=0,maxhp=0,sp=0,maxsp=0; };
struct Session {
    u32 m_gid=0,m_aid=0;
    CHARACTER_INFO* selected=nullptr;
    const CHARACTER_INFO* GetSelectedCharacterInfo() const { return selected; }
} g_session;

struct CGameActor {
    struct Message { CGameActor* sender; int id; msgparam_t a,b,c; };
    std::vector<Message> messages;
    std::vector<int> effectIds;
    bool effectLaunchOk=true;
    u32 m_gid=0,m_Hp=0,m_MaxHp=0,m_Sp=0,m_MaxSp=0;
    void SendMsg(CGameActor* sender,int id,msgparam_t a,msgparam_t b,msgparam_t c) {
        messages.push_back({sender,id,a,b,c});
    }
    bool LaunchEffect(int id,vector3d,float) {
        effectIds.push_back(id);
        return effectLaunchOk;
    }
};
using CPlayer = CGameActor;

struct CWorld {
    CGameActor* m_player=nullptr;
};
struct CGameMode {
    CWorld* m_world=nullptr;
    std::unordered_map<u32,CGameActor*> m_runtimeActors;
    int m_isOnQuest=0,m_isPlayerDead=0,m_waitingUseItemAck=0;
};

constexpr u32 kStatusHp=5,kStatusSp=7;
constexpr u32 kEffectIdRecoveryHp=331,kEffectIdRecoverySp=332;
constexpr int kRecoveryNumberKind=114;
constexpr u32 kRecoveryHpNumberColor=0xFF00FF00u,kRecoverySpNumberColor=0xFF0000FFu;

u16 ReadLE16(const u8* data) { return static_cast<u16>(data[0] | (static_cast<u16>(data[1])<<8)); }
u32 ReadLE32(const u8* data) {
    return static_cast<u32>(data[0]) | (static_cast<u32>(data[1])<<8)
        | (static_cast<u32>(data[2])<<16) | (static_cast<u32>(data[3])<<24);
}
void DbgLog(const char*,...) {}

struct StatusUpdate { u32 status=0,value=0; };
std::vector<StatusUpdate> statusUpdates;
bool ApplySelfStatusUpdate(CGameMode&,u32 status,u32 value) {
    statusUpdates.push_back({status,value});
    return true;
}

#include "RecoveryResurrectionPacketBodies.inc"

void WriteLE16(u8* data,u16 value) { data[0]=static_cast<u8>(value);data[1]=static_cast<u8>(value>>8); }
void WriteLE32(u8* data,u32 value) {
    for(int i=0;i<4;++i) data[i]=static_cast<u8>(value>>(8*i));
}

int main() {
    CWorld world;
    CGameMode mode;mode.m_world=&world;
    CGameActor player;player.m_gid=100;world.m_player=&player;
    CGameActor remote;remote.m_gid=200;mode.m_runtimeActors[200]=&remote;
    std::array<u8,8> bytes{};

    // Resurrection packet framing and zero-AID guards.
    WriteLE32(bytes.data()+2,200);
    HandleActorResurrection(mode,PacketView{0x0148,7,bytes.data()});
    assert(remote.messages.empty());
    HandleActorResurrection(mode,PacketView{0x0148,8,nullptr});
    WriteLE32(bytes.data()+2,0);
    HandleActorResurrection(mode,PacketView{0x0148,8,bytes.data()});
    assert(remote.messages.empty()&&player.messages.empty());

    // Remote actors receive message 98 and local session flags are untouched.
    WriteLE32(bytes.data()+2,200);
    mode.m_isOnQuest=1;mode.m_isPlayerDead=1;mode.m_waitingUseItemAck=1;
    HandleActorResurrection(mode,PacketView{0x0148,8,bytes.data()});
    assert(remote.messages.size()==1&&remote.messages[0].id==98&&remote.messages[0].sender==&remote);
    assert(mode.m_isOnQuest==1&&mode.m_isPlayerDead==1&&mode.m_waitingUseItemAck==1);

    // Local AID falls back to the world player and clears the current implementation's flags.
    g_session={100,100,nullptr};
    mode.m_isOnQuest=1;mode.m_isPlayerDead=1;mode.m_waitingUseItemAck=1;
    WriteLE32(bytes.data()+2,100);
    HandleActorResurrection(mode,PacketView{0x0148,8,bytes.data()});
    assert(player.messages.size()==1&&player.messages[0].id==98&&player.messages[0].sender==&player);
    assert(mode.m_isOnQuest==0&&mode.m_isPlayerDead==0&&mode.m_waitingUseItemAck==0);

    // A missing nonlocal runtime actor causes no message or local-state mutation.
    WriteLE32(bytes.data()+2,300);
    HandleActorResurrection(mode,PacketView{0x0148,8,bytes.data()});
    assert(player.messages.size()==1&&mode.m_isOnQuest==0&&mode.m_isPlayerDead==0);

    // HP and SP recovery clamp to the current world player's maxima and emit exact UI/effect payloads.
    statusUpdates.clear();player.messages.clear();player.effectIds.clear();
    player.m_Hp=95;player.m_MaxHp=100;
    WriteLE16(bytes.data()+2,5);WriteLE16(bytes.data()+4,12);
    HandleRecovery(mode,PacketView{0x013D,6,bytes.data()});
    assert(statusUpdates.size()==1&&statusUpdates[0].status==kStatusHp&&statusUpdates[0].value==100);
    assert(player.messages.size()==1&&player.messages[0].sender==&player&&player.messages[0].id==88
        &&player.messages[0].a==12&&player.messages[0].b==static_cast<std::int32_t>(kRecoveryHpNumberColor)
        &&player.messages[0].c==114&&player.effectIds==std::vector<int>{331});

    statusUpdates.clear();player.messages.clear();player.effectIds.clear();
    player.m_Sp=20;player.m_MaxSp=25;
    WriteLE16(bytes.data()+2,7);WriteLE16(bytes.data()+4,9);
    HandleRecovery(mode,PacketView{0x013D,6,bytes.data()});
    assert(statusUpdates.size()==1&&statusUpdates[0].status==kStatusSp&&statusUpdates[0].value==25);
    assert(player.messages.size()==1&&player.messages[0].id==88&&player.messages[0].a==9
        &&player.messages[0].b==static_cast<std::int32_t>(kRecoverySpNumberColor)
        &&player.messages[0].c==114&&player.effectIds==std::vector<int>{332});

    // Invalid framing and zero amount are closed no-op gates.
    statusUpdates.clear();player.messages.clear();player.effectIds.clear();
    HandleRecovery(mode,PacketView{0x013D,5,bytes.data()});
    WriteLE16(bytes.data()+4,0);
    HandleRecovery(mode,PacketView{0x013D,6,bytes.data()});
    HandleRecovery(mode,PacketView{0x013D,6,nullptr});
    assert(statusUpdates.empty()&&player.messages.empty()&&player.effectIds.empty());

    // Selected-character fallback is used when the world player is unavailable.
    CHARACTER_INFO info{95,100,4,8};g_session.selected=&info;world.m_player=nullptr;
    WriteLE16(bytes.data()+2,5);WriteLE16(bytes.data()+4,20);statusUpdates.clear();
    HandleRecovery(mode,PacketView{0x013D,6,bytes.data()});
    assert(statusUpdates.size()==1&&statusUpdates[0].status==kStatusHp&&statusUpdates[0].value==100);

    // The local handler still emits a green recovery number for unknown status IDs,
    // although the canonical Ref handler does nothing for them.
    world.m_player=&player;player.messages.clear();player.effectIds.clear();statusUpdates.clear();
    WriteLE16(bytes.data()+2,99);WriteLE16(bytes.data()+4,3);
    HandleRecovery(mode,PacketView{0x013D,6,bytes.data()});
    assert(statusUpdates.empty()&&player.messages.size()==1&&player.messages[0].id==88
        &&player.messages[0].a==3&&player.messages[0].b==static_cast<std::int32_t>(kRecoveryHpNumberColor)
        &&player.effectIds.empty());

    // A failed cosmetic effect launch does not suppress the recovery number.
    player.messages.clear();player.effectIds.clear();player.effectLaunchOk=false;
    WriteLE16(bytes.data()+2,5);WriteLE16(bytes.data()+4,2);
    HandleRecovery(mode,PacketView{0x013D,6,bytes.data()});
    assert(player.messages.size()==1&&player.effectIds==std::vector<int>{331});
    return 0;
}
