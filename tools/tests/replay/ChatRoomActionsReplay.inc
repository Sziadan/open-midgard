#include "core/SettingsIni.h"

void ChatRoomActionsReplay()
{
    SetCase("chat_room_actions","PK09.21","source-menu-e0-e2","selected source menu index -> current member -> real E0/E2 and authoritative role receipts");
    g_windowMgr.RemoveAllWindows();CGameMode mode;TestConnection receive;
    struct Restore {
        CMode* current=SkillPacketReplayAccess::Current();int type=SkillPacketReplayAccess::CurrentType();
        ServerType server=g_serverType;int folder=g_readFolderFirst;
        std::list<FRIEND_INFO> friends=g_session.m_friendList;SubChatWindowInfo geometry=g_windowMgr.m_chatRoomWindowInfo;
        std::array<char,25> name{};std::filesystem::path cwd=std::filesystem::current_path(),resource;
        Restore(){std::copy(std::begin(g_session.m_playerName),std::end(g_session.m_playerName),name.begin());}
        ~Restore(){g_windowMgr.RemoveAllWindows();g_windowMgr.ClearChatEvents();CRagConnection::instance()->Disconnect();SkillPacketReplayAccess::Restore(current,type);
            g_serverType=server;g_readFolderFirst=folder;g_session.m_friendList=std::move(friends);g_windowMgr.m_chatRoomWindowInfo=geometry;
            std::copy(name.begin(),name.end(),std::begin(g_session.m_playerName));std::filesystem::current_path(cwd);
            if(!resource.empty())std::filesystem::remove_all(resource);}
    } restore;
    restore.resource=restore.cwd/("chat-actions-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(restore.resource);std::filesystem::current_path(restore.resource);g_readFolderFirst=1;
    {std::ofstream file("msgstringtable.txt",std::ios::binary);for(int i=0;i<=358;++i)file<<(i==129?"#\r\n":"Menu label#\r\n");}
    SkillPacketReplayAccess::SetCurrent(&mode);g_serverType=ServerNormal;g_session.m_friendList.clear();
    std::memset(g_session.m_playerName,0,sizeof(g_session.m_playerName));std::memcpy(g_session.m_playerName,"Owner",5);
    auto* connection=CRagConnection::instance();connection->Disconnect();LoopbackPeer peer(*connection);
    auto* room=static_cast<UIChatRoomWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_CHATROOMWND));room->SetInfo({"Action room","",1,2,20,77});
    const auto select=[&](int index){const auto bounds=room->GetLayout().members;
        room->OnLBtnDown(room->m_x+bounds.left+2,room->m_y+bounds.top+(index<0?70:index*14+2));};
    const auto ids=[&](){std::vector<int> result;for(const auto& item:room->GetMenuItems())result.push_back(item.action);return result;};
    const auto open=[&](){room->OpenMemberMenu(100,100);return g_windowMgr.m_chatRoomMenuWnd;};
    const auto choose=[&](int index){auto* menu=g_windowMgr.m_chatRoomMenuWnd;Require(menu,"missing source menu owner");menu->OnLBtnUp(menu->m_x+4,menu->m_y+index*14+2);};
    Require(!open(),"missing local role fabricated a menu");room->AddMember("Owner",0);room->AddMember("Peer",1);
    select(-1);Require(open() && ids()==std::vector<int>({2,11}),"owner blank menu order changed");
    select(0);Require(open() && ids()==std::vector<int>({3,2}),"owner self must retain info/settings without self kick/transfer/friend");
    select(1);Require(open() && ids()==std::vector<int>({0,1,3,21,2}) && room->GetMenuItems()[2].label.empty(),"owner other menu/source known-empty label changed");
    auto* popup=g_windowMgr.m_chatRoomMenuWnd;popup->OnMouseMove(popup->m_x+2,popup->m_y+2);popup->OnLBtnUp(-20,-20);
    Require(g_windowMgr.m_chatRoomMenuWnd==popup && popup->GetHoverIndex()==-1,"source outside menu release changed lifetime or retained selection");
    choose(0);std::vector<u8> kick(26);kick[0]=0xe2;std::copy_n("Peer",4,kick.begin()+2);
    Require(peer.ReceiveExact(26)==kick && room->GetMembers().size()==2 && !g_windowMgr.m_chatRoomMenuWnd,"E2 bytes or optimistic member deletion changed");
    open();choose(1);std::vector<u8> transfer(30);transfer[0]=0xe0;std::copy_n("Peer",4,transfer.begin()+6);
    Require(peer.ReceiveExact(30)==transfer && room->GetMembers().at("Owner")==0 && room->GetMembers().at("Peer")==1,"E0 bytes or optimistic role mutation changed");
    open();choose(3);std::vector<u8> addFriend(26);addFriend[0]=2;addFriend[1]=2;std::copy_n("Peer",4,addFriend.begin()+2);
    Require(peer.ReceiveExact(26)==addFriend && g_session.m_friendList.empty(),"room friend request bytes or optimistic friend membership changed");
    FRIEND_INFO friendInfo;friendInfo.characterName="Peer";g_session.m_friendList.push_back(friendInfo);
    Require(open() && ids()==std::vector<int>({0,1,3,2}),"existing friend retained add-friend entry");g_session.m_friendList.clear();
    g_serverType=ServerInstantEvent;Require(open() && ids()==std::vector<int>({0,1,3,2}),"instant-event server exposed friend action");g_serverType=ServerNormal;
    open();choose(2);Require(g_windowMgr.m_roomMessageBox && g_windowMgr.m_roomMessageBox->GetMessageId()==191,"unsupported information action did not use source modal191");
    g_windowMgr.OnKeyDown(VK_RETURN);
    auto& router=GetProductionGameModePacketRouter();router.Clear();RegisterDefaultGameModePacketHandlers(router);
    const auto dispatch=[&](const std::vector<u8>& bytes){Require(receive.QueueBytes(bytes),"room action receipt queue failed");
        Require(ProcessGameModePacketLoop(mode,receive,router,1).packetsDispatched==1,"room action receipt framing failed");};
    open();std::vector<u8> role(30);role[0]=0xe1;role[2]=1;std::copy_n("Owner",5,role.begin()+6);dispatch(role);
    Require(room->GetMembers().at("Owner")==1 && room->GetSelectedMember()==-1,"E1 did not own role/selection update");
    choose(0);Require(SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(20))==0,"stale menu selection emitted E2 after roster rebuild");
    select(1);Require(open() && ids()==std::vector<int>({3,21}),"ordinary other menu exposed owner actions");
    select(0);Require(open() && ids()==std::vector<int>({3}),"ordinary self menu unexpectedly removed source info action");
    select(-1);Require(open() && ids()==std::vector<int>({11}),"ordinary blank menu must contain save only");
    room->ChangeRole("Owner",2);select(1);Require(!open(),"unrecognized local role exposed selected-member actions");
    select(-1);Require(open() && ids()==std::vector<int>({11}),"unrecognized local role lost source blank save");
    room->ChangeRole("Owner",0xffffffffu);Require(!open(),"source minus-one local role exposed a menu");
    room->ChangeRole("Owner",1);room->ChangeRole("Peer",0xffffffffu);select(1);Require(!open(),"source minus-one target role exposed a menu");
    room->ChangeRole("Peer",1);select(-1);open();
    g_windowMgr.DeleteWindow(g_windowMgr.m_chatRoomMenuWnd);
    room->SetInfo({"log:/<>|","",1,2,20,77});room->Append("One\nTwo",0);
    const auto directory=GetOpenMidgardIniPath().parent_path()/"Chat";std::error_code error;std::filesystem::create_directories(directory,error);
    // Save tests use unique fixture titles; avoid collisions with prior independent runs.
    const auto unique="room-actions-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    room->SetInfo({unique+":/<>|","",1,2,20,77});const auto saved=directory/(unique+"_____.txt"),saved2=directory/(unique+"_____001.txt");
    g_windowMgr.EnsureChatWindowVisible();g_windowMgr.m_chatWnd->SetState(0);g_windowMgr.ClearChatEvents();
    Require(room->WriteChatToFile() && room->WriteChatToFile(),"source room history export failed");
    Require(g_windowMgr.m_chatWnd->GetExpansionState()!=0 && !g_windowMgr.GetChatEvents().empty()
        && g_windowMgr.GetChatEvents().back().text==saved2.u8string()+" is Saved." && g_windowMgr.GetChatEvents().back().color==0x00ff00,
        "source export manager3 expansion/green saved-path notice changed");
    std::ifstream a(saved,std::ios::binary),b(saved2,std::ios::binary);
    const std::string one((std::istreambuf_iterator<char>(a)),{}),two((std::istreambuf_iterator<char>(b)),{}),literal="One\r\nTwo\r\n";
    Require(one==literal && two==literal,"history export source bytes/sanitization/collision suffix changed");
    a.close();b.close();std::filesystem::remove(saved);std::filesystem::remove(saved2);
    connection->Disconnect();select(1);room->ChangeRole("Owner",0);select(1);open();choose(1);
    Require(room->GetMembers().at("Owner")==0 && room->GetMembers().at("Peer")==1,"failed transfer changed authoritative roles");
    open();auto* retained=g_windowMgr.m_chatRoomMenuWnd;g_windowMgr.DeleteWindow(room);
    Require(g_windowMgr.m_chatRoomMenuWnd==retained,"source room deletion incorrectly destroyed the independent menu");
    room=static_cast<UIChatRoomWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_CHATROOMWND));
    retained->OnLBtnUp(retained->m_x+2,retained->m_y+2);
    Require(!g_windowMgr.m_chatRoomMenuWnd && g_windowMgr.m_chatRoomWnd==room && room->GetMembers().empty(),"retained menu dispatched stale actions into a replacement room");
    std::cout<<"{\"case_id\":\"chat_room_actions\",\"contract_id\":\"PK09.21\",\"source_menu_index\":true,\"actual_e0_e2_friend\":true,\"receipt_owned_roles\":true,\"stale_selection\":true,\"source_export\":true,\"outcome\":\"PASS\"}\n";
}
