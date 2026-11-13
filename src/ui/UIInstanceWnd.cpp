#include "UIInstanceWnd.h"

#include "UIShopCommon.h"
#include "UIWindowMgr.h"
#include "core/SourceTextCodec.h"
#include "session/Session.h"

#include <cstdio>
#include <ctime>

namespace {
std::string Substitute(std::string text, const char* format, const std::string& value)
{
    const auto offset = text.find(format);
    if (offset != std::string::npos) {
        text.replace(offset, 2, value);
    }
    for (const char* token : {"^ff0000", "^000000"}) {
        const auto color = text.find(token);
        if (color != std::string::npos) text.erase(color, 7);
    }
    return text;
}

std::string Countdown(u32 deadline, u64 now)
{
    const u32 remaining = deadline > now ? static_cast<u32>(deadline - now) : 0;
    char text[24];
    std::snprintf(text, sizeof(text), "%lu:%02lu:%02lu", static_cast<unsigned long>(remaining / 3600),
        static_cast<unsigned long>(remaining / 60 % 60), static_cast<unsigned long>(remaining % 60));
    return text;
}
}

UIInstanceWnd::UIInstanceWnd(int windowId)
{
    m_id = windowId;
    Create(200, 150);
    int x = 0, y = 310;
    LoadUiWindowPlacement("MEMORIALDUNWND", &x, &y);
    Move(x, y);
    m_strings.Load();
}

UIInstanceWnd::Display UIInstanceWnd::GetDisplay(u64 now) const
{
    const auto label = [&](int id, const char* fallback) {
        const auto text = m_strings.Get(id, fallback);
        return ro::DecodeSourceText(text, ro::SelectedSourceTextCodePage()).value_or(text);
    };
    const auto& snapshot = g_session.m_instanceStatus;
    const auto name = ro::DecodeSourceText(snapshot.name, ro::SelectedSourceTextCodePage()).value_or(snapshot.name);
    Display display;
    display.title = label(1336, "Memorial Dungeon");
    using Phase = CSession::InstanceStatus::Phase;
    switch (snapshot.phase) {
    case Phase::None:
        break;
    case Phase::Waiting:
        display.status = Substitute(label(1337, "%s in Standby"), "%s", name);
        display.details.push_back(Substitute(label(1342, "Your Standby Priority: %d"), "%d",
            std::to_string(snapshot.queuePosition)));
        break;
    case Phase::Ready:
        display.status = Substitute(label(1338, "%s Available"), "%s", name);
        display.details.push_back(Substitute(label(1343, "The requested dungeon will be removed if you do not enter within %s."),
            "%s", Countdown(snapshot.idleDeadline, now)));
        break;
    case Phase::Active:
        display.status = Substitute(label(1339, "%s in Progress"), "%s", name);
        if (snapshot.expiration) {
            display.details.push_back(label(1344, "Dungeon Mission Time Limit:"));
            display.details.push_back(Countdown(snapshot.expiration, now));
        }
        break;
    }
    return display;
}

void UIInstanceWnd::OnProcess()
{
    const auto now = static_cast<u64>(std::time(nullptr));
    if (m_show && now != m_displaySecond) {
        m_displaySecond = now;
        Invalidate();
    }
}

void UIInstanceWnd::OnDraw()
{
    HDC dc = AcquireDrawTarget();
    if (!dc) return;
    const auto display = GetDisplay(static_cast<u64>(std::time(nullptr)));
    shopui::FillRectColor(dc, {m_x, m_y, m_x + m_w, m_y + m_h}, RGB(255, 255, 255));
    shopui::FillRectColor(dc, {m_x, m_y, m_x + m_w, m_y + 18}, RGB(222, 230, 241));
    const auto draw = [&](const std::string& text, RECT bounds, UINT flags) {
        OffsetRect(&bounds, m_x, m_y);
        shopui::DrawWindowTextRect(dc, bounds, text, RGB(0, 0, 0), flags | DT_NOPREFIX);
    };
    draw(display.title, {8, 0, 178, 18}, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw("×", {182, 0, 198, 18}, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
    draw(display.status, {8, 25, 192, 42}, DT_SINGLELINE | DT_END_ELLIPSIS);
    int y = 48;
    for (const auto& detail : display.details) {
        draw(detail, {8, y, 192, m_h - 6}, DT_WORDBREAK);
        y += 18;
    }
    ReleaseDrawTarget(dc);
}

bool UIInstanceWnd::InCloseButton(int x, int y) const
{
    return x >= m_x + m_w - 18 && x < m_x + m_w && y >= m_y && y < m_y + 18;
}

void UIInstanceWnd::OnLBtnDown(int x, int y)
{
    m_closePressed = InCloseButton(x, y);
    if (!m_closePressed) UIFrameWnd::OnLBtnDown(x, y);
}

void UIInstanceWnd::OnLBtnUp(int x, int y)
{
    if (m_closePressed && InCloseButton(x, y)) {
        g_windowMgr.SetInstanceWindowVisible(false);
    }
    m_closePressed = false;
    UIFrameWnd::OnLBtnUp(x, y);
}

void UIInstanceWnd::StoreInfo()
{
    SaveUiWindowPlacement("MEMORIALDUNWND", m_x, m_y);
}
