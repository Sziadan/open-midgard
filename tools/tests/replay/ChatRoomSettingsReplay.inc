#include "ui/UIChatRoomSettingsWnd.h"
#include "core/MannerFilter.h"
#include "gamemode/Commerce.h"
#include <fstream>

void ChatRoomSettingsReplay()
{
    SetCase("chat_room_settings","PK09.21","source-manner-modal-d5-de",
        "source manner bytes -> room validation modal -> real D5/DE -> pending metadata and receipts");
    g_windowMgr.RemoveAllWindows();CGameMode mode;TestConnection receive;
    struct Restore {
        CMode* current=SkillPacketReplayAccess::Current();int type=SkillPacketReplayAccess::CurrentType();
        int folder=g_readFolderFirst,codePage=g_codePage;ServiceType service=g_serviceType;
        ro::MannerFilter filter=ro::GetMannerFilter();ro::commerce::State commerce=ro::commerce::state;
        std::vector<ACTIVE_STATUS_ICON> icons=g_session.GetActiveStatusIcons();SubChatWindowInfo geometry=g_windowMgr.m_chatRoomWindowInfo;
        std::array<char,25> name{};std::filesystem::path cwd=std::filesystem::current_path(),resource;
        Restore(){std::copy(std::begin(g_session.m_playerName),std::end(g_session.m_playerName),name.begin());}
        ~Restore(){g_windowMgr.RemoveAllWindows();g_windowMgr.ClearChatEvents();CRagConnection::instance()->Disconnect();
            SkillPacketReplayAccess::Restore(current,type);g_readFolderFirst=folder;g_codePage=codePage;g_serviceType=service;
            ro::GetMannerFilter()=std::move(filter);ro::commerce::state=std::move(commerce);
            g_session.SetActiveStatusIcon(29,false,0);
            for(const auto& icon:icons)if(icon.statusType==29)g_session.SetActiveStatusIcon(29,true,icon.hasTimer?(std::max)(1u,icon.expireServerTime-g_session.GetServerTime()):0);
            g_windowMgr.m_chatRoomWindowInfo=geometry;std::copy(name.begin(),name.end(),std::begin(g_session.m_playerName));
            std::filesystem::current_path(cwd);if(!resource.empty())std::filesystem::remove_all(resource);}
    } restore;
    // The token contract is intentionally not a generic whitespace/line parser.
    Require(ro::TokenizeMannerFile("BAD\r\n//skip#all\r\nprefix#tail\r\nlf-only\n final")
        ==std::vector<std::string>({"BAD","prefix","tail"}),"source manner CR/hash/comment/EOF contract changed");
    ro::MannerFilter local;local.AppendEncodedFile(" BAD \r\n\t\r\nplain\r\n",65001);
    Require(local.EncodedWords().words()==std::vector<std::string>({" bad ","plain"})
        && !local.IsBadUtf8Sentence("bad") && local.IsBadUtf8Sentence("x BAD y") && local.IsBadUtf8Sentence("exPLAINed"),
        "source manner whitespace/case/substring behavior changed");
    local.Clear();local.AppendEncodedFile(std::string("\x81\x41\r\n",4),932);
    Require(local.IsBadEncodedSentence(std::string("\x81\x41",2),932) && local.IsBadUtf8Sentence(u8"、"),
        "selected codepage word did not retain raw boundaries and reach UTF-8 UI validation");
    local.Clear();local.AppendEncodedFile("A\r\n",932);
    Require(!local.IsBadEncodedSentence(std::string("\x81\x41",2),932),"DBCS ASCII trail byte was matched as a character start");
    restore.resource=restore.cwd/("chat-settings-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(restore.resource);std::filesystem::current_path(restore.resource);g_readFolderFirst=1;g_codePage=65001;
    {std::ofstream f("manner.txt",std::ios::binary);f<<"BAD\r\n";}
    {std::ofstream f("msgstringtable.txt",std::ios::binary);for(int i=0;i<=661;++i){if(i==14)f<<"#\r\n";else f<<"ROOM MSG "<<i<<"#\r\n";}}
    auto& filter=ro::GetMannerFilter();filter.Clear();Require(filter.Load(),"source manner resource did not load through the real file manager");
    Require(filter.Load() && filter.EncodedWords().words().size()==2,"repeated source load replaced rather than appended words");
    Require(!filter.Load("absent.txt") && filter.IsBadUtf8Sentence("BAD"),"failed source load discarded accepted words");
    Require(std::string(ro::MannerLanguageFolder(0x407))=="german\\" && std::string(ro::MannerLanguageFolder(0x410))=="italian\\"
        && std::string(ro::MannerLanguageFolder(0x416))=="portuguese\\" && std::string(ro::MannerLanguageFolder(0x41f))=="turkish\\"
        && std::string(ro::MannerLanguageFolder(0x41d))=="english\\","source OS primary-language folder mapping changed");
    unsigned language=0;
#if RO_PLATFORM_WINDOWS
    language=GetUserDefaultLangID();
#endif
    const auto localized=std::filesystem::path(ro::MannerLanguageFolder(language));
    std::filesystem::create_directories(localized);
    {std::ofstream f(localized/"manner.txt",std::ios::binary);f<<"LOCAL\r\n";}
    filter.Clear();g_serviceType=ServiceAmerica;
    Require(filter.Load() && filter.IsBadUtf8Sentence("LOCAL") && !filter.IsBadUtf8Sentence("BAD"),"existing language-folder manner file did not precede bare file");
    filter.Clear();g_serviceType=ServiceKorea;
    Require(filter.Load() && !filter.IsBadUtf8Sentence("LOCAL") && filter.IsBadUtf8Sentence("BAD"),"Korea did not bypass the language folder");
    std::filesystem::remove(localized/"manner.txt");filter.Clear();g_serviceType=ServiceAmerica;
    Require(filter.Load() && filter.IsBadUtf8Sentence("BAD"),"missing language file did not fall back to bare manner file");
    SkillPacketReplayAccess::SetCurrent(&mode);g_session.SetActiveStatusIcon(29,false,0);ro::commerce::state.vending.visible=false;ro::commerce::state.vendingActive=false;
    std::memset(g_session.m_playerName,0,sizeof(g_session.m_playerName));std::memcpy(g_session.m_playerName,"Creator",7);
    auto* connection=CRagConnection::instance();connection->Disconnect();LoopbackPeer peer(*connection);
    auto& router=GetProductionGameModePacketRouter();router.Clear();RegisterDefaultGameModePacketHandlers(router);
    const auto dispatch=[&](const std::vector<u8>& bytes){Require(receive.QueueBytes(bytes),"settings receipt queue failed");
        const auto result=ProcessGameModePacketLoop(mode,receive,router,1);Require(result.packetsDispatched==1 && receive.BufferedBytes()==0,"settings receipt framing failed");};
    const char* command="/chat";Require(mode.SendMsg(CGameMode::GameMsg_SubmitChat,reinterpret_cast<msgparam_t>(command),0,0)==1,"/chat failed to open make owner");
    auto* make=g_windowMgr.m_chatRoomMakeWnd;Require(make && !make->IsChange() && make->GetPeople()==20 && !make->IsPrivate(),"source make defaults changed");
    const auto client=g_windowMgr.GetChatClientRect();Require(make->m_x==(client.right-640)/2+185 && make->m_y==300*client.bottom/480,"source UIX/UICY form placement changed");
    mode.m_lastChatroomInfo={"sentinel","old",1,6,15,55};g_windowMgr.ClearChatEvents();
    const auto reject=[&](int id){
        const auto title=make->GetTitleText(),password=make->GetPasswordText();Require(!make->SubmitInput(),"invalid settings were dispatched");
        auto* message=g_windowMgr.m_roomMessageBox;
        Require(message && message->GetMessageId()==id && g_windowMgr.m_modalWindow==message
            && g_windowMgr.m_chatRoomMakeWnd==make && make->GetTitleText()==title && make->GetPasswordText()==password,
            "source room validation lost modal/message/draft ownership");
        Require(message->GetText()==(id==14?std::string():"ROOM MSG "+std::to_string(id)),"known-empty/source validation label changed");
        Require(mode.m_lastChatroomInfo.title=="sentinel" && !g_windowMgr.m_chatRoomWnd && g_windowMgr.GetChatEvents().empty()
            && g_windowMgr.HasWindowAtPoint(-500,-500),"validation changed pending/room/chat state or leaked modal outside clicks");
        g_windowMgr.OnChar('z');g_windowMgr.OnQtKeyDown('C',true,false,false);
        Require(make->GetTitleText()==title && g_windowMgr.m_chatRoomMakeWnd==make,"modal input escaped into the underlying form/hotkey");
        g_windowMgr.OnKeyDown(VK_RETURN);Require(!g_windowMgr.m_modalWindow && !g_windowMgr.m_roomMessageBox && g_windowMgr.m_editWindow==make,
            "validation dismissal did not restore the live editor owner");
        Require(SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(20))==0,"validation emitted network bytes");
    };
    reject(13);make->SetTitleText("xBADx");reject(14);g_serviceType=ServiceAmerica;make->SetTitleText(u8"é");reject(190);
    make->SetTitleText("Room");make->SetPrivate(true);reject(15);make->SetPasswordText("abc");reject(16);
    make->SetPasswordText("abcd");make->ChoosePeople(0);mode.m_isWaitingEnterRoom=7;
    Require(make->SubmitInput() && !g_windowMgr.m_chatRoomMakeWnd && !g_windowMgr.m_chatRoomWnd,"valid make did not dispatch/close without optimistic ownership");
    const std::vector<u8> create={0xd5,0,19,0,2,0,0,'a','b','c','d',0,0,0,0,'R','o','o','m'};
    Require(peer.ReceiveExact(create.size())==create && mode.m_lastChatroomInfo.title=="Room" && mode.m_lastChatroomInfo.pass=="abcd"
        && mode.m_lastChatroomInfo.numPeople==0 && mode.m_lastChatroomInfo.roomId==0xffffffffu && mode.m_isWaitingEnterRoom==7,
        "actual D5 or source pending-copy/count/id/wait ownership changed");
    connection->Disconnect();make=static_cast<UIChatRoomSettingsWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_CHATROOMMAKE));make->SetTitleText("Latest");
    Require(!make->SubmitInput() && !g_windowMgr.m_chatRoomMakeWnd && mode.m_lastChatroomInfo.title=="Latest","failed make did not retain source pending copy and close");
    // Source D6 consumes the latest global pending metadata, not a fabricated FIFO.
    dispatch({0xd6,0,0});auto* room=g_windowMgr.m_chatRoomWnd;Require(room && room->GetInfo().title=="Latest" && !mode.m_isWaitingEnterRoom,"D6 did not consume latest pending metadata");
    auto active=room->GetInfo();active.roomId=0x11223344;active.title="Active";active.numPeople=9;room->SetInfo(active);
    mode.m_lastChatroomInfo={"Prior","abcd",0,3,2,0x11223344};room->OpenMemberMenu(0,0);room->SendMsg(nullptr,48,0,0,0);
    auto* change=g_windowMgr.m_chatRoomChangeWnd;Require(change && change->GetPeople()==2 && change->GetPeopleLabel().find("20")==0
        && change->GetTitleText()=="Prior" && change->HasSelectedTitle(),"source pending prefill/last-label/selection quirks changed");
    change->OnChar('N');Require(change->GetTitleText()=="N","change prefill title was not selected for replacement");
    LoopbackPeer changePeer(*connection);Require(change->SubmitInput() && !g_windowMgr.m_chatRoomChangeWnd,"change dispatch/close failed");
    const std::vector<u8> changed={0xde,0,16,0,2,0,0,'a','b','c','d',0,0,0,0,'N'};
    Require(changePeer.ReceiveExact(changed.size())==changed && room->GetInfo().title=="Active" && room->GetInfo().numPeople==9
        && mode.m_lastChatroomInfo.title=="N" && mode.m_lastChatroomInfo.numPeople==0 && mode.m_lastChatroomInfo.roomId==0xffffffffu,
        "DE optimistically changed active info or lost source pending-copy fields");
    dispatch(RoomListingFrame(0xdf,0x55667788,0x11223344,5,4,1,"Server title"));
    Require(room->GetInfo().title=="Server title" && room->GetInfo().numPeople==4 && mode.m_lastChatroomInfo.title=="N","DF active update overwrote pending metadata");
    room->OpenMemberMenu(0,0);room->SendMsg(nullptr,48,0,0,0);change=g_windowMgr.m_chatRoomChangeWnd;change->SetTitleText("BAD");Require(!change->SubmitInput(),"change bad-title validation bypassed filter");
    dispatch({0xd8,0,0x44,0x33,0x22,0x11});Require(!g_windowMgr.m_chatRoomWnd && !g_windowMgr.m_chatRoomChangeWnd && g_windowMgr.m_roomMessageBox,
        "D8 did not remove settings/active owners while retaining the modal safely");
    g_windowMgr.OnKeyDown(VK_ESCAPE);Require(!g_windowMgr.m_modalWindow && !g_windowMgr.m_editWindow,"modal restored a deleted settings editor");
    const auto pending=mode.m_lastChatroomInfo;ro::chat::RoomInfo ignored;ignored.title="ignored";
    Require(!mode.SendChatRoomSettings(ignored,true) && mode.m_lastChatroomInfo.title==pending.title,"missing-room DE changed pending metadata");
    Require(SelectSocket(changePeer.peer.value,true,false,std::chrono::milliseconds(20))==0,"missing-room change emitted bytes");
    g_session.SetActiveStatusIcon(29,true,0);Require(!g_windowMgr.MakeWindow(UIWindowMgr::WID_CHATROOMMAKE) && !g_windowMgr.m_chatRoomMakeWnd
        && g_windowMgr.m_roomMessageBox->GetMessageId()==661,"source TrickDead make guard failed");g_windowMgr.OnKeyDown(VK_RETURN);
    change=static_cast<UIChatRoomSettingsWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_CHATROOMCHANGE));
    Require(change && g_windowMgr.m_roomMessageBox && g_windowMgr.m_roomMessageBox->GetMessageId()==661,"inherited TrickDead change fallthrough was replaced with make-only behavior");
    g_windowMgr.OnKeyDown(VK_RETURN);g_windowMgr.DeleteWindow(change);g_session.SetActiveStatusIcon(29,false,0);
    ro::commerce::state.vending.visible=true;Require(!g_windowMgr.OpenChatRoomMake() && g_windowMgr.m_roomMessageBox->GetMessageId()==587,"shop owner did not block room make command");
    g_windowMgr.OnKeyDown(VK_RETURN);ro::commerce::state.vending.visible=false;
    ro::commerce::state.vendingActive=true;Require(!g_windowMgr.OpenChatRoomMake() && g_windowMgr.m_roomMessageBox->GetMessageId()==587,"hidden active shop did not block room creation");
    g_windowMgr.OnKeyDown(VK_RETURN);ro::commerce::state.vendingActive=false;
    g_windowMgr.MakeWindow(UIWindowMgr::WID_CHATROOMWND);
    g_windowMgr.MakeWindow(UIWindowMgr::WID_CHATROOMCHANGE);
    std::vector<u8> exit(29);exit[0]=0xdd;std::copy_n("Creator",7,exit.begin()+4);dispatch(exit);
    Require(!g_windowMgr.m_chatRoomWnd && !g_windowMgr.m_chatRoomChangeWnd,"self exit retained active/settings owners");
    Require(g_windowMgr.OnQtKeyDown('C',true,false,false) && g_windowMgr.m_chatRoomMakeWnd,"Alt+C did not open original room make owner");
    make=g_windowMgr.m_chatRoomMakeWnd;g_windowMgr.OnKeyDown(VK_ESCAPE);Require(g_windowMgr.m_chatRoomMakeWnd==make,"source make default cancel117 stopped being inert");
    g_windowMgr.OnQtKeyDown('C',true,false,false);Require(!g_windowMgr.m_chatRoomMakeWnd,"Alt+C did not toggle source make owner closed");
    std::cout<<"{\"case_id\":\"chat_room_settings\",\"contract_id\":\"PK09.21\",\"source_manner\":true,\"modal_validation\":true,\"actual_d5_de\":true,\"pending_before_send\":true,\"source_prefill\":true,\"trickdead\":true,\"outcome\":\"PASS\"}\n";
}
