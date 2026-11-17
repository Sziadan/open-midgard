#include "UITransBalloonText.h"
#include "core/Globals.h"
#include "gamemode/Mode.h"
#include <algorithm>
#include <cstring>
#if RO_ENABLE_QT6_UI
#include <QFont>
#include <QFontMetrics>
#include <QImage>
#include <QPainter>
#endif

namespace {
std::size_t Next(const std::string& text,std::size_t at)
{
    if(g_codePage==65001){auto next=at+1;while(next<text.size() && (static_cast<u8>(text[next])&0xc0)==0x80)++next;return next;}
#if RO_PLATFORM_WINDOWS
    return (std::min)(text.size(),static_cast<std::size_t>(CharNextExA(static_cast<WORD>(g_codePage),text.c_str()+at,0)-text.c_str()));
#else
    return at+1;
#endif
}
int Hex(char c){return c>='0' && c<='9'?c-'0':c>='a' && c<='f'?c-'a'+10:c>='A' && c<='F'?c-'A'+10:-1;}
bool Marker(const std::string& s,std::size_t at){return g_multiLang && at+2<s.size() && s[at]=='|' && Hex(s[at+1])>=0 && Hex(s[at+2])>=0;}
unsigned Page(int charset)
{
    switch(charset){case 128:return 932;case 129:return 949;case 134:return 936;case 136:return 950;case 161:return 1253;case 162:return 1254;
    case 163:return 1258;case 177:return 1255;case 178:return 1256;case 186:return 1257;case 204:return 1251;case 222:return 874;case 238:return 1250;
    default:return g_codePage>0?static_cast<unsigned>(g_codePage):0;}
}
std::string Utf8(const std::string& bytes,unsigned page)
{
#if RO_PLATFORM_WINDOWS
    if(page!=CP_UTF8 && !bytes.empty()){
        const int n=MultiByteToWideChar(page,0,bytes.data(),static_cast<int>(bytes.size()),nullptr,0);
        if(n>0){std::wstring wide(n,L'\0');MultiByteToWideChar(page,0,bytes.data(),static_cast<int>(bytes.size()),wide.data(),n);
            const int size=WideCharToMultiByte(CP_UTF8,0,wide.data(),n,nullptr,0,nullptr,nullptr);std::string out(size,'\0');
            WideCharToMultiByte(CP_UTF8,0,wide.data(),n,out.data(),size,nullptr,nullptr);return out;}
    }
#endif
    return bytes;
}
std::string Display(const std::string& raw)
{
    std::string out;std::size_t start=0;unsigned page=g_codePage>0?static_cast<unsigned>(g_codePage):0;
    for(std::size_t i=0;i<raw.size();){if(Marker(raw,i)){
            out+=Utf8(raw.substr(start,i-start),page);page=Page(Hex(raw[i+1])*16+Hex(raw[i+2]));i+=3;start=i;
        }else i=Next(raw,i);}
    out+=Utf8(raw.substr(start),page);return out;
}
}
UITransBalloonText::UITransBalloonText(CMode* owner, int fontPixels, bool transparentBackground)
    : m_owner(owner), m_fontPixels(fontPixels), m_transparentBackground(transparentBackground),
      m_lineHeight(fontPixels)
{
    m_id = 117;
    Create(8, 8);
}
UITransBalloonText::~UITransBalloonText(){if(m_owner)m_owner->HelpBalloonDeleted(this);}
std::string UITransBalloonText::ReplaceUnderbars(const char* text)
{
    // Source512-byte copy has defined content only within its capacity.
    std::string out=text?std::string(text).substr(0,511):std::string();
    for(std::size_t i=0;i<out.size();i=Next(out,i))if(out[i]=='_')out[i]=' ';return out;
}
void UITransBalloonText::SetText(const char* text,int maximumBytes)
{
    if(!text || !*text || maximumBytes<=0)return;
    const std::string input(text);m_lines.clear();m_displayLines.clear();
    std::size_t charset=input.size();for(std::size_t i=0;i<input.size();++i)if(Marker(input,i)){charset=i;break;}
    std::size_t start=0,cursor=0,space=0,limit=static_cast<std::size_t>(maximumBytes);
    while(cursor<input.size()){
        cursor=Next(input,cursor);while(cursor<input.size() && input[cursor]==' ')space=++cursor;
        if(cursor>=limit){const auto end=space>start?space:cursor;m_lines.push_back(input.substr(start,end-start));start=end;limit=start+maximumBytes;}
    }
    if(start<cursor)m_lines.push_back(input.substr(start,cursor-start));
    if(charset<input.size())for(std::size_t i=1;i<m_lines.size();++i)m_lines[i].insert(0,input.substr(charset,3));
    int width=0;
#if RO_ENABLE_QT6_UI
    QFont font("Segoe UI");font.setPixelSize(m_fontPixels);const QFontMetrics metrics(font);m_lineHeight=metrics.height();
#endif
    for(const auto& line:m_lines){m_displayLines.push_back(Display(line));
#if RO_ENABLE_QT6_UI
        width=(std::max)(width,metrics.horizontalAdvance(QString::fromUtf8(m_displayLines.back().c_str())));
#else
        width=(std::max)(width,static_cast<int>(m_displayLines.back().size())*m_fontPixels/2);
#endif
    }
    if(!m_lines.empty())Resize(width+12,4+static_cast<int>(m_lines.size())*(m_lineHeight+4));Invalidate();
}
void UITransBalloonText::OnDraw()
{
    auto dc=AcquireDrawTarget();if(!dc)return;
#if RO_ENABLE_QT6_UI
    QImage image(m_w,m_h,QImage::Format_ARGB32_Premultiplied);image.fill(QColor(0,0,0,m_transparentBackground ? 0 : 136));
    QPainter painter(&image);QFont font("Segoe UI");font.setPixelSize(m_fontPixels);painter.setFont(font);int y=4;
    for(const auto& line:m_displayLines){painter.setPen(Qt::black);painter.drawText(QRect(7,y+1,m_w-7,m_lineHeight),Qt::AlignLeft|Qt::AlignTop|Qt::TextSingleLine,QString::fromUtf8(line.c_str()));
        painter.setPen(QColor::fromRgb(m_fontColor));painter.drawText(QRect(6,y,m_w-6,m_lineHeight),Qt::AlignLeft|Qt::AlignTop|Qt::TextSingleLine,QString::fromUtf8(line.c_str()));y+=m_lineHeight+4;}
    painter.end();AlphaBlendArgbToHdc(dc,m_x,m_y,m_w,m_h,reinterpret_cast<const unsigned int*>(image.constBits()),m_w,m_h);
#else
    if (!m_transparentBackground) {
        const std::vector<unsigned int> background(static_cast<std::size_t>(m_w)*m_h,0x88000000u);
        AlphaBlendArgbToHdc(dc,m_x,m_y,m_w,m_h,background.data(),m_w,m_h);
    }
    int y=4;
    for(const auto& line:m_displayLines){shopui::DrawWindowTextRect(dc,{m_x+7,m_y+y+1,m_x+m_w,m_y+y+1+m_lineHeight},line,RGB(0,0,0),DT_LEFT|DT_TOP|DT_SINGLELINE|DT_NOPREFIX);
        shopui::DrawWindowTextRect(dc,{m_x+6,m_y+y,m_x+m_w,m_y+y+m_lineHeight},line,RGB((m_fontColor>>16)&255,(m_fontColor>>8)&255,m_fontColor&255),DT_LEFT|DT_TOP|DT_SINGLELINE|DT_NOPREFIX);y+=m_lineHeight+4;}
#endif
    ReleaseDrawTarget(dc);
}
