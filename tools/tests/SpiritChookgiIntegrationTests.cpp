#include <array>
#include <cassert>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using s16 = std::int16_t;
using msgparam_t = std::intptr_t;

struct vector3d { float x=0, y=0, z=0; };
struct PacketView { u16 packetId=0, packetLength=0; const u8* data=nullptr; };
struct EffectGi {
    float height[21]{};
    int flag[8]{};
    int life=0, alphaB=0, process=0;
    float distance=0;
};
#include "PrimitiveEnum.inc"
struct CTexture {};
struct TextureManager {
    CTexture texture;
    std::vector<std::string> requests;
    CTexture* GetTexture(const char* path, bool) { requests.emplace_back(path); return &texture; }
} g_texMgr;
int rngDraws=0;
std::vector<int> rngValues;
size_t rngCursor=0;
int SilentRand() { ++rngDraws; return rngValues.at(rngCursor++); }
struct CRenderObject { vector3d m_pos{}; };
struct CEffectPrim {
    int m_renderFlag=0, m_duration=0;
    EffectGi m_gi[2]{};
    std::vector<CTexture*> m_texture;
};
struct CRagEffect;
struct CGameObject {};
struct CAbleToMakeEffect : CRenderObject {
    std::vector<CRagEffect*> m_effectList;
    std::vector<CRagEffect*> launched;
    CRagEffect* LaunchEffect(int, vector3d, float);
};
struct CGameActor : CAbleToMakeEffect {
    u32 m_aid=0;
    int m_job=0, m_effectState=0;
    void SendMsg(void*, int, msgparam_t, msgparam_t, msgparam_t);
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

struct CRagEffect {
    CRenderObject* m_master=nullptr;
    int m_type=0, m_stateCnt=0, m_duration=80;
    vector3d m_deltaPos{}, m_targetPos{}, m_effectPos{};
    std::vector<CEffectPrim> primitives;
    int GetEffectType() const { return m_type; }
    CRenderObject* ResolveCurrentMaster() { return m_master; }
    CEffectPrim* LaunchEffectPrim(EFFECTPRIMID type, const vector3d&) {
        if (type!=PP_CHOOKGI) return nullptr;
        primitives.emplace_back();
        return &primitives.back();
    }
    void SpawnChookgi(char);
    void SendMsg(CGameObject*, int, msgparam_t, msgparam_t, msgparam_t);
};
CRagEffect* CAbleToMakeEffect::LaunchEffect(int type, vector3d, float) {
    auto* effect=new CRagEffect;
    effect->m_type=type;
    effect->m_master=this;
    m_effectList.push_back(effect);
    launched.push_back(effect);
    return effect;
}
void CGameActor::SendMsg(void*, int, msgparam_t, msgparam_t, msgparam_t) {}
void CRagEffect::SendMsg(CGameObject*, int message, msgparam_t first, msgparam_t second, msgparam_t) {
    if (message==14 && first) {
        m_deltaPos=*reinterpret_cast<const vector3d*>(first);
        m_targetPos=m_deltaPos;
    }
    if (message==14 && second) m_deltaPos=*reinterpret_cast<const vector3d*>(second);
}
#include "SpiritChookgiBodies.inc"

void WriteLE32(u8* data, u32 value) {
    for(int i=0;i<4;++i) data[i]=static_cast<u8>(value>>(8*i));
}
void WriteLE16(u8* data, u16 value) { data[0]=static_cast<u8>(value); data[1]=static_cast<u8>(value>>8); }

int main() {
    CWorld world;
    CGameMode mode{&world};
    CGameActor actor; actor.m_aid=200; actor.m_pos={11,22,33};
    world.actors.emplace(actor.m_aid,&actor);
    std::array<u8,8> packet{};
    WriteLE32(packet.data()+2,actor.m_aid);
    const int jobs[]={4038,24,100};
    const int effects[]={504,629,228};
    const char variants[]={1,3,0};
    const int counts[]={2,6,3};
    for (int row=0;row<3;++row) {
        actor.m_job=jobs[row]; actor.launched.clear(); actor.m_effectList.clear();
        WriteLE16(packet.data()+6,static_cast<u16>(counts[row]));
        HandleSpirits(mode,PacketView{0x01D0,8,packet.data()});
        assert(actor.launched.size()==1 && actor.launched[0]->m_type==effects[row]);
        CRagEffect& effect=*actor.launched[0];
        assert(effect.m_deltaPos.x==counts[row] && effect.m_targetPos.x==counts[row]);
        rngValues.clear(); rngCursor=0;
        for(int i=0;i<counts[row]*7;++i) rngValues.push_back((i*17+13)%360);
        g_texMgr.requests.clear();
        const int drawStart=rngDraws;
        effect.SpawnChookgi(variants[row]);
        const int emitted=counts[row]>5?5:counts[row];
        assert(effect.m_effectPos.x==11 && effect.m_effectPos.y==22 && effect.m_effectPos.z==33);
        assert(effect.primitives.size()==static_cast<size_t>(emitted));
        assert(rngDraws-drawStart==emitted*7 && rngCursor==static_cast<size_t>(emitted*7));
        assert(g_texMgr.requests==std::vector<std::string>(emitted,"effect\\thunder_center.bmp"));
        for(int i=0;i<emitted;++i) {
            const auto& prim=effect.primitives[static_cast<size_t>(i)];
            const auto& gi=prim.m_gi[0];
            assert(prim.m_renderFlag==5 && prim.m_duration==80 && gi.life==1 && gi.alphaB==0 && gi.process==0);
            for (int draw=0;draw<6;++draw)
                assert(gi.height[draw]==static_cast<float>(((i*7+draw)*17+13)%360));
            assert(gi.height[8]==static_cast<float>(((i*7+6)*17+13)%360));
            if(variants[row]==1) assert(gi.height[6]==i*72 && gi.height[7]==-14 && gi.distance==1.3f && gi.height[10]==1 && gi.flag[0]==0);
            else if(variants[row]==3) assert(gi.height[6]==(counts[row]>5?i*72:i*360/counts[row])
                && gi.height[7]==-15 && gi.distance==(counts[row]>5 && counts[row]-i>=6?2.0f:1.0f)
                && gi.height[10]==0 && gi.flag[0]==1);
            else assert(gi.height[6]==i*72 && gi.height[7]==-15 && gi.distance==2.0f && gi.height[10]==0 && gi.flag[0]==0);
        }
    }
    return 0;
}
