#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>
using u32=std::uint32_t;
using u8=std::uint8_t;

namespace frozen {
u32 now=100;
u32 timeGetTime() { return now; }
constexpr int kAttackStateId=2,kSecondAttackStateId=9,kDeathStateId=3;
constexpr float kDefaultMotionDelay=1;
struct CActRes {
    int GetMotionCount(int) { return 5; }
    float GetDelay(int) { return 1; }
};
struct CGameActor {
    int m_isMotionFreezed=1,m_curMotion=3,m_stateId=0,m_isMoving=0;
    u32 m_stateStartTick=1;
    float m_attackMotion=-1,m_motionSpeed=1,m_dist=7;
};
int ResolveAvailableActionIndex(CActRes*,int action) { return action; }
#include "FrozenMotionBodies.inc"
void check() {
    CGameActor actor; CActRes act;
    for (u32 tick : {100u,500u,5000u}) {
        now=tick;
        for (int moving : {0,1}) {
            actor.m_isMoving=moving;
            assert(ResolveTimedMotionIndex(actor,&act,0)==3);
            assert(ResolveSpriteMotionIndex(actor,&act,8)==3);
        }
    }
    actor.m_isMotionFreezed=0; actor.m_isMoving=0;
    now=25; assert(ResolveSpriteMotionIndex(actor,&act,0)==1);
    now=49; assert(ResolveSpriteMotionIndex(actor,&act,0)==2);
    actor.m_isMotionFreezed=1; actor.m_curMotion=99;
    assert(ResolveSpriteMotionIndex(actor,&act,0)==4);
}
}

namespace ground {
struct vector3d { float x=0,y=0,z=0; };
using LONG=long;
struct RECT { LONG left=0,top=0,right=0,bottom=0; };
struct matrix {};
struct tlvertex3d { float x=0,y=0,z=0; };
struct Attr { float GetHeight(float,float) { return -20; } } attr;
struct World { Attr* m_attr=&attr; } g_world;
int g_renderer=0;
bool ProjectPoint(int,const matrix&,vector3d p,tlvertex3d* out) {
    *out={p.x,p.z+p.y,0.5f}; return true;
}
constexpr float kGroundItemScreenScale=1.6f;
u32 now=0;
u32 timeGetTime() { return now; }
struct CItem {
    vector3d m_pos{100,999,200};
    int m_isVisible=1,m_billboardTextureWidth=100,m_billboardTextureHeight=100,
        m_billboardAnchorX=50,m_billboardAnchorY=50;
    RECT m_billboardOpaqueBounds{40,30,60,55};
    int m_isJumping=0,m_isSingleColor=0,m_curMotion=0;
    u32 m_stateStartTick=0;
    float m_sPosY=0,m_sfallingSpeed=0;
    bool EnsureBillboardTexture() { return true; }
    void ProcessMotion() {}
    vector3d GetBillboardPosition() const;
    u8 OnProcess();
};
bool ResolveItemResources(CItem&) { return false; }
#include "GroundItemPresentationBodies.inc"
void check() {
    CItem item; matrix view; RECT rect; int x=0,top=0,label=0; float depth=0;
    assert(GetGroundItemScreenRect(view,item,&rect,&x,&top,&label,&depth));
    assert(x==100 && top==148 && label==148);
    assert(rect.left==84 && rect.right==116 && rect.bottom==188);
    item.m_sPosY=-10;
    assert(GetGroundItemScreenRect(view,item,&rect,&x,&top,&label,&depth));
    assert(x==100 && top==138 && label==138);
    item.m_isVisible=0;
    assert(!GetGroundItemScreenRect(view,item,&rect,&x,&top,&label,&depth));
    // Exact reference cadence: two highlight ticks in every 92 (24 ms ticks).
    int flashes=0;
    for (int tick=0;tick<184;++tick) {
        now=static_cast<u32>(tick*24);
        assert(item.OnProcess()==1);
        assert(bool(item.m_isSingleColor)==(tick%92>=90));
        flashes+=item.m_isSingleColor;
    }
    assert(flashes==4);
}
}

namespace shortcuts {
constexpr int kShortcutSlotsPerPage=9;
struct CGameMode { static constexpr int GameMsg_RequestShortcutUse=1; };
struct ModeMgr {
    int slot=-1;
    void SendMsg(int msg,int index,int,int) { assert(msg==1); slot=index; }
} g_modeMgr;
struct UIShortCutWnd {
    int m_visibleRowCount=3;
    int GetAbsoluteSlotIndex(int) const;
    void ActivateSlot(int);
};
#include "FixedShortcutBodies.inc"
void check() {
    UIShortCutWnd bar;
    for (int repeat=0;repeat<3;++repeat) for (int slot=0;slot<27;++slot) {
        assert(bar.GetAbsoluteSlotIndex(slot)==slot);
        bar.ActivateSlot(slot); assert(g_modeMgr.slot==slot);
    }
    assert(bar.GetAbsoluteSlotIndex(-1)==-1 && bar.GetAbsoluteSlotIndex(27)==-1);
    bar.m_visibleRowCount=1;
    assert(bar.GetAbsoluteSlotIndex(8)==8 && bar.GetAbsoluteSlotIndex(9)==-1);
}
}
int main() {
    frozen::check(); ground::check(); shortcuts::check();
    std::cout << "Production checks passed: frozen frames, item drop/opaque anchors and reference flash cadence, fixed shortcut row activation\n";
}
