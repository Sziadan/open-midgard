#include "UIMessengerGroupWnd.h"

#include "UIPartyOptionWnd.h"
#include "UIWindowMgr.h"
#include "gamemode/GameMode.h"
#include "render/DC.h"
#include "session/Session.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>

namespace {

constexpr int kWindowWidth = 290;
constexpr int kWindowHeight = 278;
constexpr int kTitleBarHeight = 17;
constexpr int kContentTop = 24;
constexpr int kTabWidth = 72;
constexpr int kTabHeight = 22;
constexpr int kTabGap = 6;
constexpr int kListLeft = 10;
constexpr int kListTop = 58;
constexpr int kListRight = 26;
constexpr int kListBottom = 84;
constexpr int kRowHeight = 19;
constexpr int kCloseButtonWidth = 14;
constexpr int kCloseButtonHeight = 12;
constexpr int kScrollWidth = 12;
constexpr int kScrollThumbMinHeight = 24;
constexpr int kActionButtonWidth = 100;
constexpr int kActionButtonHeight = 22;
constexpr int kFooterHeight = 34;
constexpr int kFooterBottomMargin = 9;
constexpr int kFooterButtonGap = 8;
constexpr int kButtonIdClose = 1;
constexpr int kButtonIdCreateParty = 100;
constexpr int kButtonIdConfigureParty = 101;
constexpr int kButtonIdDisbandParty = 102;

void HashTokenValue(unsigned long long* hash, unsigned long long value)
{
    if (!hash) {
        return;
    }
    *hash ^= value;
    *hash *= 1099511628211ull;
}

void HashTokenString(unsigned long long* hash, const std::string& value)
{
    if (!hash) {
        return;
    }
    for (unsigned char ch : value) {
        HashTokenValue(hash, static_cast<unsigned long long>(ch));
    }
    HashTokenValue(hash, 0xFFull);
}

bool IsPointInRect(const RECT& rect, int x, int y)
{
    return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
}

void FillRectColor(HDC hdc, const RECT& rect, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(hdc, &rect, brush);
    DeleteObject(brush);
}

void FrameRectColor(HDC hdc, const RECT& rect, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    FrameRect(hdc, &rect, brush);
    DeleteObject(brush);
}

void DrawTextLine(HDC hdc, const RECT& rect, const char* text, COLORREF color, UINT format)
{
    if (!hdc || !text) {
        return;
    }

    const int oldBkMode = SetBkMode(hdc, TRANSPARENT);
    const COLORREF oldColor = SetTextColor(hdc, color);
    RECT drawRect = rect;
    DrawTextA(hdc, text, -1, &drawRect, format);
    SetTextColor(hdc, oldColor);
    SetBkMode(hdc, oldBkMode);
}

std::string BuildPartyMemberSummary()
{
    char text[64] = {};
    std::snprintf(text,
        sizeof(text),
        "%u members",
        g_session.GetNumParty());
    return text;
}

std::string BuildPartyOptionSummary()
{
    char text[160] = {};
    const char* expMode = g_session.m_partyExpShare ? "Even Share" : "Each Take";
    const char* itemMode = g_session.m_itemCollectType ? "Party Share" : "Each Take";
    const char* typeMode = g_session.m_itemDivType ? "Shared" : "Individual";
    std::snprintf(text,
        sizeof(text),
        "EXP %s  |  Items %s  |  Type %s",
        expMode,
        itemMode,
        typeMode);
    return text;
}

std::string TrimMapDisplayName(const std::string& mapName)
{
    if (mapName.size() > 4 && mapName.compare(mapName.size() - 4, 4, ".rsw") == 0) {
        return mapName.substr(0, mapName.size() - 4);
    }
    return mapName;
}

}

UIMessengerGroupWnd::UIMessengerGroupWnd()
    : m_currentTab(TabFriend),
      m_selectedIndex(-1),
      m_viewOffset(0),
      m_controlsCreated(false),
      m_lastVisualStateToken(0),
      m_hasVisualStateToken(false)
{
}

UIMessengerGroupWnd::~UIMessengerGroupWnd() = default;

void UIMessengerGroupWnd::SetShow(int show)
{
    UIWindow::SetShow(show);
    if (show != 0) {
        if (!m_controlsCreated) {
            OnCreate(0, 0);
        }
        NormalizeStateForData();
    }
}

void UIMessengerGroupWnd::Move(int x, int y)
{
    UIWindow::Move(x, y);
}

bool UIMessengerGroupWnd::IsUpdateNeed()
{
    const unsigned long long token = BuildVisualStateToken();
    if (!m_hasVisualStateToken || m_lastVisualStateToken != token) {
        return true;
    }
    return UIWindow::IsUpdateNeed();
}

msgresult_t UIMessengerGroupWnd::SendMsg(UIWindow* sender, int msg, msgparam_t wparam, msgparam_t lparam, msgparam_t extra)
{
    (void)sender;
    (void)wparam;
    (void)lparam;
    (void)extra;

    if (msg == 23) {
        NormalizeStateForData();
        Invalidate();
        return 1;
    }

    return 0;
}

void UIMessengerGroupWnd::OnCreate(int x, int y)
{
    (void)x;
    (void)y;
    if (m_controlsCreated) {
        return;
    }

    Create(kWindowWidth, kWindowHeight);
    m_id = UIWindowMgr::WID_MESSENGERGROUPWND;
    m_controlsCreated = true;

    int savedX = 0;
    int savedY = 0;
    if (!LoadUiWindowPlacement("MessengerGroupWnd", &savedX, &savedY)) {
        savedX = 348;
        savedY = 56;
    }
    g_windowMgr.ClampWindowToClient(&savedX, &savedY, m_w, m_h);
    Move(savedX, savedY);
    NormalizeStateForData();
}

void UIMessengerGroupWnd::OnDestroy()
{
    StoreInfo();
}

void UIMessengerGroupWnd::OnDraw()
{
    HDC hdc = AcquireDrawTarget();
    if (!hdc) {
        return;
    }

    const RECT outer{ m_x, m_y, m_x + m_w, m_y + m_h };
    const RECT titleBar{ m_x + 1, m_y + 1, m_x + m_w - 1, m_y + kTitleBarHeight };
    const RECT content{ m_x + 1, m_y + kTitleBarHeight, m_x + m_w - 1, m_y + m_h - 1 };
    const RECT listRect = GetListRect();

    FillRectColor(hdc, outer, RGB(232, 225, 210));
    FrameRectColor(hdc, outer, RGB(92, 82, 67));
    FillRectColor(hdc, titleBar, RGB(92, 111, 96));
    FrameRectColor(hdc, titleBar, RGB(60, 74, 62));
    FillRectColor(hdc, content, RGB(245, 240, 229));

    const RECT titleTextRect{ m_x + 10, m_y + 1, m_x + 180, m_y + kTitleBarHeight };
    DrawTextLine(hdc, titleTextRect, "Friend / Party", RGB(248, 248, 240), DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    const RECT closeRect = GetCloseButtonRect();
    FillRectColor(hdc, closeRect, RGB(210, 198, 180));
    FrameRectColor(hdc, closeRect, RGB(110, 96, 74));
    DrawTextLine(hdc, closeRect, "x", RGB(34, 34, 34), DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    std::array<const char*, 2> tabLabels = { "Friend", "Party" };
    for (int index = 0; index < static_cast<int>(tabLabels.size()); ++index) {
        const RECT tabRect = GetTabRect(index);
        const bool active = index == m_currentTab;
        FillRectColor(hdc, tabRect, active ? RGB(255, 247, 233) : RGB(214, 204, 184));
        FrameRectColor(hdc, tabRect, active ? RGB(122, 98, 62) : RGB(151, 137, 114));
        DrawTextLine(hdc, tabRect, tabLabels[static_cast<size_t>(index)], RGB(42, 42, 42), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    const RECT headerRect{ listRect.left, listRect.top - 18, listRect.right, listRect.top };
    FillRectColor(hdc, headerRect, RGB(224, 216, 198));
    FrameRectColor(hdc, headerRect, RGB(171, 158, 136));

    const RECT nameHeader{ listRect.left + 6, headerRect.top, listRect.left + 102, headerRect.bottom };
    const RECT stateHeader{ listRect.left + 110, headerRect.top, listRect.left + 165, headerRect.bottom };
    const RECT detailHeader{ listRect.left + 172, headerRect.top, listRect.right - 6, headerRect.bottom };
    DrawTextLine(hdc, nameHeader, "Name", RGB(76, 61, 37), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawTextLine(hdc, stateHeader, m_currentTab == TabFriend ? "State" : "Role", RGB(76, 61, 37), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawTextLine(hdc, detailHeader, m_currentTab == TabFriend ? "Memo" : "Map", RGB(76, 61, 37), DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    DisplayData display{};
    GetDisplayDataForQt(&display);

    FillRectColor(hdc, listRect, RGB(252, 249, 242));
    FrameRectColor(hdc, listRect, RGB(158, 147, 124));

    for (const DisplayRow& row : display.rows) {
        const RECT rowRect{ row.x, row.y, row.x + row.width, row.y + row.height };
        FillRectColor(hdc, rowRect, row.selected ? RGB(235, 226, 205) : RGB(250, 247, 239));

        RECT swatchRect{ rowRect.left + 3, rowRect.top + 3, rowRect.left + 9, rowRect.bottom - 3 };
        FillRectColor(hdc, swatchRect, RGB((row.color >> 16) & 0xFFu, (row.color >> 8) & 0xFFu, row.color & 0xFFu));

        const RECT nameRect{ rowRect.left + 14, rowRect.top, rowRect.left + 104, rowRect.bottom };
        const RECT statusRect{ rowRect.left + 109, rowRect.top, rowRect.left + 165, rowRect.bottom };
        const RECT detailRect{ rowRect.left + 171, rowRect.top, rowRect.right - 4, rowRect.bottom };
        DrawTextLine(hdc, nameRect, row.name.c_str(), RGB(26, 26, 26), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        DrawTextLine(hdc, statusRect, row.status.c_str(), RGB(66, 66, 66), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        DrawTextLine(hdc, detailRect, row.detail.c_str(), RGB(92, 80, 64), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    if (display.scrollBarVisible) {
        const RECT trackRect = GetScrollTrackRect();
        const RECT thumbRect = GetScrollThumbRect();
        FillRectColor(hdc, trackRect, RGB(230, 223, 208));
        FrameRectColor(hdc, trackRect, RGB(168, 153, 126));
        FillRectColor(hdc, thumbRect, RGB(151, 131, 94));
        FrameRectColor(hdc, thumbRect, RGB(104, 86, 58));
    }

    for (const QtButtonDisplay& button : display.actionButtons) {
        const RECT rect{ button.x, button.y, button.x + button.width, button.y + button.height };
        FillRectColor(hdc, rect, button.active ? RGB(212, 226, 239) : RGB(235, 229, 214));
        FrameRectColor(hdc, rect, RGB(121, 106, 79));
        DrawTextLine(hdc, rect, button.label.c_str(), RGB(28, 28, 28), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    const RECT footerRect{ m_x + 10, m_y + m_h - (kFooterHeight + kFooterBottomMargin), m_x + m_w - 10, m_y + m_h - kFooterBottomMargin };
    FillRectColor(hdc, footerRect, RGB(232, 226, 213));
    FrameRectColor(hdc, footerRect, RGB(177, 165, 144));

    const int footerMidY = footerRect.top + (footerRect.bottom - footerRect.top) / 2;
    const RECT footerTitleRect{ footerRect.left + 6, footerRect.top + 1, footerRect.left + 84, footerMidY };
    const RECT footerValueRect{ footerRect.left + 86, footerRect.top + 1, footerRect.right - 6, footerMidY };
    const RECT footerSecondaryRect{ footerRect.left + 6, footerMidY - 1, footerRect.right - 6, footerRect.bottom - 1 };
    DrawTextLine(hdc, footerTitleRect, display.summaryTitle.c_str(), RGB(68, 58, 44), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawTextLine(hdc, footerValueRect, display.summaryValue.c_str(), RGB(48, 48, 48), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    DrawTextLine(hdc, footerSecondaryRect, display.summarySecondaryValue.c_str(), RGB(48, 48, 48), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    ReleaseDrawTarget(hdc);
    m_lastVisualStateToken = BuildVisualStateToken();
    m_hasVisualStateToken = true;
    m_isDirty = 0;
}

void UIMessengerGroupWnd::OnLBtnDown(int x, int y)
{
    if (IsPointInRect(GetCloseButtonRect(), x, y)) {
        return;
    }

    for (int index = 0; index < 2; ++index) {
        if (IsPointInRect(GetTabRect(index), x, y)) {
            return;
        }
    }

    if (IsPointInRect(GetListRect(), x, y)) {
        return;
    }

    UIFrameWnd::OnLBtnDown(x, y);
}

void UIMessengerGroupWnd::OnLBtnUp(int x, int y)
{
    const bool wasDragging = m_isDragging != 0;
    UIFrameWnd::OnLBtnUp(x, y);
    if (wasDragging) {
        return;
    }

    if (IsPointInRect(GetCloseButtonRect(), x, y)) {
        SetShow(0);
        StoreInfo();
        return;
    }

    for (int index = 0; index < 2; ++index) {
        if (IsPointInRect(GetTabRect(index), x, y)) {
            SetCurrentTab(index);
            return;
        }
    }

    if (m_currentTab == TabParty) {
        const bool showCreateButton = g_session.GetNumParty() == 0;
        const bool showLeaderButtons = g_session.GetNumParty() > 0 && g_session.m_amIPartyMaster;
        if ((showCreateButton || showLeaderButtons) && IsPointInRect(GetActionButtonRect(0), x, y)) {
            if (showCreateButton) {
                if (auto* window = static_cast<UIPartyOptionWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_PARTYOPTIONWND))) {
                    window->OpenCreateDialog();
                }
            } else {
                if (auto* window = static_cast<UIPartyOptionWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_PARTYOPTIONWND))) {
                    window->OpenConfigureDialog();
                }
            }
            return;
        }
        if (showLeaderButtons && IsPointInRect(GetActionButtonRect(1), x, y)) {
            g_modeMgr.SendMsg(CGameMode::GameMsg_RequestPartyDisband, 0, 0, 0);
            return;
        }
    }

    const int rowIndex = GetRowAtPoint(x, y);
    if (rowIndex >= 0) {
        m_selectedIndex = rowIndex;
        Invalidate();
    }
}

void UIMessengerGroupWnd::OnLBtnDblClk(int x, int y)
{
    OnLBtnUp(x, y);
}

void UIMessengerGroupWnd::OnMouseMove(int x, int y)
{
    UIFrameWnd::OnMouseMove(x, y);
}

void UIMessengerGroupWnd::OnWheel(int delta)
{
    NormalizeStateForData();
    if (delta > 0) {
        m_viewOffset = (std::max)(0, m_viewOffset - 1);
    } else if (delta < 0) {
        m_viewOffset = (std::min)(GetMaxViewOffset(), m_viewOffset + 1);
    }
    Invalidate();
}

void UIMessengerGroupWnd::StoreInfo()
{
    SaveUiWindowPlacement("MessengerGroupWnd", m_x, m_y);
}

bool UIMessengerGroupWnd::GetDisplayDataForQt(DisplayData* outData) const
{
    if (!outData) {
        return false;
    }

    outData->title = "Friend / Party";
    outData->currentTab = m_currentTab;
    outData->selectedIndex = m_selectedIndex;
    outData->viewOffset = m_viewOffset;
    outData->maxViewOffset = GetMaxViewOffset();

    if (m_currentTab == TabFriend) {
        outData->summaryTitle = "Friends";
        char summary[64] = {};
        std::snprintf(summary, sizeof(summary), "%u registered", g_session.GetNumFriend());
        outData->summaryValue = summary;
        outData->summarySecondaryValue.clear();

        int absoluteIndex = 0;
        int visibleIndex = 0;
        for (const FRIEND_INFO& info : g_session.GetFriendList()) {
            if (absoluteIndex++ < m_viewOffset) {
                continue;
            }
            if (visibleIndex >= GetVisibleRowCount()) {
                break;
            }

            DisplayRow row{};
            const RECT rowRect = GetRowRect(visibleIndex);
            row.x = rowRect.left;
            row.y = rowRect.top;
            row.width = rowRect.right - rowRect.left;
            row.height = rowRect.bottom - rowRect.top;
            row.selected = (m_selectedIndex == (m_viewOffset + visibleIndex));
            row.color = info.color;
            row.name = info.characterName;
            row.status = info.state == 0 ? "Online" : "Offline";
            row.detail = info.state == 0 ? "Available" : "Unavailable";
            outData->rows.push_back(std::move(row));
            ++visibleIndex;
        }
    } else {
        outData->summaryTitle = g_session.m_partyName.empty() ? "Party" : "Party Name";
        if (g_session.GetNumParty() == 0) {
            outData->summaryValue = "0 members";
            outData->summarySecondaryValue.clear();
        } else {
            const std::string memberSummary = BuildPartyMemberSummary();
            outData->summaryValue = g_session.m_partyName.empty()
                ? memberSummary
                : g_session.m_partyName + "  |  " + memberSummary;
            outData->summarySecondaryValue = BuildPartyOptionSummary();
        }

        if (g_session.GetNumParty() == 0) {
            const RECT buttonRect = GetActionButtonRect(0);
            QtButtonDisplay button{};
            button.id = kButtonIdCreateParty;
            button.x = buttonRect.left;
            button.y = buttonRect.top;
            button.width = buttonRect.right - buttonRect.left;
            button.height = buttonRect.bottom - buttonRect.top;
            button.label = "Create Party";
            outData->actionButtons.push_back(std::move(button));
        } else if (g_session.m_amIPartyMaster) {
            for (int slot = 0; slot < 2; ++slot) {
                const RECT buttonRect = GetActionButtonRect(slot);
                QtButtonDisplay button{};
                button.id = slot == 0 ? kButtonIdConfigureParty : kButtonIdDisbandParty;
                button.x = buttonRect.left;
                button.y = buttonRect.top;
                button.width = buttonRect.right - buttonRect.left;
                button.height = buttonRect.bottom - buttonRect.top;
                button.label = slot == 0 ? "Configure" : "Disband";
                outData->actionButtons.push_back(std::move(button));
            }
        }

        int absoluteIndex = 0;
        int visibleIndex = 0;
        for (const FRIEND_INFO& info : g_session.GetPartyList()) {
            if (absoluteIndex++ < m_viewOffset) {
                continue;
            }
            if (visibleIndex >= GetVisibleRowCount()) {
                break;
            }

            DisplayRow row{};
            const RECT rowRect = GetRowRect(visibleIndex);
            row.x = rowRect.left;
            row.y = rowRect.top;
            row.width = rowRect.right - rowRect.left;
            row.height = rowRect.bottom - rowRect.top;
            row.selected = (m_selectedIndex == (m_viewOffset + visibleIndex));
            row.color = info.color;
            row.name = info.characterName;
            row.status = info.role == 0 ? "Leader" : "Member";
            row.detail = info.state == 0 ? TrimMapDisplayName(info.mapName) : "Offline";
            outData->rows.push_back(std::move(row));
            ++visibleIndex;
        }
    }

    outData->scrollBarVisible = outData->maxViewOffset > 0;
    const RECT trackRect = GetScrollTrackRect();
    const RECT thumbRect = GetScrollThumbRect();
    outData->scrollTrackX = trackRect.left;
    outData->scrollTrackY = trackRect.top;
    outData->scrollTrackWidth = trackRect.right - trackRect.left;
    outData->scrollTrackHeight = trackRect.bottom - trackRect.top;
    outData->scrollThumbX = thumbRect.left;
    outData->scrollThumbY = thumbRect.top;
    outData->scrollThumbWidth = thumbRect.right - thumbRect.left;
    outData->scrollThumbHeight = thumbRect.bottom - thumbRect.top;
    return true;
}

int UIMessengerGroupWnd::GetQtSystemButtonCount() const
{
    return 1;
}

bool UIMessengerGroupWnd::GetQtSystemButtonDisplayForQt(int index, QtButtonDisplay* outData) const
{
    if (!outData || index != 0) {
        return false;
    }

    const RECT rect = GetCloseButtonRect();
    outData->id = kButtonIdClose;
    outData->x = rect.left;
    outData->y = rect.top;
    outData->width = rect.right - rect.left;
    outData->height = rect.bottom - rect.top;
    outData->label = "x";
    outData->visible = true;
    outData->active = false;
    return true;
}

int UIMessengerGroupWnd::GetQtTabCount() const
{
    return 2;
}

bool UIMessengerGroupWnd::GetQtTabDisplayForQt(int index, QtButtonDisplay* outData) const
{
    if (!outData || index < 0 || index >= 2) {
        return false;
    }

    const RECT rect = GetTabRect(index);
    outData->id = index;
    outData->x = rect.left;
    outData->y = rect.top;
    outData->width = rect.right - rect.left;
    outData->height = rect.bottom - rect.top;
    outData->label = index == TabFriend ? "Friend" : "Party";
    outData->visible = true;
    outData->active = (index == m_currentTab);
    return true;
}

void UIMessengerGroupWnd::NormalizeStateForData()
{
    const int entryCount = GetEntryCount();
    const int maxViewOffset = GetMaxViewOffset();
    m_viewOffset = std::clamp(m_viewOffset, 0, maxViewOffset);
    if (entryCount <= 0) {
        m_selectedIndex = -1;
        return;
    }
    m_selectedIndex = std::clamp(m_selectedIndex, 0, entryCount - 1);
}

void UIMessengerGroupWnd::SetCurrentTab(int tabId)
{
    const int clampedTab = std::clamp(tabId, 0, 1);
    if (m_currentTab == clampedTab) {
        return;
    }

    m_currentTab = clampedTab;
    m_selectedIndex = -1;
    m_viewOffset = 0;
    NormalizeStateForData();
    Invalidate();
}

int UIMessengerGroupWnd::GetEntryCount() const
{
    return m_currentTab == TabFriend
        ? static_cast<int>(g_session.GetNumFriend())
        : static_cast<int>(g_session.GetNumParty());
}

int UIMessengerGroupWnd::GetMaxViewOffset() const
{
    return (std::max)(0, GetEntryCount() - GetVisibleRowCount());
}

int UIMessengerGroupWnd::GetVisibleRowCount() const
{
    const RECT rect = GetListRect();
    const int rowCount = static_cast<int>((rect.bottom - rect.top) / kRowHeight);
    return (std::max)(1, rowCount);
}

int UIMessengerGroupWnd::GetRowAtPoint(int x, int y) const
{
    const RECT listRect = GetListRect();
    if (!IsPointInRect(listRect, x, y)) {
        return -1;
    }

    const int visibleIndex = (y - listRect.top) / kRowHeight;
    const int absoluteIndex = m_viewOffset + visibleIndex;
    return absoluteIndex < GetEntryCount() ? absoluteIndex : -1;
}

RECT UIMessengerGroupWnd::GetCloseButtonRect() const
{
    return RECT{ m_x + m_w - 22, m_y + 3, m_x + m_w - 22 + kCloseButtonWidth, m_y + 3 + kCloseButtonHeight };
}

RECT UIMessengerGroupWnd::GetTabRect(int index) const
{
    const int left = m_x + 10 + index * (kTabWidth + kTabGap);
    return RECT{ left, m_y + kContentTop, left + kTabWidth, m_y + kContentTop + kTabHeight };
}

RECT UIMessengerGroupWnd::GetListRect() const
{
    return RECT{ m_x + kListLeft, m_y + kListTop, m_x + m_w - kListRight, m_y + m_h - kListBottom };
}

RECT UIMessengerGroupWnd::GetRowRect(int visibleIndex) const
{
    const RECT listRect = GetListRect();
    const int top = listRect.top + visibleIndex * kRowHeight;
    return RECT{ listRect.left + 1, top, listRect.right - kScrollWidth - 2, top + kRowHeight };
}

RECT UIMessengerGroupWnd::GetActionButtonRect(int slot) const
{
    const int footerTop = m_y + m_h - (kFooterHeight + kFooterBottomMargin);
    const int top = footerTop - kFooterButtonGap - kActionButtonHeight;
    if (g_session.GetNumParty() == 0 || !g_session.m_amIPartyMaster) {
        const int left = m_x + (m_w - kActionButtonWidth) / 2;
        return RECT{ left, top, left + kActionButtonWidth, top + kActionButtonHeight };
    }

    const int left = m_x + 42 + slot * (kActionButtonWidth + 12);
    return RECT{ left, top, left + kActionButtonWidth, top + kActionButtonHeight };
}

RECT UIMessengerGroupWnd::GetScrollTrackRect() const
{
    const RECT listRect = GetListRect();
    return RECT{ listRect.right - kScrollWidth, listRect.top + 1, listRect.right - 1, listRect.bottom - 1 };
}

RECT UIMessengerGroupWnd::GetScrollThumbRect() const
{
    const RECT trackRect = GetScrollTrackRect();
    const int maxViewOffset = GetMaxViewOffset();
    if (maxViewOffset <= 0) {
        return trackRect;
    }

    const int trackHeight = trackRect.bottom - trackRect.top;
    const int visibleRows = GetVisibleRowCount();
    const int totalRows = GetEntryCount();
    const int thumbHeight = (std::max)(kScrollThumbMinHeight, (trackHeight * visibleRows) / (std::max)(visibleRows, totalRows));
    const int travel = (std::max)(0, trackHeight - thumbHeight);
    const int thumbTop = trackRect.top + (travel * m_viewOffset) / maxViewOffset;
    return RECT{ trackRect.left + 1, thumbTop, trackRect.right - 1, thumbTop + thumbHeight };
}

unsigned long long UIMessengerGroupWnd::BuildVisualStateToken() const
{
    unsigned long long hash = 1469598103934665603ull;
    HashTokenValue(&hash, static_cast<unsigned long long>(m_show));
    HashTokenValue(&hash, static_cast<unsigned long long>(m_currentTab));
    HashTokenValue(&hash, static_cast<unsigned long long>(m_selectedIndex + 1));
    HashTokenValue(&hash, static_cast<unsigned long long>(m_viewOffset));
    HashTokenValue(&hash, static_cast<unsigned long long>(m_x));
    HashTokenValue(&hash, static_cast<unsigned long long>(m_y));

    if (m_currentTab == TabFriend) {
        for (const FRIEND_INFO& info : g_session.GetFriendList()) {
            HashTokenValue(&hash, static_cast<unsigned long long>(info.isValid));
            HashTokenValue(&hash, static_cast<unsigned long long>(info.AID));
            HashTokenValue(&hash, static_cast<unsigned long long>(info.GID));
            HashTokenValue(&hash, static_cast<unsigned long long>(info.state));
            HashTokenValue(&hash, static_cast<unsigned long long>(info.color));
            HashTokenString(&hash, info.characterName);
        }
    } else {
        HashTokenString(&hash, g_session.m_partyName);
        HashTokenValue(&hash, static_cast<unsigned long long>(g_session.m_amIPartyMaster ? 1 : 0));
        HashTokenValue(&hash, static_cast<unsigned long long>(g_session.m_itemDivType ? 1 : 0));
        HashTokenValue(&hash, static_cast<unsigned long long>(g_session.m_itemCollectType ? 1 : 0));
        for (const FRIEND_INFO& info : g_session.GetPartyList()) {
            HashTokenValue(&hash, static_cast<unsigned long long>(info.isValid));
            HashTokenValue(&hash, static_cast<unsigned long long>(info.AID));
            HashTokenValue(&hash, static_cast<unsigned long long>(info.GID));
            HashTokenValue(&hash, static_cast<unsigned long long>(info.role));
            HashTokenValue(&hash, static_cast<unsigned long long>(info.state));
            HashTokenValue(&hash, static_cast<unsigned long long>(info.color));
            HashTokenString(&hash, info.characterName);
            HashTokenString(&hash, info.mapName);
        }
    }

    return hash;
}