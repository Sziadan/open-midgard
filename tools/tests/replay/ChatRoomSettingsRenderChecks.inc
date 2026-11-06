#include "ui/UIChatRoomSettingsWnd.h"
#include "core/MannerFilter.h"

void CheckChatRoomSettingsUi(QGuiApplication& app,QQuickView& view,QtUiState* state,
    CGameMode& mode,QtUiStateAdapter& adapter,const QString& output)
{
    g_windowMgr.RemoveAllWindows();
    struct Restore {
        int scale=GetConfiguredUiScalePercent(),folder=g_readFolderFirst;
        std::filesystem::path cwd=std::filesystem::current_path();
        ro::MannerFilter filter=ro::GetMannerFilter();ro::commerce::State commerce=ro::commerce::state;
        ~Restore(){g_windowMgr.RemoveAllWindows();SetRuntimeUiScalePercent(scale);g_readFolderFirst=folder;
            std::filesystem::current_path(cwd);ro::GetMannerFilter()=std::move(filter);ro::commerce::state=std::move(commerce);}
    } restore;
    const auto assets=std::filesystem::path(output.toStdString())/"entry-assets";
    std::filesystem::current_path(assets);g_readFolderFirst=1;SetRuntimeUiScalePercent(100);ro::commerce::state={};
    ro::GetMannerFilter().Clear();ro::GetMannerFilter().AppendEncodedFile("BAD\r\n",65001);
    {std::ofstream f("msgstringtable.txt",std::ios::binary);for(int i=0;i<=661;++i)f<<(i==14?"#\r\n":"Settings label#\r\n");}
    const auto pointer=[&](UINT message,int x,int y){WindowProc(g_hMainWnd,message,message==WM_LBUTTONUP?0:MK_LBUTTON,
        MAKELPARAM(UiScaleLogicalToRawCoordinate(x),UiScaleLogicalToRawCoordinate(y)));};
    const auto tap=[&](int x,int y){pointer(WM_LBUTTONDOWN,x,y);pointer(WM_LBUTTONUP,x,y);};
    const auto capture=[&](const QString& name){
        state->setUiScale(GetConfiguredUiScaleFactor());adapter.syncChat();Frames(app,view);
        auto* form=g_windowMgr.m_chatRoomMakeWnd?g_windowMgr.m_chatRoomMakeWnd:g_windowMgr.m_chatRoomChangeWnd;
        Require(form,"settings render lost managed owner");
        auto* item=Find(view.rootObject(),form->IsChange()?"sourceRoomChange":"sourceRoomMake");
        Require(item && item->isVisible(),"actual QML settings owner missing");
        const auto pos=item->mapToScene(QPointF(0,0));
        Require(qRound(pos.x())==UiScaleLogicalToRawCoordinate(form->m_x) && qRound(pos.y())==UiScaleLogicalToRawCoordinate(form->m_y),"settings QML source placement/scale diverged");
        const auto settings=state->chatWindowUi().value("roomSettings").toList();
        Require(settings.size()==1 && !settings[0].toMap().contains("password")
            && settings[0].toMap().value("masked").toString()==QString(static_cast<int>(form->GetPasswordText().size()),QChar('*')),"settings projection exposed password or lost mask");
        ArgbDibSurface dib;Require(dib.EnsureSize(view.width(),view.height()),"settings native DIB failed");
        std::fill_n(dib.GetPixels(),view.width()*view.height(),0xff181b20u);form->DrawToHdc(dib.GetDC());
        if(g_windowMgr.m_roomMessageBox)g_windowMgr.m_roomMessageBox->DrawToHdc(dib.GetDC());
        auto native=QImage(reinterpret_cast<uchar*>(dib.GetPixels()),view.width(),view.height(),view.width()*4,QImage::Format_RGB32).copy();
        if(GetConfiguredUiScalePercent()!=100)native=native.scaled(qRound(native.width()*GetConfiguredUiScaleFactor()),qRound(native.height()*GetConfiguredUiScaleFactor()),Qt::IgnoreAspectRatio,Qt::FastTransformation).copy(0,0,view.width(),view.height());
        const auto qt=view.grabWindow().convertToFormat(QImage::Format_RGB32);
        if(!g_windowMgr.m_roomMessageBox){const int x=UiScaleLogicalToRawCoordinate(form->m_x+194),y=UiScaleLogicalToRawCoordinate(form->m_y+101);
            const QColor expected=form->GetPressedButton()==118?QColor("#806040"):QColor("#c0a080");
            Require(native.pixelColor(x,y)==expected && qt.pixelColor(x,y)==expected,"settings actual native/QML OK artwork differs");}
        if(auto* message=g_windowMgr.m_roomMessageBox){auto* box=Find(view.rootObject(),"sourceRoomValidation");
            Require(box && box->isVisible() && box->z()>3000,"validation did not project above commerce dialogs");
            const auto point=box->mapToScene(QPointF(0,0));Require(qRound(point.x())==UiScaleLogicalToRawCoordinate(message->m_x)
                && qRound(point.y())==UiScaleLogicalToRawCoordinate(message->m_y),"modal source placement/scale differs");
            const int x=UiScaleLogicalToRawCoordinate(message->m_x+194),y=UiScaleLogicalToRawCoordinate(message->m_y+101);
            Require(qt.pixelColor(x,y)==QColor("#c0a080") && native.pixelColor(x,y)==QColor("#c0a080"),"validation button pixels missing above settings");}
        Require(native.save(output+"/"+name+".native.png") && qt.save(output+"/"+name+".qml.png"),"save settings frame failed");
    };
    Require(g_windowMgr.OnQtKeyDown('C',true,false,false),"actual Alt+C was not consumed");auto* make=g_windowMgr.m_chatRoomMakeWnd;
    Require(make,"Alt+C did not create settings owner");
    for(char c:std::string("Literal <title>"))WindowProc(g_hMainWnd,WM_CHAR,c,0);
    pointer(WM_LBUTTONDOWN,make->m_x+90,make->m_y+75);
    Require(make->IsPrivate(),"source radio did not dispatch on mouse down");pointer(WM_LBUTTONUP,-20,-20);
    tap(make->m_x+190,make->m_y+75);for(char c:std::string("secret123"))WindowProc(g_hMainWnd,WM_CHAR,c,0);
    Require(make->GetPasswordText()=="secret12","settings max8/native focused input failed");
    tap(make->m_x+122,make->m_y+50);Require(make->GetPopupKind()==0,"people combo did not open on release");
    make->OnWheel(-120);make->OnWheel(-120);make->OnWheel(-120);capture("chat-settings-combo");
    tap(make->m_x+50,make->m_y+100);Require(make->GetPeople()==20 && make->GetPopupKind()<0,"people popup final choice failed");
    pointer(WM_LBUTTONDOWN,make->m_x+194,make->m_y+101);Require(GetCapture()==g_hMainWnd,"settings button lacks actual capture");capture("chat-settings-pressed");
    pointer(WM_LBUTTONUP,-20,-20);Require(g_windowMgr.m_chatRoomMakeWnd==make && !GetCapture(),"outside release submitted settings or retained capture");
    const int x=make->m_x,y=make->m_y;pointer(WM_LBUTTONDOWN,x+3,y+115);pointer(WM_MOUSEMOVE,x+23,y+125);pointer(WM_LBUTTONUP,-20,-20);
    Require(make->m_x==x+20 && make->m_y==y+10 && !GetCapture(),"settings frame drag/outside release failed");
    pointer(WM_LBUTTONDOWN,make->m_x+3,make->m_y+115);WindowProc(g_hMainWnd,WM_CANCELMODE,0,0);
    Require(!g_windowMgr.m_captureWindow && !GetCapture(),"settings cancellation retained capture");
    make->SetTitleText("BAD");WindowProc(g_hMainWnd,WM_KEYDOWN,VK_RETURN,0);
    Require(g_windowMgr.m_roomMessageBox && g_windowMgr.m_roomMessageBox->GetText().empty(),"known-empty modal label missing");capture("chat-settings-validation-empty");
    WindowProc(g_hMainWnd,WM_CHAR,'z',0);Require(make->GetTitleText()=="BAD","modal leaked text into settings");
    WindowProc(g_hMainWnd,WM_KEYDOWN,VK_ESCAPE,0);Require(g_windowMgr.m_editWindow==make,"modal did not restore settings focus");
    g_windowMgr.DeleteWindow(make);g_windowMgr.MakeWindow(UIWindowMgr::WID_CHATROOMWND);
    mode.m_lastChatroomInfo={"Prior","abcd",0,3,2,0x11223344};g_windowMgr.m_chatRoomWnd->SendMsg(nullptr,48,2,0,0);
    auto* change=g_windowMgr.m_chatRoomChangeWnd;Require(change && change->GetPeople()==2 && change->GetPeopleLabel().find("20")==0,"actual settings prefill quirks changed");
    WindowProc(g_hMainWnd,WM_CHAR,'N',0);Require(change->GetTitleText()=="N","native selected title was not replaced");
    SetRuntimeUiScalePercent(125);capture("chat-settings-change-125");
    change->SetTitleText("");WindowProc(g_hMainWnd,WM_KEYDOWN,VK_RETURN,0);capture("chat-settings-validation-125");
    g_windowMgr.DeleteWindow(change);WindowProc(g_hMainWnd,WM_KEYDOWN,VK_RETURN,0);
    Require(!g_windowMgr.m_editWindow && !g_windowMgr.m_modalWindow,"modal restored destroyed settings editor");
    g_windowMgr.RemoveAllWindows();adapter.syncChat();Frames(app,view);
    Require(state->chatWindowUi().value("roomSettings").toList().empty() && !Find(view.rootObject(),"sourceRoomValidation")->isVisible(),"settings QML teardown retained owners");
    QFile result(output+"/chat-settings-results.json");Require(result.open(QIODevice::WriteOnly),"settings result open failed");
    result.write(QJsonDocument(QJsonObject{{"native_qml_frames",10},{"radio_down",true},{"combo",true},{"masked_native_input",true},
        {"capture_cancel_release",true},{"known_empty_modal",true},{"prefill",true},{"scale_125",true},{"deleted_editor",true},{"outcome","PASS"}}).toJson());
}
