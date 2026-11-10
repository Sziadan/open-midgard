#include "UITipOfTheDayWnd.h"
#include "ChatScrollBarUi.h"
#include "UIWindowMgr.h"
#include "core/Globals.h"
#include "core/TipOfTheDay.h"
#include "core/UiMessageStrings.h"
#include <algorithm>
#include <climits>
#include <cstdlib>
#include <cstring>
#if RO_ENABLE_QT6_UI
#include <QFont>
#include <QFontMetrics>
#include <QImage>
#include <QPainter>
#endif

namespace {
int Hex(char c){return c>='0' && c<='9'?c-'0':c>='a' && c<='f'?c-'a'+10:c>='A' && c<='F'?c-'A'+10:-1;}
bool ColorMark(const std::string& text,std::size_t at,u32* color=nullptr)
{
    if(at+7>text.size() || text[at]!='^')return false;u32 value=0;
    for(std::size_t i=1;i<7;++i){const int digit=Hex(text[at+i]);if(digit<0)return false;value=value*16+digit;}
    if(color)*color=value;return true;
}
std::size_t Next(const std::string& text,std::size_t at)
{
    if(g_codePage==65001){auto next=at+1;while(next<text.size() && (static_cast<u8>(text[next])&0xc0)==0x80)++next;return next;}
#if RO_PLATFORM_WINDOWS
    return (std::min)(text.size(),static_cast<std::size_t>(CharNextExA(static_cast<WORD>(g_codePage),text.c_str()+at,0)-text.c_str()));
#else
    return at+1;
#endif
}
std::vector<std::string> CutTipLine(const std::string& raw)
{
    // UISys::CutStringWithColor ordinary text path, maxNumCharLine38/readCR0.
    // Input is source bytes; only rendering converts the selected byte segments.
    const std::string text(raw.c_str());std::vector<std::string> rows;
    std::size_t start=0,cursor=0,lastSpace=0,limit=38;
    while(cursor<text.size()){
        if(ColorMark(text,cursor)){cursor+=7;limit+=7;}
        else cursor=Next(text,cursor);
        if(cursor<text.size() && text[cursor]==' '){do{++cursor;lastSpace=cursor;}while(cursor<text.size() && text[cursor]==' ');}
        if(cursor>=limit){const auto end=lastSpace>start?lastSpace:cursor;rows.push_back(text.substr(start,end-start));start=end;limit=start+38;}
    }
    if(start<cursor)rows.push_back(text.substr(start,cursor-start));return rows;
}
std::string Utf8(const std::string& raw)
{
#if RO_PLATFORM_WINDOWS
    if(g_codePage!=65001 && !raw.empty()){
        const UINT page=g_codePage>0?static_cast<UINT>(g_codePage):CP_ACP;
        const int n=MultiByteToWideChar(page,0,raw.data(),static_cast<int>(raw.size()),nullptr,0);
        if(n>0){std::wstring wide(n,L'\0');MultiByteToWideChar(page,0,raw.data(),static_cast<int>(raw.size()),wide.data(),n);
            const int size=WideCharToMultiByte(CP_UTF8,0,wide.data(),n,nullptr,0,nullptr,nullptr);
            std::string out(size,'\0');WideCharToMultiByte(CP_UTF8,0,wide.data(),n,out.data(),size,nullptr,nullptr);return out;}
    }
#endif
    return raw;
}
void DrawRow(HDC dc,RECT rect,const TipTextRow& row)
{
    if(rect.right<=rect.left || rect.bottom<=rect.top)return;
#if RO_ENABLE_QT6_UI
    QImage image(rect.right-rect.left,rect.bottom-rect.top,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);
    QPainter painter(&image);QFont font("Segoe UI");font.setPixelSize(12);painter.setFont(font);const QFontMetrics metrics(font);int x=0;
    for(const auto& span:row){painter.setPen(QColor::fromRgb(span.color));const auto text=QString::fromUtf8(span.text.c_str());
        painter.drawText(QRect(x,0,image.width()-x,image.height()),Qt::AlignLeft|Qt::AlignTop|Qt::TextSingleLine,text);x+=metrics.horizontalAdvance(text);if(x>=image.width())break;}
    painter.end();AlphaBlendArgbToHdc(dc,rect.left,rect.top,image.width(),image.height(),reinterpret_cast<const unsigned int*>(image.constBits()),image.width(),image.height());
#else
    int x=rect.left;
    for(const auto& span:row){shopui::DrawWindowTextRect(dc,{x,rect.top,rect.right,rect.bottom},span.text,
        RGB((span.color>>16)&255,(span.color>>8)&255,span.color&255),DT_LEFT|DT_TOP|DT_SINGLELINE|DT_NOPREFIX);
        SIZE size{};GetTextExtentPoint32A(dc,span.text.c_str(),static_cast<int>(span.text.size()),&size);x+=size.cx;}
#endif
}
}

UITipTextViewer::UITipTextViewer(){Create(260,125);}
void UITipTextViewer::SetLines(const std::vector<std::string>& lines)
{
    m_items.clear();m_rows.clear();m_offset=0;m_scrollPressed=false;u32 color=0;
    for(const auto& line:lines)for(auto& item:CutTipLine(line)){
        TipTextRow row;std::size_t start=0;
        for(std::size_t i=0;i<item.size();){u32 next;
            if(ColorMark(item,i,&next)){if(i>start)row.push_back({Utf8(item.substr(start,i-start)),color});color=next;i+=7;start=i;}
            else i=Next(item,i);}
        if(start<item.size())row.push_back({Utf8(item.substr(start)),color});
        m_items.push_back(std::move(item));m_rows.push_back(std::move(row));
    }
    Invalidate();
}
ro::chat::ScrollBar UITipTextViewer::GetScrollBar() const
{
    ro::chat::ScrollBar bar;bar.axis=m_h;bar.page=m_h/18;bar.total=static_cast<int>((std::min)(m_items.size(),static_cast<std::size_t>(INT_MAX)));
    bar.maximum=(std::max)(0,bar.total-bar.page);bar.position=m_offset;return bar;
}
int UITipTextViewer::Scroll(int delta)
{
    // UIListBox44C650: ADD32, signed lower clamp, unsigned upper comparison.
    // Direct messages can move a short list even while its bar is hidden.
    const int old=m_offset;const u32 bits=static_cast<u32>(old)+static_cast<u32>(delta);
    s32 value;std::memcpy(&value,&bits,sizeof(value));m_offset=(std::max)(0,static_cast<int>(value));
    const u32 available=static_cast<u32>(m_items.size())-static_cast<u32>(m_h/18);
    if(static_cast<u32>(m_offset)>available){s32 upper;std::memcpy(&upper,&available,sizeof(upper));m_offset=upper;}
    Invalidate();return m_offset-old;
}
void UITipTextViewer::CancelPointerCapture(){m_scrollPressed=false;}
void UITipTextViewer::OnLBtnDown(int x,int y)
{
    const auto bar=GetScrollBar();const auto hit=bar.Hit(x-m_x-(m_w-13),y-m_y);
    if(hit==ro::chat::ScrollHit::Thumb){m_scrollPressed=true;m_drag.Begin(y);}
    else Scroll(bar.Step(hit));
}
void UITipTextViewer::OnMouseMove(int,int y){if(m_scrollPressed){const int old=m_offset;Scroll(m_drag.Request(y,GetScrollBar()));m_drag.Applied(old,m_offset);}}
void UITipTextViewer::OnWheel(int delta){if(GetScrollBar().maximum>0)Scroll(delta>=0?-3:3);}
msgresult_t UITipTextViewer::SendMsg(UIWindow*,int msg,msgparam_t value,msgparam_t,msgparam_t)
{
    if(msg==7)return Scroll(static_cast<int>(value));
    if(msg==9)return Scroll(1-m_h/18);if(msg==10)return Scroll(m_h/18-1);
    if(msg==18 || msg==19){if(GetScrollBar().maximum>0)Scroll(msg==18?-1:1);return 0;}return 0;
}
void UITipTextViewer::OnDraw()
{
    auto dc=AcquireDrawTarget();if(!dc)return;const int saved=SaveDC(dc);IntersectClipRect(dc,m_x,m_y,m_x+m_w,m_y+m_h);
    shopui::FillRectColor(dc,{m_x,m_y,m_x+m_w,m_y+m_h},RGB(255,255,255));const auto bar=GetScrollBar();
    for(int i=0;i<bar.page && i+m_offset<static_cast<int>(m_rows.size());++i)DrawRow(dc,{m_x+3,m_y+4+18*i,m_x+m_w-(bar.maximum>0?13:0),m_y+4+18*i+14},m_rows[i+m_offset]);
    if(bar.maximum>0){const RECT rail{m_x+m_w-13,m_y,m_x+m_w,m_y+m_h};ro::chat::DrawScrollBar(dc,rail,ro::chat::ScrollLayout(rail,bar).thumb,13);}
    RestoreDC(dc,saved);ReleaseDrawTarget(dc);
}

UITipOfTheDayWnd::UITipOfTheDayWnd()
{
    m_id=38;m_defPushId=180;m_defCancelPushId=117;Create(280,180);ro::UiMessageStrings table;table.Load();
    const char* bases[]={"btn_back","btn_next","btn_close"};const char* labels[]={"Back","Next","Close"};const int ids[]={180,152,135};
    for(int i=0;i<3;++i){const auto base=std::string(bases[i]);m_buttons[i]=new UIFriendOptionButton(ids[i],(base+".bmp").c_str(),(base+"_a.bmp").c_str(),(base+"_b.bmp").c_str(),42,20);
        m_buttons[i]->m_text=labels[i];m_buttons[i]->Move(144+45*i,156);AddChild(m_buttons[i]);}
    m_viewer=new UITipTextViewer();m_viewer->Move(10,22);AddChild(m_viewer);
    m_startup=new UIFriendOptionCheckBox(Utf8(table.Get(176,"")),g_showTipsAtStartup!=0,INT_MAX);m_startup->Move(10,164);AddChild(m_startup);
    const auto count=g_tipOfTheDay.GetNumTip();if(count){m_index=static_cast<int>(static_cast<std::size_t>(std::rand())%count);ShowCurrentTip();}
}
void UITipOfTheDayWnd::ShowCurrentTip()
{
    const auto& tip=g_tipOfTheDay.GetTip(m_index);
    SendMsg(nullptr,29,reinterpret_cast<msgparam_t>(&tip),0,0);
}
void UITipOfTheDayWnd::Move(int x,int y){const int dx=x-m_x,dy=y-m_y;UIWindow::Move(x,y);for(auto* child:m_children)child->Move(child->m_x+dx,child->m_y+dy);}
void UITipOfTheDayWnd::OnLBtnDown(int x,int y)
{
    // Source455EA0 starts a frame drag on any parent hit. The manager already
    // gives children priority and passes normalized global coordinates.
    if(!m_show)return;m_isDragging=1;m_startGlobalX=x;m_startGlobalY=y;m_orgX=m_x;m_orgY=m_y;
}
void UITipOfTheDayWnd::CancelPointerCapture()
{
    m_isDragging=0;m_viewer->CancelPointerCapture();for(auto* button:m_buttons)button->CancelPointerCapture();
    for(auto* w=g_windowMgr.m_captureWindow;w;w=w->m_parent)if(w==this){g_windowMgr.m_captureWindow=nullptr;break;}
}
msgresult_t UITipOfTheDayWnd::SendMsg(UIWindow*,int msg,msgparam_t value,msgparam_t detail,msgparam_t)
{
    if(msg==0 || msg==1)return SendMsg(nullptr,6,msg==0?m_defPushId:m_defCancelPushId,0,0);
    if(msg==29 && value){
        ro::UiMessageStrings table;table.Load();std::vector<std::string> lines{table.Get(175,"")};
        const auto& tip=*reinterpret_cast<const CTipOfTheDay::Tip*>(value);
        lines.insert(lines.end(),tip.begin(),tip.end());m_viewer->SetLines(lines);return 0;
    }
    if(msg==6){
        if(value==135){g_windowMgr.DeleteWindow(this);return 0;}
        if(value==147){SaveSourceTipOptions(static_cast<int>(detail));return 0;}
        if(value==152 || value==180){const auto count=g_tipOfTheDay.GetNumTip();if(!count)return 0;
            if(value==152)m_index=static_cast<std::size_t>(m_index+1)>=count?0:m_index+1;else m_index=m_index>0?m_index-1:static_cast<int>(count-1);
            ShowCurrentTip();}
    }
    return 0;
}
void UITipOfTheDayWnd::OnKeyDown(int key){if(key==VK_RETURN)SendMsg(nullptr,0,0,0,0);else if(key==VK_ESCAPE)SendMsg(nullptr,1,0,0,0);}
void UITipOfTheDayWnd::OnDraw()
{
    auto dc=AcquireDrawTarget();if(!dc)return;const int saved=SaveDC(dc);IntersectClipRect(dc,m_x,m_y,m_x+m_w,m_y+m_h);
    shopui::FillRectColor(dc,{m_x,m_y,m_x+m_w,m_y+m_h},RGB(232,232,232));
    shopui::FillRectColor(dc,{m_x,m_y,m_x+m_w,m_y+17},RGB(130,140,150));DrawChildrenToHdc(dc);
    RestoreDC(dc,saved);ReleaseDrawTarget(dc);
}
