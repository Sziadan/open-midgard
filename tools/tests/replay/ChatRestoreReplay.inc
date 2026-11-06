#include "core/SettingsIni.h"

void ChatWindowRestoreReplay()
{
    SetCase("chat_cached_height_restore","PK09.21","source-message34-placement-order",
        "cached physical height -> whole-rectangle validation -> SetState bottom anchor");
    g_windowMgr.RemoveAllWindows();
    struct Restore {
        int x=g_windowMgr.m_chatWndX,y=g_windowMgr.m_chatWndY,h=g_windowMgr.m_chatWndHeight,status=g_windowMgr.m_chatWndStatus;
        ~Restore(){g_windowMgr.RemoveAllWindows();g_windowMgr.m_chatWndX=x;g_windowMgr.m_chatWndY=y;
            g_windowMgr.m_chatWndHeight=h;g_windowMgr.m_chatWndStatus=status;}
    } restore;
    g_windowMgr.m_chatWndHeight=123;g_windowMgr.m_chatWndStatus=4;
    g_windowMgr.m_chatWndX=35;g_windowMgr.m_chatWndY=200;
    UINewChatWnd main;
    Require(main.m_x==35 && main.m_y==185 && main.m_h==138 && main.GetExpansionState()==4,
        "cached height was normalized before source placement");
    const auto client=g_windowMgr.GetChatClientRect();
    // Placement fits the old123px height even though138px would overflow.
    main.Resize(main.m_w,123);g_windowMgr.m_chatWndX=17;g_windowMgr.m_chatWndY=client.bottom-123;
    main.SendMsg(nullptr,34,0,0,0);
    Require(main.m_x==17 && main.m_y==client.bottom-138 && main.m_h==138,
        "source placement validated against post-state height");
    // Either invalid coordinate resets BOTH coordinates, rather than clamping.
    main.Resize(main.m_w,123);g_windowMgr.m_chatWndX=-1;g_windowMgr.m_chatWndY=40;
    main.SendMsg(nullptr,34,0,0,0);
    Require(main.m_x==0 && main.m_y==client.bottom-138 && g_windowMgr.m_chatWndX==0
        && g_windowMgr.m_chatWndY==client.bottom-123,"negative coordinate did not reset the whole source placement");
    main.Resize(main.m_w,123);g_windowMgr.m_chatWndX=INT_MAX;g_windowMgr.m_chatWndY=40;
    main.SendMsg(nullptr,34,0,0,0);
    Require(main.m_x==0 && main.m_y==client.bottom-138,"overflowing saved rectangle was not bounded safely");
    // State0 still preserves the bottom established by the physical cache.
    main.Resize(main.m_w,123);g_windowMgr.m_chatWndStatus=0;g_windowMgr.m_chatWndX=20;g_windowMgr.m_chatWndY=100;
    main.SendMsg(nullptr,34,0,0,0);
    Require(main.m_h==24 && main.m_y==199,"compact restore lost the pre-state bottom anchor");
    std::cout<<"{\"case_id\":\"chat_cached_height_restore\",\"cached_height\":123,\"state_height\":138,\"whole_rectangle_fallback\":true,\"outcome\":\"PASS\"}\n";
}

void ChatWindowSettingsProcess(bool write)
{
    SetCase(write?"chat_window_settings_write":"chat_window_settings_read","PK09.21",
        "actual-process-chat-window-preferences","ReadRegistry -> manager cache -> actual owners -> teardown -> shutdown save -> fresh process");
    g_windowMgr.RemoveAllWindows();ReadRegistry();
    if(write){
        g_windowMgr.m_chatWndStickOn=true;g_windowMgr.m_onStSubChat=g_windowMgr.m_onBtSubChat=false;
        // This sentinel must survive ordinary owner changes and teardown.
        SaveSettingsIniInt("UIWindows","ChatWndHeight",999);
        g_windowMgr.EnsureChatWindowVisible();auto* main=g_windowMgr.m_chatWnd;
        main->SetState(4);main->Move(30,180);
        auto* st=g_windowMgr.m_subChatWndST;auto* bt=g_windowMgr.m_subChatWndBT;
        Require(st && bt,"source restart owners missing");
        st->Resize(333,111);st->Move(25,30);bt->Resize(345,113);bt->Move(285,40);
        main->StickOnOff(false);
        auto* mini=g_windowMgr.m_subChatMiniWnd;
        mini->OnLBtnDown(mini->m_x+1,mini->m_y);
        Require(st->m_show && !bt->m_show,"actual mini toggle did not set persisted flags");
        g_windowMgr.ShowChatWindow(false);
        Require(!main->m_show && !st->m_show && !mini->m_show && !g_windowMgr.m_chatWndShow,
            "source manager hide did not snapshot or hide the chat family");
        g_windowMgr.RemoveAllWindows();
        Require(g_windowMgr.m_chatWndHeight==24 && g_windowMgr.m_chatWndY==294
            && g_windowMgr.m_chatWndStatus==0,"teardown did not retain actual chat cache");
        Require(LoadSettingsIniInt("UIWindows","ChatWndHeight",-1)==999,
            "owner store unexpectedly performed the shutdown geometry write");
        g_windowMgr.SaveChatWindowPreferences();
    } else {
        Require(g_windowMgr.m_chatWndStatus==7,"write-only source status was reloaded at startup");
        Require(g_windowMgr.m_chatWndHeight==24 && g_windowMgr.m_chatWndX==30 && g_windowMgr.m_chatWndY==294,
            "fresh process lost source physical placement");
        Require(!g_windowMgr.m_chatWndShow && !g_windowMgr.m_chatWndStickOn && g_windowMgr.m_onStSubChat && !g_windowMgr.m_onBtSubChat,
            "fresh process lost source sticky/mini toggles");
        Require(LoadSettingsIniInt("UIWindows","ChatWndStatus",-1)==0,"shutdown did not write source status record");
        // Inspect physical restore independently from the source sticky toggle.
        g_windowMgr.m_chatWndStickOn=true;g_windowMgr.MakeWindow(UIWindowMgr::WID_BASICINFOWND);
        auto* main=g_windowMgr.m_chatWnd;auto* st=g_windowMgr.m_subChatWndST;auto* bt=g_windowMgr.m_subChatWndBT;
        if(main->m_h!=180 || main->m_x!=30 || main->m_y!=138) std::cerr<<"restore actual x/y/h/status: "<<main->m_x<<","<<main->m_y<<","<<main->m_h<<","<<main->GetExpansionState()<<"\n";
        Require(!main->m_show && !g_windowMgr.m_chatWndShow,"basic UI ignored remembered hidden chat");
        Require(main->m_h==180 && main->m_x==30 && main->m_y==138,
            "fresh process did not apply default state7 after cached24px placement");
        Require(st && bt && st->m_x==25 && st->m_y==30 && st->m_w==333 && st->m_h==111
            && bt->m_x==285 && bt->m_y==40 && bt->m_w==345 && bt->m_h==113,
            "fresh process quantized or lost detached geometry");
        g_windowMgr.EnsureChatWindowVisible();Require(main->m_show && g_windowMgr.m_chatWndShow,
            "explicit show failed to update remembered show state");
        g_windowMgr.DeleteWindow(main);Require(!g_windowMgr.m_chatWndShow,"individual chat deletion retained remembered show");
        g_windowMgr.RemoveAllWindows();
    }
    std::cout<<"{\"case_id\":\"chat_window_settings_"<<(write?"write":"read")<<"\",\"actual_startup\":true,\"status_write_only\":true,\"outcome\":\"PASS\"}\n";
}
