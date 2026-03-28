#include "UINewChatWnd.h"

#include "UIWindowMgr.h"
#include "gamemode/GameMode.h"
#include "gamemode/Mode.h"
#include "main/WinMain.h"

#include <algorithm>
#include <string>
#include <vector>

#include <windows.h>

#pragma comment(lib, "msimg32.lib")

namespace {
constexpr size_t kMaxChatLines = 50;
constexpr int kChatWindowMargin = 12;
constexpr int kChatWindowWidth = 420;
constexpr int kChatLineHeight = 19;
constexpr int kChatInputHeight = 22;
constexpr int kChatPanelPadding = 8;
constexpr int kChatHistoryHeight = 192;
constexpr int kChatMessageGap = 2;
constexpr size_t kMaxInputChars = 180;
constexpr size_t kMaxInputHistory = 5;
COLORREF ToColorRef(u32 color)
{
    return RGB((color >> 16) & 0xFFu, (color >> 8) & 0xFFu, color & 0xFFu);
}

void DrawBitmapTransparent(HDC target, HBITMAP bitmap, int x, int y, int width, int height)
{
    if (!target || !bitmap || width <= 0 || height <= 0) {
        return;
    }

    HDC memDc = CreateCompatibleDC(target);
    if (!memDc) {
        return;
    }

    HGDIOBJ oldBitmap = SelectObject(memDc, bitmap);
    TransparentBlt(target, x, y, width, height, memDc, 0, 0, width, height, RGB(255, 0, 255));
    SelectObject(memDc, oldBitmap);
    DeleteDC(memDc);
}

int MeasureWrappedTextHeight(HDC hdc, const std::string& text, int width)
{
    if (!hdc || width <= 0 || text.empty()) {
        return kChatLineHeight;
    }

    RECT calcRect{ 0, 0, width, 0 };
    DrawTextA(hdc, text.c_str(), -1, &calcRect, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_EDITCONTROL | DT_CALCRECT | DT_NOPREFIX);
    const int measuredHeight = calcRect.bottom - calcRect.top;
    return (std::max)(kChatLineHeight, measuredHeight);
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

HBITMAP GetHistoryPanelPattern()
{
    static HBITMAP s_pattern = nullptr;
    if (s_pattern) {
        return s_pattern;
    }

    constexpr int kPatternSize = 4;
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = kPatternSize;
    bmi.bmiHeader.biHeight = -kPatternSize;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    s_pattern = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!s_pattern || !bits) {
        return nullptr;
    }

    auto* pixels = static_cast<u32*>(bits);
    const u32 darkGrey = 0x00181818u;
    const u32 transparentKey = 0x00FF00FFu;
    for (int y = 0; y < kPatternSize; ++y) {
        for (int x = 0; x < kPatternSize; ++x) {
            const bool transparent = ((x + y) % 4) == 0;
            pixels[y * kPatternSize + x] = transparent ? transparentKey : darkGrey;
        }
    }

    return s_pattern;
}

void FillRectStippled(HDC target, const RECT& rect)
{
    if (!target || rect.right <= rect.left || rect.bottom <= rect.top) {
        return;
    }

    HBITMAP pattern = GetHistoryPanelPattern();
    if (!pattern) {
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
            DrawBitmapTransparent(target, pattern, x, y, drawWidth, drawHeight);
        }
    }
}
}

UINewChatWnd::UINewChatWnd()
    : m_lastDrawTick(0),
      m_inputActive(0),
      m_historyBrowseIndex(-1)
{
}

UINewChatWnd::~UINewChatWnd() = default;

void UINewChatWnd::AddChatLine(const char* text, u32 color, u8 channel, u32 tick)
{
    if (!text || *text == '\0') {
        return;
    }

    if (m_lines.size() >= kMaxChatLines) {
        m_lines.erase(m_lines.begin());
        if (!m_visibleLines.empty()) {
            m_visibleLines.erase(m_visibleLines.begin());
        }
    }

    ChatLine line{};
    line.text = text;
    line.color = color;
    line.channel = channel;
    line.tick = tick;
    m_lines.push_back(std::move(line));
    m_visibleLines = m_lines;
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
        m_lines.swap(syncedLines);
        m_visibleLines = m_lines;
        Invalidate();
    }
    RefreshVisibleLines(GetTickCount());
}

void UINewChatWnd::OnDraw()
{
    if (!g_hMainWnd || m_show == 0) {
        return;
    }

    const bool useShared = (UIWindow::GetSharedDrawDC() != nullptr);
    HDC hdc = useShared ? UIWindow::GetSharedDrawDC() : GetDC(g_hMainWnd);
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
    const int textWidth = (historyRc.right - historyRc.left) - (textInset * 2);
    const size_t lineCount = m_visibleLines.size();
    std::vector<int> measuredHeights(lineCount, kChatLineHeight);
    int usedHeight = 0;
    size_t firstVisibleIndex = lineCount;
    for (size_t index = lineCount; index > 0; --index) {
        const size_t lineIndex = index - 1;
        const int measured = MeasureWrappedTextHeight(hdc, m_visibleLines[lineIndex].text, textWidth);
        measuredHeights[lineIndex] = measured;
        const int blockHeight = measured + ((usedHeight > 0) ? kChatMessageGap : 0);
        if (usedHeight + blockHeight > kChatHistoryHeight) {
            break;
        }
        usedHeight += blockHeight;
        firstVisibleIndex = lineIndex;
    }
    if (firstVisibleIndex == lineCount && lineCount > 0) {
        firstVisibleIndex = lineCount - 1;
        measuredHeights[firstVisibleIndex] = MeasureWrappedTextHeight(hdc, m_visibleLines[firstVisibleIndex].text, textWidth);
        usedHeight = measuredHeights[firstVisibleIndex];
    }

    int lineY = historyRc.bottom - textInset - usedHeight;
    if (lineY < historyRc.top + textInset) {
        lineY = historyRc.top + textInset;
    }
    for (size_t index = firstVisibleIndex; index < lineCount; ++index) {
        const ChatLine& line = m_visibleLines[index];
        const int textHeight = measuredHeights[index];
        RECT textRc = {
            historyRc.left + textInset,
            lineY,
            historyRc.right - textInset,
            lineY + textHeight
        };
        SetTextColor(hdc, ToColorRef(line.color));
        DrawTextA(hdc, line.text.c_str(), -1, &textRc, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_EDITCONTROL | DT_NOPREFIX);
        lineY += textHeight + kChatMessageGap;
    }
    RestoreDC(hdc, historyClipDc);
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
    DrawTextA(hdc, drawText.c_str(), -1, &inputTextRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    RestoreDC(hdc, savedDc);
    if (!useShared) {
        ReleaseDC(g_hMainWnd, hdc);
    }

    m_lastDrawTick = GetTickCount();
    m_isDirty = 0;
}

void UINewChatWnd::RefreshVisibleLines(u32 nowTick)
{
    (void)nowTick;
    std::vector<ChatLine> nextVisible;
    nextVisible.reserve(m_lines.size());
    for (const ChatLine& line : m_lines) {
        nextVisible.push_back(line);
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
    Invalidate();
}

void UINewChatWnd::OnLBtnDown(int x, int y)
{
    (void)x;
    (void)y;
    SetInputActive(true);
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

    Move(kChatWindowMargin, clientRect.bottom - panelHeight - kChatWindowMargin);
    Resize(kChatWindowWidth, panelHeight);
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
