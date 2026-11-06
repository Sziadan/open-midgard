#include "UIAutoMessageWnd.h"
#include "UIWindowMgr.h"
#include "UIWhisperWnd.h"
#include "UINewChatWnd.h"
#include "ChatText.h"
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
bool In(RECT r,int x,int y){return x>=r.left && x<r.right && y>=r.top && y<r.bottom;}
RECT Global(RECT r,const UIWindow& owner){r.left+=owner.m_x;r.right+=owner.m_x;r.top+=owner.m_y;r.bottom+=owner.m_y;return r;}
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

UIAutoMessageWnd::UIAutoMessageWnd()
{
    m_id=116;Create(280,48);const auto client=g_windowMgr.GetChatClientRect();
    Move(185+(client.right-640)/2,180+(client.bottom-480)/2);
    ro::UiMessageStrings table;table.Load();m_prompt=table.Get(944,"Enter automatic reply.");
    SetInputText(g_session.m_strAutoMSG);m_selected=true;
    m_ok.id=118;m_ok.x=225;m_ok.y=16;
    const char* names[]={"btn_ok.bmp","btn_ok_a.bmp","btn_ok_b.bmp"};
    for(int i=0;i<3;++i){m_ok.bitmaps[i]=LoadChatRoomSkinBitmap(names[i]);m_ok.uris[i]=ChatRoomBitmapUri(m_ok.bitmaps[i]);}
}
void UIAutoMessageWnd::SetInputText(const std::string& text)
{
    const std::string_view value(text.c_str());
    m_input=ro::chat::CleanInput(value,value.size(),value.size());
    if(m_input.size()>100){
        m_input=ro::chat::CleanInput(m_input,100,100);
        // Source SetTextByMaxChar trims at a character boundary, then pads
        // with spaces to maxchar. Typed insertion uses a separate byte cap.
        m_input.append(100-m_input.size(),' ');
    }
    m_pendingUtf8.clear();m_selected=false;Invalidate();
}
void UIAutoMessageWnd::FocusForDraw()
{
    if(m_show && !g_windowMgr.m_modalWindow){g_windowMgr.m_editWindow=this;if(g_windowMgr.m_chatWnd)g_windowMgr.m_chatWnd->ClearInputFocus();}
}
void UIAutoMessageWnd::SubmitInput()
{
    g_session.m_strAutoMSG=m_input;ro::UiMessageStrings table;table.Load();const auto notice=table.Get(942,"Automatic reply enabled.");
    g_windowMgr.SendMsg(1,reinterpret_cast<msgparam_t>(notice.c_str()),6619135);
    // Source scans visible owners in order before deleting this editor. The
    // main common edit stops that scan without taking focus. Represented chat
    // editors stand in for their source child controls; never restore a raw
    // pointer captured when this window was created.
    for(auto* owner:g_windowMgr.m_children){
        if(!owner->m_show)continue;
        if(owner==g_windowMgr.m_chatWnd)break;
        if(owner==this || dynamic_cast<UIWhisperWnd*>(owner) || dynamic_cast<UIChatRoomSettingsWnd*>(owner)
            || owner==g_windowMgr.m_chatRoomPasswordWnd || owner==g_windowMgr.m_chatRoomWnd){g_windowMgr.m_editWindow=owner;break;}
        bool found=false;
        for(auto* child:owner->m_children)if(dynamic_cast<UIEditCtrl*>(child) && child->CanReceiveKeyInput()){
            g_windowMgr.m_editWindow=child;found=true;break;
        }
        if(found)break;
    }
    g_windowMgr.DeleteWindow(this);
}
void UIAutoMessageWnd::CancelInput(){g_session.m_onAutoMSG=false;g_windowMgr.DeleteWindow(this);}
void UIAutoMessageWnd::CancelPointerCapture()
{
    m_pressed=m_dragging=false;if(g_windowMgr.m_captureWindow==this)g_windowMgr.m_captureWindow=nullptr;Invalidate();
}
void UIAutoMessageWnd::OnLBtnDown(int x,int y)
{
    FocusForDraw();const int lx=x-m_x,ly=y-m_y;
    if(In(m_ok.Bounds(),lx,ly)){m_pressed=true;Invalidate();return;}
    if(In({15,22,218,38},lx,ly)){m_selected=false;Invalidate();return;}
    m_dragging=true;m_startX=x;m_startY=y;m_oldX=m_x;m_oldY=m_y;
}
void UIAutoMessageWnd::OnLBtnUp(int x,int y)
{
    const bool submit=m_pressed && In(m_ok.Bounds(),x-m_x,y-m_y);CancelPointerCapture();if(submit)SubmitInput();
}
void UIAutoMessageWnd::OnMouseHover(int x,int y)
{
    const bool hover=In(m_ok.Bounds(),x-m_x,y-m_y);if(hover!=m_hover){m_hover=hover;Invalidate();}
}
void UIAutoMessageWnd::OnMouseMove(int x,int y){if(m_dragging)Move(m_oldX+x-m_startX,m_oldY+y-m_startY);OnMouseHover(x,y);}
void UIAutoMessageWnd::OnChar(char c)
{
    const auto append=[&](const std::string& value){if(m_selected)m_input.clear();m_selected=false;m_input=ro::chat::CleanInput(m_input+value,100,100);};
    const auto byte=static_cast<unsigned char>(c);
    if(byte<0x80){m_pendingUtf8.clear();if(c=='\r' || c=='\n')return;
        if(c=='\b'){if(m_selected){m_input.clear();m_selected=false;}else ro::chat::EraseLastCharacter(m_input);}
        else if(byte>=32)append(std::string(1,c));
    }else if(byte>=0xc2 && byte<=0xf4)m_pendingUtf8.assign(1,c);
    else if(byte>=0x80 && byte<=0xbf && !m_pendingUtf8.empty()){
        m_pendingUtf8+=c;const auto first=static_cast<unsigned char>(m_pendingUtf8[0]);const std::size_t count=first<0xe0?2:first<0xf0?3:4;
        if(m_pendingUtf8.size()==count){append(m_pendingUtf8);m_pendingUtf8.clear();}
    }else m_pendingUtf8.clear();Invalidate();
}
void UIAutoMessageWnd::OnKeyDown(int key){if(key==VK_RETURN)SubmitInput();else if(key==VK_ESCAPE)CancelInput();}
msgresult_t UIAutoMessageWnd::SendMsg(UIWindow*,int message,msgparam_t value,msgparam_t,msgparam_t)
{
    if(message==6 && value==118)SubmitInput();return 0;
}
void UIAutoMessageWnd::OnDraw()
{
    FocusForDraw();auto dc=AcquireDrawTarget();if(!dc)return;const int saved=SaveDC(dc);IntersectClipRect(dc,m_x,m_y,m_x+m_w,m_y+m_h);
    shopui::FillRectColor(dc,{m_x,m_y,m_x+m_w,m_y+m_h},RGB(232,232,232));Text(dc,Global({15,6,280,22},*this),m_prompt);
    const auto edit=Global({15,22,218,38},*this);shopui::FillRectColor(dc,edit,RGB(232,232,232));
    if(m_selected && !m_input.empty()){
        int width=static_cast<int>(m_input.size())*6;
#if RO_ENABLE_QT6_UI
        QFont font("Segoe UI");font.setPixelSize(12);width=QFontMetrics(font).horizontalAdvance(QString::fromUtf8(m_input.c_str()));
#endif
        shopui::FillRectColor(dc,{edit.left,edit.top,(std::min)(edit.right,edit.left+width),edit.bottom},RGB(0,120,215));
    }
    Text(dc,edit,m_input,m_selected?0xffffff:0);
    const int status=GetButtonState();const auto bounds=Global(m_ok.Bounds(),*this);const auto& bitmap=m_ok.bitmaps[status];
    if(bitmap.IsValid())shopui::DrawBitmapPixelsTransparent(dc,bitmap,{bounds.left,bounds.top,bounds.left+bitmap.width,bounds.top+bitmap.height});
    else {const int shade=status==2?180:210;shopui::FillRectColor(dc,bounds,RGB(shade,shade,shade));Text(dc,bounds,"OK");}
    RestoreDC(dc,saved);ReleaseDrawTarget(dc);
}
