#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include <algorithm>

namespace ro::guild {
struct Member {std::uint32_t account=0,character=0,exp=0,position=0;int job=0,level=0;bool online=false;std::string name;};
struct Position {std::uint32_t id=0,mode=0,tax=0;std::string name;};
struct Relation {std::uint32_t id=0;bool hostile=false;std::string name;};
struct Ban {std::string name,reason;};
struct WindowState {
    std::uint32_t id=0,level=0,online=0,capacity=0,averageLevel=0,exp=0,nextExp=0,emblemVersion=0,menuFlags=0;
    bool loaded=false;
    std::string name,master,territory,noticeTitle,notice;
    std::vector<Member> members;
    std::map<std::uint32_t,Position> positions;
    std::vector<Relation> relations;
    std::vector<Ban> bans;
};
inline std::uint32_t Read(const unsigned char* p){return p[0]|(std::uint32_t(p[1])<<8)|(std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);}
inline unsigned short ReadShort(const unsigned char* p){return p[0]|(p[1]<<8);}
inline std::string Text(const unsigned char* p,std::size_t n){std::size_t i=0;while(i<n && p[i])++i;return {reinterpret_cast<const char*>(p),i};}

// The active pre-Renewal protocol, as laid out in Ref/packet.h and rAthena.
// Reject incomplete record arrays before replacing any previously valid state.
inline bool ReadWindowPacket(WindowState& state,unsigned short op,const unsigned char* p,std::size_t size)
{
    if(!p)return false;
    if(op==0x182 || op==0x176){
        if(size<106)return false;
        const auto* row=p+2;
        Member member{Read(row),Read(row+4),Read(row+18),Read(row+26),ReadShort(row+14),ReadShort(row+16),Read(row+22)!=0,Text(row+80,24)};
        auto existing=std::find_if(state.members.begin(),state.members.end(),[&](const auto& m){return m.account==member.account&&m.character==member.character;});
        if(existing==state.members.end())state.members.push_back(member);else *existing=member;
        state.online=std::count_if(state.members.begin(),state.members.end(),[](const auto& m){return m.online;});return true;
    }
    if(op==0x184){if(size<10)return false;const auto id=Read(p+2);const bool hostile=Read(p+6)!=0;state.relations.erase(std::remove_if(state.relations.begin(),state.relations.end(),[&](const auto& r){return r.id==id&&r.hostile==hostile;}),state.relations.end());return true;}
    if(op==0x185){if(size<34)return false;Relation relation{Read(p+6),Read(p+2)!=0,Text(p+10,24)};auto old=std::find_if(state.relations.begin(),state.relations.end(),[&](const auto& r){return r.id==relation.id&&r.hostile==relation.hostile;});if(old==state.relations.end())state.relations.push_back(relation);else *old=relation;return true;}
    if(op==0x150 || op==0x1b6) {
        if(size<(op==0x150?110u:114u))return false;
        state.id=Read(p+2);state.level=Read(p+6);state.online=Read(p+10);state.capacity=Read(p+14);
        state.averageLevel=Read(p+18);state.exp=Read(p+22);state.nextExp=Read(p+26);state.emblemVersion=Read(p+42);
        state.name=Text(p+46,24);state.master=Text(p+70,24);state.territory=Text(p+94,16);state.loaded=true;return true;
    }
    if(op==0x14e){if(size<6)return false;state.menuFlags=Read(p+2);return true;}
    if(op==0x16f){if(size<182)return false;state.noticeTitle=Text(p+2,60);state.notice=Text(p+62,120);return true;}
    if(op==0x16d){if(size<14)return false;for(auto& m:state.members)if(m.account==Read(p+2)&&m.character==Read(p+6))m.online=Read(p+10)!=0;state.online=std::count_if(state.members.begin(),state.members.end(),[](const auto& m){return m.online;});return true;}
    std::size_t stride=0;
    switch(op){case 0x154:stride=104;break;case 0x166:stride=28;break;case 0x160:stride=16;break;
    case 0x174:stride=40;break;case 0x14c:stride=32;break;case 0x163:stride=88;break;case 0x156:stride=12;break;default:return false;}
    if(size<4 || (size-4)%stride)return false;
    if(op==0x154)state.members.clear();
    if(op==0x14c)state.relations.clear();
    if(op==0x163)state.bans.clear();
    for(std::size_t offset=4;offset<size;offset+=stride) {
        const auto* row=p+offset;
        if(op==0x154)state.members.push_back({Read(row),Read(row+4),Read(row+18),Read(row+26),ReadShort(row+14),ReadShort(row+16),Read(row+22)!=0,Text(row+80,24)});
        else if(op==0x166){auto& pos=state.positions[Read(row)];pos.id=Read(row);pos.name=Text(row+4,24);}
        else if(op==0x160 || op==0x174){auto& pos=state.positions[Read(row)];pos.id=Read(row);pos.mode=Read(row+4);pos.tax=Read(row+12);if(op==0x174)pos.name=Text(row+16,24);}
        else if(op==0x14c)state.relations.push_back({Read(row+4),Read(row)!=0,Text(row+8,24)});
        else if(op==0x163)state.bans.push_back({Text(row,24),Text(row+48,40)});
        else if(op==0x156)for(auto& m:state.members)if(m.account==Read(row)&&m.character==Read(row+4))m.position=Read(row+8);
    }
    return true;
}
}
