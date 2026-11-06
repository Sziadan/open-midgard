void CheckChatScrollTrimUi(QGuiApplication& app,QQuickView& view,QtUiState* state,
    QtUiStateAdapter& adapter,const QString& output)
{
    struct Restore {
        bool sticky=g_windowMgr.m_chatWndStickOn;
        ~Restore(){g_windowMgr.RemoveAllWindows();g_windowMgr.ClearChatEvents();g_windowMgr.m_chatWndStickOn=sticky;}
    } restore;
    g_windowMgr.m_chatWndStickOn=true;g_windowMgr.ClearChatEvents();g_windowMgr.EnsureChatWindowVisible();
    auto* main=g_windowMgr.m_chatWnd;main->SetState(7);main->Move(20,100);main->SetWindowFocused(true);
    for(int i=0;i<240;++i){const auto text="trim row "+std::to_string(i);
        g_windowMgr.PushSourceChatEvent(text.c_str(),0xaaaaaa,0,ro::chat::NoticeRoute::Normal,ro::chat::NoticeType::None);}
    main->ScrollSourceHistory(-31);
    const auto capture=[&](const QString& name,bool textExpected){
        adapter.syncChat();Frames(app,view);
        const auto layout=main->GetLayoutDisplay();
        // Compare the text interior, excluding the QML pane's chrome borders.
        const QRect area(main->m_x+layout.history.left+2,main->m_y+layout.history.top+1,
            layout.history.right-layout.history.left-15,layout.history.bottom-layout.history.top-2);
        const auto qt=view.grabWindow().convertToFormat(QImage::Format_RGB32);
        ArgbDibSurface dib;Require(dib.EnsureSize(view.width(),view.height()),"trim native DIB failed");
        std::fill_n(dib.GetPixels(),view.width()*view.height(),0xff181b20u);main->DrawNativeToHdc(dib.GetDC());
        const auto native=QImage(reinterpret_cast<uchar*>(dib.GetPixels()),view.width(),view.height(),
            view.width()*4,QImage::Format_RGB32).copy();
        const auto ink=[&](const QImage& image){int count=0;
            for(int y=area.top();y<area.top()+area.height();++y)for(int x=area.left();x<area.left()+area.width();++x){
                const auto p=image.pixel(x,y);if(qRed(p)>80 && qGreen(p)>80 && qBlue(p)>80)++count;}
            return count;};
        const int qtInk=ink(qt),nativeInk=ink(native);
        if((qtInk>20)!=textExpected || (nativeInk>20)!=textExpected){
            qt.save(output+"/chat-scroll-failure.qml.png");native.save(output+"/chat-scroll-failure.native.png");
            std::cerr<<"trim capture "<<name.toStdString()<<" qt_ink="<<qtInk<<" native_ink="<<nativeInk
                <<" projected_rows="<<state->chatWindowLines().size()<<" text_first="<<main->GetScrollBarState().firstVisibleLine
                <<" thumb="<<main->GetSourceScrollBar().position<<"\n";
        }
        Require((qtInk>20)==textExpected && (nativeInk>20)==textExpected,
            "native/Qt source stale-view blank/ink pixels differ");
        Require(state->chatWindowLines().isEmpty()!=textExpected,
            "actual Qt row projection reconciled a stale source text offset");
        if(textExpected){auto* row=Find(view.rootObject(),"sourceChatRow0");
            Require(row && row->isVisible() && row->property("text").toString()
                ==QString::fromUtf8(main->GetVisibleLines().front().text.c_str()),"actual source trim text differs");}
        Require(qt.save(output+"/"+name+".qml.png") && native.save(output+"/"+name+".native.png"),"save trim frames failed");
        auto* arrow=Find(view.rootObject(),"chatScrollArrow0");
        Require(arrow && qRound(arrow->width())==13 && qRound(arrow->height())==10,
            "actual source history arrow metrics differ");
    };
    Require(main->GetScrollBarState().firstVisibleLine==200 && main->GetSourceScrollBar().position==200,
        "stale-view fixture did not start scrolled away from bottom");
    capture("chat-scroll-before-trim",true);
    g_windowMgr.PushSourceChatEvent("trim row 240",0xaaaaaa,0,ro::chat::NoticeRoute::Normal,ro::chat::NoticeType::None);
    Require(main->GetSourceHistory(false).size()==121 && main->GetScrollBarState().firstVisibleLine==200
        && main->GetSourceScrollBar().position==111,"source trim failed separate parent/thumb offsets");
    capture("chat-scroll-stale-view",false);
    const auto up=main->GetLayoutDisplay().scrollUp;
    const int x=main->m_x+up.left+1,y=main->m_y+(up.top+up.bottom)/2;
    WindowProc(g_hMainWnd,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(x,y));
    WindowProc(g_hMainWnd,WM_LBUTTONUP,0,MAKELPARAM(x,y));
    Require(main->GetScrollBarState().firstVisibleLine==112 && main->GetSourceScrollBar().position==112
        && main->GetVisibleLines().front().text=="trim row 232" && !GetCapture(),
        "actual up-arrow failed source stale-view reconciliation");
    capture("chat-scroll-reconciled",true);
    QFile result(output+"/chat-scroll-results.json");Require(result.open(QIODevice::WriteOnly),"scroll results open failed");
    result.write(QJsonDocument(QJsonObject{{"retained_rows",121},{"stale_text_first",200},{"trimmed_thumb",111},
        {"arrow_reconciled_first",112},{"history_arrow",10},{"fixed_width",13},
        {"native_qt_blank_view",true},{"actual_arrow_input",true},{"outcome","PASS"}}).toJson());
}
