void ChatDetachedViewReplay()
{
    SetCase("chat_detached_visual_history","PK09.21","source-wrapped-detached-owners",
        "managed detached owner -> logical/visual trim -> shared view/input");
    struct Restore {
        bool sticky=g_windowMgr.m_chatWndStickOn;
        std::array<SubChatWindowInfo,2> info=g_windowMgr.m_subChatInfo;
        ~Restore(){g_windowMgr.RemoveAllWindows();g_windowMgr.ClearChatEvents();
            g_windowMgr.m_chatWndStickOn=sticky;g_windowMgr.m_subChatInfo=info;}
    } restore;
    g_windowMgr.RemoveAllWindows();g_windowMgr.ClearChatEvents();
    g_windowMgr.m_chatWndStickOn=true;
    g_windowMgr.EnsureChatWindowVisible();
    auto* main=g_windowMgr.m_chatWnd;
    auto* st=g_windowMgr.m_subChatWndST;
    auto* bt=g_windowMgr.m_subChatWndBT;
    Require(main && st && bt,"detached fixture owners missing");
    st->Resize(300,95);st->Move(20,30);st->SetShow(1);
    main->SetQtInputText(UINewChatWnd::InputField_Message,"detached target keeps draft");
    for (int i=0;i<121;++i) {
        const auto text="head-"+std::to_string(i)+"\r\ntail-"+std::to_string(i);
        const auto talker="Peer"+std::to_string(i);
        st->Append(text.c_str(),0xff2020,talker.c_str());
    }
    Require(st->GetRows().size()==121 && st->GetVisualRows().size()==122
        && st->GetVisualRows().front().text=="head-60" && st->GetVisualRows().back().text=="tail-120"
        && st->GetVisualRows().front().talker=="Peer60" && bt->GetRows().empty(),
        "visual240 half-trim was conflated with logical message retention");
    Require(st->GetHistoryLayout().history.top==20 && st->GetHistoryLayout().visible==4
        && st->GetHistoryLayout().first==118,"source detached list geometry/bottom differs");
    st->ScrollHistory(-1000);st->OnWheel(-120);
    Require(st->GetHistoryLayout().first==3,"detached wheel did not move three visual rows");
    st->SendMsg(nullptr,4,0,0,0);
    Require(st->GetHistoryLayout().first==6,"detached page-down did not use visibleRows-1");
    st->OnWheel(0);
    Require(st->GetHistoryLayout().first==3,"source zero wheel is upward");
    st->ScrollHistory(-1000);
    st->OnLBtnDown(30,57);st->OnLBtnUp(30,57);
    Require(st->GetSelectedRow()==0,"detached source row selection differs");
    st->OnLBtnDblClk(30,57);
    Require(main->GetWhisperTargetText()=="Peer60" && main->IsMessageInputActive()
        && main->GetInputText()=="detached target keeps draft","detached talker target lost common draft/focus");
    st->Resize(300,109);
    Require(st->GetSelectedRow()==0 && st->GetHistoryLayout().first==117,
        "generic height resize should snap bottom while retaining selected visual index");
    st->ScrollHistory(-1000);
    st->SendMsg(nullptr,14,7,300,109); // snaps to280x106, rebuilding on source resizer path
    Require(st->m_w==280 && st->m_h==106 && st->GetSelectedRow()==-1,
        "source resize gesture did not rebuild/reset selected visual row");
    const int first=st->GetHistoryLayout().first;
    st->SendMsg(nullptr,14,7,280,106);
    Require(st->GetHistoryLayout().first==first,"same quantized resize changed view");
    st->ScrollHistory(-1000);st->OnLBtnDown(30,57);st->OnLBtnUp(30,57);
    for (int i=121;i<241;++i) {
        const auto text="head-"+std::to_string(i)+"\r\ntail-"+std::to_string(i);
        st->Append(text.c_str(),0xff2020,"Later");
    }
    Require(st->GetRows().size()==121 && st->GetRows().front().text=="head-120\r\ntail-120"
        && st->GetVisualRows().size()==122 && st->GetVisualRows().front().text=="head-180"
        && st->GetSelectedRow()==-1,"logical trim failed to rebuild independent visual cap/selection");
    const std::string longText(150,'M');
    bt->Resize(280,106);bt->Append(longText.c_str(),0x20ff20,"LongPeer");
    const auto narrow=bt->GetVisualRows();
    std::string joined;for(const auto& row:narrow) {joined+=row.text;Require(row.talker=="LongPeer","wrapped talker lost");}
    Require(narrow.size()>1 && joined==longText,"detached AddItem2 failed pixel wrapping or lost bytes");
    bt->Resize(504,106);
    Require(bt->GetVisualRows().size()<narrow.size() && bt->GetRows().size()==1,
        "width resize did not rewrap retained logical message");
    bt->SendMsg(nullptr,37,reinterpret_cast<msgparam_t>("direct COLORREF red"),255,
        reinterpret_cast<msgparam_t>("DirectPeer"));
    Require(bt->GetRows().back().color==0xff0000 && bt->GetVisualRows().back().color==0xff0000
        && bt->GetRows().back().talker=="DirectPeer","direct detached37 lost COLORREF conversion/talker");
    // Retained owners stay interactive after individual main deletion; target
    // helper is guarded by main presence and must not resurrect it.
    g_windowMgr.DeleteWindow(main);
    st->ScrollHistory(-1000);st->OnLBtnDown(30,57);st->OnLBtnUp(30,57);st->OnLBtnDblClk(30,57);
    Require(!g_windowMgr.m_chatWnd && g_windowMgr.m_subChatWndST==st,
        "absent-main detached talker action recreated a main");
    std::cout<<"{\"case_id\":\"chat_detached_visual_history\",\"logical_rows\":121,\"visual_rows\":122,\"independent_caps\":true,\"outcome\":\"PASS\"}\n";
}
