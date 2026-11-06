void ChatPrimaryViewReplay()
{
    SetCase("chat_primary_source_view", "PK09.21", "source-two-tab-view-literals",
        "managed source histories -> primary selection/scroll/page/trim -> shared native/Qt rows");
    struct Restore {
        ServiceType service = g_serviceType;
        bool sticky = g_windowMgr.m_chatWndStickOn;
        std::string name = g_session.GetPlayerName();
        std::array<bool,13> options = g_session.m_battleMsgOnOffSet;
        ~Restore() {
            g_windowMgr.RemoveAllWindows(); g_windowMgr.ClearChatEvents();
            g_serviceType = service; g_windowMgr.m_chatWndStickOn = sticky;
            g_session.m_battleMsgOnOffSet = options;
            std::snprintf(g_session.m_playerName,sizeof(g_session.m_playerName),"%s",name.c_str());
        }
    } restore;
    g_windowMgr.RemoveAllWindows(); g_windowMgr.ClearChatEvents();
    for (const auto pair : {std::pair<ServiceType,int>{ServiceKorea,71}, {ServiceAmerica,110},
        {ServicePhilippine,110}, {ServiceIndia,110}, {ServiceGermany,71}, {ServiceFrance,90}}) {
        g_serviceType = pair.first;
        UINewChatWnd standalone;
        standalone.SetState(7);
        const auto layout = standalone.GetLayoutDisplay();
        Require(standalone.GetTabCount() == 2 && standalone.GetSourceTab() == 0
            && layout.tabs[0].left == 1 && layout.tabs[0].right == pair.second - 1
            && layout.tabs[1].left == pair.second + 2 && layout.tabs[1].right == pair.second * 2 - 1,
            "source regional two-tab hit rectangles changed");
        g_serviceType = ServiceKorea;
        Require(standalone.GetLayoutDisplay().tabs[0].right == pair.second - 1,
            "live service change rewrote constructor-owned title width");
        Require(!g_windowMgr.m_subChatMiniWnd && !g_windowMgr.m_subChatWndST,
            "unmanaged main constructed manager auxiliaries");
    }
    g_serviceType = ServiceAmerica;
    std::snprintf(g_session.m_playerName,sizeof(g_session.m_playerName),"%s","PrimarySelf");
    g_windowMgr.m_chatWndStickOn = true;
    g_session.m_battleMsgOnOffSet.fill(true);
    g_windowMgr.EnsureChatWindowVisible();
    auto* chat = g_windowMgr.m_chatWnd;
    chat->SetState(7); chat->Move(20, 100);
    for (int i = 0; i < 100; ++i) {
        const auto text = "source-" + std::to_string(i);
        g_windowMgr.PushSourceChatEvent(text.c_str(),0xff2020,6,ro::chat::NoticeRoute::Normal,ro::chat::NoticeType::None,"Peer");
    }
    Require(chat->GetScrollBarState().totalLines == 100 && chat->GetScrollBarState().firstVisibleLine == 91
        && chat->GetVisibleLines().size() == 9 && chat->GetVisibleLines().front().text == "source-91",
        "primary view did not use source14px rows and follow bottom");
    chat->ScrollSourceHistory(-20);
    g_windowMgr.PushSourceChatEvent("source-100",0xff2020,6,ro::chat::NoticeRoute::Normal,ro::chat::NoticeType::None,"Peer");
    Require(chat->GetScrollBarState().firstVisibleLine == 71, "append moved an upward-scrolled source view");
    for (int i = 101; i < 240; ++i) {
        const auto text = "source-" + std::to_string(i);
        g_windowMgr.PushSourceChatEvent(text.c_str(),0xff2020,6,ro::chat::NoticeRoute::Normal,ro::chat::NoticeType::None,"Peer");
    }
    Require(chat->GetLines().size() == 50 && chat->GetScrollBarState().totalLines == 240,
        "primary source history was capped by the modern50-event receipt log");
    chat->ScrollSourceHistory(-1000);
    Require(chat->GetVisibleLines().front().text == "source-0", "primary cannot render source rows older than receipt cache");
    for (int i = 0; i < 270; ++i) g_windowMgr.PushChatEvent("UNCLASSIFIED MODERN RECEIPT",0x20ff20,0);
    Require(g_windowMgr.GetChatEvents().size() == 256 && chat->GetLines().size() == 50,
        "fixture confused manager receipt retention with the old primary50-line view");
    Require(chat->GetVisibleLines().front().text == "source-0" && chat->GetFilteredLines().size() == 240,
        "unclassified receipt was guessed into a source stream");
    for (int i = 0; i < 40; ++i) {
        const auto text = "command-" + std::to_string(i);
        g_windowMgr.PushSourceChatEvent(text.c_str(),0x20ff20,0,ro::chat::NoticeRoute::Command,ro::chat::NoticeType::None,"CommandPeer");
    }
    chat->SelectSourceTab(1);
    Require(chat->GetScrollBarState().totalLines == 40 && chat->GetScrollBarState().firstVisibleLine == 31,
        "ChangeTab did not resize/snap selected source history");
    chat->ScrollSourceHistory(-5);
    chat->SelectSourceTab(1);
    Require(chat->GetScrollBarState().firstVisibleLine == 26, "same-tab click reset source scroll position");
    chat->SendMsg(nullptr,3,0,0,0);
    Require(chat->GetScrollBarState().firstVisibleLine == 18, "source page-up did not step visibleRows-1");
    chat->SendMsg(nullptr,4,0,0,0);
    Require(chat->GetScrollBarState().firstVisibleLine == 26, "source page-down did not step visibleRows-1");
    chat->SelectSourceTab(0);
    Require(chat->GetScrollBarState().firstVisibleLine == 231, "tab change preserved scroll instead of source virtual resize");
    // At y18 the real history child overlaps the painted tabs and owns the hit.
    const auto overlap = chat->GetLayoutDisplay();
    const int overlapX = chat->m_x + overlap.tabs[1].left + 5;
    const int overlapY = chat->m_y + overlap.history.top;
    chat->HandleQtMouseDown(overlapX,overlapY); chat->HandleQtMouseUp(overlapX,overlapY);
    Require(chat->GetSourceTab() == 0 && chat->GetSourceSelectedRow() == 231,
        "painted tab stole the source history child's overlapping y18 hit");
    chat->ScrollSourceHistory(-1000);
    const auto bounds = chat->GetLayoutDisplay().history;
    const int x = chat->m_x + bounds.left + 5, y = chat->m_y + bounds.top + 7;
    const int oldX = chat->m_x, oldY = chat->m_y;
    chat->HandleQtMouseDown(x,y); chat->HandleQtMouseMove(x+40,y+40); chat->HandleQtMouseUp(x+40,y+40);
    Require(chat->GetSourceSelectedRow() == 0 && chat->m_x == oldX && chat->m_y == oldY,
        "source row selection moved the frame");
    chat->SetQtInputText(UINewChatWnd::InputField_Message,"draft survives target selection");
    chat->OnLBtnDblClk(x,y);
    Require(chat->GetWhisperTargetText() == "Peer" && chat->IsMessageInputActive()
        && chat->GetInputText() == "draft survives target selection", "selected talker did not target whisper/common edit");
    // Source child borders cannot become modern frame-resize gestures.
    const int childWidth=chat->m_w,childHeight=chat->m_h;
    const auto childLayout=chat->GetLayoutDisplay();
    const int childX=chat->m_x+childLayout.history.right-1;
    chat->HandleQtMouseDown(childX,y);chat->HandleQtMouseMove(childX+30,y);chat->HandleQtMouseUp(childX+30,y);
    Require(chat->m_w==childWidth && chat->m_h==childHeight,
        "history child right edge was stolen by frame resize");
    const int editX=chat->m_x+childLayout.whisperInput.left+5;
    const int editY=chat->m_y+childLayout.whisperInput.bottom-1;
    chat->HandleQtMouseDown(editX,editY);chat->HandleQtMouseMove(editX+20,editY+20);chat->HandleQtMouseUp(editX+20,editY+20);
    Require(chat->IsWhisperTargetActive() && chat->m_w==childWidth && chat->m_h==childHeight,
        "editor bottom edge was stolen by frame resize");
    chat->OnLBtnDblClk(x,y);
    // A held double-click remains history-child capture and must not trigger the
    // main-frame capture gate for manager state operations.
    g_windowMgr.m_captureWindow=chat;
    chat->SetState(8);
    Require(chat->GetExpansionState()==8,"history doubleclick was mistaken for frame capture");
    chat->HandleQtMouseUp(x,y);g_windowMgr.m_captureWindow=nullptr;
    chat->SetState(7);
    // Double-clicking the scrollbar must not target the selected row's talker.
    chat->SetQtInputText(UINewChatWnd::InputField_WhisperTarget,"KeepTarget");
    chat->OnLBtnDblClk(chat->m_x+chat->GetLayoutDisplay().scrollTrack.left+2,y);
    Require(chat->GetWhisperTargetText()=="KeepTarget","scrollbar doubleclick targeted a history talker");
    chat->SetQtInputText(UINewChatWnd::InputField_WhisperTarget,"Peer");
    chat->ScrollSourceHistory(-1000);
    // Preserve numeric main selection across trim, but never dereference a stale index.
    chat->ScrollSourceHistory(230);
    chat->HandleQtMouseDown(x,y); chat->HandleQtMouseUp(x,y);
    Require(chat->GetSourceSelectedRow() == 230, "scrolled row hit did not select source numeric index");
    g_windowMgr.PushSourceChatEvent("trimmed",0xff2020,6,ro::chat::NoticeRoute::Normal,ro::chat::NoticeType::None,"OtherPeer");
    Require(chat->GetSourceHistory(false).size() == 121 && chat->GetSourceSelectedRow() == 230,
        "main trim reset numeric selection instead of preserving source state");
    chat->OnLBtnDblClk(x,y);
    Require(chat->GetWhisperTargetText() == "Peer", "stale trimmed row dereferenced a new/out-of-range talker");
    chat->SelectSourceTab(1); chat->StickOnOff(false); chat->StickOnOff(true);
    Require(chat->GetSourceTab() == 0 && chat->GetSourceHistory(true).size() == 40,
        "reattach did not reset source tab or destroyed other history");
    std::cout << "{\"case_id\":\"chat_primary_source_view\",\"status\":\"PASS\",\"source_rows\":240,\"row_height\":14,\"tabs\":2,\"source_resize_snap\":true}\n";
}
