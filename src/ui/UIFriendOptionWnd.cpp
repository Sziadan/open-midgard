#include "UIFriendOptionWnd.h"
#include "UIWindowMgr.h"
#include "core/Globals.h"
#include "core/UiMessageStrings.h"
#include "session/Session.h"
#if RO_ENABLE_QT6_UI
#include <QFont>
#include <QFontMetrics>
#include <QImage>
#include <QPainter>
#endif

namespace {
bool In(const UIWindow& w,int x,int y){return x>=w.m_x && x<w.m_x+w.m_w && y>=w.m_y && y<w.m_y+w.m_h;}
void Text(HDC dc,RECT r,const std::string& text,u32 rgb=0)
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
UIFriendOptionButton::UIFriendOptionButton(int id,const char* normal,const char* hover,const char* pressed,int width,int height)
{
    m_id=id;m_button.id=id;const char* names[]={normal,hover,pressed};int loadedWidth=0,loadedHeight=0;
    for(int i=0;i<3;++i){m_button.bitmaps[i]=LoadChatRoomSkinBitmap(names[i]);m_button.uris[i]=ChatRoomBitmapUri(m_button.bitmaps[i]);
        if(m_button.bitmaps[i].IsValid()){loadedWidth=(std::max)(loadedWidth,m_button.bitmaps[i].width);loadedHeight=(std::max)(loadedHeight,m_button.bitmaps[i].height);}}
    Create(loadedWidth?loadedWidth:width,loadedHeight?loadedHeight:height);
}
void UIFriendOptionButton::CancelPointerCapture(){m_pressed=false;Invalidate();}
void UIFriendOptionButton::OnLBtnDown(int x,int y){if(!m_isDisabled && In(*this,x,y)){m_pressed=true;m_hover=true;Invalidate();}}
void UIFriendOptionButton::OnLBtnUp(int x,int y)
{
    const bool activate=m_pressed && !m_isDisabled && In(*this,x,y) && g_windowMgr.m_captureWindow==this;
    m_pressed=false;m_hover=In(*this,x,y);Invalidate();if(activate && m_parent)m_parent->SendMsg(this,6,m_id,0,0);
}
void UIFriendOptionButton::OnMouseMove(int x,int y){const bool hover=In(*this,x,y);if(hover!=m_hover){m_hover=hover;Invalidate();}}
void UIFriendOptionButton::OnDraw()
{
    auto dc=AcquireDrawTarget();if(!dc)return;const auto& bitmap=m_button.bitmaps[GetVisualState()];
    if(bitmap.IsValid())shopui::DrawBitmapPixelsTransparent(dc,bitmap,{m_x,m_y,m_x+bitmap.width,m_y+bitmap.height});
    else {const int shade=m_pressed?180:210;shopui::FillRectColor(dc,{m_x,m_y,m_x+m_w,m_y+m_h},RGB(shade,shade,shade));Text(dc,{m_x,m_y,m_x+m_w,m_y+m_h},m_text.empty()?(m_id==135?"x":"*"):m_text);}
    ReleaseDrawTarget(dc);
}
UIFriendOptionCheckBox::UIFriendOptionCheckBox(std::string label,bool checked,int maximumWidth):m_checked(checked)
{
    m_id=147;m_text=std::move(label);int width=static_cast<int>(m_text.size())*6;
#if RO_ENABLE_QT6_UI
    QFont font("Segoe UI");font.setPixelSize(12);width=QFontMetrics(font).horizontalAdvance(QString::fromUtf8(m_text.c_str()));
#endif
    Create((std::min)(width+12,maximumWidth),12);m_on=LoadChatRoomSkinBitmap("checkbox_1.bmp");m_off=LoadChatRoomSkinBitmap("checkbox_0.bmp");
    m_onUri=ChatRoomBitmapUri(m_on);m_offUri=ChatRoomBitmapUri(m_off);
}
void UIFriendOptionCheckBox::OnLBtnDown(int,int)
{
    if(m_isDisabled)return;m_checked=!m_checked;Invalidate();if(m_parent)m_parent->SendMsg(this,6,147,m_checked?1:0,0);
}
void UIFriendOptionCheckBox::OnDraw()
{
    auto dc=AcquireDrawTarget();if(!dc)return;const int saved=SaveDC(dc);IntersectClipRect(dc,m_x,m_y,m_x+m_w,m_y+m_h);
    const auto& bitmap=GetBitmap();
    if(bitmap.IsValid()){const int y=m_y+(m_h-bitmap.height)/2;shopui::DrawBitmapPixelsTransparent(dc,bitmap,{m_x,y,m_x+bitmap.width,y+bitmap.height});}
    else {shopui::FillRectColor(dc,{m_x,m_y,m_x+10,m_y+10},RGB(255,255,255));if(m_checked)Text(dc,{m_x,m_y,m_x+11,m_y+12},"x");}
    Text(dc,{m_x+11,m_y,m_x+m_w,m_y+12},m_text);RestoreDC(dc,saved);ReleaseDrawTarget(dc);
}
UIFriendOptionWnd::UIFriendOptionWnd()
{
    m_id=68;Create(280,120);ro::UiMessageStrings table;table.Load();m_caption=table.Get(355,"Friend settings");
    const int ids[]={361,359,362};const char* fallback[]={"Open private windows","Open friend private windows","Play private-window sound"};
    const bool values[]={g_session.m_isShowWhisperWnd,g_session.m_isShowWhisperWnd_Friend,g_session.m_isPlayWhisperOpenSound};
    for(int i=0;i<3;++i){m_rows[i]=new UIFriendOptionCheckBox(table.Get(ids[i],fallback[i]),values[i]);m_rows[i]->Move(6,21+16*i);AddChild(m_rows[i]);}
    m_close=new UIFriendOptionButton(135,"basic_interface\\sys_close_off.bmp","basic_interface\\sys_close_on.bmp","basic_interface\\sys_close_on.bmp",12,12);
    m_close->Move(266,3);m_close->SetToolTip("Alt+I");AddChild(m_close);
}
void UIFriendOptionWnd::Move(int x,int y)
{
    const int dx=x-m_x,dy=y-m_y;UIWindow::Move(x,y);for(auto* child:m_children)child->Move(child->m_x+dx,child->m_y+dy);
}
void UIFriendOptionWnd::StoreInfo(){if(m_info)*m_info={m_x,m_y,m_w,m_h};}
void UIFriendOptionWnd::CancelPointerCapture()
{
    m_isDragging=0;m_close->CancelPointerCapture();
    for(auto* w=g_windowMgr.m_captureWindow;w;w=w->m_parent)if(w==this){g_windowMgr.m_captureWindow=nullptr;break;}
}
msgresult_t UIFriendOptionWnd::SendMsg(UIWindow* sender,int message,msgparam_t value,msgparam_t detail,msgparam_t)
{
    if(message==6){
        if(value==135){g_windowMgr.DeleteWindow(this);return 0;}
        if(value==147){if(sender==m_rows[0])g_session.m_isShowWhisperWnd=detail!=0;
            else if(sender==m_rows[1])g_session.m_isShowWhisperWnd_Friend=detail!=0;
            else if(sender==m_rows[2])g_session.m_isPlayWhisperOpenSound=detail!=0;}
        return 0;
    }
    if(message==34 && value){m_info=reinterpret_cast<SubChatWindowInfo*>(value);const auto client=g_windowMgr.GetChatClientRect();
        if(m_info->width!=280 || m_info->height!=120 || m_info->x<0 || m_info->y<0
            || static_cast<long long>(m_info->x)+280>client.right || static_cast<long long>(m_info->y)+120>client.bottom)*m_info={200,120,280,120};
        Move(m_info->x,m_info->y);}
    return 0;
}
void UIFriendOptionWnd::OnDraw()
{
    auto dc=AcquireDrawTarget();if(!dc)return;const int saved=SaveDC(dc);IntersectClipRect(dc,m_x,m_y,m_x+m_w,m_y+m_h);
    shopui::FillRectColor(dc,{m_x,m_y,m_x+m_w,m_y+m_h},RGB(232,232,232));shopui::FillRectColor(dc,{m_x,m_y,m_x+m_w,m_y+17},RGB(130,140,150));
    Text(dc,{m_x+5,m_y+3,m_x+m_w,m_y+17},m_caption,0xffffff);Text(dc,{m_x+4,m_y+2,m_x+m_w,m_y+17},m_caption);
    shopui::FillRectColor(dc,{m_x+1,m_y+17,m_x+m_w-1,m_y+m_h},RGB(242,242,242));DrawChildrenToHdc(dc);
    RestoreDC(dc,saved);ReleaseDrawTarget(dc);
}
