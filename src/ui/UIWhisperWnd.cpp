#include "UIWhisperWnd.h"
#include "UIWindowMgr.h"
#include "UINewChatWnd.h"
#include "ChatText.h"
#include "ChatScrollBarUi.h"
#include "core/Globals.h"
#include "core/ChatNotice.h"
#include "core/UiMessageStrings.h"
#include "core/SettingsIni.h"
#include "core/SourceChatCommand.h"
#include "UiScale.h"
#include "qtui/QtUiRuntime.h"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include "gamemode/GameMode.h"
#include "gamemode/Mode.h"
#include "session/Session.h"
#if RO_ENABLE_QT6_UI
#include <QFont>
#include <QImage>
#include <QPainter>
#endif

namespace {
bool In(RECT r,int x,int y){return x>=r.left && x<r.right && y>=r.top && y<r.bottom;}
void Box(HDC dc,RECT r,COLORREF color){auto brush=CreateSolidBrush(color);FillRect(dc,&r,brush);DeleteObject(brush);}
void Text(HDC dc,RECT r,const std::string& text,u32 rgb)
{
    if(r.right<=r.left || r.bottom<=r.top)return;
#if RO_ENABLE_QT6_UI
    QImage image(r.right-r.left,r.bottom-r.top,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);
    QPainter painter(&image);QFont font("Segoe UI");font.setPixelSize(12);painter.setFont(font);painter.setPen(QColor::fromRgb(rgb));
    painter.drawText(image.rect(),Qt::AlignLeft|Qt::AlignTop|Qt::TextSingleLine,QString::fromUtf8(text.c_str()));painter.end();
    AlphaBlendArgbToHdc(dc,r.left,r.top,image.width(),image.height(),reinterpret_cast<const unsigned int*>(image.constBits()),image.width(),image.height());
#else
    shopui::DrawWindowTextRect(dc,r,text,RGB((rgb>>16)&255,(rgb>>8)&255,rgb&255),DT_LEFT|DT_TOP|DT_SINGLELINE|DT_NOPREFIX);
#endif
}
}

UIWhisperWnd::UIWhisperWnd(std::string name,std::string title):m_name(std::move(name)),m_title(std::move(title))
{
    m_id=10051;Create(280,120);m_view.selected=0;m_view.Resize(82/14,0);RestoreInfo();
}
WhisperWindowLayout UIWhisperWnd::GetLayout() const
{
    WhisperWindowLayout out;
    out.history={1,17,m_childWidth-1,m_childHeight-21};
    out.edit={3,m_childHeight-18,m_childWidth-27,m_childHeight-2};
    out.close={m_childWidth-14,3,m_childWidth-2,15};
    out.resizer={m_childWidth-15,m_childHeight-15,m_childWidth+1,m_childHeight+1};
    const auto model=GetScrollModel();
    if(model.maximum>0){out.track={m_childWidth-14,17,m_childWidth-1,m_childHeight-21};
        const auto shape=ro::chat::ScrollLayout(out.track,model);out.thumb=shape.thumb;out.up=shape.up;out.down=shape.down;}
    return out;
}
ro::chat::ScrollBar UIWhisperWnd::GetScrollModel() const
{
    const int count=static_cast<int>(m_rows.size());
    return {m_childHeight-38,10,m_view.LastFirst(count),m_view.scrollPosition,m_view.rows,count};
}
void UIWhisperWnd::ResizeSource(int width,int height)
{
    width=280+(std::clamp(width,280,512)-280)/32*32;
    height=120+(std::clamp(height,120,384)-120)/32*32;
    if(width==m_w && height==m_h)return;
    Resize(width,height);m_childWidth=width;m_childHeight=height;
    m_view.Resize((height-38)/14,static_cast<int>(m_rows.size()));
}
void UIWhisperWnd::RestoreInfo()
{
    auto& info=g_windowMgr.m_whisperWindowInfo;const auto client=g_windowMgr.GetChatClientRect();
    if(info.width<280 || info.height<120 || info.x<0 || info.y<0
        || static_cast<long long>(info.x)+info.width>client.right || static_cast<long long>(info.y)+info.height>client.bottom)
        info={0,347,280,120};
    ResizeSource(info.width,info.height);Move(info.x,info.y);
}
void UIWhisperWnd::StoreInfo()
{
    g_windowMgr.m_whisperWindowInfo={m_x,m_y,m_w,m_h};
    auto& windows=g_windowMgr.m_whisperWindows;const auto found=windows.find(m_name);
    if(found!=windows.end() && found->second==this)windows.erase(found);
}
void UIWhisperWnd::Append(const char* text,u32 rgb)
{
    if(!text)return;
    for(const auto& part:ro::chat::SplitNotice(text,(std::max)(1,80*m_w/640))){
        const int before=static_cast<int>(m_rows.size());
        if(before>=240)m_rows.erase(m_rows.begin(),m_rows.begin()+before/2);
        m_rows.push_back({part,rgb,{}});m_view.AfterAppend(before,static_cast<int>(m_rows.size()));
    }
    Invalidate();
}
std::vector<SubChatRow> UIWhisperWnd::GetVisibleRows() const
{
    if(m_view.first<0 || m_view.first>=static_cast<int>(m_rows.size()))return {};
    return {m_rows.begin()+m_view.first,m_rows.begin()+(std::min)(static_cast<int>(m_rows.size()),m_view.first+m_view.rows)};
}
void UIWhisperWnd::ScrollHistory(int delta){m_view.Scroll(delta,static_cast<int>(m_rows.size()));Invalidate();}
void UIWhisperWnd::OpenMenu(int x,int y)
{
    m_menuItems.clear();
    if(g_windowMgr.m_chatRoomMenuWnd)g_windowMgr.DeleteWindow(g_windowMgr.m_chatRoomMenuWnd);
    ro::UiMessageStrings table;table.Load();
    m_menuItems.push_back({11,table.Get(312,"Save chat")});
    if(!g_session.IsFriendName(m_name.c_str()) && g_serverType!=ServerInstantEvent)
        m_menuItems.push_back({21,table.Get(358,"Register as a Friend")});
    g_modeMgr.SendMsg(114,reinterpret_cast<msgparam_t>(m_name.c_str()),0,0);
    auto* menu=new UIChatRoomMenuWnd(m_menuItems,x,y,true);
    g_windowMgr.m_chatRoomMenuWnd=menu;
    g_windowMgr.m_children.insert(std::find(g_windowMgr.m_children.begin(),g_windowMgr.m_children.end(),g_windowMgr.m_modalWindow),menu);
}
bool UIWhisperWnd::WriteChatToFile(bool report)
{
    std::string name=report?g_session.GetPlayerName():m_name;
    for(auto& c:name)if(std::string_view("\\/:*?\"<>|").find(c)!=std::string_view::npos)c='_';
    // Writable app-data ownership replaces the selected client's g_baseDir3.
    const auto directory=GetOpenMidgardIniPath().parent_path()/(report?"Report":"Whisper");
    std::error_code error;std::filesystem::create_directories(directory,error);if(error)return false;
    auto path=directory/std::filesystem::u8path(name+".txt");unsigned suffix=0;
    while(std::filesystem::exists(path,error)){
        std::ostringstream filename;filename<<name<<std::setfill('0')<<std::setw(3)<<++suffix<<".txt";
        path=directory/std::filesystem::u8path(filename.str());if(error || !suffix)return false;
    }
    if(error)return false;
    if(!report){
        std::ofstream file(path,std::ios::binary);if(!file)return false;
        // Original fopen("w") translates every LF, including embedded LFs.
        for(const auto& row:m_rows){for(char c:row.text){if(c=='\n')file.put('\r');file.put(c);}file.write("\r\n",2);}
        // Source ignores fwrite/fclose results once fopen succeeds.
        file.close();
    }
    // Selected PE 44E720 returns success without creating a report file.
    const auto notice=path.u8string()+" is Saved.";g_windowMgr.SendMsg(3,0,0,0);
    g_windowMgr.SendMsg(1,reinterpret_cast<msgparam_t>(notice.c_str()),65280);return true;
}
void UIWhisperWnd::SetInputText(const std::string& text){m_input=ro::chat::CleanInput(text,70,70);m_pendingUtf8.clear();Invalidate();}
bool UIWhisperWnd::SubmitInput()
{
    const auto text=m_input;
    if(!text.empty()){
        m_historyBrowse=std::clamp(m_historyBrowse,0,static_cast<int>(m_inputHistory.size()));
        if(m_historyBrowse==static_cast<int>(m_inputHistory.size())){
            if(m_inputHistory.size()>=30){m_inputHistory.erase(m_inputHistory.begin());--m_historyBrowse;}
            m_inputHistory.push_back(text);
        }else m_inputHistory[m_historyBrowse]=text;
        ++m_historyBrowse;
        SetInputText(m_historyBrowse==static_cast<int>(m_inputHistory.size())?"":m_inputHistory[m_historyBrowse]);
    }
    g_windowMgr.m_editWindow=this;
    if(text.empty())return false;
    if(g_serviceType==ServiceAmerica && std::any_of(text.begin(),text.end(),[](unsigned char c){return c>=0x80;})){
        ro::UiMessageStrings table;table.Load();const auto message=table.Get(190,"Only English text is allowed.");
        g_windowMgr.SendMsg(1,reinterpret_cast<msgparam_t>(message.c_str()),255);return false;
    }
    if(text[0]=='/' || m_name.empty()){
        return g_modeMgr.SendMsg(CGameMode::GameMsg_SubmitChat,reinterpret_cast<msgparam_t>(text.c_str()),0,reinterpret_cast<msgparam_t>(this))!=0;
    }
    auto* mode=g_modeMgr.GetCurrentGameMode();return mode && mode->SendSourceWhisper(m_name,text);
}
void UIWhisperWnd::OnLBtnDown(int x,int y)
{
    g_windowMgr.m_editWindow=this;if(g_windowMgr.m_chatWnd)g_windowMgr.m_chatWnd->ClearInputFocus();
    const int localX=x-m_x,localY=y-m_y;const auto layout=GetLayout();
    if(In(layout.close,localX,localY)){m_closePressed=true;Invalidate();return;}
    if(In(layout.track,localX,localY)){
        const auto model=GetScrollModel();const auto hit=model.Hit(localX-layout.track.left,localY-layout.track.top);
        if(hit==ro::chat::ScrollHit::Thumb){m_scrollDragging=true;m_scrollDrag.Begin(localY-layout.track.top);}
        else ScrollHistory(model.Step(hit));return;
    }
    if(In(layout.history,localX,localY)){
        const int row=m_view.first+(localY-layout.history.top)/14;
        m_view.selected=row>=0 && row<static_cast<int>(m_rows.size())?row:-1;Invalidate();return;
    }
    if(In(layout.edit,localX,localY))return;
    m_resizing=In(layout.resizer,localX,localY);m_dragging=!m_resizing && localY<17;
    m_startX=x;m_startY=y;m_oldX=m_x;m_oldY=m_y;m_oldW=m_w;m_oldH=m_h;
}
void UIWhisperWnd::OnMouseMove(int x,int y)
{
    if(m_scrollDragging){const auto layout=GetLayout();const int before=m_view.first;
        ScrollHistory(m_scrollDrag.Request(y-m_y-layout.track.top,GetScrollModel()));m_scrollDrag.Applied(before,m_view.first);}
    else if(m_resizing)ResizeSource(m_oldW+x-m_startX,m_oldH+y-m_startY);
    else if(m_dragging)Move(m_oldX+x-m_startX,m_oldY+y-m_startY);
}
void UIWhisperWnd::CancelPointerCapture()
{
    m_dragging=m_resizing=m_scrollDragging=m_closePressed=false;
    if(g_windowMgr.m_captureWindow==this)g_windowMgr.m_captureWindow=nullptr;Invalidate();
}
void UIWhisperWnd::OnLBtnUp(int x,int y)
{
    const bool close=m_closePressed && In(GetLayout().close,x-m_x,y-m_y);CancelPointerCapture();
    if(close)g_windowMgr.DeleteWindow(this);
}
void UIWhisperWnd::OnWheel(int delta){ScrollHistory(delta>=0?-3:3);}
void UIWhisperWnd::OnChar(char c)
{
    const auto byte=static_cast<unsigned char>(c);
    if(byte<0x80){m_pendingUtf8.clear();if(c=='\r' || c=='\n')return;
        if(c=='\b')ro::chat::EraseLastCharacter(m_input);else if(byte>=32)SetInputText(m_input+c);}
    else if(byte>=0xc2 && byte<=0xf4)m_pendingUtf8.assign(1,c);
    else if(byte>=0x80 && byte<=0xbf && !m_pendingUtf8.empty()){
        m_pendingUtf8+=c;const auto first=static_cast<unsigned char>(m_pendingUtf8[0]);
        if(m_pendingUtf8.size()==static_cast<std::size_t>(first<0xe0?2:first<0xf0?3:4))SetInputText(m_input+m_pendingUtf8);
    }else m_pendingUtf8.clear();Invalidate();
}
void UIWhisperWnd::OnKeyDown(int key)
{
    if(key==VK_RETURN){SubmitInput();return;}
    if(key==VK_UP && m_historyBrowse>0)SetInputText(m_inputHistory[--m_historyBrowse]);
    else if(key==VK_DOWN && m_historyBrowse<static_cast<int>(m_inputHistory.size())){
        ++m_historyBrowse;SetInputText(m_historyBrowse==static_cast<int>(m_inputHistory.size())?"":m_inputHistory[m_historyBrowse]);}
    // Source default cancel has no matching close action: Escape is inert.
}
void UIWhisperWnd::OnDraw()
{
    auto dc=AcquireDrawTarget();if(!dc)return;const int saved=SaveDC(dc);IntersectClipRect(dc,m_x,m_y,m_x+m_w,m_y+m_h);
    const auto global=[&](RECT r){r.left+=m_x;r.right+=m_x;r.top+=m_y;r.bottom+=m_y;return r;};const auto layout=GetLayout();
    Box(dc,{m_x,m_y,m_x+m_w,m_y+m_h},RGB(230,230,230));Box(dc,{m_x,m_y,m_x+m_w,m_y+17},RGB(90,100,110));
    Text(dc,{m_x+9,m_y+2,m_x+m_w-21,m_y+17},"With  "+m_title,0xffffff);
    Box(dc,global(layout.history),RGB(255,255,255));int y=m_y+20;
    for(const auto& row:GetVisibleRows()){Text(dc,{m_x+3,y,m_x+layout.history.right-(GetScrollModel().maximum>0?13:0),y+14},row.text,row.color);y+=14;}
    if(GetScrollModel().maximum>0)ro::chat::DrawScrollBar(dc,global(layout.track),global(layout.thumb),10);
    Box(dc,global(layout.edit),RGB(250,250,250));Text(dc,global(layout.edit),m_input,0x222222);
    Text(dc,global(layout.close),"x",0xffffff);Text(dc,global(layout.resizer),"/",0x555555);
    RestoreDC(dc,saved);ReleaseDrawTarget(dc);
}
msgresult_t UIWhisperWnd::SendMsg(UIWindow* sender,int message,msgparam_t value,msgparam_t detail,msgparam_t extra)
{
    switch(message){
    case 3:ScrollHistory(1-m_view.rows);return 0;
    case 4:ScrollHistory(m_view.rows-1);return 0;
    case 14:if(value==7)ResizeSource(static_cast<int>(detail),static_cast<int>(extra));return 0;
    case 34:RestoreInfo();return 0;
    case 37:Append(reinterpret_cast<const char*>(value),ro::chat::ColorRefToUiRgb(static_cast<u32>(detail)));return 0;
    case 48:return value>=0 && static_cast<std::size_t>(value)<m_menuItems.size() && m_menuItems[value].action==11 && WriteChatToFile()?1:0;
    case 49: {
        POINT point{};GetCursorPos(&point);if(g_hMainWnd)ScreenToClient(g_hMainWnd,&point);
        if(IsQtUiRuntimeEnabled()){point.x=UiScaleRawToLogicalCoordinate(point.x);point.y=UiScaleRawToLogicalCoordinate(point.y);}
        OpenMenu(point.x,point.y);return 0;
    }
    case 6:if(value==118)SubmitInput();else if(value==135)g_windowMgr.DeleteWindow(this);return 0;
    default:return UIWindow::SendMsg(sender,message,value,detail,extra);
    }
}
