#include "UINewChatWnd.h"
#include "ChatScrollBarUi.h"
#include "ChatText.h"
#include "ChatWindowState.h"
#include "core/Globals.h"
#include "core/UiMessageStrings.h"
#include "session/Session.h"

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
constexpr char kChatWindowIdleOpacityValue[] = "OpacityPercentIdle";
constexpr char kChatWindowWidthValue[] = "WindowWidth";
constexpr char kChatWindowHeightValue[] = "WindowHeight";
constexpr char kChatActiveTabValue[] = "ActiveTab";
constexpr char kChatTabMaskPrefix[] = "TabMask";

constexpr size_t kMaxChatLines = 50;
constexpr int kChatWindowMargin = 12;
constexpr int kChatDefaultWindowWidth = 600;
constexpr int kChatLineHeight = 14;
constexpr int kChatPanelPadding = 10;
constexpr int kChatMessageGap = 0;
constexpr int kChatDefaultWindowHeight = ro::chat::HeightForState(7);
constexpr int kChatWindowMinWidth = 360;
constexpr int kChatWindowMinHeight = 24;

constexpr int kChatResizeBorder = 6;
constexpr int kChatScrollbarWidth = 8;
constexpr int kChatScrollbarGap = 4;
constexpr int kChatTabGap = 6;
constexpr int kChatConfigPanelWidth = 336;
constexpr int kChatConfigPanelHeight = 280;
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
constexpr int kChatWindowIdleOpacityDefault = 45;
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
    RECT stickButton;
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
    RECT transparencyIdleValue;
    RECT transparencyIdleTrack;
    RECT transparencyIdleThumb;
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
    QFont font;
    font.setFamilies({QStringLiteral("Noto Sans"),QStringLiteral("Noto Sans JP"),QStringLiteral("Segoe UI")});
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
    const QString label = QString::fromUtf8(text.c_str());
    const QRect bounds = metrics.boundingRect(QRect(0, 0, width, 4096), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, label);
    return (std::max)(kChatLineHeight, bounds.height());
}

void DrawChatTextQt(HDC hdc, const RECT& rect, const std::string& text, COLORREF color, bool wrap, int pixelSize, bool elide = true, bool center = false)
{
    if (!hdc || rect.right <= rect.left || rect.bottom <= rect.top) {
        return;
    }

    QString label = QString::fromUtf8(text.c_str());
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    const QFont font = BuildChatUiFont(pixelSize);
    if (!wrap && elide) {
        const QFontMetrics metrics(font);
        label = metrics.elidedText(label, Qt::ElideRight, width);
    }

    std::vector<unsigned int> pixels(static_cast<size_t>(width) * static_cast<size_t>(height), 0u);
    QImage image(reinterpret_cast<uchar*>(pixels.data()), width, height, width * static_cast<int>(sizeof(unsigned int)), QImage::Format_ARGB32_Premultiplied);
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
        : ((center ? Qt::AlignHCenter : Qt::AlignLeft) | Qt::AlignVCenter | Qt::TextSingleLine);
    painter.drawText(QRect(0, 0, width, height), flags, label);
    painter.end();
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
    return g_windowMgr.GetChatClientRect();
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

ChatLayoutRects BuildChatLayoutRects(int x, int y, int w, int h, int titleWidth, int historyHeight)
{
    ChatLayoutRects rects{};
    rects.panel = { x, y, x + w, y + h };

    const bool expanded = h > 24;
    rects.header = {x + 5, y, x + w - 3, y + (expanded ? 16 : 0)};
    rects.gearButton = expanded ? RECT{x + w - 18, y + 1, x + w - 4, y + 15} : RECT{};
    rects.stickButton = expanded ? RECT{x + w - 36, y + 1, x + w - 22, y + 15} : RECT{};
    rects.tabs[0] = expanded ? RECT{x + 1, y + 1, x + titleWidth - 1, y + 18} : RECT{};
    rects.tabs[1] = expanded ? RECT{x + titleWidth + 2, y + 1, x + 2 * titleWidth - 1, y + 18} : RECT{};
    // Primary Create is600 wide, with90x16 whisper at6 and460x16 common
    // at110. Preserve those columns; the modern custom width scales the common
    // field while retaining the original30px right control band.
    rects.whisperInput = {x + 6, y + h - 20, x + 96, y + h - 4};
    rects.messageInput = {x + 110, y + h - 20, x + w - 30, y + h - 4};
    rects.history = {x + 5, y + (expanded ? 18 : 0), x + w - 3, y + (expanded ? 18 + historyHeight : 0)};

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
    rects.transparencyIdleValue = {
        editorLeft,
        rects.configPanel.top + 212,
        rects.configPanel.right - 12,
        rects.configPanel.top + 212 + kChatConfigButtonHeight
    };
    rects.transparencyIdleTrack = {
        rects.configPanel.left + 130,
        rects.configPanel.top + 252,
        rects.configPanel.right - 22,
        rects.configPanel.top + 252 + kChatSliderTrackHeight
    };
    rects.transparencyIdleThumb = {
        rects.transparencyIdleTrack.left,
        rects.transparencyIdleTrack.top - 5,
        rects.transparencyIdleTrack.left + kChatSliderThumbWidth,
        rects.transparencyIdleTrack.bottom + 5
    };

    const int filterWidth = ((rects.configPanel.right - 12) - editorLeft - kChatConfigButtonGap) / 2;
    const int filtersTop = rects.configPanel.top + 300;
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
    rects.resetButton = {};
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
    m_idleOpacityPercent(kChatWindowIdleOpacityDefault),
    m_windowFocused(0),
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
    ro::UiMessageStrings sourceLabels;
    if (sourceLabels.Load()) {
        m_sourceTabLabels[0] = sourceLabels.Get(1291, "ST");
        m_sourceTabLabels[1] = sourceLabels.Get(1292, "BT");
    }
    m_sourceTitleWidth = (g_serviceType == ServiceAmerica || g_serviceType == ServicePhilippine
        || g_serviceType == ServiceIndia) ? 110 : g_serviceType == ServiceFrance ? 90 : 71;
    ResetTabDefaults();
    int initialWidth = kChatDefaultWindowWidth;
    int initialHeight = kChatDefaultWindowHeight;
    LoadSettings(&initialWidth, &initialHeight);

    Create(initialWidth, initialHeight);

    RestoreSourcePlacement();
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
    if (TryLoadSettingsIniInt(kChatWndSection, kChatWindowIdleOpacityValue, &storedValue)) {
        m_idleOpacityPercent = ClampChatWindowOpacityPercent(storedValue);
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
    if (TryLoadSettingsIniInt(kChatWndSection, kChatWindowWidthValue, &storedValue)) {
        storedWidth = storedValue;
    }

    int minWidth = kChatWindowMinWidth;
    int minHeight = kChatWindowMinHeight;
    int maxWidth = kChatDefaultWindowWidth;
    int maxHeight = kChatDefaultWindowHeight;
    GetChatWindowSizeLimits(&minWidth, &minHeight, &maxWidth, &maxHeight);
    storedWidth = std::clamp(storedWidth, minWidth, maxWidth);
    if (outWindowWidth) {
        *outWindowWidth = storedWidth;
    }
    if (outWindowHeight) {
        *outWindowHeight = g_windowMgr.m_chatWndHeight>=1 && g_windowMgr.m_chatWndHeight<=8192
            ? g_windowMgr.m_chatWndHeight : 98;
    }
}

void UINewChatWnd::SaveSettings() const
{
    SaveSettingsIniInt(kChatWndSection, kChatFontSizeValue, m_fontPixelSize);
    SaveSettingsIniInt(kChatWndSection, kChatWindowOpacityValue, m_windowOpacityPercent);
    SaveSettingsIniInt(kChatWndSection, kChatWindowIdleOpacityValue, m_idleOpacityPercent);
    SaveSettingsIniInt(kChatWndSection, kChatWindowWidthValue, m_w);
    SaveSettingsIniInt(kChatWndSection, kChatActiveTabValue, m_activeTab);

    char keyName[32] = {};
    for (int index = 0; index < kChatTabCount; ++index) {
        std::snprintf(keyName, sizeof(keyName), "%s%d", kChatTabMaskPrefix, index);
        SaveSettingsIniInt(kChatWndSection, keyName, static_cast<int>(m_tabChannelMasks[static_cast<size_t>(index)]));
    }
}

bool UINewChatWnd::ReceiveSourceNotice(const char* text, u32 uiRgb, ro::chat::NoticeRoute route,
    ro::chat::NoticeType type, const char* speaker)
{
    if (!text || (route != ro::chat::NoticeRoute::Normal && route != ro::chat::NoticeRoute::Command)) return false;
    const std::string original(text);
    std::string talker = speaker ? speaker : "";
    const int numericType = static_cast<int>(type);
    if (numericType == 1 || numericType == 3 || numericType == 4) {
        const auto delimiter = original.find(" :");
        if (delimiter != std::string::npos && delimiter > 0) talker = original.substr(0, delimiter);
    }
    if ((g_serviceType == ServiceGermany || g_serviceType == ServiceVietnam) && talker.size() > 24) return false;
    if (talker == g_session.GetPlayerName()) talker.clear();
    const auto segments = ro::chat::SplitNotice(original);
    const bool normal = route == ro::chat::NoticeRoute::Normal && numericType != 8 && numericType != 9;
    const bool command = route == ro::chat::NoticeRoute::Command ||
        (numericType >= 1 && numericType <= 13 && g_session.m_battleMsgOnOffSet[numericType - 1]);
    for (const auto& segment : segments) {
        const auto append = [&](int kind, UISubChatHisWnd* detached) {
            auto& rows = m_sourceHistory[kind];
            const int before = static_cast<int>(rows.size());
            if (rows.size() >= 240) rows.erase(rows.begin(), rows.begin() + rows.size() / 2);
            rows.push_back({segment, uiRgb, talker});
            m_sourceViews[kind].AfterAppend(before, static_cast<int>(rows.size()));
            // Original subwindow receives the UNSPLIT argument for every segment.
            if (detached) detached->Append(original.c_str(), uiRgb, talker.c_str());
        };
        if (normal) append(0, g_windowMgr.m_chatWnd == this ? g_windowMgr.m_subChatWndST : nullptr);
        if (command) append(1, g_windowMgr.m_chatWnd == this ? g_windowMgr.m_subChatWndBT : nullptr);
    }
    RefreshVisibleLines(GetTickCount());
    Invalidate();
    return !segments.empty();
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

    const ChatLayoutRects layout = BuildChatLayoutRects(m_x, m_y, m_w, m_h, m_sourceTitleWidth, m_sourceViewHeights[m_sourceTab]);
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

    const ChatLayoutRects layout = BuildChatLayoutRects(m_x, m_y, m_w, m_h, m_sourceTitleWidth, m_sourceViewHeights[m_sourceTab]);
    return PointInRectXY(layout.panel, x, y);
}

bool UINewChatWnd::IsQtPointerCaptured() const
{
    return m_dragArmed != 0 || m_isDragging != 0 || m_transparencyDragActive != 0 || m_historyPointerDown || m_sourceScrollDragging;
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

std::vector<ChatLine> UINewChatWnd::GetFilteredLines() const
{
    std::vector<ChatLine> result;
    for (const auto& row : m_sourceHistory[m_sourceTab]) result.push_back({row.text, row.color, 0, 0, row.talker});
    return result;
}

void UINewChatWnd::SelectSourceTab(int tab)
{
    if (tab < 0 || tab > 1 || tab == m_sourceTab) return;
    const int sourceRows = m_sourceViews[m_sourceTab].rows;
    m_sourceViewHeights[tab]=m_sourceViewHeights[m_sourceTab];
    m_sourceTab = tab;
    // ChangeTab calls the newly selected box's virtual Resize, which snaps to bottom.
    m_sourceViews[tab].Resize(sourceRows, static_cast<int>(m_sourceHistory[tab].size()));
    RefreshVisibleLines(GetTickCount());
    Invalidate();
}

void UINewChatWnd::ScrollSourceHistory(int rows)
{
    m_sourceViews[m_sourceTab].Scroll(rows, static_cast<int>(m_sourceHistory[m_sourceTab].size()));
    RefreshVisibleLines(GetTickCount());
    Invalidate();
}

void UINewChatWnd::FocusQtInput(int field)
{
    SetWindowFocused(true);
    SetActiveInputField(field==1?InputField_WhisperTarget:field==2?InputField_Message:InputField_None);
}

void UINewChatWnd::SetQtInputText(int field,const std::string& text)
{
    if(field!=1 && field!=2)return;
    auto& buffer=field==1?m_whisperTargetText:m_inputText;
    buffer=ro::chat::CleanInput(text,field==1?kMaxWhisperTargetChars:kMaxInputChars,field==1?24:kMaxInputChars);
    m_historyBrowseIndex=-1;m_historyDraft.clear();Invalidate();
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
    if (m_expansionState == 0) return {};
    const auto& view = m_sourceViews[m_sourceTab];
    const int total = static_cast<int>(m_sourceHistory[m_sourceTab].size());
    return {total > view.rows ? 1 : 0, total, view.first, (std::min)(total, view.rows)};
}

ro::chat::ScrollBar UINewChatWnd::GetSourceScrollBar(int tab) const
{
    if(tab<0)tab=m_sourceTab;
    if(tab>1)return {};
    const auto& view=m_sourceViews[tab];
    const int count=static_cast<int>(m_sourceHistory[tab].size());
    return {m_sourceViewHeights[tab],10,view.LastFirst(count),view.scrollPosition,view.rows,count};
}

int UINewChatWnd::GetFontPixelSize() const
{
    return m_fontPixelSize;
}

int UINewChatWnd::GetWindowOpacityPercent() const
{
    return m_windowOpacityPercent;
}

int UINewChatWnd::GetIdleOpacityPercent() const
{
    return m_idleOpacityPercent;
}

bool UINewChatWnd::IsWindowFocused() const
{
    return m_windowFocused != 0;
}

void UINewChatWnd::SetWindowFocused(bool focused)
{
    const int nextFocused = focused ? 1 : 0;
    if (m_windowFocused == nextFocused) {
        return;
    }

    m_windowFocused = nextFocused;
    Invalidate();
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

    const ChatLayoutRects layout = BuildChatLayoutRects(m_x, m_y, m_w, m_h, m_sourceTitleWidth, m_sourceViewHeights[m_sourceTab]);
    *x = layout.configPanel.left;
    *y = layout.configPanel.top;
    *width = layout.configPanel.right - layout.configPanel.left;
    *height = layout.configPanel.bottom - layout.configPanel.top;
    return true;
}

int UINewChatWnd::GetTabCount() const { return 2; }

bool UINewChatWnd::GetTabDisplay(int index, ChatTabDisplay* outData) const
{
    if (!outData || index < 0 || index > 1) return false;
    *outData = {index, m_sourceTabLabels[index].c_str(), m_sourceTab == index ? 1 : 0, 0};
    return true;
}

// The old visual-channel masks are retained only as legacy settings/receipt data.
// They cannot control source streams. Source battle options139 are a separate owner.
int UINewChatWnd::GetFilterOptionCount() const { return 0; }
bool UINewChatWnd::GetFilterOptionDisplay(int, ChatFilterOptionDisplay*) const { return false; }

void UINewChatWnd::OnProcess()
{
    Layout();
    const auto& events = g_windowMgr.GetChatEvents();
    std::vector<ChatLine> syncedLines;
    const size_t start = events.size() > kMaxChatLines ? (events.size() - kMaxChatLines) : 0;
    syncedLines.reserve(events.size() - start);
    for (size_t i = start; i < events.size(); ++i) {
        // Typed source receipts belong to the main instance that received them.
        // Absent-window backlog and a destroyed main are never reclassified/replayed.
        if (events[i].source.route != ro::chat::NoticeRoute::Unspecified
            && (events[i].source.mainGeneration == 0
                || events[i].source.mainGeneration != g_windowMgr.GetSourceChatGeneration())) continue;
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

    DrawNativeToHdc(hdc);
    ReleaseDrawTarget(hdc);
}

void UINewChatWnd::DrawNativeToHdc(HDC hdc)
{
    if(!hdc||!m_show)return;
    const int savedDc = SaveDC(hdc);
    SelectObject(hdc, GetChatUiFont(m_fontPixelSize));

    const ChatLayoutRects layout = BuildChatLayoutRects(m_x, m_y, m_w, m_h, m_sourceTitleWidth, m_sourceViewHeights[m_sourceTab]);
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
    const ChatScrollBarState scrollState = GetScrollBarState();
    for (size_t index = 0; index < m_visibleLines.size(); ++index) {
        const auto& line = m_visibleLines[index];
        RECT textRc{historyRc.left + 2, historyRc.top + static_cast<int>(index) * 14 + 3,
            historyRc.right - (scrollState.visible ? 13 : 0), historyRc.top + static_cast<int>(index) * 14 + 21};
#if RO_ENABLE_QT6_UI
        DrawChatTextQt(hdc, textRc, line.text, ToColorRef(line.color), false, m_fontPixelSize, false);
#else
        SetTextColor(hdc, ToColorRef(line.color));
        DrawTextA(hdc, line.text.c_str(), -1, &textRc, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
#endif
    }
    RestoreDC(hdc, historyClipDc);
    SetBkMode(hdc, TRANSPARENT);
    const auto display = GetLayoutDisplay();
    if (m_expansionState!=0) {
        RECT button=display.battleOptionButton;
        button.left+=m_x;button.right+=m_x;button.top+=m_y;button.bottom+=m_y;
        SetTextColor(hdc,RGB(255,255,255));
        DrawTextA(hdc,"B",1,&button,DT_LEFT|DT_TOP|DT_SINGLELINE);
    }
    if (scrollState.visible) {
        const auto globalRect = [&](RECT rect) { rect.left += m_x; rect.right += m_x; rect.top += m_y; rect.bottom += m_y; return rect; };
        RECT track = globalRect(display.scrollTrack), thumb = globalRect(display.scrollThumb);
        ro::chat::DrawScrollBar(hdc,track,thumb,10);
    }
    if (m_expansionState != 0) {
        for (int tab = 0; tab < 2; ++tab) {
            RECT bounds = layout.tabs[tab];
            HBRUSH brush = CreateSolidBrush(tab == m_sourceTab ? RGB(75, 95, 115) : RGB(92, 92, 92));
            FillRect(hdc, &bounds, brush); DeleteObject(brush);
            SetTextColor(hdc, RGB(255, 255, 255));
#if RO_ENABLE_QT6_UI
            DrawChatTextQt(hdc,bounds,m_sourceTabLabels[tab],RGB(255,255,255),false,m_fontPixelSize,false,true);
#else
            DrawTextA(hdc, m_sourceTabLabels[tab].c_str(), -1, &bounds, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
#endif
        }
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

    if (m_expansionState != 0) {
        RECT stick = layout.stickButton;
        FrameRect(hdc, &stick, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
        SetTextColor(hdc, RGB(255, 255, 255));
        DrawTextA(hdc, "-", 1, &stick, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    RestoreDC(hdc, savedDc);

    m_lastDrawTick = GetTickCount();
    m_isDirty = 0;
}

void UINewChatWnd::RefreshVisibleLines(u32 nowTick)
{
    (void)nowTick;
    const auto& rows = m_sourceHistory[m_sourceTab];
    auto& view = m_sourceViews[m_sourceTab];
    m_firstVisibleLineIndex = view.first;
    m_visibleLines.clear();
    if (m_expansionState != 0 && view.first>=0 && view.first<static_cast<int>(rows.size())) {
        const int end = (std::min)(static_cast<int>(rows.size()), view.first + view.rows);
        for (int index = view.first; index < end; ++index) {
            const auto& row = rows[index];
            m_visibleLines.push_back({row.text, row.color, 0, 0, row.talker});
        }
    }
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
    const ChatLayoutRects layout = BuildChatLayoutRects(m_x, m_y, m_w, m_h, m_sourceTitleWidth, m_sourceViewHeights[m_sourceTab]);
    if (IsQtInteractionPoint(x, y)) {
        SetWindowFocused(true);
    }

    if (m_configVisible != 0 && RectIsValid(layout.configPanel) && PointInRectXY(layout.configPanel, x, y)) {
        for (int index = 0; index < GetTabCount(); ++index) {
            if (PointInRectXY(layout.configTabs[static_cast<size_t>(index)], x, y)) {
                SelectSourceTab(index);
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
            UpdateWindowOpacityFromPointer(x, layout.transparencyTrack, false);
            return true;
        }
        if (PointInRectXY(layout.transparencyIdleValue, x, y) || PointInRectXY(layout.transparencyIdleTrack, x, y)) {
            m_transparencyDragActive = 2;
            UpdateWindowOpacityFromPointer(x, layout.transparencyIdleTrack, true);
            return true;
        }
        for (int index = 0; index < GetFilterOptionCount(); ++index) {
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

    if (m_expansionState != 0 && !PointInRectXY(layout.history, x, y)) {
        for (int tab = 0; tab < 2; ++tab) if (PointInRectXY(layout.tabs[tab], x, y)) {
            SelectSourceTab(tab);
            return true;
        }
    }
    auto battleButton=GetLayoutDisplay().battleOptionButton;
    battleButton.left+=m_x;battleButton.right+=m_x;battleButton.top+=m_y;battleButton.bottom+=m_y;
    // Source child windows win overlapping frame borders. The port's inline
    // history/edit/button controls must keep the same hit-test precedence.
    const bool childHit=PointInRectXY(layout.history,x,y) || PointInRectXY(layout.whisperInput,x,y)
        || PointInRectXY(layout.messageInput,x,y) || PointInRectXY(layout.gearButton,x,y)
        || PointInRectXY(layout.stickButton,x,y) || PointInRectXY(battleButton,x,y);
    const int resizeEdges = childHit ? ChatResizeEdge_None : GetChatResizeEdgesForPoint(layout,x,y);
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

    if (PointInRectXY(layout.stickButton, x, y)) {
        m_stickyControlDispatch = true;
        StickOnOff(false);
        m_stickyControlDispatch = false;
        return true;
    }

    if (PointInRectXY(battleButton,x,y)) {SendMsg(this,6,127,0,0);return true;}
    if (PointInRectXY(layout.gearButton, x, y)) {
        SetConfigVisible(m_configVisible == 0);
        return true;
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

    if (PointInRectXY(layout.header, x, y) && (y - m_y <= 0 || y - m_y >= 21)) {
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
        const auto display = GetLayoutDisplay();
        if (GetScrollBarState().visible && x - m_x >= display.scrollTrack.left) {
            const int localY = y - m_y;
            const auto bar=GetSourceScrollBar();
            const auto hit=bar.Hit(x-m_x-display.scrollTrack.left,localY-display.scrollTrack.top);
            if(hit==ro::chat::ScrollHit::Thumb){
                m_sourceScrollDragging=true;m_sourceScrollTab=m_sourceTab;
                m_sourceScrollDrag.Begin(localY-display.scrollTrack.top);
            }else{ScrollSourceHistory(bar.Step(hit));m_historyPointerDown=true;}
            return true;
        }
        auto& view = m_sourceViews[m_sourceTab];
        const int index = view.first + (y - layout.history.top) / 14;
        view.selected = index < static_cast<int>(m_sourceHistory[m_sourceTab].size()) ? index : -1;
        m_historyPointerDown = true;
        return true;
    }

    if (PointInRectXY(layout.panel, x, y) && (y - m_y <= 0 || y - m_y >= 21)) {
        m_dragArmed = 1; m_isDragging = 0; m_resizeEdges = ChatResizeEdge_None;
        m_dragStartGlobalX = x; m_dragStartGlobalY = y;
        m_dragStartWindowX = m_x; m_dragStartWindowY = m_y;
        m_dragStartWindowW = m_w; m_dragStartWindowH = m_h;
        return true;
    }

    return true;
}

bool UINewChatWnd::HandlePointerMove(int x, int y)
{
    if (m_sourceScrollDragging) {
        const auto bar=GetSourceScrollBar(m_sourceScrollTab);
        auto& view=m_sourceViews[m_sourceScrollTab];
        const int before=view.first;
        // ChangeTab/compact move the captured source child to y=-100. Keep
        // that child owner and its retained pixel height until mouse-up.
        const int childTop=m_expansionState!=0 && m_sourceScrollTab==m_sourceTab ? 18 : -100;
        view.Scroll(m_sourceScrollDrag.Request(y-m_y-childTop,bar),static_cast<int>(m_sourceHistory[m_sourceScrollTab].size()));
        m_sourceScrollDrag.Applied(before,view.first);
        RefreshVisibleLines(GetTickCount());Invalidate();
        return true;
    }
    if (m_historyPointerDown) return true;

    if (m_transparencyDragActive != 0) {
        const ChatLayoutRects layout = BuildChatLayoutRects(m_x, m_y, m_w, m_h, m_sourceTitleWidth, m_sourceViewHeights[m_sourceTab]);
        const bool idle = m_transparencyDragActive == 2;
        UpdateWindowOpacityFromPointer(x, idle ? layout.transparencyIdleTrack : layout.transparencyTrack, idle);
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
        if (nextW != m_w) Resize(nextW, m_h);
        if (nextX != m_x) Move(nextX, m_y);
        if ((m_resizeEdges & (ChatResizeEdge_Top | ChatResizeEdge_Bottom)) != 0)
            SendMsg(this, 14, 2, nextW, nextH);
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
    if (m_historyPointerDown || m_sourceScrollDragging) {
        m_historyPointerDown = m_sourceScrollDragging = false;
        m_sourceScrollTab=-1;
        return true;
    }

    const bool hadCapture = m_dragArmed != 0 || m_isDragging != 0 || m_transparencyDragActive != 0;
    if (m_transparencyDragActive != 0) {
        const ChatLayoutRects layout = BuildChatLayoutRects(m_x, m_y, m_w, m_h, m_sourceTitleWidth, m_sourceViewHeights[m_sourceTab]);
        const bool idle = m_transparencyDragActive == 2;
        UpdateWindowOpacityFromPointer(x, idle ? layout.transparencyIdleTrack : layout.transparencyTrack, idle);
    }
    if (m_isDragging != 0) {
        StoreInfo();
    } else if (m_dragArmed != 0 && m_resizeEdges == ChatResizeEdge_None) {
        const ChatLayoutRects layout = BuildChatLayoutRects(m_x, m_y, m_w, m_h, m_sourceTitleWidth, m_sourceViewHeights[m_sourceTab]);
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
    if (!m_show || m_expansionState == 0) return false;
    const auto layout = BuildChatLayoutRects(m_x, m_y, m_w, m_h, m_sourceTitleWidth, m_sourceViewHeights[m_sourceTab]);
    if (!PointInRectXY(layout.history, x, y)) return false;
    ScrollSourceHistory(delta >= 0 ? -3 : 3);
    return true;
}

void UINewChatWnd::OnLBtnDown(int x, int y)
{
    HandlePointerDown(x, y);
}

void UINewChatWnd::OnLBtnDblClk(int x, int y)
{
    const auto layout = BuildChatLayoutRects(m_x, m_y, m_w, m_h, m_sourceTitleWidth, m_sourceViewHeights[m_sourceTab]);
    if (!m_show || m_expansionState == 0 || !PointInRectXY(layout.history, x, y)) return;
    if (GetScrollBarState().visible && x >= layout.history.right - 13) return;
    // The source capture belongs to the history child, not its main frame.
    m_historyPointerDown = true;
    const auto& rows = m_sourceHistory[m_sourceTab];
    const int selected = m_sourceViews[m_sourceTab].selected;
    if (selected < 0 || selected >= static_cast<int>(rows.size()) || rows[selected].talker.empty()) return;
    SendMsg(this, 6, 281, reinterpret_cast<msgparam_t>(rows[selected].talker.c_str()), 0);
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
        SetWindowFocused(true);
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
            ro::chat::EraseLastCharacter(*activeBuffer);
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
    const int nextHeight = ro::chat::HeightForState(m_expansionState);
    const bool sizeChanged = nextWidth != m_w || nextHeight != m_h;
    if (sizeChanged) Resize(nextWidth, nextHeight);
    // SetState preserves the old bottom even when the new top is off-screen.
    // Only actual pointer movement applies the existing viewport drag clamp.
    if (sizeChanged) {
        RefreshVisibleLines(GetTickCount());
    }
}

void UINewChatWnd::AdjustScroll(int delta) { ScrollSourceHistory(-delta); }
void UINewChatWnd::ClampScrollOffset() {}

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

void UINewChatWnd::SetIdleOpacityPercent(int value)
{
    const int clampedValue = ClampChatWindowOpacityPercent(value);
    if (m_idleOpacityPercent == clampedValue) {
        return;
    }

    m_idleOpacityPercent = clampedValue;
    SaveSettings();
    Invalidate();
}

void UINewChatWnd::UpdateWindowOpacityFromPointer(int x, const RECT& trackRect, bool idle)
{
    const int trackWidth = trackRect.right - trackRect.left;
    if (trackWidth <= 0) {
        return;
    }

    const int clampedX = std::clamp(x, static_cast<int>(trackRect.left), static_cast<int>(trackRect.right));
    const double t = static_cast<double>(clampedX - trackRect.left) / static_cast<double>(trackWidth);
    const int value = static_cast<int>(std::lround(kChatWindowOpacityMin + t * (kChatWindowOpacityMax - kChatWindowOpacityMin)));
    if (idle) {
        SetIdleOpacityPercent(value);
    } else {
        SetWindowOpacityPercent(value);
    }
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
    m_whisperTargetText = ro::chat::CleanInput(whisperTargetText,kMaxWhisperTargetChars,24);
    m_inputText = ro::chat::CleanInput(inputText,kMaxInputChars,kMaxInputChars);
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

void UINewChatWnd::RestoreSourcePlacement()
{
    const RECT client=GetChatLogicalClientRect();
    // Source message34 validates against the CURRENT physical height before
    // SetState changes height while preserving the bottom anchor.
    int x=g_windowMgr.m_chatWndX,y=g_windowMgr.m_chatWndY;
    if(x<0 || y<0 || static_cast<long long>(x)+m_w>client.right
        || static_cast<long long>(y)+m_h>client.bottom){
        x=0;y=static_cast<int>(client.bottom)-m_h;
        g_windowMgr.m_chatWndX=x;g_windowMgr.m_chatWndY=y;
    }
    Move(x,y);
    SetState(g_windowMgr.m_chatWndStatus);
    Move(m_x,m_y);
}

void UINewChatWnd::StoreInfo()
{
    g_windowMgr.m_chatWndStatus = m_expansionState;
    g_windowMgr.m_chatWndHeight = m_h;
    g_windowMgr.m_chatWndX = m_x;
    g_windowMgr.m_chatWndY = m_y;
    SaveSettings();
}

void UINewChatWnd::StickOnOff(bool sticky)
{
    if (g_windowMgr.m_chatWndStickOn != sticky) {
        g_windowMgr.m_chatWndStickOn = sticky;
        if (sticky) {
            if (m_savedExpansionState == 0) m_savedExpansionState = 1;
            m_sourceTab = 0;
            SetState(m_savedExpansionState);
        } else {
            m_savedExpansionState = m_expansionState;
            SetState(0);
        }
    }
    if (g_windowMgr.m_chatWnd == this) g_windowMgr.UpdateChatAuxiliaryVisibility();
    Invalidate();
}

void UINewChatWnd::SetShow(int show)
{
    UIWindow::SetShow(show);
    if (g_windowMgr.m_chatWnd == this) StickOnOff(g_windowMgr.m_chatWndStickOn);
}

void UINewChatWnd::Move(int x, int y)
{
    UIWindow::Move(x, y);
    if (g_windowMgr.m_chatWnd == this) g_windowMgr.UpdateChatAuxiliaryVisibility();
}

void UINewChatWnd::SetState(int requested)
{
    // Qt stores frame capture locally; native input also has the manager
    // pointer. Inline resizer/slider captures represent source child controls.
    const bool frameCaptured = (g_windowMgr.m_captureWindow == this || m_dragArmed || m_isDragging)
        && m_resizeEdges == ChatResizeEdge_None && m_transparencyDragActive == 0 && !m_stickyControlDispatch && !m_historyPointerDown && !m_sourceScrollDragging;
    if (frameCaptured) return;
    const int bottom = m_y + m_h;
    m_expansionState = ro::chat::NormalizeState(requested);
    const int height = ro::chat::HeightForState(m_expansionState);
    Resize(m_w, height);
    if (m_expansionState != 0) {m_sourceViewHeights[m_sourceTab]=height-44;m_sourceViews[m_sourceTab].Resize((height - 44) / 14, static_cast<int>(m_sourceHistory[m_sourceTab].size()));}
    Move(m_x, bottom - height);
    RefreshVisibleLines(GetTickCount());
    Invalidate();
}

void UINewChatWnd::CancelPointerCapture()
{
    m_dragArmed = m_isDragging = m_transparencyDragActive = 0;
    m_historyPointerDown = m_sourceScrollDragging = false;
    m_sourceScrollTab=-1;
    m_resizeEdges = ChatResizeEdge_None;
    if (g_windowMgr.m_captureWindow == this) g_windowMgr.m_captureWindow = nullptr;
}

void UINewChatWnd::RememberWhisperTarget(const std::string& target)
{
    if(std::find(m_whisperTargets.begin(),m_whisperTargets.end(),target)!=m_whisperTargets.end())return;
    m_whisperTargets.push_back(target);
    if(m_whisperTargetHistoryIndex>=static_cast<int>(m_whisperTargets.size())-1)m_whisperTargetHistoryIndex=static_cast<int>(m_whisperTargets.size());
}

msgresult_t UINewChatWnd::SendMsg(UIWindow* sender, int msg, msgparam_t value, msgparam_t detail, msgparam_t extra)
{
    switch (msg) {
    case 34: RestoreSourcePlacement(); return 0;
    case 3: ScrollSourceHistory(1 - m_sourceViews[m_sourceTab].rows); return 0;
    case 4: ScrollSourceHistory(m_sourceViews[m_sourceTab].rows - 1); return 0;
    case 37:
    case 115:
        ReceiveSourceNotice(reinterpret_cast<const char*>(value), ro::chat::ColorRefToUiRgb(static_cast<u32>(detail)),
            msg == 37 ? ro::chat::NoticeRoute::Normal : ro::chat::NoticeRoute::Command,
            static_cast<ro::chat::NoticeType>(extra));
        return 0;
    case 6:
        if (value==127) {g_windowMgr.ToggleWindow(UIWindowMgr::WID_BATTLEMSGOPTION);return 0;}
        if (value == 281 && detail) {
            const std::string target(reinterpret_cast<const char*>(detail));
            RememberWhisperTarget(target);
            SetQtInputText(InputField_WhisperTarget, target);
            FocusQtInput(InputField_Message);
        }
        else if (value == 291) StickOnOff(true);
        else if (value == 292) StickOnOff(false);
        return 0;
    case 2:
        if (g_windowMgr.m_chatWndStickOn) SetState(ro::chat::CycleState(m_expansionState, value != 0));
        return 0;
    case 14:
        if (value == 2 && g_windowMgr.m_chatWndStickOn) {
            const int next = ro::chat::StateFromRequestedHeight(static_cast<int>(extra));
            if (next != m_expansionState) SetState(next);
        }
        return 0;
    case 15: SetState(static_cast<int>(value)); return 0;
    case 16:
        if (g_windowMgr.m_chatWndStickOn && m_expansionState == 0) SetState(1);
        return 0;
    default: return UIWindow::SendMsg(sender, msg, value, detail, extra);
    }
}

ChatWindowLayoutDisplay UINewChatWnd::GetLayoutDisplay() const
{
    const auto rects = BuildChatLayoutRects(0, 0, m_w, m_h, m_sourceTitleWidth, m_sourceViewHeights[m_sourceTab]);
    ChatWindowLayoutDisplay display{rects.header, rects.history, rects.whisperInput, rects.messageInput, rects.gearButton, rects.stickButton, rects.tabs};
    if (m_expansionState!=0) display.battleOptionButton={2*m_sourceTitleWidth+19,4,2*m_sourceTitleWidth+31,16};
    const auto state = GetScrollBarState();
    if (state.visible) {
        display.scrollTrack={rects.history.right-13,rects.history.top,rects.history.right,rects.history.bottom};
        const auto bars=ro::chat::ScrollLayout(display.scrollTrack,GetSourceScrollBar());
        display.scrollThumb=bars.thumb;display.scrollUp=bars.up;display.scrollDown=bars.down;
    }
    return display;
}
