#include "UISelectServerWnd.h"

#include "UILoginWnd.h"
#include "core/ClientInfoLocale.h"
#include "gamemode/LoginMode.h"
#include "gamemode/Mode.h"
#include "main/WinMain.h"
#include "qtui/QtUiRuntime.h"
#include "render/DC.h"
#include "render/DrawUtil.h"
#include "ui/UiScale.h"
#include "ui/UIWindowMgr.h"

#if RO_ENABLE_QT6_UI
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QString>
#endif

#include <algorithm>

namespace {

constexpr int kWindowWidth = 280;
constexpr int kTitleHeight = 20;
constexpr int kRowHeight = 24;
constexpr int kWindowPadding = 10;
constexpr int kBottomListPadding = 8;
constexpr int kWindowGapAboveLogin = 10;

int GetMenuLayoutClientExtent(int rawExtent)
{
#if RO_ENABLE_QT6_UI
    if (IsQtUiRuntimeEnabled()) {
        return UiScaleRawToLogicalCoordinate(rawExtent);
    }
#endif
    return rawExtent;
}

#if RO_ENABLE_QT6_UI
QFont BuildSelectServerFontFromHdc(HDC hdc)
{
    LOGFONTA logFont{};
    if (hdc) {
        if (HGDIOBJ fontObject = GetCurrentObject(hdc, OBJ_FONT)) {
            GetObjectA(fontObject, sizeof(logFont), &logFont);
        }
    }

    const QString family = logFont.lfFaceName[0] != '\0'
        ? QString::fromLocal8Bit(logFont.lfFaceName)
        : QStringLiteral("MS Sans Serif");
    QFont font(family);
    font.setPixelSize(logFont.lfHeight != 0 ? (std::max)(1, static_cast<int>(std::abs(logFont.lfHeight))) : 14);
    font.setBold(logFont.lfWeight >= FW_BOLD);
    font.setStyleStrategy(QFont::NoAntialias);
    return font;
}

void DrawSelectServerText(HDC hdc, int x, int y, const std::string& text, COLORREF color)
{
    if (!hdc || text.empty()) {
        return;
    }

    const QString label = QString::fromLocal8Bit(text.c_str());
    if (label.isEmpty()) {
        return;
    }

    const QFont font = BuildSelectServerFontFromHdc(hdc);
    const QFontMetrics metrics(font);
    const int width = (std::max)(1, metrics.horizontalAdvance(label) + 2);
    const int height = (std::max)(1, metrics.height() + 2);
    std::vector<unsigned int> pixels(static_cast<size_t>(width) * static_cast<size_t>(height), 0u);
    QImage image(reinterpret_cast<uchar*>(pixels.data()), width, height, width * static_cast<int>(sizeof(unsigned int)), QImage::Format_ARGB32);
    if (image.isNull()) {
        return;
    }

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setRenderHint(QPainter::TextAntialiasing, false);
    painter.setFont(font);
    painter.setPen(QColor(GetRValue(color), GetGValue(color), GetBValue(color)));
    painter.drawText(0, metrics.ascent(), label);
    AlphaBlendArgbToHdc(hdc, x, y, width, height, pixels.data(), width, height);
}
#endif

}

UISelectServerWnd::UISelectServerWnd()
    : m_controlsCreated(false),
      m_hoverIndex(-1)
{
    Create(kWindowWidth, ComputeWindowHeight());
}

UISelectServerWnd::~UISelectServerWnd() = default;

void UISelectServerWnd::SetShow(int show)
{
    UIWindow::SetShow(show);
    if (show == 0) {
        m_hoverIndex = -1;
        m_entryRects.clear();
        return;
    }

    EnsureCreated();
    SyncGeometry();
}

void UISelectServerWnd::EnsureCreated()
{
    if (m_controlsCreated || !g_hMainWnd) {
        return;
    }

    RECT clientRect{};
    GetClientRect(g_hMainWnd, &clientRect);
    OnCreate(clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);
}

int UISelectServerWnd::ComputeWindowHeight() const
{
    const int rows = (std::max)(0, GetClientInfoConnectionCount());
    return kWindowPadding * 2 + kTitleHeight + rows * kRowHeight + kBottomListPadding;
}

void UISelectServerWnd::SyncGeometry()
{
    const int desiredHeight = ComputeWindowHeight();
    Resize(kWindowWidth, desiredHeight);

    RECT clientRect{};
    if (!g_hMainWnd || !GetClientRect(g_hMainWnd, &clientRect)) {
        return;
    }

    const int logicalW = GetMenuLayoutClientExtent(clientRect.right - clientRect.left);
    const int logicalH = GetMenuLayoutClientExtent(clientRect.bottom - clientRect.top);
    int x = (logicalW - m_w) / 2;
    int y = (logicalH - m_h) / 2;
    if (g_windowMgr.m_loginWnd) {
        x = g_windowMgr.m_loginWnd->m_x;
        if (IsQtUiRuntimeEnabled()) {
            x += (g_windowMgr.m_loginWnd->m_w - m_w) / 2;
        }
        y = g_windowMgr.m_loginWnd->m_y - m_h - kWindowGapAboveLogin;
    }

    if (y < 8) {
        y = 8;
    }
    Move(x, y);
    RebuildEntryRects();
}

void UISelectServerWnd::OnCreate(int cx, int cy)
{
    if (m_controlsCreated) {
        SyncGeometry();
        return;
    }
    m_controlsCreated = true;

    Create(kWindowWidth, ComputeWindowHeight());
    Move((GetMenuLayoutClientExtent(cx) - m_w) / 2, (GetMenuLayoutClientExtent(cy) - m_h) / 2);
    SyncGeometry();
}

int UISelectServerWnd::HitTestEntry(int x, int y) const
{
    for (size_t index = 0; index < m_entryRects.size(); ++index) {
        const RECT& rect = m_entryRects[index];
        if (x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

bool UISelectServerWnd::HandleQtMouseDown(int x, int y)
{
    if (!g_hMainWnd || m_show == 0 || GetClientInfoConnectionCount() <= 1) {
        return false;
    }

    EnsureCreated();
    SyncGeometry();
    RebuildEntryRects();
    if (x < m_x || y < m_y || x >= m_x + m_w || y >= m_y + m_h) {
        return false;
    }

    const int index = HitTestEntry(x, y);
    if (index >= 0) {
        PlayUiButtonSound();
        g_modeMgr.SendMsg(CLoginMode::LoginMsg_SelectClientInfo, index, 0, 0);
    }
    return true;
}

void UISelectServerWnd::RebuildEntryRects()
{
    m_entryRects.clear();

    const std::vector<ClientInfoConnection>& connections = GetClientInfoConnections();
    const int listLeft = m_x + kWindowPadding;
    const int listTop = m_y + kWindowPadding + kTitleHeight;
    const int listWidth = m_w - kWindowPadding * 2;

    for (size_t index = 0; index < connections.size(); ++index) {
        RECT rowRect = {
            listLeft,
            listTop + static_cast<int>(index) * kRowHeight,
            listLeft + listWidth,
            listTop + static_cast<int>(index + 1) * kRowHeight - 2
        };
        m_entryRects.push_back(rowRect);
    }
}

void UISelectServerWnd::OnDraw()
{
    if (!g_hMainWnd || m_show == 0 || GetClientInfoConnectionCount() <= 1) {
        return;
    }

    EnsureCreated();
    SyncGeometry();
    RebuildEntryRects();

    if (IsQtUiRuntimeEnabled()) {
        return;
    }

    HDC hdc = AcquireDrawTarget();
    if (!hdc) {
        return;
    }

    RECT panel = { m_x, m_y, m_x + m_w, m_y + m_h };
    HBRUSH panelBrush = CreateSolidBrush(RGB(243, 240, 231));
    HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(120, 112, 96));
    HGDIOBJ oldBrush = SelectObject(hdc, panelBrush);
    HGDIOBJ oldPen = SelectObject(hdc, borderPen);
    RoundRect(hdc, panel.left, panel.top, panel.right, panel.bottom, 8, 8);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(borderPen);
    DeleteObject(panelBrush);

    DrawDC drawDc(hdc);
    drawDc.SetFont(FONT_DEFAULT, 14, 1);
    SetBkMode(hdc, TRANSPARENT);
    const char* title = "Select Server";
#if RO_ENABLE_QT6_UI
    DrawSelectServerText(hdc, m_x + kWindowPadding, m_y + kWindowPadding - 1, title, RGB(48, 48, 48));
#else
    drawDc.SetTextColor(RGB(48, 48, 48));
    drawDc.TextOutA(m_x + kWindowPadding, m_y + kWindowPadding - 1, title, 13);
#endif

    const std::vector<ClientInfoConnection>& connections = GetClientInfoConnections();
    const int selectedIndex = GetSelectedClientInfoIndex();
    const int listLeft = m_x + kWindowPadding;
    const int listTop = m_y + kWindowPadding + kTitleHeight;

    for (size_t index = 0; index < connections.size(); ++index) {
        const RECT& rowRect = m_entryRects[index];

        COLORREF fill = RGB(250, 248, 242);
        if (static_cast<int>(index) == selectedIndex) {
            fill = RGB(214, 224, 198);
        } else if (static_cast<int>(index) == m_hoverIndex) {
            fill = RGB(229, 233, 224);
        }

        HBRUSH fillBrush = CreateSolidBrush(fill);
        FillRect(hdc, &rowRect, fillBrush);
        DeleteObject(fillBrush);
        FrameRect(hdc, &rowRect, reinterpret_cast<HBRUSH>(GetStockObject(GRAY_BRUSH)));

        const ClientInfoConnection& info = connections[index];
        const std::string label = !info.display.empty() ? info.display : info.address;
        const std::string detail = !info.desc.empty() ? info.desc : info.port;
#if RO_ENABLE_QT6_UI
        DrawSelectServerText(hdc, rowRect.left + 6, rowRect.top + 4, label, RGB(24, 24, 24));
#else
        drawDc.SetTextColor(RGB(24, 24, 24));
        drawDc.TextOutA(rowRect.left + 6, rowRect.top + 4, label.c_str(), static_cast<int>(label.size()));
#endif
        if (!detail.empty()) {
            const int detailX = (std::max)(rowRect.left + 90, rowRect.right - 8 - static_cast<int>(detail.size()) * 6);
#if RO_ENABLE_QT6_UI
            DrawSelectServerText(hdc, detailX, rowRect.top + 4, detail, RGB(96, 96, 96));
#else
            drawDc.SetTextColor(RGB(96, 96, 96));
            drawDc.TextOutA(detailX, rowRect.top + 4, detail.c_str(), static_cast<int>(detail.size()));
#endif
        }
    }

    DrawChildrenToHdc(hdc);
    ReleaseDrawTarget(hdc);
}

void UISelectServerWnd::OnLBtnDown(int x, int y)
{
    RebuildEntryRects();
    const int index = HitTestEntry(x, y);
    if (index >= 0) {
        PlayUiButtonSound();
        g_modeMgr.SendMsg(CLoginMode::LoginMsg_SelectClientInfo, index, 0, 0);
        return;
    }

    UIFrameWnd::OnLBtnDown(x, y);
}

void UISelectServerWnd::OnLBtnDblClk(int x, int y)
{
    OnLBtnDown(x, y);
}

void UISelectServerWnd::OnMouseMove(int x, int y)
{
    RebuildEntryRects();
    m_hoverIndex = HitTestEntry(x, y);
    UIFrameWnd::OnMouseMove(x, y);
}

void UISelectServerWnd::OnKeyDown(int virtualKey)
{
    const int count = GetClientInfoConnectionCount();
    if (count <= 1) {
        return;
    }

    if (virtualKey == VK_UP || virtualKey == VK_DOWN) {
        int index = GetSelectedClientInfoIndex();
        if (virtualKey == VK_UP) {
            index = (index <= 0) ? (count - 1) : (index - 1);
        } else {
            index = (index + 1) % count;
        }
        g_modeMgr.SendMsg(CLoginMode::LoginMsg_SelectClientInfo, index, 0, 0);
    }
}
