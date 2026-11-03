"""Replay Hallucination status lifecycle and server-supplied damage displays."""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
def function(file, signature):
    source=(root/file).read_text(encoding="utf-8")
    start=source.index(signature); opening=source.index("{",start); depth=1; end=opening+1
    while depth:
        depth+=(source[end]=="{")-(source[end]=="}");end+=1
    return source[start:end]
session="\n".join(function("src/session/Session.cpp",s) for s in ["void CSession::ClearActiveStatusIcons(","void CSession::SetActiveStatusIcon(","void CSession::PruneExpiredStatusIcons(","bool CSession::IsStatusIconActive("])
packet="\n".join(function("src/gamemode/GameModePacket.cpp",s) for s in ["void ApplySelfStatusIconPacket(","void HandlePacket0196(","void HandlePacket043F("])
damage="\n".join(function("src/world/GameActor.cpp",s) for s in ["int ClampDamageNumber(","void MakeDamageNumber("])
normal_decode=function("src/gamemode/GameModePacket.cpp","void HandleActorActionNotify(").split("    CGameActor* actor =")[0]
normal_decode=normal_decode.replace("void HandleActorActionNotify(","int DecodeNormalDamage(").replace("return;","return -1;")+"return actionBlock[0];\n}\n"
skill_decode=function("src/gamemode/GameModePacket.cpp","void HandleSkillDamageNotify(").split("    SkillUseArgs args{}")[0]
skill_decode=skill_decode.replace("void HandleSkillDamageNotify(","int DecodeSkillDamage(").replace("return;","return -1;")+"return damage;\n}\n"
build=root/"build-mingw-preset/hallucination-check";build.mkdir(parents=True,exist_ok=True)
cpp=build/"check.cpp"
cpp.write_text(r'''
#include "render3d/ScreenEffects.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>
using u32=uint32_t;using u16=uint16_t;using u8=uint8_t;
struct ACTIVE_STATUS_ICON { int statusType=0; bool hasTimer=false; u32 expireServerTime=0; };
struct CSession {
    std::vector<ACTIVE_STATUS_ICON> m_activeStatusIcons;
    u32 m_gid=1,m_aid=2,now=100;
    u32 GetServerTime() const {return now;}
    void ClearActiveStatusIcons();void SetActiveStatusIcon(int,bool,u32);void PruneExpiredStatusIcons(u32);
    bool IsStatusIconActive(int) const;
} g_session;
'''+session+r'''
struct CGameMode { void* m_world=reinterpret_cast<void*>(1); } mode;
struct PacketView { const unsigned char* data; int packetLength; int packetId=0; };
u32 ReadLE32(const unsigned char* p) {return u32(p[0])|u32(p[1])<<8|u32(p[2])<<16|u32(p[3])<<24;}
unsigned ReadLE16(const unsigned char* p) {return p[0]|unsigned(p[1])<<8;}
void ApplyActorStatusVisual(CGameMode&,int,u32,int) {}
void DbgLog(const char*,...) {}
namespace ro::net {
bool IsActiveActorActionNotifyExtendedPacket(int id) {return id==0x2e1;}
bool IsActiveSkillDamageNotifyExtendedPacket(int id) {return id==0x1de;}
}
bool SessionIsAttack(int) {return true;}
'''+packet+normal_decode+skill_decode+r'''
struct CGameActor { unsigned hp=1000;void DeleteTotalNumber(int) {} } target,attacker;
int displayed=0;
void SpawnDamageNumberEffect(CGameActor&,CGameActor*,int value,u32,int) {displayed=value;}
'''+damage+r'''
int main() {
    unsigned char packet[25]={0x96,1,34,0,1,0,0,0,1};
    HandlePacket0196(mode,{packet,9});assert(g_session.IsStatusIconActive(34));
    assert(ResolveScreenEffectFlags(false,true,true,0,g_session.IsStatusIconActive(34))==2);
    assert(ResolveScreenEffectFlags(false,false,true,0,true)==0);
    assert(ResolveScreenEffectFlags(false,true,false,0,true)==0);
    assert(ResolveScreenEffectFlags(false,true,true,8,false)==0);
    assert(ResolveScreenEffectFlags(true,true,true,16,true)==7);
    packet[8]=0;HandlePacket0196(mode,{packet,9});assert(!g_session.IsStatusIconActive(34));
    packet[8]=1;packet[4]=99;HandlePacket0196(mode,{packet,9});assert(!g_session.IsStatusIconActive(34));
    packet[4]=2;packet[9]=50;HandlePacket043F(mode,{packet,25});assert(g_session.IsStatusIconActive(34));
    g_session.now=150;assert(!g_session.IsStatusIconActive(34));
    g_session.SetActiveStatusIcon(34,true,0);g_session.ClearActiveStatusIcons();assert(!g_session.IsStatusIconActive(34));
    // Ref/eAthena_src_2011/map/{status,clif}.c: the SERVER has already
    // replaced the damage with damage*(5+level)+rand()%100 before sending.
    const int fake=123*(5+3)+42;
    unsigned char hit[33]{};
    for (bool wide : {false,true}) {
        const int value=wide?60042:fake;
        for (int offset : {22,24}) {
            std::fill(std::begin(hit),std::end(hit),0);
            for (int byte=0;byte<(wide?4:2);++byte) hit[offset+byte]=(value>>(8*byte))&255;
            const int decoded=offset==22 ? DecodeNormalDamage(mode,{hit,33,wide?0x2e1:0x8a})
                : DecodeSkillDamage(mode,{hit,33,wide?0x1de:0x114});
            assert(decoded==value);
            MakeDamageNumber(target,&attacker,decoded,0xFFFFFFFFu,14);
            assert(displayed==value && target.hp==1000);
        }
    }
    for (bool active : {false,true}) {
        g_session.SetActiveStatusIcon(34,active,0);
        for (int kind : {0,14,16,21,22}) {
            MakeDamageNumber(target,&attacker,fake,0xFFFFFFFFu,kind);
            assert(displayed==fake && target.hp==1000);
        }
    }
    std::cout<<"PASS: Hallucination packets, local-player identity, clearing/expiry, wave option, Confusion separation, server fake damage preserved without changing HP\n";
}
''')
compiler=Path("C:/Qt/Tools/mingw1310_64/bin/g++.exe");exe=build/"check.exe"
subprocess.run([str(compiler),"-std=c++17","-O3",str(cpp),"-I",str(root/"src"),"-o",str(exe)],check=True)
env=os.environ.copy();env["PATH"]=str(compiler.parent)+os.pathsep+env.get("PATH","")
subprocess.run([str(exe)],check=True,env=env)
