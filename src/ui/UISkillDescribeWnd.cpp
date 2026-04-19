#include "UISkillDescribeWnd.h"

#include "UIWindowMgr.h"
#include "session/Session.h"
#include "skill/Skill.h"

#if RO_ENABLE_QT6_UI
#include <QColor>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QString>
#endif

#include <algorithm>
#include <cctype>
#include <sstream>

namespace {
constexpr int kWindowWidth = 292;
constexpr int kWindowHeight = 220;
constexpr int kTitleBarHeight = 17;
constexpr int kCloseButtonSize = 12;
constexpr int kPadding = 8;
constexpr int kIconBoxSize = 52;
constexpr int kDescriptionTop = 88;
constexpr int kDescriptionScrollBarWidth = 10;
constexpr int kDescriptionScrollBarGap = 4;
constexpr int kDescriptionWheelStep = 24;

bool IsInsideRect(const RECT& rect, int x, int y)
{
    return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
}

bool IsHexDigit(char ch)
{
    return std::isxdigit(static_cast<unsigned char>(ch)) != 0;
}

std::string SanitizeRoText(const std::string& text)
{
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        const char ch = text[i];
        if (ch == '^' && i + 6 < text.size()) {
            bool isColorCode = true;
            for (size_t j = 1; j <= 6; ++j) {
                if (!IsHexDigit(text[i + j])) {
                    isColorCode = false;
                    break;
                }
            }
            if (isColorCode) {
                i += 6;
                continue;
            }
        }
        if (ch == '\r') {
            continue;
        }
        out.push_back(ch == '_' ? ' ' : ch);
    }
    return out;
}

std::string ResolveSkillIconPath(const PLAYER_SKILL_INFO& skillInfo)
{
    g_skillMgr.EnsureLoaded();
    const SkillMetadata* metadata = g_skillMgr.GetSkillMetadata(skillInfo.SKID);
    if (!metadata) {
        return std::string();
    }

    const std::string lowered = shopui::ToLowerAscii(metadata->skillIdName);
    const std::string direct = "texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\item\\" + lowered + ".bmp";
    const std::string dataPath = "data\\" + direct;
    if (g_fileMgr.IsDataExist(direct.c_str())) {
        return direct;
    }
    if (g_fileMgr.IsDataExist(dataPath.c_str())) {
        return dataPath;
    }
    return direct;
}

void DrawWrappedTextRect(HDC hdc, const RECT& rect, const std::string& text, COLORREF color)
{
    if (!hdc || text.empty() || rect.right <= rect.left || rect.bottom <= rect.top) {
        return;
    }

#if RO_ENABLE_QT6_UI
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    std::vector<unsigned int> pixels(static_cast<size_t>(width) * static_cast<size_t>(height), 0u);
    QImage image(reinterpret_cast<uchar*>(pixels.data()), width, height, width * static_cast<int>(sizeof(unsigned int)), QImage::Format_ARGB32);
    if (!image.isNull()) {
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing, false);
        painter.setRenderHint(QPainter::TextAntialiasing, false);
        QFont font(QStringLiteral("MS Sans Serif"));
        font.setPixelSize(11);
        font.setStyleStrategy(QFont::NoAntialias);
        painter.setFont(font);
        painter.setPen(QColor(GetRValue(color), GetGValue(color), GetBValue(color)));
        painter.drawText(QRect(0, 0, width, height), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, QString::fromLocal8Bit(text.c_str()));
        AlphaBlendArgbToHdc(hdc, rect.left, rect.top, width, height, pixels.data(), width, height);
        return;
    }
#endif

#if RO_PLATFORM_WINDOWS
    RECT drawRect = rect;
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    DrawTextA(hdc, text.c_str(), -1, &drawRect, DT_LEFT | DT_TOP | DT_WORDBREAK);
#endif
}

int MeasureWrappedTextHeight(HDC hdc, int width, const std::string& text)
{
    if (width <= 0 || text.empty()) {
        return 0;
    }

#if RO_ENABLE_QT6_UI
    QFont font(QStringLiteral("MS Sans Serif"));
    font.setPixelSize(11);
    font.setStyleStrategy(QFont::NoAntialias);
    const QRect bounds = QFontMetrics(font).boundingRect(
        QRect(0, 0, width, 16384),
        Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
        QString::fromLocal8Bit(text.c_str()));
    if (bounds.height() > 0) {
        return bounds.height();
    }
#endif

#if RO_PLATFORM_WINDOWS
    RECT calcRect{ 0, 0, width, 0 };
    DrawTextA(hdc, text.c_str(), -1, &calcRect, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_EDITCONTROL | DT_CALCRECT | DT_NOPREFIX);
    return (std::max)(0, static_cast<int>(calcRect.bottom - calcRect.top));
#else
    (void)hdc;
    return 0;
#endif
}

void DrawWrappedTextRectScrolled(HDC hdc, const RECT& rect, const std::string& text, COLORREF color, int scrollOffset, int contentHeight)
{
    if (!hdc || text.empty() || rect.right <= rect.left || rect.bottom <= rect.top) {
        return;
    }

#if RO_ENABLE_QT6_UI
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    std::vector<unsigned int> pixels(static_cast<size_t>(width) * static_cast<size_t>(height), 0u);
    QImage image(reinterpret_cast<uchar*>(pixels.data()), width, height, width * static_cast<int>(sizeof(unsigned int)), QImage::Format_ARGB32);
    if (!image.isNull()) {
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing, false);
        painter.setRenderHint(QPainter::TextAntialiasing, false);
        QFont font(QStringLiteral("MS Sans Serif"));
        font.setPixelSize(11);
        font.setStyleStrategy(QFont::NoAntialias);
        painter.setFont(font);
        painter.setPen(QColor(GetRValue(color), GetGValue(color), GetBValue(color)));
        painter.drawText(
            QRect(0, -scrollOffset, width, (std::max)(height + scrollOffset, contentHeight)),
            Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
            QString::fromLocal8Bit(text.c_str()));
        AlphaBlendArgbToHdc(hdc, rect.left, rect.top, width, height, pixels.data(), width, height);
        return;
    }
#endif

#if RO_PLATFORM_WINDOWS
    const int savedDc = SaveDC(hdc);
    IntersectClipRect(hdc, rect.left, rect.top, rect.right, rect.bottom);
    const int viewportHeight = static_cast<int>(rect.bottom - rect.top);
    RECT drawRect{ rect.left, rect.top - scrollOffset, rect.right, rect.top - scrollOffset + (std::max)(viewportHeight, contentHeight) };
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    DrawTextA(hdc, text.c_str(), -1, &drawRect, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_EDITCONTROL | DT_NOPREFIX);
    RestoreDC(hdc, savedDc);
#endif
}
}

UISkillDescribeWnd::UISkillDescribeWnd()
    : m_skillInfo(),
      m_hasSkill(false),
      m_closeHovered(false),
      m_closePressed(false),
      m_hasStoredPlacement(false),
      m_descriptionScrollOffset(0),
      m_isDraggingDescriptionScroll(false),
      m_descriptionScrollDragOffsetY(0),
      m_iconBitmap()
{
    Create(kWindowWidth, kWindowHeight);
    Move(48, 48);
    int savedX = m_x;
    int savedY = m_y;
    if (LoadUiWindowPlacement("SkillDescribeWnd", &savedX, &savedY)) {
        g_windowMgr.ClampWindowToClient(&savedX, &savedY, m_w, m_h);
        Move(savedX, savedY);
        m_hasStoredPlacement = true;
    }
}

void UISkillDescribeWnd::OnDraw()
{
    if (m_show == 0 || !m_hasSkill) {
        return;
    }

    HDC hdc = AcquireDrawTarget();
    if (!hdc) {
        return;
    }

    const RECT bounds = shopui::MakeRect(m_x, m_y, m_w, m_h);
    shopui::DrawFrameWindow(hdc, bounds, "Skill Information");

    const RECT closeRect = GetCloseButtonRect();
    shopui::FillRectColor(hdc, closeRect, m_closePressed ? RGB(189, 199, 222) : (m_closeHovered ? RGB(215, 223, 240) : RGB(231, 226, 214)));
    shopui::FrameRectColor(hdc, closeRect, RGB(127, 122, 112));
    shopui::DrawWindowTextRect(hdc, closeRect, "X", RGB(0, 0, 0), DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    const RECT iconBox = shopui::MakeRect(m_x + kPadding, m_y + kTitleBarHeight + kPadding, kIconBoxSize, kIconBoxSize);
    shopui::FillRectColor(hdc, iconBox, RGB(245, 242, 234));
    shopui::FrameRectColor(hdc, iconBox, RGB(166, 159, 145));
    if (m_iconBitmap.IsValid()) {
        const RECT iconRect = shopui::MakeRect(iconBox.left + 2, iconBox.top + 2, kIconBoxSize - 4, kIconBoxSize - 4);
        shopui::DrawBitmapPixelsTransparent(hdc, m_iconBitmap, iconRect);
    }

    RECT nameRect{ iconBox.right + 8, iconBox.top, m_x + m_w - kPadding, iconBox.top + 18 };
    shopui::DrawWindowTextRect(hdc, nameRect, SanitizeRoText(m_skillInfo.skillName.empty() ? m_skillInfo.skillIdName : m_skillInfo.skillName), RGB(0, 0, 0), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    const std::array<std::string, 3> lines = {
        "Level: " + std::to_string(m_skillInfo.level) + (m_skillInfo.skillMaxLv > 0 ? (" / " + std::to_string(m_skillInfo.skillMaxLv)) : std::string()),
        (m_skillInfo.spcost > 0 ? ("SP Cost: " + std::to_string(m_skillInfo.spcost)) : std::string("Type: Passive")),
        (m_skillInfo.attackRange > 0 ? ("Range: " + std::to_string(m_skillInfo.attackRange)) : std::string())
    };

    int detailY = nameRect.bottom + 2;
    for (const std::string& line : lines) {
        if (line.empty()) {
            continue;
        }
        RECT detailRect{ nameRect.left, detailY, m_x + m_w - kPadding, detailY + 14 };
        shopui::DrawWindowTextRect(hdc, detailRect, line, RGB(64, 64, 64), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        detailY += 13;
    }

    const DescriptionLayout descriptionLayout = GetDescriptionLayout();
    DrawWrappedTextRectScrolled(
        hdc,
        descriptionLayout.viewportRect,
        BuildDescriptionText(),
        RGB(16, 16, 16),
        m_descriptionScrollOffset,
        descriptionLayout.contentHeight);

    if (descriptionLayout.scrollBarVisible) {
        shopui::FillRectColor(hdc, descriptionLayout.scrollTrackRect, RGB(227, 231, 238));
        shopui::FrameRectColor(hdc, descriptionLayout.scrollTrackRect, RGB(164, 173, 189));
        shopui::FillRectColor(hdc, descriptionLayout.scrollThumbRect, RGB(180, 188, 205));
        shopui::FrameRectColor(hdc, descriptionLayout.scrollThumbRect, RGB(120, 130, 150));
    }

    ReleaseDrawTarget(hdc);
    m_isDirty = 0;
}

void UISkillDescribeWnd::OnLBtnDown(int x, int y)
{
    m_closePressed = false;
    if (IsInsideRect(GetCloseButtonRect(), x, y)) {
        m_closePressed = true;
        Invalidate();
        return;
    }

    const DescriptionLayout descriptionLayout = GetDescriptionLayout();
    if (descriptionLayout.scrollBarVisible) {
        if (IsInsideRect(descriptionLayout.scrollThumbRect, x, y)) {
            m_isDraggingDescriptionScroll = true;
            m_descriptionScrollDragOffsetY = y - descriptionLayout.scrollThumbRect.top;
            return;
        }
        if (IsInsideRect(descriptionLayout.scrollTrackRect, x, y)) {
            const int thumbHeight = descriptionLayout.scrollThumbRect.bottom - descriptionLayout.scrollThumbRect.top;
            UpdateDescriptionScrollFromThumbPosition(y - (thumbHeight / 2));
            Invalidate();
            return;
        }
    }

    UIFrameWnd::OnLBtnDown(x, y);
}

void UISkillDescribeWnd::OnLBtnUp(int x, int y)
{
    if (m_isDraggingDescriptionScroll) {
        m_isDraggingDescriptionScroll = false;
        UpdateInteraction(x, y);
        return;
    }

    const bool wasDragging = m_isDragging != 0;
    UIFrameWnd::OnLBtnUp(x, y);
    if (wasDragging) {
        return;
    }

    const bool activateClose = m_closePressed && IsInsideRect(GetCloseButtonRect(), x, y);
    m_closePressed = false;
    UpdateInteraction(x, y);
    if (activateClose) {
        SetShow(0);
    }
}

void UISkillDescribeWnd::OnMouseMove(int x, int y)
{
    if (m_isDraggingDescriptionScroll) {
        UpdateDescriptionScrollFromThumbPosition(y - m_descriptionScrollDragOffsetY);
        UpdateInteraction(x, y);
        Invalidate();
        return;
    }

    UIFrameWnd::OnMouseMove(x, y);
    UpdateInteraction(x, y);
}

void UISkillDescribeWnd::OnMouseHover(int x, int y)
{
    UpdateInteraction(x, y);
}

void UISkillDescribeWnd::OnWheel(int delta)
{
    const DescriptionLayout descriptionLayout = GetDescriptionLayout();
    if (!descriptionLayout.scrollBarVisible) {
        return;
    }

    if (delta > 0) {
        AdjustDescriptionScroll(-kDescriptionWheelStep);
    } else if (delta < 0) {
        AdjustDescriptionScroll(kDescriptionWheelStep);
    }
}

void UISkillDescribeWnd::StoreInfo()
{
    SaveUiWindowPlacement("SkillDescribeWnd", m_x, m_y);
    m_hasStoredPlacement = true;
}

void UISkillDescribeWnd::SetSkillInfo(const PLAYER_SKILL_INFO& skillInfo, int preferredX, int preferredY)
{
    m_skillInfo = skillInfo;
    m_hasSkill = skillInfo.SKID != 0;
    m_descriptionScrollOffset = 0;
    m_isDraggingDescriptionScroll = false;
    m_descriptionScrollDragOffsetY = 0;
    m_iconBitmap.Clear();
    if (m_hasSkill) {
        const std::string iconPath = ResolveSkillIconPath(skillInfo);
        if (!iconPath.empty()) {
            m_iconBitmap = shopui::LoadBitmapPixelsFromGameData(iconPath, true);
        }
    }

    if (m_show == 0 && !m_hasStoredPlacement) {
        int nextX = preferredX;
        int nextY = preferredY;
        g_windowMgr.ClampWindowToClient(&nextX, &nextY, m_w, m_h);
        Move(nextX, nextY);
    }
    SetShow(m_hasSkill ? 1 : 0);
    Invalidate();
}

bool UISkillDescribeWnd::GetDisplayDataForQt(DisplayData* outData) const
{
    if (!outData || m_show == 0 || !m_hasSkill) {
        return false;
    }

    outData->title = "Skill Information";
    outData->skillId = m_skillInfo.SKID;
    outData->name = SanitizeRoText(m_skillInfo.skillName.empty() ? m_skillInfo.skillIdName : m_skillInfo.skillName);
    outData->detailLines = {
        "Level: " + std::to_string(m_skillInfo.level) + (m_skillInfo.skillMaxLv > 0 ? (" / " + std::to_string(m_skillInfo.skillMaxLv)) : std::string()),
        (m_skillInfo.spcost > 0 ? ("SP Cost: " + std::to_string(m_skillInfo.spcost)) : std::string("Type: Passive")),
        (m_skillInfo.attackRange > 0 ? ("Range: " + std::to_string(m_skillInfo.attackRange)) : std::string())
    };
    outData->description = BuildDescriptionText();
    const DescriptionLayout descriptionLayout = GetDescriptionLayout();
    outData->descriptionX = descriptionLayout.viewportRect.left;
    outData->descriptionY = descriptionLayout.viewportRect.top;
    outData->descriptionWidth = descriptionLayout.viewportRect.right - descriptionLayout.viewportRect.left;
    outData->descriptionHeight = descriptionLayout.viewportRect.bottom - descriptionLayout.viewportRect.top;
    outData->descriptionContentHeight = descriptionLayout.contentHeight;
    outData->descriptionScrollOffset = m_descriptionScrollOffset;
    outData->descriptionScrollBar.visible = descriptionLayout.scrollBarVisible;
    outData->descriptionScrollBar.trackX = descriptionLayout.scrollTrackRect.left;
    outData->descriptionScrollBar.trackY = descriptionLayout.scrollTrackRect.top;
    outData->descriptionScrollBar.trackWidth = descriptionLayout.scrollTrackRect.right - descriptionLayout.scrollTrackRect.left;
    outData->descriptionScrollBar.trackHeight = descriptionLayout.scrollTrackRect.bottom - descriptionLayout.scrollTrackRect.top;
    outData->descriptionScrollBar.thumbX = descriptionLayout.scrollThumbRect.left;
    outData->descriptionScrollBar.thumbY = descriptionLayout.scrollThumbRect.top;
    outData->descriptionScrollBar.thumbWidth = descriptionLayout.scrollThumbRect.right - descriptionLayout.scrollThumbRect.left;
    outData->descriptionScrollBar.thumbHeight = descriptionLayout.scrollThumbRect.bottom - descriptionLayout.scrollThumbRect.top;
    const RECT closeRect = GetCloseButtonRect();
    outData->closeButton = { closeRect.left, closeRect.top, kCloseButtonSize, kCloseButtonSize, m_closeHovered, m_closePressed, "X" };
    return true;
}

bool UISkillDescribeWnd::HasSkill() const
{
    return m_hasSkill;
}

RECT UISkillDescribeWnd::GetCloseButtonRect() const
{
    return shopui::MakeRect(m_x + m_w - 16, m_y + 2, kCloseButtonSize, kCloseButtonSize);
}

UISkillDescribeWnd::DescriptionLayout UISkillDescribeWnd::GetDescriptionLayout() const
{
    DescriptionLayout layout{};
    layout.viewportRect = RECT{ m_x + kPadding, m_y + kDescriptionTop, m_x + m_w - kPadding, m_y + m_h - kPadding };

    HDC hdc = AcquireDrawTarget();
    const std::string descriptionText = BuildDescriptionText();
    layout.contentHeight = MeasureWrappedTextHeight(hdc, layout.viewportRect.right - layout.viewportRect.left, descriptionText);
    if (layout.contentHeight > (layout.viewportRect.bottom - layout.viewportRect.top)) {
        layout.scrollBarVisible = true;
        layout.viewportRect.right -= (kDescriptionScrollBarGap + kDescriptionScrollBarWidth);
        layout.contentHeight = MeasureWrappedTextHeight(hdc, layout.viewportRect.right - layout.viewportRect.left, descriptionText);
        layout.scrollTrackRect = RECT{
            layout.viewportRect.right + kDescriptionScrollBarGap,
            layout.viewportRect.top,
            layout.viewportRect.right + kDescriptionScrollBarGap + kDescriptionScrollBarWidth,
            layout.viewportRect.bottom
        };
    }
    if (hdc) {
        ReleaseDrawTarget(hdc);
    }

    const int viewportHeight = (std::max)(0, static_cast<int>(layout.viewportRect.bottom - layout.viewportRect.top));
    layout.contentHeight = (std::max)(layout.contentHeight, viewportHeight);
    layout.maxScrollOffset = (std::max)(0, layout.contentHeight - viewportHeight);

    if (layout.scrollBarVisible) {
        const int trackHeight = (std::max)(0, static_cast<int>(layout.scrollTrackRect.bottom - layout.scrollTrackRect.top - 8));
        int thumbHeight = viewportHeight > 0 && layout.contentHeight > 0
            ? (trackHeight * viewportHeight) / layout.contentHeight
            : trackHeight;
        thumbHeight = (std::max)(18, thumbHeight);
        thumbHeight = (std::min)(trackHeight, thumbHeight);

        int thumbTop = layout.scrollTrackRect.top + 4;
        if (layout.maxScrollOffset > 0 && trackHeight > thumbHeight) {
            thumbTop += ((trackHeight - thumbHeight) * m_descriptionScrollOffset) / layout.maxScrollOffset;
        }
        layout.scrollThumbRect = RECT{
            layout.scrollTrackRect.left + 2,
            thumbTop,
            layout.scrollTrackRect.right - 2,
            thumbTop + thumbHeight
        };
    }

    return layout;
}

void UISkillDescribeWnd::ClampDescriptionScroll()
{
    const DescriptionLayout layout = GetDescriptionLayout();
    m_descriptionScrollOffset = std::clamp(m_descriptionScrollOffset, 0, layout.maxScrollOffset);
}

void UISkillDescribeWnd::AdjustDescriptionScroll(int delta)
{
    const int oldOffset = m_descriptionScrollOffset;
    m_descriptionScrollOffset += delta;
    ClampDescriptionScroll();
    if (m_descriptionScrollOffset != oldOffset) {
        Invalidate();
    }
}

void UISkillDescribeWnd::UpdateDescriptionScrollFromThumbPosition(int globalY)
{
    const DescriptionLayout layout = GetDescriptionLayout();
    if (!layout.scrollBarVisible || layout.maxScrollOffset <= 0) {
        m_descriptionScrollOffset = 0;
        return;
    }

    const int thumbHeight = layout.scrollThumbRect.bottom - layout.scrollThumbRect.top;
    const int minTop = layout.scrollTrackRect.top + 4;
    const int maxTop = layout.scrollTrackRect.bottom - 4 - thumbHeight;
    const int clampedTop = (std::max)(minTop, (std::min)(globalY, maxTop));
    const int denominator = (std::max)(1, maxTop - minTop);
    m_descriptionScrollOffset = ((clampedTop - minTop) * layout.maxScrollOffset) / denominator;
}

void UISkillDescribeWnd::UpdateInteraction(int x, int y)
{
    const bool nextHover = IsInsideRect(GetCloseButtonRect(), x, y);
    if (m_closeHovered == nextHover) {
        return;
    }
    m_closeHovered = nextHover;
    Invalidate();
}

std::string UISkillDescribeWnd::BuildDescriptionText() const
{
    if (!m_skillInfo.descriptionLines.empty()) {
        std::string text;
        for (size_t index = 0; index < m_skillInfo.descriptionLines.size(); ++index) {
            if (index > 0) {
                text += '\n';
            }
            text += SanitizeRoText(m_skillInfo.descriptionLines[index]);
        }
        return text;
    }
    return "No description available.";
}