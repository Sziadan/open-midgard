// Source-selected room receipts. Included inside GameModePacket.cpp's namespace.
std::string RoomMessage(int id,const char* fallback)
{
    ro::UiMessageStrings table;table.Load();return table.Get(id,fallback);
}
std::string RoomMemberMessage(int id,const char* fallback,const std::string& name)
{
    auto text=RoomMessage(id,fallback);const auto marker=text.find("%s");
    if(marker!=std::string::npos)text.replace(marker,2,name);return text;
}
void RoomMainNotice(int id,const char* fallback,u32 colorRef)
{
    g_windowMgr.SendMsg(3,0,0);const auto text=RoomMessage(id,fallback);
    g_windowMgr.SendMsg(1,reinterpret_cast<msgparam_t>(text.c_str()),colorRef);
}
CGameActor* ResolveRoomActor(CGameMode& mode,u32 owner)
{
    // Keep the port's local AID/GID adaptation, but a room bubble requires
    // an actual world, matching the original room-handler context.
    return mode.m_world?ResolveChatSpeakerActor(mode,owner):nullptr;
}
UIChatRoomWnd* EnsureRoomWindow()
{
    return static_cast<UIChatRoomWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_CHATROOMWND));
}
void HandleRoomCreateAck(CGameMode& mode,const PacketView& packet)
{
    if(!packet.data || packet.packetLength!=3)return;
    mode.m_isWaitingEnterRoom=0;
    switch(packet.data[2]){
    case 0: {
        RoomMainNotice(64,"The room has been created.",16776960);
        mode.m_lastChatroomInfo.numPeople=1;auto* room=EnsureRoomWindow();
        room->SetInfo(mode.m_lastChatroomInfo);room->ClearMembers();room->AddMember(g_session.GetPlayerName(),0);return;
    }
    case 1:RoomMainNotice(65,"Too many rooms.",255);return;
    case 2:RoomMainNotice(66,"A room with that title already exists.",255);return;
    default:return;
    }
}
void HandleRoomListing(CGameMode& mode,const PacketView& packet)
{
    ro::chat::RoomListing listing;
    if(!ro::chat::ParseRoomListing(packet.data,packet.packetLength,listing))return;
    mode.m_chatRoomOwners[listing.info.roomId]=listing.owner;
    if(packet.packetId==0x00df){EnsureRoomWindow()->SetInfo(listing.info);return;}
    if(auto* actor=ResolveRoomActor(mode,listing.owner))
        actor->SendMsg(nullptr,32,reinterpret_cast<msgparam_t>(&listing.info),0,0);
}
void HandleRoomEntry(CGameMode& mode,const PacketView& packet)
{
    ro::chat::RoomEntry entry;
    if(!ro::chat::ParseRoomEntry(packet.data,packet.packetLength,entry))return;
    mode.m_isWaitingEnterRoom=0;
    // Source insert preserves a previous D7 room-to-owner mapping. It is not
    // the port's unrelated actor timestamp cache.
    const auto owner=mode.m_chatRoomOwners.emplace(entry.roomId,0).first->second;
    auto* actor=ResolveRoomActor(mode,owner);if(!actor)return;
    const bool matches=actor->m_chatTitle && actor->m_chatTitle->m_roomInfo.roomId==entry.roomId;
    ro::chat::RoomInfo info;if(matches)info=actor->m_chatTitle->m_roomInfo;
    actor->SendMsg(nullptr,33,0,0,0);
    if(!matches)return;
    info.numPeople=static_cast<int>(entry.members.size());auto* room=EnsureRoomWindow();room->ClearMembers();room->SetInfo(info);
    for(const auto& member:entry.members)room->AddMember(member.name,member.role);
    mode.m_lastChatroomInfo=info;
}
void HandleRoomDestroy(CGameMode& mode,const PacketView& packet)
{
    if(!packet.data || packet.packetLength!=6)return;const auto roomId=ReadLE32(packet.data+2);
    const auto owner=mode.m_chatRoomOwners.emplace(roomId,0).first->second;
    if(auto* actor=ResolveRoomActor(mode,owner))actor->SendMsg(nullptr,33,0,0,0);
    if(g_windowMgr.m_chatRoomWnd && g_windowMgr.m_chatRoomWnd->GetInfo().roomId==roomId){
        g_windowMgr.DeleteWindow(g_windowMgr.m_chatRoomWnd);
        if(g_windowMgr.m_chatRoomChangeWnd)g_windowMgr.DeleteWindow(g_windowMgr.m_chatRoomChangeWnd);
    }
    // Original keeps the mapping and last room metadata; map/session reset owns their lifetime.
}
void HandleRoomMemberEnter(CGameMode&,const PacketView& packet)
{
    if(!packet.data || packet.packetLength!=28)return;
    const auto name=ro::chat::detail::ReadBoundedRoomName(packet.data+4,24);auto* room=EnsureRoomWindow();
    auto info=room->GetInfo();info.numPeople=ro::chat::detail::ReadRoomI16(packet.data+2);room->SetInfo(info);room->AddMember(name,1);
    const auto text=RoomMemberMessage(179,"(%s) entered the room.",name);
    g_windowMgr.SendMsg(5,reinterpret_cast<msgparam_t>(text.c_str()),8421376);
}
void HandleRoomMemberExit(CGameMode&,const PacketView& packet)
{
    if(!packet.data || packet.packetLength!=29)return;
    const auto name=ro::chat::detail::ReadBoundedRoomName(packet.data+4,24);
    if(name==g_session.GetPlayerName()){
        if(g_windowMgr.m_chatRoomWnd)g_windowMgr.DeleteWindow(g_windowMgr.m_chatRoomWnd);
        if(g_windowMgr.m_chatRoomChangeWnd)g_windowMgr.DeleteWindow(g_windowMgr.m_chatRoomChangeWnd);
        return;
    }
    auto* room=EnsureRoomWindow();auto info=room->GetInfo();info.numPeople=ro::chat::detail::ReadRoomI16(packet.data+2);room->SetInfo(info);room->RemoveMember(name);
    if(packet.data[28]>1)return;
    const auto text=packet.data[28]==0?RoomMemberMessage(180,"(%s) left the room.",name)
        :RoomMemberMessage(181,"(%s) was removed from the room.",name);
    g_windowMgr.SendMsg(5,reinterpret_cast<msgparam_t>(text.c_str()),128);
}
void HandleRoomRole(CGameMode&,const PacketView& packet)
{
    if(!packet.data || packet.packetLength!=30)return;
    const auto name=ro::chat::detail::ReadBoundedRoomName(packet.data+6,24);
    EnsureRoomWindow()->ChangeRole(name,ReadLE32(packet.data+2));
}
void HandleRoomEnterRefused(CGameMode& mode,const PacketView& packet)
{
    if(!packet.data || packet.packetLength!=3)return;mode.m_isWaitingEnterRoom=0;
    switch(packet.data[2]){
    case 0:RoomMainNotice(67,"The room is full.",255);return;
    case 1:RoomMainNotice(7,"Incorrect password.",255);return;
    case 2:RoomMainNotice(68,"You have been banned from the room.",255);return;
    case 4:RoomMainNotice(55,"Insufficient money.",255);return;
    case 5:RoomMainNotice(432,"Your level is too low.",255);return;
    case 6:RoomMainNotice(433,"Your level is too high.",255);return;
    case 7:RoomMainNotice(434,"Your job is not accepted.",255);return;
    default:return;
    }
}
