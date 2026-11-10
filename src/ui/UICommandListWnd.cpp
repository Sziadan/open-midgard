#include "UICommandListWnd.h"
#include "ChatScrollBarUi.h"
#include "UIWindowMgr.h"
#include "core/Globals.h"
#include "core/SourceCommandListTables.h"
#include "core/UiMessageStrings.h"
#include "gamemode/Mode.h"
#include "session/Session.h"
#include <algorithm>
#include <cstring>
#include <cstdio>
#if RO_ENABLE_QT6_UI
#include <QFont>
#include <QImage>
#include <QPainter>
#endif

namespace {
#include "core/SourceCommandHoverHelp.generated.inc"
std::string Utf8(const char* raw)
{
    if(!raw)return {};
#if RO_PLATFORM_WINDOWS
    const UINT page=g_codePage>0?static_cast<UINT>(g_codePage):CP_ACP;
    if(page!=CP_UTF8){const int n=MultiByteToWideChar(page,0,raw,-1,nullptr,0);
        if(n>0){std::wstring wide(n,L'\0');MultiByteToWideChar(page,0,raw,-1,wide.data(),n);
            const int size=WideCharToMultiByte(CP_UTF8,0,wide.c_str(),-1,nullptr,0,nullptr,nullptr);
            if(size>0){std::string out(size,'\0');WideCharToMultiByte(CP_UTF8,0,wide.c_str(),-1,out.data(),size,nullptr,nullptr);out.pop_back();return out;}}}
#endif
    return raw;
}
void Text(HDC dc,RECT rect,const std::string& text,u32 rgb)
{
    if(rect.right<=rect.left || rect.bottom<=rect.top)return;
#if RO_ENABLE_QT6_UI
    QImage image(rect.right-rect.left,rect.bottom-rect.top,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);
    QPainter painter(&image);QFont font("Segoe UI");font.setPixelSize(12);painter.setFont(font);painter.setPen(QColor::fromRgb(rgb));
    painter.drawText(image.rect(),Qt::AlignLeft|Qt::AlignTop|Qt::TextSingleLine,QString::fromUtf8(text.c_str()));painter.end();
    AlphaBlendArgbToHdc(dc,rect.left,rect.top,image.width(),image.height(),reinterpret_cast<const unsigned int*>(image.constBits()),image.width(),image.height());
#else
    shopui::DrawWindowTextRect(dc,rect,text,RGB((rgb>>16)&255,(rgb>>8)&255,rgb&255),DT_LEFT|DT_TOP|DT_SINGLELINE|DT_NOPREFIX);
#endif
}
std::pair<const ro::chat::CommandListEntry*,std::size_t> Table(int state)
{
    using namespace ro::chat;
    if(state==1)return {SourceCommandActions,std::size(SourceCommandActions)};
    if(state==2)return {g_serviceType==ServiceBrazil?SourceCommandWindowsBrazil:SourceCommandWindows,std::size(SourceCommandWindows)};
    return {SourceCommandToggles,std::size(SourceCommandToggles)};
}
}

UICommandListTab::UICommandListTab()
{
    m_id=117;Create(20,132);m_state=0;
    for(int i=0;i<3;++i){char name[64];std::snprintf(name,sizeof(name),"basic_interface\\tab_cmd_%02d.bmp",i+1);
        m_bitmaps[i]=LoadChatRoomSkinBitmap(name,false);m_uris[i]=ChatRoomBitmapUri(m_bitmaps[i]);}
}
const shopui::BitmapPixels& UICommandListTab::GetBitmap() const
{
    static const shopui::BitmapPixels empty;return m_state>=0 && m_state<3?m_bitmaps[m_state]:empty;
}
std::string UICommandListTab::GetBitmapUri() const {return m_state>=0 && m_state<3?m_uris[m_state]:std::string();}
msgresult_t UICommandListTab::SendMsg(UIWindow*,int msg,msgparam_t value,msgparam_t,msgparam_t)
{
    if(msg==13){m_state=static_cast<int>(value);Invalidate();}return 0;
}
void UICommandListTab::OnLBtnDown(int,int y)
{
    const int state=(std::max)(0,(std::min)(2,(y-m_y)/(m_h/3)));
    if(m_parent && state!=m_state){m_state=state;Invalidate();m_parent->SendMsg(this,22,state,0,0);}
}
void UICommandListTab::OnDraw()
{
    auto dc=AcquireDrawTarget();if(!dc)return;const auto& bitmap=GetBitmap();
    if(bitmap.IsValid())shopui::DrawBitmapPixelsTransparent(dc,bitmap,{m_x,m_y,m_x+bitmap.width,m_y+bitmap.height});
    ReleaseDrawTarget(dc);
}
UICommandListScrollBar::UICommandListScrollBar(UICommandListWnd* owner):m_owner(owner){m_id=117;Create(13,owner->m_h-38);}
void UICommandListScrollBar::CancelPointerCapture(){m_dragging=false;}
void UICommandListScrollBar::OnLBtnUp(int,int){m_dragging=false;}
void UICommandListScrollBar::OnLBtnDown(int x,int y)
{
    const auto hit=m_owner->GetScrollBar().Hit(x-m_x,y-m_y);
    if(hit==ro::chat::ScrollHit::Thumb){m_dragging=true;m_drag.Begin(y);}
    else if(hit==ro::chat::ScrollHit::Up || hit==ro::chat::ScrollHit::Down)m_owner->SendMsg(this,7,hit==ro::chat::ScrollHit::Up?-1:1,0,0);
    else if(hit==ro::chat::ScrollHit::PageUp || hit==ro::chat::ScrollHit::PageDown)m_owner->SendMsg(this,hit==ro::chat::ScrollHit::PageUp?9:10,0,0,0);
}
void UICommandListScrollBar::OnMouseMove(int,int y)
{
    if(m_dragging){const int old=m_owner->GetOffset();m_owner->SendMsg(this,7,m_drag.Request(y,m_owner->GetScrollBar()),0,0);m_drag.Applied(old,m_owner->GetOffset());}
}
void UICommandListScrollBar::OnWheel(int delta){m_owner->OnWheel(delta);}
void UICommandListScrollBar::OnDraw()
{
    auto dc=AcquireDrawTarget();if(!dc)return;const RECT rail{m_x,m_y,m_x+m_w,m_y+m_h};
    ro::chat::DrawScrollBar(dc,rail,ro::chat::ScrollLayout(rail,m_owner->GetScrollBar()).thumb,13);ReleaseDrawTarget(dc);
}
UICommandListWnd::UICommandListWnd()
{
    m_id=112;m_defPushId=117;m_defCancelPushId=117;Create(250,200);
    m_tab=new UICommandListTab();m_tab->Move(0,17);AddChild(m_tab);
    m_scroll=new UICommandListScrollBar(this);m_scroll->Move(m_w-15,14);AddChild(m_scroll);
    const char* names[]={"base","close"};
    for(int i=0;i<2;++i){const auto base=std::string("basic_interface\\sys_")+names[i];
        m_buttons[i]=new UIFriendOptionButton(134+i,(base+"_off.bmp").c_str(),(base+"_on.bmp").c_str(),(base+"_on.bmp").c_str(),11,11);
        m_buttons[i]->Move(i?m_w-14:3,3);m_buttons[i]->SetToolTip(i?"Alt+Y":"");AddChild(m_buttons[i]);}
    for(int i=0;i<2;++i){m_indicators[i]=LoadChatRoomSkinBitmap(i?"basic_interface\\grp_online.bmp":"basic_interface\\grp_offline.bmp");m_indicatorUris[i]=ChatRoomBitmapUri(m_indicators[i]);}
    const char* frame[]={"titlebar_left.bmp","titlebar_mid.bmp","titlebar_right.bmp","itemwin_left.bmp","itemwin_right.bmp","btnbar_left.bmp","btnbar_mid.bmp","btnbar_right.bmp"};
    for(int i=0;i<8;++i){m_frame[i]=LoadChatRoomSkinBitmap((std::string("basic_interface\\")+frame[i]).c_str(),false);m_frameUris[i]=ChatRoomBitmapUri(m_frame[i]);}
}
void UICommandListWnd::Move(int x,int y)
{
    const int dx=x-m_x,dy=y-m_y;UIWindow::Move(x,y);for(auto* child:m_children)child->Move(child->m_x+dx,child->m_y+dy);
}
void UICommandListWnd::CancelPointerCapture()
{
    m_isDragging=0;m_scroll->CancelPointerCapture();for(auto* button:m_buttons)button->CancelPointerCapture();
    for(auto* window=g_windowMgr.m_captureWindow;window;window=window->m_parent)if(window==this){g_windowMgr.m_captureWindow=nullptr;break;}
}
std::string UICommandListWnd::GetTitle() const {return g_serviceType==ServiceBrazil?"Lista de Comandos":"Command List";}
ro::chat::ScrollBar UICommandListWnd::GetScrollBar() const
{
    ro::chat::ScrollBar bar;bar.axis=m_h-38;bar.page=m_visibleRows;bar.total=(static_cast<int>(std::size(ro::chat::SourceCommandToggles))+1)/2;
    bar.maximum=(std::max)(0,bar.total-bar.page);bar.position=m_offset;return bar;
}
int UICommandListWnd::Scroll(msgparam_t delta)
{
    const int old=m_offset;const u32 bits=static_cast<u32>(old)+static_cast<u32>(delta);s32 raw;std::memcpy(&raw,&bits,sizeof(raw));
    if(raw!=old){m_offset=(std::max)(0,(std::min)(GetScrollBar().maximum,static_cast<int>(raw)));m_scroll->Invalidate();Invalidate();}
    return m_offset-old;
}
int UICommandListWnd::GetCurItem(int x,int y) const
{
    if(x<0 || y<17 || y>=17+20*m_visibleRows || m_w/2<=0)return -1;
    const int index=2*m_offset+x/(m_w/2)+2*((y-17)/20);
    return index<static_cast<int>(std::size(ro::chat::SourceCommandToggles))?index:-1;
}
std::vector<CommandListDisplayRow> UICommandListWnd::GetVisibleRows() const
{
    const auto table=Table(m_tab->m_state);std::vector<CommandListDisplayRow> rows;
    for(int i=0;i<2*m_visibleRows && i+2*m_offset<static_cast<int>(table.second);++i){
        const int index=i+2*m_offset;const auto& item=table.first[index];CommandListDisplayRow row;
        row.index=index;row.value=item.value;row.caption=Utf8(item.caption);row.x=25+(m_w-40)/2*(i%2);row.y=17+20*(i/2);row.width=(m_w-40)/2;
        row.textOffset=m_tab->m_state==1 || m_tab->m_state==2?15:25;row.selected=index==m_currentItem;
        if(m_tab->m_state!=2)for(const auto& flag:g_session.m_cmdOnOffSet)if(flag.second==item.value){row.hasIndicator=true;row.on=flag.first!=0;break;}
        rows.push_back(std::move(row));
    }
    return rows;
}
std::vector<CommandListFramePiece> UICommandListWnd::GetFramePieces() const
{
    std::vector<CommandListFramePiece> pieces;
    const auto add=[&](int i,int x,int y){const auto& bitmap=m_frame[i];if(bitmap.IsValid())pieces.push_back({x,y,bitmap.width,bitmap.height,i<3?0:i<5?1:2,bitmap,m_frameUris[i]});};
    add(0,0,0);add(2,m_w-m_frame[2].width,0);for(int i=0;i<(std::max)(0,(m_w-24+11)/12);++i)add(1,12+12*i,0);
    for(int i=0;i<(std::max)(0,(m_h-42+7)/8);++i)add(3,0,19+8*i);
    for(int i=0;i<(std::max)(0,(m_h-38+7)/8);++i)add(4,m_w-m_frame[4].width,17+8*i);
    add(5,0,m_h-m_frame[5].height);add(7,m_w-m_frame[7].width,m_h-21);
    for(int i=0;i<(std::max)(0,(m_w-42+20)/21);++i)add(6,21+21*i,m_h-m_frame[6].height);
    return pieces;
}
void UICommandListWnd::OnDraw()
{
    auto dc=AcquireDrawTarget();if(!dc)return;const int saved=SaveDC(dc);IntersectClipRect(dc,m_x,m_y,m_x+m_w,m_y+m_h);
    const auto pieces=GetFramePieces();
    const auto drawPhase=[&](int phase){for(const auto& piece:pieces)if(piece.phase==phase)shopui::DrawBitmapPixelsTransparent(dc,piece.bitmap,{m_x+piece.x,m_y+piece.y,m_x+piece.x+piece.width,m_y+piece.y+piece.height});};
    shopui::FillRectColor(dc,{m_x,m_y,m_x+m_w,m_y+m_h},RGB(215,215,215));drawPhase(0);
    shopui::FillRectColor(dc,{m_x,m_y+17,m_x+m_w,m_y+m_h},RGB(215,215,215));
    shopui::FillRectColor(dc,{m_x+1,m_y+17,m_x+m_w-1,m_y+m_h},RGB(255,255,255));
    drawPhase(1);
    shopui::FillRectColor(dc,{m_x+18,m_y+17,m_x+19,m_y+m_h-4},RGB(215,215,215));
    drawPhase(2);
    Text(dc,{m_x+18,m_y+3,m_x+m_w,m_y+17},GetTitle(),0);Text(dc,{m_x+17,m_y+2,m_x+m_w,m_y+17},GetTitle(),0xffffff);
    for(const auto& row:GetVisibleRows()){
        if(row.selected){shopui::FillRectColor(dc,{m_x+row.x,m_y+row.y+2,m_x+row.x+row.width,m_y+row.y+22},RGB(128,128,128));
            shopui::FillRectColor(dc,{m_x+row.x+1,m_y+row.y+3,m_x+row.x+row.width-1,m_y+row.y+21},RGB(255,255,255));}
        Text(dc,{m_x+row.x+row.textOffset,m_y+row.y+5,m_x+m_w,m_y+row.y+20},row.caption,0);
        if(row.hasIndicator){const auto& bitmap=GetIndicatorBitmap(row.on);if(bitmap.IsValid())shopui::DrawBitmapPixelsTransparent(dc,bitmap,{m_x+row.x+2,m_y+row.y+5,m_x+row.x+2+bitmap.width,m_y+row.y+5+bitmap.height});}
    }
    DrawChildrenToHdc(dc);RestoreDC(dc,saved);ReleaseDrawTarget(dc);
}
void UICommandListWnd::OnLBtnDown(int x,int y)
{
    if(y-m_y<17){m_isDragging=1;m_startGlobalX=x;m_startGlobalY=y;m_orgX=m_x;m_orgY=m_y;return;}
    const int current=GetCurItem(x-m_x,y-m_y);if(current!=m_currentItem){m_currentItem=current;Invalidate();}
    const auto table=Table(m_tab->m_state);
    // Keep the common23-count selection, but suppress undefined access into a
    // shorter selected table. Do not pad rows or change the helper/scroll range.
    if(current<0 || current>=static_cast<int>(table.second))return;
    const int value=table.first[current].value;
    if(m_tab->m_state==2)g_windowMgr.ProcessSourceCommandWindowKey(value);
    else if(value==150){const std::array<std::string,3> parameters{};g_modeMgr.SendMsg(47,value,reinterpret_cast<msgparam_t>(&parameters),0);}
    else g_modeMgr.SendMsg(47,value,0,0);
}
std::string UICommandListWnd::HoverHelp(int index) const
{
    const auto table=Table(m_tab->m_state);if(index<0 || index>=static_cast<int>(table.second))return {};
    if(m_tab->m_state==2)return ro::chat::SourceCommandWindowKeys[index].caption;
    for(const auto& entry:kSourceCommandHelpEntries)if(entry.type==table.first[index].value){
        int id=entry.first;
        if(entry.condition==SourceCommandHelpCondition::Miss)id=g_session.m_bMakeMissEffect?entry.first:entry.second;
        if(entry.condition==SourceCommandHelpCondition::ShowName)id=g_session.m_isShowNameTypeOld?entry.first:entry.second;
        if(entry.condition==SourceCommandHelpCondition::Set2 && g_serviceType==ServiceAmerica)return "/set2 : /set1 + /q3";
        if(id<0)return {};
        if(m_helpService!=static_cast<int>(g_serviceType) || m_helpCodePage!=g_codePage || m_helpFolder!=g_readFolderFirst){
            m_helpStrings.Load();m_helpService=static_cast<int>(g_serviceType);m_helpCodePage=g_codePage;m_helpFolder=g_readFolderFirst;}
        return m_helpStrings.Get(id,"");
    }
    return {};
}
void UICommandListWnd::OnMouseHover(int x,int y)
{
    const int current=GetCurItem(x-m_x,y-m_y);if(current!=m_currentItem){m_currentItem=current;Invalidate();}
    const auto table=Table(m_tab->m_state);if(current<0 || current>=static_cast<int>(table.second))return;
    if(auto* mode=g_modeMgr.GetCurrentMode()){
        mode->SetCursorAction(2);auto help=HoverHelp(current);
        // Bound the original128-byte stack copy; preserve complete source bytes
        // for all defined lengths, and never treat it as a previous-help cache.
        if(help.size()>127)help.resize(127);
        if(!help.empty())mode->MakeHelpBalloon(help.c_str(),m_x+20*((x-m_x-40)/20+2),m_y+20*((y-m_y-17)/20)+1,0xffffff);
    }
}
void UICommandListWnd::OnMouseMove(int x,int y)
{
    if(m_isDragging)UIFrameWnd::OnMouseMove(x,y);else OnMouseHover(x,y);
}
void UICommandListWnd::OnWheel(int delta){if(GetScrollBar().maximum>0){const int step=(std::min)(m_visibleRows-1,3);Scroll(delta>=0?-step:step);}}
void UICommandListWnd::OnKeyDown(int key){if(key==VK_RETURN || key==VK_ESCAPE)SendMsg(this,6,key==VK_RETURN?m_defPushId:m_defCancelPushId,0,0);}
msgresult_t UICommandListWnd::SendMsg(UIWindow*,int msg,msgparam_t value,msgparam_t,msgparam_t)
{
    if(msg==6){if(value==135)g_windowMgr.DeleteWindow(this);return 0;}
    if(msg==7)return Scroll(value);if(msg==9)return Scroll(1-m_visibleRows);if(msg==10)return Scroll(m_visibleRows-1);
    if(msg==22){Invalidate();return 0;}return 0;
}
