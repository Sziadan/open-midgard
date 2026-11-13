#include "ui/UIWhisperWnd.h"
#include "ui/UIChatRoomWnd.h"
#include "ui/UIOptionWnd.h"
#include "core/MannerFilter.h"
#include "core/SourceLanguageCharset.h"
#include "qtui/QtUiRuntime.h"
#include "world/Weather.h"
#include "core/Timer.h"
#include "core/TipOfTheDay.h"
#include "ui/UITipOfTheDayWnd.h"
#include "ui/UICommandListWnd.h"
#include "ui/UITransBalloonText.h"
#include "ui/UIPartyOptionWnd.h"
#include "ui/UIBasicInfoWnd.h"
#include "ui/UIPetInfoWnd.h"
#include "ui/UIPetEggListWnd.h"
#include "ui/UIPetTamingDeceiveWnd.h"
#include "ui/UISelectCartWnd.h"
#include "skill/Skill.h"
#include "core/SourceWeight.h"
#include "core/SourceCommandListTables.h"
bool ReplayTryPetCaptureOnActor(CGameMode&,CGameActor*,bool,bool,bool);
bool ReplayTrySourceSkillOnActor(CGameMode&,CGameActor*,bool,bool,bool);
void ReplayPumpSkillChase(CGameMode&);
void ReplayClearSkillChase(CGameMode&);

namespace {
// Golden selected /help order, independently retained from57AB1B..57B3CD.
constexpr int expectedSourceHelpIds[]={177,33,34,35,36,37,38,39,40,41,42,43,44,45,46,107,132,133,134,135,
    137,138,139,48,108,140,141,142,154,155,156,157,158,173,211,275,313,757,321,349,736,852,907,727,728,
    710,715,771,735,724,767,784,918,804,787,788,795,796,924,923,849,729,732,700,716,770,1043,1107,925,1191};
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
        {175,"Did you know?"},{176,"Show tips at startup"},{187,"NOWHERE"},{294,"ONLY PARTY MASTER"},
        {306,"WEIGHT NO RECOVER"},{307,"WEIGHT NO ATTACK"},{308,"WEIGHT RECOVER RESTORED"},{309,"WEIGHT ATTACK RESTORED"},
        {23,"EFFECT ON"},{24,"EFFECT OFF"},{27,"T27"},{28,"T28"},{31,"T31"},{32,"T32"},{317,"T317"},{318,"T318"},{701,"T701"},{702,"T702"},
        {711,"T711"},{712,"T712"},{717,""},{718,"T718"},{722,"T722"},{723,"T723"},{730,"T730"},{731,"T731"},
        {733,"T733"},{734,"T734"},{765,"T765"},{766,"T766"},{768,"T768"},{769,"T769"},{785,"T785"},{786,"T786"},
        {790,"First prayer line"},{793,"Last prayer line"},
        {805,"T805"},{806,"T806"},{850,"T850"},{851,"T851"},{908,"T908"},{909,"T909"},{1044,"T1044"},{1045,"T1045"},{1108,"T1108"},{1109,"T1109"},
        {687,"MIN ON"},{688,"MIN OFF"},{698,"/tiny"},{940,"Default"},{943,"Auto off"},{944,"Configure"},
        {407,"Name"},{408,"Level"},{584,"Pet Info"},{585,"Hunger"},{586,"Intimacy"},
        {592,"Feed"},{593,"Performance"},{594,"Return Egg"},{595,"Accessory Off"},{596,"Pet Info"},{597,"Accessory"},{598,"Equipped"},{600,"Unequipped"},{601,"Feed now?"},
        {667,"Very hungry"},{668,"Hungry"},{669,"Normal"},{670,"Full"},{671,"Very full"},{672,"Very awkward"},{673,"Awkward"},{674,"Friendly"},{675,"Very friendly"},{676,"Unknown"},
        {682,"Name too long"},{683,"Rename to %s once?"},{1021,"Hate"},{1022,"Very hate"},
        {1365,"BATTLE CHAT ON"},{1366,"BATTLE CHAT OFF"}};
        for(int i=0;i<=1366;++i){const auto found=labels.find(i);if(found!=labels.end())file<<found->second;
            else if(std::find(std::begin(expectedSourceHelpIds),std::end(expectedSourceHelpIds),i)!=std::end(expectedSourceHelpIds)
                    || i==921 || i==922 || i==760 || i==789)file<<"HELP-"<<i;
            file<<"#\r\n";}}
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
    auto* connection=CRagConnection::instance();connection->Disconnect();
    const bool previousBookingPending = g_session.m_partyBookingRegistrationPending;
    g_session.m_partyBookingRegistrationPending = false;
    Require(!submit("/partybooking register 90 513") && !g_session.m_partyBookingRegistrationPending
        && g_windowMgr.GetChatEvents().back().text == "Could not send the party booking registration.",
        "local booking send failure left a pending request");
    LoopbackPeer peer(*connection);const auto& profile=ro::net::GetActiveMapGameplaySendProfile();
    Require(profile.id==ro::net::MapGameplaySendProfileId::PacketVer23 && profile.globalMessage==0x00f3 && profile.whisper==0x0096
        && profile.partyMessage==0x0108 && profile.guildMessage==0x017e && profile.battleMessage==0x02db
        && profile.characterNameRequest==0x00a2 && profile.characterNameRequestLength==15 && profile.characterNameRequestIdOffset==11,
        "selected source chat profile IDs or preserved name-request layout differ from primary/server registration");
    TestConnection bookingInput;
    CGameModePacketRouter bookingRouter;
    RegisterDefaultGameModePacketHandlers(bookingRouter);
    const auto bookingResult = [&](u16 result) {
        Require(bookingInput.QueueBytes({3, 8, u8(result), u8(result >> 8)})
            && ProcessGameModePacketLoop(mode, bookingInput, bookingRouter, 1).packetsDispatched == 1
            && !bookingInput.BufferedBytes(), "booking registration result did not frame and dispatch");
    };
    const auto chatCount = g_windowMgr.GetChatEvents().size();
    bookingResult(0);
    Require(g_windowMgr.GetChatEvents().size() == chatCount, "unsolicited booking result produced a notice");
    mode.m_mapLoadingStage = CGameMode::MapLoading_PresentScreen;
    Require(!submit("/partybooking register 90 513") && !g_session.m_partyBookingRegistrationPending,
        "booking registration was sent while the server could still discard gameplay requests");
    mode.m_mapLoadingStage = CGameMode::MapLoading_None;
    Require(!submit("/partybooking register 90 5 0 1 2 3 4 5 6")
        && !submit("/partybooking register 90 5 255") && !submit("/partybooking delete extra"),
        "invalid booking command passed local validation");
    Require(submit("/partybooking register 90 513 0 254 -1") && g_session.m_partyBookingRegistrationPending,
        "booking registration did not enter the pending state");
    Require(peer.ReceiveExact(18) == std::vector<u8>({2, 8, 90, 0, 1, 2, 0, 0, 254, 0,
        255, 255, 255, 255, 255, 255, 255, 255}), "booking registration fields differ");
    Require(!submit("/partybooking register 90 513") && !submit("/partybooking delete")
        && SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(20)) == 0,
        "another booking operation overlapped an outstanding registration");
    bookingResult(0);
    Require(!g_session.m_partyBookingRegistrationPending && g_windowMgr.GetChatEvents().back().text == "Party booking registered.",
        "booking success did not resolve registration");
    Require(submit("/partybooking register 90 513"), "booking did not allow a subsequent registration");
    Require(peer.ReceiveExact(18) == std::vector<u8>({2, 8, 90, 0, 1, 2, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255}), "booking omitted default wildcard jobs");
    bookingResult(2);
    Require(!g_session.m_partyBookingRegistrationPending
        && g_windowMgr.GetChatEvents().back().text == "A party booking is already registered.",
        "duplicate booking result did not resolve registration");
    for (const auto& [result, expected] : std::array<std::pair<u16, const char*>, 2>{
            {{1, "Party booking registration failed."}, {0x101, "Party booking returned an unknown registration result."}}}) {
        Require(submit("/partybooking register 90 513"), "booking could not send after a terminal result");
        Require(peer.ReceiveExact(18) == std::vector<u8>({2, 8, 90, 0, 1, 2, 255, 255, 255, 255,
            255, 255, 255, 255, 255, 255, 255, 255}), "retry changed the booking frame");
        bookingResult(result);
        Require(!g_session.m_partyBookingRegistrationPending && g_windowMgr.GetChatEvents().back().text == expected,
            "booking failure or unknown 16-bit result did not resolve registration");
    }
    Require(submit("/partybooking delete") && peer.ReceiveExact(4) == std::vector<u8>({6, 8, 0, 0})
        && !g_session.m_partyBookingRegistrationPending
        && g_windowMgr.GetChatEvents().back().text == "Party booking deletion requested. The server sends no confirmation.",
        "booking deletion used the later layout or reported unconfirmed success");
    g_session.m_partyBookingRegistrationPending = previousBookingPending;
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
    submissionKeys={};g_session.m_talkType=0;
    Require(submit("/battlechat") && g_session.m_isBattleChat
        && g_windowMgr.GetChatEvents().back().text == "BATTLE CHAT ON", "battle chat command did not enable the route");
    Require(submit("battle"),"battle source route failed");named(0x02db,"battle");
    auto* room=static_cast<UIChatRoomWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_CHATROOMWND));room->ClearMembers();room->SetInputText("room world");
    Require(room->SubmitInput(),"room ordinary source dispatch failed");named(profile.globalMessage,"room world");
    Require(submit("/battlechat") && !g_session.m_isBattleChat
        && g_windowMgr.GetChatEvents().back().text == "BATTLE CHAT OFF", "battle chat command did not disable the route");
    g_session.m_talkType=0;
    mode.m_sourceLastSubmittedChat.clear();mode.m_sourceSameChatRepeatCount=0;
    Require(submit("repeat"),"first shared-repeat send failed");named(profile.globalMessage,"repeat");
    Require(submit("repeat","Peer"),"second shared-repeat send failed");whispered("Peer","repeat");
    Require(!submit("%repeat") && g_windowMgr.GetChatEvents().back().text=="REPEAT" && std::string(g_windowMgr.GetChatMsg()).empty(),"shared source repeat state did not reject/clear across public/private/party");
    Require(!submit("forbidden") && g_windowMgr.GetChatEvents().back().text=="MANNER" && std::string(g_windowMgr.GetChatMsg()).empty(),"manner rejection did not own manager clear");
    g_session.SetActiveStatusIcon(107,true,0);Require(!submit("berserk") && std::string(g_windowMgr.GetChatMsg())=="berserk","Berserk should retain source message");g_session.SetActiveStatusIcon(107,false,0);
    g_session.m_guildWindow.id=0;Require(!submit("/gc stale guild") && std::string(g_windowMgr.GetChatMsg())=="stale guild","guild guard should retain parser-owned tail");
    Require(!submit("%") && std::string(g_windowMgr.GetChatMsg())=="%","empty percent prefix should not clear source message");
    Require(SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(20))==0,"rejected source messages leaked packets");
    const bool previousMinEffect = g_session.m_isMinEffect;
    Require(submit("/tiny") && g_session.m_isMinEffect != previousMinEffect
        && g_session.m_isMinEffectOld == g_session.m_isMinEffect,
        "minimum effects did not update the local preference");
    Require(submit("/tiny") && g_session.m_isMinEffect == previousMinEffect
        && SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(30)) == 0,
        "minimum effects sent a packet or failed to restore the preference");
    mode.m_reqEmotionTick=0;mode.m_isReqEmotion=0;Require(submit("/!"),"registered source emotion did not route47->33");
    Require(peer.ReceiveExact(3)==std::vector<u8>({0xbf,0,0}) && mode.m_isReqEmotion && mode.m_reqEmotionTick==10000,"source emotion0/pending/time/frame differs");
    Require(!submit("/?"),"pending source emotion was not guarded");mode.m_isReqEmotion=0;submissionTick=12500;Require(!submit("/?"),"source emotion exact2500 boundary changed");
    submissionTick=12501;Require(submit("/?"),"source emotion after2500 was blocked");Require(peer.ReceiveExact(3)==std::vector<u8>({0xbf,0,1}),"source question emotion argument was shifted");
    {
        struct EmotionPlayer : CPlayer {
            int emotion = -1;
            void SendMsg(CGameObject*, int message, msgparam_t val, msgparam_t, msgparam_t) override
            {
                if (message == 21) emotion = static_cast<int>(val);
            }
        } player;
        CWorld world;
        player.m_gid = 0xf2000041;
        world.m_player = &player;
        auto* previousWorld = mode.m_world;
        mode.m_world = &world;
        CGameModePacketRouter emotionRouter;
        RegisterDefaultGameModePacketHandlers(emotionRouter);
        TestConnection input;
        const auto dispatch = [&](std::vector<u8> frame) {
            Require(input.QueueBytes(frame)
                && ProcessGameModePacketLoop(mode, input, emotionRouter, 1).packetsDispatched == 1,
                "emotion reply did not reach the production router");
        };
        dispatch({0xc0, 0, 0x42, 0, 0, 0xf2, 1});
        Require(mode.m_isReqEmotion && player.emotion == -1, "unrelated emotion cleared local pending state");
        dispatch({0xc0, 0, 0x41, 0, 0, 0xf2, 81});
        Require(mode.m_isReqEmotion && player.emotion == -1, "unsupported emotion reached actor action");
        dispatch({0xc0, 0, 0x41, 0, 0, 0xf2, 1});
        Require(!mode.m_isReqEmotion && player.emotion == 1, "local 00C0 did not clear and dispatch");
        submissionTick = 15002;
        Require(submit("/!") && peer.ReceiveExact(3) == std::vector<u8>({0xbf,0,0}),
            "acknowledged emotion could not send after cooldown");
        submissionTick = 20001;
        Require(!submit("/!"), "missing reply released before timeout");
        submissionTick = 20002;
        Require(submit("/!") && peer.ReceiveExact(3) == std::vector<u8>({0xbf,0,0}),
            "missing reply prevented a fresh emotion after timeout");
        dispatch({0x10, 0x01, 1, 0, 1, 0, 0, 0, 0, 0});
        Require(!mode.m_isReqEmotion, "selected emotion rejection did not clear pending");
        world.m_player = nullptr;
        mode.m_world = previousWorld;
        submissionTick = 12501;
    }
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
        struct TurnCase {float before,after;int body;};
        const TurnCase turns[]={{0,315,3},{45,0,4},{360,315,3},{-360,-45,3},{45.5f,0.5f,4},{-900,-585,255}};
        for(const auto& turn:turns){player.m_roty=turn.before;player.m_headDir=2;player.m_curMotion=2;
            Require(submit("/bangbang"),"source bangbang parser/dispatch failed");direction(0,turn.body);
            Require(player.m_roty==turn.after && player.m_headDir==2 && player.m_curMotion==2,
                "bangbang wrap/head-state semantics changed");}
        player.m_roty=1.0e30f;Require(submit("/bangbang"),"large finite bangbang request failed");direction(0,4);
        Require(mode.SendMsg(152,0xab,0x1234,0)!=0,"source direction recipient failed");direction(0x1234,0xab);
        const auto oldAid=g_session.m_aid,oldGid=g_session.m_gid;
        struct RestoreIds {decltype(g_session.m_aid) aid;decltype(g_session.m_gid) gid;
            ~RestoreIds(){g_session.m_aid=aid;g_session.m_gid=gid;}} restoreIds{oldAid,oldGid};
        g_session.m_aid=0x12345678;g_session.m_gid=0x90abcdef;
        world.m_isPKZone=0;Require(!submit("/pvpinfo"),"non-PK pvpinfo request was sent");
        Require(SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(20))==0,"non-PK pvpinfo leaked a frame");
        for(int zone:{1,-1}){world.m_isPKZone=zone;Require(submit("/pvpinfo"),"PK pvpinfo parser/dispatch failed");
            Require(peer.ReceiveExact(10)==std::vector<u8>({0x0f,0x02,0x78,0x56,0x34,0x12,0xef,0xcd,0xab,0x90}),
                "pvpinfo must send both account and character IDs in the selected10-byte request");}
    }
    {
        CWorld world;
        CPlayer player;
        CGameMode prayerMode;
        prayerMode.SetSourceChatInputServices(&services);
        prayerMode.m_world = &world;
        world.m_player = &player;
        const int oldJob = g_session.m_playerJob;
        const auto prayer = [&](int job, int state, u32 elapsed, bool expected) {
            g_session.m_playerJob = job;
            player.m_stateId = state;
            const u32 start = submissionTick + 1000;
            for (int index = 0; index < 10; ++index) {
                submissionTick = index == 9 ? start + elapsed : start + index;
                const std::string line = index == 7 ? "First prayer line" : index == 9 ? "Last prayer line"
                    : "Prayer filler " + std::to_string(index);
                Require(prayerMode.SubmitSourceChat(line, ""), "prayer fixture could not send public speech");
                if (index == 9 && expected) {
                    Require(peer.ReceiveExact(2) == std::vector<u8>{0xED, 1},
                        "prayer did not precede the final speech frame");
                }
                named(profile.globalMessage, line);
            }
        };
        prayer(23, 0, 9999, false);
        prayer(23, 3, 10000, false);
        prayer(0, 0, 10000, false);
        prayer(23, 0, 10000, true);
        prayer(4045, 0, 10000, true);
        g_session.m_playerJob = oldJob;
        prayerMode.m_world = nullptr;
        world.m_player = nullptr;
    }
    {
        const ServiceType previous=g_serviceType;
        struct RestoreService {ServiceType value;~RestoreService(){g_serviceType=value;}} restoreService{previous};
        g_serviceType=ServiceKorea;
        const std::pair<const char*,u16> requests[]={{"/who",193},{"/memo",285},{"/blacksmith",535},{"/alchemist",536},{"/taekwon",549},{"/pk",567},{"/hunting",633}};
        for(const auto& request:requests){Require(submit(request.first),"source request command dispatch failed");
            Require(peer.ReceiveExact(2)==std::vector<u8>({static_cast<u8>(request.second),static_cast<u8>(request.second>>8)}),"source command request opcode/length changed");}
        for(auto service:{ServiceChina,ServiceMalaysia,ServiceSingapore}){g_serviceType=service;Require(!submit("/who"),"source WHO service guard missing");}
        g_serviceType=ServiceVietnam;g_windowMgr.ClearChatEvents();Require(!submit("/who"),"Vietnam WHO must use unknown-command feedback");
        const auto& events=g_windowMgr.GetChatEvents();Require(!events.empty(),"Vietnam WHO lost unknown-command feedback");
        Require(SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(20))==0,"guarded WHO leaked a network frame");
        // The Vietnam restriction belongs to the slash prelude, not recipient34.
        Require(mode.SendMsg(34,0,0,0)!=0 && peer.ReceiveExact(2)==std::vector<u8>({0xc1,0}),"direct recipient34 acquired an extra Vietnam guard");
    }
    {
        unsigned seen=0;
        for(unsigned seed=0;seed<1024 && seen!=15;++seed){
            std::srand(seed);std::vector<int> expected(std::begin(expectedSourceHelpIds),std::end(expectedSourceHelpIds));
            unsigned branch;
            if(std::rand()%10<4){expected.push_back(921);expected.push_back(922);branch=1;}
            else if(std::rand()%5<2){expected.push_back(760);branch=2;}
            else if(std::rand()%3<2){expected.push_back(789);branch=4;}
            else branch=8;
            const int next=std::rand();std::srand(seed);g_windowMgr.ClearChatEvents();
            Require(submit("/help"),"source HELP parser/action failed");
            Require(std::rand()==next,"HELP changed shared RNG short-circuit consumption");
            const auto& events=g_windowMgr.GetChatEvents();Require(events.size()==expected.size(),"HELP row count/two-row warning tail changed");
            for(std::size_t i=0;i<expected.size();++i)Require(events[i].text=="HELP-"+std::to_string(expected[i]) && events[i].color==0x64ffffu,
                "HELP selected message order/color changed");
            seen|=branch;
        }
        Require(seen==15,"HELP replay did not cover all four random-tail outcomes");
        Require(SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(20))==0,"HELP emitted a network request");
    }
#include "WhereCommandReplay.inc"
#include "CommandWindowReplay.inc"
#include "PetRequestReplay.inc"
#include "PetWindowReplay.inc"
#include "PetContextMenuReplay.inc"
#include "PetEggReplay.inc"
#include "PetCaptureRequestReplay.inc"
#include "PetTamingReplay.inc"
#include "PetCaptureTargetReplay.inc"
#include "PlayerSkillProceedReplay.inc"
    connection->Disconnect();
#include "PlayerAutoRunFailedSendReplay.inc"
#include "PetWindowFailedSendReplay.inc"
#include "PetEggFailedSendReplay.inc"
    Require(!mode.SendMsg(162,0x12345678,0,0),"disconnected pet capture request reported success");
#include "PetTamingFailedSendReplay.inc"
    Require(!mode.SendMsg(163,1,0,0) && !mode.SendMsg(158,reinterpret_cast<msgparam_t>("Poring"),0,0),"disconnected pet requests reported success");
    mode.m_isReqEmotion=0;mode.m_reqEmotionTick=12501;submissionTick=16000;
    Require(!submit("/!") && !mode.m_isReqEmotion && mode.m_reqEmotionTick==12501,
        "failed emotion send left a pending reply or consumed the cooldown");
#include "ChatIgnoreReplay.inc"
#include "FriendGreetingReplay.inc"
#include "CommandToggleReplay.inc"
#include "WeatherCommandReplay.inc"
#include "TipDataReplay.inc"
#include "TipWindowReplay.inc"
    mode.SetSourceChatInputServices(nullptr);
    std::cout<<"{\"case_id\":\"source_chat_submission\",\"contract_id\":\"PK09.21\",\"actual_channel_frames\":true,\"caller_history_before_dispatch\":true,\"manager_buffer\":true,\"source47_11\":true,\"emotion_pending\":true,"
        "\"source_command_controller\":true,\"shared_help_owner\":true,\"pet_info_controller\":true,\"window_keys_pending\":[68,87],\"outcome\":\"PASS\"}\n";
}
