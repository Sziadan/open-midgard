#include "core/SettingsIni.h"
#include "main/WinMain.h"

void ChatBattleOptionsSettingsProcess(bool write)
{
    SetCase(write?"chat_options_settings_write":"chat_options_settings_read","PK09.21",
        "actual-process-settings","ReadRegistry -> session filters -> source135 persist -> new process ReadRegistry");
    g_windowMgr.RemoveAllWindows();
    ReadRegistry();
    if(write){
        g_session.m_battleMsgOnOffSet.fill(true);
        auto* options=static_cast<UIBattleMsgOptionWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_BATTLEMSGOPTION));
        for(int i=0;i<13;i+=2){
            options->ScrollRows(-1000);if(i>=8)options->ScrollRows(6);
            const int y=options->m_y+17+(i-options->GetFirstRow())*20;
            g_windowMgr.OnLBtnDown(options->m_x+50,y);g_windowMgr.OnLBtnUp(options->m_x+50,y);
        }
        options->SendMsg(nullptr,6,135,0,0);
        Require(!g_windowMgr.m_battleMsgOptionWnd,"source135 failed to delete persisted options owner");
    }
    for(int i=0;i<13;++i)Require(g_session.m_battleMsgOnOffSet[i]==(i%2!=0),
        "cross-process source option pattern differs");
    std::cout<<"{\"case_id\":\"chat_options_settings_"<<(write?"write":"read")<<"\",\"actual_startup\":true,\"outcome\":\"PASS\"}\n";
}

void ChatBattleOptionsReplay()
{
    SetCase("chat_battle_options_routes","PK09.21","source139-window",
        "manager139/source127 -> UI checkbox -> production008D and typed source routes -> save/close/recreate");
    struct Restore {
        std::array<bool,13> flags=g_session.m_battleMsgOnOffSet;
        std::filesystem::path cwd=std::filesystem::current_path(),ini=GetOpenMidgardIniPath();
        bool existed=std::filesystem::exists(ini),sticky=g_windowMgr.m_chatWndStickOn;
        int folder=g_readFolderFirst,width=WINDOW_WIDTH,height=WINDOW_HEIGHT;
        int noTalk=g_session.m_isNoTalkMsg,noTalk2=g_session.m_isNoTalkMsg2;
        ServiceType service=g_serviceType;
        std::list<u32> excluded=g_session.m_exAidList;
        std::string bytes;
        Restore(){if(existed){std::ifstream f(ini,std::ios::binary);bytes.assign(std::istreambuf_iterator<char>(f),{});}}
        ~Restore(){g_windowMgr.RemoveAllWindows();g_windowMgr.ClearChatEvents();
            std::filesystem::current_path(cwd);g_readFolderFirst=folder;
            g_session.m_battleMsgOnOffSet=flags;g_windowMgr.m_chatWndStickOn=sticky;
            WINDOW_WIDTH=width;WINDOW_HEIGHT=height;g_serviceType=service;
            g_session.m_isNoTalkMsg=noTalk;g_session.m_isNoTalkMsg2=noTalk2;g_session.m_exAidList=excluded;
            if(existed){std::ofstream f(ini,std::ios::binary|std::ios::trunc);f<<bytes;}
            else std::filesystem::remove(ini);
        }
    } restore;
    g_windowMgr.RemoveAllWindows();g_windowMgr.ClearChatEvents();
    std::filesystem::create_directories(restore.ini.parent_path());
    {std::ofstream f(restore.ini,std::ios::trunc);f<<"[ChatOption]\n0=0\n1=7\n2=invalid\n";}
    g_session.m_battleMsgOnOffSet.fill(false);ReadRegistry();
    Require(!g_session.m_battleMsgOnOffSet[0] && g_session.m_battleMsgOnOffSet[1]
        && g_session.m_battleMsgOnOffSet[2] && g_session.m_battleMsgOnOffSet[12],
        "actual startup did not load nonzero/missing/invalid option defaults");
    const auto directory=restore.cwd/("chat-options-resource-"+std::to_string(GetTickCount()));
    std::filesystem::create_directories(directory);
    {std::ofstream f(directory/"msgstringtable.txt",std::ios::binary);
        for(int i=0;i<=1331;++i){if(i!=1282)f<<"[OPTION "<<i<<"]";f<<"#\r\n";}}
    std::filesystem::current_path(directory);g_readFolderFirst=1;
    g_session.m_battleMsgOnOffSet.fill(true);g_windowMgr.m_chatWndStickOn=true;
    g_session.m_isNoTalkMsg=g_session.m_isNoTalkMsg2=0;g_session.m_exAidList.clear();g_serviceType=ServiceKorea;
    g_windowMgr.EnsureChatWindowVisible();auto* main=g_windowMgr.m_chatWnd;
    auto* st=g_windowMgr.m_subChatWndST;auto* bt=g_windowMgr.m_subChatWndBT;
    main->SendMsg(nullptr,6,127,0,0);auto* options=g_windowMgr.m_battleMsgOptionWnd;
    Require(options && options->m_id==139 && options->m_w==280 && options->m_h==200
        && options->GetFirstRow()==0 && options->GetSelectedRow()==-1,"source139 manager creation differs");
    Require(options->GetLabel(0)=="[OPTION 1277]" && options->GetLabel(5).empty()
        && options->GetLabel(12)=="[OPTION 1331]" && options->GetTitle()=="[OPTION 1289]",
        "actual positional message table/known empty options label differs");
    const auto click=[&](int index){
        options->ScrollRows(-1000);if(index>=8)options->ScrollRows(6);
        const int x=options->m_x+50,y=options->m_y+17+(index-options->GetFirstRow())*20;
        g_windowMgr.OnLBtnDown(x,y);g_windowMgr.OnLBtnUp(x,y);
        Require(options->GetSelectedRow()==index,"actual manager options click selected wrong source index");
    };
    click(0);Require(!g_session.m_battleMsgOnOffSet[0],"chat checkbox did not change session filter");
    Require(LoadSettingsIniInt("ChatOption","0",-1)==0,"toggle unexpectedly changed persisted value");
    CGameMode mode;TestConnection connection;
    auto& router=GetProductionGameModePacketRouter();router.Clear();RegisterDefaultGameModePacketHandlers(router);
    std::vector<u8> speech={0x8d,0,0,0,1,0,0,0x74};
    const std::string words="OptionsSpeaker : routed source speech";
    speech.insert(speech.end(),words.begin(),words.end());speech.push_back(0);
    speech[2]=static_cast<u8>(speech.size());speech[3]=static_cast<u8>(speech.size()>>8);
    Require(connection.QueueBytes(speech),"queue source options speech failed");
    const auto dispatched=ProcessGameModePacketLoop(mode,connection,router,1);
    Require(dispatched.packetsDispatched==1 && main->GetSourceHistory(false).size()==1
        && main->GetSourceHistory(true).empty() && st->GetRows().size()==1 && bt->GetRows().empty(),
        "actual008D did not honor the toggled BT filter while retaining ST/detached ownership");
    for(int i=1;i<13;++i)click(i);
    for(int i=1;i<=13;++i)g_windowMgr.PushSourceChatEvent("source option type",0xaaaaaa,6,
        ro::chat::NoticeRoute::Normal,static_cast<ro::chat::NoticeType>(i));
    Require(main->GetSourceHistory(false).size()==12 && main->GetSourceHistory(true).empty(),
        "source option domain/type8/9 suppression differs");
    for(int i=1;i<=13;++i)g_windowMgr.PushSourceChatEvent("command bypass",0xaaaaaa,6,
        ro::chat::NoticeRoute::Command,static_cast<ro::chat::NoticeType>(i));
    Require(main->GetSourceHistory(true).size()==13,"command route did not bypass battle options");
    const auto off=g_session.m_battleMsgOnOffSet;
    options->SendMsg(nullptr,6,134,0,0);Require(g_session.m_battleMsgOnOffSet==off,"source inert134 invented reset behavior");
    options->SendMsg(nullptr,10,0,0,0);Require(options->GetFirstRow()==6,"source options omitted blank trailing scroll row");
    const int x=options->m_x+50,y=options->m_y+17+7*20;
    g_windowMgr.OnLBtnDown(x,y);g_windowMgr.OnLBtnUp(x,y);
    Require(options->GetSelectedRow()==-1 && g_session.m_battleMsgOnOffSet==off,"blank final row toggled an option");
    main->SendMsg(nullptr,6,127,0,0);Require(!g_windowMgr.m_battleMsgOptionWnd,"source127 did not delete owner");
    Require(LoadSettingsIniInt("ChatOption","1",-1)==7,"source127 toggle-close unexpectedly persisted filters");
    main->SendMsg(nullptr,6,127,0,0);options=g_windowMgr.m_battleMsgOptionWnd;
    Require(options && options->GetFirstRow()==0 && options->GetSelectedRow()==-1
        && g_session.m_battleMsgOnOffSet==off,"reopen reloaded preferences or retained old scroll/selection");
    Require(g_windowMgr.OnQtKeyDown(VK_ESCAPE,false,false,false) && !g_windowMgr.m_battleMsgOptionWnd,
        "source Escape did not save and delete139");
    for(int i=0;i<13;++i)Require(LoadSettingsIniInt("ChatOption",std::to_string(i).c_str(),-1)==0,
        "source135 did not persist all thirteen options");
    bt->SendMsg(nullptr,6,127,0,0);options=g_windowMgr.m_battleMsgOptionWnd;
    g_windowMgr.DeleteWindow(main);
    Require(options && g_windowMgr.m_battleMsgOptionWnd==options,"individual main deletion destroyed options owner");
    g_windowMgr.RemoveAllWindows();Require(!g_windowMgr.m_battleMsgOptionWnd,"bulk teardown retained options pointer");
    std::cout<<"{\"case_id\":\"chat_battle_options_routes\",\"filters\":13,\"siege_message_id\":1331,\"last_first\":6,\"actual_008d\":true,\"close_only_persistence\":true,\"outcome\":\"PASS\"}\n";
}
