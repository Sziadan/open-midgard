// Compile the production picker with deterministic projected sprite bounds.
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>
#include "world/CursorSnap.h"
struct { float m_screenXFactor=1; } g_renderer;
struct matrix {};
struct CGameActor { virtual ~CGameActor()=default; bool m_isVisible=true; };
struct CPc : CGameActor { bool dead=false, monster=false; };
struct Ground { float m_zoom=5; };
struct Attr { int m_zoom=5; };
constexpr int kActorLabelBaseYOffset=4;
constexpr float kEnemyHoverStickyPadding=3;
struct CWorld {
    struct BillboardScreenEntry {
        CPc* actor=nullptr;
        float left=0,right=0,top=0,bottom=0,baseX=0,baseY=0;
    };
    CGameActor* m_player=nullptr;
    Ground* m_ground=nullptr;Attr* m_attr=nullptr;
    std::vector<BillboardScreenEntry> m_billboardFrameEntries;
    void EnsureBillboardFrameCache(const matrix&,float)const {}
    bool FindHoveredActorScreen(const matrix&,float,int,int,CGameActor**,int*,int*,bool=false)const;
};
static bool projectionSucceeds=true;
static float projectedZoom=0;
bool BuildBillboardRenderEntry(CPc* pc,const matrix&,float,float zoom,CWorld::BillboardScreenEntry* entry)
{
    projectedZoom=zoom;
    *entry={pc,90,110,60,100,100,100};
    return projectionSucceeds;
}
bool IsUntargetableDeadHoverActor(CPc* pc){return pc->dead;}
bool IsMonsterLikeHoverActor(CPc* pc){return pc->monster;}
float ComputePointToRectDistanceSq(float x,float y,float left,float top,float right,float bottom)
{
    const float dx=std::max({left-x,0.0f,x-right}),dy=std::max({top-y,0.0f,y-bottom});
    return dx*dx+dy*dy;
}
#include "SkillSelfTargetPicker.inc"
int main()
{
    CWorld world;CPc player,other;matrix view;
    Ground ground;ground.m_zoom=7;world.m_ground=&ground;world.m_player=&player;
    CGameActor* target=nullptr;int labelX=0,labelY=0;
    // A click on the player's sprite must still fall through for movement.
    assert(!world.FindHoveredActorScreen(view,0,100,80,&target,&labelX,&labelY));
    assert(target==nullptr&&labelX==0&&labelY==0);
    // Pending Heal/support targeting includes the actual rendered sprite bounds.
    assert(world.FindHoveredActorScreen(view,0,100,80,&target,&labelX,&labelY,true));
    assert(target==&player&&labelX==100&&labelY==104&&projectedZoom==7);
    assert(!world.FindHoveredActorScreen(view,0,111,80,&target,nullptr,nullptr,true));
    assert(!world.FindHoveredActorScreen(view,0,100,59,&target,nullptr,nullptr,true));
    player.m_isVisible=false;
    assert(!world.FindHoveredActorScreen(view,0,100,80,&target,nullptr,nullptr,true));
    player.m_isVisible=true;player.dead=true;
    assert(!world.FindHoveredActorScreen(view,0,100,80,&target,nullptr,nullptr,true));
    player.dead=false;projectionSucceeds=false;
    assert(!world.FindHoveredActorScreen(view,0,100,80,&target,nullptr,nullptr,true));
    projectionSucceeds=true;
    // Other actors remain targetable outside the self sprite, in both modes.
    world.m_billboardFrameEntries.push_back({&other,150,170,60,100,160,100});
    assert(world.FindHoveredActorScreen(view,0,160,80,&target,nullptr,nullptr,true)&&target==&other);
    assert(world.FindHoveredActorScreen(view,0,160,80,&target,nullptr,nullptr)&&target==&other);
    world.m_player=nullptr;
    assert(!world.FindHoveredActorScreen(view,0,100,80,&target,nullptr,nullptr,true));
    std::cout<<"Production skill self-target picker checks passed\n";
}
