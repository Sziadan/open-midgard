#include "ui/UIFriendOptionWnd.h"
#include "ui/UIMessengerGroupWnd.h"

void FriendOptionsReplay()
{
    SetCase("source_friend_options","PK09.21","messenger-168-options68","actual messenger child168 -> window68 child147 -> Session policy -> actual0097 owner/fallback");
    g_windowMgr.RemoveAllWindows();CGameMode mode;TestConnection receive;
    struct Restore {
        CMode* current=SkillPacketReplayAccess::Current();int type=SkillPacketReplayAccess::CurrentType(),folder=g_readFolderFirst,codePage=g_codePage;
        bool base=g_session.m_isShowWhisperWnd,friendWindows=g_session.m_isShowWhisperWnd_Friend,sound=g_session.m_isPlayWhisperOpenSound,autoMsg=g_session.m_onAutoMSG,save=g_session.m_isSaveChat;
        std::list<FRIEND_INFO> friends=g_session.m_friendList;ro::guild::WindowState guild=g_session.m_guildWindow;std::vector<std::string> excluded=g_session.m_exNameList;
        SubChatWindowInfo geometry=g_windowMgr.m_friendOptionWindowInfo,whisperGeometry=g_windowMgr.m_whisperWindowInfo;
        std::filesystem::path cwd=std::filesystem::current_path(),resource;bool hadData=std::getenv("OPEN_MIDGARD_DATA_DIR")!=nullptr;
        std::string data=hadData?std::getenv("OPEN_MIDGARD_DATA_DIR"):"";
        ~Restore(){g_windowMgr.RemoveAllWindows();g_windowMgr.ClearChatEvents();SkillPacketReplayAccess::Restore(current,type);
            g_readFolderFirst=folder;g_codePage=codePage;g_session.m_isShowWhisperWnd=base;g_session.m_isShowWhisperWnd_Friend=friendWindows;g_session.m_isPlayWhisperOpenSound=sound;
            g_session.m_onAutoMSG=autoMsg;g_session.m_isSaveChat=save;g_session.m_friendList=std::move(friends);g_session.m_guildWindow=std::move(guild);g_session.m_exNameList=std::move(excluded);
            g_windowMgr.m_friendOptionWindowInfo=geometry;g_windowMgr.m_whisperWindowInfo=whisperGeometry;std::filesystem::current_path(cwd);
#if RO_PLATFORM_WINDOWS
            _putenv_s("OPEN_MIDGARD_DATA_DIR",hadData?data.c_str():"");
#else
            if(hadData)setenv("OPEN_MIDGARD_DATA_DIR",data.c_str(),1);else unsetenv("OPEN_MIDGARD_DATA_DIR");
#endif
            const auto target=std::filesystem::absolute(resource).lexically_normal();if(target.parent_path()==cwd && target.filename().string().rfind("friend-options-replay-",0)==0){std::error_code error;std::filesystem::remove_all(target,error);}
        }
    } restore;
    restore.resource=restore.cwd/("friend-options-replay-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(restore.resource/"settings");std::filesystem::current_path(restore.resource);
#if RO_PLATFORM_WINDOWS
    _putenv_s("OPEN_MIDGARD_DATA_DIR",(restore.resource/"settings").string().c_str());
#else
    setenv("OPEN_MIDGARD_DATA_DIR",(restore.resource/"settings").string().c_str(),1);
#endif
    const auto language=[](int empty=-1,bool longLabel=false){std::ofstream file("msgstringtable.txt",std::ios::binary);
        const std::map<int,std::string> labels={{102,"Friend"},{355,"Preferences <private>"},{359,"Friends"},{361,longLabel?std::string(120,'W'):"Regular"},{362,"Sound"}};
        for(int i=0;i<=362;++i){const auto found=labels.find(i);if(i!=empty && found!=labels.end())file<<found->second;file<<"#\r\n";}
    };
    language();g_readFolderFirst=1;g_codePage=65001;SkillPacketReplayAccess::SetCurrent(&mode);
    g_session.m_isShowWhisperWnd=g_session.m_isShowWhisperWnd_Friend=true;g_session.m_isPlayWhisperOpenSound=false;g_session.m_onAutoMSG=g_session.m_isSaveChat=false;
    g_session.m_friendList.clear();g_session.m_guildWindow={};g_session.m_exNameList.clear();
    SaveSettingsIniInt("Option","m_isShowWhisperWnd",1);SaveSettingsIniInt("Option","m_isShowWhisperWnd_Friend",1);SaveSettingsIniInt("Option","m_isPlayWhisperOpenSound",0);
    SaveSettingsIniInt("UIWindows","FriendOptionWndX",37);
    g_windowMgr.m_friendOptionWindowInfo={-1,0,281,120};
    g_windowMgr.m_whisperWindowInfo={0,300,280,120};
    auto* messenger=static_cast<UIMessengerGroupWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_MESSENGERGROUPWND));
    auto* entry=messenger->GetFriendOptionButton();Require(entry && entry->m_id==168 && entry->m_parent==messenger && entry->m_x==messenger->m_x+83 && entry->m_y==messenger->m_y+messenger->m_h-40,"source messenger168 child/placement absent");
    const auto down=[](UIWindow* w){g_windowMgr.OnLBtnDown(w->m_x+1,w->m_y+1);};
    const auto up=[](UIWindow* w){g_windowMgr.OnLBtnUp(w->m_x+1,w->m_y+1);};
    down(entry);g_windowMgr.OnLBtnUp(-1,-1);Require(!g_windowMgr.m_friendOptionWnd,"messenger entry activated on outside release");
    down(entry);g_windowMgr.OnMouseMove(-1,-1);g_windowMgr.OnMouseMove(entry->m_x+1,entry->m_y+1);up(entry);
    auto* options=g_windowMgr.m_friendOptionWnd;Require(options && options->m_x==200 && options->m_y==120 && options->m_w==280 && options->m_h==120
        && options->GetCaption()=="Preferences <private>","messenger168 did not open source68 or invalid record did not use raw fallback");
    auto* regular=options->GetCheckBox(0);auto* friends=options->GetCheckBox(1);auto* sound=options->GetCheckBox(2);
    Require(regular->GetLabel()=="Regular" && friends->GetLabel()=="Friends" && sound->GetLabel()=="Sound" && regular->m_w>12 && regular->m_w<=265
        && regular->m_x==206 && regular->m_y==141 && friends->m_y==157 && sound->m_y==173 && regular->m_h==12,"checkbox source labels/geometry changed");
    down(regular);Require(!g_session.m_isShowWhisperWnd && g_session.m_isShowWhisperWnd_Friend && !regular->IsChecked(),"checkbox147 did not notify synchronously or coupled base/friend at runtime");
    g_windowMgr.OnLBtnUp(-1,-1);Require(!g_session.m_isShowWhisperWnd && LoadSettingsIniInt("Option","m_isShowWhisperWnd",-1)==1,"outside release reverted down toggle or click saved options");
    friends->m_isDisabled=1;down(friends);up(friends);Require(g_session.m_isShowWhisperWnd_Friend && friends->IsChecked(),"disabled source checkbox toggled");friends->m_isDisabled=0;
    UIButton unknown;options->SendMsg(&unknown,6,147,0,0);Require(g_session.m_isShowWhisperWnd_Friend,"unknown checkbox sender mutated Session");
    down(sound);Require(g_session.m_isPlayWhisperOpenSound && sound->IsChecked(),"sound checkbox failed immediate update");up(sound);down(sound);up(sound);
    auto& router=GetProductionGameModePacketRouter();router.Clear();RegisterDefaultGameModePacketHandlers(router);
    const auto incoming=[&](const std::string& name){const std::string body="preferences";std::vector<u8> packet(29+body.size());packet[0]=0x97;packet[2]=u8(packet.size());packet[3]=u8(packet.size()>>8);
        std::copy(name.begin(),name.end(),packet.begin()+4);std::copy(body.begin(),body.end(),packet.begin()+28);Require(receive.QueueBytes(packet),"preference incoming queue failed");
        Require(ProcessGameModePacketLoop(mode,receive,router,1).packetsDispatched==1,"preference incoming production dispatch failed");};
    FRIEND_INFO friendInfo;friendInfo.characterName="PrefFriend";g_session.m_friendList.push_back(friendInfo);friendInfo.characterName="PrefFriend2";g_session.m_friendList.push_back(friendInfo);
    incoming("PrefRegular");incoming("PrefFriend");Require(!g_windowMgr.m_whisperWindows.count("PrefRegular") && g_windowMgr.m_whisperWindows.count("PrefFriend"),"runtime base-off/friend-on routing changed");
    if(g_windowMgr.m_chatWnd)g_windowMgr.m_chatWnd->SetShow(0);
    auto* retained=g_windowMgr.m_whisperWindows.at("PrefFriend");retained->SetShow(0);const auto rowCount=retained->GetRows().size();
    down(regular);up(regular);down(friends);up(friends);incoming("PrefRegular2");incoming("PrefFriend2");incoming("PrefFriend");
    Require(g_session.m_isShowWhisperWnd && !g_session.m_isShowWhisperWnd_Friend && g_windowMgr.m_whisperWindows.count("PrefRegular2")
        && !g_windowMgr.m_whisperWindows.count("PrefFriend2") && !retained->m_show && retained->GetRows().size()==rowCount+1,"checkboxes closed/unhid retained windows or mixed friend/nonfriend routing");
    if(g_windowMgr.m_chatWnd)g_windowMgr.m_chatWnd->SetShow(0);
    options->Move(41,43);auto* close=options->GetCloseButton();down(close);g_windowMgr.OnLBtnUp(-1,-1);Require(g_windowMgr.m_friendOptionWnd==options,"outside close release deleted preferences");
    down(close);up(close);Require(!g_windowMgr.m_friendOptionWnd && g_windowMgr.m_friendOptionWindowInfo.x==41 && LoadSettingsIniInt("UIWindows","FriendOptionWndX",-1)==37
        && LoadSettingsIniInt("Option","m_isShowWhisperWnd_Friend",-1)==1,"close lost geometry snapshot or wrote preferences prematurely");
    messenger->SendMsg(nullptr,6,168,0,0);options=g_windowMgr.m_friendOptionWnd;Require(options && options->m_x==41 && options->m_y==43 && !options->GetCheckBox(1)->IsChecked(),"source toggle recreation lost cached state");
    messenger->SendMsg(nullptr,6,168,0,0);Require(!g_windowMgr.m_friendOptionWnd,"second messenger168 did not delete singleton");
    UIMessengerGroupWnd::QtButtonDisplay tab{};Require(messenger->GetQtTabDisplayForQt(1,&tab),"party tab fixture missing");
    g_windowMgr.OnLBtnDown(tab.x+2,tab.y+2);g_windowMgr.OnLBtnUp(tab.x+2,tab.y+2);messenger->SendMsg(nullptr,6,168,0,0);
    Require(!entry->m_show && !g_windowMgr.m_friendOptionWnd,"friend-only source entry leaked into party tab");
    Require(messenger->GetQtTabDisplayForQt(0,&tab),"friend tab fixture missing");g_windowMgr.OnLBtnDown(tab.x+2,tab.y+2);g_windowMgr.OnLBtnUp(tab.x+2,tab.y+2);
    Require(entry->m_show,"returning to friend tab did not restore source entry");
    language(361);messenger->SendMsg(nullptr,6,168,0,0);options=g_windowMgr.m_friendOptionWnd;
    Require(options->GetCheckBox(0)->GetLabel().empty() && options->GetCheckBox(0)->m_w==12,"known-empty checkbox label lost source hit width");
    messenger->SendMsg(nullptr,6,168,0,0);language(-1,true);messenger->SendMsg(nullptr,6,168,0,0);options=g_windowMgr.m_friendOptionWnd;
    Require(options->GetCheckBox(0)->m_w==265,"long checkbox label escaped source width cap");
    g_windowMgr.DeleteWindow(messenger);Require(g_windowMgr.m_friendOptionWnd==options,"messenger deletion incorrectly owned preference lifetime");
    g_windowMgr.DeleteWindow(options);language();
    const auto client=g_windowMgr.GetChatClientRect();
    for(const auto& info:std::vector<SubChatWindowInfo>{{0,0,279,120},{0,0,280,119},{-1,0,280,120},{static_cast<int>(client.right)-279,0,280,120}}){
        g_windowMgr.m_friendOptionWindowInfo=info;options=static_cast<UIFriendOptionWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_FRIENDOPTIONWND));
        Require(options->m_x==200 && options->m_y==120,"invalid complete friend-options record was partly salvaged");g_windowMgr.DeleteWindow(options);
    }
    g_windowMgr.m_friendOptionWindowInfo={static_cast<int>(client.right)-280,static_cast<int>(client.bottom)-120,280,120};
    options=static_cast<UIFriendOptionWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_FRIENDOPTIONWND));Require(options->m_x==client.right-280 && options->m_y==client.bottom-120,"valid edge record was rejected");
    g_windowMgr.RemoveAllWindows();Require(!g_windowMgr.m_friendOptionWnd && g_session.m_isShowWhisperWnd && !g_session.m_isShowWhisperWnd_Friend,"bulk teardown changed preference state");
    std::cout<<"{\"case_id\":\"source_friend_options\",\"contract_id\":\"PK09.21\",\"actual_messenger168\":true,\"down_time_child147\":true,\"actual0097_policy\":true,\"cached_rect\":true,\"no_immediate_save\":true,\"outcome\":\"PASS\"}\n";
}
