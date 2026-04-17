#include "UINewChatWnd.h"

#include "UIWindowMgr.h"
#include "gamemode/GameMode.h"
#include "gamemode/Mode.h"
#include "main/WinMain.h"
#include "qtui/QtUiRuntime.h"
#include "render/DC.h"

#if RO_ENABLE_QT6_UI
#include <QFont>
#include <QFontMetrics>
#include <QImage>
#include <QPainter>
#include <QString>
#endif

#include <algorithm>
#include <string>
#include <vector>

#include <windows.h>

#if RO_PLATFORM_WINDOWS
#pragma comment(lib, "msimg32.lib")
#endif

namespace {
constexpr size_t kMaxChatLines = 50;
constexpr int kChatWindowMargin = 12;
constexpr int kChatWindowWidth = 420;
constexpr int kChatLineHeight = 19;
constexpr int kChatInputHeight = 22;
constexpr int kChatPanelPadding = 8;
constexpr int kChatMessageGap = 2;
constexpr int kChatVisibleLineCount = 12;
constexpr int kChatHistoryHeight =
    (kChatLineHeight * kChatVisibleLineCount) + (kChatMessageGap * (kChatVisibleLineCount - 1));
constexpr int kChatScrollbarWidth = 8;
constexpr int kChatScrollbarGap = 4;
constexpr size_t kMaxInputChars = 180;
constexpr size_t kMaxInputHistory = 5;
constexpr int kChatWheelScrollLines = 3;

bool PointInRectXY(const RECT& rc, int x, int y)
{
    return x >= rc.left && x < rc.right && y >= rc.top && y < rc.bottom;
}

COLORREF ToColorRef(u32 color)
{
    return RGB((color >> 16) & 0xFFu, (color >> 8) & 0xFFu, color & 0xFFu);
}

void DrawBitmapPixelsTransparent(HDC target, const shopui::BitmapPixels& bitmap, int x, int y, int width, int height)
{
    if (!target || !bitmap.IsValid() || width <= 0 || height <= 0) {
        return;
    }
    RECT dst = { x, y, x + width, y + height };
    shopui::DrawBitmapPixelsTransparent(target, bitmap, dst);
}

#if RO_ENABLE_QT6_UI
QFont BuildChatFontFromHdc(HDC hdc)
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

int MeasureWrappedTextHeightQt(HDC hdc, const std::string& text, int width)
{
    if (!hdc || width <= 0 || text.empty()) {
        return kChatLineHeight;
    }

    const QFont font = BuildChatFontFromHdc(hdc);
    const QFontMetrics metrics(font);
    const QString label = QString::fromLocal8Bit(text.c_str());
    const QRect bounds = metrics.boundingRect(QRect(0, 0, width, 4096), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, label);
    return (std::max)(kChatLineHeight, bounds.height());
}

void DrawChatTextQt(HDC hdc, const RECT& rect, const std::string& text, COLORREF color, bool wrap)
{
    if (!hdc || rect.right <= rect.left || rect.bottom <= rect.top) {
        return;
    }

    QString label = QString::fromLocal8Bit(text.c_str());
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    const QFont font = BuildChatFontFromHdc(hdc);
    if (!wrap) {
        const QFontMetrics metrics(font);
        label = metrics.elidedText(label, Qt::ElideRight, width);
    }

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
    const int flags = wrap
        ? (Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap)
        : (Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine);
    painter.drawText(QRect(0, 0, width, height), flags, label);
    AlphaBlendArgbToHdc(hdc, rect.left, rect.top, width, height, pixels.data(), width, height);
}
#endif

int MeasureWrappedTextHeight(HDC hdc, const std::string& text, int width)
{
    if (!hdc || width <= 0 || text.empty()) {
        return kChatLineHeight;
    }

#if RO_ENABLE_QT6_UI
    return MeasureWrappedTextHeightQt(hdc, text, width);
#else
    RECT calcRect{ 0, 0, width, 0 };
    DrawTextA(hdc, text.c_str(), -1, &calcRect, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_EDITCONTROL | DT_CALCRECT | DT_NOPREFIX);
    const int measuredHeight = calcRect.bottom - calcRect.top;
    return (std::max)(kChatLineHeight, measuredHeight);
#endif
}

HFONT GetChatUiFont()
{
    static HFONT s_chatUiFont = nullptr;
    if (!s_chatUiFont) {
        s_chatUiFont = CreateFontA(
            -13,
            0,
            0,
            0,
            FW_NORMAL,
            FALSE,
            FALSE,
            FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            NONANTIALIASED_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE,
            "MS Sans Serif");
    }
    return s_chatUiFont;
}

const shopui::BitmapPixels& GetHistoryPanelPattern()
{
    static shopui::BitmapPixels s_pattern;
    if (s_pattern.IsValid()) {
        return s_pattern;
    }

    constexpr int kPatternSize = 4;
    s_pattern.width = kPatternSize;
    s_pattern.height = kPatternSize;
    s_pattern.pixels.resize(static_cast<size_t>(kPatternSize) * kPatternSize);

    const u32 darkGrey = 0xFF181818u;
    const u32 transparent = 0x00000000u;
    for (int y = 0; y < kPatternSize; ++y) {
        for (int x = 0; x < kPatternSize; ++x) {
            const bool useTransparentPixel = ((x + y) % 4) == 0;
            s_pattern.pixels[static_cast<size_t>(y) * kPatternSize + x] = useTransparentPixel ? transparent : darkGrey;
        }
    }

    return s_pattern;
}

void FillRectStippled(HDC target, const RECT& rect)
{
    if (!target || rect.right <= rect.left || rect.bottom <= rect.top) {
        return;
    }

    const shopui::BitmapPixels& pattern = GetHistoryPanelPattern();
    if (!pattern.IsValid()) {
        HBRUSH brush = CreateSolidBrush(RGB(24, 24, 24));
        FillRect(target, &rect, brush);
        DeleteObject(brush);
        return;
    }

    constexpr int kPatternSize = 4;
    for (int y = rect.top; y < rect.bottom; y += kPatternSize) {
        for (int x = rect.left; x < rect.right; x += kPatternSize) {
            const int drawWidth = (std::min)(kPatternSize, static_cast<int>(rect.right - x));
            const int drawHeight = (std::min)(kPatternSize, static_cast<int>(rect.bottom - y));
            DrawBitmapPixelsTransparent(target, pattern, x, y, drawWidth, drawHeight);
        }
    }
}
}

UINewChatWnd::UINewChatWnd()
    : m_lastDrawTick(0),
      m_inputActive(0),
      m_historyBrowseIndex(-1),
      m_scrollLineOffset(0),
      m_firstVisibleLineIndex(0),
      m_dragStartGlobalX(0),
      m_dragStartGlobalY(0),
      m_dragStartWindowX(0),
      m_dragStartWindowY(0),
      m_dragArmed(0),
      m_isDragging(0)
{
    const int panelHeight = kChatHistoryHeight + kChatInputHeight + (kChatPanelPadding * 3);
    Create(kChatWindowWidth, panelHeight);

    int defaultX = kChatWindowMargin;
    int defaultY = 480 - panelHeight - kChatWindowMargin;
    if (g_hMainWnd) {
        RECT clientRect{};
        GetClientRect(g_hMainWnd, &clientRect);
        defaultY = clientRect.bottom - panelHeight - kChatWindowMargin;
    }

    Move(defaultX, defaultY);

    int savedX = m_x;
    int savedY = m_y;
    if (LoadUiWindowPlacement("ChatWnd", &savedX, &savedY)) {
        g_windowMgr.ClampWindowToClient(&savedX, &savedY, m_w, m_h);
        Move(savedX, savedY);
    }
}

UINewChatWnd::~UINewChatWnd() = default;

void UINewChatWnd::AddChatLine(const char* text, u32 color, u8 channel, u32 tick)
{
    if (!text || *text == '\0') {
        return;
    }

    const bool wasPinnedBottom = m_scrollLineOffset == 0;

    if (m_lines.size() >= kMaxChatLines) {
        m_lines.erase(m_lines.begin());
    }

    ChatLine line{};
    line.text = text;
    line.color = color;
    line.channel = channel;
    line.tick = tick;
    m_lines.push_back(std::move(line));

    if (!wasPinnedBottom) {
        ++m_scrollLineOffset;
    }
    ClampScrollOffset();
    RefreshVisibleLines(GetTickCount());
    Invalidate();
}

const std::vector<ChatLine>& UINewChatWnd::GetLines() const
{
    return m_lines;
}

const std::vector<ChatLine>& UINewChatWnd::GetVisibleLines() const
{
    return m_visibleLines;
}

const std::string& UINewChatWnd::GetInputText() const
{
    return m_inputText;
}

const std::vector<std::string>& UINewChatWnd::GetInputHistory() const
{
    return m_inputHistory;
}

ChatScrollBarState UINewChatWnd::GetScrollBarState() const
{
    ChatScrollBarState state{};
    state.totalLines = static_cast<int>(m_lines.size());
    state.firstVisibleLine = m_firstVisibleLineIndex;
    state.visibleLineCount = static_cast<int>(m_visibleLines.size());
    state.visible = (state.totalLines > state.visibleLineCount) ? 1 : 0;
    return state;
}

void UINewChatWnd::OnProcess()
{
    Layout();
    const auto& events = g_windowMgr.GetChatEvents();
    std::vector<ChatLine> syncedLines;
    const size_t start = events.size() > kMaxChatLines ? (events.size() - kMaxChatLines) : 0;
    syncedLines.reserve(events.size() - start);
    for (size_t i = start; i < events.size(); ++i) {
        ChatLine line{};
        line.text = events[i].text;
        line.color = events[i].color;
        line.channel = events[i].channel;
        line.tick = events[i].tick;
        syncedLines.push_back(std::move(line));
    }
    bool linesChanged = syncedLines.size() != m_lines.size();
    if (!linesChanged) {
        for (size_t i = 0; i < syncedLines.size(); ++i) {
            if (syncedLines[i].text != m_lines[i].text ||
                syncedLines[i].color != m_lines[i].color ||
                syncedLines[i].channel != m_lines[i].channel ||
                syncedLines[i].tick != m_lines[i].tick) {
                linesChanged = true;
                break;
            }
        }
    }
    if (linesChanged) {
        const bool wasPinnedBottom = m_scrollLineOffset == 0;
        const size_t oldCount = m_lines.size();
        m_lines.swap(syncedLines);
        if (!wasPinnedBottom && m_lines.size() > oldCount) {
            m_scrollLineOffset += static_cast<int>(m_lines.size() - oldCount);
        }
        ClampScrollOffset();
        RefreshVisibleLines(GetTickCount());
        Invalidate();
    }
    RefreshVisibleLines(GetTickCount());
}

void UINewChatWnd::OnDraw()
{
    if (IsQtUiRuntimeEnabled()) {
        m_lastDrawTick = GetTickCount();
        m_isDirty = 0;
        return;
    }

    if (!g_hMainWnd || m_show == 0) {
        return;
    }

    HDC hdc = AcquireDrawTarget();
    if (!hdc) {
        return;
    }

    const int savedDc = SaveDC(hdc);
    SelectObject(hdc, GetChatUiFont());

    RECT panelRc = { m_x, m_y, m_x + m_w, m_y + m_h };
    RECT inputRc = {
        m_x + kChatPanelPadding,
        m_y + m_h - kChatInputHeight - kChatPanelPadding,
        m_x + m_w - kChatPanelPadding,
        m_y + m_h - kChatPanelPadding
    };
    RECT historyRc = {
        inputRc.left,
        m_y + kChatPanelPadding,
        inputRc.right,
        inputRc.top - kChatPanelPadding
    };
    const RECT topStrip = { panelRc.left, panelRc.top, panelRc.right, historyRc.top };
    const RECT leftStrip = { panelRc.left, historyRc.top, historyRc.left, panelRc.bottom };
    const RECT rightStrip = { historyRc.right, historyRc.top, panelRc.right, panelRc.bottom };
    const RECT middleStrip = { panelRc.left, historyRc.bottom, panelRc.right, inputRc.top };
    const RECT bottomStrip = { panelRc.left, inputRc.bottom, panelRc.right, panelRc.bottom };
    FillRectStippled(hdc, topStrip);
    FillRectStippled(hdc, leftStrip);
    FillRectStippled(hdc, rightStrip);
    FillRectStippled(hdc, middleStrip);
    FillRectStippled(hdc, bottomStrip);
    FillRectStippled(hdc, historyRc);
    FrameRect(hdc, &panelRc, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));

    const int historyClipDc = SaveDC(hdc);
    IntersectClipRect(hdc, historyRc.left, historyRc.top, historyRc.right, historyRc.bottom);
    SetBkMode(hdc, TRANSPARENT);
    const int textInset = 4;
    const ChatScrollBarState scrollState = GetScrollBarState();
    const int reservedScrollbarWidth = scrollState.visible ? (kChatScrollbarWidth + kChatScrollbarGap) : 0;
    const int textWidth = (historyRc.right - historyRc.left) - (textInset * 2) - reservedScrollbarWidth;
    const size_t lineCount = m_visibleLines.size();
    std::vector<int> measuredHeights(lineCount, kChatLineHeight);
    int usedHeight = 0;
    for (size_t index = 0; index < lineCount; ++index) {
        measuredHeights[index] = MeasureWrappedTextHeight(hdc, m_visibleLines[index].text, textWidth);
        usedHeight += measuredHeights[index];
        if (index + 1 < lineCount) {
            usedHeight += kChatMessageGap;
        }
    }

    int lineY = historyRc.bottom - textInset - usedHeight;
    if (lineY < historyRc.top + textInset) {
        lineY = historyRc.top + textInset;
    }
    for (size_t index = 0; index < lineCount; ++index) {
        const ChatLine& line = m_visibleLines[index];
        const int textHeight = measuredHeights[index];
        RECT textRc = {
            historyRc.left + textInset,
            lineY,
            historyRc.right - textInset - reservedScrollbarWidth,
            lineY + textHeight
        };
        SetTextColor(hdc, ToColorRef(line.color));
#if RO_ENABLE_QT6_UI
        DrawChatTextQt(hdc, textRc, line.text, ToColorRef(line.color), true);
#else
        DrawTextA(hdc, line.text.c_str(), -1, &textRc, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_EDITCONTROL | DT_NOPREFIX);
#endif
        lineY += textHeight + kChatMessageGap;
    }
    RestoreDC(hdc, historyClipDc);

    if (scrollState.visible) {
        RECT trackRc = {
            historyRc.right - textInset - kChatScrollbarWidth,
            historyRc.top + textInset,
            historyRc.right - textInset,
            historyRc.bottom - textInset
        };
        HBRUSH trackBrush = CreateSolidBrush(RGB(40, 40, 40));
        FillRect(hdc, &trackRc, trackBrush);
        DeleteObject(trackBrush);
        FrameRect(hdc, &trackRc, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));

        const int trackHeight = trackRc.bottom - trackRc.top;
        const int visibleLineCount = (std::max)(1, scrollState.visibleLineCount);
        const int totalLines = (std::max)(visibleLineCount, scrollState.totalLines);
        const int thumbHeight = (std::max)(18, trackHeight * visibleLineCount / totalLines);
        const int maxTravel = (std::max)(0, trackHeight - thumbHeight);
        const int scrollDenom = (std::max)(1, totalLines - visibleLineCount);
        const int thumbTop = trackRc.top + (maxTravel * scrollState.firstVisibleLine) / scrollDenom;
        RECT thumbRc = { trackRc.left + 1, thumbTop, trackRc.right - 1, thumbTop + thumbHeight };
        HBRUSH thumbBrush = CreateSolidBrush(RGB(184, 184, 184));
        FillRect(hdc, &thumbRc, thumbBrush);
        DeleteObject(thumbBrush);
    }

    HBRUSH inputBg = CreateSolidBrush(m_inputActive ? RGB(245, 245, 220) : RGB(210, 210, 210));
    FillRect(hdc, &inputRc, inputBg);
    DeleteObject(inputBg);
    FrameRect(hdc, &inputRc, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));

    std::string drawText = m_inputText;
    if (m_inputActive) {
        drawText += '_';
    }

    RECT inputTextRc = { inputRc.left + 4, inputRc.top + 2, inputRc.right - 2, inputRc.bottom - 2 };
    SetTextColor(hdc, RGB(16, 16, 16));
#if RO_ENABLE_QT6_UI
    DrawChatTextQt(hdc, inputTextRc, drawText, RGB(16, 16, 16), false);
#else
    DrawTextA(hdc, drawText.c_str(), -1, &inputTextRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
#endif

    RestoreDC(hdc, savedDc);
    ReleaseDrawTarget(hdc);

    m_lastDrawTick = GetTickCount();
    m_isDirty = 0;
}

void UINewChatWnd::RefreshVisibleLines(u32 nowTick)
{
    (void)nowTick;

    ClampScrollOffset();

    std::vector<ChatLine> nextVisible;
    m_firstVisibleLineIndex = 0;

    if (!m_lines.empty()) {
        HDC hdc = AcquireDrawTarget();
        const int textWidth = kChatWindowWidth - (kChatPanelPadding * 2) - 8 - 8 - (kChatScrollbarWidth + kChatScrollbarGap);
        const int endExclusive = (std::max)(0, static_cast<int>(m_lines.size()) - m_scrollLineOffset);
        int usedHeight = 0;
        int firstVisibleIndex = endExclusive;
        for (int index = endExclusive - 1; index >= 0; --index) {
            const int measured = MeasureWrappedTextHeight(hdc, m_lines[static_cast<size_t>(index)].text, textWidth);
            const int blockHeight = measured + ((usedHeight > 0) ? kChatMessageGap : 0);
            if (usedHeight + blockHeight > kChatHistoryHeight) {
                break;
            }
            usedHeight += blockHeight;
            firstVisibleIndex = index;
        }

        if (firstVisibleIndex == endExclusive && endExclusive > 0) {
            firstVisibleIndex = endExclusive - 1;
        }

        m_firstVisibleLineIndex = (std::max)(0, firstVisibleIndex);
        nextVisible.reserve(static_cast<size_t>((std::max)(0, endExclusive - m_firstVisibleLineIndex)));
        for (int index = m_firstVisibleLineIndex; index < endExclusive; ++index) {
            nextVisible.push_back(m_lines[static_cast<size_t>(index)]);
        }

        if (hdc) {
            ReleaseDrawTarget(hdc);
        }
    }

    if (nextVisible.size() != m_visibleLines.size()) {
        Invalidate();
    } else {
        for (size_t i = 0; i < nextVisible.size(); ++i) {
            if (nextVisible[i].text != m_visibleLines[i].text ||
                nextVisible[i].color != m_visibleLines[i].color ||
                nextVisible[i].channel != m_visibleLines[i].channel) {
                Invalidate();
                break;
            }
        }
    }
    m_visibleLines.swap(nextVisible);
}

void UINewChatWnd::ClearLines()
{
    m_lines.clear();
    m_visibleLines.clear();
    m_scrollLineOffset = 0;
    m_firstVisibleLineIndex = 0;
    Invalidate();
}

void UINewChatWnd::OnLBtnDown(int x, int y)
{
    const RECT inputRc = {
        m_x + kChatPanelPadding,
        m_y + m_h - kChatInputHeight - kChatPanelPadding,
        m_x + m_w - kChatPanelPadding,
        m_y + m_h - kChatPanelPadding
    };

    if (PointInRectXY(inputRc, x, y)) {
        SetInputActive(true);
        m_dragArmed = 0;
        m_isDragging = 0;
        return;
    }

    m_dragArmed = 1;
    m_isDragging = 0;
    m_dragStartGlobalX = x;
    m_dragStartGlobalY = y;
    m_dragStartWindowX = m_x;
    m_dragStartWindowY = m_y;
}

void UINewChatWnd::OnMouseMove(int x, int y)
{
    if (m_dragArmed == 0 && m_isDragging == 0) {
        return;
    }

    const int dx = x - m_dragStartGlobalX;
    const int dy = y - m_dragStartGlobalY;
    if (m_isDragging == 0 && ((dx * dx) + (dy * dy)) >= 16) {
        m_isDragging = 1;
    }

    if (m_isDragging == 0) {
        return;
    }

    int snappedX = m_dragStartWindowX + dx;
    int snappedY = m_dragStartWindowY + dy;
    g_windowMgr.SnapWindowToNearby(this, &snappedX, &snappedY);
    g_windowMgr.ClampWindowToClient(&snappedX, &snappedY, m_w, m_h);
    Move(snappedX, snappedY);
}

void UINewChatWnd::OnLBtnUp(int x, int y)
{
    if (m_isDragging != 0) {
        StoreInfo();
    } else if (m_dragArmed != 0) {
        const RECT inputRc = {
            m_x + kChatPanelPadding,
            m_y + m_h - kChatInputHeight - kChatPanelPadding,
            m_x + m_w - kChatPanelPadding,
            m_y + m_h - kChatPanelPadding
        };
        if (!PointInRectXY(inputRc, x, y)) {
            SetInputActive(true);
        }
    }

    m_dragArmed = 0;
    m_isDragging = 0;
}

void UINewChatWnd::OnWheel(int delta)
{
    if (delta > 0) {
        AdjustScroll(kChatWheelScrollLines);
    } else if (delta < 0) {
        AdjustScroll(-kChatWheelScrollLines);
    }
}

bool UINewChatWnd::HandleKeyDown(int virtualKey)
{
    if (virtualKey == VK_RETURN) {
        if (m_inputActive == 0) {
            SetInputActive(true);
            return true;
        }
        return SubmitInput();
    }

    if ((virtualKey == VK_UP || virtualKey == VK_DOWN) && m_inputActive == 0) {
        SetInputActive(true);
    }

    if (virtualKey == VK_UP && m_inputActive != 0) {
        if (m_inputHistory.empty()) {
            return true;
        }
        if (m_historyBrowseIndex < 0) {
            m_historyDraft = m_inputText;
            m_historyBrowseIndex = 0;
        } else if (m_historyBrowseIndex + 1 < static_cast<int>(m_inputHistory.size())) {
            ++m_historyBrowseIndex;
        }
        m_inputText = m_inputHistory[static_cast<size_t>(m_historyBrowseIndex)];
        Invalidate();
        return true;
    }

    if (virtualKey == VK_DOWN && m_inputActive != 0) {
        if (m_historyBrowseIndex < 0) {
            return true;
        }
        --m_historyBrowseIndex;
        if (m_historyBrowseIndex >= 0) {
            m_inputText = m_inputHistory[static_cast<size_t>(m_historyBrowseIndex)];
        } else {
            m_inputText = m_historyDraft;
            m_historyDraft.clear();
        }
        Invalidate();
        return true;
    }

    if (virtualKey == VK_BACK && m_inputActive != 0) {
        if (!m_inputText.empty()) {
            if (m_historyBrowseIndex >= 0) {
                m_historyBrowseIndex = -1;
                m_historyDraft.clear();
            }
            m_inputText.pop_back();
            Invalidate();
        }
        return true;
    }

    if (virtualKey == VK_ESCAPE && m_inputActive != 0) {
        SetInputActive(false);
        return true;
    }

    return false;
}

bool UINewChatWnd::HandleChar(char c)
{
    if (m_inputActive == 0) {
        return false;
    }

    if (c == '\b' || c == '\r' || c == '\n') {
        return true;
    }

    if (static_cast<unsigned char>(c) < 0x20u) {
        return false;
    }

    if (m_inputText.size() >= kMaxInputChars) {
        return true;
    }

    if (m_historyBrowseIndex >= 0) {
        m_historyBrowseIndex = -1;
        m_historyDraft.clear();
    }
    m_inputText += c;
    Invalidate();
    return true;
}

bool UINewChatWnd::IsInputActive() const
{
    return m_inputActive != 0;
}

void UINewChatWnd::Layout()
{
    if (!g_hMainWnd) {
        return;
    }

    RECT clientRect{};
    GetClientRect(g_hMainWnd, &clientRect);

    const int panelHeight = kChatHistoryHeight + kChatInputHeight + (kChatPanelPadding * 3);
    Resize(kChatWindowWidth, panelHeight);

    int clampedX = m_x;
    int clampedY = m_y;
    g_windowMgr.ClampWindowToClient(&clampedX, &clampedY, m_w, m_h);
    Move(clampedX, clampedY);
}

void UINewChatWnd::AdjustScroll(int lineDelta)
{
    if (lineDelta == 0 || m_lines.empty()) {
        return;
    }

    m_scrollLineOffset += lineDelta;
    ClampScrollOffset();
    RefreshVisibleLines(GetTickCount());
    Invalidate();
}

void UINewChatWnd::ClampScrollOffset()
{
    const int maxOffset = m_lines.empty() ? 0 : static_cast<int>(m_lines.size()) - 1;
    m_scrollLineOffset = std::clamp(m_scrollLineOffset, 0, maxOffset);
}

void UINewChatWnd::SetInputActive(bool active)
{
    m_inputActive = active ? 1 : 0;
    if (!m_inputActive) {
        m_historyBrowseIndex = -1;
        m_historyDraft.clear();
    }
    Invalidate();
}

bool UINewChatWnd::SubmitInput()
{
    std::string text = m_inputText;
    if (text.empty()) {
        SetInputActive(false);
        return true;
    }

    const size_t firstNonSpace = text.find_first_not_of(" \t\r\n");
    if (firstNonSpace == std::string::npos) {
        m_inputText.clear();
        Invalidate();
        return true;
    }

    const size_t lastNonSpace = text.find_last_not_of(" \t\r\n");
    text = text.substr(firstNonSpace, lastNonSpace - firstNonSpace + 1);

    const msgresult_t sent = g_modeMgr.SendMsg(CGameMode::GameMsg_SubmitChat, reinterpret_cast<msgparam_t>(text.c_str()), 0, 0);
    if (sent != 0) {
        AddInputHistory(text);
        m_inputText.clear();
        m_historyBrowseIndex = -1;
        m_historyDraft.clear();
        Invalidate();
        return true;
    }

    return false;
}

void UINewChatWnd::AddInputHistory(const std::string& text)
{
    if (text.empty()) {
        return;
    }

    auto existing = std::find(m_inputHistory.begin(), m_inputHistory.end(), text);
    if (existing != m_inputHistory.end()) {
        m_inputHistory.erase(existing);
    }
    m_inputHistory.insert(m_inputHistory.begin(), text);
    if (m_inputHistory.size() > kMaxInputHistory) {
        m_inputHistory.resize(kMaxInputHistory);
    }
}

void UINewChatWnd::RestorePersistentState(const std::vector<std::string>& inputHistory,
    const std::string& inputText,
    bool inputActive,
    int scrollLineOffset)
{
    m_inputHistory = inputHistory;
    if (m_inputHistory.size() > kMaxInputHistory) {
        m_inputHistory.resize(kMaxInputHistory);
    }
    m_inputText = inputText;
    m_inputActive = inputActive ? 1 : 0;
    m_historyBrowseIndex = -1;
    m_historyDraft.clear();
    m_scrollLineOffset = scrollLineOffset;
    ClampScrollOffset();
    RefreshVisibleLines(GetTickCount());
    Invalidate();
}

void UINewChatWnd::StoreInfo()
{
    SaveUiWindowPlacement("ChatWnd", m_x, m_y);
}
