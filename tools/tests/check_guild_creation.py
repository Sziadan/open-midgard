"""Compile the production /guild route and result handler with a captured socket."""
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
mode_source = (root / 'src/gamemode/GameMode.cpp').read_text(encoding='utf-8')
packet_source = (root / 'src/gamemode/GameModePacket.cpp').read_text(encoding='utf-8')
start = mode_source.index('        std::string guildName;', mode_source.index('case GameMsg_SubmitChat:'))
end = mode_source.index('        std::string command = text;', start)
route = mode_source[start:end]
assert end < mode_source.index('const bool sent = SendGlobalChatMessage', end)
start = packet_source.index('void HandleGuildCreateAck(')
end = packet_source.index('void HandlePartyCreateAck(', start)
ack = packet_source[start:end]
assert 'router.Register(0x0167, HandleGuildCreateAck)' in packet_source
build = root / 'build-mingw-preset/guild-creation-check'
build.mkdir(exist_ok=True)
cpp = build / 'check.cpp'
cpp.write_text(r'''
#include "gamemode/GuildCreation.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <map>
#include <vector>
using u8=std::uint8_t;using u32=std::uint32_t;
struct {u32 m_gid=0x12345678;} g_session;
struct {std::vector<std::string> messages;
    void PushChatEvent(const char* text,u32,int){messages.emplace_back(text);}
} g_windowMgr;
void DbgLog(const char*,...){}
struct CRagConnection {
    bool accept=true;std::vector<u8> last;
    static CRagConnection* instance(){static CRagConnection socket;return &socket;}
    bool SendPacket(const char* data,int len){if(!accept)return false;last.assign(data,data+len);return true;}
};
struct World {void* m_player=this;} world;
struct CGameMode {World* m_world=&world;std::map<u32,int> m_actorNameListByGID,m_actorNameByGIDReqTimer;};
struct PacketView {int packetId=0,packetLength=0;const u8* data=nullptr;};
constexpr u8 kChatChannelSystem=6;
void RecordChat(CGameMode&,const std::string& text,u32,u8 channel){assert(channel==6);g_windowMgr.messages.push_back(text);}
''' + ack + '\nstruct Submitter:CGameMode { int Submit(const char* text) {\n' + route + r'''
    return -1;
}};
int main(){
    using ro::guild::CommandResult;
    std::string name;
    assert(ro::guild::ParseCreateCommand("/guild \"Mixed Case Guild\"",name)==CommandResult::Valid && name=="Mixed Case Guild");
    assert(ro::guild::ParseCreateCommand("  /GUILD SingleName  ",name)==CommandResult::Valid && name=="SingleName");
    for(const auto& text:std::vector<std::string>{"/guild","/guild \"\"","/guild \"   \"","/guild \"unfinished","/guild \"A\" extra","/guild Two Words","/guild "+std::string(24,'a'),std::string("/guild \"a\0b\"",12)})
        assert(ro::guild::ParseCreateCommand(text,name)==CommandResult::Invalid);
    assert(ro::guild::ParseCreateCommand("/guild "+std::string(23,'a'),name)==CommandResult::Valid);
    assert(ro::guild::ParseCreateCommand(u8"/guild \"åäö 日本語\"",name)==CommandResult::Valid && name==u8"åäö 日本語");
    assert(ro::guild::ParseCreateCommand("/guildchat hello",name)==CommandResult::NotCommand);
    Submitter mode;auto* socket=CRagConnection::instance();
    assert(mode.Submit("/guild \"Mixed Case Guild\"")==1);
    assert(socket->last.size()==30 && socket->last[0]==0x65 && socket->last[1]==1);
    assert(socket->last[2]==0x78 && socket->last[3]==0x56 && socket->last[4]==0x34 && socket->last[5]==0x12);
    assert(std::strcmp(reinterpret_cast<const char*>(socket->last.data()+6),"Mixed Case Guild")==0 && socket->last[29]==0);
    socket->last.clear();assert(mode.Submit("/guild \"bad\" extra")==0 && socket->last.empty());
    assert(mode.Submit("ordinary chat")==-1 && socket->last.empty());
    socket->accept=false;assert(mode.Submit("/guild Valid")==0 && socket->last.empty());socket->accept=true;
    for(u8 result:{0,1,2,3,99}){
        mode.m_actorNameListByGID[g_session.m_gid]=1;mode.m_actorNameByGIDReqTimer[g_session.m_gid]=1;
        const u8 reply[]={0x67,1,result};const auto old=g_windowMgr.messages.size();
        HandleGuildCreateAck(mode,{0x167,2,reply});assert(g_windowMgr.messages.size()==old);
        HandleGuildCreateAck(mode,{0x167,3,reply});
        assert(g_windowMgr.messages.back()==ro::guild::CreateResultMessage(result));
        assert(mode.m_actorNameListByGID.count(g_session.m_gid)==(result==0?0:1));
    }
    HandleGuildCreateAck(mode,{0x167,3,nullptr});
    std::cout<<"PASS: /guild quoting/limits/case, exact 30-byte request, no public-chat fallthrough, send failure, all creation replies and self-hover refresh\n";
}
''', encoding='utf-8')
compiler = Path('C:/Qt/Tools/mingw1310_64/bin/g++.exe')
executable = build / 'check.exe'
subprocess.run([str(compiler), '-std=c++17', '-O2', '-I'+str(root/'src'), str(cpp), '-o', str(executable)], check=True)
environment = os.environ.copy()
environment['PATH'] = str(compiler.parent)+os.pathsep+environment.get('PATH', '')
subprocess.run([str(executable)], env=environment, check=True)
