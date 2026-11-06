#include "ui/UIChatRoomWnd.h"

void CheckChatRoomUi(QGuiApplication& app,QQuickView& view,QtUiState* state,
    CGameMode& mode,QtUiStateAdapter& adapter,const QString& output)
{
    struct Restore {
        SubChatWindowInfo geometry=g_windowMgr.m_chatRoomWindowInfo;
        std::map<u32,u32> owners;
        CGameMode& mode;
        explicit Restore(CGameMode& value):owners(value.m_chatRoomOwners),mode(value){}
        ~Restore(){if(g_windowMgr.m_chatRoomWnd)g_windowMgr.DeleteWindow(g_windowMgr.m_chatRoomWnd);
            g_windowMgr.m_chatRoomWindowInfo=geometry;mode.m_chatRoomOwners=owners;}
    } restore(mode);
    if(g_windowMgr.m_chatRoomWnd)g_windowMgr.DeleteWindow(g_windowMgr.m_chatRoomWnd);
    g_windowMgr.m_chatRoomWindowInfo={40,300,280,120};
    const auto put16=[](std::vector<u8>& p,int at,u16 n){p[at]=u8(n);p[at+1]=u8(n>>8);};
    const auto put32=[](std::vector<u8>& p,int at,u32 n){for(int i=0;i<4;++i)p[at+i]=u8(n>>(8*i));};
    const auto listing=[&](u32 id,const std::string& title){
        std::vector<u8> bytes(17+title.size());put16(bytes,0,0xdf);put16(bytes,2,u16(bytes.size()));
        put32(bytes,4,0x12345678);put32(bytes,8,id);put16(bytes,12,8);put16(bytes,14,2);bytes[16]=1;
        std::copy(title.begin(),title.end(),bytes.begin()+17);Dispatch(mode,bytes);
    };
    listing(0xf0123456,"Room <literal>");auto* room=g_windowMgr.m_chatRoomWnd;
    Require(room && room->m_x==40 && room->m_y==300 && room->m_w==280 && room->m_h==120,
        "actual DF receipt did not create source room geometry");
    std::vector<u8> member(28);put16(member,0,0xdc);put16(member,2,3);
    const std::string name="Guest";std::copy(name.begin(),name.end(),member.begin()+4);Dispatch(mode,member);
    Require(room->GetSelectedMember()==0,"fresh source member constructor lost selected0 before any clear");
    const auto mainCount=g_windowMgr.m_chatWnd?g_windowMgr.m_chatWnd->GetSourceHistory(false).size():0;
    const auto events=g_windowMgr.GetChatEvents().size();
    const auto speech=[&](u16 opcode,int header,const std::string& words){
        std::vector<u8> bytes(header+words.size()+1);put16(bytes,0,opcode);put16(bytes,2,u16(bytes.size()));
        if(header==8)put32(bytes,4,0x76543210);std::copy(words.begin(),words.end(),bytes.begin()+header);Dispatch(mode,bytes);
    };
    speech(0x8d,8,"Guest : room receipt");speech(0x8e,4,"Self : room reply");
    Require(room->GetRows().size()>=3 && room->GetRows()[room->GetRows().size()-2].color==0x222222
        && room->GetRows().back().color==0x007800 && g_windowMgr.GetChatEvents().size()==events
        && (!g_windowMgr.m_chatWnd || g_windowMgr.m_chatWnd->GetSourceHistory(false).size()==mainCount),
        "room speech leaked into source main or lost original colors");
    const auto capture=[&](const QString& stem){
        adapter.syncChat();Frames(app,view);
        auto* panel=Find(view.rootObject(),"sourceChatRoomWindow");auto* history=Find(view.rootObject(),"sourceChatRoomHistory");
        auto* members=Find(view.rootObject(),"sourceChatRoomMembers");auto* edit=Find(view.rootObject(),"sourceChatRoomEdit");
        Require(panel && history && members && edit && panel->isVisible(),"actual room QML regions absent");
        const auto position=panel->mapToScene(QPointF(0,0));
        Require(qRound(position.x())==room->m_x && qRound(position.y())==room->m_y
            && qRound(panel->width())==room->m_w && qRound(panel->height())==room->m_h,
            "actual room QML geometry differs from native owner");
        const int memberWidth=56*room->m_w/280;
        Require(qRound(history->x())==1 && qRound(history->y())==17 && qRound(history->width())==room->m_w-memberWidth-3
            && qRound(history->height())==room->m_h-38 && qRound(members->x())==room->m_w-memberWidth-1
            && qRound(members->width())==memberWidth && qRound(edit->x())==3 && qRound(edit->y())==room->m_h-18
            && qRound(edit->width())==room->m_w-30 && qRound(edit->height())==16,
            "source room child rectangles changed");
        const auto data=state->chatWindowUi().value("room").toMap();
        Require(data.value("id").toULongLong()==room->GetInfo().roomId && data.value("rows").toList().size()==room->GetVisibleRows().size(),
            "room projection changed unsigned room identity or visible rows");
        const auto qt=view.grabWindow().convertToFormat(QImage::Format_RGB32);
        ArgbDibSurface dib;Require(dib.EnsureSize(view.width(),view.height()),"room native DIB allocation failed");
        std::fill_n(dib.GetPixels(),view.width()*view.height(),0xff181b20u);room->DrawToHdc(dib.GetDC());
        const auto native=QImage(reinterpret_cast<uchar*>(dib.GetPixels()),view.width(),view.height(),view.width()*4,QImage::Format_RGB32).copy();
        Require(qt.pixelColor(room->m_x+2,room->m_y+18)==QColor("white")
            && native.pixelColor(room->m_x+2,room->m_y+18)==QColor("white"),"room native/Qt history body differs");
        if(room->GetSelectedMember()==0 && !room->GetMemberRows().empty()){
            const int x=room->m_x+room->GetLayout().members.left+1,y=room->m_y+18;
            Require(qt.pixelColor(x,y)==QColor("#32c8c8") && native.pixelColor(x,y)==QColor("#32c8c8"),
                "source member selected-row offset/color differs in native/Qt");
        }
        Require(qt.save(output+"/"+stem+".qml.png") && native.save(output+"/"+stem+".native.png"),"room capture failed");
    };
    capture("chat-room-open");
    const auto pointer=[&](int x,int y){
        WindowProc(g_hMainWnd,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(x,y));
        WindowProc(g_hMainWnd,WM_LBUTTONUP,0,MAKELPARAM(x,y));
    };
    auto layout=room->GetLayout();
    WindowProc(g_hMainWnd,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(room->m_x+277,room->m_y+117));
    WindowProc(g_hMainWnd,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(room->m_x+367,room->m_y+157));
    WindowProc(g_hMainWnd,WM_LBUTTONUP,0,MAKELPARAM(room->m_x+367,room->m_y+157));
    Require(room->m_w==344 && room->m_h==152 && g_windowMgr.m_chatRoomWindowInfo.width==344
        && g_windowMgr.m_chatRoomWindowInfo.height==152 && !g_windowMgr.m_captureWindow && !GetCapture(),
        "actual source room resizer quantization/store/release failed");
    WindowProc(g_hMainWnd,WM_CHAR,'h',0);WindowProc(g_hMainWnd,WM_CHAR,'i',0);
    Require(room->GetInputText()=="hi","native focused room input was stolen by main chat");
    capture("chat-room-resized");
    const auto close=room->GetLayout().close;
    pointer(room->m_x+close.left+5,room->m_y+close.top+5);
    Require(g_windowMgr.m_chatRoomWnd==room,"close135 optimistically destroyed room before server exit");
    WindowProc(g_hMainWnd,WM_KEYDOWN,VK_ESCAPE,0);
    Require(g_windowMgr.m_chatRoomWnd==room,"source inert cancel117 unexpectedly closed room");
    g_windowMgr.DeleteWindow(g_windowMgr.m_chatWnd);capture("chat-room-without-main");
    std::vector<u8> destroy(6);put16(destroy,0,0xd8);put32(destroy,2,0xf0123456);Dispatch(mode,destroy);
    Require(!g_windowMgr.m_chatRoomWnd && !g_windowMgr.m_editWindow,"D8 retained deleted room input owner");
    g_windowMgr.m_chatRoomWindowInfo={40,300,280,120};listing(0x81234567,"Scroll room");room=g_windowMgr.m_chatRoomWnd;
    for(int i=0;i<240;++i){const auto row="room row "+std::to_string(i);g_windowMgr.SendMsg(5,reinterpret_cast<msgparam_t>(row.c_str()),0);}
    Require(room->GetRows().size()==240 && room->GetRows().back().color==0,"source room manager5 changed black to white");
    room->ScrollHistory(-135);Require(room->GetFirst()==100,"room history last-first/page count differs");
    layout=room->GetLayout();pointer(room->m_x+layout.scrollUp.left+6,room->m_y+layout.scrollUp.top+5);
    Require(room->GetFirst()==99,"actual room arrow-up region did not move one row");
    pointer(room->m_x+layout.scrollDown.left+6,room->m_y+layout.scrollDown.top+5);
    Require(room->GetFirst()==100,"actual room arrow-down region did not move one row");
    layout=room->GetLayout();const int thumbX=room->m_x+layout.scrollThumb.left+6,thumbY=room->m_y+layout.scrollThumb.top+3;
    WindowProc(g_hMainWnd,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(thumbX,thumbY));
    WindowProc(g_hMainWnd,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(thumbX,thumbY+1));
    Require(room->GetFirst()==104,"actual room relative thumb arithmetic changed");
    WindowProc(g_hMainWnd,WM_LBUTTONUP,0,MAKELPARAM(-20,-20));
    Require(!GetCapture() && !g_windowMgr.m_captureWindow,"room outside release retained capture");
    capture("chat-room-scroll");
    layout=room->GetLayout();WindowProc(g_hMainWnd,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(room->m_x+layout.scrollThumb.left+6,room->m_y+layout.scrollThumb.top+2));
    put32(destroy,2,0x81234567);Dispatch(mode,destroy);
    Require(!g_windowMgr.m_chatRoomWnd && !g_windowMgr.m_captureWindow && !g_windowMgr.m_editWindow && !GetCapture(),
        "room destruction during thumb capture retained deleted owner/OS capture");
    adapter.syncChat();Frames(app,view);
    Require(!Find(view.rootObject(),"sourceChatRoomWindow")->isVisible(),"QML retained a destroyed room");
    QFile result(output+"/chat-room-results.json");Require(result.open(QIODevice::WriteOnly),"room results open failed");
    result.write(QJsonDocument(QJsonObject{{"source_df_dc_speech",true},{"room_id",QString("0xF0123456")},
        {"native_qt_rectangles",true},{"actual_resize",true},{"native_input",true},{"close_waits_server",true},
        {"absent_main",true},{"black_color",true},{"relative_thumb",true},{"destroy_during_capture",true},{"outcome","PASS"}}).toJson());
}
