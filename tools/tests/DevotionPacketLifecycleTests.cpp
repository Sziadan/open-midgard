#include <array>
#include <cassert>
#include <cstdint>
#include <unordered_map>
#include <vector>

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
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
    struct Launch { int id; vector3d offset; float size; };
    std::vector<CRagEffect> effects;
    std::vector<Launch> launches;
    bool failLaunch=false;
    CRagEffect* LaunchEffect(int effectId, vector3d offset, float size) {
        launches.push_back({effectId,offset,size});
        if(failLaunch) return nullptr;
        effects.emplace_back();
        effects.back().type=effectId;
        return &effects.back();
    }
};

struct CGameActor : CAbleToMakeEffect {
    struct Message { int id; msgparam_t arg; };
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

u32 ReadLE32(const u8* data) {
    return static_cast<u32>(data[0]) | (static_cast<u32>(data[1])<<8)
        | (static_cast<u32>(data[2])<<16) | (static_cast<u32>(data[3])<<24);
}
void DbgLog(const char*, ...) {}

#include "DevotionPacketBody.inc"

void WriteLE32(u8* data, u32 value) {
    for(int i=0;i<4;++i) data[i]=static_cast<u8>(value>>(8*i));
}

int main() {
    CWorld world;
    CGameMode mode{&world};
    CGameActor player;world.m_player=&player;
    CGameActor devoter;world.actors[200]=&devoter;
    std::array<u8,28> bytes{};
    WriteLE32(bytes.data()+2,200);
    const u32 devoteeIds[5]={301,0,301,999,0};
    for(int i=0;i<5;++i) WriteLE32(bytes.data()+6+i*4,devoteeIds[i]);
    bytes[26]=0x34;bytes[27]=0x12; // Source ignores max distance.

    g_session={100,false};
    HandleDevotionList(mode,PacketView{0x01CF,28,bytes.data()});
    assert(devoter.messages.empty()&&devoter.launches.empty());
    g_session.m_isEffectOn=true;
    HandleDevotionList(mode,PacketView{0x01CF,27,bytes.data()});
    HandleDevotionList(mode,PacketView{0x01CE,28,bytes.data()});
    HandleDevotionList(mode,PacketView{0x01CF,28,nullptr});
    assert(devoter.messages.empty()&&devoter.launches.empty());

    HandleDevotionList(mode,PacketView{0x01CF,28,bytes.data()});
    assert(devoter.messages.size()==5);
    for(const auto& message:devoter.messages) assert(message.id==109&&message.arg==232);
    assert(devoter.launches.size()==3);
    const u32 launchedDevotees[3]={301,301,999};
    for(int i=0;i<3;++i) {
        assert(devoter.launches[i].id==232&&devoter.launches[i].offset.x==0
            &&devoter.launches[i].offset.y==0&&devoter.launches[i].offset.z==0
            &&devoter.launches[i].size==0);
        const CRagEffect& effect=devoter.effects[i];
        assert(effect.type==232&&effect.message==14
            &&effect.target.x==200&&effect.target.y==static_cast<float>(launchedDevotees[i])
            &&effect.target.z==0);
    }

    // Self AID is resolved to the local player; each nonzero slot launches regardless
    // of whether that devotee AID resolves to an actor.
    player.messages.clear();player.launches.clear();player.effects.clear();
    g_session.m_aid=100;WriteLE32(bytes.data()+2,100);
    HandleDevotionList(mode,PacketView{0x01CF,28,bytes.data()});
    assert(player.messages.size()==5&&player.launches.size()==3);
    assert(player.effects[0].target.x==100&&player.effects[0].target.y==301);

    // An unresolved devoter cannot receive the five refresh messages or launches.
    WriteLE32(bytes.data()+2,777);
    HandleDevotionList(mode,PacketView{0x01CF,28,bytes.data()});
    assert(player.messages.size()==5&&player.launches.size()==3);

    // Valid packet slots still run in order, but a failed effect launch has no msg14.
    devoter.messages.clear();devoter.launches.clear();devoter.effects.clear();devoter.failLaunch=true;
    WriteLE32(bytes.data()+2,200);
    for(int i=0;i<5;++i) WriteLE32(bytes.data()+6+i*4,static_cast<u32>(400+i));
    HandleDevotionList(mode,PacketView{0x01CF,28,bytes.data()});
    assert(devoter.messages.size()==5&&devoter.launches.size()==5&&devoter.effects.empty());
    return 0;
}
