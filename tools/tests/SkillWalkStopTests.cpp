#include <cassert>
#include <cstdint>
#include <iostream>
#include <map>
#include <vector>
using u32=std::uint32_t;
using s16=std::int16_t;
struct vector3d { float x=0,y=0,z=0; };
struct CellPos { int x=0,y=0; };
struct CPathInfo {
    std::vector<CellPos> m_cells;
    void Reset() { m_cells.clear(); }
};
struct CGameActor {
    int m_isVisible=0,m_moveSrcX=0,m_moveSrcY=0,m_moveDestX=0,m_moveDestY=0,
        m_lastTlvertX=0,m_lastTlvertY=0,m_isMoving=0,m_isWaitingMoveAck=1,state=1;
    u32 m_moveStartTime=0,m_moveEndTime=999;
    vector3d m_pos,m_moveStartPos,m_moveEndPos;
    CPathInfo m_path;
    void UnRegisterPos() {}
    void RegisterPos() {}
    void ReapplyConcealmentVisibility() {}
    void SetState(int value) { state=value; }
};
struct World { CGameActor* m_player=nullptr; } world;
struct CGameMode {
    enum { MapLoading_None=0 };
    World* m_world=&world;
    int m_mapLoadingStage=0,m_attackChaseSourceCellX=0,m_attackChaseSourceCellY=0;
    u32 m_lastActorBootstrapPacketTick=0,m_lastPcGid=0;
    std::map<u32,u32> m_aidList;
    std::map<u32,CellPos> m_actorPosList;
};
struct Session {
    int m_playerPosX=90,m_playerPosY=80,m_playerDir=3;
    u32 GetServerTime() { return 1200; }
    void SetPlayerPosDir(int x,int y,int dir) { m_playerPosX=x;m_playerPosY=y;m_playerDir=dir; }
} g_session;
struct PacketView { const unsigned char* data; int packetLength; unsigned short packetId; };
u32 ReadLE32(const unsigned char* b) { return u32(b[0])|(u32(b[1])<<8)|(u32(b[2])<<16)|(u32(b[3])<<24); }
unsigned short ReadLE16(const unsigned char* b) { return b[0]|(b[1]<<8); }
u32 GetTickCount() { return 1300; }
void LogActorPacketSeenOnce(const PacketView&) {}
CGameActor* EnsureRuntimeActor(CGameMode& mode,u32 gid) { return gid==42 ? mode.m_world->m_player : nullptr; }
bool IsLocalPlayerActor(CGameMode&,u32 gid) { return gid==42; }
float TileToWorldCoordX(World*,int x) { return float(x*5); }
float TileToWorldCoordZ(World*,int y) { return float(y*5); }
float ResolveActorHeight(World*,float,float) { return -7; }
namespace ro::net { bool IsActiveActorSetPositionPacket(unsigned short id) { return id==0x88 || id==0x1ff; } }
#include "SkillWalkStopBodies.inc"
int main() {
    CGameMode mode; CGameActor player; world.m_player=&player;
    // The stop is at a cell on/behind the currently displayed segment, while
    // the acknowledged walk destination is still much farther away.
    player.m_pos={57,-7,100}; player.m_isMoving=1;
    player.m_moveDestX=90; player.m_moveDestY=80;
    player.m_moveEndPos={450,-7,400};
    player.m_path.m_cells={{10,20},{11,20},{12,20},{90,80}};
    unsigned char bytes[]={0x88,0,42,0,0,0,11,0,20,0};
    HandleActorSetPosition(mode,{bytes,10,0x88});
    assert(!player.m_isMoving && player.m_path.m_cells.empty());
    assert(player.m_pos.x==55 && player.m_pos.z==100 && player.m_pos.y==-7);
    assert(player.m_moveDestX==11 && player.m_moveDestY==20);
    assert(player.m_moveEndPos.x==55 && player.m_moveStartPos.x==55);
    assert(player.m_moveEndTime==1200 && player.m_moveStartTime==1200);
    assert(g_session.m_playerPosX==11 && g_session.m_playerPosY==20);
    assert(mode.m_actorPosList[42].x==11 && mode.m_attackChaseSourceCellX==11);
    assert(!player.m_isWaitingMoveAck && player.state==0);
    // Following cast and idle frames retain the confirmed stop cell.
    player.SetState(8);
    assert(player.m_pos.x==55 && player.m_pos.z==100);
    player.SetState(0);
    assert(player.m_pos.x==55 && player.m_pos.z==100);
    // Invalid/truncated packets cannot disturb an existing pose.
    bytes[6]=77;
    HandleActorSetPosition(mode,{bytes,9,0x88});
    HandleActorSetPosition(mode,{bytes,10,0x99});
    assert(player.m_pos.x==55);
    std::cout << "Production stop packet resets the walk to the server cell, including a cell behind the visible segment\n";
}
