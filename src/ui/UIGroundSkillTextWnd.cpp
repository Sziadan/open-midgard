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

#if RO_ENABLE_QT6_UI
#include <QFont>
#include <QFontMetrics>
#include <QImage>
#include <QPainter>
#endif

namespace {
bool Contains(RECT r,int x,int y)
{
    return x>=r.left && x<r.right && y>=r.top && y<r.bottom;
}

RECT GlobalBounds(RECT r,const UIWindow& owner)
{
    r.left+=owner.m_x;r.right+=owner.m_x;r.top+=owner.m_y;r.bottom+=owner.m_y;return r;
}

void DrawText(HDC dc,RECT rect,const std::string& value,COLORREF color,bool utf8=false)
{
    if(rect.right<=rect.left || rect.bottom<=rect.top)return;
#if RO_ENABLE_QT6_UI
    QImage image(rect.right-rect.left,rect.bottom-rect.top,QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);QFont font("Segoe UI");font.setPixelSize(12);painter.setFont(font);
    painter.setPen(QColor::fromRgb(GetRValue(color),GetGValue(color),GetBValue(color)));
    painter.drawText(image.rect(),Qt::AlignLeft|Qt::AlignTop|Qt::TextSingleLine,
                     utf8 ? QString::fromStdString(value) : QString::fromLocal8Bit(value.c_str()));
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
        const auto cursor = static_cast<std::size_t>(m_selectionCursor);
        const auto anchor = static_cast<std::size_t>(m_selectionOrigin);
        const std::string_view text(m_text);
        if (!focused || (std::min)(anchor,cursor) == 0) m_scrollStart = 0;
        m_scrollStart = (std::min)(m_scrollStart,cursor);
        while (m_scrollStart && m_scrollStart < text.size() && (static_cast<unsigned char>(text[m_scrollStart]) & 0xc0) == 0x80)
            --m_scrollStart;
        while (focused && m_scrollStart < cursor && EditTextWidth(dc,text.substr(m_scrollStart,cursor-m_scrollStart)) > m_w-12)
            m_scrollStart = NextCharacter(text,m_scrollStart);
        const int originX = m_x+3;
        const auto pixel = [&](std::size_t offset) {
            return originX + EditTextWidth(dc,text.substr(m_scrollStart,offset > m_scrollStart ? offset-m_scrollStart : 0));
        };
        const int saved = SaveDC(dc);
        IntersectClipRect(dc,m_x,m_y,m_x+m_w,m_y+m_h);
        shopui::FillRectColor(dc,{m_x,m_y,m_x+m_w,m_y+m_h},RGB(m_r,m_g,m_b));
        if (focused && anchor != cursor) {
            const auto rgb = m_owner->GetColorChip().IsValid() ? m_owner->GetColorChipRgb(6,2) : 0xb4cde6u;
            shopui::FillRectColor(dc,{pixel((std::min)(anchor,cursor)),m_y,pixel((std::max)(anchor,cursor)),m_y+m_h},
                RGB((rgb>>16)&255,(rgb>>8)&255,rgb&255));
        }
        const auto textColor = RGB(m_textR,m_textG,m_textB);
        DrawText(dc,{originX,m_y+m_yOffset,m_x+m_w,m_y+m_h},std::string(text.substr(m_scrollStart)),textColor,true);
        if (focused && anchor == cursor && ((timeGetTime()/500)&1)==0)
            DrawText(dc,{pixel(cursor)-2,m_y+m_yOffset,m_x+m_w,m_y+m_h},"|",textColor);
        RestoreDC(dc,saved);
        ReleaseDrawTarget(dc);
    }
    void OnLBtnDown(int x,int y) override
    {
        UIEditCtrl::OnLBtnDown(x,y);
        if(m_owner)m_owner->FocusEdit();
    }
private:
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
        m_selectionText = m_editCtrl->m_text;
        m_editCtrl->m_selectionOrigin = m_editCtrl->m_selectionCursor = static_cast<int>(m_selectionText.size());
    }
}

void UIGroundSkillTextWnd::UpdateText(std::string text, std::size_t cursor)
{
    m_editCtrl->m_text = std::move(text);
    m_selectionText = m_editCtrl->m_text;
    m_editCtrl->m_selectionOrigin = m_editCtrl->m_selectionCursor = static_cast<int>(cursor);
    m_editCtrl->Invalidate();
}

void UIGroundSkillTextWnd::CommitText(std::u16string_view text)
{
    m_highSurrogate = 0;
    if (!m_editCtrl) return;
    std::string insertion;
    for (std::size_t i = 0; i < text.size(); ++i) {
        char32_t code = text[i];
        if (code >= 0xd800 && code <= 0xdbff) {
            if (i + 1 == text.size() || text[i + 1] < 0xdc00 || text[i + 1] > 0xdfff) continue;
            code = 0x10000 + ((code - 0xd800) << 10) + (text[++i] - 0xdc00);
        }
        if (code < 0x20 || (code >= 0xdc00 && code <= 0xdfff)) continue;
        const int count = code < 0x80 ? 1 : code < 0x800 ? 2 : code < 0x10000 ? 3 : 4;
        if (count == 1) {
            insertion += static_cast<char>(code);
        } else {
            constexpr unsigned prefixes[]{0, 0, 0xc0, 0xe0, 0xf0};
            insertion += static_cast<char>(prefixes[count] | (code >> (6 * (count - 1))));
            for (int shift = 6 * (count - 2); shift >= 0; shift -= 6)
                insertion += static_cast<char>(0x80 | ((code >> shift) & 0x3f));
        }
    }
    if (insertion.empty()) return;
    SyncSelection();
    const auto begin = static_cast<std::size_t>((std::min)(m_editCtrl->m_selectionOrigin,m_editCtrl->m_selectionCursor));
    const auto end = static_cast<std::size_t>((std::max)(m_editCtrl->m_selectionOrigin,m_editCtrl->m_selectionCursor));
    std::string next = m_editCtrl->m_text;
    next.replace(begin,end-begin,insertion);
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
    const auto cursor = (std::min)(begin+insertion.size(),next.size());
    UpdateText(std::move(next),cursor);
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
