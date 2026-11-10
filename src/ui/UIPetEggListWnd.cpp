#include "UIPetEggListWnd.h"
#include "UIWindowMgr.h"
#include "UIItemWnd.h"
#include "ChatScrollBarUi.h"
#include "core/Globals.h"
#include "core/UiMessageStrings.h"
#include "gamemode/Mode.h"
#include "session/Session.h"
#include <algorithm>
#include <filesystem>
#include <tuple>
#if RO_ENABLE_QT6_UI
#include <QFont>
#include <QImage>
#include <QPainter>
#endif

namespace {
std::string Utf8(const std::string& text)
{
#if RO_PLATFORM_WINDOWS
    const UINT page=g_codePage>0?static_cast<UINT>(g_codePage):CP_ACP;
    if(!text.empty() && page!=CP_UTF8){
        const int n=MultiByteToWideChar(page,0,text.data(),static_cast<int>(text.size()),nullptr,0);
        if(n>0){
            std::wstring wide(n,L'\0');MultiByteToWideChar(page,0,text.data(),static_cast<int>(text.size()),wide.data(),n);
            const int size=WideCharToMultiByte(CP_UTF8,0,wide.data(),n,nullptr,0,nullptr,nullptr);
            std::string out(size,'\0');
            if(size)WideCharToMultiByte(CP_UTF8,0,wide.data(),n,out.data(),size,nullptr,nullptr);
            return out;
        }
    }
#endif
    return text;
}
void Text(HDC dc,RECT rect,const std::string& text,unsigned rgb=0)
{
    if(rect.right<=rect.left || rect.bottom<=rect.top)return;
#if RO_ENABLE_QT6_UI
    QImage image(rect.right-rect.left,rect.bottom-rect.top,QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);QPainter painter(&image);QFont font("Segoe UI");font.setPixelSize(12);
    painter.setFont(font);painter.setPen(QColor::fromRgb(rgb));
    painter.drawText(image.rect(),Qt::AlignLeft|Qt::AlignTop|Qt::TextSingleLine,QString::fromUtf8(text.c_str()));painter.end();
    AlphaBlendArgbToHdc(dc,rect.left,rect.top,image.width(),image.height(),reinterpret_cast<const unsigned int*>(image.constBits()),image.width(),image.height());
#else
    shopui::DrawWindowTextRect(dc,rect,text,RGB((rgb>>16)&255,(rgb>>8)&255,rgb&255),DT_LEFT|DT_TOP|DT_SINGLELINE|DT_NOPREFIX);
#endif
}
}

UIPetEggScrollBar::UIPetEggScrollBar(UIPetEggListWnd* owner):m_owner(owner){Create(13,145);}
void UIPetEggScrollBar::CancelPointerCapture(){m_dragging=false;Invalidate();}
void UIPetEggScrollBar::OnLBtnDown(int x,int y)
{
    const auto bar=m_owner->GetScrollBar();const auto hit=bar.Hit(x-m_x,y-m_y);
    if(hit==ro::chat::ScrollHit::Thumb){m_dragging=true;m_drag.Begin(y);}
    else m_owner->ScrollBy(bar.Step(hit));
}
void UIPetEggScrollBar::OnLBtnUp(int,int){m_dragging=false;}
void UIPetEggScrollBar::OnWheel(int delta){m_owner->OnWheel(delta);}
void UIPetEggScrollBar::OnMouseMove(int,int y)
{
    if(!m_dragging)return;
    const auto bar=m_owner->GetScrollBar();const int before=bar.position;
    m_owner->ScrollBy(m_drag.Request(y,bar));m_drag.Applied(before,m_owner->GetOffset());
}
void UIPetEggScrollBar::OnDraw()
{
    auto dc=AcquireDrawTarget();if(!dc)return;
    const RECT rect{m_x,m_y,m_x+m_w,m_y+m_h};const auto bar=m_owner->GetScrollBar();
    ro::chat::DrawScrollBar(dc,rect,ro::chat::ScrollLayout(rect,bar).thumb,bar.button);ReleaseDrawTarget(dc);
}

UIPetEggListWnd::UIPetEggListWnd()
{
    m_id=90;m_defPushId=118;m_defCancelPushId=117;Create(200,200);
    m_scroll=new UIPetEggScrollBar(this);m_scroll->Move(185,34);AddChild(m_scroll);
    m_buttons[0]=new UIFriendOptionButton(118,"btn_ok.bmp","btn_ok_a.bmp","btn_ok_b.bmp",42,20);
    m_buttons[1]=new UIFriendOptionButton(119,"btn_cancel.bmp","btn_cancel_a.bmp","btn_cancel_b.bmp",42,20);
    for(int i=0;i<2;++i){m_buttons[i]->m_text=i?"Cancel":"OK";m_buttons[i]->Move(108+46*i,176);AddChild(m_buttons[i]);}
    const char* names[]={"basic_interface\\titlebar_left.bmp","basic_interface\\titlebar_right.bmp","basic_interface\\titlebar_mid.bmp",
        "basic_interface\\btnbar_left2.bmp","basic_interface\\btnbar_right2.bmp","basic_interface\\btnbar_mid2.bmp"};
    for(int i=0;i<6;++i){m_frame[i]=LoadChatRoomSkinBitmap(names[i]);m_frameUris[i]=ChatRoomBitmapUri(m_frame[i]);}
    const auto client=g_windowMgr.GetChatClientRect();Move(200+(client.right-640)/2,200*client.bottom/480);
}
void UIPetEggListWnd::Move(int x,int y)
{
    const int dx=x-m_x,dy=y-m_y;UIFrameWnd::Move(x,y);
    for(auto* child:m_children)child->Move(child->m_x+dx,child->m_y+dy);
}
std::string UIPetEggListWnd::GetTitle() const
{
    static ro::UiMessageStrings table;
    static decltype(std::make_tuple(g_serviceType,g_codePage,g_readFolderFirst,std::filesystem::current_path())) previous;
    static bool loaded=false;
    const auto key=std::make_tuple(g_serviceType,g_codePage,g_readFolderFirst,std::filesystem::current_path());
    if(!loaded || key!=previous){table.Load();previous=key;loaded=true;}
    return Utf8(table.Get(599,""));
}
ro::chat::ScrollBar UIPetEggListWnd::GetScrollBar() const
{
    const int count=static_cast<int>((std::min)(m_items.size(),static_cast<std::size_t>(INT_MAX)));
    return {145,13,(std::max)(0,count-4),m_offset,4,count};
}
int UIPetEggListWnd::ScrollBy(int delta)
{
    const int before=m_offset;m_offset=static_cast<int>((std::clamp)(static_cast<long long>(m_offset)+delta,0LL,static_cast<long long>(GetScrollBar().maximum)));
    if(before!=m_offset){Invalidate();m_scroll->Invalidate();}return m_offset-before;
}
int UIPetEggListWnd::GetCurItem(int x,int y) const
{
    if(x<4 || x>m_w-14 || y<34 || y>=162)return -1;
    const int row=m_offset+(y-34)/32;return static_cast<std::size_t>(row)<m_items.size()?row:-1;
}
void UIPetEggListWnd::OnLBtnDown(int x,int y)
{
    if(y-m_y<17){UIFrameWnd::OnLBtnDown(x,y);return;}
    const int next=GetCurItem(x-m_x,y-m_y);if(next!=m_current){m_current=next;Invalidate();}
}
void UIPetEggListWnd::OnLBtnDblClk(int x,int y){UIWindow::OnLBtnDown(x,y);SubmitSelection();}
void UIPetEggListWnd::OnWheel(int delta){if(GetScrollBar().maximum>0)ScrollBy(delta>=0?-3:3);}
void UIPetEggListWnd::CancelPointerCapture()
{
    m_isDragging=0;m_scroll->CancelPointerCapture();for(auto* button:m_buttons)button->CancelPointerCapture();
    for(auto* current=g_windowMgr.m_captureWindow;current;current=current->m_parent)
        if(current==this){g_windowMgr.m_captureWindow=nullptr;break;}
}
void UIPetEggListWnd::SubmitSelection()
{
    if(m_current<0 || static_cast<std::size_t>(m_current)>=m_items.size())return;
    ITEM_INFO item=m_items[static_cast<std::size_t>(m_current)];item.m_isDamaged=1;
    g_session.SetInventoryItem(item);g_session.m_petStatus.eggIndex=static_cast<int>(item.m_itemIndex);
    if(g_windowMgr.m_itemWnd)g_windowMgr.m_itemWnd->Invalidate();
    g_modeMgr.SendMsg(159,item.m_itemIndex,0,0);g_windowMgr.DeleteWindow(this);
}
msgresult_t UIPetEggListWnd::SendMsg(UIWindow*,int message,msgparam_t value,msgparam_t extra,msgparam_t)
{
    if(message==6){if(value==118)SubmitSelection();else if(value==119)g_windowMgr.DeleteWindow(this);}
    else if(message==31){
        ITEM_INFO snapshot{};
        if(const auto* item=g_session.GetInventoryItemByIndex(static_cast<unsigned int>(extra)))snapshot=*item;
        m_items.push_back(std::move(snapshot));ScrollBy(0);Invalidate();
    }else if(message==7)return ScrollBy(static_cast<int>(value));
    else if(message==9)return ScrollBy(-3);
    else if(message==10)return ScrollBy(3);
    else if(message==60){ScrollBy(0);Invalidate();}
    // Selected native SendMsg48B650 ignores75: reuse appends snapshots and
    // preserves current row/offset. Never reinterpret75 as a clear operation.
    return 0;
}
std::vector<PetEggArtwork> UIPetEggListWnd::GetFramePieces() const
{
    std::vector<PetEggArtwork> pieces;
    const auto add=[&](int index,int x,int y){if(m_frame[index].IsValid())pieces.push_back({x,y,index>=3?1:0,m_frame[index],m_frameUris[index]});};
    add(0,0,0);add(1,m_w-m_frame[1].width,0);for(int x=12;x<m_w-12;x+=12)add(2,x,0);
    add(3,0,m_h-m_frame[3].height);add(4,m_w-m_frame[4].width,m_h-m_frame[4].height);
    for(int x=21;x<m_w-21;x+=21)add(5,x,m_h-m_frame[5].height);return pieces;
}
std::vector<PetEggDisplayRow> UIPetEggListWnd::GetVisibleRows()
{
    std::vector<PetEggDisplayRow> rows;
    for(int i=0;i<4 && static_cast<std::size_t>(m_offset+i)<m_items.size();++i){
        const int index=m_offset+i;const auto& item=m_items[static_cast<std::size_t>(index)];
        const auto key=(static_cast<unsigned long long>(item.GetItemId())<<1)|(item.m_isIdentified?1:0);
        auto found=m_icons.find(key);
        if(found==m_icons.end()){
            shopui::BitmapPixels bitmap;shopui::TryLoadItemIconPixels(item,&bitmap);
            auto uri=ChatRoomBitmapUri(bitmap);found=m_icons.emplace(key,std::make_pair(std::move(bitmap),std::move(uri))).first;
        }
        rows.push_back({index,34+32*i,item.m_itemIndex,item.GetItemId(),index==m_current,
            Utf8(item.GetDisplayName()),found->second.second,found->second.first});
    }
    return rows;
}
void UIPetEggListWnd::OnDraw()
{
    auto dc=AcquireDrawTarget();if(!dc)return;const int saved=SaveDC(dc);IntersectClipRect(dc,m_x,m_y,m_x+m_w,m_y+m_h);
    const auto pieces=GetFramePieces();
    const auto paint=[&](int phase){for(const auto& piece:pieces)if(piece.phase==phase)
        shopui::DrawBitmapPixelsTransparent(dc,piece.bitmap,{m_x+piece.x,m_y+piece.y,m_x+piece.x+piece.bitmap.width,m_y+piece.y+piece.bitmap.height});};
    paint(0);const auto title=GetTitle();Text(dc,{m_x+17,m_y+3,m_x+m_w,m_y+17},title,0xffffff);
    Text(dc,{m_x+16,m_y+2,m_x+m_w,m_y+17},title);
    shopui::FillRectColor(dc,{m_x,m_y+17,m_x+m_w,m_y+m_h},RGB(128,128,128));
    shopui::FillRectColor(dc,{m_x+1,m_y+17,m_x+m_w-1,m_y+m_h},RGB(245,242,234));paint(1);
    for(const auto& row:GetVisibleRows()){
        if(row.selected)shopui::FillRectColor(dc,{m_x+5,m_y+row.y-3,m_x+m_w-19,m_y+row.y+27},RGB(215,223,240));
        if(row.bitmap.IsValid())shopui::DrawBitmapPixelsTransparent(dc,row.bitmap,{m_x+8,m_y+row.y,m_x+8+row.bitmap.width,m_y+row.y+row.bitmap.height});
        Text(dc,{m_x+40,m_y+row.y,m_x+m_w-16,m_y+row.y+30},row.text);
    }
    DrawChildrenToHdc(dc);RestoreDC(dc,saved);ReleaseDrawTarget(dc);
}
