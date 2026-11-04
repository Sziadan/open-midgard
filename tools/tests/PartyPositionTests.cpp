#include "session/Session.h"

#include <cassert>
#include <iostream>
#include <list>

int main()
{
    FRIEND_INFO first{};
    first.AID=101;first.GID=201;first.characterName="Party Member";first.mapName="prontera.gat";
    FRIEND_INFO second{};
    second.AID=202;second.GID=101;second.characterName="Other Member";second.mapName="prontera.gat";
    std::list<FRIEND_INFO> members{first,second};

    assert(ApplyPartyMapPosition(members,101,100,200));
    assert(members.front().partyPositionValid&&members.front().partyX==100&&members.front().partyY==200);
    assert(!members.back().partyPositionValid); // AID lookup must not match another member's GID.
    assert(!ApplyPartyMapPosition(members,999,20,30));
    assert(!ApplyPartyMapPosition(members,101,-1,30));
    assert(members.front().partyPositionValid&&members.front().partyX==100&&members.front().partyY==200);
    assert(!ApplyPartyMapPosition(members,101,0x8000,1));

    FRIEND_INFO refresh=members.front();
    refresh.partyPositionValid=false;refresh.partyX=0;refresh.partyY=0;
    PreservePartyMapPosition(members.front(),refresh);
    assert(refresh.partyPositionValid&&refresh.partyX==100&&refresh.partyY==200);

    FRIEND_INFO remoteMap=refresh;
    remoteMap.mapName="geffen.gat";remoteMap.partyPositionValid=false;
    PreservePartyMapPosition(refresh,remoteMap);
    assert(!remoteMap.partyPositionValid);

    assert(ApplyPartyMapPosition(members,101,-1,-1));
    assert(!members.front().partyPositionValid&&members.front().partyX==0&&members.front().partyY==0);
    assert(!ApplyPartyMapPosition(members,101,-1,200));

    std::cout<<"PASS: party position AID lookup, signed cell bounds, atomic sentinel, same-map roster preservation, remote-map invalidation\n";
    return 0;
}
