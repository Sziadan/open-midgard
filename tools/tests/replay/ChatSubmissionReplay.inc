#include "ui/UIWhisperWnd.h"
#include "ui/UIChatRoomWnd.h"
#include "ui/UIOptionWnd.h"
#include "core/MannerFilter.h"
#include "core/SourceLanguageCharset.h"

namespace {
ro::chat::SourceChatModifiers submissionKeys;
u32 submissionTick=10000;
u16 submissionLanguage=0x0409;
u16 SubmissionLanguage(){return submissionLanguage;}
ro::chat::SourceChatModifiers SubmissionModifiers(){return submissionKeys;}
u32 SubmissionTicks(){return submissionTick;}
}
void ChatSubmissionReplay()
{
    SetCase("source_chat_submission","PK09.21","source-caller-dispatch","actual editor -> parser ->47/11 -> manager buffer -> real request frames");
    g_windowMgr.RemoveAllWindows();CGameMode mode;
    struct Restore {
        bool multi=g_multiLang;s8 hiddenHi=g_MessageHi;
        CMode* current=SkillPacketReplayAccess::Current();int type=SkillPacketReplayAccess::CurrentType();
        decltype(g_serverType) server=g_serverType;ServiceType service=g_serviceType;
        int folder=g_readFolderFirst,codePage=g_codePage,talk=g_session.m_talkType;
        bool battle=g_session.m_isBattleChat,minimum=g_session.m_isMinEffect,oldMinimum=g_session.m_isMinEffectOld,autoReply=g_session.m_onAutoMSG;
        std::vector<std::pair<int,int>> flags=g_session.m_cmdOnOffSet;
        std::list<FRIEND_INFO> party=g_session.GetPartyList();ro::guild::WindowState guild=g_session.m_guildWindow;
        ro::MannerFilter filter=ro::GetMannerFilter();std::vector<ACTIVE_STATUS_ICON> icons=g_session.GetActiveStatusIcons();
        std::string pending=g_windowMgr.GetChatMsg();std::array<char,25> name{};
        std::vector<std::string> cachedHistory=g_windowMgr.m_chatInputHistory;
        std::string cachedTarget=g_windowMgr.m_chatWhisperTargetText,cachedInput=g_windowMgr.m_chatInputText;
        int cachedActive=g_windowMgr.m_chatActiveInputField,cachedScroll=g_windowMgr.m_chatScrollLineOffset;
        int chatShow=g_windowMgr.m_chatWndShow,chatStatus=g_windowMgr.m_chatWndStatus,chatHeight=g_windowMgr.m_chatWndHeight,chatX=g_windowMgr.m_chatWndX,chatY=g_windowMgr.m_chatWndY;
        SubChatWindowInfo privateInfo=g_windowMgr.m_whisperWindowInfo,roomInfo=g_windowMgr.m_chatRoomWindowInfo;
        float volume=CAudio::GetInstance()->GetVolume();int bgm=CAudio::GetInstance()->GetBgmVolume();
        bool hadData=std::getenv("OPEN_MIDGARD_DATA_DIR")!=nullptr;std::string data=hadData?std::getenv("OPEN_MIDGARD_DATA_DIR"):"";
        std::filesystem::path cwd=std::filesystem::current_path(),resource;
        Restore(){std::copy(std::begin(g_session.m_playerName),std::end(g_session.m_playerName),name.begin());}
        ~Restore(){g_multiLang=multi;g_MessageHi=hiddenHi;
            g_windowMgr.m_commandListWnd=nullptr;g_windowMgr.RemoveAllWindows();g_windowMgr.ClearChatEvents();CRagConnection::instance()->Disconnect();
            SkillPacketReplayAccess::Restore(current,type);g_serverType=server;g_serviceType=service;g_readFolderFirst=folder;g_codePage=codePage;
            g_session.m_talkType=talk;g_session.m_isBattleChat=battle;g_session.m_isMinEffect=minimum;g_session.m_isMinEffectOld=oldMinimum;
            g_session.m_onAutoMSG=autoReply;g_session.m_cmdOnOffSet=std::move(flags);g_session.m_partyList=std::move(party);g_session.m_guildWindow=std::move(guild);
            ro::GetMannerFilter()=std::move(filter);g_windowMgr.SetChatMsg(pending);
            g_windowMgr.m_chatInputHistory=std::move(cachedHistory);g_windowMgr.m_chatWhisperTargetText=cachedTarget;g_windowMgr.m_chatInputText=cachedInput;
            g_windowMgr.m_chatActiveInputField=cachedActive;g_windowMgr.m_chatScrollLineOffset=cachedScroll;
            g_windowMgr.m_chatWndShow=chatShow;g_windowMgr.m_chatWndStatus=chatStatus;g_windowMgr.m_chatWndHeight=chatHeight;g_windowMgr.m_chatWndX=chatX;g_windowMgr.m_chatWndY=chatY;
            g_windowMgr.m_whisperWindowInfo=privateInfo;g_windowMgr.m_chatRoomWindowInfo=roomInfo;
            g_session.SetActiveStatusIcon(107,false,0);for(const auto& icon:icons)if(icon.statusType==107){const s32 remaining=static_cast<s32>(icon.expireServerTime-g_session.GetServerTime());
                if(!icon.hasTimer || remaining>0)g_session.SetActiveStatusIcon(107,true,icon.hasTimer?static_cast<u32>(remaining):0);}
            std::copy(name.begin(),name.end(),std::begin(g_session.m_playerName));CAudio::GetInstance()->SetVolume(volume);CAudio::GetInstance()->SetBgmVolume(bgm);
            std::filesystem::current_path(cwd);
#if RO_PLATFORM_WINDOWS
            _putenv_s("OPEN_MIDGARD_DATA_DIR",hadData?data.c_str():"");
#else
            if(hadData)setenv("OPEN_MIDGARD_DATA_DIR",data.c_str(),1);else unsetenv("OPEN_MIDGARD_DATA_DIR");
#endif
            const auto path=std::filesystem::absolute(resource).lexically_normal();
            if(path.parent_path()==cwd && path.filename().string().rfind("chat-submission-replay-",0)==0){std::error_code error;std::filesystem::remove_all(path,error);}
        }
    } restore;
    restore.resource=restore.cwd/("chat-submission-replay-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(restore.resource/"settings");std::filesystem::current_path(restore.resource);
#if RO_PLATFORM_WINDOWS
    _putenv_s("OPEN_MIDGARD_DATA_DIR",(restore.resource/"settings").string().c_str());
#else
    setenv("OPEN_MIDGARD_DATA_DIR",(restore.resource/"settings").string().c_str(),1);
#endif
    {std::ofstream file("msgstringtable.txt",std::ios::binary);const std::map<int,std::string> labels={{25,"SOUND"},{26,"MUSIC"},{58,"ALL DENIED"},{59,""},{60,"ALL ALLOWED"},{61,"ALL ALLOW FAIL"},{62,"EMPTY LIST"},{63,"EXCLUSION LIST"},{193," IGNORE OK"},{194," IGNORE FAIL1"},{195,""},{196," ALLOW OK"},{197," ALLOW FAIL1"},{198," ALLOW FAIL2"},{49,"REPEAT"},{50,"MANNER"},{95,"INVALID"},{190,"ENGLISH"},{544,"/!"},{545,"/?"},
        {687,"MIN ON"},{688,"MIN OFF"},{698,"/tiny"},{940,"Default"},{943,"Auto off"},{944,"Configure"}};
        for(int i=0;i<=944;++i){const auto found=labels.find(i);if(found!=labels.end())file<<found->second;file<<"#\r\n";}}
    {std::ofstream file("manner.txt");file<<"forbidden#\r\n";}
    g_readFolderFirst=1;g_codePage=65001;g_serviceType=ServiceAmerica;g_serverType=ServerNormal;
    g_session.m_talkType=0;g_session.m_isBattleChat=false;g_session.m_onAutoMSG=false;g_session.m_cmdOnOffSet.clear();
    g_session.m_partyList.clear();g_session.m_guildWindow={};g_session.SetActiveStatusIcon(107,false,0);
    ro::GetMannerFilter().Clear();ro::GetMannerFilter().Load("manner.txt");
    std::memset(g_session.m_playerName,0,sizeof(g_session.m_playerName));std::memcpy(g_session.m_playerName,"SourceLocal",11);
    SkillPacketReplayAccess::SetCurrent(&mode);const SourceChatInputServices services{SubmissionModifiers,SubmissionTicks,SubmissionLanguage};mode.SetSourceChatInputServices(&services);
    submissionKeys={};submissionTick=10000;submissionLanguage=0x0409;g_multiLang=false;g_MessageHi=0;
    g_windowMgr.ShowChatWindow(true);auto* chat=g_windowMgr.m_chatWnd;
#include "SourceCommandFlagsReplay.inc"
    chat->RestorePersistentState({},"","",0,0);chat->FocusQtInput(2);
    // Failure does not roll back original text/history; whitespace is retained.
    CRagConnection::instance()->Disconnect();chat->SetQtInputText(2,"  raw failed  ");Require(chat->HandleKeyDown(VK_RETURN),"main Enter was not consumed on failed send");
    Require(chat->GetInputText().empty() && chat->GetInputHistory().front()=="  raw failed  " && std::string(g_windowMgr.GetChatMsg()).empty(),"source history/clear/raw text depended on successful send");
    chat->SetQtInputText(2,"  raw failed  ");chat->HandleKeyDown(VK_RETURN);Require(chat->GetInputHistory().size()==2,"source input history deduplicated repeated entries");
    chat->SetQtInputText(2,u8"é");Require(chat->HandleKeyDown(VK_RETURN) && chat->GetInputText().empty() && chat->GetInputHistory().front()==u8"é"
        && g_windowMgr.GetChatEvents().back().text=="ENGLISH","America validation preceded source history/store or kept the submitted draft");
    for(int i=0;i<33;++i){chat->SetQtInputText(2,"history"+std::to_string(i));chat->HandleKeyDown(VK_RETURN);}
    Require(chat->GetInputHistory().size()==30 && chat->GetInputHistory().front()=="history32" && chat->GetInputHistory().back()=="history3","source history did not retain30 ordered entries");
    chat->FocusQtInput(2);chat->HandleKeyDown(VK_UP);chat->HandleKeyDown(VK_UP);
    // Editing a recalled row keeps the original source history cursor.
    chat->HandleChar('x');const auto edited=chat->GetInputText();chat->HandleKeyDown(VK_RETURN);
    Require(chat->GetInputHistory()[1]==edited && chat->GetInputText()=="history32","source browsing overwrite did not advance to the next newer history row");
    UIWindow commandList;g_windowMgr.m_commandListWnd=&commandList;commandList.m_isDirty=0;
    g_session.m_cmdOnOffSet={{0,3},{7,3}};g_windowMgr.SetChatMsg("preserved sentinel");chat->SetQtInputText(2,"/not-a-command");chat->HandleKeyDown(VK_RETURN);
    Require(g_session.m_cmdOnOffSet[0].first==1 && g_session.m_cmdOnOffSet[1].first==7 && commandList.m_isDirty
        && std::string(g_windowMgr.GetChatMsg())=="preserved sentinel" && g_windowMgr.GetChatEvents().back().text=="INVALID","negative parser result skipped47/prelude or overwrote manager message");
    g_windowMgr.m_commandListWnd=nullptr;g_session.m_cmdOnOffSet.clear();
    g_session.m_isMinEffect=false;g_session.m_isMinEffectOld=false;
    const auto submit=[&](const std::string& text,const char* target=""){return mode.SendMsg(CGameMode::GameMsg_SubmitChat,reinterpret_cast<msgparam_t>(text.c_str()),reinterpret_cast<msgparam_t>(target),0)!=0;};
    Require(submit("/tiny") && g_session.m_isMinEffect && g_session.m_isMinEffectOld && std::string(g_windowMgr.GetChatMsg())=="/tiny"
        && g_windowMgr.GetChatEvents().back().text=="MIN ON","GetTalkType minimum-effect mutation/timing missing");
    Require(submit("/str+ 257") && mode.m_SiyeonStatus[0]==1 && std::string(g_windowMgr.GetChatMsg())=="/str+ 257","source stat byte assignment or caller offset missing");
    g_windowMgr.SetChatMsg("volume sentinel");Require(submit("/v 999") && CAudio::GetInstance()->GetVolume()==1.0f
        && std::string(g_windowMgr.GetChatMsg())=="volume sentinel" && g_windowMgr.GetChatEvents().back().text=="SOUND 127","source volume clamp/notice/cached message changed");
    Require(submit("/bv -1") && CAudio::GetInstance()->GetBgmVolume()==0 && g_windowMgr.GetChatEvents().back().text=="MUSIC 0","source BGM127 range clamp failed");
    g_session.m_cmdOnOffSet={{0,6}};g_serviceType=ServiceVietnam;g_windowMgr.SetChatMsg("service sentinel");
    Require(!mode.SendMsg(47,6,0,0) && g_session.m_cmdOnOffSet[0].first==1 && g_windowMgr.GetChatEvents().back().text=="INVALID"
        && std::string(g_windowMgr.GetChatMsg())=="service sentinel","Vietnam type6 exception ran before prelude or changed retained message");
    g_serviceType=ServiceAmerica;g_session.m_cmdOnOffSet.clear();
    chat->SetQtInputText(2,"/am");chat->HandleKeyDown(VK_RETURN);
    Require(g_session.m_onAutoMSG && g_windowMgr.m_autoMessageWnd && g_windowMgr.m_editWindow==g_windowMgr.m_autoMessageWnd,
        "main caller restored common focus after synchronous auto-editor creation");
    auto* autoEditor=g_windowMgr.m_autoMessageWnd;Require(submit("/am") && !g_session.m_onAutoMSG,"auto toggle-off failed through general dispatch");
    g_windowMgr.DeleteWindow(autoEditor);
    auto* connection=CRagConnection::instance();connection->Disconnect();LoopbackPeer peer(*connection);const auto& profile=ro::net::GetActiveMapGameplaySendProfile();
    Require(profile.id==ro::net::MapGameplaySendProfileId::PacketVer23 && profile.globalMessage==0x00f3 && profile.whisper==0x0096
        && profile.partyMessage==0x0108 && profile.guildMessage==0x017e && profile.battleMessage==0x02db
        && profile.characterNameRequest==0x00a2 && profile.characterNameRequestLength==15 && profile.characterNameRequestIdOffset==11,
        "selected source chat profile IDs or preserved name-request layout differ from primary/server registration");
    const auto named=[&](u16 opcode,const std::string& body){const auto payload="SourceLocal : "+body;std::vector<u8> expected(payload.size()+5,0);
        expected[0]=u8(opcode);expected[1]=u8(opcode>>8);expected[2]=u8(expected.size());expected[3]=u8(expected.size()>>8);std::copy(payload.begin(),payload.end(),expected.begin()+4);
        Require(peer.ReceiveExact(expected.size())==expected,"actual named source channel frame differs");};
    const auto whispered=[&](const std::string& target,const std::string& body){std::vector<u8> expected(29+body.size(),0);expected[0]=u8(profile.whisper);expected[1]=u8(profile.whisper>>8);
        expected[2]=u8(expected.size());expected[3]=u8(expected.size()>>8);std::copy(target.begin(),target.end(),expected.begin()+4);std::copy(body.begin(),body.end(),expected.begin()+28);
        Require(peer.ReceiveExact(expected.size())==expected,"actual raw-target source11 whisper frame differs");};
    Require(submit("  raw public  "),"raw public dispatch failed");named(profile.globalMessage,"  raw public  ");
    Require(submit(" raw private "," Peer "),"raw private dispatch failed");whispered(" Peer "," raw private ");
    Require(submit(" /am","Peer"),"leading space should select whisper11");whispered("Peer"," /am");Require(!g_session.m_onAutoMSG,"main source caller trimmed before slash predicate");
    FRIEND_INFO member{};member.AID=99;member.isValid=true;member.characterName="Member";g_session.m_partyList.push_back(member);
    Require(submit("%party"),"percent party route failed");named(profile.partyMessage,"party");
    g_session.m_guildWindow.id=42;Require(submit("$guild"),"dollar guild route failed");named(profile.guildMessage,"guild");
    g_session.m_talkType=2;submissionKeys={true,false};Require(submit("guild priority"),"guild mode priority failed");named(profile.guildMessage,"guild priority");
    submissionKeys={false,true};Require(submit("alt public"),"guild Alt override failed");named(profile.globalMessage,"alt public");
    submissionKeys={};g_session.m_talkType=0;g_session.m_isBattleChat=true;Require(submit("battle"),"battle source route failed");named(profile.battleMessage,"battle");
    auto* room=static_cast<UIChatRoomWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_CHATROOMWND));room->ClearMembers();room->SetInputText("room world");
    Require(room->SubmitInput(),"room ordinary source dispatch failed");named(profile.globalMessage,"room world");
    g_session.m_isBattleChat=false;g_session.m_talkType=0;
    mode.m_sourceLastSubmittedChat.clear();mode.m_sourceSameChatRepeatCount=0;
    Require(submit("repeat"),"first shared-repeat send failed");named(profile.globalMessage,"repeat");
    Require(submit("repeat","Peer"),"second shared-repeat send failed");whispered("Peer","repeat");
    Require(!submit("%repeat") && g_windowMgr.GetChatEvents().back().text=="REPEAT" && std::string(g_windowMgr.GetChatMsg()).empty(),"shared source repeat state did not reject/clear across public/private/party");
    Require(!submit("forbidden") && g_windowMgr.GetChatEvents().back().text=="MANNER" && std::string(g_windowMgr.GetChatMsg()).empty(),"manner rejection did not own manager clear");
    g_session.SetActiveStatusIcon(107,true,0);Require(!submit("berserk") && std::string(g_windowMgr.GetChatMsg())=="berserk","Berserk should retain source message");g_session.SetActiveStatusIcon(107,false,0);
    g_session.m_guildWindow.id=0;Require(!submit("/gc stale guild") && std::string(g_windowMgr.GetChatMsg())=="stale guild","guild guard should retain parser-owned tail");
    Require(!submit("%") && std::string(g_windowMgr.GetChatMsg())=="%","empty percent prefix should not clear source message");
    Require(SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(20))==0,"rejected source messages leaked packets");
    mode.m_reqEmotionTick=0;mode.m_isReqEmotion=0;Require(submit("/!"),"registered source emotion did not route47->33");
    Require(peer.ReceiveExact(3)==std::vector<u8>({0xbf,0,0}) && mode.m_isReqEmotion && mode.m_reqEmotionTick==10000,"source emotion0/pending/time/frame differs");
    Require(!submit("/?"),"pending source emotion was not guarded");mode.m_isReqEmotion=0;submissionTick=12500;Require(!submit("/?"),"source emotion exact2500 boundary changed");
    submissionTick=12501;Require(submit("/?"),"source emotion after2500 was blocked");Require(peer.ReceiveExact(3)==std::vector<u8>({0xbf,0,1}),"source question emotion argument was shifted");
    Require(submit("/leave"),"source leave-party command failed");Require(peer.ReceiveExact(2)==std::vector<u8>({0,1}),"source leave-party command used disband/kick semantics");
    // Source51 exports BOTH complete histories, independently of the visible tab/filter.
    {
        UINewChatWnd exported;
        struct RestoreMain {UINewChatWnd* borrowed;UINewChatWnd* chat;UIWindow* edit;~RestoreMain(){g_windowMgr.m_children.remove(borrowed);g_windowMgr.m_chatWnd=chat;g_windowMgr.m_editWindow=edit;}} restoreMain{&exported,g_windowMgr.m_chatWnd,g_windowMgr.m_editWindow};
        g_windowMgr.m_chatWnd=&exported;
        exported.ReceiveSourceNotice("Normal",0xffffff,ro::chat::NoticeRoute::Normal,static_cast<ro::chat::NoticeType>(0));
        exported.ReceiveSourceNotice("Battle",0xffffff,ro::chat::NoticeRoute::Command,static_cast<ro::chat::NoticeType>(0));
        const auto base=GetOpenMidgardIniPath().parent_path();
        const auto bytes=[](const std::filesystem::path& path){std::ifstream file(path,std::ios::binary);return std::string(std::istreambuf_iterator<char>(file),{});};
        const auto saveEventStart=g_windowMgr.GetChatEvents().size();
        exported.SelectSourceTab(1);exported.FocusQtInput(2);exported.SetQtInputText(2,"/SAVECHAT");
        Require(exported.HandleKeyDown(VK_RETURN),"main save Enter not consumed");
        Require(bytes(base/"Chat"/"Chat.txt")=="Normal\r\n" && bytes(base/"Chat_BM"/"Chat_BM.txt")=="Battle\r\n","main save did not export separate complete histories/CRLF bytes");
        // Inspect unsplit event notices: long temporary paths can span several history rows.
        Require(g_windowMgr.GetChatEvents().size()==saveEventStart+2 && g_windowMgr.GetChatEvents()[saveEventStart].text==(base/"Chat"/"Chat.txt").u8string()+" is Saved."
            && g_windowMgr.GetChatEvents()[saveEventStart+1].text==(base/"Chat_BM"/"Chat_BM.txt").u8string()+" is Saved.","dual export notices/order differ");
        exported.SetQtInputText(2,"/savechat");exported.HandleKeyDown(VK_RETURN);
        Require(bytes(base/"Chat"/"Chat.txt")=="Normal\r\n" && bytes(base/"Chat_BM"/"Chat001_BM.txt")=="Battle\r\n"
            && bytes(base/"Chat"/"Chat001.txt").rfind("Normal\r\n",0)==0,"main collision suffix or no-overwrite behavior differs");
        const auto events=g_windowMgr.GetChatEvents().size();
        g_session.m_cmdOnOffSet={{0,115},{0,116}};
        exported.SetQtInputText(2,"/report");exported.HandleKeyDown(VK_RETURN);
        Require(g_windowMgr.GetChatEvents().size()==events && !std::filesystem::exists(base/"Report") && g_session.m_cmdOnOffSet[0].first==1,
            "main report should enter47 prelude then selected manager17 no-op");
        std::filesystem::create_directories(base/"Report");
        {std::ofstream file(base/"Report"/"SourceLocal.txt");file<<"keep";}
        room->SetInputText("/report");Require(room->SubmitInput(),"room report remap failed");
        Require(bytes(base/"Report"/"SourceLocal.txt")=="keep" && !std::filesystem::exists(base/"Report"/"SourceLocal001.txt")
            && g_windowMgr.GetChatEvents().back().text==(base/"Report"/"SourceLocal001.txt").u8string()+" is Saved."
            && g_session.m_cmdOnOffSet[1].first==1,"room report must use local name/collision and success-only stub");
        auto roomInfo=room->GetInfo();roomInfo.title="Room:/Name";room->SetInfo(roomInfo);room->Append("Room export",0xabcdef);
        room->SetInputText("/savechat");Require(room->SubmitInput() && bytes(base/"Chat"/"Room__Name.txt")=="Room export\r\n","room52 save did not enter manager7/title export");
        const auto absentEvents=g_windowMgr.GetChatEvents().size();
        g_windowMgr.m_chatWnd=nullptr;g_windowMgr.m_chatRoomWnd=nullptr;
        g_windowMgr.SendMsg(6,0,0,0);g_windowMgr.SendMsg(7,0,0,0);g_windowMgr.SendMsg(17,0,0,0);g_windowMgr.SendMsg(18,0,0,0);
        g_windowMgr.m_chatWnd=&exported;g_windowMgr.m_chatRoomWnd=room;
        Require(g_windowMgr.GetChatEvents().size()==absentEvents,"save/report manager created absent owners or notices");
        // Directory failure suppresses both streams; second-directory failure preserves first success.
        const auto failBase=base/"export-failure";std::filesystem::create_directories(failBase);
#if RO_PLATFORM_WINDOWS
        _putenv_s("OPEN_MIDGARD_DATA_DIR",failBase.string().c_str());
#else
        setenv("OPEN_MIDGARD_DATA_DIR",failBase.string().c_str(),1);
#endif
        {std::ofstream file(failBase/"Chat");file<<"blocked";}
        const auto before=g_windowMgr.GetChatEvents().size();
        Require(!exported.WriteChatToFile() && g_windowMgr.GetChatEvents().size()==before && !std::filesystem::exists(failBase/"Chat_BM"),"first directory failure should abort dual export");
        std::filesystem::remove(failBase/"Chat");{std::ofstream file(failBase/"Chat_BM");file<<"blocked";}
        Require(exported.WriteChatToFile() && std::filesystem::is_regular_file(failBase/"Chat"/"Chat.txt") && g_windowMgr.GetChatEvents().size()==before+1,
            "second directory failure discarded first success or emitted a second notice");
#if RO_PLATFORM_WINDOWS
        _putenv_s("OPEN_MIDGARD_DATA_DIR",base.string().c_str());
#else
        setenv("OPEN_MIDGARD_DATA_DIR",base.string().c_str(),1);
#endif
    }
    // Private save/report/quit are consumed by the original caller before47.
    Require(g_windowMgr.OpenWhisperWindow("PrivateScope",nullptr),"private command entry failed");
    auto* privateWindow=g_windowMgr.m_whisperWindows.at("PrivateScope");
    Require(privateWindow!=nullptr,"private command caller creation failed");
    g_session.m_cmdOnOffSet={{0,51},{0,115},{0,24}};
    privateWindow->SetInputText("/SAVECHAT");Require(privateWindow->SubmitInput(),"private source parser save interception failed");
    privateWindow->SetInputText("/REPORT");Require(privateWindow->SubmitInput(),"private source parser report interception failed");
    privateWindow->SetInputText("/q");Require(privateWindow->SubmitInput() && g_windowMgr.m_whisperWindows.find("PrivateScope")==g_windowMgr.m_whisperWindows.end()
        && g_windowMgr.m_chatRoomWnd==room,"private quit closed the room or retained its private owner");
    for(const auto& entry:g_session.m_cmdOnOffSet)Require(entry.first==0,"private special command incorrectly entered mode47 prelude");
    {
        CWorld world;CPlayer player;world.m_player=&player;mode.m_world=&world;
        struct Detach {CGameMode& mode;CWorld& world;~Detach(){mode.m_world=nullptr;world.m_player=nullptr;}} detach{mode,world};
        player.m_stateId=0;player.m_effectState=0;player.m_isSitting=0;player.m_proceedTargetGid=0x11223344;player.m_targetGid=77;
        const auto stance=[&](int action){const auto frame=peer.ReceiveExact(7);Require((frame[0]|(u16(frame[1])<<8))==profile.actionRequest
            && frame[2]==0x44 && frame[3]==0x33 && frame[4]==0x22 && frame[5]==0x11 && frame[6]==action,"source stance toggle did not forward target/state action");};
        Require(submit("/stand"),"source stand-while-standing toggle failed");stance(2);Require(player.m_targetGid==77,"source stance command cleared an unrelated target binding");
        player.m_stateId=6;Require(submit("/sit"),"source sit-while-seated toggle failed");stance(3);
        player.m_stateId=2;Require(!submit("/sit"),"source stance request accepted a nonidle/nonsitting state");
        player.m_stateId=0;player.m_effectState=2;Require(!submit("/stand"),"source burrow stance guard missing");player.m_effectState=0;
        Require(SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(20))==0,"blocked stance command leaked a packet");
        player.m_roty=0;player.m_curMotion=0;player.m_headDir=0;player.m_stateId=6;submissionTick=18000;
        const u32 times[]={16000,16100,16200,16300,16400,16500};std::copy(std::begin(times),std::end(times),std::begin(mode.m_doritime));
        const auto direction=[&](int head,int body){const bool legacy=ro::net::IsLegacyMapGameplaySendProfile();const auto bytes=peer.ReceiveExact(legacy?5:11);
            const int h=legacy?2:7,d=legacy?4:10;Require((bytes[0]|(u16(bytes[1])<<8))==profile.changeDir && (bytes[h]|(u16(bytes[h+1])<<8))==head && bytes[d]==body,"source head/body direction payload mismatch");};
        Require(submit("/doridori"),"source doridori dispatch failed");direction(2,4);
        Require(peer.ReceiveExact(2)==std::vector<u8>({0xe7,1}) && player.m_curMotion==2 && player.m_headDir==2
            && std::all_of(std::begin(mode.m_doritime),std::end(mode.m_doritime),[](u32 value){return value==0;}),"source doridori head toggle/streak request/reset changed");
        player.m_roty=315;Require(submit("/bingbing"),"source bingbing dispatch failed");direction(2,4);Require(player.m_roty==0,"source bingbing did not wrap its body angle");
    }
    connection->Disconnect();mode.m_isReqEmotion=0;submissionTick=16000;Require(!submit("/!") && mode.m_isReqEmotion && mode.m_reqEmotionTick==16000,"failed emotion send rolled back source pending state");
#include "ChatIgnoreReplay.inc"
#include "FriendGreetingReplay.inc"
    mode.SetSourceChatInputServices(nullptr);
    std::cout<<"{\"case_id\":\"source_chat_submission\",\"contract_id\":\"PK09.21\",\"actual_channel_frames\":true,\"caller_history_before_dispatch\":true,\"manager_buffer\":true,\"source47_11\":true,\"emotion_pending\":true,\"outcome\":\"PASS\"}\n";
}
