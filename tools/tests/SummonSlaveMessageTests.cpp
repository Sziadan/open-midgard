#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <vector>
struct vector3d { float x=0,y=0,z=0; };
struct tlvertex3d { float x=0,y=0; };
struct CRenderObject {
    vector3d m_pos;
    int calls=0, message=-1;
    void SendMsg(void* sender, int msg, std::intptr_t a, std::intptr_t b, std::intptr_t c) {
        // Message 0 interprets a as a movement pointer in the real actor.
        assert(sender==nullptr && msg==63 && a==0 && b==0 && c==0);
        ++calls; message=msg;
    }
};
struct View { int GetViewMatrix() { return 0; } } view;
struct CGameMode { View* m_view=&view; } mode;
struct ModeMgr { CGameMode* GetCurrentGameMode() { return &mode; } } g_modeMgr;
void ProjectReferenceEffectVertex(vector3d pos, int, tlvertex3d& out) { out={pos.x,pos.z}; }
struct TextureMgr { int* GetTexture(const char*, bool) { return nullptr; } } g_texMgr;
constexpr int PP_2DFLASH=1;
struct Prim {
    int m_duration=0,m_fadeOutCnt=0;
    vector3d m_deltaPos2;
    float m_longitude=0,m_longSpeed=0,m_longAccel=0,m_arcAngle=0,m_heightSize=0,
        m_heightSpeed=0,m_alpha=0,m_maxAlpha=0,m_alphaSpeed=0;
    std::vector<int*> m_texture;
};
using CEffectPrim=Prim;
struct CRagEffect {
    CRenderObject* master=nullptr;
    int m_stateCnt=0;
    float m_tlvertX=0,m_tlvertY=0;
    std::vector<Prim> particles;
    CRenderObject* ResolveCurrentMaster() { return master; }
    Prim* LaunchEffectPrim(int type, vector3d) {
        assert(type==PP_2DFLASH); particles.emplace_back(); return &particles.back();
    }
    void SpawnSummonSlave();
};
#include "SummonSlaveBody.inc"
int main() {
    CRenderObject monster; monster.m_pos={12,3,45};
    CRagEffect effect; effect.master=&monster;
    for (int state=0;state<=40;++state) {
        effect.m_stateCnt=state;
        effect.SpawnSummonSlave();
        assert(monster.calls==(state>=35 ? 1 : 0));
        assert(effect.particles.size()==20);
        assert(effect.m_tlvertX==12 && effect.m_tlvertY==45);
    }
    assert(monster.message==63);
    std::cout << "Production summon effect: states 0-40, smoke dispatch, no invalid movement pointer passed\n";
}
