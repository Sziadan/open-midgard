#include "ui/UIChatRoomWnd.h"
#include "core/SettingsIni.h"
#include <fstream>

void ChatRoomAckReplay()
{
    SetCase("chat_room_acknowledgements","PK09.21","source-d6-da-and-e3-wire",
        "framed acknowledgements -> source cache/notice/owner -> actual room close and text request bytes");
    g_windowMgr.RemoveAllWindows();g_windowMgr.ClearChatEvents();
    CWorld world;CPlayer player;CGameMode mode;TestConnection receive;
    struct Restore {
        CGameMode& mode;CWorld& world;CPlayer& player;
        CMode* current=SkillPacketReplayAccess::Current();int type=SkillPacketReplayAccess::CurrentType();
        bool sticky=g_windowMgr.m_chatWndStickOn;
        std::array<char,25> name{};
        int folder=g_readFolderFirst;ServiceType service=g_serviceType;u32 aid=g_session.m_aid;
        SubChatWindowInfo geometry=g_windowMgr.m_chatRoomWindowInfo;
        std::filesystem::path cwd=std::filesystem::current_path(),ini=GetOpenMidgardIniPath(),resource;
        bool existed=std::filesystem::exists(ini);std::string bytes;
        Restore(CGameMode& value,CWorld& scene,CPlayer& actor):mode(value),world(scene),player(actor){std::copy(std::begin(g_session.m_playerName),std::end(g_session.m_playerName),name.begin());
            if(existed){std::ifstream f(ini,std::ios::binary);bytes.assign(std::istreambuf_iterator<char>(f),{});}}
        ~Restore(){world.UnregisterActor(&player);world.m_player=nullptr;mode.m_world=nullptr;CRagConnection::instance()->Disconnect();g_windowMgr.RemoveAllWindows();g_windowMgr.ClearChatEvents();
            g_windowMgr.m_chatWndStickOn=sticky;g_windowMgr.m_chatRoomWindowInfo=geometry;
            std::copy(name.begin(),name.end(),std::begin(g_session.m_playerName));g_readFolderFirst=folder;g_serviceType=service;g_session.m_aid=aid;
            std::filesystem::current_path(cwd);if(!resource.empty())std::filesystem::remove_all(resource);
            if(existed){std::ofstream f(ini,std::ios::binary|std::ios::trunc);f<<bytes;}else std::filesystem::remove(ini);
            SkillPacketReplayAccess::Restore(current,type);}
    } restore(mode,world,player);
    restore.resource=restore.cwd/("chat-room-ack-language-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(restore.resource);
    {std::ofstream f(restore.resource/"msgstringtable.txt",std::ios::binary);
        for(int i=0;i<=434;++i){if(i==64)f<<"#\r\n";else f<<"ROOM LABEL "<<i<<"#\r\n";}}
    std::filesystem::current_path(restore.resource);g_readFolderFirst=1;
    std::memset(g_session.m_playerName,0,sizeof(g_session.m_playerName));std::memcpy(g_session.m_playerName,"Creator",7);
    SkillPacketReplayAccess::SetCurrent(&mode);g_windowMgr.m_chatWndStickOn=true;g_windowMgr.EnsureChatWindowVisible();
    auto* main=g_windowMgr.m_chatWnd;main->SetState(0);g_windowMgr.ClearChatEvents();
    auto& router=GetProductionGameModePacketRouter();router.Clear();RegisterDefaultGameModePacketHandlers(router);
    const auto dispatch=[&](std::vector<u8> frame){
        Require(receive.QueueBytes(frame),"room acknowledgement queue failed");
        const auto result=ProcessGameModePacketLoop(mode,receive,router,1);
        Require(result.packetsDispatched==1 && receive.BufferedBytes()==0,"room acknowledgement did not use production framing/handler");
    };
    mode.m_lastChatroomInfo={"Pending source room","secret",0,0,4,0xfedc1234};
    mode.m_isWaitingEnterRoom=1;
    Require(!g_windowMgr.m_chatRoomWnd,"pending room metadata invented an owner before acknowledgement");
    dispatch({0xd6,0,0});auto* room=g_windowMgr.m_chatRoomWnd;
    Require(room && !mode.m_isWaitingEnterRoom && main->GetExpansionState()==1 && main->GetSourceHistory(false).empty(),
        "known-empty create notice lost explicit expansion or acknowledged owner");
    Require(room->GetInfo().title=="Pending source room" && room->GetInfo().pass=="secret"
        && room->GetInfo().roomId==0xfedc1234 && room->GetInfo().numPeople==1 && room->GetInfo().maxNumPeople==4
        && room->GetMembers().at("Creator")==0 && room->GetSelectedMember()==-1,
        "D6 success did not retain pending metadata and rebuild the local owner roster");
    const auto preserved=mode.m_lastChatroomInfo;
    for(u8 result:{u8(1),u8(2),u8(255)}){
        const auto before=g_windowMgr.GetChatEvents().size();mode.m_isWaitingEnterRoom=1;
        dispatch({0xd6,0,result});
        Require(!mode.m_isWaitingEnterRoom && g_windowMgr.m_chatRoomWnd==room && mode.m_lastChatroomInfo.pass==preserved.pass,
            "D6 failure altered room ownership or pending metadata");
        Require(g_windowMgr.GetChatEvents().size()==before+(result==255?0:1),"D6 unknown/known result notice domain changed");
        if(result!=255)Require(g_windowMgr.GetChatEvents().back().text=="ROOM LABEL "+std::to_string(result==1?65:66)
            && g_windowMgr.GetChatEvents().back().color==0xff0000,"D6 refusal selected wrong source label/color");
    }
    const std::array<int,8> ids={67,7,68,-1,55,432,433,434};
    for(int result=0;result<9;++result){
        const u8 wire=result==8?255:static_cast<u8>(result);const int id=result==8?-1:ids[result];
        const auto before=g_windowMgr.GetChatEvents().size();mode.m_isWaitingEnterRoom=1;main->SetState(0);
        dispatch({0xda,0,wire});Require(!mode.m_isWaitingEnterRoom && g_windowMgr.m_chatRoomWnd==room,"DA changed active ownership or retained wait");
        Require(g_windowMgr.GetChatEvents().size()==before+(id<0?0:1),"DA notice domain includes unsupported result");
        if(id>=0)Require(main->GetExpansionState()==1 && g_windowMgr.GetChatEvents().back().text=="ROOM LABEL "+std::to_string(id)
            && g_windowMgr.GetChatEvents().back().color==0xff0000,"DA label/color/explicit expansion differs");
    }
    const std::string sourceJoin="(Alice"+std::string("\x29\xb4\xd4\xc0\xcc\x20\xc0\xd4\xc0\xe5\x20\xc7\xcf\xbf\xb4\xbd\xc0\xb4\xcf\xb4\xd9\x2e");
    room->Append(sourceJoin.c_str(),0);
    Require(!room->GetRows().empty() && room->GetRows().back().talker=="Alice",
        "selected-PE Korean member delimiter did not preserve source talker extraction");
    g_session.m_aid=0x12345678;world.m_player=&player;player.m_gid=0x42345678;world.RegisterActor(&player);mode.m_world=&world;
    player.SetChatBubbleText("held before room submit",GetTickCount()+60000);
    auto* connection=CRagConnection::instance();connection->Disconnect();LoopbackPeer peer(*connection);
    g_serviceType=ServiceAmerica;main->SetState(0);room->SetInputText(u8"é");
    Require(!room->SubmitInput() && room->GetInputText()==u8"é" && main->GetExpansionState()==0
        && g_windowMgr.GetChatEvents().back().text=="ROOM LABEL 190"
        && g_windowMgr.GetChatEvents().back().color==0xff0000,
        "America-only input rejection lost its source notice/draft/non-expansion behavior");
    Require(SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(25))==0,
        "America-only rejected input emitted a chat request");
    g_serviceType=ServiceKorea;
    room->SetInputText("hello room");const auto rowCount=room->GetRows().size();
    Require(room->SubmitInput() && room->GetInputText().empty() && room->GetRows().size()==rowCount,
        "room submit failed or optimistically echoed before server speech");
    const std::string payload="Creator : hello room";std::vector<u8> expected(4+payload.size()+1,0);
    expected[0]=0xf3;expected[2]=static_cast<u8>(expected.size());std::copy(payload.begin(),payload.end(),expected.begin()+4);
    const auto normalBytes=peer.ReceiveExact(expected.size());
    Require(player.GetChatBubbleText()=="held before room submit","outgoing room text predicted a world speech bubble");
    Require(normalBytes==expected,"room normal submit request bytes differ: expected="+Hex(expected)+" actual="+Hex(normalBytes));
    room->AddMember("Target",1);const auto layout=room->GetLayout();room->OnLBtnDown(room->m_x+layout.members.left+2,room->m_y+layout.members.top+16);
    room->OnLBtnUp(0,0);room->SetInputText("private");Require(room->SubmitInput(),"selected member whisper submit failed");
    expected.assign(36,0);expected[0]=0x96;expected[2]=36;
    const std::string target="Target",message="private";std::copy(target.begin(),target.end(),expected.begin()+4);
    std::copy(message.begin(),message.end(),expected.begin()+28);
    Require(peer.ReceiveExact(expected.size())==expected,"selected non-self member whisper bytes differ");
    room->OnKeyDown(VK_ESCAPE);
    Require(SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(25))==0,"source cancel117 emitted a room exit");
    room->SendMsg(nullptr,6,135,0,0);
    Require(peer.ReceiveExact(2)==std::vector<u8>({0xe3,0}) && g_windowMgr.m_chatRoomWnd==room,
        "close135 room exit bytes or server-owned lifetime differ");
    room->SetInputText(std::string(69,'a'));room->OnChar(static_cast<char>(0xc3));room->OnChar(static_cast<char>(0xa9));
    Require(room->GetInputText()==std::string(69,'a'),"room byte ceiling retained a partial UTF-8 unit");
    connection->Disconnect();room->SetInputText("failed draft");
    Require(!room->SubmitInput() && room->GetInputText().empty() && g_windowMgr.m_chatRoomWnd==room,
        "legacy room dispatch clear/failure lifetime behavior changed");
    dispatch(RoomMemberChangeFrame(0xdd,0,"Creator",0));
    Require(!g_windowMgr.m_chatRoomWnd && mode.SendMsg(53,0,0,0)==0,"self exit or missing-owner leave guard failed");
    LoopbackPeer worldPeer(*connection);
    const std::string worldText="world echo";
    Require(mode.SendMsg(CGameMode::GameMsg_SubmitChat,reinterpret_cast<msgparam_t>(worldText.c_str()),0,0)!=0,"world normal submit failed");
    const std::string worldPayload="Creator : "+worldText;
    expected.assign(4+worldPayload.size()+1,0);expected[0]=0xf3;expected[2]=static_cast<u8>(expected.size());
    std::copy(worldPayload.begin(),worldPayload.end(),expected.begin()+4);
    Require(worldPeer.ReceiveExact(expected.size())==expected
        && player.GetChatBubbleText()=="held before room submit",
        "outgoing world text predicted a bubble or emitted incorrect bytes");
    auto echo=expected;echo[0]=0x8e;const auto eventsBeforeEcho=g_windowMgr.GetChatEvents().size();
    dispatch(echo);
    Require(player.GetChatBubbleText()==worldText && g_windowMgr.GetChatEvents().size()==eventsBeforeEcho+1,
        "008E world echo did not own bubble and main-history insertion");
    std::cout<<"{\"case_id\":\"chat_room_acknowledgements\",\"contract_id\":\"PK09.21\",\"known_empty_create_notice\":true,\"d6_da_domain\":true,\"actual_e3_f3_96_wire\":true,\"outcome\":\"PASS\"}\n";
}
