#include <array>
#include <cassert>
#include <cstdint>
#include <unordered_map>
#include <vector>

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using s16 = std::int16_t;
using msgparam_t = std::intptr_t;

struct vector3d { float x=0, y=0, z=0; };
struct PacketView { u16 packetId=0, packetLength=0; const u8* data=nullptr; };

struct CRagEffect {
    int type=0;
    int message=0;
    vector3d target{};
    int GetEffectType() const { return type; }
    void SendMsg(void*, int msg, msgparam_t value, msgparam_t, msgparam_t) {
        message=msg;
        if(msg==14 && value!=0) target=*reinterpret_cast<const vector3d*>(value);
    }
};

struct CAbleToMakeEffect {
    std::vector<CRagEffect*> m_effectList;
    std::vector<CRagEffect> launched;
    std::vector<int> launchedIds;
    CRagEffect* LaunchEffect(int effectId, vector3d, float) {
        launched.emplace_back();
        launched.back().type=effectId;
        launchedIds.push_back(effectId);
        m_effectList.push_back(&launched.back());
        return &launched.back();
    }
};

struct CGameActor : CAbleToMakeEffect {
    struct Message { int id; msgparam_t arg; };
    u32 m_aid=0;
    int m_job=0;
    int m_effectState=0;
    std::vector<Message> messages;
    void SendMsg(void*, int id, msgparam_t arg, msgparam_t, msgparam_t) { messages.push_back({id,arg}); }
};

struct CWorld {
    CGameActor* m_player=nullptr;
    std::unordered_map<u32,CGameActor*> actors;
    CGameActor* GetGameActorByAID(u32 aid) const {
        const auto found=actors.find(aid);
        return found==actors.end()?nullptr:found->second;
    }
};
struct CGameMode { CWorld* m_world=nullptr; };
struct Session { u32 m_aid=0; bool m_isEffectOn=true; } g_session;

constexpr int kEffectStateHidingMask=0x0004;
constexpr int kEffectStateBurrowMask=0x0002;

u32 ReadLE32(const u8* data) {
    return static_cast<u32>(data[0]) | (static_cast<u32>(data[1])<<8)
        | (static_cast<u32>(data[2])<<16) | (static_cast<u32>(data[3])<<24);
}
u16 ReadLE16(const u8* data) { return static_cast<u16>(data[0] | (static_cast<u16>(data[1])<<8)); }
void DbgLog(const char*, ...) {}

#include "SpiritPacketBodies.inc"

void WriteLE32(u8* data, u32 value) {
    for(int i=0;i<4;++i) data[i]=static_cast<u8>(value>>(8*i));
}
void WriteLE16(u8* data, u16 value) {
    data[0]=static_cast<u8>(value);data[1]=static_cast<u8>(value>>8);
}

int main() {
    CWorld world;
    CGameMode mode{&world};
    CGameActor player;player.m_aid=100;world.m_player=&player;
    CGameActor actor;actor.m_aid=200;world.actors[200]=&actor;
    std::array<u8,8> bytes{};

    g_session={100,false};
    WriteLE32(bytes.data()+2,200);
    HandleSpirits(mode,PacketView{0x01D0,8,bytes.data()});
    HandleSpirits2(mode,PacketView{0x01E1,8,bytes.data()});
    assert(actor.messages.empty()&&actor.launched.empty());

    g_session={100,true};
    HandleSpirits(mode,PacketView{0x01D0,7,bytes.data()});
    HandleSpirits(mode,PacketView{0x01D1,8,bytes.data()});
    HandleSpirits2(mode,PacketView{0x01E1,7,bytes.data()});
    HandleSpirits2(mode,PacketView{0x01E0,8,bytes.data()});
    HandleSpirits(mode,PacketView{0x01D0,8,nullptr});
    assert(actor.messages.empty()&&actor.launched.empty());

    const int jobs[]={4038,24,100};
    const int effects[]={504,629,228};
    const u16 xValues[]={0xFFFE,0x1234,0x8000};
    const int signedX[]={-2,0x1234,-32768};
    for(int i=0;i<3;++i) {
        actor={};actor.m_aid=200;actor.m_job=jobs[i];
        WriteLE16(bytes.data()+6,xValues[i]);
        HandleSpirits(mode,PacketView{0x01D0,8,bytes.data()});
        assert(actor.messages.size()==1&&actor.messages[0].id==109&&actor.messages[0].arg==effects[i]);
        assert(actor.launchedIds==std::vector<int>{effects[i]});
        assert(actor.launched[0].message==14&&actor.launched[0].target.x==static_cast<float>(signedX[i])
            &&actor.launched[0].target.y==0&&actor.launched[0].target.z==0);
    }

    for(int hiddenMask:{kEffectStateBurrowMask,kEffectStateHidingMask}) {
        actor={};actor.m_aid=200;actor.m_job=24;actor.m_effectState=hiddenMask;
        HandleSpirits(mode,PacketView{0x01D0,8,bytes.data()});
        assert(actor.messages.size()==2&&actor.messages[0].id==109&&actor.messages[0].arg==629
            &&actor.messages[1].id==109&&actor.messages[1].arg==629&&actor.launched.empty());
    }

    player={};player.m_aid=100;player.m_job=4038;world.m_player=&player;
    WriteLE32(bytes.data()+2,100);
    HandleSpirits(mode,PacketView{0x01D0,8,bytes.data()});
    assert(player.messages.size()==1&&player.messages[0].arg==504&&player.launched.empty());

    for(int i=0;i<3;++i) {
        actor={};actor.m_aid=200;actor.m_job=jobs[i];
        WriteLE32(bytes.data()+2,200);WriteLE16(bytes.data()+6,xValues[i]);
        HandleSpirits2(mode,PacketView{0x01E1,8,bytes.data()});
        assert(actor.messages.empty()&&actor.launchedIds==std::vector<int>{effects[i]});
        assert(actor.launched[0].message==14&&actor.launched[0].target.x==static_cast<float>(signedX[i])
            &&actor.launched[0].target.y==0&&actor.launched[0].target.z==0);

        actor.messages.clear();
        HandleSpirits2(mode,PacketView{0x01E1,8,bytes.data()});
        assert(actor.messages.empty()&&actor.launchedIds==std::vector<int>{effects[i]});
    }

    actor={};actor.m_aid=200;actor.m_job=4038;actor.m_effectState=kEffectStateBurrowMask;
    HandleSpirits2(mode,PacketView{0x01E1,8,bytes.data()});
    assert(actor.messages.size()==1&&actor.messages[0].id==109&&actor.messages[0].arg==504&&actor.launched.empty());

    actor={};actor.m_aid=200;actor.m_job=24;actor.m_effectList.push_back(nullptr);
    actor.launched.emplace_back();actor.launched.back().type=629;actor.m_effectList.push_back(&actor.launched.back());
    HandleSpirits2(mode,PacketView{0x01E1,8,bytes.data()});
    assert(actor.launchedIds.empty());

    g_session.m_aid=0;player={};player.m_job=24;world.m_player=&player;
    WriteLE32(bytes.data()+2,0);WriteLE16(bytes.data()+6,0xFFFD);
    HandleSpirits(mode,PacketView{0x01D0,8,bytes.data()});
    assert(player.messages.size()==1&&player.messages[0].arg==629&&player.launched.empty());
    player.messages.clear();
    HandleSpirits2(mode,PacketView{0x01E1,8,bytes.data()});
    assert(player.messages.empty()&&player.launched.empty());

    CGameMode noWorld{};
    g_session={100,true};WriteLE32(bytes.data()+2,999);
    HandleSpirits(noWorld,PacketView{0x01D0,8,bytes.data()});
    assert(actor.messages.empty());
    return 0;
}
