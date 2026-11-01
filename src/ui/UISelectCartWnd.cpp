#include "UiMetrics.h"
#include "UISelectCartWnd.h"

#include "gamemode/GameMode.h"
#include "gamemode/Mode.h"
#include "main/WinMain.h"
#include "qtui/QtUiRuntime.h"
#include "world/GameActor.h"
#include "world/World.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>

namespace {

constexpr int kWindowWidth = 332;
constexpr int kWindowHeight = 126;
constexpr int kTitleBarHeight = uiMetrics::titleHeight;
constexpr int kOptionWidth = 56;
constexpr int kOptionHeight = 52;
constexpr int kOptionSpacing = 8;
constexpr int kOptionBaseId = 9000;
constexpr int kCloseButtonId = 135;

RECT MakeRect(int x, int y, int w, int h)
{
    RECT rect{ x, y, x + w, y + h };
    return rect;
}

bool IsPointInRect(const RECT& rect, int x, int y)
{
    return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
}

void FillRectColor(HDC hdc, const RECT& rect, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(hdc, &rect, brush);
    DeleteObject(brush);
}

void FrameRectColor(HDC hdc, const RECT& rect, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    FrameRect(hdc, &rect, brush);
    DeleteObject(brush);
}

void DrawCenteredText(HDC hdc, const RECT& rect, const char* text, COLORREF color)
{
    if (!hdc || !text) {
        return;
    }

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    RECT drawRect = rect;
    DrawTextA(hdc, text, -1, &drawRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

int ResolveLocalPlayerBaseLevel()
{
    const CGameMode* gameMode = g_modeMgr.GetCurrentGameMode();
    if (!gameMode || !gameMode->m_world || !gameMode->m_world->m_player) {
        return 1;
    }

    return (std::max)(1, static_cast<int>(gameMode->m_world->m_player->m_clevel));
}

const char* GetCartLabel(int index)
{
    static constexpr std::array<const char*, 5> kLabels = {
        "Cart 1",
        "Cart 2",
        "Cart 3",
        "Cart 4",
        "Cart 5",
    };
    return (index >= 0 && index < static_cast<int>(kLabels.size())) ? kLabels[static_cast<size_t>(index)] : "Cart";
}

} // namespace

UISelectCartWnd::UISelectCartWnd()
    : m_controlsCreated(false)
    , m_hoveredIndex(-1)
    , m_pressedIndex(-1)
{
    Create(kWindowWidth, kWindowHeight);
}

UISelectCartWnd::~UISelectCartWnd() = default;

void UISelectCartWnd::SetShow(int show)
{
    UIWindow::SetShow(show);
    if (show == 0) {
        m_hoveredIndex = -1;
        m_pressedIndex = -1;
        return;
    }

    EnsureCreated();
    if (!g_hMainWnd) {
        return;
    }

    RECT clientRect{};
    GetClientRect(g_hMainWnd, &clientRect);
    Move((clientRect.right - clientRect.left - m_w) / 2,
        (clientRect.bottom - clientRect.top - m_h) / 2);
    Invalidate();
}

void UISelectCartWnd::OnCreate(int cx, int cy)
{
    if (m_controlsCreated) {
        return;
    }

    m_controlsCreated = true;
    Create(kWindowWidth, kWindowHeight);
    Move((cx - m_w) / 2, (cy - m_h) / 2);
}

void UISelectCartWnd::OnDraw()
{
    EnsureCreated();
    if (IsQtUiRuntimeEnabled()) {
        return;
    }
    if (!g_hMainWnd || m_show == 0) {
        return;
    }

    HDC hdc = AcquireDrawTarget();
    if (!hdc) {
        return;
    }

    const RECT panel = MakeRect(m_x, m_y, m_w, m_h);
    const RECT titleBar = MakeRect(m_x, m_y, m_w, kTitleBarHeight);
    const RECT content = MakeRect(m_x, m_y + kTitleBarHeight, m_w, m_h - kTitleBarHeight);

    FillRectColor(hdc, panel, RGB(244, 241, 230));
    FrameRectColor(hdc, panel, RGB(70, 58, 44));
    FillRectColor(hdc, titleBar, RGB(98, 74, 42));
    DrawCenteredText(hdc, MakeRect(m_x + 8, m_y + 1, m_w - 40, kTitleBarHeight - 2), "Select Cart", RGB(255, 248, 232));

    const RECT closeRect = GetCloseRect();
    FillRectColor(hdc, closeRect, RGB(160, 70, 55));
    FrameRectColor(hdc, closeRect, RGB(95, 28, 20));
    DrawCenteredText(hdc, closeRect, "X", RGB(255, 255, 255));

    FillRectColor(hdc, content, RGB(252, 250, 245));
    DrawCenteredText(hdc, MakeRect(m_x + 8, m_y + 22, m_w - 16, 16), "Choose the pushcart style to equip.", RGB(65, 54, 42));

    const int optionCount = GetAvailableCartCount();
    for (int index = 0; index < optionCount; ++index) {
        const RECT optionRect = GetOptionRect(index);
        COLORREF fillColor = RGB(230, 223, 206);
        if (index == m_pressedIndex) {
            fillColor = RGB(191, 170, 130);
        } else if (index == m_hoveredIndex) {
            fillColor = RGB(214, 198, 162);
        }

        FillRectColor(hdc, optionRect, fillColor);
        FrameRectColor(hdc, optionRect, RGB(107, 88, 54));
        DrawCenteredText(hdc, MakeRect(optionRect.left, optionRect.top + 10, optionRect.right - optionRect.left, 16), GetCartLabel(index), RGB(43, 34, 22));

        char numberText[8]{};
        std::snprintf(numberText, sizeof(numberText), "%d", index + 1);
        DrawCenteredText(hdc, MakeRect(optionRect.left, optionRect.top + 26, optionRect.right - optionRect.left, 18), numberText, RGB(120, 88, 24));
    }

    ReleaseDrawTarget(hdc);
    m_isDirty = 0;
}

void UISelectCartWnd::OnLBtnDown(int x, int y)
{
    m_pressedIndex = -1;

    if (HitTestClose(x, y)) {
        m_pressedIndex = kCloseButtonId;
        Invalidate();
        return;
    }

    const int optionIndex = HitTestOption(x, y);
    if (optionIndex >= 0) {
        m_pressedIndex = optionIndex;
        PlayUiButtonSound();
        Invalidate();
        return;
    }

    if (y >= m_y && y < m_y + kTitleBarHeight) {
        UIFrameWnd::OnLBtnDown(x, y);
    }
}

void UISelectCartWnd::OnLBtnUp(int x, int y)
{
    const bool wasDragging = m_isDragging != 0;
    UIFrameWnd::OnLBtnUp(x, y);
    if (wasDragging) {
        m_pressedIndex = -1;
        return;
    }

    const int pressedIndex = m_pressedIndex;
    m_pressedIndex = -1;

    if (pressedIndex == kCloseButtonId && HitTestClose(x, y)) {
        CloseWindow();
        return;
    }

    if (pressedIndex >= 0 && pressedIndex < GetAvailableCartCount() && HitTestOption(x, y) == pressedIndex) {
        ActivateOption(pressedIndex);
        return;
    }

    Invalidate();
}

void UISelectCartWnd::OnMouseMove(int x, int y)
{
    UIFrameWnd::OnMouseMove(x, y);

    const int hovered = HitTestOption(x, y);
    if (hovered != m_hoveredIndex) {
        m_hoveredIndex = hovered;
        Invalidate();
    }
}

void UISelectCartWnd::OnKeyDown(int virtualKey)
{
    if (virtualKey == VK_ESCAPE) {
        CloseWindow();
        return;
    }

    if (virtualKey >= '1' && virtualKey <= '5') {
        const int index = virtualKey - '1';
        if (index < GetAvailableCartCount()) {
            ActivateOption(index);
        }
    }
}

void UISelectCartWnd::StoreInfo()
{
    SaveUiWindowPlacement("SelectCartWnd", m_x, m_y);
}

int UISelectCartWnd::GetVisibleOptionCount() const
{
    return GetAvailableCartCount();
}

int UISelectCartWnd::GetHoverIndex() const
{
    return m_hoveredIndex;
}

int UISelectCartWnd::GetPressedIndex() const
{
    return m_pressedIndex;
}

RECT UISelectCartWnd::GetOptionRectForRender(int index) const
{
    return GetOptionRect(index);
}

void UISelectCartWnd::EnsureCreated()
{
    if (m_controlsCreated || !g_hMainWnd) {
        return;
    }

    RECT clientRect{};
    GetClientRect(g_hMainWnd, &clientRect);
    OnCreate(clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);
}

int UISelectCartWnd::GetAvailableCartCount() const
{
    const int baseLevel = ResolveLocalPlayerBaseLevel();
    if (baseLevel <= 40) {
        return 1;
    }
    if (baseLevel <= 65) {
        return 2;
    }
    if (baseLevel <= 80) {
        return 3;
    }
    if (baseLevel <= 90) {
        return 4;
    }
    return 5;
}

RECT UISelectCartWnd::GetOptionRect(int index) const
{
    const int optionCount = GetAvailableCartCount();
    const int totalWidth = optionCount * kOptionWidth + (optionCount - 1) * kOptionSpacing;
    const int startX = m_x + (m_w - totalWidth) / 2;
    return MakeRect(startX + index * (kOptionWidth + kOptionSpacing), m_y + 48, kOptionWidth, kOptionHeight);
}

RECT UISelectCartWnd::GetCloseRect() const
{
    return MakeRect(m_x + m_w - 22, m_y + 2, 18, 13);
}

int UISelectCartWnd::HitTestOption(int x, int y) const
{
    const int optionCount = GetAvailableCartCount();
    for (int index = 0; index < optionCount; ++index) {
        if (IsPointInRect(GetOptionRect(index), x, y)) {
            return index;
        }
    }
    return -1;
}

bool UISelectCartWnd::HitTestClose(int x, int y) const
{
    return IsPointInRect(GetCloseRect(), x, y);
}

void UISelectCartWnd::CloseWindow()
{
    SetShow(0);
}

bool UISelectCartWnd::ActivateOption(int index)
{
    if (index < 0 || index >= GetAvailableCartCount()) {
        return false;
    }

    if (g_modeMgr.SendMsg(CGameMode::GameMsg_RequestChangeCart, index + 1, 0, 0) != 0) {
        CloseWindow();
        return true;
    }

    return false;
}