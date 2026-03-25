#include "UINewChatWnd.h"

#include "UIWindowMgr.h"
#include "gamemode/GameMode.h"
#include "gamemode/Mode.h"
#include "main/WinMain.h"

#include <algorithm>
#include <string>

#include <windows.h>

namespace {
constexpr size_t kMaxChatLines = 256;
constexpr size_t kMaxVisibleLines = 12;
constexpr u32 kVisibleLineLifetimeMs = 15000;
constexpr int kChatWindowMargin = 12;
constexpr int kChatWindowWidth = 420;
constexpr int kChatLineHeight = 16;
constexpr int kChatInputHeight = 22;
constexpr int kChatPanelPadding = 8;
constexpr size_t kMaxInputChars = 180;
COLORREF ToColorRef(u32 color)
{
    return RGB((color >> 16) & 0xFFu, (color >> 8) & 0xFFu, color & 0xFFu);
}
}

UINewChatWnd::UINewChatWnd() : m_lastDrawTick(0), m_inputActive(0)
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
    }

    ChatLine line{};
    line.text = text;
    line.color = color;
    line.channel = channel;
    line.tick = tick;
    m_lines.push_back(std::move(line));
    m_isDirty = 1;
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

    const bool shouldDrawPanel = m_inputActive != 0 || !m_visibleLines.empty();
    if (shouldDrawPanel) {
        RECT rc = { m_x, m_y, m_x + m_w, m_y + m_h };
        HBRUSH bgBrush = CreateSolidBrush(m_inputActive ? RGB(30, 30, 36) : RGB(22, 22, 26));
        FillRect(hdc, &rc, bgBrush);
        DeleteObject(bgBrush);
        FrameRect(hdc, &rc, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));

        SetBkMode(hdc, TRANSPARENT);
        const int lineX = m_x + kChatPanelPadding;
        int lineY = m_y + kChatPanelPadding;
        for (const ChatLine& line : m_visibleLines) {
            RECT textRc = { lineX, lineY, m_x + m_w - kChatPanelPadding, lineY + kChatLineHeight };
            SetTextColor(hdc, ToColorRef(line.color));
            DrawTextA(hdc, line.text.c_str(), -1, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            lineY += kChatLineHeight;
        }

        RECT inputRc = {
            m_x + kChatPanelPadding,
            m_y + m_h - kChatInputHeight - kChatPanelPadding,
            m_x + m_w - kChatPanelPadding,
            m_y + m_h - kChatPanelPadding
        };
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
        DrawTextA(hdc, drawText.c_str(), -1, &inputTextRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    if (!useShared) {
        ReleaseDC(g_hMainWnd, hdc);
    }

    m_lastDrawTick = GetTickCount();
    m_isDirty = 0;
}

void UINewChatWnd::RefreshVisibleLines(u32 nowTick)
{
    std::vector<ChatLine> nextVisible;
    nextVisible.reserve(kMaxVisibleLines);

    for (auto it = m_lines.rbegin(); it != m_lines.rend(); ++it) {
        const u32 age = nowTick - it->tick;
        if (age > kVisibleLineLifetimeMs) {
            continue;
        }
        nextVisible.push_back(*it);
        if (nextVisible.size() >= kMaxVisibleLines) {
            break;
        }
    }

    std::reverse(nextVisible.begin(), nextVisible.end());
    if (nextVisible.size() != m_visibleLines.size()) {
        m_isDirty = 1;
    } else {
        for (size_t i = 0; i < nextVisible.size(); ++i) {
            if (nextVisible[i].text != m_visibleLines[i].text ||
                nextVisible[i].color != m_visibleLines[i].color ||
                nextVisible[i].channel != m_visibleLines[i].channel) {
                m_isDirty = 1;
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
    m_isDirty = 1;
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

    if (virtualKey == VK_BACK && m_inputActive != 0) {
        if (!m_inputText.empty()) {
            m_inputText.pop_back();
            m_isDirty = 1;
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

    m_inputText += c;
    m_isDirty = 1;
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

    const int visibleCount = m_inputActive != 0 ? static_cast<int>(kMaxVisibleLines) : static_cast<int>(m_visibleLines.size());
    const int lineAreaHeight = (std::max)(1, visibleCount) * kChatLineHeight;
    const int panelHeight = lineAreaHeight + kChatInputHeight + (kChatPanelPadding * 3);

    Move(kChatWindowMargin, clientRect.bottom - panelHeight - kChatWindowMargin);
    Resize(kChatWindowWidth, panelHeight);
}

void UINewChatWnd::SetInputActive(bool active)
{
    m_inputActive = active ? 1 : 0;
    m_isDirty = 1;
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
        m_isDirty = 1;
        return true;
    }

    const size_t lastNonSpace = text.find_last_not_of(" \t\r\n");
    text = text.substr(firstNonSpace, lastNonSpace - firstNonSpace + 1);

    const int sent = g_modeMgr.SendMsg(CGameMode::GameMsg_SubmitChat, reinterpret_cast<int>(text.c_str()), 0, 0);
    if (sent != 0) {
        m_inputText.clear();
        m_isDirty = 1;
        return true;
    }

    return false;
}
