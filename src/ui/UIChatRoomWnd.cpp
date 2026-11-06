#include "UIChatRoomWnd.h"
#include "UIChatRoomSettingsWnd.h"
#include "UIWindowMgr.h"
#include "UINewChatWnd.h"
#include "ChatScrollBarUi.h"
#include "ChatText.h"
#include "core/ChatNotice.h"
#include "core/Globals.h"
#include "core/ClientInfoLocale.h"
#include "core/UiMessageStrings.h"
#include "gamemode/GameMode.h"
#include "gamemode/Mode.h"
#include "session/Session.h"
#include <algorithm>
#if RO_ENABLE_QT6_UI
#include <QFont>
#include <QImage>
#include <QPainter>
#endif

namespace {
bool Contains(RECT r,int x,int y){return x>=r.left && y>=r.top && x<r.right && y<r.bottom;}
void Box(HDC dc,RECT r,COLORREF color){auto brush=CreateSolidBrush(color);FillRect(dc,&r,brush);DeleteObject(brush);}
void Text(HDC dc,RECT r,const std::string& text,u32 rgb){
    if(r.right<=r.left || r.bottom<=r.top)return;
#if RO_ENABLE_QT6_UI
    QImage image(r.right-r.left,r.bottom-r.top,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);
    QPainter paint(&image);QFont font("Segoe UI");font.setPixelSize(12);paint.setFont(font);
    paint.setPen(QColor::fromRgb(rgb));paint.drawText(image.rect(),Qt::AlignLeft|Qt::AlignTop|Qt::TextSingleLine,QString::fromUtf8(text.c_str()));paint.end();
    AlphaBlendArgbToHdc(dc,r.left,r.top,image.width(),image.height(),reinterpret_cast<const unsigned int*>(image.constBits()),image.width(),image.height());
#else
    shopui::DrawWindowTextRect(dc,r,text,RGB((rgb>>16)&255,(rgb>>8)&255,rgb&255),DT_LEFT|DT_TOP|DT_SINGLELINE|DT_NOPREFIX);
#endif
}
}

UIChatRoomWnd::UIChatRoomWnd()
{
    m_id=28;Create(280,120);
    ro::UiMessageStrings table;table.Load();
    m_publicLabel=table.Get(90,"[Public] ");m_privateLabel=table.Get(89,"[Private] ");
    m_view.selected=m_memberView.selected=0;
    m_view.Resize(82/14,0);m_memberView.Resize(82/14,0);
    RestoreInfo();
}
void UIChatRoomWnd::SetInfo(const ro::chat::RoomInfo& info){m_info=info;Invalidate();}
std::string UIChatRoomWnd::GetTitle() const {
    return (m_info.roomType?m_publicLabel:m_privateLabel)+m_info.title+" ("+std::to_string(m_info.numPeople)+"/"+std::to_string(m_info.maxNumPeople)+")";
}
void UIChatRoomWnd::ClearMembers(){m_members.clear();m_memberRows.clear();m_memberView={};m_memberView.Resize((m_childHeight-38)/14,0);Invalidate();}
void UIChatRoomWnd::AddMember(const std::string& name,u32 role){
    m_members[name]=role;const int before=static_cast<int>(m_memberRows.size());
    if(before>=240)m_memberRows.erase(m_memberRows.begin(),m_memberRows.begin()+before/2);
    m_memberRows.push_back({role,name});m_memberView.AfterAppend(before,static_cast<int>(m_memberRows.size()));Invalidate();
}
void UIChatRoomWnd::RebuildMembers(){
    m_memberRows.clear();m_memberView.first=m_memberView.scrollPosition=0;m_memberView.selected=-1;
    for(const auto& member:m_members){
        const int before=static_cast<int>(m_memberRows.size());
        if(before>=240)m_memberRows.erase(m_memberRows.begin(),m_memberRows.begin()+before/2);
        m_memberRows.push_back({member.second,member.first});m_memberView.AfterAppend(before,static_cast<int>(m_memberRows.size()));
    }
    Invalidate();
}
void UIChatRoomWnd::RemoveMember(const std::string& name){if(m_members.erase(name))RebuildMembers();}
void UIChatRoomWnd::ChangeRole(const std::string& name,u32 role){
    const auto found=m_members.find(name);if(found!=m_members.end())found->second=role;
    RebuildMembers();
}
void UIChatRoomWnd::Append(const char* text,u32 rgb){
    if(!text)return;const std::string input(text);std::string talker;
    // Source also recognizes the Korean join/leave delimiters for '(name)'.
    const auto sourceJoin=input.find("\x29\xb4\xd4\xc0\xcc\x20\xc0\xd4\xc0\xe5\x20\xc7\xcf\xbf\xb4\xbd\xc0\xb4\xcf\xb4\xd9\x2e");
    const auto sourceLeave=input.find("\x29\xb4\xd4\xc0\xcc\x20\xc5\xf0\xc0\xe5\x20\xc7\xcf\xbf\xb4\xbd\xc0\xb4\xcf\xb4\xd9\x2e");
    const auto join=sourceJoin!=std::string::npos?sourceJoin:input.find(u8")님이 입장 하였습니다.");
    const auto leave=sourceLeave!=std::string::npos?sourceLeave:input.find(u8")님이 퇴장 하였습니다.");
    const auto ordinary=input.find(" :");
    if(join>0 && join<input.size())talker=input.substr(1,join-1);
    else if(leave>0 && leave<input.size())talker=input.substr(1,leave-1);
    else if(ordinary>0 && ordinary<input.size())talker=input.substr(0,ordinary);
    if(talker==g_session.GetPlayerName())talker.clear();
    if(talker.size()>24)talker.resize(24); // Source fixed buffer, bounded safely.
    for(const auto& part:ro::chat::SplitNotice(input,(std::max)(1,80*m_w/640))){
        const int before=static_cast<int>(m_rows.size());
        if(before>=240)m_rows.erase(m_rows.begin(),m_rows.begin()+before/2);
        m_rows.push_back({part,rgb,talker});m_view.AfterAppend(before,static_cast<int>(m_rows.size()));
    }
    Invalidate();
}
std::vector<SubChatRow> UIChatRoomWnd::GetVisibleRows() const {
    if(m_view.first<0 || m_view.first>=static_cast<int>(m_rows.size()))return {};
    const int end=(std::min)(static_cast<int>(m_rows.size()),m_view.first+m_view.rows);
    return {m_rows.begin()+m_view.first,m_rows.begin()+end};
}
void UIChatRoomWnd::ScrollHistory(int delta){m_view.Scroll(delta,static_cast<int>(m_rows.size()));Invalidate();}
void UIChatRoomWnd::ResizeSource(int width,int height){
    width=std::clamp(width,280,512);height=std::clamp(height,120,384);
    width=280+32*((width-280)/32);height=120+32*((height-120)/32);
    if(width!=m_w || height!=m_h){
        UIWindow::Resize(width,height);m_childWidth=width;m_childHeight=height;
        m_view.Resize((height-38)/14,static_cast<int>(m_rows.size()));
        m_memberView.Resize((height-38)/14,static_cast<int>(m_memberRows.size()));
    }
}
void UIChatRoomWnd::RestoreInfo(){
    auto& info=g_windowMgr.m_chatRoomWindowInfo;const auto client=g_windowMgr.GetChatClientRect();
    if(info.width<280 || info.height<120 || info.x<0 || info.y<0
        || static_cast<long long>(info.x)+info.width>client.right || static_cast<long long>(info.y)+info.height>client.bottom)
        info={500,180,280,120};
    ResizeSource(info.width,info.height);Move(info.x,info.y);
}
void UIChatRoomWnd::StoreInfo(){g_windowMgr.m_chatRoomWindowInfo={m_x,m_y,m_w,m_h};}
ro::chat::ScrollBar UIChatRoomWnd::GetScrollModel(bool members) const {
    const auto& view=members?m_memberView:m_view;
    const int count=static_cast<int>(members?m_memberRows.size():m_rows.size());
    return {m_childHeight-38,10,view.LastFirst(count),view.scrollPosition,view.rows,count};
}
ChatRoomLayout UIChatRoomWnd::GetLayout() const {
    ChatRoomLayout out;const int memberWidth=56*m_childWidth/280;
    out.history={1,17,m_childWidth-memberWidth-2,m_childHeight-21};out.members={m_childWidth-memberWidth-1,17,m_childWidth-1,m_childHeight-21};
    out.edit={3,m_childHeight-18,m_childWidth-27,m_childHeight-2};out.close={m_childWidth-14,3,m_childWidth-2,15};out.resizer={m_childWidth-15,m_childHeight-15,m_childWidth+1,m_childHeight+1};
    const auto bars=[&](bool member,RECT bounds,RECT& track,RECT& thumb,RECT& up,RECT& down){
        const auto model=GetScrollModel(member);if(model.maximum<=0)return;
        track={bounds.right-13,bounds.top,bounds.right,bounds.bottom};const auto shape=ro::chat::ScrollLayout(track,model);
        thumb=shape.thumb;up=shape.up;down=shape.down;
    };
    bars(false,out.history,out.scrollTrack,out.scrollThumb,out.scrollUp,out.scrollDown);
    bars(true,out.members,out.memberTrack,out.memberThumb,out.memberUp,out.memberDown);return out;
}
void UIChatRoomWnd::OnDraw(){
    auto dc=AcquireDrawTarget();if(!dc)return;const int saved=SaveDC(dc);
    IntersectClipRect(dc,m_x,m_y,m_x+m_w,m_y+m_h);
    const auto global=[&](RECT r){r.left+=m_x;r.right+=m_x;r.top+=m_y;r.bottom+=m_y;return r;};
    Box(dc,{m_x,m_y,m_x+m_w,m_y+m_h},RGB(230,230,230));
    Box(dc,{m_x,m_y,m_x+m_w,m_y+17},RGB(90,100,110));
    Text(dc,{m_x+9,m_y+2,m_x+m_childWidth-21,m_y+17},GetTitle(),0xffffff);
    const auto layout=GetLayout();Box(dc,global(layout.history),RGB(255,255,255));Box(dc,global(layout.members),RGB(255,255,255));
    int y=m_y+20;
    for(const auto& row:GetVisibleRows()){Text(dc,{m_x+3,y,m_x+layout.history.right-(GetScrollModel().maximum>0?13:0),y+14},row.text,row.color);y+=14;}
    const int count=static_cast<int>(m_memberRows.size());
    for(int i=m_memberView.first;i<count && i<m_memberView.first+m_memberView.rows;++i){
        if(i<0)continue;const int yy=m_y+20+(i-m_memberView.first)*14;
        RECT row{m_x+layout.members.left,yy-2,m_x+layout.members.right,yy+12};
        if(i==m_memberView.selected)Box(dc,row,RGB(50,200,200));
        Text(dc,{row.left+2,yy,row.right,yy+14},m_memberRows[i].name,m_memberRows[i].role?0:0x008080);
    }
    if(GetScrollModel().maximum>0)ro::chat::DrawScrollBar(dc,global(layout.scrollTrack),global(layout.scrollThumb),10);
    if(GetScrollModel(true).maximum>0)ro::chat::DrawScrollBar(dc,global(layout.memberTrack),global(layout.memberThumb),10);
    Box(dc,global(layout.edit),RGB(250,250,250));Text(dc,global(layout.edit),m_input,0x222222);
    Text(dc,global(layout.close),"x",0xffffff);Text(dc,global(layout.resizer),"/",0x555555);
    RestoreDC(dc,saved);ReleaseDrawTarget(dc);
}
void UIChatRoomWnd::OnLBtnDown(int x,int y){
    x-=m_x;y-=m_y;const auto layout=GetLayout();
    if(g_windowMgr.m_chatWnd)g_windowMgr.m_chatWnd->ClearInputFocus();
    g_windowMgr.m_editWindow=this;
    if(Contains(layout.close,x,y)){g_modeMgr.SendMsg(53,0,0,0);return;}
    for(bool member:{false,true}){
        const auto track=member?layout.memberTrack:layout.scrollTrack;
        if(Contains(track,x,y)){
            const auto model=GetScrollModel(member);const auto hit=model.Hit(x-track.left,y-track.top);
            if(hit==ro::chat::ScrollHit::Thumb){m_scrollDragging=true;m_scrollMembers=member;m_scrollDrag.Begin(y-track.top);}
            else {auto& view=member?m_memberView:m_view;view.Scroll(model.Step(hit),static_cast<int>(member?m_memberRows.size():m_rows.size()));Invalidate();}
            return;
        }
    }
    if(Contains(layout.members,x,y)){
        const int row=m_memberView.first+(y-layout.members.top)/14;
        m_memberView.selected=row>=0 && row<static_cast<int>(m_memberRows.size())?row:-1;Invalidate();return;
    }
    if(Contains(layout.resizer,x,y))m_resizing=true;
    else if(y<17)m_dragging=true;
    m_startX=x+m_x;m_startY=y+m_y;m_oldX=m_x;m_oldY=m_y;m_oldW=m_w;m_oldH=m_h;
}
void UIChatRoomWnd::OnMouseMove(int x,int y){
    if(m_scrollDragging){
        auto& view=m_scrollMembers?m_memberView:m_view;const auto layout=GetLayout();
        const auto track=m_scrollMembers?layout.memberTrack:layout.scrollTrack;const int before=view.first;
        view.Scroll(m_scrollDrag.Request(y-m_y-track.top,GetScrollModel(m_scrollMembers)),static_cast<int>(m_scrollMembers?m_memberRows.size():m_rows.size()));
        m_scrollDrag.Applied(before,view.first);Invalidate();
    }else if(m_resizing)ResizeSource(m_oldW+x-m_startX,m_oldH+y-m_startY);
    else if(m_dragging)Move(m_oldX+x-m_startX,m_oldY+y-m_startY);
}
void UIChatRoomWnd::CancelPointerCapture(){m_dragging=m_resizing=m_scrollDragging=false;if(g_windowMgr.m_captureWindow==this)g_windowMgr.m_captureWindow=nullptr;}
void UIChatRoomWnd::OnLBtnUp(int,int){if(m_dragging || m_resizing)StoreInfo();CancelPointerCapture();}
void UIChatRoomWnd::OnWheel(int delta){ScrollHistory(delta>=0?-3:3);}
void UIChatRoomWnd::SetInputText(const std::string& text){m_pendingUtf8.clear();m_input=ro::chat::CleanInput(text,70,70);Invalidate();}
bool UIChatRoomWnd::SubmitInput(){
    if(m_input.empty())return false;
    if(g_serviceType==ServiceAmerica && std::any_of(m_input.begin(),m_input.end(),[](unsigned char c){return c>=0x80;})){
        ro::UiMessageStrings table;table.Load();const auto notice=table.Get(190,"Only English text is allowed.");
        g_windowMgr.SendMsg(1,reinterpret_cast<msgparam_t>(notice.c_str()),255);return false;
    }
    std::string target;
    const int selected=m_memberView.selected;
    if(m_input[0]!='/' && selected>=0 && selected<static_cast<int>(m_memberRows.size())
        && m_memberRows[selected].name!=g_session.GetPlayerName())target=m_memberRows[selected].name;
    const bool sent=g_modeMgr.SendMsg(CGameMode::GameMsg_SubmitChat,reinterpret_cast<msgparam_t>(m_input.c_str()),reinterpret_cast<msgparam_t>(target.c_str()),0)!=0;
    // Original UI stores history and clears after dispatch, independent of send result.
    m_input.clear();m_pendingUtf8.clear();Invalidate();return sent;
}
void UIChatRoomWnd::OnChar(char value)
{
    const auto byte=static_cast<unsigned char>(value);
    if(byte<0x80){
        m_pendingUtf8.clear();
        if(value=='\r' || value=='\n')return;
        if(value=='\b')ro::chat::EraseLastCharacter(m_input);
        else if(byte>=32)SetInputText(m_input+value);
    }else if(byte>=0xc2 && byte<=0xf4){
        m_pendingUtf8.assign(1,value);
    }else if(byte>=0x80 && byte<=0xbf && !m_pendingUtf8.empty()){
        m_pendingUtf8+=value;
        const auto first=static_cast<unsigned char>(m_pendingUtf8[0]);
        const std::size_t expected=first<0xe0?2:first<0xf0?3:4;
        if(m_pendingUtf8.size()==expected){SetInputText(m_input+m_pendingUtf8);m_pendingUtf8.clear();}
    }else m_pendingUtf8.clear();
    Invalidate();
}

void UIChatRoomWnd::OnKeyDown(int key){if(key==VK_RETURN)SubmitInput();}
msgresult_t UIChatRoomWnd::SendMsg(UIWindow* sender,int msg,msgparam_t value,msgparam_t detail,msgparam_t extra){
    switch(msg){
    case 48:
        if(value==2){auto* settings=static_cast<UIChatRoomSettingsWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_CHATROOMCHANGE));
            if(settings)if(auto* mode=g_modeMgr.GetCurrentGameMode())settings->SetInfo(mode->m_lastChatroomInfo);}
        return 0;
    case 3:ScrollHistory(1-m_view.rows);return 0;
    case 4:ScrollHistory(m_view.rows-1);return 0;
    case 14:if(value==7)ResizeSource(static_cast<int>(detail),static_cast<int>(extra));return 0;
    case 34:if(value)SetInfo(*reinterpret_cast<const ro::chat::RoomInfo*>(value));return 0;
    case 37:Append(reinterpret_cast<const char*>(value),ro::chat::ColorRefToUiRgb(static_cast<u32>(detail)));return 0;
    case 43:ClearMembers();return 0;
    case 44:if(value)AddMember(reinterpret_cast<const char*>(value),static_cast<u32>(detail));return 0;
    case 45:if(value)RemoveMember(reinterpret_cast<const char*>(value));return 0;
    case 46:if(value)ChangeRole(reinterpret_cast<const char*>(value),static_cast<u32>(detail));return 0;
    case 6:if(value==135)g_modeMgr.SendMsg(53,0,0,0);else if(value==118)SubmitInput();return 0;
    default:return UIWindow::SendMsg(sender,msg,value,detail,extra);
    }
}
