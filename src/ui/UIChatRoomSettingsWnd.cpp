#include "UIChatRoomSettingsWnd.h"
#include "UIWindowMgr.h"
#include "ChatText.h"
#include "core/Globals.h"
#include "core/MannerFilter.h"
#include "core/UiMessageStrings.h"
#include "gamemode/GameMode.h"
#include "gamemode/Mode.h"
#if RO_ENABLE_QT6_UI
#include <QFont>
#include <QImage>
#include <QPainter>
#endif

namespace {
constexpr int people[]={2,3,5,10,15,20};
bool In(RECT r,int x,int y){return x>=r.left && x<r.right && y>=r.top && y<r.bottom;}
RECT Global(RECT r,const UIWindow& w){r.left+=w.m_x;r.right+=w.m_x;r.top+=w.m_y;r.bottom+=w.m_y;return r;}
void Text(HDC dc,RECT r,const std::string& text,bool wrap=false,u32 rgb=0)
{
    if(r.right<=r.left || r.bottom<=r.top)return;
#if RO_ENABLE_QT6_UI
    QImage image(r.right-r.left,r.bottom-r.top,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);
    QPainter painter(&image);QFont font("Segoe UI");font.setPixelSize(12);painter.setFont(font);painter.setPen(QColor::fromRgb(rgb));
    painter.drawText(image.rect(),Qt::AlignLeft|Qt::AlignTop|(wrap?Qt::TextWordWrap:Qt::TextSingleLine),QString::fromUtf8(text.c_str()));painter.end();
    AlphaBlendArgbToHdc(dc,r.left,r.top,image.width(),image.height(),reinterpret_cast<const unsigned int*>(image.constBits()),image.width(),image.height());
#else
    shopui::DrawWindowTextRect(dc,r,text,RGB((rgb>>16)&255,(rgb>>8)&255,rgb&255),DT_LEFT|DT_TOP|DT_NOPREFIX|(wrap?DT_WORDBREAK:DT_SINGLELINE));
#endif
}
RoomSettingsButton Button(int id,int x,int y,const char* a,const char* b,const char* c)
{
    RoomSettingsButton result;result.id=id;result.x=x;result.y=y;const char* names[]={a,b,c};
    for(int i=0;i<3;++i){result.bitmaps[i]=LoadChatRoomSkinBitmap(names[i]);result.uris[i]=ChatRoomBitmapUri(result.bitmaps[i]);}
    return result;
}
void PaintButton(HDC dc,const UIWindow& owner,const RoomSettingsButton& button,int state)
{
    const auto bounds=Global(button.Bounds(),owner);const auto& image=button.bitmaps[state];
    if(image.IsValid())shopui::DrawBitmapPixelsTransparent(dc,image,{bounds.left,bounds.top,bounds.left+image.width,bounds.top+image.height});
    else {
        const int shade=state==2?180:210;shopui::FillRectColor(dc,bounds,RGB(shade,shade,shade));
        const char* label=button.id==118 || button.id==121?"OK":button.id==119 || button.id==122?"Cancel":button.id==135?"x":"v";
        Text(dc,bounds,label);
    }
}
}

RECT RoomSettingsButton::Bounds() const
{
    int width=0,height=0;for(const auto& bitmap:bitmaps)if(bitmap.IsValid()){
        width=(std::max)(width,bitmap.width);height=(std::max)(height,bitmap.height);
    }
    if(!width)width=id==135 || id==162 || id==163?12:42;
    if(!height)height=id==135?12:id==162 || id==163?16:20;
    return {x,y,x+width,y+height};
}
UIChatRoomSettingsWnd::UIChatRoomSettingsWnd(bool change):m_change(change)
{
    m_id=change?30:27;Create(280,120);const auto client=g_windowMgr.GetChatClientRect();
    Move((client.right-640)/2+185,300*client.bottom/480);
    ro::UiMessageStrings table;table.Load();m_caption=table.Get(change?126:125,change?"Room settings":"Create room");
    m_suffix=table.Get(131," people");m_classLabel=table.Get(130,"Chat room");
    m_publicLabel=table.Get(201,"Public");m_privateLabel=table.Get(200,"Private");
    m_titlebar=LoadChatRoomSkinBitmap("basic_interface\\titlebar_fix.bmp");m_background=LoadChatRoomSkinBitmap("basic_interface\\chatwin0_bg.bmp");
    m_titlebarUri=ChatRoomBitmapUri(m_titlebar);m_backgroundUri=ChatRoomBitmapUri(m_background);
    m_buttons={Button(118,189,96,"btn_ok.bmp","btn_ok_a.bmp","btn_ok_b.bmp"),
        Button(119,234,96,"btn_cancel.bmp","btn_cancel_a.bmp","btn_cancel_b.bmp"),
        Button(135,266,3,"basic_interface\\sys_close_off.bmp","basic_interface\\sys_close_on.bmp","basic_interface\\sys_close_on.bmp"),
        Button(162,119,46,"basic_interface\\txtbox_btn_a.bmp","basic_interface\\txtbox_btn_b.bmp","basic_interface\\txtbox_btn_c.bmp"),
        Button(163,256,46,"basic_interface\\txtbox_btn_a.bmp","basic_interface\\txtbox_btn_b.bmp","basic_interface\\txtbox_btn_c.bmp")};
}
const RoomSettingsButton* UIChatRoomSettingsWnd::GetButton(int id) const
{
    for(const auto& button:m_buttons)if(button.id==id)return &button;return nullptr;
}
std::string UIChatRoomSettingsWnd::GetPeopleLabel(int index) const
{
    index=index<0?m_displayPeople:index;
    return index>=0 && index<6?std::to_string(people[index])+m_suffix:std::string();
}
void UIChatRoomSettingsWnd::ChoosePeople(int index)
{
    if(index<0 || index>=6)return;m_people=people[index];m_displayPeople=index;m_popup=-1;Invalidate();
}
void UIChatRoomSettingsWnd::SetTitleText(const std::string& text){m_title=ro::chat::CleanInput(text,36,36);m_pendingUtf8.clear();m_selectTitle=false;Invalidate();}
void UIChatRoomSettingsWnd::SetPasswordText(const std::string& text){m_password=ro::chat::CleanInput(text,8,8);m_pendingUtf8.clear();Invalidate();}
void UIChatRoomSettingsWnd::SetPrivate(bool privacy){m_private=privacy;Invalidate();}
void UIChatRoomSettingsWnd::SetInfo(const ro::chat::RoomInfo& info)
{
    m_people=info.maxNumPeople;m_displayPeople=5; // Source prefill displays the last label, independently of the loaded maximum.
    SetTitleText(std::string(info.title.c_str()));SetPasswordText(std::string(info.pass.c_str()));m_private=info.roomType==0;
    m_passwordFocused=false;m_selectTitle=true;g_windowMgr.m_editWindow=this;Invalidate();
}
RECT UIChatRoomSettingsWnd::GetPopupBounds() const
{
    if(m_popup<0)return {};return {m_popup?182:45,64,(m_popup?182:45)+74,m_popup?80:112};
}
bool UIChatRoomSettingsWnd::SubmitInput()
{
    int error=-1;auto& filter=ro::GetMannerFilter();
    if(m_title.empty())error=13;
    else {filter.EnsureLoaded();if(filter.IsBadUtf8Sentence(m_title))error=14;
        else if(g_serviceType==ServiceAmerica && std::any_of(m_title.begin(),m_title.end(),[](unsigned char c){return c>=0x80;}))error=190;
        else if(m_private && m_password.size()<4)error=m_password.empty()?15:16;}
    if(error>=0){g_windowMgr.ShowRoomValidationMessage(error);return false;}
    ro::chat::RoomInfo info{m_title,m_password,m_private?0:1,0,m_people,0xffffffffu};
    const bool sent=g_modeMgr.SendMsg(m_change?49:48,reinterpret_cast<msgparam_t>(&info),0,0)!=0;
    g_windowMgr.DeleteWindow(this);return sent;
}
void UIChatRoomSettingsWnd::Activate(int id)
{
    if(id==118){SubmitInput();return;}
    if(id==119 || id==135){g_windowMgr.DeleteWindow(this);return;}
    if(id==162 || id==163){m_popup=id==162?0:1;m_popupFirst=0;Invalidate();}
}
void UIChatRoomSettingsWnd::CancelPointerCapture()
{
    m_pressed=0;m_popupPressed=-1;m_dragging=false;
    if(g_windowMgr.m_captureWindow==this)g_windowMgr.m_captureWindow=nullptr;Invalidate();
}
void UIChatRoomSettingsWnd::OnLBtnDown(int x,int y)
{
    g_windowMgr.m_editWindow=this;const int localX=x-m_x,localY=y-m_y;
    if(m_popup>=0 && In(GetPopupBounds(),localX,localY)){m_popupPressed=m_popupFirst+(localY-64)/16;return;}
    m_popup=-1;
    for(const auto& button:m_buttons)if(In(button.Bounds(),localX,localY)){m_pressed=button.id;Invalidate();return;}
    if(In({45,24,273,40},localX,localY)){m_passwordFocused=false;m_selectTitle=false;return;}
    if(In({182,70,272,86},localX,localY)){m_passwordFocused=true;return;}
    if(In({44,70,84,82},localX,localY)){SetPrivate(false);return;}
    if(In({84,70,136,82},localX,localY)){SetPrivate(true);return;}
    m_dragging=true;m_startX=x;m_startY=y;m_oldX=m_x;m_oldY=m_y;
}
void UIChatRoomSettingsWnd::OnLBtnUp(int x,int y)
{
    const int pressed=m_pressed,row=m_popupPressed;const int popup=m_popup;
    const auto* button=GetButton(pressed);const bool activate=button && In(button->Bounds(),x-m_x,y-m_y);
    const bool choose=row>=0 && popup>=0 && In(GetPopupBounds(),x-m_x,y-m_y) && row==m_popupFirst+(y-m_y-64)/16;
    CancelPointerCapture();
    if(choose){if(!popup)ChoosePeople(row);else {m_popup=-1;Invalidate();}return;}
    if(activate)Activate(pressed);
}
void UIChatRoomSettingsWnd::OnMouseHover(int x,int y)
{
    int hover=0;for(const auto& button:m_buttons)if(In(button.Bounds(),x-m_x,y-m_y)){hover=button.id;break;}
    if(hover!=m_hover){m_hover=hover;Invalidate();}
}
void UIChatRoomSettingsWnd::OnMouseMove(int x,int y){if(m_dragging)Move(m_oldX+x-m_startX,m_oldY+y-m_startY);OnMouseHover(x,y);}
void UIChatRoomSettingsWnd::OnWheel(int delta){if(m_popup==0){m_popupFirst=std::clamp(m_popupFirst+(delta<0?1:-1),0,3);Invalidate();}}
void UIChatRoomSettingsWnd::OnChar(char c)
{
    const auto byte=static_cast<unsigned char>(c);auto& input=m_passwordFocused?m_password:m_title;
    const auto append=[&](const std::string& value){if(!m_passwordFocused && m_selectTitle){input.clear();m_selectTitle=false;}
        input=ro::chat::CleanInput(input+value,m_passwordFocused?8:36,m_passwordFocused?8:36);};
    if(byte<0x80){m_pendingUtf8.clear();if(c=='\r' || c=='\n')return;
        if(c=='\b'){if(!m_passwordFocused && m_selectTitle){input.clear();m_selectTitle=false;}else ro::chat::EraseLastCharacter(input);}
        else if(byte>=32)append(std::string(1,c));}
    else if(byte>=0xc2 && byte<=0xf4)m_pendingUtf8.assign(1,c);
    else if(byte>=0x80 && byte<=0xbf && !m_pendingUtf8.empty()){
        m_pendingUtf8+=c;const auto first=static_cast<unsigned char>(m_pendingUtf8[0]);const std::size_t count=first<0xe0?2:first<0xf0?3:4;
        if(m_pendingUtf8.size()==count){append(m_pendingUtf8);m_pendingUtf8.clear();}
    }else m_pendingUtf8.clear();Invalidate();
}
void UIChatRoomSettingsWnd::OnKeyDown(int key)
{
    if(key==VK_RETURN){SubmitInput();return;}
    if(key==VK_TAB){m_passwordFocused=!m_passwordFocused;m_pendingUtf8.clear();Invalidate();}
    // Source default cancel117 has no matching room-make/change branch.
}
msgresult_t UIChatRoomSettingsWnd::SendMsg(UIWindow*,int msg,msgparam_t value,msgparam_t,msgparam_t)
{
    if(msg==34 && value){SetInfo(*reinterpret_cast<const ro::chat::RoomInfo*>(value));return 0;}
    if(msg==6){Activate(static_cast<int>(value));return 0;}
    if(msg==39 && m_popup==0)ChoosePeople(static_cast<int>(value));return 0;
}
void UIChatRoomSettingsWnd::OnDraw()
{
    auto dc=AcquireDrawTarget();if(!dc)return;const int saved=SaveDC(dc);IntersectClipRect(dc,m_x,m_y,m_x+m_w,m_y+m_h);
    shopui::FillRectColor(dc,{m_x,m_y,m_x+m_w,m_y+m_h},RGB(232,232,232));
    if(m_background.IsValid())shopui::DrawBitmapPixelsTransparent(dc,m_background,{m_x,m_y+17,m_x+m_background.width,m_y+17+m_background.height});
    else {Text(dc,Global({3,24,44,40},*this),"Title");Text(dc,Global({3,49,44,63},*this),"Limit");Text(dc,Global({143,49,181,63},*this),"Type");}
    if(m_titlebar.IsValid())shopui::DrawBitmapPixelsTransparent(dc,m_titlebar,{m_x,m_y,m_x+m_titlebar.width,m_y+m_titlebar.height});
    Text(dc,Global({9,4,260,19},*this),m_caption,false,0xffffff);Text(dc,Global({8,3,259,18},*this),m_caption);
    for(auto rect:{RECT{45,24,273,40},RECT{182,70,272,86},RECT{48,49,116,63},RECT{187,49,255,63}})shopui::FillRectColor(dc,Global(rect,*this),RGB(242,242,242));
    Text(dc,Global({45,24,273,40},*this),m_title);Text(dc,Global({182,70,272,86},*this),std::string(m_password.size(),'*'));
    Text(dc,Global({48,49,116,63},*this),GetPeopleLabel());Text(dc,Global({187,49,255,63},*this),m_classLabel);
    Text(dc,Global({44,70,84,82},*this),std::string(m_private?"o ":"* ")+m_publicLabel);
    Text(dc,Global({84,70,136,82},*this),std::string(m_private?"* ":"o ")+m_privateLabel);
    for(const auto& button:m_buttons)PaintButton(dc,*this,button,m_pressed==button.id?2:m_hover==button.id?1:0);
    if(m_popup>=0){const auto popup=Global(GetPopupBounds(),*this);shopui::FillRectColor(dc,popup,RGB(255,255,255));
        for(int row=0;row<(m_popup?1:3);++row)Text(dc,{popup.left+2,popup.top+16*row,popup.right,popup.top+16*(row+1)},m_popup?m_classLabel:GetPeopleLabel(m_popupFirst+row));}
    RestoreDC(dc,saved);ReleaseDrawTarget(dc);
}

UIChatRoomMessageBox::UIChatRoomMessageBox(std::string text,int messageId):m_text(std::move(text)),m_messageId(messageId),m_previousEdit(g_windowMgr.m_editWindow)
{
    Create(280,120);const auto client=g_windowMgr.GetChatClientRect();Move((client.right-640)/2+185,(client.bottom-480)/2+180);
    m_ok=Button(118,189,96,"btn_ok.bmp","btn_ok_a.bmp","btn_ok_b.bmp");
}
void UIChatRoomMessageBox::ConfigureConfirmation(UIWindow* owner,std::function<void(bool)> completion)
{
    m_sourceOwner=owner;m_completion=std::move(completion);
    m_ok=Button(121,150,96,"btn_ok.bmp","btn_ok_a.bmp","btn_ok_b.bmp");
    m_cancel=Button(122,215,96,"btn_cancel.bmp","btn_cancel_a.bmp","btn_cancel_b.bmp");
}
void UIChatRoomMessageBox::Dismiss(bool accepted)
{
    auto completion=std::move(m_completion);
    Close();
    if(completion)completion(accepted);
}
void UIChatRoomMessageBox::Close()
{
    const auto previous=m_previousEdit;g_windowMgr.DeleteWindow(this);
    if(g_windowMgr.ContainsWindow(previous))g_windowMgr.m_editWindow=previous;
}
void UIChatRoomMessageBox::CancelPointerCapture(){m_pressed=m_cancelPressed=m_dragging=false;if(g_windowMgr.m_captureWindow==this)g_windowMgr.m_captureWindow=nullptr;Invalidate();}
void UIChatRoomMessageBox::OnLBtnDown(int x,int y)
{
    if(In(m_ok.Bounds(),x-m_x,y-m_y)){m_pressed=true;Invalidate();return;}
    if(HasCancel() && In(m_cancel.Bounds(),x-m_x,y-m_y)){m_cancelPressed=true;Invalidate();return;}
    if(In({10,22,270,87},x-m_x,y-m_y))return;
    m_dragging=true;m_startX=x;m_startY=y;m_oldX=m_x;m_oldY=m_y;
}
void UIChatRoomMessageBox::OnLBtnUp(int x,int y){const bool ok=m_pressed && In(m_ok.Bounds(),x-m_x,y-m_y),cancel=m_cancelPressed && In(m_cancel.Bounds(),x-m_x,y-m_y);CancelPointerCapture();if(ok || cancel)Dismiss(ok);}
void UIChatRoomMessageBox::OnMouseMove(int x,int y){if(m_dragging)Move(m_oldX+x-m_startX,m_oldY+y-m_startY);}
void UIChatRoomMessageBox::OnKeyDown(int key){if(key==VK_RETURN || key==VK_ESCAPE)Dismiss(key==VK_RETURN || !HasCancel());}
void UIChatRoomMessageBox::OnDraw()
{
    auto dc=AcquireDrawTarget();if(!dc)return;shopui::FillRectColor(dc,{m_x,m_y,m_x+m_w,m_y+m_h},RGB(232,232,232));
    shopui::FillRectColor(dc,Global({10,22,270,87},*this),RGB(242,242,242));Text(dc,Global({10,22,270,87},*this),m_text,true);
    PaintButton(dc,*this,m_ok,m_pressed?2:0);if(HasCancel())PaintButton(dc,*this,m_cancel,m_cancelPressed?2:0);ReleaseDrawTarget(dc);
}
