#include "UIChooseWnd.h"

#include "core/File.h"
#include "gamemode/GameMode.h"
#include "gamemode/Mode.h"
#include "main/WinMain.h"
#include "qtui/QtUiRuntime.h"
#include "ui/UIWindowMgr.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <vector>

namespace {

constexpr int kDefaultWidth = 239;
constexpr int kDefaultHeight = 113;
constexpr int kButtonWidth = 221;
constexpr int kButtonHeight = 20;
constexpr int kButtonSpacing = 3;
constexpr int kMenuButtonBaseId = 200;
constexpr int kMenuEntryCount = 4;
constexpr int kWindowCornerRadius = 8;

std::string ToLowerAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string NormalizeSlash(std::string value)
{
    std::replace(value.begin(), value.end(), '/', '\\');
    return value;
}

void AddUniqueCandidate(std::vector<std::string>& out, const std::string& raw)
{
    if (raw.empty()) {
        return;
    }

    const std::string normalized = NormalizeSlash(raw);
    const std::string lowered = ToLowerAscii(normalized);
    for (const std::string& existing : out) {
        if (ToLowerAscii(existing) == lowered) {
            return;
        }
    }
    out.push_back(normalized);
}

std::vector<std::string> BuildUiAssetCandidates(const char* fileName)
{
    std::vector<std::string> out;
    if (!fileName || !*fileName) {
        return out;
    }

    static const char* kUiKor =
        "texture\\"
        "\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA"
        "\\";

    const char* pathPrefixes[] = {
        "",
        "skin\\default\\",
        "skin\\default\\basic_interface\\",
        "skin\\default\\login_interface\\",
        "skin\\default\\interface\\",
        "texture\\",
        "texture\\interface\\",
        "texture\\interface\\basic_interface\\",
        "texture\\login_interface\\",
        "data\\",
        "data\\texture\\",
        "data\\texture\\interface\\",
        "data\\texture\\interface\\basic_interface\\",
        "data\\texture\\login_interface\\",
        kUiKor,
        "texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\basic_interface\\",
        "texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\login_interface\\",
        "data\\texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\",
        "data\\texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\basic_interface\\",
        "data\\texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\login_interface\\",
        nullptr
    };

    std::string base = NormalizeSlash(fileName);
    AddUniqueCandidate(out, base);

    std::string filenameOnly = base;
    const size_t slashPos = filenameOnly.find_last_of('\\');
    if (slashPos != std::string::npos && slashPos + 1 < filenameOnly.size()) {
        filenameOnly = filenameOnly.substr(slashPos + 1);
    }

    for (int i = 0; pathPrefixes[i]; ++i) {
        AddUniqueCandidate(out, std::string(pathPrefixes[i]) + filenameOnly);
    }

    return out;
}

std::string ResolveUiAssetPath(const char* fileName)
{
    for (const std::string& candidate : BuildUiAssetCandidates(fileName)) {
        if (g_fileMgr.IsDataExist(candidate.c_str())) {
            return candidate;
        }
    }
    return NormalizeSlash(fileName ? fileName : "");
}

RECT MakeRect(int x, int y, int w, int h)
{
    RECT rc = { x, y, x + w, y + h };
    return rc;
}

void FillRectColor(HDC hdc, const RECT& rc, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(hdc, &rc, brush);
    DeleteObject(brush);
}

void DrawRectFrame(HDC hdc, const RECT& rc, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    FrameRect(hdc, &rc, brush);
    DeleteObject(brush);
}

void FillRoundedRectColor(HDC hdc, const RECT& rc, COLORREF color, int radius)
{
    HBRUSH brush = CreateSolidBrush(color);
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ oldBrush = SelectObject(hdc, brush);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius, radius);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);
}

const char* GetEntryBitmapName(int index, int stateIndex)
{
    static const char* kBitmaps[kMenuEntryCount][3] = {
        { "esc_01a.bmp", "esc_01b.bmp", "esc_01c.bmp" },
        { "esc_02a.bmp", "esc_02b.bmp", "esc_02c.bmp" },
        { "esc_03a.bmp", "esc_03b.bmp", "esc_03c.bmp" },
        { "esc_04a.bmp", "esc_04b.bmp", "esc_04c.bmp" },
    };

    if (index < 0 || index >= kMenuEntryCount || stateIndex < 0 || stateIndex >= 3) {
        return "";
    }

    return kBitmaps[index][stateIndex];
}

} // namespace

UIChooseWnd::UIChooseWnd()
    : m_controlsCreated(false),
      m_entryButtons{ nullptr, nullptr, nullptr, nullptr },
      m_selectedIndex(MenuEntry_ReturnToGame),
      m_pressedIndex(-1)
{
    m_defPushId = 0;
    m_defCancelPushId = 0;
    Create(kDefaultWidth, kDefaultHeight);
}

UIChooseWnd::~UIChooseWnd() = default;

void UIChooseWnd::SetShow(int show)
{
    UIWindow::SetShow(show);

    if (show == 0) {
        m_pressedIndex = -1;
        for (UIBitmapButton* button : m_entryButtons) {
            if (button) {
                button->m_state = 0;
            }
        }
        return;
    }

    EnsureCreated();
    if (!g_hMainWnd) {
        return;
    }

    RECT clientRect{};
    GetClientRect(g_hMainWnd, &clientRect);
    Move((clientRect.right - clientRect.left - m_w) / 2,
        (clientRect.bottom - clientRect.top - m_h) / 2);
    LayoutButtons();
    SyncSelectionVisuals();
}

void UIChooseWnd::EnsureCreated()
{
    if (m_controlsCreated || !g_hMainWnd) {
        return;
    }

    RECT clientRect{};
    GetClientRect(g_hMainWnd, &clientRect);
    OnCreate(clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);
}

void UIChooseWnd::OnCreate(int cx, int cy)
{
    if (m_controlsCreated) {
        return;
    }
    m_controlsCreated = true;

    Create(kDefaultWidth, kDefaultHeight);
    Move((cx - m_w) / 2, (cy - m_h) / 2);

    if (IsQtUiRuntimeEnabled()) {
        return;
    }

    for (int index = 0; index < MenuEntry_Count; ++index) {
        auto* button = new UIBitmapButton();
        button->SetBitmapName(ResolveUiAssetPath(GetEntryBitmapName(index, 0)).c_str(), 0);
        button->SetBitmapName(ResolveUiAssetPath(GetEntryBitmapName(index, 1)).c_str(), 1);
        button->SetBitmapName(ResolveUiAssetPath(GetEntryBitmapName(index, 2)).c_str(), 2);
        button->Create(button->m_bitmapWidth > 0 ? button->m_bitmapWidth : kButtonWidth,
            button->m_bitmapHeight > 0 ? button->m_bitmapHeight : kButtonHeight);
        button->m_id = kMenuButtonBaseId + index;
        AddChild(button);
        m_entryButtons[index] = button;
    }

    LayoutButtons();
}

void UIChooseWnd::LayoutButtons()
{
    for (int index = 0; index < MenuEntry_Count; ++index) {
        if (m_entryButtons[index]) {
            const RECT entryRect = GetEntryRect(index);
            m_entryButtons[index]->Move(entryRect.left, entryRect.top);
        }
    }
}

RECT UIChooseWnd::GetEntryRect(int index) const
{
    return MakeRect(
        m_x + (m_w - kButtonWidth) / 2,
        m_y + 12 + index * (kButtonHeight + kButtonSpacing),
        kButtonWidth,
        kButtonHeight);
}

int UIChooseWnd::HitTestEntry(int x, int y) const
{
    for (int index = 0; index < MenuEntry_Count; ++index) {
        const RECT entryRect = GetEntryRect(index);
        if (x >= entryRect.left && x < entryRect.right && y >= entryRect.top && y < entryRect.bottom) {
            return index;
        }
    }
    return -1;
}

void UIChooseWnd::SyncSelectionVisuals()
{
    for (int index = 0; index < MenuEntry_Count; ++index) {
        if (!m_entryButtons[index] || m_entryButtons[index]->m_state == 1) {
            continue;
        }
        m_entryButtons[index]->m_state = (index == m_selectedIndex) ? 2 : 0;
    }
}

void UIChooseWnd::UpdateSelectedIndexFromHover() const
{
    if (!g_windowMgr.m_lastHitWindow) {
        return;
    }

    for (int index = 0; index < MenuEntry_Count; ++index) {
        if (g_windowMgr.m_lastHitWindow == m_entryButtons[index]) {
            const_cast<UIChooseWnd*>(this)->m_selectedIndex = index;
            return;
        }
    }
}

void UIChooseWnd::CloseMenu()
{
    SetShow(0);
}

msgresult_t UIChooseWnd::ActivateSelection()
{
    switch (m_selectedIndex) {
    case MenuEntry_CharacterSelect:
        if (g_modeMgr.SendMsg(CGameMode::GameMsg_RequestReturnToCharSelect, 0, 0, 0) != 0) {
            CloseMenu();
            return 1;
        }
        return 0;

    case MenuEntry_ReturnToGame:
        CloseMenu();
        return 1;

    case MenuEntry_ExitToWindows:
        CloseMenu();
        return g_modeMgr.SendMsg(CGameMode::GameMsg_RequestExitToWindows, 0, 0, 0);

    case MenuEntry_ReturnToSavePoint:
        if (g_modeMgr.SendMsg(CGameMode::GameMsg_RequestReturnToSavePoint, 0, 0, 0) != 0) {
            CloseMenu();
            return 1;
        }
        return 0;

    default:
        CloseMenu();
        return 1;
    }
}

void UIChooseWnd::OnDraw()
{
    EnsureCreated();
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

    LayoutButtons();

    const RECT panel = MakeRect(m_x, m_y, m_w, m_h);
    FillRoundedRectColor(hdc, panel, RGB(255, 255, 255), kWindowCornerRadius);

    DrawChildrenToHdc(hdc);

    ReleaseDrawTarget(hdc);
}

void UIChooseWnd::OnLBtnDown(int x, int y)
{
    const int hitIndex = HitTestEntry(x, y);
    m_pressedIndex = hitIndex;
    if (hitIndex >= 0) {
        m_selectedIndex = hitIndex;
        SyncSelectionVisuals();
    }
}

void UIChooseWnd::OnLBtnUp(int x, int y)
{
    const int pressedIndex = m_pressedIndex;
    const int hitIndex = HitTestEntry(x, y);
    m_pressedIndex = -1;
    if (hitIndex >= 0) {
        m_selectedIndex = hitIndex;
        SyncSelectionVisuals();
    }
    if (pressedIndex >= 0 && pressedIndex == hitIndex) {
        ActivateSelection();
    }
}

void UIChooseWnd::OnLBtnDblClk(int x, int y)
{
    const int hitIndex = HitTestEntry(x, y);
    if (hitIndex >= 0) {
        m_selectedIndex = hitIndex;
        SyncSelectionVisuals();
        ActivateSelection();
    }
}

void UIChooseWnd::OnMouseMove(int x, int y)
{
    const int hitIndex = HitTestEntry(x, y);
    if (hitIndex >= 0 && hitIndex != m_selectedIndex) {
        m_selectedIndex = hitIndex;
        SyncSelectionVisuals();
    }
}

msgresult_t UIChooseWnd::SendMsg(UIWindow* sender, int msg, msgparam_t wparam, msgparam_t lparam, msgparam_t extra)
{
    (void)sender;
    (void)lparam;
    (void)extra;

    if (msg != 6) {
        return 0;
    }

    if (wparam >= kMenuButtonBaseId && wparam < kMenuButtonBaseId + MenuEntry_Count) {
        m_selectedIndex = static_cast<int>(wparam - kMenuButtonBaseId);
        return ActivateSelection();
    }

    return 0;
}

void UIChooseWnd::OnKeyDown(int virtualKey)
{
    UpdateSelectedIndexFromHover();

    switch (virtualKey) {
    case VK_ESCAPE:
        CloseMenu();
        break;

    case VK_RETURN:
        ActivateSelection();
        break;

    case VK_UP:
        m_selectedIndex = (m_selectedIndex <= 0) ? (MenuEntry_Count - 1) : (m_selectedIndex - 1);
        SyncSelectionVisuals();
        break;

    case VK_DOWN:
        m_selectedIndex = (m_selectedIndex + 1) % MenuEntry_Count;
        SyncSelectionVisuals();
        break;

    default:
        break;
    }
}

int UIChooseWnd::GetSelectedIndex() const
{
    return m_selectedIndex;
}