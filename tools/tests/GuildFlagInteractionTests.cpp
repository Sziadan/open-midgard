#include "network/Packet.h"
#include "core/GuildFlagTexture.h"
#include <vector>
#include <iostream>
struct CGameActor {int m_isPc=0,m_job=722,m_objectType=6;u32 m_gid=110007412;};
struct TestView {matrix view{};const matrix& GetViewMatrix(){return view;}float GetCameraLongitude(){return 0;}};
struct TestWorld {
    CGameActor* target=nullptr;
    bool FindHoveredActorScreen(const matrix&,float,int,int,CGameActor** result,int*,int*){*result=target;return target!=nullptr;}
};
struct CGameMode {
    TestWorld* m_world;TestView* m_view;bool m_canRotateView=false;
    int m_lastMonGid=1,m_lastLockOnMonGid=1,m_isAutoMoveClickOn=1,m_isLeftButtonHeld=1,m_hasHeldMoveTarget=1;
    int m_lastMoveRequestCellX=1,m_lastMoveRequestCellY=1,m_heldMoveTargetCellX=1,m_heldMoveTargetCellY=1;
    int m_lastMoveRequestTick=1,m_lastAttackRequestTick=1;
};
struct CRagConnection {
    std::vector<std::vector<u8>> packets;bool success=true;
    static CRagConnection* instance(){static CRagConnection socket;return &socket;}
    bool SendPacket(const char* data,int size){if(success)packets.emplace_back(data,data+size);return success;}
};
void DbgLog(const char*,...){}
void ClearPickupIntent(CGameMode&){}void ClearAttackChaseHint(CGameMode&){}void ClearSkillChase(CGameMode&){}
#include "GuildFlagInteraction.inc"
int main(){
    std::array<std::uint32_t,24*24> pixels;pixels.fill(0xFF123456u);
    pixels[0]=0xFFFF00FFu;pixels[1]=0xFFFE02FDu;
    const auto canvas=ro::guild::FlagEmblemCanvas(pixels.data());
    assert(canvas.size()==32*32 && canvas[0]==0 && canvas[1]==0);
    int opaque=0;
    for(int y=0;y<32;++y)for(int x=0;x<32;++x){
        const auto color=canvas[y*32+x];
        if(x>=24 || y>=24)assert(color==0);
        else if(y || x>1){assert(color==0xFF123456u);++opaque;}
    }
    assert(opaque==574); // 24/32 of each UV axis, never stretched to fill it.
    CGameActor flag;TestView view;TestWorld world{&flag};CGameMode mode{&world,&view};
    auto* socket=CRagConnection::instance();
    assert(TryRequestNpcTalkFromScreenPoint(mode,400,300));
    const auto& p=socket->packets.back();assert(p.size()==7 && p[0]==0x90 && p[1]==0 && p[6]==1);
    u32 id=0;std::memcpy(&id,p.data()+2,4);assert(id==110007412);
    assert(mode.m_isLeftButtonHeld==0 && mode.m_lastMonGid==0 && mode.m_heldMoveTargetCellX==-1);
    flag.m_job=111;assert(TryRequestNpcTalkFromScreenPoint(mode,400,300));
    assert(socket->packets.size()==2 && socket->packets.back()[0]==0x90);
    flag.m_job=1286;flag.m_objectType=5;assert(!TryRequestNpcTalkFromScreenPoint(mode,400,300));
    flag.m_job=722;flag.m_objectType=6;mode.m_canRotateView=true;assert(!TryRequestNpcTalkFromScreenPoint(mode,400,300));
    mode.m_canRotateView=false;socket->success=false;assert(!TryRequestNpcTalkFromScreenPoint(mode,400,300));
    assert(socket->packets.size()==2);
    world.target=nullptr;assert(!TryRequestNpcTalkFromScreenPoint(mode,400,300));
    std::cout<<"PASS: production flag click routes to 0090 NPC contact with the flag actor ID; guardian, camera and failed-send guards hold\n";
}
