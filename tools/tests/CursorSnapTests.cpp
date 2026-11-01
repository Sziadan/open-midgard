#include "world/CursorSnap.h"
#include <cassert>
#include <vector>
#include <iostream>
#include <cstdint>
using u16=std::uint16_t;
struct matrix {};
struct POINT { int x,y; };
struct RECT { int left,top,right,bottom; };
struct { float m_screenXFactor=1; } g_renderer;
struct CGameActor { int m_job=1002; } monster;
struct CItem { RECT rect{100,100,110,110}; float depth=0.5f; bool visible=true; } item;
bool GetGroundItemScreenRect(const matrix&,const CItem& item,RECT* rect,int* x,int* top,int* label,float* depth) {
    if (!item.visible) return false;
    if (rect) *rect=item.rect;
    if (x) *x=(item.rect.left+item.rect.right)/2;
    if (top) *top=item.rect.top;
    if (label) *label=item.rect.top;
    if (depth) *depth=item.depth;
    return true;
}
struct CWorld {
    struct Entry { CGameActor* actor; float left,top,right,bottom; };
    std::vector<Entry> m_billboardFrameEntries{{&monster,200,200,220,240}};
    std::vector<CItem*> m_itemList{&item};
    bool FindHoveredActorScreen(const matrix&,float,int x,int y,CGameActor** actor,int*,int*) const {
        const auto p=ro::cursor::ExpandPickRect({200,200,220,240},g_renderer.m_screenXFactor,40);
        *actor=(x>p.left&&x<p.right&&y>p.top&&y<p.bottom)?&monster:nullptr;
        return *actor!=nullptr;
    }
    bool FindHoveredGroundItemScreen(const matrix&,int,int,CItem**,int*,int*) const;
    bool ResolveCursorSnap(const matrix&,float,int,int,bool,int*,int*) const;
};
enum class ShortcutSkillUseMode { ActorTarget,GroundTarget,SkillTarget };
ShortcutSkillUseMode ResolveShortcutSkillUseMode(u16 id) {
    return id==28?ShortcutSkillUseMode::ActorTarget:ShortcutSkillUseMode::GroundTarget;
}
bool uiBlocked=false;
bool IsWorldHoverBlockedByUi(int,int) { return uiBlocked; }
struct View { matrix m; const matrix& GetViewMatrix() const { return m; } float GetCameraLongitude() const { return 0; } } view;
struct CGameMode {
    static constexpr int MapLoading_None=0;
    CWorld* m_world=nullptr; View* m_view=&view;
    int m_mapLoadingStage=0,m_canRotateView=0,m_isLeftButtonHeld=0,m_dragType=0;
    struct { int id=0,level=0; } m_skillUseInfo;
    bool ResolveCursorDrawPosition(int,int,int*,int*) const;
};
#include "CursorSnapBodies.inc"
int main() {
    auto p=ro::cursor::ExpandPickRect({0,0,10,10},1,34);
    assert(p.left==-12 && p.right==22 && p.top==-12 && p.bottom==22);
    p=ro::cursor::ExpandPickRect({0,0,100,120},1,40);
    assert(p.left==0 && p.right==100 && p.bottom==120);
    p=ro::cursor::ExpandPickRect({0,0,10,10},2,34);
    assert(p.left==-29 && p.right==39);
    CWorld world; CGameMode mode; mode.m_world=&world;
    int rawX=89,rawY=105,x=rawX,y=rawY;
    assert(mode.ResolveCursorDrawPosition(rawX,rawY,&x,&y) && x==105 && y==105);
    assert(rawX==89 && rawY==105);
    assert(!mode.ResolveCursorDrawPosition(88,105,&x,&y));
    assert(mode.ResolveCursorDrawPosition(191,220,&x,&y) && x==210 && y==220);
    monster.m_job=45; assert(!mode.ResolveCursorDrawPosition(210,220,&x,&y)); monster.m_job=1002;
    uiBlocked=true; assert(!mode.ResolveCursorDrawPosition(105,105,&x,&y)); uiBlocked=false;
    mode.m_dragType=1; assert(!mode.ResolveCursorDrawPosition(105,105,&x,&y)); mode.m_dragType=0;
    mode.m_isLeftButtonHeld=1; assert(!mode.ResolveCursorDrawPosition(105,105,&x,&y)); mode.m_isLeftButtonHeld=0;
    mode.m_skillUseInfo={89,10}; assert(!mode.ResolveCursorDrawPosition(210,220,&x,&y));
    mode.m_skillUseInfo={28,10}; assert(mode.ResolveCursorDrawPosition(210,220,&x,&y));
    assert(!mode.ResolveCursorDrawPosition(105,105,&x,&y));
    std::cout << "Production cursor checks passed: minimum pick bounds, item/monster centers, physical input preserved, UI/drag/ground targeting excluded\n";
}
