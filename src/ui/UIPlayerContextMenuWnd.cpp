#include "UIPlayerContextMenuWnd.h"

#include "NpcDialogColoredText.h"
#include "render/DC.h"
#include "UIWindowMgr.h"
#include "gamemode/GameMode.h"
#include "gamemode/Mode.h"
#include "main/WinMain.h"
#include "qtui/QtUiRuntime.h"

#include <windows.h>

#include <algorithm>
#include <cmath>

#if RO_ENABLE_QT6_UI
#include <QColor>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QString>
#endif

namespace {

constexpr int kMenuWidth = 320;
constexpr int kMenuBorder = 1;
constexpr int kMenuPaddingX = 8;
constexpr int kMenuPaddingY = 6;
constexpr int kMenuRowHeight = 20;
constexpr int kMenuCornerRadius = 8;

int GetMenuHeightForOptionCount(int optionCount)
{
    const int safeOptionCount = (std::max)(0, optionCount);
    return kMenuPaddingY * 2 + safeOptionCount * kMenuRowHeight;
}

HFONT GetPlayerContextMenuFont()
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
QFont BuildPlayerContextMenuFontFromHdc(HDC hdc)
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
    font.setPixelSize(logFont.lfHeight != 0 ? (std::max)(1, static_cast<int>(std::abs(logFont.lfHeight))) : 13);
    font.setBold(logFont.lfWeight >= FW_BOLD);
    font.setStyleStrategy(QFont::NoAntialias);
    return font;
}

void DrawPlayerContextMenuText(HDC hdc, const RECT& rect, const char* text, COLORREF color)
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
    painter.setFont(BuildPlayerContextMenuFontFromHdc(hdc));
    painter.setPen(QColor(GetRValue(color), GetGValue(color), GetBValue(color)));
    painter.drawText(QRect(0, 0, width, height), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, label);
    AlphaBlendArgbToHdc(hdc, rect.left, rect.top, width, height, pixels.data(), width, height);
}
#endif

} // namespace

UIPlayerContextMenuWnd::UIPlayerContextMenuWnd()
    : m_targetAid(0),
      m_selectedIndex(-1),
      m_hoverIndex(-1),
      m_pressedIndex(-1)
{
    Create(kMenuWidth, GetMenuHeightForOptionCount(0));
    UIWindow::SetShow(0);
}

UIPlayerContextMenuWnd::~UIPlayerContextMenuWnd() = default;

void UIPlayerContextMenuWnd::SetShow(int show)
{
    UIWindow::SetShow(show);
    if (show == 0) {
        m_hoverIndex = -1;
        m_pressedIndex = -1;
    }
}

void UIPlayerContextMenuWnd::StoreInfo()
{
}

u32 UIPlayerContextMenuWnd::GetTargetAid() const
{
    return m_targetAid;
}

const std::string& UIPlayerContextMenuWnd::GetTargetName() const
{
    return m_targetName;
}

const std::vector<std::string>& UIPlayerContextMenuWnd::GetOptions() const
{
    return m_optionLabels;
}

int UIPlayerContextMenuWnd::GetSelectedIndex() const
{
    return m_selectedIndex;
}

int UIPlayerContextMenuWnd::GetHoverIndex() const
{
    return m_hoverIndex;
}

bool UIPlayerContextMenuWnd::IsOkPressed() const
{
    return false;
}

bool UIPlayerContextMenuWnd::IsCancelPressed() const
{
    return false;
}

bool UIPlayerContextMenuWnd::GetOkRectForQt(RECT* outRect) const
{
    if (outRect) {
        *outRect = RECT{};
    }
    return false;
}

bool UIPlayerContextMenuWnd::GetCancelRectForQt(RECT* outRect) const
{
    if (outRect) {
        *outRect = RECT{};
    }
    return false;
}

RECT UIPlayerContextMenuWnd::GetOptionRect(int index) const
{
    return MakeRect(m_x + kMenuPaddingX,
        m_y + kMenuPaddingY + index * kMenuRowHeight,
        m_w - kMenuPaddingX * 2,
        kMenuRowHeight);
}

int UIPlayerContextMenuWnd::GetOptionIndexAtPoint(int x, int y) const
{
    if (x < m_x || x >= m_x + m_w || y < m_y || y >= m_y + m_h) {
        return -1;
    }

    const int localY = y - m_y - kMenuPaddingY;
    if (localY < 0) {
        return -1;
    }

    const int index = localY / kMenuRowHeight;
    return (index >= 0 && index < static_cast<int>(m_entries.size())) ? index : -1;
}

void UIPlayerContextMenuWnd::SetHighlightedIndex(int index)
{
    if (index == m_selectedIndex && index == m_hoverIndex) {
        return;
    }

    m_selectedIndex = index;
    m_hoverIndex = index;
    Invalidate();
}

void UIPlayerContextMenuWnd::OnDraw()
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
    RECT inner = MakeRect(m_x + kMenuBorder, m_y + kMenuBorder, m_w - 2 * kMenuBorder, m_h - 2 * kMenuBorder);
    FillRoundedRect(hdc, outer, RGB(130, 130, 130), RGB(130, 130, 130), kMenuCornerRadius);
    FillRoundedRect(hdc, inner, RGB(248, 248, 248), RGB(224, 224, 224), kMenuCornerRadius - 2);

    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, GetPlayerContextMenuFont()));
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(0, 0, 0));

    for (size_t index = 0; index < m_optionLabels.size(); ++index) {
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
#if RO_ENABLE_QT6_UI
        DrawPlayerContextMenuText(hdc, textRect, m_optionLabels[index].c_str(), RGB(0, 0, 0));
#else
        DrawNpcMenuOptionColoredText(hdc, textRect, m_optionLabels[index]);
#endif
    }

    SelectObject(hdc, oldFont);
    ReleaseDrawTarget(hdc);
}

void UIPlayerContextMenuWnd::SetMenu(u32 targetAid,
    const std::string& targetName,
    const std::vector<PlayerContextMenuEntry>& options,
    int screenX,
    int screenY)
{
    m_targetAid = targetAid;
    m_targetName = targetName;
    m_entries = options;
    m_optionLabels.clear();
    m_optionLabels.reserve(m_entries.size());
    for (const PlayerContextMenuEntry& entry : m_entries) {
        m_optionLabels.push_back(entry.label);
    }

    m_selectedIndex = m_entries.empty() ? -1 : 0;
    m_hoverIndex = -1;
    m_pressedIndex = -1;
    Resize(kMenuWidth, GetMenuHeightForOptionCount(static_cast<int>(m_entries.size())));
    Move(screenX, screenY);
    SetShow(1);
    Invalidate();
}

void UIPlayerContextMenuWnd::HideMenu()
{
    m_targetAid = 0;
    m_targetName.clear();
    m_entries.clear();
    m_optionLabels.clear();
    m_selectedIndex = -1;
    m_hoverIndex = -1;
    m_pressedIndex = -1;
    SetShow(0);
}

void UIPlayerContextMenuWnd::SubmitSelection(int index)
{
    if (index < 0 || index >= static_cast<int>(m_entries.size())) {
        return;
    }

    const u32 targetAid = m_targetAid;
    const int actionId = m_entries[static_cast<size_t>(index)].actionId;
    HideMenu();
    PlayUiButtonSound();
    g_modeMgr.SendMsg(CGameMode::GameMsg_RequestPlayerContextAction, targetAid, actionId, 0);
}

bool UIPlayerContextMenuWnd::HandleKeyDown(int virtualKey)
{
    if (m_show == 0) {
        return false;
    }

    switch (virtualKey) {
    case VK_UP:
        if (!m_entries.empty()) {
            const int count = static_cast<int>(m_entries.size());
            const int nextIndex = (m_selectedIndex <= 0) ? (count - 1) : (m_selectedIndex - 1);
            SetHighlightedIndex(nextIndex);
        }
        return true;

    case VK_DOWN:
        if (!m_entries.empty()) {
            const int count = static_cast<int>(m_entries.size());
            const int baseIndex = (m_selectedIndex < 0) ? 0 : m_selectedIndex;
            SetHighlightedIndex((baseIndex + 1) % count);
        }
        return true;

    case VK_RETURN:
        if (m_selectedIndex >= 0) {
            SubmitSelection(m_selectedIndex);
        }
        return true;

    case VK_ESCAPE:
        HideMenu();
        return true;

    default:
        return false;
    }
}

void UIPlayerContextMenuWnd::OnLBtnDown(int x, int y)
{
    m_pressedIndex = GetOptionIndexAtPoint(x, y);
    if (m_pressedIndex >= 0) {
        SetHighlightedIndex(m_pressedIndex);
    }
}

void UIPlayerContextMenuWnd::OnMouseMove(int x, int y)
{
    const int hoverIndex = GetOptionIndexAtPoint(x, y);
    if (hoverIndex != m_hoverIndex) {
        m_hoverIndex = hoverIndex;
        if (hoverIndex >= 0) {
            m_selectedIndex = hoverIndex;
        }
        Invalidate();
    }
}

void UIPlayerContextMenuWnd::OnMouseHover(int x, int y)
{
    OnMouseMove(x, y);
}

void UIPlayerContextMenuWnd::OnLBtnUp(int x, int y)
{
    const int pressedIndex = m_pressedIndex;
    m_pressedIndex = -1;
    if (pressedIndex < 0) {
        return;
    }

    const int releasedIndex = GetOptionIndexAtPoint(x, y);
    if (releasedIndex == pressedIndex) {
        SubmitSelection(releasedIndex);
        return;
    }

    Invalidate();
}