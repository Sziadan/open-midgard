#include "UIGroundSkillTextWnd.h"

#include "UIChatRoomEntry.h"
#include "UIWindowMgr.h"
#include "UINewChatWnd.h"
#include "core/Globals.h"
#include "core/Timer.h"
#include "core/SourceTextCodec.h"
#include "core/UiMessageStrings.h"
#include "gamemode/GameMode.h"
#include "gamemode/Mode.h"
#include "main/WinMain.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#if defined(_WIN32)
#include <imm.h>
#endif

#if RO_ENABLE_QT6_UI
#if !defined(_WIN32)
#include <QClipboard>
#include <QGuiApplication>
#include <QMimeData>
#endif
#include <QFont>
#include <QFontMetrics>
#include <QImage>
#include <QInputMethodEvent>
#include <QPainter>
#include <QTextLayout>
#endif

namespace {
#if defined(_WIN32)
class PromptClipboard {
public:
    explicit PromptClipboard(HWND owner) : m_open(OpenClipboard(owner) != FALSE) {}
    ~PromptClipboard() { if (m_open) CloseClipboard(); }
    PromptClipboard(const PromptClipboard&) = delete;
    PromptClipboard& operator=(const PromptClipboard&) = delete;

    std::optional<std::string> Read() const
    {
        if (!m_open) return std::nullopt;
        const auto handle = GetClipboardData(CF_TEXT);
        if (!handle) return std::nullopt;
        const auto size = GlobalSize(handle);
        const auto* bytes = static_cast<const char*>(GlobalLock(handle));
        if (!bytes) return std::nullopt;
        const auto* end = static_cast<const char*>(std::memchr(bytes, 0, size));
        std::optional<std::string> result;
        if (end) result.emplace(bytes, end);
        GlobalUnlock(handle);
        return result;
    }

    void Write(const std::string& bytes) const
    {
        if (!m_open) return;
        const auto handle = GlobalAlloc(GMEM_MOVEABLE, bytes.size() + 1);
        if (!handle) return;
        if (auto* destination = GlobalLock(handle)) {
            std::memcpy(destination, bytes.c_str(), bytes.size() + 1);
            GlobalUnlock(handle);
            if (EmptyClipboard() && SetClipboardData(CF_TEXT, handle)) return;
        }
        GlobalFree(handle);
    }

private:
    bool m_open;
};
#endif

bool UsesSourceCandidates(int codePage)
{
    return codePage == 932 || codePage == 936 || codePage == 949 || codePage == 950;
}

struct TcKeystroke {
    int slot;
    unsigned bytes;
};

TcKeystroke LookupTcKeystroke(int key)
{
    // Selected client constructor at 0x500530. Entries are raw Big5 byte pairs.
    constexpr std::array<int, 41> keys{
        '1','2','3','4','5','6','7','8','9','0','A','B','C','D','E','F','G','H','I','J',
        'K','L','N','M','O','P','Q','R','S','T','U','V','W','X','Y','Z',0xbd,0xba,0xbc,0xbe,0xbf,
    };
    constexpr std::string_view slotOrder = "11441443331111111132331233111121111133333";
    constexpr std::array<unsigned, 41> glyphs{
        0xa374,0xa378,0xa3be,0xa3bf,0xa3a4,0xa3bd,0xa3bb,0xa3ab,
        0xa3af,0xa3b3,0xa376,0xa3a7,0x3f3f,0x3fa3,0xa2a3,0xa6a3,
        0xa9a3,0xaca3,0xb9a3,0xada3,0xb1a3,0xaaa3,0xbaa3,0xb0a3,
        0xb4a3,0x75a3,0xa1a3,0x7aa3,0xa53f,0xa3a3,0xa379,0x3fa3,
        0xa8a3,0x77a3,0xb7a3,0xb5a3,0xaea3,0xb2a3,0xb600,
        0,      // VK_OEM_PERIOD reads uninitialized stack bytes in the selected binary.
        0x3131, // VK_OEM_2 reads the first two bytes of the adjacent slot string.
    };
    static_assert(keys.size() == slotOrder.size() && keys.size() == glyphs.size());
    const auto entry = std::find(keys.begin(), keys.end(), key);
    if (entry == keys.end()) return {4, 0};
    const auto i = static_cast<std::size_t>(entry - keys.begin());
    return {slotOrder[i] - '1', glyphs[i]};
}

std::string DecodeTcKeystroke(unsigned pair)
{
    if (!pair) return {};
    const char bytes[]{static_cast<char>(pair >> 8), static_cast<char>(pair & 0xff)};
    const std::string_view text(bytes, bytes[1] ? 2 : 1);
    if (const auto decoded = ro::DecodeSourceText(text, 950)) return *decoded;
    std::string display;
    for (const unsigned char byte : text) {
        if (byte < 0x80) display += static_cast<char>(byte);
        else display += "\xef\xbf\xbd";
    }
    return display;
}

RECT ImePopupBounds(const UIEditCtrl& edit, RECT client, int anchor, int width, int height)
{
    width = (std::min)(width, static_cast<int>(client.right - client.left));
    const int x = (std::clamp)(anchor, static_cast<int>(client.left), static_cast<int>(client.right) - width);
    int y = edit.m_y + edit.m_h + 6;
    if (y + height > client.bottom) y = edit.m_y - height - 6;
    y = (std::max)(y, static_cast<int>(client.top));
    return {x,y,x+width,y+height};
}

#if defined(_WIN32)
std::string InputUtf8(std::u16string_view text);

std::optional<UIGroundSkillTextWnd::ImeCandidatePage> ReadImeCandidatePage(HIMC context)
{
    // Read the first list, as CLanguage::GetImeCandidateList does.
    const DWORD size = ImmGetCandidateListW(context, 0, nullptr, 0);
    constexpr auto headerSize = offsetof(CANDIDATELIST, dwOffset);
    if (size < headerSize) return std::nullopt;
    std::vector<DWORD> storage((static_cast<std::size_t>(size) + sizeof(DWORD) - 1) / sizeof(DWORD));
    auto* list = reinterpret_cast<CANDIDATELIST*>(storage.data());
    const DWORD read = ImmGetCandidateListW(context, 0, list, size);
    if (read < headerSize || read > size || list->dwSize < headerSize || list->dwSize > read) return std::nullopt;
    if (list->dwCount > (list->dwSize - headerSize) / sizeof(DWORD) || list->dwPageStart >= list->dwCount
        || !list->dwPageSize) return std::nullopt;

    UIGroundSkillTextWnd::ImeCandidatePage page;
    page.page = list->dwPageStart / list->dwPageSize;
    page.lastPage = (list->dwCount - 1) / list->dwPageSize;
    const DWORD count = (std::min)({list->dwPageSize, list->dwCount - list->dwPageStart, DWORD{9}});
    const auto* bytes = reinterpret_cast<const char*>(storage.data());
    const auto offsetsEnd = headerSize + static_cast<std::size_t>(list->dwCount) * sizeof(DWORD);
    for (DWORD row = 0; row < count; ++row) {
        const DWORD index = list->dwPageStart + row;
        const DWORD offset = storage[headerSize / sizeof(DWORD) + index];
        if (offset < offsetsEnd || offset >= list->dwSize || offset % sizeof(char16_t)) return std::nullopt;
        std::u16string candidate;
        for (std::size_t cursor = offset; cursor + sizeof(char16_t) <= list->dwSize; cursor += sizeof(char16_t)) {
            char16_t unit;
            std::memcpy(&unit, bytes + cursor, sizeof(unit));
            if (!unit) break;
            candidate += unit;
        }
        const auto terminatedSize = (candidate.size() + 1) * sizeof(char16_t);
        if (terminatedSize > list->dwSize - offset) return std::nullopt;
        page.entries.push_back(InputUtf8(candidate));
        if (index == list->dwSelection) page.selection = static_cast<int>(row);
    }
    return page;
}
#endif

bool Contains(RECT r,int x,int y)
{
    return x>=r.left && x<r.right && y>=r.top && y<r.bottom;
}

RECT GlobalBounds(RECT r,const UIWindow& owner)
{
    r.left+=owner.m_x;r.right+=owner.m_x;r.top+=owner.m_y;r.bottom+=owner.m_y;return r;
}

void DrawText(HDC dc,RECT rect,const std::string& value,COLORREF color,bool utf8=false
#if RO_ENABLE_QT6_UI
    , const QList<QTextLayout::FormatRange>& formats = {}
#endif
)
{
    if(rect.right<=rect.left || rect.bottom<=rect.top)return;
#if RO_ENABLE_QT6_UI
    QImage image(rect.right-rect.left,rect.bottom-rect.top,QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);QFont font("Segoe UI");font.setPixelSize(12);painter.setFont(font);
    painter.setPen(QColor::fromRgb(GetRValue(color),GetGValue(color),GetBValue(color)));
    const auto text = utf8 ? QString::fromStdString(value) : QString::fromLocal8Bit(value.c_str());
    if (formats.isEmpty()) {
        painter.drawText(image.rect(),Qt::AlignLeft|Qt::AlignTop|Qt::TextSingleLine,text);
    } else {
        QTextLayout layout(text, font);
        layout.setFormats(formats);
        layout.beginLayout();
        if (auto line = layout.createLine(); line.isValid()) line.setNumColumns(text.size());
        layout.endLayout();
        layout.draw(&painter, {});
    }
    painter.end();
    AlphaBlendArgbToHdc(dc,rect.left,rect.top,image.width(),image.height(),
                        reinterpret_cast<const unsigned int*>(image.constBits()),image.width(),image.height());
#else
    const auto encoded = utf8 ? ro::EncodeSourceText(value, 0) : std::optional<std::string>(value);
    if (encoded) shopui::DrawWindowTextRect(dc,rect,*encoded,color,DT_LEFT|DT_TOP|DT_SINGLELINE|DT_NOPREFIX);
#endif
}

std::size_t PreviousCharacter(std::string_view text, std::size_t offset)
{
    if (!offset) return 0;
    --offset;
    while (offset && (static_cast<unsigned char>(text[offset]) & 0xc0) == 0x80) --offset;
    return offset;
}

std::size_t NextCharacter(std::string_view text, std::size_t offset)
{
    if (offset == text.size()) return offset;
    ++offset;
    while (offset < text.size() && (static_cast<unsigned char>(text[offset]) & 0xc0) == 0x80) ++offset;
    return offset;
}

std::string InputUtf8(std::u16string_view text)
{
    std::string utf8;
    for (std::size_t i = 0; i < text.size(); ++i) {
        char32_t code = text[i];
        if (code >= 0xd800 && code <= 0xdbff) {
            if (i + 1 == text.size() || text[i + 1] < 0xdc00 || text[i + 1] > 0xdfff) continue;
            code = 0x10000 + ((code - 0xd800) << 10) + (text[++i] - 0xdc00);
        }
        if (code < 0x20 || (code >= 0xdc00 && code <= 0xdfff)) continue;
        const int count = code < 0x80 ? 1 : code < 0x800 ? 2 : code < 0x10000 ? 3 : 4;
        if (count == 1) {
            utf8 += static_cast<char>(code);
        } else {
            constexpr unsigned prefixes[]{0, 0, 0xc0, 0xe0, 0xf0};
            utf8 += static_cast<char>(prefixes[count] | (code >> (6 * (count - 1))));
            for (int shift = 6 * (count - 2); shift >= 0; shift -= 6)
                utf8 += static_cast<char>(0x80 | ((code >> shift) & 0x3f));
        }
    }
    return utf8;
}

int EditTextWidth(HDC dc, std::string_view text)
{
#if RO_ENABLE_QT6_UI
    QFont font("Segoe UI");
    font.setPixelSize(12);
    return QFontMetrics(font).horizontalAdvance(QString::fromUtf8(text.data(), text.size()));
#else
    const auto encoded = ro::EncodeSourceText(text, 0);
    SIZE size{};
    if (encoded) GetTextExtentPoint32A(dc, encoded->data(), static_cast<int>(encoded->size()), &size);
    return size.cx;
#endif
}

RoomSettingsButton MakeButton(int id,int x,int y,const char* normal,const char* hover,const char* pressed)
{
    RoomSettingsButton button;button.id=id;button.x=x;button.y=y;
    const char* names[]={normal,hover,pressed};
    for(int i=0;i<3;++i){button.bitmaps[i]=LoadChatRoomSkinBitmap(names[i]);button.uris[i]=ChatRoomBitmapUri(button.bitmaps[i]);}
    return button;
}

void PaintButton(HDC dc,const UIWindow& owner,const RoomSettingsButton& button,int state)
{
    const RECT bounds=GlobalBounds(button.Bounds(),owner);const auto& bitmap=button.bitmaps[state];
    if(bitmap.IsValid()){
        shopui::DrawBitmapPixelsTransparent(dc,bitmap,{bounds.left,bounds.top,bounds.left+bitmap.width,bounds.top+bitmap.height});
    }else{
        const int shade=state==2?180:state==1?225:210;
        shopui::FillRectColor(dc,bounds,RGB(shade,shade,shade));
        DrawText(dc,bounds,button.id==118?std::string("OK"):std::string("Cancel"),RGB(0,0,0));
    }
}
}

class GroundSkillTextEditCtrl : public UIEditCtrl {
public:
    explicit GroundSkillTextEditCtrl(UIGroundSkillTextWnd* owner):m_owner(owner){}
    void OnDraw() override
    {
        if (!m_show) return;
        auto dc = AcquireDrawTarget();
        if (!dc) return;
        m_owner->SyncSelection();
        const bool focused = g_windowMgr.GetFocusedGroundSkillText() == m_owner;
        const auto display = MakeDisplay(dc, focused);
        const int originX = m_x+3;
        const int saved = SaveDC(dc);
        IntersectClipRect(dc,m_x,m_y,m_x+m_w,m_y+m_h);
        shopui::FillRectColor(dc,{m_x,m_y,m_x+m_w,m_y+m_h},RGB(m_r,m_g,m_b));
        const bool blink = ((timeGetTime() / 500) & 1) == 0;
        const bool showSelection = display.selected && (!display.composing || display.native);
        if (focused && (showSelection || (display.native && blink))) {
            const auto rgb = m_owner->GetColorChip().IsValid() ? m_owner->GetColorChipRgb(6,2) : 0xb4cde6u;
            const int left = Pixel(dc, display.text, display.highlightStart);
            int right = Pixel(dc, display.text, display.highlightEnd);
            if (display.native && !display.selected) right = (std::max)(right, left + 2);
            right = (std::min)(right, m_x + m_w);
            if (right > left)
                shopui::FillRectColor(dc,{left,m_y,right,m_y+m_h},
                    RGB((rgb>>16)&255,(rgb>>8)&255,rgb&255));
        }
        const auto textColor = RGB(m_textR,m_textG,m_textB);
        const RECT textRect{originX,m_y+m_yOffset,m_x+m_w,m_y+m_h};
#if RO_ENABLE_QT6_UI
        QList<QTextLayout::FormatRange> formats;
        if (display.composing && !display.native && !m_compositionFormats.isEmpty()) {
            const auto before = QString::fromUtf8(display.text.data(), display.anchor).size();
            const auto hidden = QString::fromUtf8(display.text.data(), m_scrollStart).size();
            for (auto range : m_compositionFormats) {
                const auto start = before - hidden + range.start;
                const auto end = start + range.length;
                if (end <= 0) continue;
                range.start = static_cast<int>((std::max)(qsizetype{0}, start));
                range.length = static_cast<int>(end - range.start);
                formats.append(range);
            }
        }
        DrawText(dc, textRect, display.text.substr(m_scrollStart), textColor, true, formats);
#else
        DrawText(dc, textRect, display.text.substr(m_scrollStart), textColor, true);
#endif
        if (focused && !display.native && blink
            && (display.composing ? m_owner->m_preeditCursorVisible : !display.selected))
            DrawText(dc,{Pixel(dc,display.text,display.cursor)-2,m_y+m_yOffset,m_x+m_w,m_y+m_h},"|",textColor);
        RestoreDC(dc,saved);
        ReleaseDrawTarget(dc);
    }
    void OnLBtnDown(int x,int y) override
    {
        UIEditCtrl::OnLBtnDown(x,y);
        if(m_owner)m_owner->FocusEdit();
    }
    int CandidateX(HDC dc)
    {
        m_owner->SyncSelection();
        const auto display = MakeDisplay(dc, true);
        return Pixel(dc, display.text, display.native ? display.highlightStart : (std::min)(display.anchor, display.cursor)) - 3;
    }
    int KeystrokeX(HDC dc)
    {
        m_owner->SyncSelection();
        const auto display = MakeDisplay(dc, true);
        return Pixel(dc, display.text, display.cursor) - 3;
    }
#if RO_ENABLE_QT6_UI
    QRect CursorRectangle()
    {
        const auto display = MakeDisplay(nullptr, true);
        return {Pixel(nullptr, display.text, display.cursor), m_y + m_yOffset, 1, m_h - m_yOffset};
    }
    QList<QTextLayout::FormatRange> m_compositionFormats;
#endif
private:
    struct Display {
        std::string text;
        std::size_t cursor;
        std::size_t anchor;
        bool composing;
        bool native;
        bool selected;
        std::size_t highlightStart;
        std::size_t highlightEnd;
    };

    Display MakeDisplay(HDC dc, bool focused)
    {
        const auto cursor = static_cast<std::size_t>(m_selectionCursor);
        const auto anchor = static_cast<std::size_t>(m_selectionOrigin);
        const bool composing = focused && !m_owner->m_preedit.empty();
        const bool native = composing && m_owner->m_nativePreedit;
        Display display{m_text, cursor, anchor, composing, native, cursor != anchor,
            (std::min)(anchor, cursor), (std::max)(anchor, cursor)};
        if (native) {
            display.text.insert(cursor, m_owner->m_preedit);
            display.cursor = cursor + m_owner->m_preedit.size();
            display.highlightEnd += m_owner->m_preedit.size();
        } else if (composing) {
            display.anchor = display.highlightStart;
            display.text.replace(display.anchor, display.highlightEnd - display.anchor, m_owner->m_preedit);
            display.cursor = display.anchor + m_owner->m_preeditCursor;
        }
        const std::string_view text(display.text);
        const auto scrollCursor = native ? cursor : display.cursor;
        if (!focused || (std::min)(display.anchor, display.cursor) == 0) m_scrollStart = 0;
        m_scrollStart = (std::min)(m_scrollStart, scrollCursor);
        while (m_scrollStart < text.size() && m_scrollStart
            && (static_cast<unsigned char>(text[m_scrollStart]) & 0xc0) == 0x80)
            --m_scrollStart;
        while (focused && m_scrollStart < scrollCursor
            && EditTextWidth(dc, text.substr(m_scrollStart, scrollCursor - m_scrollStart)) > m_w - 12)
            m_scrollStart = NextCharacter(text, m_scrollStart);
        return display;
    }

    int Pixel(HDC dc, std::string_view text, std::size_t offset) const
    {
        return m_x + 3 + EditTextWidth(dc, text.substr(m_scrollStart,
            offset > m_scrollStart ? offset - m_scrollStart : 0));
    }

    UIGroundSkillTextWnd* m_owner;
    std::size_t m_scrollStart = 0;
};

UIGroundSkillTextWnd::UIGroundSkillTextWnd(int windowId)
{
    m_id=windowId;Create(280,120);m_trans=m_transTarget=255;m_transTime=timeGetTime();
    const RECT client=g_windowMgr.GetChatClientRect();
    Move((client.right-640)/2+185,300*client.bottom/480-60);

    auto* edit=new GroundSkillTextEditCtrl(this);
    edit->Create(240,16);edit->Move(m_x+22,m_y+50);edit->m_maxchar=78;
    edit->SetFrameColor(242,242,242);m_editCtrl=edit;AddChild(edit);

    m_ok=MakeButton(118,189,96,"btn_ok.bmp","btn_ok_a.bmp","btn_ok_b.bmp");
    m_cancel=MakeButton(119,234,96,"btn_cancel.bmp","btn_cancel_a.bmp","btn_cancel_b.bmp");
    const char* frameNames[]={"basic_interface\\titlebar_left.bmp","basic_interface\\titlebar_right.bmp",
        "basic_interface\\titlebar_mid.bmp","basic_interface\\btnbar_left2.bmp",
        "basic_interface\\btnbar_right2.bmp","basic_interface\\btnbar_mid2.bmp"};
    for(std::size_t i=0;i<m_frame.size();++i){m_frame[i]=LoadChatRoomSkinBitmap(frameNames[i]);m_frameUris[i]=ChatRoomBitmapUri(m_frame[i]);}
    m_colorChip=LoadChatRoomSkinBitmap("colorchip.bmp",false);
    ro::UiMessageStrings messages;messages.Load();
    m_caption=messages.Get(435,"Talkbox");m_prompt=messages.Get(436,"Enter text");
    FocusEdit();
}

u32 UIGroundSkillTextWnd::GetColorChipRgb(int x,int y) const
{
    // Selected CBitmapRes::GetColor42C5D0 uses top-down x+y*width and red
    // for an out-of-range pixel; CSkinMgr43D3E0 ignores the alpha byte.
    if(!m_colorChip.IsValid() || x<0 || y<0 || x>=m_colorChip.width || y>=m_colorChip.height)return 0x00ff0000u;
    return m_colorChip.pixels[static_cast<std::size_t>(y)*m_colorChip.width+x]&0x00ffffffu;
}

const RoomSettingsButton* UIGroundSkillTextWnd::GetButton(int id) const
{
    if(id==118)return &m_ok;if(id==119)return &m_cancel;return nullptr;
}

int UIGroundSkillTextWnd::GetButtonState(int id) const
{
    if(id!=118 && id!=119)return 0;
    if(m_pressed==id)return 2;
    return m_hover==id?1:0;
}

void UIGroundSkillTextWnd::FocusEdit()
{
    if(m_show && !g_windowMgr.m_modalWindow){
        const bool wasFocused = g_windowMgr.GetFocusedGroundSkillText() == this;
        g_windowMgr.m_editWindow=this;
        if (m_editCtrl) {
            SyncSelection();
            m_editCtrl->m_hasFocus=true;
            if (!wasFocused) {
                CancelComposition();
                SetImeCandidates({});
                ClearImeKeystrokes();
                m_editCtrl->m_selectionOrigin=0;
                m_editCtrl->m_selectionCursor=static_cast<int>(m_editCtrl->m_text.size());
                m_editCtrl->Invalidate();
            }
        }
        if(g_windowMgr.m_chatWnd)g_windowMgr.m_chatWnd->ClearInputFocus();
    }
}

void UIGroundSkillTextWnd::Move(int x,int y)
{
    const int dx=x-m_x,dy=y-m_y;UIWindow::Move(x,y);
    if(m_editCtrl)m_editCtrl->MoveDelta(dx,dy);
}

void UIGroundSkillTextWnd::OnDraw()
{
    auto dc=AcquireDrawTarget();if(!dc)return;
    const int saved=SaveDC(dc);IntersectClipRect(dc,m_x,m_y,m_x+m_w,m_y+m_h);
    const auto color=[&](int x,int y){const u32 value=GetColorChipRgb(x,y);return RGB((value>>16)&255,(value>>8)&255,value&255);};
    const COLORREF body=m_colorChip.IsValid()?color(2,2):RGB(232,232,232);
    const COLORREF border=m_colorChip.IsValid()?color(20,6):RGB(90,90,90);
    shopui::FillRectColor(dc,{m_x,m_y,m_x+m_w,m_y+m_h},body);
    const auto& titleLeft=m_frame[0];const auto& titleRight=m_frame[1];const auto& titleMid=m_frame[2];
    if(titleMid.IsValid())for(int x=12;x<m_w-12;x+=12)
        shopui::DrawBitmapPixelsTransparent(dc,titleMid,{m_x+x,m_y,m_x+x+titleMid.width,m_y+titleMid.height});
    if(titleLeft.IsValid())shopui::DrawBitmapPixelsTransparent(dc,titleLeft,{m_x,m_y,m_x+titleLeft.width,m_y+titleLeft.height});
    if(titleRight.IsValid())shopui::DrawBitmapPixelsTransparent(dc,titleRight,{m_x+m_w-titleRight.width,m_y,m_x+m_w,m_y+titleRight.height});
    const auto& footerLeft=m_frame[3];const auto& footerRight=m_frame[4];const auto& footerMid=m_frame[5];
    if(footerMid.IsValid())for(int x=21;x<m_w-21;x+=21)
        shopui::DrawBitmapPixelsTransparent(dc,footerMid,{m_x+x,m_y+m_h-footerMid.height,m_x+x+footerMid.width,m_y+m_h});
    if(footerLeft.IsValid())shopui::DrawBitmapPixelsTransparent(dc,footerLeft,{m_x,m_y+m_h-footerLeft.height,m_x+footerLeft.width,m_y+m_h});
    if(footerRight.IsValid())shopui::DrawBitmapPixelsTransparent(dc,footerRight,{m_x+m_w-footerRight.width,m_y+m_h-footerRight.height,m_x+m_w,m_y+m_h});
    shopui::FillRectColor(dc,{m_x,m_y+17,m_x+1,m_y+m_h-21},border);
    shopui::FillRectColor(dc,{m_x+m_w-1,m_y+17,m_x+m_w,m_y+m_h-21},border);
    if(m_id==84){
        DrawText(dc,{m_x+17,m_y+3,m_x+m_w-8,m_y+17},m_caption,RGB(255,255,255));
        DrawText(dc,{m_x+16,m_y+2,m_x+m_w-8,m_y+16},m_caption,RGB(0,0,0));
    }
    DrawText(dc,{m_x+17,m_y+23,m_x+m_w-8,m_y+39},m_prompt,RGB(255,255,255));
    DrawText(dc,{m_x+16,m_y+22,m_x+m_w-8,m_y+38},m_prompt,RGB(0,0,0));
    PaintButton(dc,*this,m_ok,GetButtonState(118));
    PaintButton(dc,*this,m_cancel,GetButtonState(119));
    DrawChildrenToHdc(dc);
    RestoreDC(dc,saved);ReleaseDrawTarget(dc);
}

void UIGroundSkillTextWnd::CancelPointerCapture()
{
    if(m_dragging && g_windowMgr.m_captureWindow==this){m_transTarget=255;m_transTime=timeGetTime();}
    m_pressed=0;m_dragging=false;
    if(g_windowMgr.m_captureWindow==this){g_windowMgr.m_captureWindow=nullptr;if(g_hMainWnd && GetCapture()==g_hMainWnd)ReleaseCapture();}
    Invalidate();
}

void UIGroundSkillTextWnd::OnLBtnDown(int x,int y)
{
    FocusEdit();m_pressed=0;m_dragging=false;const int localX=x-m_x,localY=y-m_y;
    if(Contains(m_ok.Bounds(),localX,localY))m_pressed=118;
    else if(Contains(m_cancel.Bounds(),localX,localY))m_pressed=119;
    else if(m_editCtrl && Contains({m_editCtrl->m_x,m_editCtrl->m_y,m_editCtrl->m_x+m_editCtrl->m_w,m_editCtrl->m_y+m_editCtrl->m_h},x,y)){
        m_editCtrl->OnLBtnDown(x,y);
    }else{
        m_dragging=true;m_dragStartX=x;m_dragStartY=y;m_dragOriginX=m_x;m_dragOriginY=m_y;
        m_transTarget=128;m_transTime=timeGetTime();
    }
    if(m_pressed || m_dragging){g_windowMgr.m_captureWindow=this;if(g_hMainWnd)SetCapture(g_hMainWnd);}
    Invalidate();
}

void UIGroundSkillTextWnd::OnLBtnUp(int x,int y)
{
    const int pressed=m_pressed;CancelPointerCapture();
    if(!pressed)return;
    const RoomSettingsButton* button=GetButton(pressed);
    if(button && Contains(button->Bounds(),x-m_x,y-m_y))ActivateButton(pressed);
}

void UIGroundSkillTextWnd::OnMouseMove(int x,int y)
{
    if(m_dragging && g_windowMgr.m_captureWindow==this)
        Move(m_dragOriginX+x-m_dragStartX,m_dragOriginY+y-m_dragStartY);
    OnMouseHover(x,y);
}

void UIGroundSkillTextWnd::OnMouseHover(int x,int y)
{
    const int localX=x-m_x,localY=y-m_y;
    const int hover=Contains(m_ok.Bounds(),localX,localY)?118:Contains(m_cancel.Bounds(),localX,localY)?119:0;
    if(hover!=m_hover){m_hover=hover;Invalidate();}
}

void UIGroundSkillTextWnd::OnChar(char c)
{
    OnUtf16Character(static_cast<unsigned char>(c));
}

void UIGroundSkillTextWnd::OnUtf16Character(char16_t codeUnit)
{
    if (codeUnit == u'\b') {
        OnKeyDown(VK_BACK);
        return;
    }
    if (codeUnit >= 0xd800 && codeUnit <= 0xdbff) {
        m_highSurrogate = codeUnit;
        return;
    }
    const std::array<char16_t,2> units{m_highSurrogate, codeUnit};
    const bool pair = m_highSurrogate && codeUnit >= 0xdc00 && codeUnit <= 0xdfff;
    CommitText(pair ? std::u16string_view(units.data(), 2) : std::u16string_view(&codeUnit, 1));
}

void UIGroundSkillTextWnd::SyncSelection()
{
    if (m_editCtrl->m_text != m_selectionText) {
        CancelComposition();
        m_selectionText = m_editCtrl->m_text;
        m_editCtrl->m_selectionOrigin = m_editCtrl->m_selectionCursor = static_cast<int>(m_selectionText.size());
    }
}

void UIGroundSkillTextWnd::UpdateText(std::string text, std::size_t cursor)
{
    CancelComposition();
    m_editCtrl->m_text = std::move(text);
    m_selectionText = m_editCtrl->m_text;
    m_editCtrl->m_selectionOrigin = m_editCtrl->m_selectionCursor = static_cast<int>(cursor);
    m_editCtrl->Invalidate();
}

void UIGroundSkillTextWnd::CommitText(std::u16string_view text)
{
    m_highSurrogate = 0;
    if (!m_editCtrl) return;
    const auto insertion = InputUtf8(text);
    if (insertion.empty()) return;
    CancelComposition();
    ReplaceSelection(insertion);
}

void UIGroundSkillTextWnd::SetNativeComposition(std::u16string_view text)
{
    SetComposition(text, -1, true, true);
}

void UIGroundSkillTextWnd::SetComposition(std::u16string_view text, int cursor, bool visible, bool native)
{
    if (!m_editCtrl) return;
    SyncSelection();
    CancelComposition();
    m_highSurrogate = 0;
    m_preedit = InputUtf8(text);
    m_nativePreedit = native;
    auto offset = cursor < 0 ? text.size() : (std::min)(static_cast<std::size_t>(cursor), text.size());
    if (offset < text.size() && offset && text[offset] >= 0xdc00 && text[offset] <= 0xdfff
        && text[offset - 1] >= 0xd800 && text[offset - 1] <= 0xdbff)
        --offset;
    m_preeditCursor = offset == text.size() ? m_preedit.size() : InputUtf8(text.substr(0, offset)).size();
    m_preeditCursorVisible = visible;
    m_editCtrl->Invalidate();
}

void UIGroundSkillTextWnd::CancelComposition()
{
    if (!m_editCtrl || m_preedit.empty()) return;
    m_preedit.clear();
    m_nativePreedit = false;
    m_preeditCursor = 0;
#if RO_ENABLE_QT6_UI
    static_cast<GroundSkillTextEditCtrl*>(m_editCtrl)->m_compositionFormats.clear();
#endif
    m_editCtrl->Invalidate();
}

void UIGroundSkillTextWnd::SetImeCandidates(ImeCandidatePage page)
{
#if defined(_WIN32)
    if (page.entries.empty()) m_systemCandidates = false;
#endif
    if (page.entries.size() > 9) page.entries.resize(9);
    if (m_candidates.entries.empty() && page.entries.empty()) return;
    m_candidates = std::move(page);
    Invalidate();
}

void UIGroundSkillTextWnd::RecordImeKeystroke(int key)
{
    if (ro::SelectedSourceTextCodePage() != 950) return;
    if (key == VK_BACK) {
        if (m_imeKeystrokes.empty()) return;
        m_imeKeystrokes.pop_back();
    } else {
        const int slot = LookupTcKeystroke(key).slot;
        const auto previous = std::find_if(m_imeKeystrokes.begin(), m_imeKeystrokes.end(),
            [slot](int stroke) { return LookupTcKeystroke(stroke).slot == slot; });
        if (previous != m_imeKeystrokes.end()) m_imeKeystrokes.erase(previous);
        m_imeKeystrokes.push_back(key);
    }
    Invalidate();
}

void UIGroundSkillTextWnd::RetainLastImeKeystroke()
{
    if (m_imeKeystrokes.empty()) return;
    const int last = m_imeKeystrokes.back();
    m_imeKeystrokes.clear();
    if (LookupTcKeystroke(last).slot < 4) m_imeKeystrokes.push_back(last);
    Invalidate();
}

void UIGroundSkillTextWnd::ClearImeKeystrokes()
{
    if (m_imeKeystrokes.empty()) return;
    m_imeKeystrokes.clear();
    Invalidate();
}

std::optional<std::array<std::string, 4>> UIGroundSkillTextWnd::ImeKeystrokeCells() const
{
    if (ro::SelectedSourceTextCodePage() != 950 || m_imeKeystrokes.empty()
        || (m_imeKeystrokes.size() == 1 && LookupTcKeystroke(m_imeKeystrokes.front()).slot == 4))
        return std::nullopt;
    std::array<std::string, 4> cells{"??", "??", {}, {}};
    for (const auto key : m_imeKeystrokes) {
        const auto stroke = LookupTcKeystroke(key);
        if (stroke.slot < 4) cells[stroke.slot] = DecodeTcKeystroke(stroke.bytes);
    }
    return cells;
}

void UIGroundSkillTextWnd::DrawImeKeystrokes(HDC dc, RECT client)
{
    if (!m_editCtrl || client.right <= client.left || client.bottom <= client.top) return;
    const auto cells = ImeKeystrokeCells();
    if (!cells) return;
    const auto anchor = static_cast<GroundSkillTextEditCtrl*>(m_editCtrl)->KeystrokeX(dc);
    const auto bounds = ImePopupBounds(*m_editCtrl, client, anchor, 60, 18);
    const int saved = SaveDC(dc);
    if (!saved) return;
    IntersectClipRect(dc, client.left, client.top, client.right, client.bottom);
    IntersectClipRect(dc, bounds.left, bounds.top, bounds.right, bounds.bottom);
    const auto chip = [&](int x, int y, COLORREF fallback) {
        if (!m_colorChip.IsValid()) return fallback;
        const auto rgb = GetColorChipRgb(x, y);
        return RGB((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255);
    };
    shopui::FillRectColor(dc, bounds, chip(22, 2, RGB(90,90,90)));
    if (bounds.right - bounds.left > 2)
        shopui::FillRectColor(dc, {bounds.left+1,bounds.top+1,bounds.right-1,bounds.bottom-1},
            chip(2, 2, RGB(232,232,232)));
    for (std::size_t i = 0; i < cells->size(); ++i) {
        const int x = bounds.left + 2 + static_cast<int>(i) * 14;
        DrawText(dc, {x,bounds.top+2,bounds.right,bounds.bottom}, (*cells)[i], RGB(0,0,0), true);
    }
    RestoreDC(dc, saved);
}

#if defined(_WIN32)
bool UIGroundSkillTextWnd::HandleNativeImeCandidates(HWND hwnd, WPARAM event, LPARAM lists)
{
    if (lists != 1 || !UsesSourceCandidates(ro::SelectedSourceTextCodePage())) return false;
    if (event == IMN_CLOSECANDIDATE) {
        const bool owned = !m_candidates.entries.empty();
        SetImeCandidates({});
        return owned;
    }
    if (event != IMN_OPENCANDIDATE && event != IMN_CHANGECANDIDATE) return false;
    if (event == IMN_OPENCANDIDATE) m_systemCandidates = false;
    if (m_systemCandidates) return false;
    if (const HIMC context = ImmGetContext(hwnd)) {
        auto page = ReadImeCandidatePage(context);
        ImmReleaseContext(hwnd, context);
        if (page) {
            SetImeCandidates(std::move(*page));
            return true;
        }
    }
    if (!m_candidates.entries.empty()) return true;
    m_systemCandidates = true;
    return false;
}
#endif

void UIGroundSkillTextWnd::DrawImeCandidates(HDC dc, RECT client)
{
    const int codePage = ro::SelectedSourceTextCodePage();
    if (!m_editCtrl || m_candidates.entries.empty() || !UsesSourceCandidates(codePage)) return;
    if (client.right <= client.left || client.bottom <= client.top) return;

    const bool vertical = codePage == 932;
    std::vector<std::string> labels;
    std::vector<int> widths;
    int contentWidth = 0;
    for (const auto& entry : m_candidates.entries) {
        labels.push_back(std::to_string(labels.size() + 1) + " " + entry);
        const int width = EditTextWidth(dc, labels.back());
        widths.push_back(width);
        contentWidth = vertical ? (std::max)(contentWidth, width + 8) : contentWidth + width + 8;
    }
    const int height = vertical ? static_cast<int>(labels.size()) * 20 + 14 : 34;
    const auto anchor = static_cast<GroundSkillTextEditCtrl*>(m_editCtrl)->CandidateX(dc);
    const auto bounds = ImePopupBounds(*m_editCtrl, client, anchor, contentWidth + 22, height);
    const int x = bounds.left;
    const int y = bounds.top;
    const int width = bounds.right - bounds.left;
    const int saved = SaveDC(dc);
    if (!saved) return;
    IntersectClipRect(dc, client.left, client.top, client.right, client.bottom);
    IntersectClipRect(dc, bounds.left, bounds.top, bounds.right, bounds.bottom);

    const auto chip = [&](int sx, int sy, COLORREF fallback) {
        if (!m_colorChip.IsValid()) return fallback;
        const auto rgb = GetColorChipRgb(sx, sy);
        return RGB((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255);
    };
    const auto body = chip(2, 2, RGB(232,232,232));
    if (!m_candidateFrame) {
        const char* names[]{"sysbox_lu.bmp","sysbox_mu.bmp","sysbox_ru.bmp","sysbox_lm.bmp",
            "sysbox_rm.bmp","sysbox_ld.bmp","sysbox_md.bmp","sysbox_rd.bmp"};
        m_candidateFrame.emplace();
        for (std::size_t i = 0; i < m_candidateFrame->size(); ++i)
            (*m_candidateFrame)[i] = LoadChatRoomSkinBitmap(names[i]);
    }
    const auto& frame = *m_candidateFrame;
    if (width >= 28 && std::all_of(frame.begin(), frame.end(), [](const auto& tile) { return tile.IsValid(); })) {
        const auto draw = [&](int tile, int left, int top) {
            const auto& bitmap = frame[tile];
            shopui::DrawBitmapPixelsTransparent(dc, bitmap, {left,top,left+bitmap.width,top+bitmap.height});
        };
        for (int tx = x + 14; tx < bounds.right - 14; tx += 14) {
            draw(1, tx, y);
            draw(6, tx, bounds.bottom - frame[6].height);
        }
        for (int ty = y + 14; ty < bounds.bottom - 14; ty += 14) {
            draw(3, x, ty);
            draw(4, bounds.right - frame[4].width, ty);
        }
        draw(0, x, y);
        draw(2, bounds.right - frame[2].width, y);
        draw(5, x, bounds.bottom - frame[5].height);
        draw(7, bounds.right - frame[7].width, bounds.bottom - frame[7].height);
        shopui::FillRectColor(dc, {x+14,y+14,bounds.right-14,bounds.bottom-14}, body);
    } else {
        shopui::FillRectColor(dc, bounds, chip(22, 2, RGB(90,90,90)));
        shopui::FillRectColor(dc, {x+1,y+1,bounds.right-1,bounds.bottom-1}, body);
    }

    int textX = x + (vertical ? 10 : 14);
    int textY = y + 12;
    for (std::size_t i = 0; i < labels.size(); ++i) {
        const bool selected = static_cast<int>(i) == m_candidates.selection;
        if (vertical && selected)
            shopui::FillRectColor(dc, {textX-2,textY-2,textX+widths[i]+2,textY+14}, chip(6, 2, RGB(180,205,230)));
        const COLORREF color = selected && (codePage == 936 || codePage == 950) ? RGB(255,0,0) : RGB(0,0,0);
        DrawText(dc, {textX,textY,bounds.right,bounds.bottom}, labels[i], color, true);
        if (vertical) textY += 20;
        else textX += widths[i] + 8;
    }
    if (vertical) {
        const auto label = std::to_string(m_candidates.page + 1) + " / " + std::to_string(m_candidates.lastPage + 1);
        const int right = x + 110;
        DrawText(dc, {right-EditTextWidth(dc,label),y+192,bounds.right,bounds.bottom}, label, RGB(0,0,0), true);
    } else {
        const auto arrow = [&](int tip, int direction, bool active) {
            const auto color = static_cast<COLORREF>(active ? 16752496 : 13816530);
            for (int dy = -8; dy <= 8; ++dy) {
                const int edge = tip + direction * (std::abs(dy) * 6 / 8);
                const int base = tip + direction * 6;
                shopui::FillRectColor(dc, {(std::min)(edge,base),y+18+dy,
                    (std::max)(edge,base)+1,y+19+dy}, color);
            }
        };
        arrow(x + 8, 1, m_candidates.page > 0);
        arrow(bounds.right - 8, -1, m_candidates.page < m_candidates.lastPage);
    }
    RestoreDC(dc, saved);
}

#if RO_ENABLE_QT6_UI
namespace {
qsizetype Utf16Boundary(QStringView text, qint64 index, bool forward = false)
{
    auto offset = static_cast<qsizetype>((std::clamp)(index, qint64{0}, static_cast<qint64>(text.size())));
    if (offset > 0 && offset < text.size() && text[offset].isLowSurrogate() && text[offset - 1].isHighSurrogate())
        offset += forward ? 1 : -1;
    return offset;
}
}

void UIGroundSkillTextWnd::ApplyInputMethod(const QInputMethodEvent& event)
{
    if (!m_editCtrl) return;
    SyncSelection();
    CancelComposition();
    const auto commit = InputUtf8(event.commitString().toStdU16String());
    if (!commit.empty() || event.replacementStart() || event.replacementLength() > 0) {
        const auto begin = (std::min)(m_editCtrl->m_selectionOrigin, m_editCtrl->m_selectionCursor);
        const auto end = (std::max)(m_editCtrl->m_selectionOrigin, m_editCtrl->m_selectionCursor);
        auto next = m_editCtrl->m_text;
        next.erase(begin, end - begin);
        const auto surrounding = QString::fromStdString(next);
        const auto prefix = QString::fromUtf8(next.data(), begin).size();
        const auto start = static_cast<qint64>(prefix) + event.replacementStart();
        const auto length = (std::max)(0, event.replacementLength());
        const auto first = Utf16Boundary(surrounding, start);
        const auto last = length ? Utf16Boundary(surrounding, start + length, true) : first;
        const auto byteStart = surrounding.first(first).toUtf8().size();
        const auto byteEnd = surrounding.first(last).toUtf8().size();
        next.replace(byteStart, byteEnd - byteStart, commit);
        ApplyEdit(std::move(next), byteStart + commit.size());
    }
    int cursor = -1;
    bool visible = true;
    QList<QTextLayout::FormatRange> formats;
    for (const auto& attribute : event.attributes()) {
        if (attribute.type == QInputMethodEvent::Cursor) {
            cursor = (std::max)(0, attribute.start);
            visible = attribute.length != 0;
        } else if (attribute.type == QInputMethodEvent::TextFormat) {
            const auto start = Utf16Boundary(event.preeditString(), attribute.start);
            const auto end = Utf16Boundary(event.preeditString(),
                static_cast<qint64>(attribute.start) + attribute.length, true);
            if (end > start)
                formats.append({static_cast<int>(start), static_cast<int>(end - start),
                    attribute.value.value<QTextFormat>().toCharFormat()});
        } else if (attribute.type == QInputMethodEvent::Selection) {
            const auto surrounding = QString::fromStdString(m_editCtrl->m_text);
            const auto start = Utf16Boundary(surrounding, attribute.start);
            const auto end = Utf16Boundary(surrounding, static_cast<qint64>(attribute.start) + attribute.length);
            m_editCtrl->m_selectionOrigin = static_cast<int>(surrounding.first(start).toUtf8().size());
            m_editCtrl->m_selectionCursor = static_cast<int>(surrounding.first(end).toUtf8().size());
        }
    }
    SetComposition(event.preeditString().toStdU16String(), cursor, visible, false);
    if (!m_preedit.empty())
        static_cast<GroundSkillTextEditCtrl*>(m_editCtrl)->m_compositionFormats = std::move(formats);
}

void UIGroundSkillTextWnd::QueryInputMethod(QInputMethodQueryEvent& event)
{
    if (!m_editCtrl) return;
    SyncSelection();
    const auto& text = m_editCtrl->m_text;
    const auto cursor = m_editCtrl->m_selectionCursor;
    const auto anchor = m_editCtrl->m_selectionOrigin;
    event.setValue(Qt::ImEnabled, true);
    event.setValue(Qt::ImCursorRectangle, static_cast<GroundSkillTextEditCtrl*>(m_editCtrl)->CursorRectangle());
    event.setValue(Qt::ImSurroundingText, QString::fromStdString(text));
    const auto position = static_cast<int>(QString::fromUtf8(text.data(), cursor).size());
    event.setValue(Qt::ImCursorPosition, position);
    event.setValue(Qt::ImAbsolutePosition, position);
    event.setValue(Qt::ImAnchorPosition, static_cast<int>(QString::fromUtf8(text.data(), anchor).size()));
    const auto begin = (std::min)(cursor, anchor);
    const auto end = (std::max)(cursor, anchor);
    event.setValue(Qt::ImCurrentSelection, QString::fromUtf8(text.data() + begin, end - begin));
}
#endif

void UIGroundSkillTextWnd::ReplaceSelection(std::string_view insertion)
{
    SyncSelection();
    const auto begin = static_cast<std::size_t>((std::min)(m_editCtrl->m_selectionOrigin,m_editCtrl->m_selectionCursor));
    const auto end = static_cast<std::size_t>((std::max)(m_editCtrl->m_selectionOrigin,m_editCtrl->m_selectionCursor));
    std::string next = m_editCtrl->m_text;
    next.replace(begin,end-begin,insertion);
    ApplyEdit(std::move(next), begin + insertion.size());
}

void UIGroundSkillTextWnd::ApplyEdit(std::string next, std::size_t cursor)
{
    const int codePage = ro::SelectedSourceTextCodePage();
    const auto limit = static_cast<std::size_t>(m_editCtrl->m_maxchar);
    auto encoded = ro::EncodeSourceText(next,codePage);
    if (!encoded) return;
    if (encoded->size() > limit) {
        // Source SetTextByMaxChar keeps the prefix at a character boundary,
        // then fills the remaining byte positions with spaces.
        std::size_t retained = 0;
        std::size_t retainedBytes = 0;
        while (retained < next.size()) {
            const auto boundary = NextCharacter(next,retained);
            const auto prefix = ro::EncodeSourceText(std::string_view(next).substr(0,boundary),codePage);
            if (!prefix || prefix->size() > limit) break;
            retained = boundary;
            retainedBytes = prefix->size();
        }
        next.resize(retained);
        next.append(limit-retainedBytes,' ');
    }
    cursor = (std::min)(cursor,next.size());
    UpdateText(std::move(next),cursor);
}

std::optional<std::string> UIGroundSkillTextWnd::CopySelectionBytes()
{
    if (!m_editCtrl) return std::nullopt;
    SyncSelection();
    const auto begin = (std::min)(m_editCtrl->m_selectionOrigin, m_editCtrl->m_selectionCursor);
    const auto end = (std::max)(m_editCtrl->m_selectionOrigin, m_editCtrl->m_selectionCursor);
    if (begin == end) return std::nullopt;
    return ro::EncodeSourceText(std::string_view(m_editCtrl->m_text).substr(begin, end - begin),
        ro::SelectedSourceTextCodePage());
}

void UIGroundSkillTextWnd::PasteSelectionBytes(std::string_view bytes)
{
    m_highSurrogate = 0;
    if (!m_editCtrl) return;
    const auto decoded = ro::DecodeSourceText(bytes.substr(0, bytes.find('\0')), ro::SelectedSourceTextCodePage());
    if (decoded) ReplaceSelection(*decoded);
}

bool UIGroundSkillTextWnd::HandleClipboardKey(int key, bool shift, bool control, HWND owner)
{
    // CLanguage gives Shift precedence when both modifiers are down.
    const bool copy = !shift && control && key == 'C';
    const bool cut = shift ? key == VK_DELETE : control && key == 'X';
    const bool paste = shift ? key == VK_INSERT : control && key == 'V';
    if (!m_editCtrl || (!copy && !cut && !paste)) return false;
    m_highSurrogate = 0;

    const auto selection = paste ? std::nullopt : CopySelectionBytes();
#if defined(_WIN32)
    if (paste || selection) {
        PromptClipboard clipboard(owner);
        if (paste) {
            if (const auto bytes = clipboard.Read()) PasteSelectionBytes(*bytes);
        } else {
            clipboard.Write(*selection);
        }
    }
#elif RO_ENABLE_QT6_UI
    if (auto* clipboard = QGuiApplication::clipboard()) {
        const int codePage = ro::SelectedSourceTextCodePage();
        if (paste) {
            const auto* mime = clipboard->mimeData();
            if (mime && mime->hasText()) {
                if (const auto bytes = ro::EncodeSourceText(mime->text().toUtf8().toStdString(), codePage))
                    PasteSelectionBytes(*bytes);
            }
        } else if (selection) {
            if (const auto text = ro::DecodeSourceText(*selection, codePage))
                clipboard->setText(QString::fromUtf8(text->data(), text->size()));
        }
    }
#endif
    // The source deletes the selection even if opening the clipboard fails.
    if (cut && m_editCtrl->m_selectionOrigin != m_editCtrl->m_selectionCursor)
        HandleEditKey(VK_DELETE, false);
    return true;
}

bool UIGroundSkillTextWnd::HandleEditKey(int key, bool extendSelection)
{
    if (!m_editCtrl) return false;
    m_highSurrogate = 0;
    SyncSelection();
    const auto origin = static_cast<std::size_t>(m_editCtrl->m_selectionOrigin);
    auto cursor = static_cast<std::size_t>(m_editCtrl->m_selectionCursor);
    const auto& text = m_editCtrl->m_text;
    switch (key) {
    case VK_HOME: cursor = 0; break;
    case VK_END: cursor = text.size(); break;
    case VK_LEFT: cursor = PreviousCharacter(text,cursor); break;
    case VK_RIGHT: cursor = NextCharacter(text,cursor); break;
    case VK_BACK:
    case VK_DELETE: {
        if (key == VK_DELETE && extendSelection) return true;
        auto begin = (std::min)(origin,cursor);
        auto end = (std::max)(origin,cursor);
        if (begin == end) {
            if (key == VK_BACK) begin = PreviousCharacter(text,begin);
            else end = NextCharacter(text,end);
        }
        if (begin != end) {
            auto next = text;
            next.erase(begin,end-begin);
            UpdateText(std::move(next),begin);
        }
        return true;
    }
    default: return false;
    }
    CancelComposition();
    m_editCtrl->m_selectionCursor = static_cast<int>(cursor);
    if (!extendSelection) m_editCtrl->m_selectionOrigin = m_editCtrl->m_selectionCursor;
    m_editCtrl->Invalidate();
    return true;
}

void UIGroundSkillTextWnd::OnKeyDown(int key)
{
    m_highSurrogate = 0;
    HandleEditKey(key,(GetKeyState(VK_SHIFT)&0x8000)!=0);
}

void UIGroundSkillTextWnd::ActivateButton(int id)
{
    SendMsg(this,6,id,0,0);
}

msgresult_t UIGroundSkillTextWnd::SendMsg(UIWindow* sender,int message,msgparam_t value,msgparam_t lparam,msgparam_t extra)
{
    if(message==68){
        const auto* fields=reinterpret_cast<const int*>(value);
        if(fields)for(std::size_t i=0;i<m_values.size();++i)m_values[i]=fields[i];
        return 0;
    }
    if(message==0){return SendMsg(this,6,118,0,0);}
    if(message==1){return SendMsg(this,6,117,0,0);}
    if(message!=6)return UIWindow::SendMsg(sender,message,value,lparam,extra);
    if(value==118){
        GroundSkillTextRequest request{m_values,m_editCtrl?m_editCtrl->GetText():""};
        if(auto* mode=g_modeMgr.GetCurrentGameMode())mode->SendMsg(144,reinterpret_cast<msgparam_t>(&request),0,0);
        g_windowMgr.PostQuit(this);
    }else if(value==119){
        g_windowMgr.PostQuit(this);
    }
    return 0;
}
