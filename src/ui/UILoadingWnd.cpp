#include "UILoadingWnd.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>

namespace {

RECT MakeRect(int left, int top, int right, int bottom)
{
    RECT rect{ left, top, right, bottom };
    return rect;
}

void FillSolidRect(HDC hdc, const RECT& rect, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    if (!brush) {
        return;
    }
    FillRect(hdc, &rect, brush);
    DeleteObject(brush);
}

} // namespace

UILoadingWnd::UILoadingWnd()
    : m_progress(0.0f)
{
    m_message = "Loading...";
}

UILoadingWnd::~UILoadingWnd() = default;

void UILoadingWnd::SetProgress(float progress)
{
    m_progress = (std::max)(0.0f, (std::min)(1.0f, progress));
}

void UILoadingWnd::SetMessage(const std::string& message)
{
    if (!message.empty()) {
        m_message = message;
    }
}

float UILoadingWnd::GetProgress() const
{
    return m_progress;
}

const std::string& UILoadingWnd::GetMessage() const
{
    return m_message;
}

void UILoadingWnd::OnCreate(int x, int y)
{
    m_x = x;
    m_y = y;
}

void UILoadingWnd::OnDraw()
{
    HDC hdc = UIWindow::GetSharedDrawDC();
    if (!hdc || !g_hMainWnd) {
        return;
    }

    RECT clientRect{};
    GetClientRect(g_hMainWnd, &clientRect);
    const int width = clientRect.right - clientRect.left;
    const int height = clientRect.bottom - clientRect.top;
    if (width <= 0 || height <= 0) {
        return;
    }

    const int panelWidth = (std::min)(540, width - 80);
    const int panelHeight = 92;
    const int panelLeft = (width - panelWidth) / 2;
    const int panelTop = height - panelHeight - 42;
    const RECT shadowRect = MakeRect(panelLeft + 3, panelTop + 3, panelLeft + panelWidth + 3, panelTop + panelHeight + 3);
    const RECT panelRect = MakeRect(panelLeft, panelTop, panelLeft + panelWidth, panelTop + panelHeight);
    const RECT barBackRect = MakeRect(panelLeft + 24, panelTop + 48, panelLeft + panelWidth - 24, panelTop + 68);
    const int fillWidth = static_cast<int>((barBackRect.right - barBackRect.left - 4) * m_progress);
    const RECT barFillRect = MakeRect(barBackRect.left + 2, barBackRect.top + 2, barBackRect.left + 2 + fillWidth, barBackRect.bottom - 2);

    FillSolidRect(hdc, shadowRect, RGB(8, 12, 16));
    FillSolidRect(hdc, panelRect, RGB(25, 31, 38));
    FillSolidRect(hdc, barBackRect, RGB(50, 58, 68));
    if (fillWidth > 0) {
        FillSolidRect(hdc, barFillRect, RGB(230, 198, 88));
    }

    HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(194, 176, 103));
    HGDIOBJ oldPen = SelectObject(hdc, borderPen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(hdc, panelRect.left, panelRect.top, panelRect.right, panelRect.bottom);
    Rectangle(hdc, barBackRect.left, barBackRect.top, barBackRect.right, barBackRect.bottom);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(borderPen);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(245, 239, 221));

    HFONT titleFont = CreateFontA(-22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
        DEFAULT_PITCH | FF_SWISS, "Tahoma");
    HFONT bodyFont = CreateFontA(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
        DEFAULT_PITCH | FF_SWISS, "Tahoma");

    HGDIOBJ oldFont = SelectObject(hdc, titleFont);
    RECT titleRect = MakeRect(panelLeft + 22, panelTop + 10, panelLeft + panelWidth - 22, panelTop + 32);
    DrawTextA(hdc, "Entering World", -1, &titleRect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    SelectObject(hdc, bodyFont);
    RECT messageRect = MakeRect(panelLeft + 24, panelTop + 28, panelLeft + panelWidth - 24, panelTop + 48);
    DrawTextA(hdc, m_message.c_str(), -1, &messageRect, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

    char percentText[16] = {};
    std::snprintf(percentText, sizeof(percentText), "%d%%", static_cast<int>(m_progress * 100.0f + 0.5f));
    RECT percentRect = MakeRect(panelLeft + panelWidth - 70, panelTop + 10, panelLeft + panelWidth - 20, panelTop + 32);
    DrawTextA(hdc, percentText, -1, &percentRect, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);

    SelectObject(hdc, oldFont);
    DeleteObject(titleFont);
    DeleteObject(bodyFont);
}