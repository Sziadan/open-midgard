#include "UIProhibitionHistoryWnd.h"
#include "UIWindowMgr.h"
#include "UIShopCommon.h"
#include "render/DC.h"
#include "session/Session.h"
#include <cstdio>

namespace {
constexpr int visibleRows = 18;
}

UIProhibitionHistoryWnd::UIProhibitionHistoryWnd()
{
    Create(500, 400);
    Move(100, 100);
}

void UIProhibitionHistoryWnd::SetTarget(u32 aid, std::string server)
{
    m_aid = aid;
    m_server = std::move(server);
    m_closePressed = false;
    m_firstRow = 0;
    Invalidate();
}

RECT UIProhibitionHistoryWnd::CloseRect() const
{
    return shopui::MakeRect(m_x + m_w - 14, m_y + 2, 12, 14);
}

void UIProhibitionHistoryWnd::OnDraw()
{
    if (!m_show) return;
    HDC hdc = AcquireDrawTarget();
    if (!hdc) return;
    shopui::DrawFrameWindow(hdc, shopui::MakeRect(m_x, m_y, m_w, m_h), "Prohibition Log");
    const auto draw = [&](int y, const std::string& text) {
        shopui::DrawWindowTextRect(hdc, shopui::MakeRect(m_x + 10, m_y + y, m_w - 20, 18),
            text, RGB(0, 0, 0), DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    };
    int y = 65;
    const auto matches = [&](const auto& row) { return row.aid == m_aid && row.server == m_server; };
    const auto& rows = g_session.m_prohibitionHistory.Rows();
    const int count = static_cast<int>(std::count_if(rows.begin(), rows.end(), matches));
    m_firstRow = std::clamp(m_firstRow, 0, (std::max)(0, count - visibleRows));
    int index = 0;
    for (const auto& row : rows) {
        if (!matches(row)) continue;
        if (index == 0) {
            draw(25, "AccountName : " + row.account + "        CharacterName : " + row.character);
            draw(41, "ServerName : " + row.server);
        }
        if (index++ < m_firstRow) continue;
        char details[96]{};
        std::snprintf(details, sizeof(details), "ProhibitTime : %u        %04u-%02u-%02u %02u:%02u:%02u",
            row.duration, row.date >> 16, row.date >> 8 & 255u, row.date & 255u,
            row.time >> 16, row.time >> 8 & 255u, row.time & 255u);
        draw(y, details);
        y += 18;
        if (y + 18 > m_h) break;
    }
    shopui::DrawWindowTextRect(hdc, CloseRect(), "x", RGB(0, 0, 0), DT_CENTER | DT_SINGLELINE);
    ReleaseDrawTarget(hdc);
}

void UIProhibitionHistoryWnd::OnWheel(int delta)
{
    if (delta == 0) return;
    m_firstRow = (std::max)(0, m_firstRow + (delta < 0 ? 3 : -3));
    Invalidate();
}

void UIProhibitionHistoryWnd::OnLBtnDown(int x, int y)
{
    const RECT close = CloseRect();
    m_closePressed = PtInRect(&close, POINT{x, y}) != 0;
    if (!m_closePressed) UIFrameWnd::OnLBtnDown(x, y);
}

void UIProhibitionHistoryWnd::OnLBtnUp(int x, int y)
{
    const RECT close = CloseRect();
    const bool pressed = m_closePressed;
    m_closePressed = false;
    if (pressed && PtInRect(&close, POINT{x, y})) g_windowMgr.DeleteWindow(this);
    else UIFrameWnd::OnLBtnUp(x, y);
}

void UIProhibitionHistoryWnd::OnKeyDown(int key)
{
    if (key == VK_ESCAPE) g_windowMgr.DeleteWindow(this);
    else UIFrameWnd::OnKeyDown(key);
}
