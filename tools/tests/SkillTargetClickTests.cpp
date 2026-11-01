#include <cassert>
#include <cstdint>
#include <iostream>
using msgparam_t=std::intptr_t;
struct CGameActor {int m_isSitting=0,m_baseAction=0,m_curAction=0;} player;
struct World {CGameActor* m_player=&player;} world;
struct CGameMode {
    World* m_world=&world;
    int m_isOnQuest=0,m_isLeftButtonHeld=0,m_hasHeldMoveTarget=0;
    int m_lastMoveRequestCellX=-1,m_lastMoveRequestCellY=-1,m_lastMoveRequestTick=0,m_lastAttackRequestTick=0;
    int m_lastMonGid=0,m_lastLockOnMonGid=0,m_leftBtnClickTick=0;
    struct {int id=0,level=0;} m_skillUseInfo;
    int Click(msgparam_t wparam,msgparam_t lparam);
};
bool modal=false,shift=false,validTarget=true;
int casts=0,turns=0,moves=0,attacks=0,pickups=0,talks=0,selections=0,skillChaseClears=0;
constexpr int VK_SHIFT=16;
int GetKeyState(int){return shift?0x8000:0;}
int GetTickCount(){return 1000;}
struct {bool HasActiveNpcDialog(){return modal;}} g_windowMgr;
void ClearPickupIntent(CGameMode&){}
void ClearAttackChaseHint(CGameMode&){}
void ClearSkillChase(CGameMode&){++skillChaseClears;}
void ClearPendingSkillUse(CGameMode& mode){mode.m_skillUseInfo={};}
bool TryRequestPendingSkillFromScreenPoint(CGameMode& mode,int,int){
    ++casts;
    if(!validTarget)return false;
    ClearPendingSkillUse(mode);return true;
}
void TryRequestChangeDirFromScreenPoint(CGameMode&,int,int){++turns;}
bool TryRequestGroundItemFromScreenPoint(CGameMode&,int,int){++pickups;return false;}
bool TryRequestAttackFromScreenPoint(CGameMode&,int,int){++attacks;return false;}
bool TryRequestNpcTalkFromScreenPoint(CGameMode&,int,int){++talks;return false;}
bool TrySelectPcTargetFromScreenPoint(CGameMode&,int,int){++selections;return false;}
void UpdateHeldMoveTargetFromScreenPoint(CGameMode&,int,int){}
void PumpHeldMoveRequest(CGameMode&){++moves;}
#include "SkillTargetClickBodies.inc"
int main(){
    // Actor-target and ground-target shortcuts must both work with Shift held.
    for(int skill:{28,89})for(bool modifier:{false,true}){
        CGameMode mode;mode.m_skillUseInfo={skill,1};
        shift=modifier;validTarget=true;
        casts=turns=moves=attacks=pickups=talks=selections=skillChaseClears=0;
        assert(mode.Click(100,200)==1);
        assert(casts==1&&turns==0&&moves==0&&attacks==0&&pickups==0&&skillChaseClears==0);
        assert(mode.m_skillUseInfo.id==0&&!mode.m_isLeftButtonHeld);
    }
    // Invalid skill targets retain targeting; they must not turn or walk.
    CGameMode mode;mode.m_skillUseInfo={28,1};shift=true;validTarget=false;
    casts=turns=moves=attacks=0;
    assert(mode.Click(100,200)==1&&casts==1&&turns==0&&moves==0&&attacks==0);
    assert(mode.m_skillUseInfo.id==28);
    // Ordinary Shift-click and seated turning retain their existing behavior.
    mode.m_skillUseInfo={};turns=casts=0;
    assert(mode.Click(100,200)==1&&turns==1&&casts==0);
    shift=false;player.m_isSitting=1;turns=0;
    assert(mode.Click(100,200)==1&&turns==1);
    player.m_isSitting=0;moves=turns=0;
    assert(mode.Click(100,200)==1&&moves==1&&turns==0);
    // NPC modal handling still takes precedence over world actions.
    modal=true;mode.m_skillUseInfo={28,1};casts=turns=moves=0;
    assert(mode.Click(100,200)==1&&casts==0&&turns==0&&moves==0);
    std::cout<<"Production click dispatch: Shift skill targeting, invalid targets, ordinary turning/movement and NPC gating passed\n";
}
