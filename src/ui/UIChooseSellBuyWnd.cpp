#include "UIChooseSellBuyWnd.h"

#include "UIShopCommon.h"
#include "UIWindowMgr.h"
#include "gamemode/GameMode.h"
#include "gamemode/Mode.h"
#include "main/WinMain.h"
#include "qtui/QtUiRuntime.h"
#include "session/Session.h"

#include <windows.h>

#include <algorithm>
#include <string>

namespace {

constexpr int kWindowWidth = 180;
constexpr int kWindowHeight = 110;
constexpr int kButtonWidth = 76;
constexpr int kButtonHeight = 22;

const char* GetButtonLabel(UIChooseSellBuyWnd::ButtonId buttonId)
{
    switch (buttonId) {
    case UIChooseSellBuyWnd::ButtonBuy:
        return "Buy";
    case UIChooseSellBuyWnd::ButtonSell:
        return "Sell";
    case UIChooseSellBuyWnd::ButtonCancel:
        return "Cancel";
    default:
        return "";
    }
}

void DrawButton(HDC hdc, const RECT& rect, const char* label, bool hot, bool pressed)
{
    COLORREF fill = RGB(220, 220, 220);
    if (pressed) {
        fill = RGB(170, 185, 205);
    } else if (hot) {
        fill = RGB(196, 210, 228);
    }

    shopui::FillRectColor(hdc, rect, fill);
    shopui::FrameRectColor(hdc, rect, RGB(88, 88, 88));
    shopui::DrawWindowTextRect(hdc, rect, label ? label : "", RGB(24, 24, 24), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

} // namespace

UIChooseSellBuyWnd::UIChooseSellBuyWnd()
    : m_hoverButton(ButtonNone),
      m_pressedButton(ButtonNone),
      m_lastDrawStateToken(0ull),
      m_hasDrawStateToken(false)
{
    Create(kWindowWidth, kWindowHeight);
    Move(228, 146);
}

UIChooseSellBuyWnd::~UIChooseSellBuyWnd() = default;

void UIChooseSellBuyWnd::SetShow(int show)
{
    UIWindow::SetShow(show);
    if (show == 0) {
        m_hoverButton = ButtonNone;
        m_pressedButton = ButtonNone;
        return;
    }

    int savedX = m_x;
    int savedY = m_y;
    if (LoadUiWindowPlacement("ChooseSellBuyWnd", &savedX, &savedY)) {
        g_windowMgr.ClampWindowToClient(&savedX, &savedY, m_w, m_h);
        Move(savedX, savedY);
    } else if (g_hMainWnd) {
        RECT clientRect{};
        GetClientRect(g_hMainWnd, &clientRect);
        Move((clientRect.right - clientRect.left - m_w) / 2,
            (clientRect.bottom - clientRect.top - m_h) / 2);
    }
}

bool UIChooseSellBuyWnd::IsUpdateNeed()
{
    if (m_show == 0) {
        return false;
    }
    if (m_isDirty != 0 || !m_hasDrawStateToken) {
        return true;
    }
    return BuildDisplayStateToken() != m_lastDrawStateToken;
}

void UIChooseSellBuyWnd::StoreInfo()
{
    SaveUiWindowPlacement("ChooseSellBuyWnd", m_x, m_y);
}

RECT UIChooseSellBuyWnd::GetButtonRect(ButtonId buttonId) const
{
    switch (buttonId) {
    case ButtonBuy:
        return shopui::MakeRect(m_x + 14, m_y + 46, kButtonWidth, kButtonHeight);
    case ButtonSell:
        return shopui::MakeRect(m_x + 90, m_y + 46, kButtonWidth, kButtonHeight);
    case ButtonCancel:
        return shopui::MakeRect(m_x + (m_w - kButtonWidth) / 2, m_y + 74, kButtonWidth, kButtonHeight);
    default:
        return shopui::MakeRect(0, 0, 0, 0);
    }
}

UIChooseSellBuyWnd::ButtonId UIChooseSellBuyWnd::HitTestButton(int x, int y) const
{
    for (int id = ButtonBuy; id <= ButtonCancel; ++id) {
        const RECT rect = GetButtonRect(static_cast<ButtonId>(id));
        if (x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom) {
            return static_cast<ButtonId>(id);
        }
    }
    return ButtonNone;
}

void UIChooseSellBuyWnd::ActivateButton(ButtonId buttonId)
{
    switch (buttonId) {
    case ButtonBuy:
        g_modeMgr.SendMsg(CGameMode::GameMsg_RequestShopDealType, 0, 0, 0);
        break;
    case ButtonSell:
        g_modeMgr.SendMsg(CGameMode::GameMsg_RequestShopDealType, 1, 0, 0);
        break;
    case ButtonCancel:
        g_modeMgr.SendMsg(CGameMode::GameMsg_RequestShopCancel, 0, 0, 0);
        return;
    default:
        break;
    }
}

void UIChooseSellBuyWnd::OnDraw()
{
    if (IsQtUiRuntimeEnabled()) {
        m_lastDrawStateToken = BuildDisplayStateToken();
        m_hasDrawStateToken = true;
        return;
    }

    HDC hdc = AcquireDrawTarget();
    if (!hdc || m_show == 0) {
        return;
    }

    const RECT bounds = shopui::MakeRect(m_x, m_y, m_w, m_h);
    shopui::DrawFrameWindow(hdc, bounds, "Shop");

    const RECT textRect = shopui::MakeRect(m_x + 10, m_y + 24, m_w - 20, 18);
    shopui::DrawWindowTextRect(hdc, textRect, "Choose a transaction type.", RGB(44, 44, 44), DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    for (int id = ButtonBuy; id <= ButtonCancel; ++id) {
        const ButtonId buttonId = static_cast<ButtonId>(id);
        DrawButton(hdc,
            GetButtonRect(buttonId),
            GetButtonLabel(buttonId),
            m_hoverButton == buttonId,
            m_pressedButton == buttonId);
    }

    m_lastDrawStateToken = BuildDisplayStateToken();
    m_hasDrawStateToken = true;
    ReleaseDrawTarget(hdc);
}

void UIChooseSellBuyWnd::OnLBtnDown(int x, int y)
{
    m_pressedButton = HitTestButton(x, y);
    if (m_pressedButton == ButtonNone) {
        UIFrameWnd::OnLBtnDown(x, y);
    }
}

void UIChooseSellBuyWnd::OnLBtnUp(int x, int y)
{
    const ButtonId releasedButton = HitTestButton(x, y);
    const ButtonId pressedButton = m_pressedButton;
    m_pressedButton = ButtonNone;
    UIFrameWnd::OnLBtnUp(x, y);
    if (pressedButton != ButtonNone && pressedButton == releasedButton) {
        ActivateButton(pressedButton);
    }
}

void UIChooseSellBuyWnd::OnMouseMove(int x, int y)
{
    m_hoverButton = HitTestButton(x, y);
    UIFrameWnd::OnMouseMove(x, y);
}

void UIChooseSellBuyWnd::HandleKeyDown(int virtualKey)
{
    switch (virtualKey) {
    case 'B':
        ActivateButton(ButtonBuy);
        break;
    case 'S':
        ActivateButton(ButtonSell);
        break;
    case VK_RETURN:
        ActivateButton(ButtonBuy);
        break;
    case VK_ESCAPE:
        ActivateButton(ButtonCancel);
        break;
    default:
        break;
    }
}

UIChooseSellBuyWnd::ButtonId UIChooseSellBuyWnd::GetHoverButton() const
{
    return m_hoverButton;
}

UIChooseSellBuyWnd::ButtonId UIChooseSellBuyWnd::GetPressedButton() const
{
    return m_pressedButton;
}

unsigned long long UIChooseSellBuyWnd::BuildDisplayStateToken() const
{
    unsigned long long hash = 1469598103934665603ull;
    shopui::HashTokenValue(&hash, static_cast<unsigned long long>(m_show));
    shopui::HashTokenValue(&hash, static_cast<unsigned long long>(m_x));
    shopui::HashTokenValue(&hash, static_cast<unsigned long long>(m_y));
    shopui::HashTokenValue(&hash, static_cast<unsigned long long>(m_w));
    shopui::HashTokenValue(&hash, static_cast<unsigned long long>(m_h));
    shopui::HashTokenValue(&hash, static_cast<unsigned long long>(static_cast<int>(m_hoverButton) + 1));
    shopui::HashTokenValue(&hash, static_cast<unsigned long long>(static_cast<int>(m_pressedButton) + 1));
    shopui::HashTokenValue(&hash, static_cast<unsigned long long>(g_session.m_shopNpcId));
    return hash;
}
