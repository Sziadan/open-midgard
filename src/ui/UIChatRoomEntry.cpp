#include "UIChatRoomEntry.h"
#include "UIWindowMgr.h"
#include "UINewChatWnd.h"
#include "ChatText.h"
#include "UiScale.h"
#include "core/UiMessageStrings.h"
#include "gamemode/Mode.h"
#include "gamemode/GameMode.h"
#include "world/GameActor.h"
#if RO_ENABLE_QT6_UI
#include <QBuffer>
#include <QFontMetrics>
#endif

namespace {
void PaintText(HDC dc,RECT r,const std::string& text)
{
#if RO_ENABLE_QT6_UI
    if(r.right<=r.left || r.bottom<=r.top)return;
    QImage image(r.right-r.left,r.bottom-r.top,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);
    QPainter paint(&image);QFont font("Segoe UI");font.setPixelSize(12);paint.setFont(font);paint.setPen(Qt::black);
    paint.drawText(image.rect(),Qt::AlignLeft|Qt::AlignTop|Qt::TextSingleLine,QString::fromUtf8(text.c_str()));paint.end();
    AlphaBlendArgbToHdc(dc,r.left,r.top,image.width(),image.height(),reinterpret_cast<const unsigned int*>(image.constBits()),image.width(),image.height());
#else
    shopui::DrawWindowTextRect(dc,r,text,RGB(0,0,0),DT_LEFT|DT_TOP|DT_SINGLELINE|DT_NOPREFIX);
#endif
}
bool Contains(RECT r,int x,int y)
{
    return x>=r.left && x<r.right && y>=r.top && y<r.bottom;
}
std::string BitmapUri(const shopui::BitmapPixels& bitmap)
{
#if RO_ENABLE_QT6_UI
    if(!bitmap.IsValid())return {};
    QImage image(reinterpret_cast<const uchar*>(bitmap.pixels.data()),bitmap.width,bitmap.height,QImage::Format_ARGB32);
    QByteArray png;QBuffer buffer(&png);buffer.open(QIODevice::WriteOnly);
    if(image.save(&buffer,"PNG"))return "data:image/png;base64,"+png.toBase64().toStdString();
#endif
    return {};
}
shopui::BitmapPixels LoadRoomBitmap(const char* name)
{
    // The common resolver's English/flat fallbacks do not include the
    // original CP949 interface directory used by the archive's raw keys.
    const std::string source="texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\"+std::string(name);
    for(const auto& path:{source,"data\\"+source}){
        if(!g_fileMgr.IsDataExist(path.c_str()))continue;
        auto bitmap=shopui::LoadBitmapPixelsFromGameData(path,true);
        if(bitmap.IsValid())return bitmap;
    }
    return shopui::LoadBitmapPixelsFromGameData(shopui::ResolveUiAssetPath(name),true);
}
}

shopui::BitmapPixels LoadChatRoomSkinBitmap(const char* name){return LoadRoomBitmap(name);}
std::string ChatRoomBitmapUri(const shopui::BitmapPixels& bitmap){return BitmapUri(bitmap);}

UIChatRoomTitle::UIChatRoomTitle(CGameActor* owner):m_owner(owner)
{
    Create(200,34);
    // Receipt-time placement is distinct from the per-frame projected anchor.
    const auto client=g_windowMgr.GetChatClientRect();
    if(owner)Move(UiScaleRawToLogicalCoordinate(owner->m_lastTlvertX)-100,UiScaleRawToLogicalCoordinate(owner->m_lastTlvertY)-110*client.bottom/480);
    LoadArtwork();
}
UIChatRoomTitle::~UIChatRoomTitle()
{
    if(m_owner && m_owner->m_chatTitle==this)m_owner->m_chatTitle=nullptr;
}
void UIChatRoomTitle::SetInfo(const ro::chat::RoomInfo& info)
{
    const bool changedIcon=(info.roomType==0)!=(m_roomInfo.roomType==0);
    m_roomInfo=info;
    if(changedIcon)LoadArtwork();
    Invalidate();
}
std::string UIChatRoomTitle::GetTitle() const
{
    const std::string title(m_roomInfo.title.c_str()); // Source sprintf uses the C-string prefix.
    if(m_roomInfo.roomType==3)return title;
    return title+" ("+std::to_string(m_roomInfo.numPeople)+"/"+std::to_string(m_roomInfo.maxNumPeople)+")";
}
std::string UIChatRoomTitle::GetDisplayTitle() const
{
    const auto full=GetTitle();
#if RO_ENABLE_QT6_UI
    QFont font("Segoe UI");font.setPixelSize(12);const QFontMetrics metrics(font);
    auto text=QString::fromUtf8(full.c_str());
    if(metrics.horizontalAdvance(text)<=m_w-34)return full;
    const QString suffix=QStringLiteral("...");
    while(!text.isEmpty() && metrics.horizontalAdvance(text+suffix)>m_w-34){
        const bool pair=text.back().isLowSurrogate() && text.size()>1 && text.at(text.size()-2).isHighSurrogate();
        text.chop(pair?2:1);
    }
    return (text+suffix).toUtf8().toStdString();
#else
    return full;
#endif
}
void UIChatRoomTitle::LoadArtwork()
{
    static const char* names[]={"sysbox_lu.bmp","sysbox_ru.bmp","sysbox_mu.bmp",
        "sysbox_ld.bmp","sysbox_rd.bmp","sysbox_md.bmp","sysbox_arr_l.bmp"};
    std::vector<shopui::BitmapPixels> pieces;
    for(const auto* name:names)pieces.push_back(LoadRoomBitmap(name));
    pieces.push_back(LoadRoomBitmap(m_roomInfo.roomType?"chat_open.bmp":"chat_close.bmp"));
    m_artwork.Clear();m_artworkUri.clear();
    if(!std::all_of(pieces.begin(),pieces.end(),[](const auto& b){return b.IsValid();}))return;
    m_artwork.width=200;m_artwork.height=34;m_artwork.pixels.assign(200*34,0);
    const auto blit=[&](std::size_t piece,int x,int y){
        const auto& bitmap=pieces[piece];
        for(int yy=0;yy<bitmap.height;++yy)for(int xx=0;xx<bitmap.width;++xx){
            const int dx=x+xx,dy=y+yy;const auto pixel=bitmap.pixels[yy*bitmap.width+xx];
            if(dx>=0 && dx<200 && dy>=0 && dy<34 && (pixel>>24))m_artwork.pixels[dy*200+dx]=pixel;
        }
    };
    blit(0,0,0);blit(1,200-pieces[1].width,0);
    for(int i=0;i<(200-24+11)/12;++i)blit(2,12+12*i,0);
    blit(3,0,34-pieces[3].height-6);blit(4,200-pieces[4].width,34-pieces[4].height-6);
    for(int i=0;i<(200-24+11)/12;++i)blit(5,12+12*i,34-pieces[5].height-6);
    blit(6,100-pieces[6].width/2,34-pieces[6].height);blit(7,3,3);
    m_artworkUri=BitmapUri(m_artwork);
}
void UIChatRoomTitle::OnDraw()
{
    auto dc=AcquireDrawTarget();if(!dc)return;
    if(m_artwork.IsValid())shopui::DrawBitmapPixelsTransparent(dc,m_artwork,{m_x,m_y,m_x+200,m_y+34});
    else shopui::FillRectColor(dc,{m_x,m_y,m_x+200,m_y+28},RGB(245,245,235));
    PaintText(dc,{m_x+31,m_y+8,m_x+197,m_y+23},GetDisplayTitle());
    ReleaseDrawTarget(dc);
}
void UIChatRoomTitle::OnLBtnDblClk(int,int)
{
    if(m_roomInfo.roomType==3)return;
    if(m_roomInfo.roomType==0){
        auto* password=static_cast<UIChatRoomPasswordWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_CHATROOMPASSWORD));
        password->SetRoomId(m_roomInfo.roomId);
    }else g_modeMgr.SendMsg(50,m_roomInfo.roomId,0,0);
}
void UIChatRoomTitle::OnMouseShape(int x,int y)
{
    if(auto* mode=g_modeMgr.GetCurrentGameMode())mode->SetCursorAction(Contains({m_x,m_y,m_x+m_w,m_y+m_h+1},x,y)?2:0);
}

UIChatRoomPasswordWnd::UIChatRoomPasswordWnd()
{
    m_id=UIWindowMgr::WID_CHATROOMPASSWORD;Create(176,46);
    const auto client=g_windowMgr.GetChatClientRect();Move((client.right-640)/2+200,200*client.bottom/480);
    ro::UiMessageStrings table;table.Load();m_prompt=table.Get(15,"Enter password.");
    const char* names[]={"btn_ok.bmp","btn_ok_a.bmp","btn_ok_b.bmp"};
    for(int i=0;i<3;++i){m_okBitmaps[i]=LoadRoomBitmap(names[i]);m_okUris[i]=BitmapUri(m_okBitmaps[i]);}
}
RECT UIChatRoomPasswordWnd::GetOkBounds() const
{
    int width=0,height=0;
    for(const auto& bitmap:m_okBitmaps)if(bitmap.IsValid()){
        width=(std::max)(width,bitmap.width);height=(std::max)(height,bitmap.height);
    }
    return {m_w-55,m_h-32,m_w-55+(width?width:42),m_h-32+(height?height:20)};
}
const std::string& UIChatRoomPasswordWnd::GetOkArtworkUri() const
{
    return m_okUris[m_pressedOk?2:m_hoverOk?1:0];
}
int UIChatRoomPasswordWnd::GetOkArtworkWidth() const { return m_okBitmaps[m_pressedOk?2:m_hoverOk?1:0].width; }
int UIChatRoomPasswordWnd::GetOkArtworkHeight() const { return m_okBitmaps[m_pressedOk?2:m_hoverOk?1:0].height; }
void UIChatRoomPasswordWnd::SetInputText(const std::string& text)
{
    m_pendingUtf8.clear();m_input=ro::chat::CleanInput(text,8,8);Invalidate();
}
bool UIChatRoomPasswordWnd::SubmitInput()
{
    const bool sent=g_modeMgr.SendMsg(50,m_roomId,reinterpret_cast<msgparam_t>(m_input.c_str()),0)!=0;
    // Source closes on dispatch, including a guard rejection or immediate send failure.
    g_windowMgr.DeleteWindow(this);return sent;
}
void UIChatRoomPasswordWnd::OnDraw()
{
    auto dc=AcquireDrawTarget();if(!dc)return;
    shopui::FillRectColor(dc,{m_x,m_y,m_x+176,m_y+46},RGB(232,232,232));
    PaintText(dc,{m_x+15,m_y+6,m_x+161,m_y+21},m_prompt);
    shopui::FillRectColor(dc,{m_x+15,m_y+22,m_x+105,m_y+38},RGB(255,255,255));
    PaintText(dc,{m_x+15,m_y+22,m_x+105,m_y+38},std::string(m_input.size(),'*'));
    auto button=GetOkBounds();button.left+=m_x;button.right+=m_x;button.top+=m_y;button.bottom+=m_y;
    const auto& bitmap=m_okBitmaps[m_pressedOk?2:m_hoverOk?1:0];
    if(bitmap.IsValid())shopui::DrawBitmapPixelsTransparent(dc,bitmap,{button.left,button.top,button.left+bitmap.width,button.top+bitmap.height});
    else {
        const int shade=m_pressedOk?180:210;shopui::FillRectColor(dc,button,RGB(shade,shade,shade));
        PaintText(dc,{button.left+4,button.top+2,button.right,button.bottom},"OK");
    }
    ReleaseDrawTarget(dc);
}
void UIChatRoomPasswordWnd::OnLBtnDown(int x,int y)
{
    g_windowMgr.m_editWindow=this;
    if(Contains(GetOkBounds(),x-m_x,y-m_y)){m_pressedOk=true;Invalidate();return;}
    if(Contains({m_x+15,m_y+22,m_x+105,m_y+38},x,y))return;
    // Original frame dragging starts on any uncovered frame area. The
    // password frame is not registered with the source snap manager.
    m_dragging=true;m_startX=x;m_startY=y;m_oldX=m_x;m_oldY=m_y;
}
void UIChatRoomPasswordWnd::OnMouseMove(int x,int y)
{
    if(m_dragging)Move(m_oldX+x-m_startX,m_oldY+y-m_startY);
    OnMouseHover(x,y);
}
void UIChatRoomPasswordWnd::OnMouseHover(int x,int y)
{
    const bool hover=Contains(GetOkBounds(),x-m_x,y-m_y);
    if(m_hoverOk!=hover){m_hoverOk=hover;Invalidate();}
}
void UIChatRoomPasswordWnd::CancelPointerCapture()
{
    m_dragging=m_pressedOk=m_hoverOk=false;
    if(g_windowMgr.m_captureWindow==this)g_windowMgr.m_captureWindow=nullptr;
    Invalidate();
}
void UIChatRoomPasswordWnd::OnLBtnUp(int x,int y)
{
    const bool submit=m_pressedOk && Contains(GetOkBounds(),x-m_x,y-m_y);
    CancelPointerCapture();
    if(submit)SubmitInput();
}
void UIChatRoomPasswordWnd::OnChar(char c)
{
    const auto byte=static_cast<unsigned char>(c);
    if(byte<0x80){
        m_pendingUtf8.clear();if(c=='\r' || c=='\n')return;
        if(c=='\b')ro::chat::EraseLastCharacter(m_input);
        else if(byte>=32)SetInputText(m_input+c);
    }else if(byte>=0xc2 && byte<=0xf4)m_pendingUtf8.assign(1,c);
    else if(byte>=0x80 && byte<=0xbf && !m_pendingUtf8.empty()){
        m_pendingUtf8+=c;const auto first=static_cast<unsigned char>(m_pendingUtf8[0]);
        const std::size_t count=first<0xe0?2:first<0xf0?3:4;
        if(m_pendingUtf8.size()==count)SetInputText(m_input+m_pendingUtf8);
    }else m_pendingUtf8.clear();
    Invalidate();
}
void UIChatRoomPasswordWnd::OnKeyDown(int key){if(key==VK_RETURN)SubmitInput();}
msgresult_t UIChatRoomPasswordWnd::SendMsg(UIWindow* sender,int msg,msgparam_t value,msgparam_t detail,msgparam_t extra)
{
    if(msg==47){m_roomId=static_cast<u32>(value);return 0;}
    if(msg==6){if(value==118)SubmitInput();return 0;}
    return UIWindow::SendMsg(sender,msg,value,detail,extra);
}
