#include "UINpcMenuWnd.h"

#include "NpcDialogColoredText.h"
#include "render/DC.h"
#include "UIWindowMgr.h"
#include "gamemode/GameMode.h"
#include "gamemode/Mode.h"
#include "main/WinMain.h"
#include "qtui/QtUiRuntime.h"

#include <windows.h>

#if RO_ENABLE_QT6_UI
#include <QFont>
#include <QFontMetrics>
#include <QImage>
#include <QPainter>
#include <QString>
#endif

namespace {

constexpr int kMenuWidth = 250;
constexpr int kMenuHeight = 164;
constexpr int kBorder = 1;
constexpr int kPadding = 10;
constexpr int kOptionHeight = 18;
constexpr int kButtonWidth = 68;
constexpr int kButtonHeight = 22;
constexpr int kCornerRadius = 10;

HFONT GetNpcMenuFont()
{
    static HFONT s_font = nullptr;
    if (!s_font) {
        s_font = CreateFontA(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            NONANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "MS Sans Serif");
    }
    return s_font;
}

void FillSolidRect(HDC hdc, const RECT& rect, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(hdc, &rect, brush);
    DeleteObject(brush);
}

void DrawRectOutline(HDC hdc, const RECT& rect, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    FrameRect(hdc, &rect, brush);
    DeleteObject(brush);
}

void FillRoundedRect(HDC hdc, const RECT& rect, COLORREF fillColor, COLORREF borderColor, int radius)
{
    HBRUSH brush = CreateSolidBrush(fillColor);
    HPEN pen = CreatePen(PS_SOLID, 1, borderColor);
    HGDIOBJ oldBrush = SelectObject(hdc, brush);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);
}

RECT MakeRect(int left, int top, int width, int height)
{
    RECT rect{ left, top, left + width, top + height };
    return rect;
}

#if RO_ENABLE_QT6_UI
QFont BuildNpcMenuFontFromHdc(HDC hdc)
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
    font.setPixelSize(logFont.lfHeight != 0 ? (std::max)(1, std::abs(logFont.lfHeight)) : 13);
    font.setBold(logFont.lfWeight >= FW_BOLD);
    font.setStyleStrategy(QFont::NoAntialias);
    return font;
}

void DrawNpcMenuText(HDC hdc, const RECT& rect, const char* text, COLORREF color, Qt::Alignment alignment)
{
    if (!hdc || !text || rect.right <= rect.left || rect.bottom <= rect.top) {
        return;
    }

    const QString label = QString::fromLocal8Bit(text);
    if (label.isEmpty()) {
        return;
    }

    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    std::vector<unsigned int> pixels(static_cast<size_t>(width) * static_cast<size_t>(height), 0u);
    QImage image(reinterpret_cast<uchar*>(pixels.data()), width, height, width * static_cast<int>(sizeof(unsigned int)), QImage::Format_ARGB32);
    if (image.isNull()) {
        return;
    }

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setRenderHint(QPainter::TextAntialiasing, false);
    painter.setFont(BuildNpcMenuFontFromHdc(hdc));
    painter.setPen(QColor(GetRValue(color), GetGValue(color), GetBValue(color)));
    painter.drawText(QRect(0, 0, width, height), alignment | Qt::TextSingleLine, label);
    AlphaBlendArgbToHdc(hdc, rect.left, rect.top, width, height, pixels.data(), width, height);
}
#endif

} // namespace

UINpcMenuWnd::UINpcMenuWnd()
    : m_npcId(0),
      m_selectedIndex(0),
      m_hoverIndex(-1),
      m_pressedTarget(ClickTarget::None)
{
    Create(kMenuWidth, kMenuHeight);
    int defaultX = 195;
    int defaultY = 215;
    LoadUiWindowPlacement("NpcMenuDialog", &defaultX, &defaultY);
    g_windowMgr.ClampWindowToClient(&defaultX, &defaultY, m_w, m_h);
    UIWindow::Move(defaultX, defaultY);
    UIWindow::SetShow(0);
}

UINpcMenuWnd::~UINpcMenuWnd() = default;

void UINpcMenuWnd::SetShow(int show)
{
    UIWindow::SetShow(show);
    if (show == 0) {
        m_isDragging = 0;
        m_hoverIndex = -1;
        m_pressedTarget = ClickTarget::None;
    }
}

void UINpcMenuWnd::Move(int x, int y)
{
    g_windowMgr.ClampWindowToClient(&x, &y, m_w, m_h);
    UIWindow::Move(x, y);
}

void UINpcMenuWnd::StoreInfo()
{
    SaveUiWindowPlacement("NpcMenuDialog", m_x, m_y);
}

u32 UINpcMenuWnd::GetNpcId() const
{
    return m_npcId;
}

const std::vector<std::string>& UINpcMenuWnd::GetOptions() const
{
    return m_options;
}

int UINpcMenuWnd::GetSelectedIndex() const
{
    return m_selectedIndex;
}

int UINpcMenuWnd::GetHoverIndex() const
{
    return m_hoverIndex;
}

bool UINpcMenuWnd::IsOkPressed() const
{
    return m_pressedTarget == ClickTarget::Ok;
}

bool UINpcMenuWnd::IsCancelPressed() const
{
    return m_pressedTarget == ClickTarget::Cancel;
}

bool UINpcMenuWnd::GetOkRectForQt(RECT* outRect) const
{
    if (!outRect) {
        return false;
    }
    *outRect = GetOkRect();
    return true;
}

bool UINpcMenuWnd::GetCancelRectForQt(RECT* outRect) const
{
    if (!outRect) {
        return false;
    }
    *outRect = GetCancelRect();
    return true;
}

RECT UINpcMenuWnd::GetOptionRect(int index) const
{
    return MakeRect(m_x + kPadding,
        m_y + kPadding + index * kOptionHeight,
        m_w - kPadding * 2,
        kOptionHeight);
}

RECT UINpcMenuWnd::GetOkRect() const
{
    return MakeRect(m_x + m_w - kPadding - kButtonWidth * 2 - 8,
        m_y + m_h - kPadding - kButtonHeight,
        kButtonWidth,
        kButtonHeight);
}

RECT UINpcMenuWnd::GetCancelRect() const
{
    return MakeRect(m_x + m_w - kPadding - kButtonWidth,
        m_y + m_h - kPadding - kButtonHeight,
        kButtonWidth,
        kButtonHeight);
}

bool UINpcMenuWnd::IsPointInRect(const RECT& rect, int x, int y) const
{
    return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
}

void UINpcMenuWnd::StartDragging(int x, int y)
{
    m_isDragging = 1;
    m_startGlobalX = x;
    m_startGlobalY = y;
    m_orgX = m_x;
    m_orgY = m_y;
}

void UINpcMenuWnd::StopDragging()
{
    if (m_isDragging == 0) {
        return;
    }
    m_isDragging = 0;
    StoreInfo();
}

void UINpcMenuWnd::DrawButton(HDC hdc, const RECT& rect, const char* label, bool hovered, bool pressed) const
{
    FillSolidRect(hdc, rect, pressed ? RGB(196, 196, 196) : (hovered ? RGB(228, 228, 228) : RGB(240, 240, 240)));
    DrawRectOutline(hdc, rect, RGB(110, 110, 110));
    RECT textRect = rect;
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(0, 0, 0));
#if RO_ENABLE_QT6_UI
    DrawNpcMenuText(hdc, textRect, label, RGB(0, 0, 0), Qt::AlignCenter | Qt::AlignVCenter);
#else
    DrawTextA(hdc, label, -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
#endif
}

void UINpcMenuWnd::OnDraw()
{
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

    RECT outer = MakeRect(m_x, m_y, m_w, m_h);
    RECT inner = MakeRect(m_x + kBorder, m_y + kBorder, m_w - 2 * kBorder, m_h - 2 * kBorder);
    FillRoundedRect(hdc, outer, RGB(130, 130, 130), RGB(130, 130, 130), kCornerRadius);
    FillRoundedRect(hdc, inner, RGB(248, 248, 248), RGB(224, 224, 224), kCornerRadius - 2);

    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, GetNpcMenuFont()));
    SetBkMode(hdc, TRANSPARENT);
    for (size_t index = 0; index < m_options.size(); ++index) {
        const RECT rowRect = GetOptionRect(static_cast<int>(index));
        const bool selected = static_cast<int>(index) == m_selectedIndex;
        const bool hovered = static_cast<int>(index) == m_hoverIndex;
        if (selected || hovered) {
            FillSolidRect(hdc, rowRect, selected ? RGB(209, 224, 244) : RGB(232, 239, 248));
            DrawRectOutline(hdc, rowRect, RGB(160, 180, 205));
        }

        RECT textRect = rowRect;
        textRect.left += 4;
        textRect.right -= 4;
        DrawNpcMenuOptionColoredText(hdc, textRect, m_options[index]);
    }

    const bool okPressed = m_pressedTarget == ClickTarget::Ok;
    const bool cancelPressed = m_pressedTarget == ClickTarget::Cancel;
    DrawButton(hdc, GetOkRect(), "OK", false, okPressed);
    DrawButton(hdc, GetCancelRect(), "Cancel", false, cancelPressed);

    SelectObject(hdc, oldFont);
    ReleaseDrawTarget(hdc);
}

void UINpcMenuWnd::SetMenu(u32 npcId, const std::vector<std::string>& options)
{
    m_npcId = npcId;
    m_options = options;
    m_selectedIndex = m_options.empty() ? -1 : 0;
    m_hoverIndex = -1;
    m_pressedTarget = ClickTarget::None;
    SetShow(1);
    Invalidate();
}

void UINpcMenuWnd::HideMenu()
{
    m_npcId = 0;
    m_options.clear();
    m_selectedIndex = -1;
    m_hoverIndex = -1;
    m_pressedTarget = ClickTarget::None;
    SetShow(0);
}

void UINpcMenuWnd::SubmitSelection(u8 choice)
{
    if (m_npcId == 0 || choice == 0) {
        return;
    }
    const u32 npcId = m_npcId;
    if (choice == 0xFF) {
        g_windowMgr.CloseNpcDialogWindows();
    } else {
        HideMenu();
    }
    PlayUiButtonSound();
    g_modeMgr.SendMsg(CGameMode::GameMsg_RequestNpcSelectMenu, npcId, choice, 0);
}

bool UINpcMenuWnd::HandleKeyDown(int virtualKey)
{
    if (m_show == 0) {
        return false;
    }

    switch (virtualKey) {
    case VK_UP:
        if (!m_options.empty()) {
            m_selectedIndex = (m_selectedIndex <= 0) ? static_cast<int>(m_options.size() - 1) : (m_selectedIndex - 1);
            Invalidate();
        }
        break;

    case VK_DOWN:
        if (!m_options.empty()) {
            m_selectedIndex = (m_selectedIndex + 1) % static_cast<int>(m_options.size());
            Invalidate();
        }
        break;

    case VK_RETURN:
        if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_options.size())) {
            SubmitSelection(static_cast<u8>(m_selectedIndex + 1));
        }
        break;

    case VK_ESCAPE:
        SubmitSelection(0xFF);
        break;

    default:
        break;
    }

    return true;
}

void UINpcMenuWnd::OnLBtnDown(int x, int y)
{
    if (m_show == 0) {
        return;
    }

    if (IsPointInRect(GetOkRect(), x, y)) {
        m_pressedTarget = ClickTarget::Ok;
        Invalidate();
        return;
    }
    if (IsPointInRect(GetCancelRect(), x, y)) {
        m_pressedTarget = ClickTarget::Cancel;
        Invalidate();
        return;
    }

    for (size_t index = 0; index < m_options.size(); ++index) {
        if (IsPointInRect(GetOptionRect(static_cast<int>(index)), x, y)) {
            m_selectedIndex = static_cast<int>(index);
            m_pressedTarget = ClickTarget::Option;
            Invalidate();
            return;
        }
    }

    if (IsPointInRect(MakeRect(m_x, m_y, m_w, m_h), x, y)) {
        StartDragging(x, y);
    }
}

void UINpcMenuWnd::OnMouseMove(int x, int y)
{
    if (m_isDragging != 0) {
        int snappedX = m_orgX + (x - m_startGlobalX);
        int snappedY = m_orgY + (y - m_startGlobalY);
        g_windowMgr.SnapWindowToNearby(this, &snappedX, &snappedY);
        Move(snappedX, snappedY);
        return;
    }

    int hoverIndex = -1;
    for (size_t index = 0; index < m_options.size(); ++index) {
        if (IsPointInRect(GetOptionRect(static_cast<int>(index)), x, y)) {
            hoverIndex = static_cast<int>(index);
            break;
        }
    }
    if (m_hoverIndex != hoverIndex) {
        m_hoverIndex = hoverIndex;
        Invalidate();
    }
}

void UINpcMenuWnd::OnLBtnUp(int x, int y)
{
    if (m_show == 0) {
        return;
    }

    if (m_isDragging != 0) {
        StopDragging();
        return;
    }

    const ClickTarget pressedTarget = m_pressedTarget;
    m_pressedTarget = ClickTarget::None;
    Invalidate();
    if (pressedTarget == ClickTarget::None) {
        return;
    }

    if (pressedTarget == ClickTarget::Ok && IsPointInRect(GetOkRect(), x, y)) {
        if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_options.size())) {
            SubmitSelection(static_cast<u8>(m_selectedIndex + 1));
        }
        return;
    }

    if (pressedTarget == ClickTarget::Cancel && IsPointInRect(GetCancelRect(), x, y)) {
        SubmitSelection(0xFF);
        return;
    }

    if (pressedTarget == ClickTarget::Option) {
        for (size_t index = 0; index < m_options.size(); ++index) {
            if (IsPointInRect(GetOptionRect(static_cast<int>(index)), x, y)) {
                m_selectedIndex = static_cast<int>(index);
                Invalidate();
                return;
            }
        }
    }
}
