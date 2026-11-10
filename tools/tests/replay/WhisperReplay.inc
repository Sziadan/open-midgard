#include "ui/UIWhisperWnd.h"
#include "ui/UIFriendOptionWnd.h"
#include "core/WhisperNames.h"
#include "core/SettingsIni.h"
#include "core/MannerFilter.h"

void WhisperReplay()
{
    SetCase("source_whisper_owners","PK09.21","per-name-source-routing","actual 0097/0098 framing -> per-name windows or typed main fallback -> real 0096 replies");
    g_windowMgr.RemoveAllWindows();CGameMode mode;TestConnection receive;
    struct Restore {
        CMode* current=SkillPacketReplayAccess::Current();int type=SkillPacketReplayAccess::CurrentType();
        ServiceType service=g_serviceType;ServerType server=g_serverType;int folder=g_readFolderFirst,codePage=g_codePage;
        bool base=g_session.m_isShowWhisperWnd,friendWindows=g_session.m_isShowWhisperWnd_Friend,sound=g_session.m_isPlayWhisperOpenSound;
        bool autoMessage=g_session.m_onAutoMSG,save=g_session.m_isSaveChat;
        std::vector<ACTIVE_STATUS_ICON> icons=g_session.GetActiveStatusIcons();
        std::string autoText=g_session.m_strAutoMSG;u32 aid=g_session.m_aid;
        std::vector<u32> admins=s_dwAdminAID;std::vector<std::string> excluded=g_session.m_exNameList;
        std::list<u32> excludedAids=g_session.m_exAidList;std::list<FRIEND_INFO> friends=g_session.m_friendList;
        ro::guild::WindowState guild=g_session.m_guildWindow;ro::MannerFilter filter=ro::GetMannerFilter();
        ro::commerce::State commerce=ro::commerce::state;
        std::array<bool,13> options=g_session.m_battleMsgOnOffSet;
        SubChatWindowInfo geometry=g_windowMgr.m_whisperWindowInfo;std::array<char,25> name{};
        std::filesystem::path cwd=std::filesystem::current_path(),resource;
        bool hadDataDir=std::getenv("OPEN_MIDGARD_DATA_DIR")!=nullptr;
        std::string dataDir=hadDataDir?std::getenv("OPEN_MIDGARD_DATA_DIR"):"";
        Restore(){std::copy(std::begin(g_session.m_playerName),std::end(g_session.m_playerName),name.begin());}
        ~Restore(){
            g_windowMgr.RemoveAllWindows();g_windowMgr.ClearChatEvents();CRagConnection::instance()->Disconnect();SkillPacketReplayAccess::Restore(current,type);
            g_serviceType=service;g_serverType=server;g_readFolderFirst=folder;g_codePage=codePage;
            g_session.m_isShowWhisperWnd=base;g_session.m_isShowWhisperWnd_Friend=friendWindows;g_session.m_isPlayWhisperOpenSound=sound;
            g_session.m_onAutoMSG=autoMessage;g_session.m_isSaveChat=save;g_session.m_strAutoMSG=autoText;g_session.m_aid=aid;
            s_dwAdminAID=std::move(admins);g_session.m_exNameList=std::move(excluded);g_session.m_exAidList=std::move(excludedAids);
            g_session.m_friendList=std::move(friends);g_session.m_guildWindow=std::move(guild);g_session.m_battleMsgOnOffSet=options;
            g_session.SetActiveStatusIcon(107,false,0);
            for(const auto& icon:icons)if(icon.statusType==107){const s32 remaining=static_cast<s32>(icon.expireServerTime-g_session.GetServerTime());
                if(!icon.hasTimer || remaining>0)g_session.SetActiveStatusIcon(107,true,icon.hasTimer?static_cast<u32>(remaining):0);}
            ro::GetMannerFilter()=std::move(filter);g_windowMgr.m_whisperWindowInfo=geometry;
            ro::commerce::state=std::move(commerce);
            std::copy(name.begin(),name.end(),std::begin(g_session.m_playerName));std::filesystem::current_path(cwd);
#if RO_PLATFORM_WINDOWS
            _putenv_s("OPEN_MIDGARD_DATA_DIR",hadDataDir?dataDir.c_str():"");
#else
            if(hadDataDir)setenv("OPEN_MIDGARD_DATA_DIR",dataDir.c_str(),1);else unsetenv("OPEN_MIDGARD_DATA_DIR");
#endif
            if(!resource.empty()){const auto target=std::filesystem::absolute(resource).lexically_normal();
                if(target.parent_path()==cwd && target.filename().string().rfind("whisper-replay-",0)==0){std::error_code error;std::filesystem::remove_all(target,error);}}
        }
    } restore;
    restore.resource=restore.cwd/("whisper-replay-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(restore.resource/"settings");std::filesystem::current_path(restore.resource);
#if RO_PLATFORM_WINDOWS
    Require(_putenv_s("OPEN_MIDGARD_DATA_DIR",(restore.resource/"settings").string().c_str())==0,"isolated whisper settings failed");
#else
    Require(setenv("OPEN_MIDGARD_DATA_DIR",(restore.resource/"settings").string().c_str(),1)==0,"isolated whisper settings failed");
#endif
    const auto language=[&](int empty=-1,const std::string& autoPrefix="[AUTO] "){
        std::ofstream file("msgstringtable.txt",std::ios::binary);
        const std::map<int,std::string> labels={{49,"REPEAT"},{50,"FILTER"},{95,"INVALID"},{102,"FRIEND"},{148,"MISSING"},{149,"BLOCKED"},{150,"BLOCKED ALL"},{190,"ENGLISH"},{312,"SAVE"},{358,"ADD FRIEND"},{917,"FRIEND WARNING"},{919,"GUILD WARNING"},{941,autoPrefix}};
        for(int i=0;i<=941;++i){const auto found=labels.find(i);if(i!=empty && found!=labels.end())file<<found->second;file<<"#\r\n";}
    };
    language();g_readFolderFirst=1;g_codePage=65001;g_serviceType=ServiceAmerica;g_serverType=ServerNormal;
    g_session.m_friendList.clear();g_session.m_guildWindow={};g_session.m_exNameList.clear();g_session.m_exAidList.clear();s_dwAdminAID.clear();
    g_session.m_isShowWhisperWnd=g_session.m_isShowWhisperWnd_Friend=true;g_session.m_isPlayWhisperOpenSound=false;
    g_session.m_onAutoMSG=g_session.m_isSaveChat=false;g_session.SetActiveStatusIcon(107,false,0);g_session.m_aid=0x10203040;
    std::memset(g_session.m_playerName,0,sizeof(g_session.m_playerName));std::memcpy(g_session.m_playerName,"Local",5);
    g_session.m_battleMsgOnOffSet.fill(true);ro::GetMannerFilter().Clear();SkillPacketReplayAccess::SetCurrent(&mode);
    auto& router=GetProductionGameModePacketRouter();router.Clear();RegisterDefaultGameModePacketHandlers(router);
    const auto dispatch=[&](const std::vector<u8>& packet){Require(receive.QueueBytes(packet),"whisper frame queue failed");
        Require(ProcessGameModePacketLoop(mode,receive,router,1).packetsDispatched==1 && receive.BufferedBytes()==0,"whisper frame dispatch failed");};
    const auto incoming=[&](const std::string& sender,const std::string& text){std::vector<u8> packet(29+text.size());packet[0]=0x97;
        packet[2]=u8(packet.size());packet[3]=u8(packet.size()>>8);std::copy_n(sender.begin(),(std::min)(sender.size(),std::size_t(24)),packet.begin()+4);
        std::copy(text.begin(),text.end(),packet.begin()+28);dispatch(packet);};
    const auto last=[&]()->const UIChatEvent&{Require(!g_windowMgr.GetChatEvents().empty(),"missing whisper fallback event");return g_windowMgr.GetChatEvents().back();};
    mode.m_lastWhisperName="Pending";mode.m_lastWhisper="sentinel";g_windowMgr.m_whisperWindowInfo={-1,-1,270,119};
    incoming("Peer","literal <body> ^FF0000");auto* peerWindow=g_windowMgr.m_whisperWindows.at("Peer");
    Require(peerWindow->m_x==0 && peerWindow->m_y==347 && peerWindow->m_w==280 && peerWindow->m_h==120
        && peerWindow->GetRows().back().text=="Peer : literal <body> ^FF0000" && peerWindow->GetRows().back().color==0x222222,
        "source whisper creation/default rectangle/raw text/color changed");
    Require(!g_windowMgr.m_chatWnd && g_windowMgr.GetChatEvents().empty() && mode.m_lastWhisperName=="Pending" && mode.m_lastWhisper=="sentinel",
        "dedicated receive resurrected main or overwrote outgoing pending state");
    peerWindow->SetShow(0);g_session.m_isShowWhisperWnd=g_session.m_isShowWhisperWnd_Friend=false;
    incoming("Peer","hidden delivery");Require(peerWindow->GetRows().back().text=="Peer : hidden delivery" && !peerWindow->m_show && g_windowMgr.GetChatEvents().empty(),"existing hidden whisper was unhidden or fell back");
    incoming("Other","fallback");Require(last().text=="( From Other ) : fallback" && last().color==0xffff00
        && last().source.type==ro::chat::NoticeType::Whisper && last().source.talker=="Other","ordinary fallback lost source format/type/talker/color");
    Require(g_windowMgr.m_chatWnd && g_windowMgr.m_chatWnd->GetWhisperTargets()==std::vector<std::string>({"Other"}),"source recipient history replayed an absent-main target");
    const auto battle=g_windowMgr.m_chatWnd->GetSourceHistory(true).size();g_session.m_battleMsgOnOffSet[1]=false;incoming("Another","normal only");
    Require(g_windowMgr.m_chatWnd->GetSourceHistory(true).size()==battle && last().source.type==ro::chat::NoticeType::Whisper,"fallback bypassed source type2 filter");
    g_session.m_battleMsgOnOffSet[1]=true;
    const auto beforeSelf=g_windowMgr.GetChatEvents().size();incoming("Local","self fallback");Require(g_windowMgr.GetChatEvents().size()==beforeSelf,"self fallback emitted main notice");
    g_session.m_isShowWhisperWnd=g_session.m_isShowWhisperWnd_Friend=true;incoming("Local","self creation");
    Require(g_windowMgr.m_whisperWindows.count("Local")==1 && g_windowMgr.m_whisperWindows.at("Local")->GetRows().empty(),"source self window creation-before-self-check changed");
    FRIEND_INFO friendInfo;friendInfo.characterName="Buddy";g_session.m_friendList.push_back(friendInfo);incoming("Buddy","friend");
    Require(g_windowMgr.m_whisperWindows.at("Buddy")->GetTitle()=="Buddy  (FRIEND)  *^_^*","friend window title/source label changed");
    incoming("buddy","case distinct");Require(g_windowMgr.m_whisperWindows.at("buddy")->GetTitle()=="buddy" && !g_session.IsFriendName("buddy"),"exact friend/map name identity became case folding");
    g_session.m_guildWindow.members.push_back({});g_session.m_guildWindow.members.back().name="Guildmate";g_session.m_guildWindow.master="Leader";
    incoming("Guildmate","guild");incoming("Leader","master");Require(g_windowMgr.m_whisperWindows.at("Guildmate")->GetTitle()=="Guildmate  (GuildMember)  *^_^*"
        && g_windowMgr.m_whisperWindows.at("Leader")->GetTitle()=="Leader  (GuildMember)  *^_^*","guild member/master classification changed");
    incoming("Guildmatx","lookalike");Require(g_windowMgr.m_whisperWindows.at("Guildmatx")->GetTitle()=="Guildmatx" && last().text=="GUILD WARNING","guild lookalike warning became membership/drop");
    g_session.m_isShowWhisperWnd=g_session.m_isShowWhisperWnd_Friend=false;g_windowMgr.ClearChatEvents();incoming("xBuddy","lookalike fallback");
    Require(g_windowMgr.GetChatEvents().size()==3 && g_windowMgr.GetChatEvents()[0].text=="FRIEND WARNING"
        && g_windowMgr.GetChatEvents()[1].source.route==ro::chat::NoticeRoute::Command && last().text=="( From xBuddy ) : lookalike fallback","source warning double-route/fallback order changed");
    language(102);FRIEND_INFO emptyLabel;emptyLabel.characterName="FriendZero";g_session.m_friendList.push_back(emptyLabel);
    g_session.m_isShowWhisperWnd_Friend=true;incoming("FriendZero","empty label");Require(g_windowMgr.m_whisperWindows.at("FriendZero")->GetTitle()=="FriendZero  ()  *^_^*","known-empty friend label used fallback or omitted decoration");language();
    auto* connection=CRagConnection::instance();connection->Disconnect();LoopbackPeer peer(*connection);
    const auto expectSend=[&](const std::string& target,const std::string& text){std::vector<u8> expected(29+text.size());expected[0]=0x96;
        expected[2]=u8(expected.size());expected[3]=u8(expected.size()>>8);std::copy(target.begin(),target.end(),expected.begin()+4);std::copy(text.begin(),text.end(),expected.begin()+28);
        Require(peer.ReceiveExact(expected.size())==expected,"actual whisper request differs from independent 0096 frame");};
    g_session.m_isShowWhisperWnd=true;Require(mode.SendSourceWhisper("Outbox","outbound"),"source outgoing whisper rejected");expectSend("Outbox","outbound");
    Require(!g_windowMgr.m_whisperWindows.count("Outbox"),"send manufactured an acknowledged owner");incoming("Interleaved","incoming");
    Require(mode.m_lastWhisperName=="Outbox" && mode.m_lastWhisper=="outbound","incoming message replaced pending outgoing acknowledgement metadata");
    dispatch({0x98,0,0});auto* outbox=g_windowMgr.m_whisperWindows.at("Outbox");Require(outbox->GetRows().back().text=="Local : outbound" && outbox->GetRows().back().color==0x007800,"success acknowledgement did not reach per-name history with source own color");
    dispatch({0x98,0,1});Require(outbox->GetRows().back().text=="(Outbox)MISSING","missing-character acknowledgement source format changed");
    mode.m_hiddenWhisperAcknowledgements=1;const auto rows=outbox->GetRows().size();dispatch({0x98,0,1});
    Require(outbox->GetRows().size()==rows && mode.m_hiddenWhisperAcknowledgements==0,"case1 hidden acknowledgement guard changed");
    mode.m_hiddenWhisperAcknowledgements=1;dispatch({0x98,0,2});Require(outbox->GetRows().back().text=="(Outbox)BLOCKED" && mode.m_hiddenWhisperAcknowledgements==1,"case2 incorrectly consumed hidden count");
    dispatch({0x98,0,3});Require(outbox->GetRows().back().text=="(Outbox)BLOCKED ALL","case3 acknowledgement changed");mode.m_hiddenWhisperAcknowledgements=0;
    const auto outboxRows=outbox->GetRows().size();outbox->SetInputText("from editor");
    Require(outbox->SubmitInput() && outbox->GetInputText().empty() && outbox->GetRows().size()==outboxRows,"dedicated editor optimistically echoed or retained submitted draft");expectSend("Outbox","from editor");
    dispatch({0x98,0,0});Require(outbox->GetRows().back().text=="Local : from editor","dedicated editor acknowledgement did not echo authoritatively");
    outbox->SetInputText(u8"é");Require(!outbox->SubmitInput() && outbox->GetInputText().empty() && outbox->GetInputHistory().back()==u8"é" && last().text=="ENGLISH","America rejection changed clear/history/notice timing");
    outbox->SetInputText(std::string(69,'a')+u8"é");Require(outbox->GetInputText()==std::string(69,'a'),"70-byte editor split a UTF-8 boundary");
    for(int i=0;i<31;++i){const auto text="history-"+std::to_string(i);outbox->SetInputText(text);Require(outbox->SubmitInput(),"history fixture send failed");expectSend("Outbox",text);}
    Require(outbox->GetInputHistory().size()==30 && outbox->GetInputHistory().front()=="history-1" && outbox->GetInputHistory().back()=="history-30","dedicated edit history cap/tail changed");
    outbox->OnKeyDown(VK_UP);Require(outbox->GetInputText()=="history-30","dedicated edit Up did not browse last submitted input");
    outbox->OnKeyDown(VK_UP);outbox->SetInputText("replacement");Require(outbox->SubmitInput(),"browsed history replacement dispatch failed");expectSend("Outbox","replacement");
    Require(outbox->GetInputHistory().size()==30 && outbox->GetInputHistory()[28]=="replacement" && outbox->GetInputText()=="history-30","source browsed entry was appended/cleared instead of replaced and advanced");
    outbox->OnKeyDown(VK_DOWN);Require(outbox->GetInputText().empty(),"dedicated edit Down did not restore empty history tail");
    outbox->SetInputText("modal draft");g_windowMgr.m_editWindow=outbox;g_windowMgr.ShowRoomValidationMessage(13);
    incoming("UnderModal","arrival");g_windowMgr.OnChar('z');
    Require(g_windowMgr.m_roomMessageBox && g_windowMgr.m_editWindow==nullptr && outbox->GetInputText()=="modal draft"
        && g_windowMgr.m_whisperWindows.at("UnderModal")->GetInputText().empty(),"incoming whisper stole modal input/focus");
    g_windowMgr.DeleteWindow(outbox);g_windowMgr.OnKeyDown(VK_RETURN);
    Require(!g_windowMgr.m_modalWindow && !g_windowMgr.m_editWindow,"modal restored a deleted whisper editor");
    g_session.m_isShowWhisperWnd=g_session.m_isShowWhisperWnd_Friend=false;
    mode.m_lastWhisperName="FallbackAck";mode.m_lastWhisper="echo";dispatch({0x98,0,0});
    Require(last().text=="( To FallbackAck ) : echo" && last().source.type==ro::chat::NoticeType::Whisper && last().source.talker=="FallbackAck","ordinary success fallback format/type/target changed");
    FRIEND_INFO globalFriend;globalFriend.characterName="GlobalFriend";g_session.m_friendList.push_back(globalFriend);
    mode.m_lastWhisperName="GlobalFriend";mode.m_lastWhisper="echo";dispatch({0x98,0,0});Require(last().text=="[ To GlobalFriend ] : echo","friend success fallback lost selected bracket prefix");
    mode.m_hiddenWhisperAcknowledgements=1;const auto hiddenEvents=g_windowMgr.GetChatEvents().size();dispatch({0x98,0,0});
    Require(mode.m_hiddenWhisperAcknowledgements==0 && g_windowMgr.GetChatEvents().size()==hiddenEvents,"hidden success fallback was not consumed once");
    mode.m_lastWhisperName="Peer";mode.m_hiddenWhisperAcknowledgements=1;dispatch({0x98,0,0});
    Require(mode.m_hiddenWhisperAcknowledgements==1 && peerWindow->GetRows().back().text=="Local : echo","existing hidden owner did not precede success hidden-counter guard");mode.m_hiddenWhisperAcknowledgements=0;
    language(149);mode.m_lastWhisperName="NoOwner";g_windowMgr.m_chatWnd->SetState(0);dispatch({0x98,0,2});
    Require(last().text=="(NoOwner)" && last().source.talker.empty() && g_windowMgr.m_chatWnd->GetExpansionState()==0,"known-empty error label or no-expansion fallback changed");language();
    mode.m_sourceLastSubmittedChat.clear();mode.m_sourceSameChatRepeatCount=0;
    Require(mode.SendSourceWhisper("Repeat","same"),"first repeated whisper rejected");expectSend("Repeat","same");incoming("Between","receive between sends");
    Require(mode.SendSourceWhisper("Repeat","same"),"second identical whisper rejected");expectSend("Repeat","same");
    Require(!mode.SendSourceWhisper("Repeat","same") && last().text=="REPEAT","third identical source whisper escaped rate policy");
    g_serviceType=ServiceGermany;Require(mode.SendSourceWhisper("Repeat","same"),"Germany repeat exception lost");expectSend("Repeat","same");
    g_serviceType=ServiceAmerica;g_serverType=ServerInstantEvent;Require(mode.SendSourceWhisper("Repeat","same"),"instant-event repeat exception lost");expectSend("Repeat","same");g_serverType=ServerNormal;
    g_session.m_onAutoMSG=true;Require(mode.SendSourceWhisper("Repeat","same"),"auto-message repeat exception lost");expectSend("Repeat","same");g_session.m_onAutoMSG=false;
    g_session.SetActiveStatusIcon(107,true,0);const auto pending=mode.m_lastWhisperName;
    Require(!mode.SendSourceWhisper("Blocked","berserk") && mode.m_lastWhisperName==pending,"Berserk107 did not stop source whisper before pending update");g_session.SetActiveStatusIcon(107,false,0);
    ro::GetMannerFilter().AppendEncodedFile("bad\r\n",65001);Require(!mode.SendSourceWhisper("Filtered","bad") && last().text=="FILTER","source whisper insult filter was bypassed");
    g_session.m_isShowWhisperWnd=true;g_session.m_onAutoMSG=true;g_session.m_strAutoMSG="Away";incoming("ReplyPeer","hello");expectSend("ReplyPeer","[AUTO] Away");
    incoming("ReplyPeer","[AUTO] already automatic");Require(SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(20))==0,"auto-prefix loop guard emitted reply");
    language(941);incoming("ReplyPeer","empty auto-prefix");Require(SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(20))==0,"known-empty auto prefix did not suppress reply");language();g_session.m_onAutoMSG=false;
    const std::string target24(24,'N'),body221(221,'x');Require(mode.SendSourceWhisper(target24,body221),"maximum source body/name rejected");expectSend(target24,body221);
    Require(!mode.SendSourceWhisper("TooLong",std::string(222,'x')) && mode.m_lastWhisper.size()==222,"source222-byte body rejection/pending order changed");
    Require(ro::commerce::PlayerAction(CGameMode::PlayerContext_RejectWhispering,0x55667788,"Muted"),"actual reject-whisper action failed");
    const auto exclude=peer.ReceiveExact(27);Require(exclude[0]==0xcf && exclude[26]==0 && g_session.IsExName("Muted"),"source local exclusion not updated after action");
    const auto events=g_windowMgr.GetChatEvents().size();incoming("Muted","must drop");Require(!g_windowMgr.m_whisperWindows.count("Muted") && g_windowMgr.GetChatEvents().size()==events,"excluded sender reached window/main");
    Require(ro::commerce::PlayerAction(CGameMode::PlayerContext_AllowWhispering,0x55667788,"Muted"),"actual allow-whisper action failed");
    const auto include=peer.ReceiveExact(27);Require(include[0]==0xcf && include[26]==1 && !g_session.IsExName("Muted"),"source local include did not erase exact name");incoming("Muted","allowed");
    Require(g_windowMgr.m_whisperWindows.count("Muted")==1,"allowed sender remained excluded");
    Require(!mode.ToggleSourceChatAutoSave() && !g_session.m_isSaveChat && last().text=="INVALID","non-admin autosave guard changed");s_dwAdminAID.push_back(g_session.m_aid);
    Require(mode.ToggleSourceChatAutoSave() && g_session.m_isSaveChat,"privileged autosave failed to enable");const auto log=std::filesystem::u8path(mode.GetSourceAutoSaveChatPath());
    g_session.m_isShowWhisperWnd=g_session.m_isShowWhisperWnd_Friend=false;incoming("LogOnly","One\nTwo");
    std::ifstream file(log,std::ios::binary);const std::string saved((std::istreambuf_iterator<char>(file)),{});file.close();
    const std::string expectedLog="( From LogOnly ) : One\nTwo"+std::string("\0\r\n",3);
    Require(saved==expectedLog && mode.m_sourceAutoSaveChatCount==1,"source autosave did not retain embedded LF/NUL/CRLF bytes");
    g_session.m_isShowWhisperWnd=true;incoming("DedicatedLogSkip","no global save");Require(mode.m_sourceAutoSaveChatCount==1,"dedicated whisper incorrectly entered autosave fallback");
    mode.m_lastWhisperName="DedicatedLogSkip";mode.m_hiddenWhisperAcknowledgements=1;dispatch({0x98,0,5});
    Require(mode.m_sourceAutoSaveChatCount==2 && mode.m_hiddenWhisperAcknowledgements==1,"default acknowledgement lost independent autosave/hidden-counter behavior");mode.m_hiddenWhisperAcknowledgements=0;
    std::ifstream appended(log,std::ios::binary);const std::string all((std::istreambuf_iterator<char>(appended)),{});appended.close();
    Require(all==expectedLog+"(DedicatedLogSkip)MISSING"+std::string("\0\r\n",3),"default acknowledgement autosave bytes changed");
    Require(mode.ToggleSourceChatAutoSave() && !g_session.m_isSaveChat,"source autosave failed to disable");
    Require(mode.ToggleSourceChatAutoSave() && std::filesystem::u8path(mode.GetSourceAutoSaveChatPath()).filename()=="Chat001.txt","autosave collision did not choose first unused three-digit suffix");
    Require(mode.ToggleSourceChatAutoSave() && !g_session.m_isSaveChat,"second autosave disable failed");
    connection->Disconnect();Require(!mode.SendSourceWhisper("Failed","failed send") && mode.m_lastWhisperName=="Failed" && mode.m_lastWhisper=="failed send","failed immediate send lost source pending metadata");
    Require(!ro::commerce::PlayerAction(CGameMode::PlayerContext_RejectWhispering,0x77889900,"OfflineMuted") && g_session.IsExName("OfflineMuted"),"failed immediate ignore send incorrectly rolled back source local exclusion");
    dispatch({0xd1,0,0,1});Require(g_session.IsExName("OfflineMuted"),"failed ignore acknowledgement incorrectly removed local exclusion");
    g_windowMgr.DeleteWindow(g_windowMgr.m_chatWnd);Require(!g_windowMgr.m_chatWnd,"main removal failed");incoming("Peer","still dedicated");
    Require(!g_windowMgr.m_chatWnd && peerWindow->GetRows().back().text=="Peer : still dedicated","retained dedicated owner required or recreated main");
    const auto geometry=peerWindow->GetLayout();peerWindow->Resize(312,152);Require(peerWindow->GetLayout().history.right==geometry.history.right,"generic resize repaired source children");
    peerWindow->ResizeSource(312,152);Require(peerWindow->GetLayout().history.right==geometry.history.right,"unchanged source size repaired stale children");
    peerWindow->ResizeSource(344,184);Require(peerWindow->GetLayout().history.right==343,"changed source resize did not update children");
    peerWindow->ResizeSource(999,999);Require(peerWindow->m_w==504 && peerWindow->m_h==376,"source whisper upper clamp/quantizer changed");
    peerWindow->ResizeSource(-1,-1);Require(peerWindow->m_w==280 && peerWindow->m_h==120,"source whisper lower clamp changed");
    g_windowMgr.OpenWhisperWindow("History");auto* history=g_windowMgr.m_whisperWindows.at("History");
    for(int i=0;i<240;++i)history->Append(("row-"+std::to_string(i)).c_str(),0);
    history->ScrollHistory(-35);Require(history->GetFirst()==200,"history fixture did not establish source off-bottom offset");
    history->Append("after-trim",0);Require(history->GetRows().size()==121 && history->GetFirst()==200 && history->GetVisibleRows().empty()
        && history->GetScrollModel().position==115,"source whisper trim incorrectly clamped the text offset with the thumb");
    history->ScrollHistory(-1);Require(history->GetFirst()==116 && !history->GetVisibleRows().empty(),"source whisper trim did not reconcile on next scroll");
    // Real private menu -> mode114/current mode26 -> exact-name lookup/send.
    g_serverType=ServerNormal;g_session.m_isSaveChat=false;g_session.m_onAutoMSG=false;
    g_windowMgr.OpenWhisperWindow("MenuPeer");auto* menuOwner=g_windowMgr.m_whisperWindows.at("MenuPeer");
    menuOwner->Append("alpha\nbeta",0);menuOwner->Append("^FF0000 <raw>",0);
    menuOwner->OpenMenu(790,590);
    Require(menuOwner->GetMenuItems().size()==2 && menuOwner->GetMenuItems()[0].action==11
        && menuOwner->GetMenuItems()[0].label=="SAVE" && menuOwner->GetMenuItems()[1].action==21
        && mode.m_menuIdList==std::vector<int>({11,21}) && mode.m_lastWhisperMenuCharacterName=="MenuPeer",
        "private menu lost source labels/order or mode114 state");
    auto* popup=g_windowMgr.m_chatRoomMenuWnd;
    Require(popup && popup->m_x+popup->m_w<=g_windowMgr.GetChatClientRect().right
        && popup->m_y+popup->m_h<=g_windowMgr.GetChatClientRect().bottom,"private source menu escaped viewport");
    popup->OnLBtnUp(-1,-1);Require(g_windowMgr.m_chatRoomMenuWnd==popup,"private menu outside release unexpectedly closed");
    popup->OnLBtnUp(popup->m_x+2,popup->m_y+2);
    const auto whisperDir=GetOpenMidgardIniPath().parent_path()/"Whisper";
    const auto readFile=[](const std::filesystem::path& path){std::ifstream file(path,std::ios::binary);return std::string((std::istreambuf_iterator<char>(file)),{});};
    Require(!g_windowMgr.m_chatRoomMenuWnd && readFile(whisperDir/"MenuPeer.txt")=="alpha\r\nbeta\r\n^FF0000 <raw>\r\n"
        && last().text==(whisperDir/"MenuPeer.txt").u8string()+" is Saved." && last().color==0x00ff00,
        "private save mode26/48 lost exact rows/newlines/notice");
    menuOwner->SetInputText("/savechat ignored");Require(menuOwner->SubmitInput()
        && readFile(whisperDir/"MenuPeer001.txt")=="alpha\r\nbeta\r\n^FF0000 <raw>\r\n"
        && menuOwner->GetInputText().empty(),"private save command/collision/history handling changed");
    menuOwner->SetInputText("/rkfanfl");Require(menuOwner->SubmitInput() && std::filesystem::exists(whisperDir/"MenuPeer002.txt"),"source keyboard save alias failed");
    g_serviceType=ServiceKorea;menuOwner->SetInputText(u8"/갈무리");Require(menuOwner->SubmitInput()
        && std::filesystem::exists(whisperDir/"MenuPeer003.txt"),"source Korean save alias failed");g_serviceType=ServiceAmerica;
    const auto reportDir=GetOpenMidgardIniPath().parent_path()/"Report";
    std::filesystem::create_directories(reportDir);{std::ofstream report(reportDir/"Local.txt");report<<"untouched";}
    menuOwner->SetInputText("/report");Require(menuOwner->SubmitInput() && !std::filesystem::exists(reportDir/"Local001.txt")
        && readFile(reportDir/"Local.txt")=="untouched" && last().text==(reportDir/"Local001.txt").u8string()+" is Saved.",
        "selected report no-op/file-name/notice behavior changed");
    menuOwner->SetInputText("/tlsrh");Require(menuOwner->SubmitInput() && !std::filesystem::exists(reportDir/"Local001.txt"),"source report keyboard alias created content");
    g_serviceType=ServiceKorea;menuOwner->SetInputText(u8"/신고");Require(menuOwner->SubmitInput(),"source Korean report alias failed");g_serviceType=ServiceAmerica;
    g_windowMgr.OpenWhisperWindow("A/B:C*D?E\"F<G>H|I\\J");auto* unsafe=g_windowMgr.m_whisperWindows.at("A/B:C*D?E\"F<G>H|I\\J");
    unsafe->Append("sanitized",0);Require(unsafe->WriteChatToFile() && readFile(whisperDir/"A_B_C_D_E_F_G_H_I_J.txt")=="sanitized\r\n","private export filename sanitization failed");
    g_windowMgr.OpenWhisperWindow("CannotWrite");auto* cannot=g_windowMgr.m_whisperWindows.at("CannotWrite");
    {std::ofstream blocked(whisperDir/"CannotWrite.txt");blocked<<"collision";}
    const auto blockedBase=GetOpenMidgardIniPath().parent_path()/"blocked-root";{std::ofstream blocked(blockedBase);blocked<<"file";}
    const auto settingsRoot=GetOpenMidgardIniPath().parent_path();
#if RO_PLATFORM_WINDOWS
    _putenv_s("OPEN_MIDGARD_DATA_DIR",blockedBase.string().c_str());
#else
    setenv("OPEN_MIDGARD_DATA_DIR",blockedBase.string().c_str(),1);
#endif
    const auto beforeFailure=g_windowMgr.GetChatEvents().size();Require(!cannot->WriteChatToFile() && g_windowMgr.GetChatEvents().size()==beforeFailure,"failed private directory creation emitted success notice");
#if RO_PLATFORM_WINDOWS
    _putenv_s("OPEN_MIDGARD_DATA_DIR",settingsRoot.string().c_str());
#else
    setenv("OPEN_MIDGARD_DATA_DIR",settingsRoot.string().c_str(),1);
#endif
    g_serverType=ServerInstantEvent;menuOwner->OpenMenu(5,5);Require(menuOwner->GetMenuItems().size()==1 && mode.m_menuIdList.size()==2,"instant-event private friend suppression altered mode114 list");
    g_serverType=ServerNormal;FRIEND_INFO menuFriend;menuFriend.characterName="MenuPeer";g_session.m_friendList.push_back(menuFriend);
    menuOwner->OpenMenu(5,5);Require(menuOwner->GetMenuItems().size()==1,"private friend menu ignored exact membership");
    g_session.m_friendList.pop_back();g_windowMgr.m_whisperWindows.at("Local")->OpenMenu(5,5);
    Require(g_windowMgr.m_whisperWindows.at("Local")->GetMenuItems().size()==2,"private self menu invented friend suppression");
    language(312);menuOwner->OpenMenu(5,5);Require(menuOwner->GetMenuItems()[0].label.empty(),"private menu replaced known-empty save label");language();
    {
        LoopbackPeer friendPeer(*connection);menuOwner->OpenMenu(5,5);popup=g_windowMgr.m_chatRoomMenuWnd;
        popup->OnLBtnUp(popup->m_x+2,popup->m_y+16);
        std::vector<u8> expected(26);expected[0]=2;expected[1]=2;std::copy_n("MenuPeer",8,expected.begin()+2);
        Require(friendPeer.ReceiveExact(26)==expected && !g_windowMgr.m_chatRoomMenuWnd && !g_session.IsFriendName("MenuPeer"),"private friend mode26 changed0202 bytes or mutated roster");
        const std::string fullName(24,'F');mode.SendMsg(114,reinterpret_cast<msgparam_t>(fullName.c_str()),0,0);
        Require(mode.SendMsg(26,1,0,0)!=0,"source name-only friend action incorrectly required actor ID or rejected24 bytes");
        expected.assign(26,'F');expected[0]=2;expected[1]=2;Require(friendPeer.ReceiveExact(26)==expected,"source friend24 field acquired terminator or truncation");
    }
    menuOwner->OpenMenu(5,5);popup=g_windowMgr.m_chatRoomMenuWnd;
    auto* storage=g_windowMgr.MakeWindow(UIWindowMgr::WID_STORAGEWND);Require(storage,"storage guard fixture failed");
    Require(mode.SendMsg(26,0,0,0)==0 && !g_windowMgr.m_chatRoomMenuWnd && !std::filesystem::exists(whisperDir/"MenuPeer004.txt"),"source storage-owner menu guard was bypassed");
    g_windowMgr.DeleteWindow(storage);menuOwner->OpenMenu(5,5);popup=g_windowMgr.m_chatRoomMenuWnd;
    g_windowMgr.DeleteWindow(menuOwner);Require(g_windowMgr.m_chatRoomMenuWnd==popup,"private popup was incorrectly owned by deleted whisper");
    popup->OnLBtnUp(popup->m_x+2,popup->m_y+2);Require(!g_windowMgr.m_chatRoomMenuWnd && !std::filesystem::exists(whisperDir/"MenuPeer004.txt"),"stale private menu wrote through dead owner");
    g_windowMgr.OpenWhisperWindow("MenuPeer");Require(!g_windowMgr.WriteWhisperChatToFile("MenuPeer"),"replacement owner inherited stale menu items");
    Require(mode.SendMsg(26,-1,0,0)==0 && mode.SendMsg(26,2,0,0)==0,"source menu invalid indices were not bounded");
    g_windowMgr.RemoveAllWindows();Require(g_windowMgr.m_whisperWindows.empty(),"bulk teardown retained per-name owners");
    std::cout<<"{\"case_id\":\"source_whisper_owners\",\"contract_id\":\"PK09.21\",\"actual_0097_0098_0096\":true,\"per_name_hidden_lifetime\":true,\"typed_fallback\":true,\"pending_not_overwritten\":true,\"source_guards\":true,\"auto_prefix\":true,\"privileged_autosave_bytes\":true,\"outcome\":\"PASS\"}\n";
}

void WhisperPreferencesReplay(const std::string& action)
{
    SetCase("whisper_preferences_"+action,"PK09.21","whisper-startup-options-geometry","actual saved Option/UIWindows records -> fresh ReadRegistry -> per-name restore");
    Require(std::getenv("OPEN_MIDGARD_DATA_DIR")!=nullptr,"whisper preference harness requires an isolated settings directory");
    g_windowMgr.RemoveAllWindows();
    if(action=="write"){
        g_session.m_isShowWhisperWnd=g_session.m_isShowWhisperWnd_Friend=g_session.m_isPlayWhisperOpenSound=false;
        Require(g_session.SaveWhisperOptions(),"source whisper preference writer failed");
        g_windowMgr.m_whisperWindowInfo={13,17,311,151};g_windowMgr.m_friendOptionWindowInfo={29,31,280,120};g_windowMgr.SaveChatWindowPreferences();
    }else if(action=="read"){
        g_session.m_isShowWhisperWnd=g_session.m_isShowWhisperWnd_Friend=g_session.m_isPlayWhisperOpenSound=true;
        ReadRegistry();Require(!g_session.m_isShowWhisperWnd && !g_session.m_isShowWhisperWnd_Friend && !g_session.m_isPlayWhisperOpenSound,"fresh startup did not load source whisper options");
        const auto& info=g_windowMgr.m_whisperWindowInfo;
        Require(info.x==13 && info.y==17 && info.width==311 && info.height==151,"fresh startup did not load shared whisper geometry");
        Require(g_windowMgr.OpenWhisperWindow("Restored"),"saved whisper owner did not open");auto* window=g_windowMgr.m_whisperWindows.at("Restored");
        Require(window->m_x==13 && window->m_y==17 && window->m_w==280 && window->m_h==120 && info.width==311 && info.height==151,"source whisper cached/raw size and quantized window order changed");
        g_windowMgr.DeleteWindow(window);Require(g_windowMgr.m_whisperWindows.empty() && LoadSettingsIniInt("UIWindows","WhisperWndW",0)==311,"individual deletion leaked name map or prematurely saved geometry");
        auto* options=static_cast<UIFriendOptionWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_FRIENDOPTIONWND));
        Require(options && options->m_x==29 && options->m_y==31 && options->m_w==280 && options->m_h==120,"fresh friend-options geometry did not restore");
        options->Move(41,43);g_windowMgr.DeleteWindow(options);
        Require(g_windowMgr.m_friendOptionWindowInfo.x==41 && LoadSettingsIniInt("UIWindows","FriendOptionWndX",-1)==29,"friend-options close failed to snapshot or wrote geometry immediately");
    }else {
        const auto path=GetOpenMidgardIniPath();std::error_code error;std::filesystem::remove(path,error);
        g_session.m_isShowWhisperWnd=g_session.m_isShowWhisperWnd_Friend=g_session.m_isPlayWhisperOpenSound=false;
        ReadRegistry();Require(g_session.m_isShowWhisperWnd && g_session.m_isShowWhisperWnd_Friend && g_session.m_isPlayWhisperOpenSound,"missing whisper preferences did not default enabled");
        Require(g_windowMgr.m_friendOptionWindowInfo.x==200 && g_windowMgr.m_friendOptionWindowInfo.y==120 && g_windowMgr.m_friendOptionWindowInfo.width==280 && g_windowMgr.m_friendOptionWindowInfo.height==120,"missing friend-options geometry default changed");
        SaveSettingsIniInt("Option","m_isShowWhisperWnd",0);SaveSettingsIniInt("Option","m_isShowWhisperWnd_Friend",1);SaveSettingsIniInt("Option","m_isPlayWhisperOpenSound",0);
        ReadRegistry();Require(g_session.m_isShowWhisperWnd && g_session.m_isShowWhisperWnd_Friend && !g_session.m_isPlayWhisperOpenSound,"source friend preference did not force base enabled");
        Require(LoadSettingsIniInt("Option","m_isShowWhisperWnd",-1)==0,"startup normalization wrote preferences immediately");
    }
    std::cout<<"{\"case_id\":\"whisper_preferences_"<<action<<"\",\"contract_id\":\"PK09.21\",\"outcome\":\"PASS\"}\n";
}
