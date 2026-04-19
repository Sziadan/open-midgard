#include "UIJoinPartyAcceptWnd.h"

#include "UIWindowMgr.h"
#include "gamemode/GameMode.h"
#include "main/WinMain.h"
#include "render/DC.h"

#include <windows.h>

namespace {

constexpr int kWindowWidth = 280;
constexpr int kWindowHeight = 132;
constexpr int kTitleBarHeight = 18;
constexpr int kMessageBoxTop = 28;
constexpr int kMessageBoxHeight = 52;
constexpr int kCloseButtonWidth = 14;
constexpr int kCloseButtonHeight = 12;
constexpr int kButtonWidth = 92;
constexpr int kButtonHeight = 24;

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

void DrawTextLine(HDC hdc, const RECT& rect, const char* text, COLORREF color, UINT format)
{
    if (!hdc || !text) {
        return;
    }

    const int oldBkMode = SetBkMode(hdc, TRANSPARENT);
    const COLORREF oldColor = SetTextColor(hdc, color);
    RECT drawRect = rect;
    DrawTextA(hdc, text, -1, &drawRect, format);
    SetTextColor(hdc, oldColor);
    SetBkMode(hdc, oldBkMode);
}

} // namespace

UIJoinPartyAcceptWnd::UIJoinPartyAcceptWnd()
    : m_partyId(0),
      m_hoverButtonId(0),
      m_pressedButtonId(0),
      m_lastVisualStateToken(0),
      m_hasVisualStateToken(false),
      m_controlsCreated(false)
{
    Create(kWindowWidth, kWindowHeight);
    UIWindow::SetShow(0);
}

UIJoinPartyAcceptWnd::~UIJoinPartyAcceptWnd() = default;

void UIJoinPartyAcceptWnd::SetShow(int show)
{
    UIWindow::SetShow(show);
    if (show == 0) {
        m_hoverButtonId = 0;
        m_pressedButtonId = 0;
    } else if (!m_controlsCreated) {
        OnCreate(0, 0);
    }
}

bool UIJoinPartyAcceptWnd::IsUpdateNeed()
{
    const unsigned long long token = BuildVisualStateToken();
    if (!m_hasVisualStateToken || m_lastVisualStateToken != token) {
        return true;
    }
    return UIWindow::IsUpdateNeed();
}

void UIJoinPartyAcceptWnd::OnCreate(int x, int y)
{
    (void)x;
    (void)y;
    if (m_controlsCreated) {
        return;
    }

    Create(kWindowWidth, kWindowHeight);
    m_id = UIWindowMgr::WID_JOINPARTYACCEPTWND;
    m_controlsCreated = true;

    if (g_hMainWnd) {
        RECT clientRect{};
        GetClientRect(g_hMainWnd, &clientRect);
        Move((clientRect.right - clientRect.left - m_w) / 2,
            (clientRect.bottom - clientRect.top - m_h) / 2);
    } else {
        Move(240, 120);
    }
}

void UIJoinPartyAcceptWnd::OnDraw()
{
    HDC hdc = AcquireDrawTarget();
    if (!hdc) {
        return;
    }

    DisplayData display{};
    GetDisplayDataForQt(&display);

    const RECT outer{ m_x, m_y, m_x + m_w, m_y + m_h };
    const RECT titleBar{ m_x + 1, m_y + 1, m_x + m_w - 1, m_y + kTitleBarHeight };
    const RECT content{ m_x + 1, m_y + kTitleBarHeight, m_x + m_w - 1, m_y + m_h - 1 };
    const RECT messageBox{ m_x + 14, m_y + kMessageBoxTop, m_x + m_w - 14, m_y + kMessageBoxTop + kMessageBoxHeight };

    FillRectColor(hdc, outer, RGB(232, 225, 210));
    FrameRectColor(hdc, outer, RGB(92, 82, 67));
    FillRectColor(hdc, titleBar, RGB(92, 111, 96));
    FrameRectColor(hdc, titleBar, RGB(60, 74, 62));
    FillRectColor(hdc, content, RGB(245, 240, 229));
    FillRectColor(hdc, messageBox, RGB(239, 232, 219));
    FrameRectColor(hdc, messageBox, RGB(165, 150, 125));

    const RECT titleTextRect{ m_x + 10, m_y + 1, m_x + m_w - 28, m_y + kTitleBarHeight };
    DrawTextLine(hdc, titleTextRect, display.title.c_str(), RGB(248, 248, 240), DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    const RECT closeRect = GetCloseButtonRect();
    FillRectColor(hdc, closeRect, RGB(210, 198, 180));
    FrameRectColor(hdc, closeRect, RGB(110, 96, 74));
    DrawTextLine(hdc, closeRect, "x", RGB(34, 34, 34), DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    const RECT bodyRect{ messageBox.left + 6, messageBox.top + 4, messageBox.right - 6, messageBox.bottom - 4 };
    DrawTextLine(hdc,
        bodyRect,
        display.bodyText.c_str(),
        RGB(56, 56, 56),
        DT_CENTER | DT_VCENTER | DT_WORDBREAK | DT_NOPREFIX);

    for (const QtButtonDisplay& button : display.buttons) {
        const RECT rect{ button.x, button.y, button.x + button.width, button.y + button.height };
        FillRectColor(hdc, rect, button.active ? RGB(212, 226, 239) : RGB(235, 229, 214));
        FrameRectColor(hdc, rect, RGB(121, 106, 79));
        DrawTextLine(hdc, rect, button.label.c_str(), RGB(28, 28, 28), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    ReleaseDrawTarget(hdc);
    m_lastVisualStateToken = BuildVisualStateToken();
    m_hasVisualStateToken = true;
}

void UIJoinPartyAcceptWnd::OnLBtnDown(int x, int y)
{
    m_pressedButtonId = HitTestButton(x, y);
    Invalidate();
}

void UIJoinPartyAcceptWnd::OnLBtnUp(int x, int y)
{
    const int releasedButtonId = HitTestButton(x, y);
    const int pressedButtonId = m_pressedButtonId;
    m_pressedButtonId = 0;

    if (pressedButtonId != 0 && pressedButtonId == releasedButtonId) {
        SubmitReply(releasedButtonId == ButtonAccept);
    }

    Invalidate();
}

void UIJoinPartyAcceptWnd::OnMouseMove(int x, int y)
{
    const int hoverButtonId = HitTestButton(x, y);
    if (m_hoverButtonId != hoverButtonId) {
        m_hoverButtonId = hoverButtonId;
        Invalidate();
    }
}

void UIJoinPartyAcceptWnd::OnKeyDown(int virtualKey)
{
    switch (virtualKey) {
    case VK_RETURN:
        SubmitReply(true);
        break;
    case VK_ESCAPE:
        SubmitReply(false);
        break;
    default:
        break;
    }
}

bool UIJoinPartyAcceptWnd::HandleKeyDown(int virtualKey)
{
    OnKeyDown(virtualKey);
    return true;
}

void UIJoinPartyAcceptWnd::OpenInvite(u32 partyId, const char* partyName)
{
    if (!m_controlsCreated) {
        OnCreate(0, 0);
    }

    m_partyId = partyId;
    m_partyName = partyName ? partyName : "";
    if (g_hMainWnd) {
        RECT clientRect{};
        GetClientRect(g_hMainWnd, &clientRect);
        Move((clientRect.right - clientRect.left - m_w) / 2,
            (clientRect.bottom - clientRect.top - m_h) / 2);
    }

    m_hoverButtonId = 0;
    m_pressedButtonId = 0;
    SetShow(1);
    Invalidate();
}

bool UIJoinPartyAcceptWnd::GetDisplayDataForQt(DisplayData* outData) const
{
    if (!outData) {
        return false;
    }

    outData->title = "Party Invitation";
    outData->bodyText = (m_partyName.empty() ? std::string("Party") : m_partyName) + "\nSuggest Join Party";
    outData->messageLines.clear();
    outData->messageLines.push_back(m_partyName.empty() ? "Party" : m_partyName);
    outData->messageLines.push_back("Suggest Join Party");

    outData->buttons.clear();
    for (int buttonId : { ButtonAccept, ButtonDecline }) {
        QtButtonDisplay button{};
        const RECT rect = GetButtonRect(buttonId);
        button.id = buttonId;
        button.x = rect.left;
        button.y = rect.top;
        button.width = rect.right - rect.left;
        button.height = rect.bottom - rect.top;
        button.label = buttonId == ButtonAccept ? "Accept" : "Decline";
        button.active = (m_pressedButtonId == buttonId) || (m_hoverButtonId == buttonId);
        outData->buttons.push_back(button);
    }

    return true;
}

RECT UIJoinPartyAcceptWnd::GetCloseButtonRect() const
{
    return RECT{ m_x + m_w - 18, m_y + 3, m_x + m_w - 18 + kCloseButtonWidth, m_y + 3 + kCloseButtonHeight };
}

RECT UIJoinPartyAcceptWnd::GetButtonRect(int buttonId) const
{
    const int baseY = m_y + m_h - 36;
    if (buttonId == ButtonAccept) {
        return RECT{ m_x + 26, baseY, m_x + 26 + kButtonWidth, baseY + kButtonHeight };
    }
    return RECT{ m_x + m_w - 26 - kButtonWidth, baseY, m_x + m_w - 26, baseY + kButtonHeight };
}

int UIJoinPartyAcceptWnd::HitTestButton(int x, int y) const
{
    if (IsPointInRect(GetCloseButtonRect(), x, y)) {
        return ButtonClose;
    }
    if (IsPointInRect(GetButtonRect(ButtonAccept), x, y)) {
        return ButtonAccept;
    }
    if (IsPointInRect(GetButtonRect(ButtonDecline), x, y)) {
        return ButtonDecline;
    }
    return 0;
}

void UIJoinPartyAcceptWnd::ClearState()
{
    m_partyId = 0;
    m_partyName.clear();
    m_hoverButtonId = 0;
    m_pressedButtonId = 0;
}

bool UIJoinPartyAcceptWnd::SubmitReply(bool accept)
{
    const u32 partyId = m_partyId;
    ClearState();
    SetShow(0);
    Invalidate();

    if (partyId == 0) {
        return false;
    }

    return g_modeMgr.SendMsg(CGameMode::GameMsg_RequestPartyInviteReply, partyId, accept ? 1 : 0, 0) != 0;
}

unsigned long long UIJoinPartyAcceptWnd::BuildVisualStateToken() const
{
    unsigned long long token = 1469598103934665603ull;
    const auto mix = [&token](unsigned long long value) {
        token ^= value;
        token *= 1099511628211ull;
    };

    mix(static_cast<unsigned long long>(m_show));
    mix(static_cast<unsigned long long>(m_x));
    mix(static_cast<unsigned long long>(m_y));
    mix(static_cast<unsigned long long>(m_partyId));
    for (unsigned char ch : m_partyName) {
        mix(static_cast<unsigned long long>(ch));
    }
    mix(static_cast<unsigned long long>(m_hoverButtonId));
    mix(static_cast<unsigned long long>(m_pressedButtonId));
    return token;
}