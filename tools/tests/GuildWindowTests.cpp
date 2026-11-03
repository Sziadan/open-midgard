#include "core/GuildState.h"
#include <cassert>
#include <cstring>
#include <iostream>
using namespace ro::guild;
std::vector<unsigned char> Packet(int size){return std::vector<unsigned char>(size);}
void Put(std::vector<unsigned char>& p,int at,std::uint32_t value){for(int i=0;i<4;++i)p[at+i]=static_cast<unsigned char>(value>>(8*i));}
void Str(std::vector<unsigned char>& p,int at,const char* text){std::memcpy(p.data()+at,text,std::strlen(text));}
int main(){
    WindowState state;
    auto basic=Packet(114);Put(basic,2,77);Put(basic,6,12);Put(basic,10,8);Put(basic,14,36);Put(basic,18,94);
    Put(basic,22,0xf0000000);Put(basic,26,0xff000000);Put(basic,42,3);Str(basic,46,"Guild With Spaces");Str(basic,70,"Leader");Str(basic,94,"No castles");
    assert(!ReadWindowPacket(state,0x1b6,basic.data(),113)&&!state.loaded);
    assert(ReadWindowPacket(state,0x1b6,basic.data(),114)&&state.id==77&&state.averageLevel==94&&state.exp==0xf0000000&&state.name=="Guild With Spaces"&&state.master=="Leader");
    assert(ReadWindowPacket(state,0x150,basic.data(),110));
    auto names=Packet(32);Put(names,4,2);Str(names,8,"Officer");assert(ReadWindowPacket(state,0x166,names.data(),names.size()));
    auto positions=Packet(20);Put(positions,4,2);Put(positions,8,17);Put(positions,16,25);
    assert(ReadWindowPacket(state,0x160,positions.data(),positions.size())&&state.positions[2].name=="Officer"&&state.positions[2].tax==25&&state.positions[2].mode==17);
    auto changed=Packet(44);Put(changed,4,2);Put(changed,8,1);Put(changed,16,30);Str(changed,20,"Recruiter");
    assert(ReadWindowPacket(state,0x174,changed.data(),changed.size())&&state.positions[2].name=="Recruiter"&&state.positions[2].tax==30);
    auto members=Packet(108);Put(members,4,100);Put(members,8,200);members[18]=8;members[20]=99;Put(members,22,123456);Put(members,26,1);Put(members,30,2);Str(members,84,"Member Name");
    assert(ReadWindowPacket(state,0x154,members.data(),members.size())&&state.members.size()==1);
    auto m=state.members[0];assert(m.account==100&&m.character==200&&m.job==8&&m.level==99&&m.exp==123456&&m.online&&m.position==2&&m.name=="Member Name");
    assert(!ReadWindowPacket(state,0x154,members.data(),107)&&state.members.size()==1);
    auto online=Packet(14);Put(online,2,100);Put(online,6,200);assert(ReadWindowPacket(state,0x16d,online.data(),14)&&!state.members[0].online);
    auto rank=Packet(16);Put(rank,4,100);Put(rank,8,200);Put(rank,12,3);assert(ReadWindowPacket(state,0x156,rank.data(),16)&&state.members[0].position==3);
    auto notice=Packet(182);Str(notice,2,"Subject");Str(notice,62,"Notice text");assert(ReadWindowPacket(state,0x16f,notice.data(),182)&&state.notice=="Notice text");
    auto relations=Packet(36);Put(relations,4,1);Put(relations,8,88);Str(relations,12,"Rival Guild");assert(ReadWindowPacket(state,0x14c,relations.data(),36)&&state.relations[0].hostile&&state.relations[0].id==88);
    auto bans=Packet(92);Str(bans,4,"Former Member");Str(bans,52,"Reason");assert(ReadWindowPacket(state,0x163,bans.data(),92)&&state.bans[0].reason=="Reason");
    auto added=Packet(106);std::copy(members.begin()+4,members.end(),added.begin()+2);Put(added,2,101);Put(added,6,201);Str(added,82,"New Member");
    assert(ReadWindowPacket(state,0x182,added.data(),106)&&state.members.size()==2);
    assert(ReadWindowPacket(state,0x176,added.data(),106)&&state.members.size()==2);
    assert(!ReadWindowPacket(state,0x182,added.data(),105));
    auto relationAdded=Packet(34);Put(relationAdded,2,0);Put(relationAdded,6,90);Str(relationAdded,10,"Allied Guild");
    assert(ReadWindowPacket(state,0x185,relationAdded.data(),34)&&state.relations.size()==2);
    assert(ReadWindowPacket(state,0x185,relationAdded.data(),34)&&state.relations.size()==2);
    auto relationRemoved=Packet(10);Put(relationRemoved,2,90);assert(ReadWindowPacket(state,0x184,relationRemoved.data(),10)&&state.relations.size()==1);
    assert(!ReadWindowPacket(state,0x184,relationRemoved.data(),9));
    for(unsigned short op:{0x154,0x166,0x160,0x174,0x14c,0x163,0x156}){
        assert(!ReadWindowPacket(state,op,members.data(),3));assert(!ReadWindowPacket(state,op,nullptr,1000));
    }
    assert(ReadWindowPacket(state,0x154,members.data(),4)&&state.members.empty());
    std::cout<<"PASS: Ref/rAthena guild packet layouts, large EXP, member status/rank updates, positions, notices, diplomacy, bans and malformed-array rejection\n";
}
