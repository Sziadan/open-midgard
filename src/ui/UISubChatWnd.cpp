#include "UISubChatWnd.h"
#include "UIWindowMgr.h"
#include "UINewChatWnd.h"
#include <algorithm>

namespace {
void Text(HDC dc, RECT rect, const std::string& text, u32 color)
{
    shopui::DrawWindowTextRect(dc, rect, text, RGB((color >> 16) & 255, (color >> 8) & 255, color & 255),
        DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
}
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

void UISubChatHisWnd::Append(const char* text, u32 color, const char* talker)
{
    if (!text) return;
    // Source trims BEFORE append, keeping aligned text/color/talker ownership.
    if (m_rows.size() >= 240) m_rows.erase(m_rows.begin(), m_rows.begin() + m_rows.size() / 2);
    m_rows.push_back({text, color, talker ? talker : ""});
    Invalidate();
}

msgresult_t UISubChatHisWnd::SendMsg(UIWindow* sender, int message, msgparam_t value, msgparam_t detail, msgparam_t extra)
{
    if (message == 14 && value == 7) {
        const int width = 280 + 32 * ((std::clamp(static_cast<int>(detail), 280, 512) - 280) / 32);
        const int height = 74 + 32 * ((std::clamp(static_cast<int>(extra), 74, 384) - 74) / 32);
        Resize(width, height);
        Invalidate();
        return 0;
    }
    if (message == 37) {
        Append(reinterpret_cast<const char*>(value), static_cast<u32>(detail), reinterpret_cast<const char*>(extra));
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
    const int visible = std::max(0, (m_h - 15 - 16 - 4) / 14);
    const int first = std::max(0, static_cast<int>(m_rows.size()) - visible);
    for (int i = first; i < static_cast<int>(m_rows.size()); ++i) {
        const int y = m_y + 18 + (i - first) * 14;
        Text(dc, {m_x + 5, y, m_x + m_w - 3, y + 14}, m_rows[i].text, m_rows[i].color);
    }
    RestoreDC(dc, saved);
    ReleaseDrawTarget(dc);
}

void UISubChatHisWnd::OnLBtnDown(int x, int y)
{
    const int localX = x - m_x, localY = y - m_y;
    if (localY < 16 && localX >= m_w - 18) { SendMsg(this, 6, 136, 0, 0); return; }
    if (m_kind == 1 && localY < 16 && localX >= m_w - 65) { SendMsg(this, 6, 291, 0, 0); return; }
    m_resizing = localX >= m_w - 15 && localY >= m_h - 15;
    m_dragging = !m_resizing && localY < 16;
    m_startX = x; m_startY = y; m_oldX = m_x; m_oldY = m_y; m_oldW = m_w; m_oldH = m_h;
}

void UISubChatHisWnd::OnMouseMove(int x, int y)
{
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
    m_dragging = m_resizing = false;
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
