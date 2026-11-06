#include "UISubChatWnd.h"
#include "UIWindowMgr.h"
#include "UINewChatWnd.h"
#include "core/ChatHistoryWrap.h"
#include "core/ChatNotice.h"
#include <algorithm>

namespace {
#if RO_ENABLE_QT6_UI
QFont HistoryFont()
{
    QFont font;
    font.setFamilies({QStringLiteral("Noto Sans"),QStringLiteral("Noto Sans JP"),QStringLiteral("Segoe UI")});
    font.setPixelSize(12);
    font.setStyleStrategy(QFont::PreferAntialias);
    return font;
}
#endif
int MeasureHistory(std::string_view text)
{
#if RO_ENABLE_QT6_UI
    return QFontMetrics(HistoryFont()).horizontalAdvance(QString::fromUtf8(text.data(), static_cast<int>(text.size())));
#else
    HDC dc = GetDC(nullptr);
    const auto old = SelectObject(dc, shopui::GetUiFont());
    SIZE size{};
    GetTextExtentPoint32A(dc, text.data(), static_cast<int>(text.size()), &size);
    SelectObject(dc, old); ReleaseDC(nullptr, dc);
    return size.cx;
#endif
}
void Text(HDC dc, RECT rect, const std::string& text, u32 color)
{
#if RO_ENABLE_QT6_UI
    if (rect.right <= rect.left || rect.bottom <= rect.top) return;
    const int width=rect.right-rect.left, height=rect.bottom-rect.top;
    QImage image(width,height,QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setFont(HistoryFont());
    painter.setPen(QColor::fromRgb(color));
    painter.drawText(QRect(0,0,width,height),Qt::AlignLeft|Qt::AlignTop|Qt::TextSingleLine,QString::fromUtf8(text.c_str()));
    painter.end();
    AlphaBlendArgbToHdc(dc,rect.left,rect.top,width,height,
        reinterpret_cast<const unsigned int*>(image.constBits()),width,height);
#else
    shopui::DrawWindowTextRect(dc,rect,text,RGB((color>>16)&255,(color>>8)&255,color&255),
        DT_LEFT|DT_TOP|DT_SINGLELINE|DT_NOPREFIX);
#endif
}
bool Contains(RECT rect,int x,int y) { return x>=rect.left && x<rect.right && y>=rect.top && y<rect.bottom; }
void Box(HDC dc, RECT rect, COLORREF color)
{
    const auto brush = CreateSolidBrush(color);
    FillRect(dc, &rect, brush);
    DeleteObject(brush);
}
}

UISubChatHisWnd::UISubChatHisWnd(int kind) : m_kind(kind)
{
    m_id = kind == 0 ? UIWindowMgr::WID_SUBCHATST : UIWindowMgr::WID_SUBCHATBT;
    m_rows.reserve(240);
    Create(300, 95);
    RestoreInfo();
    SetShow(0);
}

void UISubChatHisWnd::RestoreInfo()
{
    auto& info = g_windowMgr.m_subChatInfo[m_kind];
    const RECT client = g_windowMgr.GetChatClientRect();
    // Validate the complete cached rectangle. Restoration is not a resize gesture.
    if (info.width <= 0 || info.height <= 0 || info.x < 0 || info.y < 0
        || static_cast<long long>(info.x) + info.width > client.right
        || static_cast<long long>(info.y) + info.height > client.bottom) {
        info = {m_kind * 300, static_cast<int>(client.bottom) - 119, 300, 95};
    }
    Move(info.x, info.y);
    Resize(info.width, info.height);
}

void UISubChatHisWnd::StoreInfo()
{
    g_windowMgr.m_subChatInfo[m_kind] = {m_x, m_y, m_w, m_h};
}

void UISubChatHisWnd::AppendVisual(const SubChatRow& row)
{
    // AddItem2 reserves16px whenever a scrollbar object exists, even when hidden.
    for (const auto& text : ro::chat::WrapHistory(row.text,m_w-24,MeasureHistory)) {
        const int before=static_cast<int>(m_visualRows.size());
        if (before>=240) m_visualRows.erase(m_visualRows.begin(),m_visualRows.begin()+before/2);
        m_visualRows.push_back({text,row.color,row.talker});
        m_view.AfterAppend(before,static_cast<int>(m_visualRows.size()));
    }
}
void UISubChatHisWnd::RebuildVisual()
{
    m_visualRows.clear();
    m_view.first=0; m_view.selected=-1;
    for (const auto& row:m_rows) AppendVisual(row);
}
void UISubChatHisWnd::Resize(int width,int height)
{
    const int oldWidth=m_w;
    UIWindow::Resize(width,height);
    m_view.Resize((std::max)(0,(height-35)/14),static_cast<int>(m_visualRows.size()));
    if (oldWidth!=width) RebuildVisual();
    Invalidate();
}
void UISubChatHisWnd::Append(const char* text, u32 color, const char* talker)
{
    if (!text) return;
    if (m_rows.size()>=240) {
        m_rows.erase(m_rows.begin(),m_rows.begin()+m_rows.size()/2);
        // Logical trim clears/replays the visual owner; visual trim alone does not.
        RebuildVisual();
    }
    m_rows.push_back({text,color,talker?talker:""});
    AppendVisual(m_rows.back());
    Invalidate();
}
void UISubChatHisWnd::ScrollHistory(int delta)
{
    m_view.Scroll(delta,static_cast<int>(m_visualRows.size()));
    Invalidate();
}
std::vector<SubChatRow> UISubChatHisWnd::GetVisibleRows() const
{
    const int end=(std::min)(static_cast<int>(m_visualRows.size()),m_view.first+m_view.rows);
    return {m_visualRows.begin()+m_view.first,m_visualRows.begin()+end};
}
SubChatHistoryLayout UISubChatHisWnd::GetHistoryLayout() const
{
    SubChatHistoryLayout layout;
    layout.history={5,20,m_w-3,m_h-15};
    layout.first=m_view.first;
    layout.total=static_cast<int>(m_visualRows.size());
    layout.visible=(std::min)(layout.total,m_view.rows);
    if (layout.total>m_view.rows && layout.history.bottom>layout.history.top) {
        layout.scrollTrack={m_w-16,20,m_w-4,m_h-15};
        const int height=layout.scrollTrack.bottom-layout.scrollTrack.top;
        const int thumb=(std::min)(height,(std::max)(8,height*layout.visible/layout.total));
        const int offset=(height-thumb)*layout.first/(layout.total-layout.visible);
        layout.scrollThumb={layout.scrollTrack.left+1,20+offset,layout.scrollTrack.right-1,20+offset+thumb};
    }
    return layout;
}

msgresult_t UISubChatHisWnd::SendMsg(UIWindow* sender, int message, msgparam_t value, msgparam_t detail, msgparam_t extra)
{
    if (message == 14 && value == 7) {
        const int width = 280 + 32 * ((std::clamp(static_cast<int>(detail), 280, 512) - 280) / 32);
        const int height = 74 + 32 * ((std::clamp(static_cast<int>(extra), 74, 384) - 74) / 32);
        if (width==m_w && height==m_h) return 0;
        const bool sameWidth=width==m_w;
        Resize(width, height);
        // The source resizer message clears/replays even on height-only changes.
        if (sameWidth) RebuildVisual();
        Invalidate();
        return 0;
    }
    if (message==3 || message==4) {
        if (static_cast<int>(m_visualRows.size())>m_view.rows)
            ScrollHistory(message==3 ? 1-m_view.rows : m_view.rows-1);
        return 0;
    }
    if (message==6 && value==281) {
        // AddWhisperCharName2 does nothing without a main; do not resurrect it.
        if (g_windowMgr.m_chatWnd && detail) g_windowMgr.m_chatWnd->SendMsg(this,6,281,detail,0);
        return 0;
    }
    if (message == 37) {
        Append(reinterpret_cast<const char*>(value), ro::chat::ColorRefToUiRgb(static_cast<u32>(detail)), reinterpret_cast<const char*>(extra));
        return 0;
    }
    if (message == 6 && value == 136) {
        SetShow(0);
        if (g_windowMgr.m_subChatMiniWnd) g_windowMgr.m_subChatMiniWnd->SendMsg(this, 6, 136, m_kind, 0);
        return 0;
    }
    if (message == 6 && value == 291 && g_windowMgr.m_chatWnd) {
        g_windowMgr.m_chatWnd->SendMsg(this, 6, 291, 0, 0);
        return 0;
    }
    return UIWindow::SendMsg(sender, message, value, detail, extra);
}

void UISubChatHisWnd::OnDraw()
{
    HDC dc = AcquireDrawTarget();
    if (!dc) return;
    const int saved = SaveDC(dc);
    IntersectClipRect(dc, m_x, m_y, m_x + m_w, m_y + m_h);
    SetBkMode(dc, TRANSPARENT);
    Box(dc, {m_x, m_y, m_x + m_w, m_y + m_h}, RGB(42, 39, 35));
    Box(dc, {m_x, m_y, m_x + m_w, m_y + 16}, RGB(80, 90, 100));
    Text(dc, {m_x + 5, m_y, m_x + m_w - 65, m_y + 16}, m_kind == 0 ? "ST" : "BT", 0xffffff);
    Text(dc, {m_x + m_w - 18, m_y, m_x + m_w, m_y + 16}, "x", 0xffffff);
    if (m_kind == 1) Text(dc, {m_x + m_w - 65, m_y, m_x + m_w - 18, m_y + 16}, "Attach", 0xffffff);
    const auto layout=GetHistoryLayout();
    const int clip=SaveDC(dc);
    IntersectClipRect(dc,m_x+layout.history.left,m_y+layout.history.top,m_x+layout.history.right,m_y+layout.history.bottom);
    const auto visible=GetVisibleRows();
    for (std::size_t i=0;i<visible.size();++i) {
        const int y=m_y+layout.history.top+static_cast<int>(i)*14+3;
        Text(dc,{m_x+7,y,m_x+m_w-(layout.total>layout.visible?16:3),y+18},visible[i].text,visible[i].color);
    }
    RestoreDC(dc,clip);
    if (layout.total>layout.visible) {
        const auto global=[&](RECT r){r.left+=m_x;r.right+=m_x;r.top+=m_y;r.bottom+=m_y;return r;};
        Box(dc,global(layout.scrollTrack),RGB(64,64,64));
        Box(dc,global(layout.scrollThumb),RGB(184,184,184));
    }
    RestoreDC(dc, saved);
    ReleaseDrawTarget(dc);
}

void UISubChatHisWnd::OnLBtnDown(int x, int y)
{
    const int localX = x - m_x, localY = y - m_y;
    if (localY < 16 && localX >= m_w - 18) { SendMsg(this, 6, 136, 0, 0); return; }
    if (m_kind == 1 && localY < 16 && localX >= m_w - 65) { SendMsg(this, 6, 291, 0, 0); return; }
    const auto layout=GetHistoryLayout();
    if (Contains(layout.history,localX,localY)) {
        if (layout.total>layout.visible && Contains(layout.scrollTrack,localX,localY)) {
            if (Contains(layout.scrollThumb,localX,localY)) {
                m_scrollDragging=true; m_scrollGrab=localY-layout.scrollThumb.top;
            } else ScrollHistory(localY<layout.scrollThumb.top ? 1-m_view.rows : m_view.rows-1);
        } else {
            const int index=m_view.first+(localY-layout.history.top)/14;
            m_view.selected=index<layout.total?index:-1;
        }
        return;
    }
    m_resizing = localX >= m_w - 15 && localY >= m_h - 15;
    m_dragging = !m_resizing && localY < 16;
    m_startX = x; m_startY = y; m_oldX = m_x; m_oldY = m_y; m_oldW = m_w; m_oldH = m_h;
}

void UISubChatHisWnd::OnLBtnDblClk(int x,int y)
{
    const auto layout=GetHistoryLayout();
    if (!Contains(layout.history,x-m_x,y-m_y) || Contains(layout.scrollTrack,x-m_x,y-m_y)) return;
    if (m_view.selected<0 || m_view.selected>=static_cast<int>(m_visualRows.size())) return;
    const auto& talker=m_visualRows[m_view.selected].talker;
    if (!talker.empty()) SendMsg(this,6,281,reinterpret_cast<msgparam_t>(talker.c_str()),0);
}
void UISubChatHisWnd::OnWheel(int delta) { ScrollHistory(delta>=0?-3:3); }
void UISubChatHisWnd::OnMouseMove(int x, int y)
{
    if (m_scrollDragging) {
        const auto layout=GetHistoryLayout();
        const int travel=layout.scrollTrack.bottom-layout.scrollTrack.top-(layout.scrollThumb.bottom-layout.scrollThumb.top);
        const int pos=std::clamp(y-m_y-static_cast<int>(layout.scrollTrack.top)-m_scrollGrab,0,(std::max)(0,travel));
        const int target=travel>0?pos*m_view.LastFirst(layout.total)/travel:0;
        ScrollHistory(target-m_view.first);
        return;
    }
    if (m_resizing) SendMsg(this, 14, 7, m_oldW + x - m_startX, m_oldH + y - m_startY);
    else if (m_dragging) Move(m_oldX + x - m_startX, m_oldY + y - m_startY);
}

void UISubChatHisWnd::OnLBtnUp(int, int)
{
    if (m_dragging || m_resizing) StoreInfo();
    CancelPointerCapture();
}

void UISubChatHisWnd::CancelPointerCapture()
{
    m_dragging = m_resizing = m_scrollDragging = false;
    if (g_windowMgr.m_captureWindow == this) g_windowMgr.m_captureWindow = nullptr;
}

UISubChatMiniWnd::UISubChatMiniWnd()
{
    m_id = UIWindowMgr::WID_SUBCHATMINI;
    Create(64, 32);
    SetShow(0);
}

void UISubChatMiniWnd::UpdateHistoryVisibility()
{
    if (g_windowMgr.m_subChatWndST) g_windowMgr.m_subChatWndST->SetShow(m_show && g_windowMgr.m_onStSubChat);
    if (g_windowMgr.m_subChatWndBT) g_windowMgr.m_subChatWndBT->SetShow(m_show && g_windowMgr.m_onBtSubChat);
}

msgresult_t UISubChatMiniWnd::SendMsg(UIWindow* sender, int message, msgparam_t value, msgparam_t detail, msgparam_t extra)
{
    if (message == 6) {
        if (value == 131) Move(static_cast<int>(detail), static_cast<int>(extra) - 32);
        else if (value == 136) OnLBtnDown(m_x + static_cast<int>(detail) * 32 + 1, m_y);
        else if (value == 205) UpdateHistoryVisibility();
        return 0;
    }
    return UIWindow::SendMsg(sender, message, value, detail, extra);
}

void UISubChatMiniWnd::OnLBtnDown(int x, int)
{
    const int localX = x - m_x;
    bool& toggle = localX <= 0 || localX > 32 ? g_windowMgr.m_onBtSubChat : g_windowMgr.m_onStSubChat;
    toggle = !toggle;
    UpdateHistoryVisibility();
    Invalidate();
}

void UISubChatMiniWnd::OnDraw()
{
    HDC dc = AcquireDrawTarget();
    if (!dc) return;
    const int saved = SaveDC(dc);
    SetBkMode(dc, TRANSPARENT);
    Box(dc, {m_x, m_y, m_x + 32, m_y + 32}, g_windowMgr.m_onStSubChat ? RGB(90, 120, 155) : RGB(55, 60, 65));
    Box(dc, {m_x + 32, m_y, m_x + 64, m_y + 32}, g_windowMgr.m_onBtSubChat ? RGB(90, 120, 155) : RGB(55, 60, 65));
    Text(dc, {m_x + 4, m_y, m_x + 32, m_y + 32}, "ST", 0xffffff);
    Text(dc, {m_x + 36, m_y, m_x + 64, m_y + 32}, "BT", 0xffffff);
    RestoreDC(dc, saved);
    ReleaseDrawTarget(dc);
}
