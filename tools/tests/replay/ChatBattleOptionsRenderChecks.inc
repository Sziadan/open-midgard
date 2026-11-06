#include "core/SettingsIni.h"
#include "core/Globals.h"
#include <fstream>

void CheckChatBattleOptionsUi(QGuiApplication& app,QQuickView& view,QtUiState* state,
    QtUiStateAdapter& adapter,const QString& output)
{
    struct Restore {
        std::array<bool,13> flags=g_session.m_battleMsgOnOffSet;
        ServiceType service=g_serviceType;
        int folder=g_readFolderFirst;
        bool sticky=g_windowMgr.m_chatWndStickOn;
        std::filesystem::path cwd=std::filesystem::current_path(),ini=GetOpenMidgardIniPath();
        bool existed=std::filesystem::exists(ini);
        std::string bytes;
        Restore(){if(existed){std::ifstream f(ini,std::ios::binary);bytes.assign(std::istreambuf_iterator<char>(f),{});}}
        ~Restore(){g_windowMgr.RemoveAllWindows();g_session.m_battleMsgOnOffSet=flags;g_serviceType=service;g_windowMgr.m_chatWndStickOn=sticky;
            g_readFolderFirst=folder;std::filesystem::current_path(cwd);
            if(existed){std::ofstream f(ini,std::ios::binary|std::ios::trunc);f<<bytes;}
            else std::filesystem::remove(ini);}
    } restore;
    const auto language=std::filesystem::path(output.toStdString())/"options-language";
    std::filesystem::create_directories(language);
    {std::ofstream f(language/"msgstringtable.txt",std::ios::binary);
        for(int i=0;i<=1331;++i){f<<"FILTER LABEL "<<i<<"#\r\n";}}
    std::filesystem::current_path(language);g_readFolderFirst=1;g_serviceType=ServiceKorea;
    g_session.m_battleMsgOnOffSet.fill(true);SaveSettingsIniInt("ChatOption","0",1);
    g_windowMgr.m_chatWndStickOn=true;
    g_windowMgr.EnsureChatWindowVisible();auto* main=g_windowMgr.m_chatWnd;
    auto* bt=static_cast<UISubChatHisWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_SUBCHATBT));
    g_windowMgr.MakeWindow(UIWindowMgr::WID_SUBCHATST);g_windowMgr.MakeWindow(UIWindowMgr::WID_SUBCHATMINI);
    main->SetState(7);main->Move(20,100);
    const auto pointer=[&](int x,int y){
        WindowProc(g_hMainWnd,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(x,y));
        WindowProc(g_hMainWnd,WM_LBUTTONUP,0,MAKELPARAM(x,y));
    };
    const auto button=main->GetLayoutDisplay().battleOptionButton;
    pointer(main->m_x+button.left+6,main->m_y+button.top);
    auto* options=g_windowMgr.m_battleMsgOptionWnd;
    Require(options && options->m_w==280 && options->m_h==200,"actual source127 pointer failed to open139");
    const auto client=g_windowMgr.GetChatClientRect();
    Require(options->m_x==400+(client.right-640)/2 && options->m_y==200*client.bottom/480,
        "source default options UIX/UICY placement differs");
    const auto capture=[&](const QString& name){
        adapter.syncChat();Frames(app,view);
        auto* panel=Find(view.rootObject(),"chatBattleOptionsWindow");
        Require(panel && panel->isVisible() && qRound(panel->width())==280 && qRound(panel->height())==200,
            "actual options QML dimensions/visibility differ");
        const auto position=panel->mapToScene(QPointF(0,0));
        Require(qRound(position.x())==options->m_x && qRound(position.y())==options->m_y,
            "actual options QML position differs from native");
        auto* first=Find(view.rootObject(),QString("battleOptionRow%1").arg(options->GetFirstRow()));
        Require(first && first->isVisible(),"actual options first row missing");
        const auto expectedLabel=QString("FILTER LABEL %1").arg(ro::chat::BattleOptionMessageIds[options->GetFirstRow()]);
        Require(QString::fromUtf8(options->GetLabel(options->GetFirstRow()).c_str())==expectedLabel,
            "actual options resource loaded the wrong positional label");
        QQuickItem* label=nullptr;
        for(auto* child:first->childItems())if(child->property("text").toString()==expectedLabel)label=child;
        Require(label && label->isVisible(),"actual QML option label differs from source table");
        const auto labelPosition=label->mapToScene(QPointF(0,0));
        Require(qRound(labelPosition.x())==options->m_x+50 && qRound(labelPosition.y())==options->m_y+22,
            "actual QML option text source offset differs");
        const auto rowPosition=first->mapToScene(QPointF(0,0));
        Require(qRound(rowPosition.y())==options->m_y+19,"source selected-background row offset differs");
        const auto qt=view.grabWindow().convertToFormat(QImage::Format_RGB32);
        ArgbDibSurface dib;Require(dib.EnsureSize(view.width(),view.height()),"options native DIB allocation failed");
        std::fill_n(dib.GetPixels(),view.width()*view.height(),0xff181b20u);options->DrawToHdc(dib.GetDC());
        const auto native=QImage(reinterpret_cast<uchar*>(dib.GetPixels()),view.width(),view.height(),
            view.width()*4,QImage::Format_RGB32).copy();
        const auto green=[](QRgb p){return qRed(p)<80 && qGreen(p)>70 && qGreen(p)<150 && qBlue(p)<90;};
        for(int i=0;i<8 && options->GetFirstRow()+i<13;++i){
            const int row=options->GetFirstRow()+i,x=options->m_x+30,y=options->m_y+25+i*20;
            Require(green(qt.pixel(x,y))==g_session.m_battleMsgOnOffSet[row]
                && green(native.pixel(x,y))==g_session.m_battleMsgOnOffSet[row],
                "native/Qt checkbox pixels or peer window stacking differ from session filter");
        }
        int blackQt=0,blackNative=0;
        for(int yy=options->m_y+22;yy<options->m_y+40;++yy)
            for(int xx=options->m_x+50;xx<options->m_x+240;++xx){
                const auto a=qt.pixel(xx,yy),b=native.pixel(xx,yy);
                if(qRed(a)<60 && qGreen(a)<60 && qBlue(a)<60)++blackQt;
                if(qRed(b)<60 && qGreen(b)<60 && qBlue(b)<60)++blackNative;
            }
        Require(blackQt>20 && blackNative>20,"localized option label did not paint native/Qt text pixels");
        Require(qt.save(output+"/"+name+".qml.png") && native.save(output+"/"+name+".native.png"),
            "save options render frames failed");
    };
    const auto tapOptionArrow=[&](RECT r){pointer(options->m_x+r.left+1,options->m_y+(r.top+r.bottom)/2);};
    tapOptionArrow(options->GetLayout().scrollDown);Require(options->GetFirstRow()==1,"actual options lower arrow differs");
    tapOptionArrow(options->GetLayout().scrollUp);Require(options->GetFirstRow()==0 && !GetCapture(),"actual options upper arrow/release differs");
    capture("chat-options-default");
    pointer(options->m_x+50,options->m_y+17);
    Require(!g_session.m_battleMsgOnOffSet[0] && options->GetSelectedRow()==0
        && LoadSettingsIniInt("ChatOption","0",-1)==1,"actual row toggle changed wrong filter or persisted early");
    capture("chat-options-unchecked");
    auto bar=options->GetLayout();
    const int thumbX=options->m_x+bar.scrollThumb.left+2,thumbY=options->m_y+bar.scrollThumb.top+2;
    WindowProc(g_hMainWnd,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(thumbX,thumbY));
    Require(GetCapture()==g_hMainWnd && g_windowMgr.m_captureWindow==options,
        "options thumb did not acquire real OS capture");
    WindowProc(g_hMainWnd,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(thumbX,options->m_y+bar.scrollTrack.bottom-2));
    WindowProc(g_hMainWnd,WM_LBUTTONUP,0,MAKELPARAM(-20,-20));
    Require(options->GetFirstRow()==6 && !GetCapture() && !g_windowMgr.m_captureWindow,
        "options last blank scroll row/outside release differs");
    capture("chat-options-last-page");
    Require(!Find(view.rootObject(),"battleOptionRow13"),"source blank final row fabricated a checkbox");
    // Escape arriving through the actual common Qt editor must close options
    // before the chat editor clears its draft/focus.
    main->SetQtInputText(UINewChatWnd::InputField_Message,"options Escape preserves draft");
    main->FocusQtInput(UINewChatWnd::InputField_Message);adapter.syncChat();Frames(app,view);
    auto* editor=Find(view.rootObject(),"chatMessageEditor");Require(editor,"actual common editor missing");
    editor->forceActiveFocus();
    QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);
    QCoreApplication::sendEvent(&view,&escape);Frames(app,view);
    Require(!g_windowMgr.m_battleMsgOptionWnd && main->GetInputText()=="options Escape preserves draft"
        && main->IsMessageInputActive() && LoadSettingsIniInt("ChatOption","0",-1)==0,
        "actual Qt editor Escape failed source135 save/close priority");
    main->StickOnOff(false);Require(main->GetExpansionState()==0,"options fixture detach failed");g_windowMgr.m_onBtSubChat=true;g_windowMgr.UpdateChatAuxiliaryVisibility();
    bt->Move(350,380);
    const auto detachedButton=bt->GetBattleOptionButton();
    pointer(bt->m_x+detachedButton.left+6,bt->m_y+detachedButton.top+6);
    options=g_windowMgr.m_battleMsgOptionWnd;Require(options,"actual detached option button failed");
    capture("chat-options-detached");
    g_windowMgr.DeleteWindow(main);capture("chat-options-without-main");
    const int oldX=options->m_x,oldY=options->m_y;
    WindowProc(g_hMainWnd,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(oldX+70,oldY+8));
    WindowProc(g_hMainWnd,WM_CANCELMODE,0,0);
    WindowProc(g_hMainWnd,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(oldX+100,oldY+80));
    Require(!GetCapture() && !g_windowMgr.m_captureWindow && options->m_x==oldX && options->m_y==oldY,
        "options canceled title capture kept dragging");
    // Source dimensions remain logical when the QML scene changes scale.
    state->setUiScale(1.25);Frames(app,view);
    auto* scaled=Find(view.rootObject(),"chatBattleOptionsWindow");
    const auto scaledRect=scaled->mapRectToScene(scaled->boundingRect());
    Require(qAbs(scaledRect.width()-350)<1 && qAbs(scaledRect.height()-250)<1
        && qAbs(scaledRect.x()-options->m_x*1.25)<1,"options escaped the shared Qt scale layer");
    auto* scaledHistory=Find(view.rootObject(),"detachedChatHistory1");
    auto* scaledMini=Find(view.rootObject(),"detachedChatMini");
    Require(scaledHistory && scaledMini
        && qAbs(scaledHistory->mapRectToScene(scaledHistory->boundingRect()).width()-bt->m_w*1.25)<1
        && qAbs(scaledMini->mapRectToScene(scaledMini->boundingRect()).width()-80)<1,
        "detached histories/mini escaped the shared chat scale layer");
    Require(view.grabWindow().save(output+"/chat-options-125.qml.png"),"save125% options frame failed");
    state->setUiScale(1.0);Frames(app,view);
    g_windowMgr.OnKeyDown(VK_ESCAPE);Require(!g_windowMgr.m_battleMsgOptionWnd,"native Escape failed without main");
    pointer(bt->m_x+detachedButton.left+6,bt->m_y+detachedButton.top+6);
    options=g_windowMgr.m_battleMsgOptionWnd;Require(options,"reopen without main failed");
    const auto close=options->GetLayout().close;
    pointer(options->m_x+close.left+6,options->m_y+close.top+6);
    Require(!g_windowMgr.m_battleMsgOptionWnd && !g_windowMgr.m_captureWindow && !GetCapture(),
        "mouse-down close135 left a deleted owner or orphaned OS capture");
    QFile result(output+"/chat-options-results.json");Require(result.open(QIODevice::WriteOnly),"options results open failed");
    result.write(QJsonDocument(QJsonObject{{"filters",13},{"siege_message_id",1331},{"rows",8},{"last_first",6},
        {"native_qt_checkbox_pixels",true},{"actual_editor_escape",true},{"close_persistence",true},
        {"real_capture_cancel",true},{"mouse_close_capture",true},{"peer_stacking",true},{"scale125_geometry",true},{"absent_main",true},{"outcome","PASS"}}).toJson());
}
