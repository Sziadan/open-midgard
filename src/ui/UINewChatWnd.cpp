#include "UINewChatWnd.h"

#include "UIWindowMgr.h"
#include "UiScale.h"
#include "core/SettingsIni.h"
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
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include <windows.h>

#if RO_PLATFORM_WINDOWS
#pragma comment(lib, "msimg32.lib")
#endif

namespace {
constexpr char kChatWndSection[] = "ChatWnd";
constexpr char kChatFontSizeValue[] = "FontPixelSize";
constexpr char kChatWindowOpacityValue[] = "OpacityPercent";
constexpr char kChatWindowWidthValue[] = "WindowWidth";
constexpr char kChatWindowHeightValue[] = "WindowHeight";
constexpr char kChatActiveTabValue[] = "ActiveTab";
constexpr char kChatTabMaskPrefix[] = "TabMask";

constexpr size_t kMaxChatLines = 50;
constexpr int kChatWindowMargin = 12;
constexpr int kChatDefaultWindowWidth = 486;
constexpr int kChatTabBarHeight = 34;
constexpr int kChatLineHeight = 19;
constexpr int kChatInputHeight = 28;
constexpr int kChatWhisperInputWidth = 126;
constexpr int kChatInputGap = 8;
constexpr int kChatPanelPadding = 10;
constexpr int kChatMessageGap = 2;
constexpr int kChatVisibleLineCount = 12;
constexpr int kChatHistoryHeight =
    (kChatLineHeight * kChatVisibleLineCount) + (kChatMessageGap * (kChatVisibleLineCount - 1));
constexpr int kChatDefaultWindowHeight = kChatTabBarHeight + kChatHistoryHeight + kChatInputHeight + (kChatPanelPadding * 4);
constexpr int kChatMinVisibleLineCount = 4;
constexpr int kChatMinHistoryHeight =
    (kChatLineHeight * kChatMinVisibleLineCount) + (kChatMessageGap * (kChatMinVisibleLineCount - 1));
constexpr int kChatWindowMinWidth = 360;
constexpr int kChatWindowMinHeight = kChatTabBarHeight + kChatMinHistoryHeight + kChatInputHeight + (kChatPanelPadding * 4);
constexpr int kChatResizeBorder = 6;
constexpr int kChatScrollbarWidth = 8;
constexpr int kChatScrollbarGap = 4;
constexpr int kChatTabGap = 6;
constexpr int kChatGearButtonSize = 24;
constexpr int kChatConfigPanelWidth = 336;
constexpr int kChatConfigPanelHeight = 392;
constexpr int kChatConfigListWidth = 92;
constexpr int kChatConfigButtonHeight = 28;
constexpr int kChatConfigButtonGap = 8;
constexpr int kChatSliderTrackHeight = 8;
constexpr int kChatSliderThumbWidth = 14;
constexpr int kChatFontSizeMin = 11;
constexpr int kChatFontSizeMax = 18;
constexpr int kChatFontSizeDefault = 13;
constexpr int kChatWindowOpacityMin = 20;
constexpr int kChatWindowOpacityMax = 100;
constexpr int kChatWindowOpacityDefault = 84;
constexpr size_t kMaxWhisperTargetChars = 24;
constexpr size_t kMaxInputChars = 180;
constexpr size_t kMaxInputHistory = 5;
constexpr int kChatWheelScrollLines = 3;
constexpr int kChatTabCount = 4;
constexpr int kChatFilterCount = 7;

enum ChatFilterId {
    ChatFilter_Normal = 0,
    ChatFilter_Player,
    ChatFilter_Whisper,
    ChatFilter_Party,
    ChatFilter_Broadcast,
    ChatFilter_Battlefield,
    ChatFilter_System,
};

enum ChatChannelId {
    ChatChannel_Normal = 0,
    ChatChannel_Player = 1,
    ChatChannel_Whisper = 2,
    ChatChannel_Party = 3,
    ChatChannel_Broadcast = 4,
    ChatChannel_Battlefield = 5,
    ChatChannel_System = 6,
};

enum ChatResizeEdge {
    ChatResizeEdge_None = 0,
    ChatResizeEdge_Left = 1 << 0,
    ChatResizeEdge_Top = 1 << 1,
    ChatResizeEdge_Right = 1 << 2,
    ChatResizeEdge_Bottom = 1 << 3,
};

struct ChatTabPreset {
    const char* label;
    u32 channelMask;
};

constexpr u32 ChatChannelBit(int channel)
{
    return channel >= 0 && channel < 32 ? (1u << channel) : 0u;
}

constexpr u32 kAllChatChannelsMask =
    ChatChannelBit(ChatChannel_Normal)
    | ChatChannelBit(ChatChannel_Player)
    | ChatChannelBit(ChatChannel_Whisper)
    | ChatChannelBit(ChatChannel_Party)
    | ChatChannelBit(ChatChannel_Broadcast)
    | ChatChannelBit(ChatChannel_Battlefield)
    | ChatChannelBit(ChatChannel_System);

constexpr std::array<ChatTabPreset, kChatTabCount> kChatTabPresets = {{
    { "All", kAllChatChannelsMask },
    { "Chat", ChatChannelBit(ChatChannel_Normal) | ChatChannelBit(ChatChannel_Player) | ChatChannelBit(ChatChannel_Whisper) },
    { "Group", ChatChannelBit(ChatChannel_Party) | ChatChannelBit(ChatChannel_Broadcast) | ChatChannelBit(ChatChannel_Battlefield) },
    { "Info", ChatChannelBit(ChatChannel_System) | ChatChannelBit(ChatChannel_Broadcast) | ChatChannelBit(ChatChannel_Whisper) },
}};

constexpr std::array<const char*, kChatFilterCount> kChatFilterLabels = {{
    "Normal",
    "Player",
    "Whisper",
    "Party",
    "Broadcast",
    "Battle",
    "System",
}};

constexpr std::array<u8, kChatFilterCount> kChatFilterChannels = {{
    ChatChannel_Normal,
    ChatChannel_Player,
    ChatChannel_Whisper,
    ChatChannel_Party,
    ChatChannel_Broadcast,
    ChatChannel_Battlefield,
    ChatChannel_System,
}};

int ClampChatWindowOpacityPercent(int value)
{
    return std::clamp(value, kChatWindowOpacityMin, kChatWindowOpacityMax);
}

struct ChatLayoutRects {
    RECT panel;
    RECT header;
    std::array<RECT, kChatTabCount> tabs;
    RECT gearButton;
    RECT history;
    RECT whisperInput;
    RECT messageInput;
    RECT configPanel;
    std::array<RECT, kChatTabCount> configTabs;
    RECT fontMinusButton;
    RECT fontPlusButton;
    RECT fontValue;
    RECT transparencyValue;
    RECT transparencyTrack;
    RECT transparencyThumb;
    std::array<RECT, kChatFilterCount> filterButtons;
    RECT resetButton;
};

int ClampChatFontPixelSize(int value)
{
    return std::clamp(value, kChatFontSizeMin, kChatFontSizeMax);
}

bool PointInRectXY(const RECT& rc, int x, int y)
{
    return x >= rc.left && x < rc.right && y >= rc.top && y < rc.bottom;
}

bool PointInExpandedRectXY(const RECT& rc, int x, int y, int expand)
{
    RECT expanded = {
        rc.left - expand,
        rc.top - expand,
        rc.right + expand,
        rc.bottom + expand
    };
    return PointInRectXY(expanded, x, y);
}

bool RectIsValid(const RECT& rc)
{
    return rc.right > rc.left && rc.bottom > rc.top;
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
QFont BuildChatUiFont(int pixelSize)
{
    QFont font(QStringLiteral("Segoe UI"));
    font.setPixelSize(ClampChatFontPixelSize(pixelSize));
    font.setStyleStrategy(QFont::PreferAntialias);
    return font;
}

int MeasureWrappedTextHeightQt(const std::string& text, int width, int pixelSize)
{
    if (width <= 0 || text.empty()) {
        return kChatLineHeight;
    }

    const QFont font = BuildChatUiFont(pixelSize);
    const QFontMetrics metrics(font);
    const QString label = QString::fromLocal8Bit(text.c_str());
    const QRect bounds = metrics.boundingRect(QRect(0, 0, width, 4096), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, label);
    return (std::max)(kChatLineHeight, bounds.height());
}

void DrawChatTextQt(HDC hdc, const RECT& rect, const std::string& text, COLORREF color, bool wrap, int pixelSize)
{
    if (!hdc || rect.right <= rect.left || rect.bottom <= rect.top) {
        return;
    }

    QString label = QString::fromLocal8Bit(text.c_str());
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    const QFont font = BuildChatUiFont(pixelSize);
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

int MeasureWrappedTextHeight(const std::string& text, int width, int pixelSize)
{
    if (width <= 0 || text.empty()) {
        return kChatLineHeight;
    }

#if RO_ENABLE_QT6_UI
    return MeasureWrappedTextHeightQt(text, width, pixelSize);
#else
    HDC hdc = AcquireMainWindowDrawTarget();
    if (!hdc) {
        return kChatLineHeight;
    }
    RECT calcRect{ 0, 0, width, 0 };
    const int savedDc = SaveDC(hdc);
    SelectObject(hdc, GetStockObject(DEFAULT_GUI_FONT));
    DrawTextA(hdc, text.c_str(), -1, &calcRect, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_EDITCONTROL | DT_CALCRECT | DT_NOPREFIX);
    RestoreDC(hdc, savedDc);
    ReleaseMainWindowDrawTarget(hdc);
    const int measuredHeight = calcRect.bottom - calcRect.top;
    return (std::max)(kChatLineHeight, measuredHeight);
#endif
}

HFONT GetChatUiFont(int pixelSize)
{
    static std::array<HFONT, kChatFontSizeMax + 1> s_chatUiFonts{};
    const int clampedPixelSize = ClampChatFontPixelSize(pixelSize);
    HFONT& cachedFont = s_chatUiFonts[static_cast<size_t>(clampedPixelSize)];
    if (!cachedFont) {
        cachedFont = CreateFontA(
            -clampedPixelSize,
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
            CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE,
            "Segoe UI");
    }
    return cachedFont;
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

RECT GetChatLogicalClientRect()
{
    RECT clientRect{ 0, 0, WINDOW_WIDTH > 0 ? WINDOW_WIDTH : 640, WINDOW_HEIGHT > 0 ? WINDOW_HEIGHT : 480 };
    if (g_hMainWnd) {
        GetClientRect(g_hMainWnd, &clientRect);
    }

    const float scaleFactor = IsQtUiRuntimeEnabled() ? GetConfiguredUiScaleFactor() : 1.0f;
    const float normalizedScale = scaleFactor > 0.0f ? scaleFactor : 1.0f;
    const int rawWidth = clientRect.right - clientRect.left;
    const int rawHeight = clientRect.bottom - clientRect.top;
    int logicalWidth = (std::max)(1, static_cast<int>(std::floor(static_cast<float>(rawWidth) / normalizedScale)));
    int logicalHeight = (std::max)(1, static_cast<int>(std::floor(static_cast<float>(rawHeight) / normalizedScale)));
    if (WINDOW_WIDTH > 0) {
        logicalWidth = (std::min)(logicalWidth, WINDOW_WIDTH);
    }
    if (WINDOW_HEIGHT > 0) {
        logicalHeight = (std::min)(logicalHeight, WINDOW_HEIGHT);
    }

    RECT logicalRect{};
    logicalRect.left = 0;
    logicalRect.top = 0;
    logicalRect.right = logicalWidth;
    logicalRect.bottom = logicalHeight;
    return logicalRect;
}

void ClampChatWindowPosition(int* x, int* y, int w, int h)
{
    if (!x || !y) {
        return;
    }

    const RECT logicalClientRect = GetChatLogicalClientRect();
    const int minX = static_cast<int>(logicalClientRect.left);
    const int minY = static_cast<int>(logicalClientRect.top);
    const int maxX = (std::max)(minX, static_cast<int>(logicalClientRect.right) - (std::max)(1, w));
    const int maxY = (std::max)(minY, static_cast<int>(logicalClientRect.bottom) - (std::max)(1, h));
    *x = std::clamp(*x, minX, maxX);
    *y = std::clamp(*y, minY, maxY);
}

void GetChatWindowSizeLimits(int* outMinWidth, int* outMinHeight, int* outMaxWidth, int* outMaxHeight)
{
    const RECT logicalClientRect = GetChatLogicalClientRect();
    const int availableWidth = (std::max)(1, static_cast<int>(logicalClientRect.right - logicalClientRect.left));
    const int availableHeight = (std::max)(1, static_cast<int>(logicalClientRect.bottom - logicalClientRect.top));
    if (outMaxWidth) {
        *outMaxWidth = availableWidth;
    }
    if (outMaxHeight) {
        *outMaxHeight = availableHeight;
    }
    if (outMinWidth) {
        *outMinWidth = (std::min)(kChatWindowMinWidth, availableWidth);
    }
    if (outMinHeight) {
        *outMinHeight = (std::min)(kChatWindowMinHeight, availableHeight);
    }
}

int GetChatResizeEdgesForPoint(const ChatLayoutRects& layout, int x, int y)
{
    if (!PointInExpandedRectXY(layout.panel, x, y, kChatResizeBorder)) {
        return ChatResizeEdge_None;
    }

    int resizeEdges = ChatResizeEdge_None;
    if (x < layout.panel.left + kChatResizeBorder) {
        resizeEdges |= ChatResizeEdge_Left;
    } else if (x >= layout.panel.right - kChatResizeBorder) {
        resizeEdges |= ChatResizeEdge_Right;
    }

    if (y < layout.panel.top + kChatResizeBorder) {
        resizeEdges |= ChatResizeEdge_Top;
    } else if (y >= layout.panel.bottom - kChatResizeBorder) {
        resizeEdges |= ChatResizeEdge_Bottom;
    }

    return resizeEdges;
}

ChatLayoutRects BuildChatLayoutRects(int x, int y, int w, int h)
{
    ChatLayoutRects rects{};
    rects.panel = { x, y, x + w, y + h };

    rects.header = {
        x + kChatPanelPadding,
        y + kChatPanelPadding,
        x + w - kChatPanelPadding,
        y + kChatPanelPadding + kChatTabBarHeight
    };

    rects.gearButton = {
        rects.header.right - kChatGearButtonSize,
        rects.header.top + (kChatTabBarHeight - kChatGearButtonSize) / 2,
        rects.header.right,
        rects.header.top + (kChatTabBarHeight - kChatGearButtonSize) / 2 + kChatGearButtonSize
    };

    const int tabsLeft = rects.header.left;
    const int tabsRight = rects.gearButton.left - kChatTabGap;
    const int availableTabWidth = (std::max)(0, tabsRight - tabsLeft - (kChatTabGap * (kChatTabCount - 1)));
    const int tabWidth = kChatTabCount > 0 ? availableTabWidth / kChatTabCount : 0;
    int tabX = tabsLeft;
    for (int index = 0; index < kChatTabCount; ++index) {
        const int right = (index + 1 == kChatTabCount) ? tabsRight : tabX + tabWidth;
        rects.tabs[static_cast<size_t>(index)] = {
            tabX,
            rects.header.top + 2,
            right,
            rects.header.bottom - 2
        };
        tabX = right + kChatTabGap;
    }

    const int inputLeft = x + kChatPanelPadding;
    const int inputRight = x + w - kChatPanelPadding;
    const int inputTop = y + h - kChatInputHeight - kChatPanelPadding;
    const int inputBottom = y + h - kChatPanelPadding;
    const int totalInputWidth = (std::max)(0, inputRight - inputLeft);
    int whisperWidth = (std::min)(kChatWhisperInputWidth, (std::max)(80, totalInputWidth / 3));
    whisperWidth = (std::min)(whisperWidth, (std::max)(80, totalInputWidth - (48 + kChatInputGap)));

    rects.whisperInput = {
        inputLeft,
        inputTop,
        inputLeft + whisperWidth,
        inputBottom
    };
    rects.messageInput = {
        rects.whisperInput.right + kChatInputGap,
        inputTop,
        inputRight,
        inputBottom
    };
    rects.history = {
        inputLeft,
        rects.header.bottom + kChatPanelPadding,
        inputRight,
        inputTop - kChatPanelPadding
    };

    const RECT clientRect = GetChatLogicalClientRect();
    rects.configPanel = {
        rects.panel.right + 10,
        rects.panel.top,
        rects.panel.right + 10 + kChatConfigPanelWidth,
        rects.panel.top + kChatConfigPanelHeight
    };
    if (rects.configPanel.right > clientRect.right - 8) {
        rects.configPanel.left = (std::max)(8, static_cast<int>(rects.panel.right) - kChatConfigPanelWidth);
        rects.configPanel.right = rects.configPanel.left + kChatConfigPanelWidth;
    }
    if (rects.configPanel.bottom > clientRect.bottom - 8) {
        rects.configPanel.top = (std::max)(8, static_cast<int>(clientRect.bottom) - 8 - kChatConfigPanelHeight);
        rects.configPanel.bottom = rects.configPanel.top + kChatConfigPanelHeight;
    }

    const int configLeft = rects.configPanel.left + 12;
    const int configTabsTop = rects.configPanel.top + 72;
    for (int index = 0; index < kChatTabCount; ++index) {
        const int rowTop = configTabsTop + index * (kChatConfigButtonHeight + 6);
        rects.configTabs[static_cast<size_t>(index)] = {
            configLeft,
            rowTop,
            configLeft + kChatConfigListWidth,
            rowTop + kChatConfigButtonHeight
        };
    }

    const int editorLeft = rects.configPanel.left + 120;
    rects.fontValue = {
        editorLeft,
        rects.configPanel.top + 72,
        rects.configPanel.right - 12,
        rects.configPanel.top + 72 + kChatConfigButtonHeight
    };
    rects.fontMinusButton = {
        rects.configPanel.right - 76,
        rects.fontValue.top,
        rects.configPanel.right - 48,
        rects.fontValue.bottom
    };
    rects.fontPlusButton = {
        rects.configPanel.right - 40,
        rects.fontValue.top,
        rects.configPanel.right - 12,
        rects.fontValue.bottom
    };

    rects.transparencyValue = {
        editorLeft,
        rects.configPanel.top + 128,
        rects.configPanel.right - 12,
        rects.configPanel.top + 128 + kChatConfigButtonHeight
    };
    rects.transparencyTrack = {
        rects.configPanel.left + 130,
        rects.configPanel.top + 168,
        rects.configPanel.right - 22,
        rects.configPanel.top + 168 + kChatSliderTrackHeight
    };
    rects.transparencyThumb = {
        rects.transparencyTrack.left,
        rects.transparencyTrack.top - 5,
        rects.transparencyTrack.left + kChatSliderThumbWidth,
        rects.transparencyTrack.bottom + 5
    };

    const int filterWidth = ((rects.configPanel.right - 12) - editorLeft - kChatConfigButtonGap) / 2;
    const int filtersTop = rects.configPanel.top + 216;
    for (int index = 0; index < kChatFilterCount; ++index) {
        const int column = index % 2;
        const int row = index / 2;
        const int left = editorLeft + column * (filterWidth + kChatConfigButtonGap);
        const int top = filtersTop + row * (kChatConfigButtonHeight + 8);
        rects.filterButtons[static_cast<size_t>(index)] = {
            left,
            top,
            left + filterWidth,
            top + kChatConfigButtonHeight
        };
    }
    rects.resetButton = {
        rects.configPanel.right - 116,
        rects.configPanel.bottom - 40,
        rects.configPanel.right - 12,
        rects.configPanel.bottom - 12
    };
    return rects;
}

} // namespace

UINewChatWnd::UINewChatWnd()
    : m_lastDrawTick(0),
      m_activeInputField(InputField_None),
      m_historyBrowseIndex(-1),
      m_scrollLineOffset(0),
      m_firstVisibleLineIndex(0),
    m_activeTab(0),
    m_fontPixelSize(kChatFontSizeDefault),
    m_windowOpacityPercent(kChatWindowOpacityDefault),
    m_configVisible(0),
      m_dragStartGlobalX(0),
      m_dragStartGlobalY(0),
      m_dragStartWindowX(0),
      m_dragStartWindowY(0),
      m_dragStartWindowW(0),
      m_dragStartWindowH(0),
      m_dragArmed(0),
      m_isDragging(0),
      m_resizeEdges(ChatResizeEdge_None),
      m_transparencyDragActive(0),
      m_tabChannelMasks{}
{
    ResetTabDefaults();
    int initialWidth = kChatDefaultWindowWidth;
    int initialHeight = kChatDefaultWindowHeight;
    LoadSettings(&initialWidth, &initialHeight);

    Create(initialWidth, initialHeight);

    int defaultX = kChatWindowMargin;
    const RECT logicalClientRect = GetChatLogicalClientRect();
    int defaultY = logicalClientRect.bottom - initialHeight - kChatWindowMargin;
    ClampChatWindowPosition(&defaultX, &defaultY, initialWidth, initialHeight);

    Move(defaultX, defaultY);

    int savedX = m_x;
    int savedY = m_y;
    if (LoadUiWindowPlacement("ChatWnd", &savedX, &savedY)) {
        ClampChatWindowPosition(&savedX, &savedY, m_w, m_h);
        Move(savedX, savedY);
    }
}

UINewChatWnd::~UINewChatWnd() = default;

void UINewChatWnd::LoadSettings(int* outWindowWidth, int* outWindowHeight)
{
    ResetTabDefaults();

    int storedValue = 0;
    if (TryLoadSettingsIniInt(kChatWndSection, kChatFontSizeValue, &storedValue)) {
        m_fontPixelSize = ClampChatFontPixelSize(storedValue);
    }
    if (TryLoadSettingsIniInt(kChatWndSection, kChatWindowOpacityValue, &storedValue)) {
        m_windowOpacityPercent = ClampChatWindowOpacityPercent(storedValue);
    }
    if (TryLoadSettingsIniInt(kChatWndSection, kChatActiveTabValue, &storedValue)) {
        m_activeTab = std::clamp(storedValue, 0, kChatTabCount - 1);
    }

    char keyName[32] = {};
    for (int index = 0; index < kChatTabCount; ++index) {
        std::snprintf(keyName, sizeof(keyName), "%s%d", kChatTabMaskPrefix, index);
        if (TryLoadSettingsIniInt(kChatWndSection, keyName, &storedValue)) {
            m_tabChannelMasks[static_cast<size_t>(index)] = static_cast<u32>(storedValue) & kAllChatChannelsMask;
        }
        if (m_tabChannelMasks[static_cast<size_t>(index)] == 0u) {
            ResetTabDefault(index);
        }
    }

    int storedWidth = kChatDefaultWindowWidth;
    int storedHeight = kChatDefaultWindowHeight;
    if (TryLoadSettingsIniInt(kChatWndSection, kChatWindowWidthValue, &storedValue)) {
        storedWidth = storedValue;
    }
    if (TryLoadSettingsIniInt(kChatWndSection, kChatWindowHeightValue, &storedValue)) {
        storedHeight = storedValue;
    }

    int minWidth = kChatWindowMinWidth;
    int minHeight = kChatWindowMinHeight;
    int maxWidth = kChatDefaultWindowWidth;
    int maxHeight = kChatDefaultWindowHeight;
    GetChatWindowSizeLimits(&minWidth, &minHeight, &maxWidth, &maxHeight);
    storedWidth = std::clamp(storedWidth, minWidth, maxWidth);
    storedHeight = std::clamp(storedHeight, minHeight, maxHeight);
    if (outWindowWidth) {
        *outWindowWidth = storedWidth;
    }
    if (outWindowHeight) {
        *outWindowHeight = storedHeight;
    }
}

void UINewChatWnd::SaveSettings() const
{
    SaveSettingsIniInt(kChatWndSection, kChatFontSizeValue, m_fontPixelSize);
    SaveSettingsIniInt(kChatWndSection, kChatWindowOpacityValue, m_windowOpacityPercent);
    SaveSettingsIniInt(kChatWndSection, kChatWindowWidthValue, m_w);
    SaveSettingsIniInt(kChatWndSection, kChatWindowHeightValue, m_h);
    SaveSettingsIniInt(kChatWndSection, kChatActiveTabValue, m_activeTab);

    char keyName[32] = {};
    for (int index = 0; index < kChatTabCount; ++index) {
        std::snprintf(keyName, sizeof(keyName), "%s%d", kChatTabMaskPrefix, index);
        SaveSettingsIniInt(kChatWndSection, keyName, static_cast<int>(m_tabChannelMasks[static_cast<size_t>(index)]));
    }
}

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

bool UINewChatWnd::HandleQtMouseDown(int x, int y)
{
    return HandlePointerDown(x, y);
}

bool UINewChatWnd::HandleQtMouseMove(int x, int y)
{
    return HandlePointerMove(x, y);
}

bool UINewChatWnd::HandleQtMouseUp(int x, int y)
{
    return HandlePointerUp(x, y);
}

bool UINewChatWnd::HandleQtWheel(int delta, int x, int y)
{
    return HandlePointerWheel(delta, x, y);
}

bool UINewChatWnd::IsQtInteractionPoint(int x, int y) const
{
    if (m_show == 0) {
        return false;
    }

    const ChatLayoutRects layout = BuildChatLayoutRects(m_x, m_y, m_w, m_h);
    if (PointInRectXY(layout.panel, x, y) || GetChatResizeEdgesForPoint(layout, x, y) != ChatResizeEdge_None) {
        return true;
    }
    return m_configVisible != 0 && RectIsValid(layout.configPanel) && PointInRectXY(layout.configPanel, x, y);
}

bool UINewChatWnd::IsQtMainPanelPoint(int x, int y) const
{
    if (m_show == 0) {
        return false;
    }

    const ChatLayoutRects layout = BuildChatLayoutRects(m_x, m_y, m_w, m_h);
    return PointInRectXY(layout.panel, x, y);
}

bool UINewChatWnd::IsQtPointerCaptured() const
{
    return m_dragArmed != 0 || m_isDragging != 0 || m_transparencyDragActive != 0;
}

void UINewChatWnd::ClearInputFocus()
{
    SetActiveInputField(InputField_None);
}

const std::vector<ChatLine>& UINewChatWnd::GetLines() const
{
    return m_lines;
}

const std::vector<ChatLine>& UINewChatWnd::GetVisibleLines() const
{
    return m_visibleLines;
}

const std::string& UINewChatWnd::GetWhisperTargetText() const
{
    return m_whisperTargetText;
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
    for (const ChatLine& line : m_lines) {
        if ((m_tabChannelMasks[static_cast<size_t>(m_activeTab)] & ChatChannelBit(line.channel)) != 0u) {
            ++state.totalLines;
        }
    }
    state.firstVisibleLine = m_firstVisibleLineIndex;
    state.visibleLineCount = static_cast<int>(m_visibleLines.size());
    state.visible = (state.totalLines > state.visibleLineCount) ? 1 : 0;
    return state;
}

int UINewChatWnd::GetFontPixelSize() const
{
    return m_fontPixelSize;
}

int UINewChatWnd::GetWindowOpacityPercent() const
{
    return m_windowOpacityPercent;
}

bool UINewChatWnd::IsConfigVisible() const
{
    return m_configVisible != 0;
}

bool UINewChatWnd::GetConfigRectForQt(int* x, int* y, int* width, int* height) const
{
    if (!x || !y || !width || !height || m_configVisible == 0) {
        return false;
    }

    const ChatLayoutRects layout = BuildChatLayoutRects(m_x, m_y, m_w, m_h);
    *x = layout.configPanel.left;
    *y = layout.configPanel.top;
    *width = layout.configPanel.right - layout.configPanel.left;
    *height = layout.configPanel.bottom - layout.configPanel.top;
    return true;
}

int UINewChatWnd::GetTabCount() const
{
    return kChatTabCount;
}

bool UINewChatWnd::GetTabDisplay(int index, ChatTabDisplay* outData) const
{
    if (!outData || index < 0 || index >= kChatTabCount) {
        return false;
    }

    outData->id = index;
    outData->label = kChatTabPresets[static_cast<size_t>(index)].label;
    outData->active = index == m_activeTab ? 1 : 0;
    outData->channelMask = m_tabChannelMasks[static_cast<size_t>(index)];
    return true;
}

int UINewChatWnd::GetFilterOptionCount() const
{
    return kChatFilterCount;
}

bool UINewChatWnd::GetFilterOptionDisplay(int index, ChatFilterOptionDisplay* outData) const
{
    if (!outData || index < 0 || index >= kChatFilterCount) {
        return false;
    }

    const u32 channelBit = ChatChannelBit(kChatFilterChannels[static_cast<size_t>(index)]);
    outData->id = index;
    outData->label = kChatFilterLabels[static_cast<size_t>(index)];
    outData->enabled = (m_tabChannelMasks[static_cast<size_t>(m_activeTab)] & channelBit) != 0u ? 1 : 0;
    return true;
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
    SelectObject(hdc, GetChatUiFont(m_fontPixelSize));

    const ChatLayoutRects layout = BuildChatLayoutRects(m_x, m_y, m_w, m_h);
    RECT panelRc = layout.panel;
    RECT historyRc = layout.history;
    RECT whisperInputRc = layout.whisperInput;
    RECT messageInputRc = layout.messageInput;
    const RECT topStrip = { panelRc.left, panelRc.top, panelRc.right, historyRc.top };
    const RECT leftStrip = { panelRc.left, historyRc.top, historyRc.left, panelRc.bottom };
    const RECT rightStrip = { historyRc.right, historyRc.top, panelRc.right, panelRc.bottom };
    const RECT middleStrip = { panelRc.left, historyRc.bottom, panelRc.right, whisperInputRc.top };
    const RECT bottomStrip = { panelRc.left, whisperInputRc.bottom, panelRc.right, panelRc.bottom };
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
        measuredHeights[index] = MeasureWrappedTextHeight(m_visibleLines[index].text, textWidth, m_fontPixelSize);
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
        DrawChatTextQt(hdc, textRc, line.text, ToColorRef(line.color), true, m_fontPixelSize);
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

    HBRUSH whisperBg = CreateSolidBrush(IsWhisperTargetActive() ? RGB(245, 245, 220) : RGB(210, 210, 210));
    FillRect(hdc, &whisperInputRc, whisperBg);
    DeleteObject(whisperBg);
    FrameRect(hdc, &whisperInputRc, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));

    HBRUSH messageBg = CreateSolidBrush(IsMessageInputActive() ? RGB(245, 245, 220) : RGB(210, 210, 210));
    FillRect(hdc, &messageInputRc, messageBg);
    DeleteObject(messageBg);
    FrameRect(hdc, &messageInputRc, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));

    std::string whisperDrawText = m_whisperTargetText;
    COLORREF whisperTextColor = RGB(16, 16, 16);
    if (whisperDrawText.empty() && !IsWhisperTargetActive()) {
        whisperDrawText = "To";
        whisperTextColor = RGB(96, 96, 96);
    }
    if (IsWhisperTargetActive()) {
        whisperDrawText += '_';
    }

    std::string messageDrawText = m_inputText;
    if (IsMessageInputActive()) {
        messageDrawText += '_';
    }

    RECT whisperTextRc = { whisperInputRc.left + 4, whisperInputRc.top + 2, whisperInputRc.right - 2, whisperInputRc.bottom - 2 };
    SetTextColor(hdc, whisperTextColor);
#if RO_ENABLE_QT6_UI
    DrawChatTextQt(hdc, whisperTextRc, whisperDrawText, whisperTextColor, false, m_fontPixelSize);
#else
    DrawTextA(hdc, whisperDrawText.c_str(), -1, &whisperTextRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
#endif

    RECT messageTextRc = { messageInputRc.left + 4, messageInputRc.top + 2, messageInputRc.right - 2, messageInputRc.bottom - 2 };
    SetTextColor(hdc, RGB(16, 16, 16));
#if RO_ENABLE_QT6_UI
    DrawChatTextQt(hdc, messageTextRc, messageDrawText, RGB(16, 16, 16), false, m_fontPixelSize);
#else
    DrawTextA(hdc, messageDrawText.c_str(), -1, &messageTextRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
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
        std::vector<const ChatLine*> filteredLines;
        filteredLines.reserve(m_lines.size());
        const u32 activeMask = m_tabChannelMasks[static_cast<size_t>(m_activeTab)];
        for (const ChatLine& line : m_lines) {
            if ((activeMask & ChatChannelBit(line.channel)) != 0u) {
                filteredLines.push_back(&line);
            }
        }

        const ChatLayoutRects layout = BuildChatLayoutRects(m_x, m_y, m_w, m_h);
        const ChatScrollBarState scrollState = GetScrollBarState();
        const int reservedScrollbarWidth = scrollState.visible ? (kChatScrollbarWidth + kChatScrollbarGap) : 0;
        const int textWidth = (layout.history.right - layout.history.left) - 8 - reservedScrollbarWidth;
        const int availableHeight = layout.history.bottom - layout.history.top - 8;
        const int endExclusive = (std::max)(0, static_cast<int>(filteredLines.size()) - m_scrollLineOffset);
        int usedHeight = 0;
        int firstVisibleIndex = endExclusive;
        for (int index = endExclusive - 1; index >= 0; --index) {
            const int measured = MeasureWrappedTextHeight(filteredLines[static_cast<size_t>(index)]->text, textWidth, m_fontPixelSize);
            const int blockHeight = measured + ((usedHeight > 0) ? kChatMessageGap : 0);
            if (usedHeight + blockHeight > availableHeight) {
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
            nextVisible.push_back(*filteredLines[static_cast<size_t>(index)]);
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

bool UINewChatWnd::HandlePointerDown(int x, int y)
{
    const ChatLayoutRects layout = BuildChatLayoutRects(m_x, m_y, m_w, m_h);

    if (m_configVisible != 0 && RectIsValid(layout.configPanel) && PointInRectXY(layout.configPanel, x, y)) {
        for (int index = 0; index < kChatTabCount; ++index) {
            if (PointInRectXY(layout.configTabs[static_cast<size_t>(index)], x, y)) {
                SetActiveTab(index);
                return true;
            }
        }
        if (PointInRectXY(layout.fontMinusButton, x, y)) {
            m_fontPixelSize = ClampChatFontPixelSize(m_fontPixelSize - 1);
            SaveSettings();
            RefreshVisibleLines(GetTickCount());
            Invalidate();
            return true;
        }
        if (PointInRectXY(layout.fontPlusButton, x, y)) {
            m_fontPixelSize = ClampChatFontPixelSize(m_fontPixelSize + 1);
            SaveSettings();
            RefreshVisibleLines(GetTickCount());
            Invalidate();
            return true;
        }
        if (PointInRectXY(layout.transparencyValue, x, y) || PointInRectXY(layout.transparencyTrack, x, y)) {
            m_transparencyDragActive = 1;
            UpdateWindowOpacityFromPointer(x, layout.transparencyTrack);
            return true;
        }
        for (int index = 0; index < kChatFilterCount; ++index) {
            if (PointInRectXY(layout.filterButtons[static_cast<size_t>(index)], x, y)) {
                const u32 channelBit = ChatChannelBit(kChatFilterChannels[static_cast<size_t>(index)]);
                m_tabChannelMasks[static_cast<size_t>(m_activeTab)] ^= channelBit;
                if (m_tabChannelMasks[static_cast<size_t>(m_activeTab)] == 0u) {
                    m_tabChannelMasks[static_cast<size_t>(m_activeTab)] = channelBit;
                }
                SaveSettings();
                ClampScrollOffset();
                RefreshVisibleLines(GetTickCount());
                Invalidate();
                return true;
            }
        }
        if (PointInRectXY(layout.resetButton, x, y)) {
            ResetTabDefault(m_activeTab);
            SaveSettings();
            ClampScrollOffset();
            RefreshVisibleLines(GetTickCount());
            Invalidate();
            return true;
        }
        return true;
    }

    const int resizeEdges = GetChatResizeEdgesForPoint(layout, x, y);
    if (resizeEdges != ChatResizeEdge_None) {
        m_dragArmed = 1;
        m_isDragging = 0;
        m_resizeEdges = resizeEdges;
        m_dragStartGlobalX = x;
        m_dragStartGlobalY = y;
        m_dragStartWindowX = m_x;
        m_dragStartWindowY = m_y;
        m_dragStartWindowW = m_w;
        m_dragStartWindowH = m_h;
        return true;
    }

    if (!PointInRectXY(layout.panel, x, y)) {
        return false;
    }

    if (PointInRectXY(layout.gearButton, x, y)) {
        SetConfigVisible(m_configVisible == 0);
        return true;
    }

    for (int index = 0; index < kChatTabCount; ++index) {
        if (PointInRectXY(layout.tabs[static_cast<size_t>(index)], x, y)) {
            SetActiveTab(index);
            return true;
        }
    }

    if (PointInRectXY(layout.whisperInput, x, y)) {
        SetActiveInputField(InputField_WhisperTarget);
        m_dragArmed = 0;
        m_isDragging = 0;
        return true;
    }

    if (PointInRectXY(layout.messageInput, x, y)) {
        SetActiveInputField(InputField_Message);
        m_dragArmed = 0;
        m_isDragging = 0;
        return true;
    }

    if (PointInRectXY(layout.header, x, y)) {
        m_dragArmed = 1;
        m_isDragging = 0;
        m_resizeEdges = ChatResizeEdge_None;
        m_dragStartGlobalX = x;
        m_dragStartGlobalY = y;
        m_dragStartWindowX = m_x;
        m_dragStartWindowY = m_y;
        m_dragStartWindowW = m_w;
        m_dragStartWindowH = m_h;
        return true;
    }

    if (PointInRectXY(layout.history, x, y)) {
        SetActiveInputField(InputField_Message);
        m_dragArmed = 1;
        m_isDragging = 0;
        m_resizeEdges = ChatResizeEdge_None;
        m_dragStartGlobalX = x;
        m_dragStartGlobalY = y;
        m_dragStartWindowX = m_x;
        m_dragStartWindowY = m_y;
        m_dragStartWindowW = m_w;
        m_dragStartWindowH = m_h;
        return true;
    }

    return true;
}

bool UINewChatWnd::HandlePointerMove(int x, int y)
{
    if (m_transparencyDragActive != 0) {
        const ChatLayoutRects layout = BuildChatLayoutRects(m_x, m_y, m_w, m_h);
        UpdateWindowOpacityFromPointer(x, layout.transparencyTrack);
        return true;
    }

    if (m_dragArmed == 0 && m_isDragging == 0) {
        return IsQtInteractionPoint(x, y);
    }

    const int dx = x - m_dragStartGlobalX;
    const int dy = y - m_dragStartGlobalY;
    if (m_isDragging == 0 && ((dx * dx) + (dy * dy)) >= 16) {
        m_isDragging = 1;
    }

    if (m_isDragging == 0) {
        return true;
    }

    if (m_resizeEdges != ChatResizeEdge_None) {
        int minWidth = kChatWindowMinWidth;
        int minHeight = kChatWindowMinHeight;
        int maxWidth = kChatDefaultWindowWidth;
        int maxHeight = kChatDefaultWindowHeight;
        GetChatWindowSizeLimits(&minWidth, &minHeight, &maxWidth, &maxHeight);

        const RECT logicalClientRect = GetChatLogicalClientRect();
        int nextX = m_dragStartWindowX;
        int nextY = m_dragStartWindowY;
        int nextW = m_dragStartWindowW;
        int nextH = m_dragStartWindowH;
        const int startRight = m_dragStartWindowX + m_dragStartWindowW;
        const int startBottom = m_dragStartWindowY + m_dragStartWindowH;

        if ((m_resizeEdges & ChatResizeEdge_Left) != 0) {
            const int minLeft = startRight - maxWidth;
            const int maxLeft = startRight - minWidth;
            nextX = std::clamp(m_dragStartWindowX + dx, minLeft, maxLeft);
            nextX = (std::max)(static_cast<int>(logicalClientRect.left), nextX);
            nextW = startRight - nextX;
        } else if ((m_resizeEdges & ChatResizeEdge_Right) != 0) {
            const int maxRight = (std::min)(static_cast<int>(logicalClientRect.right), m_dragStartWindowX + maxWidth);
            const int minRight = m_dragStartWindowX + minWidth;
            const int nextRight = std::clamp(startRight + dx, minRight, maxRight);
            nextW = nextRight - m_dragStartWindowX;
        }

        if ((m_resizeEdges & ChatResizeEdge_Top) != 0) {
            const int minTop = startBottom - maxHeight;
            const int maxTop = startBottom - minHeight;
            nextY = std::clamp(m_dragStartWindowY + dy, minTop, maxTop);
            nextY = (std::max)(static_cast<int>(logicalClientRect.top), nextY);
            nextH = startBottom - nextY;
        } else if ((m_resizeEdges & ChatResizeEdge_Bottom) != 0) {
            const int maxBottom = (std::min)(static_cast<int>(logicalClientRect.bottom), m_dragStartWindowY + maxHeight);
            const int minBottom = m_dragStartWindowY + minHeight;
            const int nextBottom = std::clamp(startBottom + dy, minBottom, maxBottom);
            nextH = nextBottom - m_dragStartWindowY;
        }

        nextW = std::clamp(nextW, minWidth, maxWidth);
        nextH = std::clamp(nextH, minHeight, maxHeight);
        ClampChatWindowPosition(&nextX, &nextY, nextW, nextH);

        const bool sizeChanged = nextW != m_w || nextH != m_h;
        if (sizeChanged) {
            Resize(nextW, nextH);
        }
        if (nextX != m_x || nextY != m_y) {
            Move(nextX, nextY);
        }
        if (sizeChanged) {
            RefreshVisibleLines(GetTickCount());
        }
        return true;
    }

    int snappedX = m_dragStartWindowX + dx;
    int snappedY = m_dragStartWindowY + dy;
    g_windowMgr.SnapWindowToNearby(this, &snappedX, &snappedY);
    ClampChatWindowPosition(&snappedX, &snappedY, m_w, m_h);
    Move(snappedX, snappedY);
    return true;
}

bool UINewChatWnd::HandlePointerUp(int x, int y)
{
    const bool hadCapture = m_dragArmed != 0 || m_isDragging != 0 || m_transparencyDragActive != 0;
    if (m_transparencyDragActive != 0) {
        const ChatLayoutRects layout = BuildChatLayoutRects(m_x, m_y, m_w, m_h);
        UpdateWindowOpacityFromPointer(x, layout.transparencyTrack);
    }
    if (m_isDragging != 0) {
        StoreInfo();
    } else if (m_dragArmed != 0 && m_resizeEdges == ChatResizeEdge_None) {
        const ChatLayoutRects layout = BuildChatLayoutRects(m_x, m_y, m_w, m_h);
        if (PointInRectXY(layout.panel, x, y)
            && !PointInRectXY(layout.whisperInput, x, y)
            && !PointInRectXY(layout.messageInput, x, y)) {
            SetActiveInputField(InputField_Message);
        }
    }

    m_dragArmed = 0;
    m_isDragging = 0;
    m_resizeEdges = ChatResizeEdge_None;
    m_transparencyDragActive = 0;
    return hadCapture || IsQtInteractionPoint(x, y);
}

bool UINewChatWnd::HandlePointerWheel(int delta, int x, int y)
{
    if (!IsQtInteractionPoint(x, y)) {
        return false;
    }

    const ChatLayoutRects layout = BuildChatLayoutRects(m_x, m_y, m_w, m_h);
    if (m_configVisible != 0 && RectIsValid(layout.configPanel) && PointInRectXY(layout.configPanel, x, y)
        && !PointInRectXY(layout.panel, x, y)) {
        return true;
    }

    if (delta > 0) {
        AdjustScroll(kChatWheelScrollLines);
    } else if (delta < 0) {
        AdjustScroll(-kChatWheelScrollLines);
    }
    return true;
}

void UINewChatWnd::OnLBtnDown(int x, int y)
{
    HandlePointerDown(x, y);
}

void UINewChatWnd::OnMouseMove(int x, int y)
{
    HandlePointerMove(x, y);
}

void UINewChatWnd::OnLBtnUp(int x, int y)
{
    HandlePointerUp(x, y);
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
    if (virtualKey == VK_ESCAPE && m_activeInputField == InputField_None && m_configVisible != 0) {
        SetConfigVisible(false);
        return true;
    }

    if (virtualKey == VK_TAB) {
        if (m_activeInputField == InputField_WhisperTarget) {
            SetActiveInputField(InputField_Message);
        } else {
            SetActiveInputField(InputField_WhisperTarget);
        }
        return true;
    }

    if (virtualKey == VK_RETURN) {
        if (m_activeInputField == InputField_None) {
            SetActiveInputField(InputField_Message);
            return true;
        }
        if (m_activeInputField == InputField_WhisperTarget) {
            SetActiveInputField(InputField_Message);
            return true;
        }
        return SubmitInput();
    }

    if ((virtualKey == VK_UP || virtualKey == VK_DOWN) && m_activeInputField == InputField_None) {
        SetActiveInputField(InputField_Message);
    }

    if (virtualKey == VK_UP && m_activeInputField == InputField_Message) {
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

    if (virtualKey == VK_DOWN && m_activeInputField == InputField_Message) {
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

    if (virtualKey == VK_BACK && m_activeInputField != InputField_None) {
        if (std::string* const activeBuffer = GetActiveInputBuffer(); activeBuffer && !activeBuffer->empty()) {
            if (m_historyBrowseIndex >= 0) {
                m_historyBrowseIndex = -1;
                m_historyDraft.clear();
            }
            activeBuffer->pop_back();
            Invalidate();
        }
        return true;
    }

    if (virtualKey == VK_ESCAPE && m_activeInputField != InputField_None) {
        SetActiveInputField(InputField_None);
        return true;
    }

    return false;
}

bool UINewChatWnd::HandleChar(char c)
{
    if (m_activeInputField == InputField_None) {
        return false;
    }

    if (c == '\b' || c == '\r' || c == '\n') {
        return true;
    }

    if (static_cast<unsigned char>(c) < 0x20u) {
        return false;
    }

    std::string* const activeBuffer = GetActiveInputBuffer();
    if (!activeBuffer) {
        return false;
    }

    const size_t maxChars = (m_activeInputField == InputField_WhisperTarget) ? kMaxWhisperTargetChars : kMaxInputChars;
    if (activeBuffer->size() >= maxChars) {
        return true;
    }

    if (m_historyBrowseIndex >= 0) {
        m_historyBrowseIndex = -1;
        m_historyDraft.clear();
    }
    *activeBuffer += c;
    Invalidate();
    return true;
}

bool UINewChatWnd::IsWhisperTargetActive() const
{
    return m_activeInputField == InputField_WhisperTarget;
}

bool UINewChatWnd::IsMessageInputActive() const
{
    return m_activeInputField == InputField_Message;
}

bool UINewChatWnd::IsInputActive() const
{
    return m_activeInputField != InputField_None;
}

void UINewChatWnd::Layout()
{
    int minWidth = kChatWindowMinWidth;
    int minHeight = kChatWindowMinHeight;
    int maxWidth = kChatDefaultWindowWidth;
    int maxHeight = kChatDefaultWindowHeight;
    GetChatWindowSizeLimits(&minWidth, &minHeight, &maxWidth, &maxHeight);

    const int nextWidth = std::clamp(m_w > 0 ? m_w : kChatDefaultWindowWidth, minWidth, maxWidth);
    const int nextHeight = std::clamp(m_h > 0 ? m_h : kChatDefaultWindowHeight, minHeight, maxHeight);
    const bool sizeChanged = nextWidth != m_w || nextHeight != m_h;
    if (sizeChanged) {
        Resize(nextWidth, nextHeight);
    }

    int clampedX = m_x;
    int clampedY = m_y;
    ClampChatWindowPosition(&clampedX, &clampedY, nextWidth, nextHeight);
    Move(clampedX, clampedY);
    if (sizeChanged) {
        RefreshVisibleLines(GetTickCount());
    }
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
    int filteredCount = 0;
    const u32 activeMask = m_tabChannelMasks[static_cast<size_t>(m_activeTab)];
    for (const ChatLine& line : m_lines) {
        if ((activeMask & ChatChannelBit(line.channel)) != 0u) {
            ++filteredCount;
        }
    }
    const int maxOffset = filteredCount > 0 ? filteredCount - 1 : 0;
    m_scrollLineOffset = std::clamp(m_scrollLineOffset, 0, maxOffset);
}

void UINewChatWnd::ResetTabDefaults()
{
    for (int index = 0; index < kChatTabCount; ++index) {
        ResetTabDefault(index);
    }
}

void UINewChatWnd::ResetTabDefault(int index)
{
    if (index < 0 || index >= kChatTabCount) {
        return;
    }
    m_tabChannelMasks[static_cast<size_t>(index)] = kChatTabPresets[static_cast<size_t>(index)].channelMask;
}

void UINewChatWnd::SetActiveTab(int index)
{
    const int clampedIndex = std::clamp(index, 0, kChatTabCount - 1);
    if (m_activeTab == clampedIndex) {
        return;
    }

    m_activeTab = clampedIndex;
    m_scrollLineOffset = 0;
    SaveSettings();
    RefreshVisibleLines(GetTickCount());
    Invalidate();
}

void UINewChatWnd::SetConfigVisible(bool visible)
{
    const int nextVisible = visible ? 1 : 0;
    if (m_configVisible == nextVisible) {
        return;
    }

    m_configVisible = nextVisible;
    if (m_configVisible == 0) {
        m_transparencyDragActive = 0;
    }
    Invalidate();
}

void UINewChatWnd::SetWindowOpacityPercent(int value)
{
    const int clampedValue = ClampChatWindowOpacityPercent(value);
    if (m_windowOpacityPercent == clampedValue) {
        return;
    }

    m_windowOpacityPercent = clampedValue;
    SaveSettings();
    Invalidate();
}

void UINewChatWnd::UpdateWindowOpacityFromPointer(int x, const RECT& trackRect)
{
    const int trackWidth = trackRect.right - trackRect.left;
    if (trackWidth <= 0) {
        return;
    }

    const int clampedX = std::clamp(x, static_cast<int>(trackRect.left), static_cast<int>(trackRect.right));
    const double t = static_cast<double>(clampedX - trackRect.left) / static_cast<double>(trackWidth);
    const int value = static_cast<int>(std::lround(kChatWindowOpacityMin + t * (kChatWindowOpacityMax - kChatWindowOpacityMin)));
    SetWindowOpacityPercent(value);
}

void UINewChatWnd::SetActiveInputField(ActiveInputField field)
{
    if (m_activeInputField == field) {
        return;
    }

    m_activeInputField = field;
    if (m_activeInputField != InputField_Message) {
        m_historyBrowseIndex = -1;
        m_historyDraft.clear();
    }
    Invalidate();
}

std::string* UINewChatWnd::GetActiveInputBuffer()
{
    switch (m_activeInputField) {
    case InputField_WhisperTarget:
        return &m_whisperTargetText;
    case InputField_Message:
        return &m_inputText;
    default:
        return nullptr;
    }
}

bool UINewChatWnd::SubmitInput()
{
    std::string text = m_inputText;
    if (text.empty()) {
        SetActiveInputField(InputField_None);
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

    std::string whisperTarget = m_whisperTargetText;
    const size_t targetFirstNonSpace = whisperTarget.find_first_not_of(" \t\r\n");
    if (targetFirstNonSpace == std::string::npos) {
        whisperTarget.clear();
    } else {
        const size_t targetLastNonSpace = whisperTarget.find_last_not_of(" \t\r\n");
        whisperTarget = whisperTarget.substr(targetFirstNonSpace, targetLastNonSpace - targetFirstNonSpace + 1);
    }

    const msgresult_t sent = g_modeMgr.SendMsg(CGameMode::GameMsg_SubmitChat,
        reinterpret_cast<msgparam_t>(text.c_str()),
        reinterpret_cast<msgparam_t>(whisperTarget.c_str()),
        0);
    if (sent != 0) {
        AddInputHistory(text);
        m_inputText.clear();
        m_historyBrowseIndex = -1;
        m_historyDraft.clear();
        SetActiveInputField(InputField_Message);
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
    const std::string& whisperTargetText,
    const std::string& inputText,
    int activeInputField,
    int scrollLineOffset)
{
    m_inputHistory = inputHistory;
    if (m_inputHistory.size() > kMaxInputHistory) {
        m_inputHistory.resize(kMaxInputHistory);
    }
    m_whisperTargetText = whisperTargetText.substr(0, kMaxWhisperTargetChars);
    m_inputText = inputText;
    switch (activeInputField) {
    case InputField_WhisperTarget:
        m_activeInputField = InputField_WhisperTarget;
        break;
    case InputField_Message:
        m_activeInputField = InputField_Message;
        break;
    default:
        m_activeInputField = InputField_None;
        break;
    }
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
    SaveSettings();
}
