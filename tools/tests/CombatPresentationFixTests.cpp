#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <list>
#include <vector>
using u8=std::uint8_t;
using u32=std::uint32_t;
struct vector3d { float x=0,y=0,z=0; };
struct matrix {};
bool near(float a,float b) { return std::abs(a-b)<0.0001f; }

namespace falcon {
struct Actor { vector3d m_pos; } owner;
struct Attr { float GetHeight(float,float) { return 0; } } attr;
struct World { Attr* m_attr=&attr; } world;
struct CGameMode { World* m_world=&world; } mode;
struct Manager { CGameMode* GetCurrentGameMode() { return &mode; } } g_modeMgr;
struct CMsgEffect {
    Actor* m_masterActor=&owner;
    vector3d m_pos,m_destPos2;
    int m_falconIsAttackTarget=0,m_falconIsAnimation=1,m_falconStateCnt=0,
        m_spriteActionIndex=0,m_spriteMotionIndex=0,m_isDisappear=0;
    float m_falconRotation=0,m_falconMotionSpeed=4;
    int Get8Dir(float angle) { return static_cast<int>(angle/45)&7; }
    void ProcessFalcon();
};
int ResolveMsgSpriteMotionCountForEffect(const CMsgEffect&,int) { return 9; }
struct QueuedMsgSpriteEffectDraw {
    bool useReferencePixelRatio=false,usePerspectiveScale=false,bypassZoomClamp=false;
    float referencePixelRatio=1,pixelsPerTile=0,desiredWorldWidthCells=0,zoom=1;
};
float ResolveMsgSpriteScale(float zoom) { return std::clamp(zoom,0.75f,2.0f); }
#include "FalconFixBodies.inc"
void check() {
    CMsgEffect bird;
    for(int i=0;i<2000;++i) { bird.ProcessFalcon(); assert(bird.m_falconStateCnt<=4); }
    bird.m_falconIsAttackTarget=1;bird.m_falconIsAnimation=0;
    bird.m_destPos2={4,2,3};const auto start=bird.m_pos;
    bird.ProcessFalcon();
    assert(!bird.m_isDisappear && bird.m_pos.x==start.x+4 && bird.m_pos.y==start.y+2);
    while(bird.m_falconStateCnt<25) bird.ProcessFalcon();
    assert(!bird.m_isDisappear);bird.ProcessFalcon();assert(bird.m_isDisappear);
    QueuedMsgSpriteEffectDraw draw;draw.useReferencePixelRatio=true;
    for(float ratio:{0.1f,0.5f,1.0f,2.0f,4.0f}) {
        draw.referencePixelRatio=ratio;draw.zoom=1;
        assert(near(ResolveQueuedMsgSpriteScale(draw,50),ratio));
        draw.zoom=1.5f;assert(near(ResolveQueuedMsgSpriteScale(draw,50),ratio*1.5f));
    }
}
}

namespace monk {
#include "EffectGiTypes.inc"
float g_sinTable[360],g_cosTable[360];
struct CTexture {} texture;
struct TextureManager { CTexture* GetTexture(const char*,bool) { return &texture; } } g_texMgr;
struct View { float GetCameraLongitude() { return 0; } float GetCameraLatitude() { return 0; } };
using CView=View;
struct CGameMode { View* m_view=nullptr; };
struct Manager { CGameMode* GetCurrentGameMode() { return nullptr; } } g_modeMgr;
struct CRenderObject { vector3d m_pos; };
constexpr int PP_TANJI2=0x61,PP_SHIELDBOOMERANG=0x2a;
struct CRagEffect;
struct CEffectPrim {
    int m_type=0,m_renderFlag=0,m_duration=0;
    float m_size=0;
    vector3d m_pos,m_deltaPos;
    std::array<EffectGi,4> m_gi;
    std::vector<CTexture*> m_texture;
    CRagEffect* m_master=nullptr;
};
struct CRagEffect {
    CRenderObject* m_master=nullptr;
    int m_stateCnt=0,m_count=5,m_duration=120;
    vector3d m_effectPos,m_deltaPos;
    CEffectPrim prim;
    CEffectPrim* LaunchEffectPrim(int type,vector3d) { prim.m_type=type;prim.m_master=this;return &prim; }
    vector3d EffectPosition() { return m_effectPos; }
    bool ProcessTanji(bool);
};
void TryPlaySingleEffectWave(vector3d,const char*,float,bool) {}
void IntimidateTowardScreen(vector3d*,short,short) {}
std::vector<vector3d> rendered;
void SubmitCloudRect(const vector3d* corners,const matrix&,CTexture*,unsigned int,bool,bool,int) {
    for(int i=0;i<4;++i) rendered.push_back(corners[i]);
}
#include "MonkProjectileFixBodies.inc"
void check() {
    for(int i=0;i<360;++i) { g_sinTable[i]=std::sin(i*3.141592653589793/180);g_cosTable[i]=std::cos(i*3.141592653589793/180); }
    CRenderObject actor;actor.m_pos={100,20,200};
    CRagEffect effect;effect.m_master=&actor;effect.m_deltaPos={180,20,260};
    assert(effect.ProcessTanji(false));auto& prim=effect.prim;auto& gi=prim.m_gi[0];
    assert(gi.sparkX==100 && gi.sparkY==12 && gi.sparkZ==200);
    for(int i=0;i<16;++i) UpdateReferenceShieldBoomerang(&prim);
    assert(gi.process==1 && gi.sparkX>100 && gi.sparkZ>200);
    const float distance=std::hypot(180-gi.sparkX,260-gi.sparkZ);
    assert(distance<100 && distance>97);
    // A world-space impact streak must not be translated by caster position again.
    for(auto& lane:prim.m_gi) lane.life=0;
    gi.life=1;gi.alphaB=200;gi.sparkX=180;gi.sparkY=12;gi.sparkZ=260;
    gi.flag[2]=1;gi.distance=3;gi.fullDisplayAngle=0;
    matrix view;rendered.clear();RenderReferenceCloud(prim,view);
    assert(rendered.size()==4);
    vector3d center;for(auto p:rendered) { center.x+=p.x/4;center.y+=p.y/4;center.z+=p.z/4; }
    assert(near(center.x,180) && near(center.y,12) && near(center.z,260));
}
}

namespace units {
int g_isSkipFrame=1;u32 now=0;
u32 timeGetTime() { return now; }
struct { bool m_isEffectOn=true; } g_session;
struct Model { int m_animType=0;void AdvanceFrame() {} };
struct CSkill {
    int m_job=127;bool m_serverOwnedUnit=false;u32 m_SkillStartTime=0,m_launchCnt=0;
    Model* m_3dactor=nullptr;
    int periodic=0;
    void ProcessWillBeAttacked() {}
    void LaunchPeriodic(int,int) { ++periodic; }
    void LaunchLoop(int) {}
    u8 OnProcess();
};
#include "GroundUnitLifetimeBody.inc"
void check() {
    CSkill wall;wall.m_serverOwnedUnit=true;
    for(int skip:{0,1}) { g_isSkipFrame=skip;now=14000;assert(wall.OnProcess()); }
    wall.m_serverOwnedUnit=false;assert(!wall.OnProcess());
    CSkill mine;mine.m_job=143;mine.m_serverOwnedUnit=true;
    for(now=0;now<=25000;now+=24) assert(mine.OnProcess());
    // Blast Mine expiry belongs to the server's removal packet, not this processor.
}
}

namespace inventory {
struct ITEM_INFO { int m_itemType,m_wearLocation,m_itemIndex;unsigned int id;unsigned int GetItemId() const { return id; } };
struct Session { std::list<ITEM_INFO> items;const std::list<ITEM_INFO>& GetInventoryItems() const { return items; } } g_session;
struct UIItemWnd {
    int m_currentTab=2;
    std::vector<const ITEM_INFO*> GetFilteredItems() const;
    int GetInventoryItemCount() const;
};
#include "InventoryAmmoBodies.inc"
void check() {
    g_session.items={{10,0x8000,1,1750},{10,0,2,1751},{4,2,3,1101},{3,0,4,909}};
    UIItemWnd window;auto items=window.GetFilteredItems();
    assert(items.size()==3 && items[0]->id==1750 && items[1]->id==1751 && items[2]->id==909);
    assert(window.GetInventoryItemCount()==3);
    window.m_currentTab=1;assert(window.GetFilteredItems().empty());
}
}
int main() {
    falcon::check();monk::check();units::check();inventory::check();
    std::cout << "Production checks passed: falcon idle/dive and world scaling, monk origin/target rendering, server-owned wall/trap persistence, equipped ammo in ETC\n";
}
