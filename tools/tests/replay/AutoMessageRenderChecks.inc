#include "ui/UIAutoMessageWnd.h"

void CheckAutoMessageUi(QGuiApplication& app,QQuickView& view,QtUiState* state,
    CGameMode& mode,QtUiStateAdapter& adapter,const QString& output)
{
    g_windowMgr.RemoveAllWindows();
    struct Restore {
        bool enabled=g_session.m_onAutoMSG;std::string text=g_session.m_strAutoMSG;
        int scale=GetConfiguredUiScalePercent(),folder=g_readFolderFirst,codePage=g_codePage;
        std::filesystem::path cwd=std::filesystem::current_path();SubChatWindowInfo geometry=g_windowMgr.m_whisperWindowInfo;
        ~Restore(){g_windowMgr.RemoveAllWindows();g_session.m_onAutoMSG=enabled;g_session.m_strAutoMSG=text;SetRuntimeUiScalePercent(scale);
            g_readFolderFirst=folder;g_codePage=codePage;std::filesystem::current_path(cwd);g_windowMgr.m_whisperWindowInfo=geometry;}
    } restore;
    // Reuse declared source-key button fixtures created by the entry phase.
    std::filesystem::current_path(std::filesystem::path(output.toStdString())/"entry-assets");g_readFolderFirst=1;g_codePage=65001;
    const auto language=[](bool empty){std::ofstream file("msgstringtable.txt",std::ios::binary);
        for(int i=0;i<=944;++i)file<<(i==942?"Auto enabled#\r\n":i==943?"Auto disabled#\r\n":i==944&&!empty?"Automatic <reply> prompt#\r\n":"#\r\n");
    };
    language(false);SetRuntimeUiScalePercent(100);g_session.m_onAutoMSG=false;g_session.m_strAutoMSG="Preset <reply>";
    Require(mode.ToggleSourceAutoMessage(),"native auto-message enable failed");auto* editor=g_windowMgr.m_autoMessageWnd;
    const auto client=g_windowMgr.GetChatClientRect();
    Require(editor && editor->m_x==185+(client.right-640)/2 && editor->m_y==180+(client.bottom-480)/2
        && editor->GetButton().bitmaps[0].IsValid() && editor->GetButton().bitmaps[0].width==43,"auto source placement/button asset lookup changed");
    const auto pointer=[&](UINT message,int x,int y){WindowProc(g_hMainWnd,message,message==WM_LBUTTONUP?0:MK_LBUTTON,
        MAKELPARAM(UiScaleLogicalToRawCoordinate(x),UiScaleLogicalToRawCoordinate(y)));};
    const auto capture=[&](const QString& name){
        state->setUiScale(GetConfiguredUiScaleFactor());adapter.syncChat();Frames(app,view);
        auto* item=Find(view.rootObject(),"sourceAutoMessage");auto* edit=Find(view.rootObject(),"sourceAutoEdit");auto* prompt=Find(view.rootObject(),"sourceAutoPrompt");
        Require(item && edit && prompt && item->isVisible(),"actual QML auto-message controls missing");
        const auto bounds=item->mapRectToScene(item->boundingRect());const auto child=edit->mapRectToScene(edit->boundingRect());const double scale=GetConfiguredUiScaleFactor();
        Require(std::abs(bounds.left()-editor->m_x*scale)<0.001 && std::abs(bounds.top()-editor->m_y*scale)<0.001
            && std::abs(bounds.width()-280*scale)<0.001 && std::abs(bounds.height()-48*scale)<0.001
            && std::abs(child.left()-(editor->m_x+15)*scale)<0.001 && std::abs(child.width()-203*scale)<0.001,
            "actual native/QML auto editor geometry differs");
        Require(prompt->property("text").toString()==QString::fromUtf8(editor->GetPrompt().c_str())
            && state->chatWindowUi().value("autoMessage").toMap().value("input")==QString::fromUtf8(editor->GetInputText().c_str())
            && !state->chatWindowInputActive() && !g_windowMgr.CanRouteQtChatInput(),"auto prompt/draft projection or main input gate differs");
        ArgbDibSurface dib;Require(dib.EnsureSize(view.width(),view.height()),"auto native DIB failed");std::fill_n(dib.GetPixels(),view.width()*view.height(),0xff181b20u);
        g_windowMgr.DrawVisibleWindowsToHdc(dib.GetDC(),true);
        auto native=QImage(reinterpret_cast<uchar*>(dib.GetPixels()),view.width(),view.height(),view.width()*4,QImage::Format_RGB32).copy();
        native=native.scaled(qRound(native.width()*scale),qRound(native.height()*scale),Qt::IgnoreAspectRatio,Qt::FastTransformation).copy(0,0,view.width(),view.height());
        const auto qt=view.grabWindow().convertToFormat(QImage::Format_RGB32);
        const auto& button=editor->GetButton();const QPoint sample(UiScaleLogicalToRawCoordinate(editor->m_x+button.x+5),UiScaleLogicalToRawCoordinate(editor->m_y+button.y+5));
        const QColor expected=editor->GetButtonState()==2?QColor("#806040"):editor->GetButtonState()==1?QColor("#80a0c0"):QColor("#c0a080");
        Require(native.pixelColor(sample)==expected && qt.pixelColor(sample)==expected,"auto native/QML button asset/state pixels differ");
        Require(native.save(output+"/"+name+".native.png") && qt.save(output+"/"+name+".qml.png"),"auto frames save failed");
    };
    capture("auto-message-selected");WindowProc(g_hMainWnd,WM_CHAR,'X',0);
    Require(editor->GetInputText()=="X" && g_session.m_strAutoMSG=="Preset <reply>","native auto selection replacement committed prematurely");
    const auto button=editor->GetButton().Bounds();pointer(WM_LBUTTONDOWN,editor->m_x+button.left+5,editor->m_y+button.top+5);
    Require(GetCapture()==g_hMainWnd && g_windowMgr.m_captureWindow==editor,"auto OK press lacks native capture");capture("auto-message-pressed");
    pointer(WM_LBUTTONUP,-20,-20);Require(g_windowMgr.m_autoMessageWnd==editor && g_session.m_strAutoMSG=="Preset <reply>" && !GetCapture(),"auto outside OK release committed or retained capture");
    const int oldX=editor->m_x,oldY=editor->m_y;pointer(WM_LBUTTONDOWN,oldX+2,oldY+2);pointer(WM_MOUSEMOVE,oldX+27,oldY+17);pointer(WM_LBUTTONUP,-20,-20);
    Require(editor->m_x==oldX+25 && editor->m_y==oldY+15,"native auto frame drag failed");
    pointer(WM_LBUTTONDOWN,editor->m_x+2,editor->m_y+2);WindowProc(g_hMainWnd,WM_CANCELMODE,0,0);pointer(WM_MOUSEMOVE,oldX+90,oldY+90);
    Require(!GetCapture() && !g_windowMgr.m_captureWindow && editor->m_x==oldX+25 && editor->m_y==oldY+15,"cancelled auto frame kept moving");
    SetRuntimeUiScalePercent(125);capture("auto-message-125");
    g_windowMgr.OpenWhisperWindow("FocusArrival");auto* privateWindow=g_windowMgr.m_whisperWindows.at("FocusArrival");
    Require(g_windowMgr.m_editWindow==privateWindow,"auto focus fixture did not transfer to new private owner");adapter.syncChat();
    Require(g_windowMgr.m_editWindow==editor,"auto source draw focus was not restored by actual Qt synchronization");
    WindowProc(g_hMainWnd,WM_CHAR,'Y',0);Require(editor->GetInputText()=="XY" && privateWindow->GetInputText().empty(),"auto reasserted focus leaked native character into private owner");
    g_windowMgr.DeleteWindow(privateWindow);g_windowMgr.ShowRoomValidationMessage(13);adapter.syncChat();Frames(app,view);
    WindowProc(g_hMainWnd,WM_CHAR,'Z',0);Require(editor->GetInputText()=="XY","active modal leaked text into auto editor");
    WindowProc(g_hMainWnd,WM_KEYDOWN,VK_ESCAPE,0);Require(g_windowMgr.m_autoMessageWnd==editor && g_session.m_onAutoMSG && !g_windowMgr.m_modalWindow,"modal Escape cancelled auto editor");
    const auto relative=editor->GetButton().Bounds();pointer(WM_LBUTTONDOWN,editor->m_x+relative.left+5,editor->m_y+relative.top+5);
    pointer(WM_LBUTTONUP,editor->m_x+relative.left+5,editor->m_y+relative.top+5);
    Require(!g_windowMgr.m_autoMessageWnd && g_session.m_onAutoMSG && g_session.m_strAutoMSG=="XY" && !GetCapture(),"native125 OK failed commit/retained capture or disabled auto");
    language(true);editor=static_cast<UIAutoMessageWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_AUTOMESSAGEWND));capture("auto-message-empty-prompt");
    Require(editor->GetPrompt().empty(),"actual QML known-empty prompt used a fallback");WindowProc(g_hMainWnd,WM_KEYDOWN,VK_ESCAPE,0);
    adapter.syncChat();Frames(app,view);Require(!g_windowMgr.m_autoMessageWnd && !g_session.m_onAutoMSG && g_session.m_strAutoMSG=="XY"
        && !Find(view.rootObject(),"sourceAutoMessage")->isVisible(),"native auto Escape retained QML owner, enabled flag or committed draft");
    g_session.m_onAutoMSG=true;editor=static_cast<UIAutoMessageWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_AUTOMESSAGEWND));
    pointer(WM_LBUTTONDOWN,editor->m_x+2,editor->m_y+2);g_windowMgr.DeleteWindow(editor);
    Require(g_session.m_onAutoMSG && !GetCapture() && !g_windowMgr.m_captureWindow && !g_windowMgr.m_editWindow,"captured auto generic delete reset flag or retained pointers");
    QFile result(output+"/auto-message-results.json");Require(result.open(QIODevice::WriteOnly),"auto results open failed");
    result.write(QJsonDocument(QJsonObject{{"native_qml_frames",8},{"selected_edit",true},{"source_button_assets",true},{"native_capture_cancel_release",true},
        {"scale125",true},{"draw_focus_and_modal_priority",true},{"known_empty_prompt",true},{"submit_escape_delete_distinct",true},{"outcome","PASS"}}).toJson());
}
