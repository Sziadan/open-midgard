#include "UINotifyItemObtainWnd.h"
#include "UIWindowMgr.h"
#include "UiScale.h"
#include "qtui/QtUiRuntime.h"
#include <algorithm>
#include <cmath>
#include "main/WinMain.h"
#if RO_ENABLE_QT6_UI
#include <QGuiApplication>
#include <QFontMetrics>
#endif

namespace {
HFONT ObtainFont()
{
    static HFONT font=CreateFontA(-12,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,NONANTIALIASED_QUALITY,DEFAULT_PITCH|FF_DONTCARE,"MS Sans Serif");
    return font;
}
int Measure(const std::string& text)
{
#if RO_ENABLE_QT6_UI
    if(QGuiApplication::instance()) {
        QFont font("MS Sans Serif");font.setPixelSize(12);font.setKerning(false);
        return QFontMetrics(font).horizontalAdvance(QString::fromUtf8(text.data(),int(text.size())));
    }
#endif
    HDC dc=CreateCompatibleDC(nullptr);const auto old=SelectObject(dc,ObtainFont());
    SIZE size{};GetTextExtentPoint32A(dc,text.data(),int(text.size()),&size);
    SelectObject(dc,old);DeleteDC(dc);return size.cx;
}
void PaintRun(HDC dc,const RECT& rect,const std::string& text,COLORREF color)
{
#if RO_ENABLE_QT6_UI
    if(QGuiApplication::instance()) {
        const int width=rect.right-rect.left,height=rect.bottom-rect.top;
        std::vector<u32> pixels(size_t(width)*height,0u);
        QImage image(reinterpret_cast<uchar*>(pixels.data()),width,height,width*4,QImage::Format_ARGB32);
        QPainter painter(&image);QFont font("MS Sans Serif");font.setPixelSize(12);font.setKerning(false);
        font.setStyleStrategy(QFont::NoAntialias);painter.setFont(font);
        painter.setPen(QColor(GetRValue(color),GetGValue(color),GetBValue(color)));
        painter.drawText(QRect(0,0,width,height),Qt::AlignLeft|Qt::AlignTop|Qt::TextSingleLine,QString::fromUtf8(text.c_str()));
        painter.end();AlphaBlendArgbToHdc(dc,rect.left,rect.top,width,height,pixels.data(),width,height);return;
    }
#endif
    SetBkMode(dc,TRANSPARENT);SetTextColor(dc,color);RECT copy=rect;
    DrawTextA(dc,text.data(),int(text.size()),&copy,DT_LEFT|DT_TOP|DT_SINGLELINE|DT_NOPREFIX);
}
size_t NextCharacter(const std::string& text,size_t pos)
{
    ++pos;while(pos<text.size()&&(static_cast<unsigned char>(text[pos])&0xc0)==0x80)++pos;
    return pos;
}
}

UINotifyItemObtainWnd::UINotifyItemObtainWnd()
{
    Create(180,32);
    RECT viewport{0,0,640,480};if(g_hMainWnd)GetClientRect(g_hMainWnd,&viewport);
    const float scale=IsQtUiRuntimeEnabled()?GetConfiguredUiScaleFactor():1.0f;
    const int width=int(std::floor(float(viewport.right-viewport.left)/scale));
    Move((width-640)/2+220,51);
}

void UINotifyItemObtainWnd::SetItem(const ITEM_INFO& item,u32 now)
{
    m_item=item;m_madeTick=now;
    m_sizingText=g_ttemmgr.GetItemObtainSizingText(item);
    Resize((std::max)(34,((Measure(m_sizingText)+54+27)/28)*28),32);
    m_icon.Clear();shopui::TryLoadItemIconPixels(item,&m_icon);
    m_segments=g_ttemmgr.GetItemObtainSegments(item);BuildRuns();SetShow(1);Invalidate();
}

void UINotifyItemObtainWnd::RefreshContent()
{
    const auto segments=g_ttemmgr.GetItemObtainSegments(m_item);
    bool same=segments.size()==m_segments.size();
    if(same)for(size_t i=0;i<segments.size();++i)
        if(segments[i].text!=m_segments[i].text||segments[i].color!=m_segments[i].color){same=false;break;}
    if(!same){m_segments=segments;BuildRuns();Invalidate();}
}

void UINotifyItemObtainWnd::BuildRuns()
{
    m_runs.clear();std::string text;size_t colored=std::string::npos;
    u32 producerColor=0xff000000u;std::vector<size_t> boundaries;
    for(const auto& segment:m_segments) {
        if(segment.color!=0xff000000u){colored=text.size();producerColor=segment.color;}
        text+=segment.text;boundaries.push_back(text.size());
    }
    const int width=m_w-50;
    const bool wrapped=Measure(text)>=width;
    int offset=0,line=1,y=wrapped?4:10;size_t cursor=0;
    for(const auto end:boundaries) {
        const bool producer=cursor==colored;
        const u32 color=producer?producerColor:0xff000000u;
        if(!wrapped) {
            const auto part=text.substr(cursor,end-cursor);
            if(!part.empty())m_runs.push_back({part,41+offset,y,Measure(part),color});
            offset+=Measure(part);cursor=end;continue;
        }
        while(cursor<end) {
            const size_t next=NextCharacter(text,cursor);
            if(line==2&&offset>=width-15&&offset+Measure(text.substr(next))>width) {
                // Original PE 46e9b8..46e9f4: literal "...", a byte-count
                // x gap, and black marker one pixel above producer markers.
                m_runs.push_back({"...",41+offset+int(next-cursor),producer?y:y-1,Measure("..."),color});
                break;
            }
            const auto glyph=text.substr(cursor,next-cursor);const int advance=Measure(glyph);
            m_runs.push_back({glyph,41+offset,y,advance,color});offset+=advance;
            if(offset>width){offset=0;y+=13;++line;}
            cursor=next;
        }
    }
}

void UINotifyItemObtainWnd::ProcessAtTick(u32 now)
{
    if(now>u32(m_madeTick+5000u)){g_windowMgr.DeleteWindow(this);return;}
    RefreshContent();
}
void UINotifyItemObtainWnd::OnProcess(){ProcessAtTick(timeGetTime());}
void UINotifyItemObtainWnd::OnDraw()
{
    HDC dc=AcquireDrawTarget();if(!dc)return;DrawNativeToHdc(dc);ReleaseDrawTarget(dc);
}
void UINotifyItemObtainWnd::DrawNativeToHdc(HDC dc)
{
    if(!dc||!m_show)return;RefreshContent();
    const auto bounds=shopui::MakeRect(m_x,m_y,m_w,m_h);
    shopui::FillRectColor(dc,bounds,RGB(245,242,234));shopui::FrameRectColor(dc,bounds,RGB(166,159,145));
    if(m_icon.IsValid())shopui::DrawBitmapPixelsTransparent(dc,m_icon,shopui::MakeRect(m_x+13,m_y+5,24,24));
    const auto old=SelectObject(dc,ObtainFont());
    for(const auto& run:m_runs) {
        const auto rect=shopui::MakeRect(m_x+run.x,m_y+run.y,(std::max)(1,run.width+2),16);
        auto shadow=rect;++shadow.left;++shadow.right;++shadow.top;++shadow.bottom;
        PaintRun(dc,shadow,run.text,RGB(224,224,224));
        PaintRun(dc,rect,run.text,RGB((run.color>>16)&255,(run.color>>8)&255,run.color&255));
    }
    SelectObject(dc,old);m_isDirty=0;
}
bool UINotifyItemObtainWnd::GetDisplayDataForQt(DisplayData* data) const
{
    if(!data||!m_show)return false;*data=DisplayData{};
    data->itemId=m_item.GetItemId();data->identified=m_item.m_isIdentified!=0;
    data->sizingText=m_sizingText;for(const auto& s:m_segments)data->text+=s.text;
    data->runs=m_runs;return true;
}
