#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
using u8=std::uint8_t;using u16=std::uint16_t;using u32=std::uint32_t;
constexpr int kAttackStateId=2,kSecondAttackStateId=9,kDeathStateId=3,kMoveStateId=1;
constexpr int kGameActorCastingStateId=8,kGameActorCastingLoopStateId=13;
constexpr float kDefaultMotionDelay=1;
u32 now=100;
u32 timeGetTime(){return now;}
struct CActRes {int GetMotionCount(int){return 3;}float GetDelay(int){return 1;}} resource;
int ResolveAvailableActionIndex(CActRes*,int action){return action;}
struct UIRechargeGage {
    int m_w=60,m_h=6,amount=0,total=0;
    void Move(int,int){}
    void SetAmount(int value,int maximum){amount=value;total=maximum;}
} gage;
struct CGameActor {
    int m_stateId=8,m_isMoving=0,m_isMotionFreezed=0,stateChanges=0;
    u32 m_stateStartTick=100,m_vanishTime=0,m_skillRechargeStartTick=100,m_skillRechargeEndTick=10100;
    float m_attackMotion=-1,m_motionSpeed=1;
    UIRechargeGage* m_skillRechargeGage=&gage;
    void SetState(int state){m_stateId=state;++stateChanges;}
    void SendMsg(CGameActor*,int message,int,int,int){if(message==83)m_skillRechargeGage=nullptr;}
    void ProcessSkillRechargeGageOverlay(int,int);
} actor;
struct World {CGameActor* m_player=&actor;} world;
struct CGameMode {World* m_world=&world;} mode;
struct PacketView {const u8* data;int packetLength;};
u32 ReadLE32(const u8* p){return u32(p[0])|(u32(p[1])<<8)|(u32(p[2])<<16)|(u32(p[3])<<24);}
CGameActor* ResolveSkillPacketActor(CGameMode&,u32){return &actor;}
#include "CastingPoseBodies.inc"
int main(){
    // A three-frame clip must not drop a ten-second cast to idle.
    for(u32 tick:{200u,2000u,9000u,10101u,11000u}){
        now=tick;
        assert(IsTransientActionActive(actor,&resource,96));
        actor.ProcessSkillRechargeGageOverlay(200,100);
        assert(actor.m_stateId==8&&actor.stateChanges==0);
    }
    assert(actor.m_skillRechargeGage==nullptr); // bar estimate ended, pose did not
    // A successful use changes state: attack/use poses retain their clip timeout.
    actor.SetState(7);
    assert(!IsTransientActionActive(actor,&resource,96));
    actor.SetState(0);
    assert(!IsTransientActionActive(actor,&resource,0));
    // Server interruption ends both ordinary and attack-style casting states.
    u8 cancel[]={0xb9,1,1,0,0,0};
    for(int state:{8,13}){
        actor.m_stateId=state;actor.m_isMotionFreezed=1;actor.m_skillRechargeGage=&gage;
        HandleSkillCastCancel(mode,{cancel,6});
        assert(actor.m_stateId==0&&actor.m_isMotionFreezed==0&&actor.m_skillRechargeGage==nullptr);
    }
    // A late cancellation must not replace a subsequent attack/hit/death state.
    for(int state:{2,3,4,7}){
        actor.m_stateId=state;
        HandleSkillCastCancel(mode,{cancel,6});
        assert(actor.m_stateId==state);
    }
    std::cout<<"Production casting pose checks passed: long casts, bar expiry, completion states and interruptions\n";
}
