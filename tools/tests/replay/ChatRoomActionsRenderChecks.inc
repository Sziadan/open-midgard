#include "core/Globals.h"

void CheckChatRoomActionsUi(QGuiApplication& app,QQuickView& view,QtUiState* state,
    CGameMode& mode,QtUiStateAdapter& adapter,const QString& output)
{
    g_windowMgr.RemoveAllWindows();
    struct Restore {
        int scale=GetConfiguredUiScalePercent();SubChatWindowInfo geometry=g_windowMgr.m_chatRoomWindowInfo;
        ServerType server=g_serverType;std::list<FRIEND_INFO> friends=g_session.m_friendList;
        ~Restore(){g_windowMgr.RemoveAllWindows();SetRuntimeUiScalePercent(scale);g_windowMgr.m_chatRoomWindowInfo=geometry;g_serverType=server;g_session.m_friendList=std::move(friends);}
    } restore;
    SetRuntimeUiScalePercent(100);g_serverType=ServerNormal;g_session.m_friendList.clear();
    auto* room=static_cast<UIChatRoomWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_CHATROOMWND));room->Move(300,80);
    room->SetInfo({"Owner actions","",1,2,20,77});room->AddMember(g_session.GetPlayerName(),0);room->AddMember("Target",1);
    const auto pointer=[&](UINT message,int x,int y){WindowProc(g_hMainWnd,message,message==WM_LBUTTONUP || message==WM_RBUTTONUP?0:message==WM_RBUTTONDOWN?MK_RBUTTON:MK_LBUTTON,
        MAKELPARAM(UiScaleLogicalToRawCoordinate(x),UiScaleLogicalToRawCoordinate(y)));};
    const auto member=room->GetLayout().members;const int selectX=room->m_x+member.left+2,selectY=room->m_y+member.top+16;
    pointer(WM_LBUTTONDOWN,selectX,selectY);pointer(WM_LBUTTONUP,selectX,selectY);
    const auto open=[&](){pointer(WM_RBUTTONDOWN,selectX,room->m_y+member.top+2);pointer(WM_RBUTTONUP,selectX,room->m_y+member.top+2);
        Require(g_windowMgr.m_chatRoomMenuWnd && room->GetSelectedMember()==1,"native right release changed selection or failed to open menu");};
    const auto capture=[&](const QString& name){
        state->setUiScale(GetConfiguredUiScaleFactor());adapter.syncChat();Frames(app,view);auto* popup=g_windowMgr.m_chatRoomMenuWnd;
        auto* item=Find(view.rootObject(),"sourceRoomMenu");Require(popup && item && item->isVisible(),"actual room popup QML missing");
        const auto bounds=item->mapRectToScene(item->boundingRect());const double scale=GetConfiguredUiScaleFactor();
        Require(std::abs(bounds.left()-popup->m_x*scale)<0.001 && std::abs(bounds.top()-popup->m_y*scale)<0.001
            && std::abs(bounds.width()-popup->m_w*scale)<0.001 && std::abs(bounds.height()-popup->m_h*scale)<0.001,"room menu native/QML scaled bounds differ");
        const auto rows=state->chatWindowUi().value("roomMenu").toMap().value("rows").toList();
        Require(rows.size()==5 && rows[0].toMap().value("action")==0 && rows[4].toMap().value("action")==2,"actual QML lost source menu order");
        ArgbDibSurface dib;Require(dib.EnsureSize(view.width(),view.height()),"menu DIB failed");std::fill_n(dib.GetPixels(),view.width()*view.height(),0xff181b20u);
        room->DrawToHdc(dib.GetDC());popup->DrawToHdc(dib.GetDC());
        auto native=QImage(reinterpret_cast<uchar*>(dib.GetPixels()),view.width(),view.height(),view.width()*4,QImage::Format_RGB32).copy();
        if(GetConfiguredUiScalePercent()!=100)native=native.scaled(qRound(native.width()*GetConfiguredUiScaleFactor()),qRound(native.height()*GetConfiguredUiScaleFactor()),Qt::IgnoreAspectRatio,Qt::FastTransformation).copy(0,0,view.width(),view.height());
        const auto qt=view.grabWindow().convertToFormat(QImage::Format_RGB32);const int x=UiScaleLogicalToRawCoordinate(popup->m_x+popup->m_w-5),y=UiScaleLogicalToRawCoordinate(popup->m_y+18);
        Require(qt.pixelColor(x,y)==QColor("#32c8c8") && native.pixelColor(x,y)==QColor("#32c8c8"),"actual native/QML menu hover pixels missing");
        Require(qt.save(output+"/"+name+".qml.png") && native.save(output+"/"+name+".native.png"),"save menu frames failed");
    };
    open();auto* popup=g_windowMgr.m_chatRoomMenuWnd;pointer(WM_MOUSEMOVE,popup->m_x+4,popup->m_y+18);capture("chat-actions-owner-menu");
    pointer(WM_LBUTTONDOWN,popup->m_x+4,popup->m_y+18);pointer(WM_LBUTTONUP,-20,-20);
    Require(g_windowMgr.m_chatRoomMenuWnd==popup && !GetCapture(),"outside release changed source menu lifetime/capture");
    // Actual source menu index4 opens settings; old action-ID2 shortcut would choose info.
    mode.m_lastChatroomInfo={"Pending title","abcd",0,2,3,77};
    pointer(WM_LBUTTONDOWN,popup->m_x+4,popup->m_y+58);pointer(WM_LBUTTONUP,popup->m_x+4,popup->m_y+58);
    Require(!g_windowMgr.m_chatRoomMenuWnd && g_windowMgr.m_chatRoomChangeWnd && g_windowMgr.m_chatRoomChangeWnd->GetTitleText()=="Pending title","native selected-member settings menu index did not route to pending prefill");
    g_windowMgr.DeleteWindow(g_windowMgr.m_chatRoomChangeWnd);
    SetRuntimeUiScalePercent(125);open();popup=g_windowMgr.m_chatRoomMenuWnd;
    pointer(WM_MOUSEMOVE,popup->m_x+4,popup->m_y+18);capture("chat-actions-owner-menu-125");
    pointer(WM_LBUTTONDOWN,popup->m_x+4,popup->m_y+18);Require(GetCapture()==g_hMainWnd,"menu did not acquire OS capture");
    g_windowMgr.DeleteWindow(room);Require(g_windowMgr.m_chatRoomMenuWnd==popup && GetCapture()==g_hMainWnd && g_windowMgr.m_captureWindow==popup,"source room deletion incorrectly destroyed independent captured menu");
    pointer(WM_LBUTTONUP,popup->m_x+4,popup->m_y+18);
    Require(!g_windowMgr.m_chatRoomMenuWnd && !GetCapture() && !g_windowMgr.m_captureWindow,"orphaned source menu failed to close safely on valid release");
    adapter.syncChat();Frames(app,view);Require(!Find(view.rootObject(),"sourceRoomMenu")->isVisible() && state->chatWindowUi().value("roomMenu").toMap().empty(),"actual QML retained deleted room menu");
    QFile result(output+"/chat-actions-results.json");Require(result.open(QIODevice::WriteOnly),"actions result open failed");
    result.write(QJsonDocument(QJsonObject{{"native_qml_frames",4},{"right_release_keeps_selection",true},{"source_index4_settings",true},
        {"outside_release_open",true},{"scale_125",true},{"captured_room_teardown",true},{"outcome","PASS"}}).toJson());
}
