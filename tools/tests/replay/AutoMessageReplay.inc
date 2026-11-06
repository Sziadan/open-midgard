#include "ui/UIAutoMessageWnd.h"
#include "core/SourceChatCommand.h"

void AutoMessageReplay()
{
    SetCase("source_auto_message","PK09.21","auto-message-configuration","source commands -> WID116 editor -> actual0097/0096 reply; accepted0069 default");
    g_windowMgr.RemoveAllWindows();CGameMode mode;TestConnection receive;
    struct Restore {
        CMode* current=SkillPacketReplayAccess::Current();int type=SkillPacketReplayAccess::CurrentType();
        ServiceType service=g_serviceType;int folder=g_readFolderFirst,codePage=g_codePage;
        bool enabled=g_session.m_onAutoMSG,save=g_session.m_isSaveChat,base=g_session.m_isShowWhisperWnd,friendWindows=g_session.m_isShowWhisperWnd_Friend,sound=g_session.m_isPlayWhisperOpenSound;
        std::string message=g_session.m_strAutoMSG;u32 aid=g_session.m_aid,auth=g_session.m_authCode,level=g_session.m_userLevel;int sex=g_session.m_sex;
        ro::MannerFilter filter=ro::GetMannerFilter();std::vector<ACTIVE_STATUS_ICON> icons=g_session.GetActiveStatusIcons();
        std::list<FRIEND_INFO> friends=g_session.m_friendList;ro::guild::WindowState guild=g_session.m_guildWindow;std::vector<std::string> excluded=g_session.m_exNameList;
        std::array<char,25> name{};SubChatWindowInfo geometry=g_windowMgr.m_whisperWindowInfo;
        std::filesystem::path cwd=std::filesystem::current_path(),resource;
        bool hadData=std::getenv("OPEN_MIDGARD_DATA_DIR")!=nullptr;std::string data=hadData?std::getenv("OPEN_MIDGARD_DATA_DIR"):"";
        Restore(){std::copy(std::begin(g_session.m_playerName),std::end(g_session.m_playerName),name.begin());}
        ~Restore(){g_windowMgr.RemoveAllWindows();g_windowMgr.ClearChatEvents();CRagConnection::instance()->Disconnect();SkillPacketReplayAccess::Restore(current,type);
            g_serviceType=service;g_readFolderFirst=folder;g_codePage=codePage;g_session.m_onAutoMSG=enabled;g_session.m_isSaveChat=save;
            g_session.m_isShowWhisperWnd=base;g_session.m_isShowWhisperWnd_Friend=friendWindows;g_session.m_isPlayWhisperOpenSound=sound;g_session.m_strAutoMSG=message;
            g_session.m_aid=aid;g_session.m_authCode=auth;g_session.m_userLevel=level;g_session.m_sex=sex;
            ro::GetMannerFilter()=std::move(filter);g_session.SetActiveStatusIcon(107,false,0);
            for(const auto& icon:icons)if(icon.statusType==107){const s32 remaining=static_cast<s32>(icon.expireServerTime-g_session.GetServerTime());
                if(!icon.hasTimer || remaining>0)g_session.SetActiveStatusIcon(107,true,icon.hasTimer?static_cast<u32>(remaining):0);}
            g_session.m_friendList=std::move(friends);g_session.m_guildWindow=std::move(guild);g_session.m_exNameList=std::move(excluded);g_windowMgr.m_whisperWindowInfo=geometry;
            std::copy(name.begin(),name.end(),std::begin(g_session.m_playerName));std::filesystem::current_path(cwd);
#if RO_PLATFORM_WINDOWS
            _putenv_s("OPEN_MIDGARD_DATA_DIR",hadData?data.c_str():"");
#else
            if(hadData)setenv("OPEN_MIDGARD_DATA_DIR",data.c_str(),1);else unsetenv("OPEN_MIDGARD_DATA_DIR");
#endif
            const auto target=std::filesystem::absolute(resource).lexically_normal();
            if(target.parent_path()==cwd && target.filename().string().rfind("auto-message-replay-",0)==0){std::error_code error;std::filesystem::remove_all(target,error);}
        }
    } restore;
    restore.resource=restore.cwd/("auto-message-replay-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(restore.resource/"settings");std::filesystem::current_path(restore.resource);
#if RO_PLATFORM_WINDOWS
    _putenv_s("OPEN_MIDGARD_DATA_DIR",(restore.resource/"settings").string().c_str());
#else
    setenv("OPEN_MIDGARD_DATA_DIR",(restore.resource/"settings").string().c_str(),1);
#endif
    const auto language=[](int empty=-1){std::ofstream file("msgstringtable.txt",std::ios::binary);
        const std::map<int,std::string> labels={{940,"DEFAULT REPLY"},{941,"[AUTO] "},{942,"ENABLED"},{943,"DISABLED"},{944,"Configure <reply>"}};
        for(int i=0;i<=944;++i){const auto found=labels.find(i);if(i!=empty && found!=labels.end())file<<found->second;file<<"#\r\n";}
    };
    language();g_readFolderFirst=1;g_codePage=65001;g_serviceType=ServiceAmerica;
    ro::GetMannerFilter().Clear();g_session.SetActiveStatusIcon(107,false,0);
    g_session.m_friendList.clear();g_session.m_guildWindow={};g_session.m_exNameList.clear();g_session.m_isSaveChat=false;
    g_session.m_isShowWhisperWnd=g_session.m_isShowWhisperWnd_Friend=true;g_session.m_isPlayWhisperOpenSound=false;
    std::memset(g_session.m_playerName,0,sizeof(g_session.m_playerName));std::memcpy(g_session.m_playerName,"AutoLocal",9);
    g_session.m_onAutoMSG=true;g_session.m_strAutoMSG="old text";
    LoginModeReplayAccess login;std::vector<u8> accepted(47);accepted[0]=0x69;accepted[2]=47;
    LoginModeReplayAccess::RunAcceptLogin(login,accepted);
    Require(g_session.m_strAutoMSG=="DEFAULT REPLY" && g_session.m_onAutoMSG,"0069 did not initialize940 or incorrectly reset enable flag");
    g_session.m_strAutoMSG="malformed sentinel";auto malformed=accepted;malformed[2]=48;LoginModeReplayAccess::RunAcceptLogin(login,malformed);
    Require(g_session.m_strAutoMSG=="malformed sentinel" && g_session.m_onAutoMSG,"invalid0069 mutated auto-message state");
    language(940);LoginModeReplayAccess::RunAcceptLogin(login,accepted);
    Require(g_session.m_strAutoMSG.empty() && g_session.m_onAutoMSG,"known-empty940 used a fallback or reset flag");language();
    LoginModeReplayAccess::RunAcceptLogin(login,accepted);std::vector<u8> chars(24);chars[0]=0x6b;chars[2]=24;
    g_session.m_strAutoMSG="character sentinel";LoginModeReplayAccess::RunAcceptChar(login,chars);
    Require(g_session.m_strAutoMSG=="character sentinel" && g_session.m_onAutoMSG,"character006B reset account auto-message state");
    g_session.m_strAutoMSG="DEFAULT REPLY";g_session.m_onAutoMSG=false;SkillPacketReplayAccess::SetCurrent(&mode);
    const auto command=[&](const std::string& text,const char* target=""){return mode.SendMsg(CGameMode::GameMsg_SubmitChat,reinterpret_cast<msgparam_t>(text.c_str()),reinterpret_cast<msgparam_t>(target),0)!=0;};
    Require(command("/AM ignored","NotAWhisper") && g_session.m_onAutoMSG && g_windowMgr.m_autoMessageWnd,"uppercase/argument auto command was sent as a targeted whisper");
    auto* editor=g_windowMgr.m_autoMessageWnd;const auto client=g_windowMgr.GetChatClientRect();
    mode.SendMsg(47,153,0,0);Require(g_session.m_onAutoMSG && g_windowMgr.m_autoMessageWnd==editor,"secondary header153 was used as selected auto talk type");
    Require(mode.SendMsg(47,152,0,0)!=0 && !g_session.m_onAutoMSG && g_windowMgr.m_autoMessageWnd==editor,"source47/152 did not toggle existing editor off");
    Require(mode.SendMsg(47,152,0,0)!=0 && g_session.m_onAutoMSG && g_windowMgr.m_autoMessageWnd==editor,"source47/152 reenable recreated editor");
    Require(editor->m_w==280 && editor->m_h==48 && editor->m_x==185+(client.right-640)/2 && editor->m_y==180+(client.bottom-480)/2
        && editor->GetPrompt()=="Configure <reply>" && editor->GetInputText()=="DEFAULT REPLY" && editor->IsSelected()
        && g_windowMgr.m_editWindow==editor && !g_windowMgr.CanRouteQtChatInput(),"auto editor source placement/default/select/focus contract changed");
    editor->OnChar('N');editor->OnChar('e');editor->OnChar('w');Require(editor->GetInputText()=="New" && g_session.m_strAutoMSG=="DEFAULT REPLY","selected editor typing changed configured message before submit");
    auto* connection=CRagConnection::instance();connection->Disconnect();LoopbackPeer peer(*connection);
    auto& router=GetProductionGameModePacketRouter();router.Clear();RegisterDefaultGameModePacketHandlers(router);
    const auto incoming=[&](const std::string& body){const std::string name="AutoPeer";std::vector<u8> packet(29+body.size());packet[0]=0x97;packet[2]=u8(packet.size());packet[3]=u8(packet.size()>>8);
        std::copy(name.begin(),name.end(),packet.begin()+4);std::copy(body.begin(),body.end(),packet.begin()+28);Require(receive.QueueBytes(packet),"auto incoming queue failed");
        Require(ProcessGameModePacketLoop(mode,receive,router,1).packetsDispatched==1 && !receive.BufferedBytes(),"auto incoming production dispatch failed");
    };
    const auto expectReply=[&](const std::string& body){std::vector<u8> expected(29+body.size());expected[0]=0x96;expected[2]=u8(expected.size());expected[3]=u8(expected.size()>>8);
        std::copy_n("AutoPeer",8,expected.begin()+4);std::copy(body.begin(),body.end(),expected.begin()+28);Require(peer.ReceiveExact(expected.size())==expected,"auto actual0096 differs from independent byte vector");
    };
    incoming("hello");expectReply("[AUTO] DEFAULT REPLY");
    Require(command("/automsg") && !g_session.m_onAutoMSG && g_windowMgr.m_autoMessageWnd==editor && editor->GetInputText()=="New"
        && g_windowMgr.GetChatEvents().back().text=="DISABLED" && g_windowMgr.GetChatEvents().back().color==0xffff64,"toggle-off closed/reinitialized editor or lost943/color");
    editor->SubmitInput();Require(!g_windowMgr.m_autoMessageWnd && !g_session.m_onAutoMSG && g_session.m_strAutoMSG=="New"
        && g_windowMgr.GetChatEvents().back().text=="ENABLED" && g_windowMgr.GetChatEvents().back().color==0xffff64,"submit incorrectly toggled auto mode or lost942/color");
    Require(command(u8"/자동응답"),"Korean auto-message alias failed");editor=g_windowMgr.m_autoMessageWnd;
    Require(editor && editor->IsSelected() && editor->GetInputText()=="New","reopened auto editor lost configured text/selection");
    editor->OnChar('\b');g_windowMgr.m_chatWnd->SetState(0);editor->SubmitInput();
    Require(g_session.m_onAutoMSG && g_session.m_strAutoMSG.empty() && !g_windowMgr.m_autoMessageWnd && g_windowMgr.m_chatWnd->GetExpansionState()==0,"empty submit disabled auto mode or forced expansion");
    incoming("empty configured");expectReply("[AUTO] ");incoming("[AUTO] loop");Require(SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(20))==0,"auto prefix received another reply");
    auto* active=static_cast<UIAutoMessageWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_AUTOMESSAGEWND));active->SetInputText(std::string(99,'x')+u8"é");
    Require(active->GetInputText()==std::string(99,'x')+" ","assigned overlong edit lost source boundary padding");
    active->SetInputText(std::string(99,'x'));active->OnChar('\xc3');active->OnChar('\xa9');
    Require(active->GetInputText()==std::string(99,'x'),"typed insertion used assignment padding or split UTF-8");
    active->SetInputText(std::string(101,'x'));Require(active->GetInputText().size()==100,"auto edit byte cap changed");
    active->SetInputText("discard");g_windowMgr.OnQtKeyDown(VK_ESCAPE,false,false,false);
    Require(!g_session.m_onAutoMSG && !g_windowMgr.m_autoMessageWnd && g_session.m_strAutoMSG.empty() && !g_windowMgr.m_chooseWnd,"explicit auto Escape committed draft or opened quit UI");
    Require(command("/am"),"auto command reenable failed");active=g_windowMgr.m_autoMessageWnd;active->SetInputText(u8"é");active->SendMsg(nullptr,6,118,0,0);
    Require(g_session.m_onAutoMSG && g_session.m_strAutoMSG==u8"é","auto config invented America English-only restriction");incoming("unicode");expectReply(std::string("[AUTO] ")+u8"é");
    active=static_cast<UIAutoMessageWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_AUTOMESSAGEWND));active->SetInputText("retained draft");active->SetShow(0);
    Require(g_windowMgr.MakeWindow(UIWindowMgr::WID_AUTOMESSAGEWND)==active && active->GetInputText()=="retained draft" && active->m_show,"existing auto owner lost draft on reopen");
    const auto eventCount=g_windowMgr.GetChatEvents().size();g_windowMgr.DeleteWindow(active);
    Require(g_session.m_onAutoMSG && g_session.m_strAutoMSG==u8"é" && g_windowMgr.GetChatEvents().size()==eventCount,"generic auto deletion disabled mode/committed draft/emitted notice");
    language(944);active=static_cast<UIAutoMessageWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_AUTOMESSAGEWND));Require(active->GetPrompt().empty(),"known-empty auto prompt used fallback");
    language(942);active->SetInputText("final");active->SubmitInput();Require(g_session.m_strAutoMSG=="final" && g_session.m_onAutoMSG,"empty confirm resource altered commit/flag behavior");
    g_windowMgr.OpenWhisperWindow("SaveToken");auto* privateEditor=g_windowMgr.m_whisperWindows.at("SaveToken");privateEditor->Append("source row",0);
    privateEditor->SetInputText("/SAVECHAT unused");Require(privateEditor->SubmitInput(),"source uppercase private save command was not recognized");
    const auto savedPath=GetOpenMidgardIniPath().parent_path()/"Whisper"/"SaveToken.txt";std::ifstream saved(savedPath,std::ios::binary);
    Require(std::string((std::istreambuf_iterator<char>(saved)),{})=="source row\r\n","normalized private save command lost actual export bytes");saved.close();
    privateEditor->SetInputText("/REPORT unused");Require(privateEditor->SubmitInput() && std::filesystem::is_directory(GetOpenMidgardIniPath().parent_path()/"Report")
        && !std::filesystem::exists(GetOpenMidgardIniPath().parent_path()/"Report"/"AutoLocal.txt"),"normalized private report command invented file content");
    language();g_windowMgr.MakeWindow(UIWindowMgr::WID_AUTOMESSAGEWND);g_windowMgr.RemoveAllWindows();
    Require(g_session.m_onAutoMSG && g_session.m_strAutoMSG=="final" && !g_windowMgr.m_autoMessageWnd,"bulk teardown reset session auto-message state");
    std::cout<<"{\"case_id\":\"source_auto_message\",\"contract_id\":\"PK09.21\",\"actual_0097_0096\":true,\"account_default940\":true,\"selected_edit100\":true,\"toggle_submit_escape_delete_distinct\":true,\"outcome\":\"PASS\"}\n";
}
