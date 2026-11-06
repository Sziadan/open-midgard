void ChatScrollBarReplay()
{
    SetCase("chat_source_scrollbar_controls","PK09.21","source-scrollbar-and-scrollbar2",
        "managed main/detached/options -> arrow/page/relative drag -> captured source child lifetime");
    struct Restore {
        std::array<bool,13> options=g_session.m_battleMsgOnOffSet;
        bool sticky=g_windowMgr.m_chatWndStickOn;
        std::array<SubChatWindowInfo,2> info=g_windowMgr.m_subChatInfo;
        ~Restore(){g_windowMgr.RemoveAllWindows();g_windowMgr.ClearChatEvents();
            g_session.m_battleMsgOnOffSet=options;g_windowMgr.m_chatWndStickOn=sticky;g_windowMgr.m_subChatInfo=info;}
    } restore;
    g_windowMgr.RemoveAllWindows();g_windowMgr.ClearChatEvents();
    g_windowMgr.m_chatWndStickOn=true;g_session.m_battleMsgOnOffSet.fill(true);
    g_windowMgr.EnsureChatWindowVisible();auto* main=g_windowMgr.m_chatWnd;
    main->SetState(7);main->Move(20,100);
    for(int i=0;i<240;++i){const auto text="scroll row "+std::to_string(i);
        g_windowMgr.PushSourceChatEvent(text.c_str(),0xaaaaaa,0,ro::chat::NoticeRoute::Normal,ro::chat::NoticeType::None);}
    for(int i=0;i<40;++i)g_windowMgr.PushSourceChatEvent("command scroll row",0xaaaaaa,0,
        ro::chat::NoticeRoute::Command,ro::chat::NoticeType::None);
    auto bar=main->GetSourceScrollBar();
    Require(main->m_h==180 && bar.axis==136 && bar.button==10 && bar.page==9 && bar.maximum==231
        && bar.ThumbSize()==8 && bar.Travel()==108,"main source pixel viewport/scrollbar2 metrics differ");
    auto layout=main->GetLayoutDisplay();
    Require(layout.scrollTrack.right-layout.scrollTrack.left==13 && layout.scrollUp.bottom-layout.scrollUp.top==10,
        "history bar inherited options13px arrow or12px fixed width");
    const auto click=[&](int x,int y){g_windowMgr.OnLBtnDown(x,y);g_windowMgr.OnLBtnUp(x,y);};
    main->ScrollSourceHistory(-131);
    int x=main->m_x+layout.scrollTrack.right-1;
    click(x,main->m_y+layout.scrollUp.top+1);
    Require(main->GetSourceScrollBar().position==99,"main upper arrow did not step one");
    click(x,main->m_y+layout.scrollUp.bottom);
    Require(main->GetSourceScrollBar().position==91,"main page-up did not step8");
    layout=main->GetLayoutDisplay();click(x,main->m_y+layout.scrollThumb.bottom);
    Require(main->GetSourceScrollBar().position==99,"main page-down did not step8");
    click(x,main->m_y+layout.scrollDown.bottom-1);
    Require(main->GetSourceScrollBar().position==100,"main lower arrow did not step one");
    layout=main->GetLayoutDisplay();const int start=main->m_y+layout.scrollThumb.top+2;
    g_windowMgr.OnLBtnDown(x,start);
    g_windowMgr.OnMouseMove(x,start+10);Require(main->GetSourceScrollBar().position==121,"main relative +1 range scale differs");
    g_windowMgr.OnMouseMove(x,start+10);Require(main->GetSourceScrollBar().position==121,"same pointer re-applied movement");
    g_windowMgr.OnMouseMove(x,start-1);Require(main->GetSourceScrollBar().position==98,"main negative drag did not truncate toward zero");
    g_windowMgr.OnMouseMove(x,start+1000);Require(main->GetSourceScrollBar().position==231,"main drag failed upper clamp");
    g_windowMgr.OnMouseMove(x,start+500);Require(main->GetSourceScrollBar().position==231,"overshoot residual was lost");
    g_windowMgr.OnMouseMove(x,start+50);Require(main->GetSourceScrollBar().position==207,"clamped reversal ignored actual movement");
    g_windowMgr.OnLBtnUp(-20,-20);g_windowMgr.OnMouseMove(x,start+30);
    Require(!g_windowMgr.m_captureWindow && main->GetSourceScrollBar().position==207,"released main drag remained active");
    // Capture remains with the old child when ChangeTab moves it off-screen.
    main->ScrollSourceHistory(-1000);layout=main->GetLayoutDisplay();
    const int held=main->m_y+layout.scrollThumb.top+2;
    g_windowMgr.OnLBtnDown(x,held);main->SelectSourceTab(1);g_windowMgr.OnMouseMove(x,held);
    Require(main->GetSourceScrollBar(0).position==231 && main->GetSourceScrollBar(1).position==31,
        "tab change reassigned the captured source scrollbar to another stream");
    g_windowMgr.OnLBtnUp(-20,-20);main->SelectSourceTab(0);main->ScrollSourceHistory(-131);
    layout=main->GetLayoutDisplay();const int compactHeld=main->m_y+layout.scrollThumb.top+2;
    Require(compactHeld==176,"source literal capture origin changed");
    g_windowMgr.OnLBtnDown(x,compactHeld);main->SetState(0);g_windowMgr.OnMouseMove(x,compactHeld);
    Require(main->m_y==256 && main->GetSourceScrollBar(0).axis==136
        && main->GetSourceScrollBar(0).position==19,"compact did not retain/move the captured child viewport");
    g_windowMgr.OnLBtnUp(-20,-20);main->SetState(7);
    main->Resize(600,200);
    Require(main->GetLayoutDisplay().history.bottom-main->GetLayoutDisplay().history.top==136
        && main->GetSourceScrollBar().axis==136,"generic main Resize incorrectly resized source child pixels");
    main->SetState(7);

    auto* detached=g_windowMgr.m_subChatWndST;
    detached->Resize(300,106);detached->Move(20,310);detached->SetShow(1);detached->ScrollHistory(-1000);
    detached->ScrollHistory(100);auto dl=detached->GetHistoryLayout();auto dm=detached->GetScrollModel();
    Require(dm.axis==71 && dm.button==10 && dm.maximum==235 && dm.page==5 && dm.ThumbSize()==8 && dm.Travel()==43,
        "detached source scrollbar2 metrics differ");
    const int dx=detached->m_x+dl.scrollTrack.right-1;
    click(dx,detached->m_y+dl.scrollUp.top+1);Require(detached->GetScrollModel().position==99,"detached arrow step differs");
    click(dx,detached->m_y+dl.scrollUp.bottom);Require(detached->GetScrollModel().position==95,"detached page-up differs");
    dl=detached->GetHistoryLayout();click(dx,detached->m_y+dl.scrollThumb.bottom);
    Require(detached->GetScrollModel().position==99,"detached page-down differs");
    click(dx,detached->m_y+dl.scrollDown.bottom-1);Require(detached->GetScrollModel().position==100,"detached lower arrow differs");
    dl=detached->GetHistoryLayout();const int ds=detached->m_y+dl.scrollThumb.top+2;
    g_windowMgr.OnLBtnDown(dx,ds);g_windowMgr.OnMouseMove(dx,ds+1);
    Require(detached->GetScrollModel().position==105,"detached one-pixel relative drag differs");
    g_windowMgr.OnLBtnUp(-20,-20);

    auto* options=static_cast<UIBattleMsgOptionWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_BATTLEMSGOPTION));
    options->Move(330,40);auto ol=options->GetLayout();auto om=options->GetScrollModel();
    Require(om.axis==162 && om.button==13 && om.maximum==6 && om.ThumbSize()==77 && om.Travel()==59,
        "options inherited history10px arrows or wrong blank-row thumb metrics");
    const auto flags=g_session.m_battleMsgOnOffSet;
    const int ox=options->m_x+ol.scrollTrack.right-1;
    click(ox,options->m_y+ol.scrollDown.bottom-1);Require(options->GetFirstRow()==1,"options lower arrow differs");
    ol=options->GetLayout();click(ox,options->m_y+ol.scrollThumb.bottom);Require(options->GetFirstRow()==6,"options page-down differs");
    click(ox,options->m_y+ol.scrollUp.bottom);Require(options->GetFirstRow()==0,"options page-up differs");
    options->ScrollRows(3);ol=options->GetLayout();const int os=options->m_y+ol.scrollThumb.top+2;
    g_windowMgr.OnLBtnDown(ox,os);g_windowMgr.OnMouseMove(ox,os+8);Require(options->GetFirstRow()==3,"options sub-step rounded up");
    g_windowMgr.OnMouseMove(ox,os+9);Require(options->GetFirstRow()==4,"options +1 drag factor differs");
    g_windowMgr.OnMouseMove(ox,os-1);Require(options->GetFirstRow()==3,"options signed sub-step did not truncate");
    g_windowMgr.OnMouseMove(ox,os-9);Require(options->GetFirstRow()==2,"options negative relative step differs");
    g_windowMgr.OnLBtnUp(-20,-20);
    Require(g_session.m_battleMsgOnOffSet==flags && options->GetSelectedRow()==-1,
        "scrollbar interaction toggled an options row");
    g_windowMgr.DeleteWindow(options);
    // Main and detached visual trims clamp their thumb, not the text offset.
    main->ScrollSourceHistory(-1000);main->ScrollSourceHistory(200);
    g_windowMgr.PushSourceChatEvent("trim newest",0xaaaaaa,0,ro::chat::NoticeRoute::Normal,ro::chat::NoticeType::None);
    Require(main->GetSourceHistory(false).size()==121 && main->GetScrollBarState().firstVisibleLine==200
        && main->GetSourceScrollBar().position==111 && main->GetVisibleLines().empty(),
        "source half-trim incorrectly reconciled stale text with its thumb");
    main->ScrollSourceHistory(-1);
    Require(main->GetScrollBarState().firstVisibleLine==112 && main->GetSourceScrollBar().position==112
        && main->GetVisibleLines().size()==9,"source stale-view scroll did not reconcile from parent offset");
    std::cout<<"{\"case_id\":\"chat_source_scrollbar_controls\",\"history_arrows\":10,\"options_arrows\":13,\"fixed_width\":13,\"relative_drag\":true,\"captured_child_lifetime\":true,\"outcome\":\"PASS\"}\n";
}
