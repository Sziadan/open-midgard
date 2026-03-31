#include "UIItemSellWnd.h"

#include "UIShopCommon.h"
#include "UIItemShopWnd.h"
#include "UIWindowMgr.h"
#include "gamemode/GameMode.h"
#include "gamemode/Mode.h"
#include "main/WinMain.h"
#include "qtui/QtUiRuntime.h"
#include "session/Session.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <string>

namespace {

constexpr int kWindowWidth = 240;
constexpr int kWindowHeight = 274;
constexpr int kListTop = 22;
constexpr int kListBottomMargin = 58;
constexpr int kListSideMargin = 8;
constexpr int kRowHeight = 18;
constexpr int kButtonWidth = 68;
constexpr int kButtonHeight = 20;

std::string FormatNumber(int value)
{
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%d", value);
    return std::string(buffer);
}

void CloseSellWindows()
{
    g_windowMgr.CloseNpcShopWindows();
}

void DrawButton(HDC hdc, const RECT& rect, const char* label, bool hot, bool pressed)
{
    COLORREF fill = RGB(220, 220, 220);
    if (pressed) {
        fill = RGB(170, 185, 205);
    } else if (hot) {
        fill = RGB(196, 210, 228);
    }

    shopui::FillRectColor(hdc, rect, fill);
    shopui::FrameRectColor(hdc, rect, RGB(88, 88, 88));
    shopui::DrawWindowTextRect(hdc, rect, label ? label : "", RGB(24, 24, 24), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

} // namespace

UIItemSellWnd::UIItemSellWnd()
    : m_viewOffset(0),
      m_hoverRow(-1),
      m_hoverButton(ButtonNone),
      m_pressedButton(ButtonNone),
      m_lastDrawStateToken(0ull),
      m_hasDrawStateToken(false)
{
    Create(kWindowWidth, kWindowHeight);
    Move(412, 98);
}

UIItemSellWnd::~UIItemSellWnd() = default;

void UIItemSellWnd::SetShow(int show)
{
    UIWindow::SetShow(show);
    if (show == 0) {
        m_hoverRow = -1;
        m_hoverButton = ButtonNone;
        m_pressedButton = ButtonNone;
        return;
    }

    int savedX = m_x;
    int savedY = m_y;
    if (LoadUiWindowPlacement("ItemSellWnd", &savedX, &savedY)) {
        g_windowMgr.ClampWindowToClient(&savedX, &savedY, m_w, m_h);
        Move(savedX, savedY);
    }
}

bool UIItemSellWnd::IsUpdateNeed()
{
    if (m_show == 0) {
        return false;
    }
    if (m_isDirty != 0 || !m_hasDrawStateToken) {
        return true;
    }
    return BuildDisplayStateToken() != m_lastDrawStateToken;
}

void UIItemSellWnd::StoreInfo()
{
    SaveUiWindowPlacement("ItemSellWnd", m_x, m_y);
}

RECT UIItemSellWnd::GetListRect() const
{
    return shopui::MakeRect(m_x + kListSideMargin, m_y + kListTop, m_w - kListSideMargin * 2, m_h - kListTop - kListBottomMargin);
}

int UIItemSellWnd::GetVisibleRowCount() const
{
    return (std::max)(1, (m_h - kListTop - kListBottomMargin) / kRowHeight);
}

int UIItemSellWnd::GetMaxViewOffset() const
{
    const int visibleRows = GetVisibleRowCount();
    if (static_cast<int>(g_session.m_shopDealRows.size()) <= visibleRows) {
        return 0;
    }
    return static_cast<int>(g_session.m_shopDealRows.size()) - visibleRows;
}

int UIItemSellWnd::HitTestDealRow(int x, int y) const
{
    const RECT listRect = GetListRect();
    if (x < listRect.left || x >= listRect.right || y < listRect.top || y >= listRect.bottom) {
        return -1;
    }

    const int localRow = (y - listRect.top) / kRowHeight;
    const int rowIndex = m_viewOffset + localRow - 1;
    if (rowIndex < 0 || rowIndex >= static_cast<int>(g_session.m_shopDealRows.size())) {
        return -1;
    }
    return rowIndex;
}

RECT UIItemSellWnd::GetButtonRect(ButtonId buttonId) const
{
    switch (buttonId) {
    case ButtonAdd:
        return shopui::MakeRect(m_x + 10, m_y + m_h - 52, kButtonWidth, kButtonHeight);
    case ButtonRemove:
        return shopui::MakeRect(m_x + 84, m_y + m_h - 52, kButtonWidth, kButtonHeight);
    case ButtonConfirm:
        return shopui::MakeRect(m_x + 10, m_y + m_h - 28, 104, kButtonHeight);
    case ButtonCancel:
        return shopui::MakeRect(m_x + 124, m_y + m_h - 28, 104, kButtonHeight);
    default:
        return shopui::MakeRect(0, 0, 0, 0);
    }
}

UIItemSellWnd::ButtonId UIItemSellWnd::HitTestButton(int x, int y) const
{
    for (int id = ButtonAdd; id <= ButtonCancel; ++id) {
        const RECT rect = GetButtonRect(static_cast<ButtonId>(id));
        if (x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom) {
            return static_cast<ButtonId>(id);
        }
    }
    return ButtonNone;
}

void UIItemSellWnd::ActivateButton(ButtonId buttonId)
{
    switch (buttonId) {
    case ButtonAdd:
        if (g_session.m_shopSelectedSourceRow >= 0) {
            g_session.AdjustNpcShopDealBySourceRow(static_cast<size_t>(g_session.m_shopSelectedSourceRow), 1);
        }
        break;
    case ButtonRemove:
        if (g_session.m_shopSelectedDealRow >= 0) {
            g_session.AdjustNpcShopDealByDealRow(static_cast<size_t>(g_session.m_shopSelectedDealRow), -1);
        } else if (g_session.m_shopSelectedSourceRow >= 0) {
            g_session.AdjustNpcShopDealBySourceRow(static_cast<size_t>(g_session.m_shopSelectedSourceRow), -1);
        }
        break;
    case ButtonConfirm:
        g_modeMgr.SendMsg(CGameMode::GameMsg_RequestShopSellList, 0, 0, 0);
        break;
    case ButtonCancel:
        g_session.ClearNpcShopState();
        CloseSellWindows();
        return;
    default:
        break;
    }
}

void UIItemSellWnd::OnDraw()
{
    if (IsQtUiRuntimeEnabled()) {
        m_lastDrawStateToken = BuildDisplayStateToken();
        m_hasDrawStateToken = true;
        return;
    }

    bool useShared = false;
    HDC hdc = AcquireDrawTarget(&useShared);
    if (!hdc || m_show == 0) {
        return;
    }

    const RECT bounds = shopui::MakeRect(m_x, m_y, m_w, m_h);
    const RECT listRect = GetListRect();
    shopui::DrawFrameWindow(hdc, bounds, "Sell");
    shopui::FillRectColor(hdc, listRect, RGB(248, 248, 248));
    shopui::FrameRectColor(hdc, listRect, RGB(120, 120, 120));

    const RECT headerRect = shopui::MakeRect(listRect.left + 1, listRect.top + 1, listRect.right - listRect.left - 2, kRowHeight - 1);
    shopui::FillRectColor(hdc, headerRect, RGB(222, 229, 237));
    shopui::DrawWindowTextRect(hdc, shopui::MakeRect(headerRect.left + 4, headerRect.top + 1, 126, headerRect.bottom - headerRect.top - 2), "Item", RGB(30, 30, 30), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    shopui::DrawWindowTextRect(hdc, shopui::MakeRect(headerRect.left + 126, headerRect.top + 1, 34, headerRect.bottom - headerRect.top - 2), "Qty", RGB(30, 30, 30), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    shopui::DrawWindowTextRect(hdc, shopui::MakeRect(headerRect.right - 64, headerRect.top + 1, 56, headerRect.bottom - headerRect.top - 2), "Gain", RGB(30, 30, 30), DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    const int visibleRows = GetVisibleRowCount();
    const int startRow = m_viewOffset;
    const int endRow = (std::min)(static_cast<int>(g_session.m_shopDealRows.size()), startRow + visibleRows - 1);
    for (int rowIndex = startRow; rowIndex < endRow; ++rowIndex) {
        const NPC_SHOP_DEAL_ROW& row = g_session.m_shopDealRows[static_cast<size_t>(rowIndex)];
        RECT rowRect = shopui::MakeRect(listRect.left + 1,
            listRect.top + 1 + (rowIndex - startRow + 1) * kRowHeight,
            listRect.right - listRect.left - 2,
            kRowHeight);
        const bool selected = g_session.m_shopSelectedDealRow == rowIndex;
        const bool hot = m_hoverRow == rowIndex;
        if (selected) {
            shopui::FillRectColor(hdc, rowRect, RGB(188, 204, 226));
        } else if (hot) {
            shopui::FillRectColor(hdc, rowRect, RGB(226, 234, 244));
        }

        shopui::DrawWindowTextRect(hdc,
            shopui::MakeRect(rowRect.left + 4, rowRect.top + 1, 126, rowRect.bottom - rowRect.top - 2),
            shopui::GetItemDisplayName(row.itemInfo),
            RGB(26, 26, 26),
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        shopui::DrawWindowTextRect(hdc,
            shopui::MakeRect(rowRect.left + 126, rowRect.top + 1, 34, rowRect.bottom - rowRect.top - 2),
            FormatNumber(row.quantity),
            RGB(48, 48, 48),
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        shopui::DrawWindowTextRect(hdc,
            shopui::MakeRect(rowRect.right - 64, rowRect.top + 1, 56, rowRect.bottom - rowRect.top - 2),
            FormatNumber(row.unitPrice * row.quantity),
            RGB(28, 60, 98),
            DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }

    const RECT totalLabelRect = shopui::MakeRect(m_x + 10, m_y + m_h - 75, 80, 16);
    const RECT totalValueRect = shopui::MakeRect(m_x + 90, m_y + m_h - 75, 138, 16);
    shopui::DrawWindowTextRect(hdc, totalLabelRect, "Total", RGB(36, 36, 36), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    shopui::DrawWindowTextRect(hdc, totalValueRect, FormatNumber(g_session.m_shopDealTotal), RGB(28, 60, 98), DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    DrawButton(hdc, GetButtonRect(ButtonAdd), "Add", m_hoverButton == ButtonAdd, m_pressedButton == ButtonAdd);
    DrawButton(hdc, GetButtonRect(ButtonRemove), "Remove", m_hoverButton == ButtonRemove, m_pressedButton == ButtonRemove);
    DrawButton(hdc, GetButtonRect(ButtonConfirm), "Sell", m_hoverButton == ButtonConfirm, m_pressedButton == ButtonConfirm);
    DrawButton(hdc, GetButtonRect(ButtonCancel), "Cancel", m_hoverButton == ButtonCancel, m_pressedButton == ButtonCancel);

    m_lastDrawStateToken = BuildDisplayStateToken();
    m_hasDrawStateToken = true;
    ReleaseDrawTarget(hdc, useShared);
}

void UIItemSellWnd::OnLBtnDown(int x, int y)
{
    m_pressedButton = HitTestButton(x, y);
    if (m_pressedButton != ButtonNone) {
        return;
    }

    const int rowIndex = HitTestDealRow(x, y);
    if (rowIndex >= 0) {
        g_session.m_shopSelectedDealRow = rowIndex;
        return;
    }

    UIFrameWnd::OnLBtnDown(x, y);
}

void UIItemSellWnd::OnLBtnDblClk(int x, int y)
{
    const int rowIndex = HitTestDealRow(x, y);
    if (rowIndex >= 0) {
        g_session.m_shopSelectedDealRow = rowIndex;
        g_session.AdjustNpcShopDealByDealRow(static_cast<size_t>(rowIndex), -1);
    }
}

void UIItemSellWnd::OnLBtnUp(int x, int y)
{
    const ButtonId releasedButton = HitTestButton(x, y);
    const ButtonId pressedButton = m_pressedButton;
    m_pressedButton = ButtonNone;
    UIFrameWnd::OnLBtnUp(x, y);
    if (pressedButton != ButtonNone && releasedButton == pressedButton) {
        ActivateButton(pressedButton);
    }
}

void UIItemSellWnd::OnMouseMove(int x, int y)
{
    m_hoverButton = HitTestButton(x, y);
    m_hoverRow = HitTestDealRow(x, y);
    UIFrameWnd::OnMouseMove(x, y);
}

void UIItemSellWnd::OnWheel(int delta)
{
    const int step = delta > 0 ? -1 : 1;
    m_viewOffset = (std::max)(0, (std::min)(GetMaxViewOffset(), m_viewOffset + step));
}

void UIItemSellWnd::HandleKeyDown(int virtualKey)
{
    if (virtualKey == VK_ESCAPE) {
        ActivateButton(ButtonCancel);
        return;
    }
    if (virtualKey == VK_RETURN) {
        ActivateButton(ButtonConfirm);
        return;
    }
    if (virtualKey == VK_ADD || virtualKey == VK_OEM_PLUS) {
        ActivateButton(ButtonAdd);
        return;
    }
    if (virtualKey == VK_SUBTRACT || virtualKey == VK_OEM_MINUS) {
        ActivateButton(ButtonRemove);
        return;
    }

    if (g_session.m_shopDealRows.empty()) {
        return;
    }

    int selectedRow = g_session.m_shopSelectedDealRow;
    if (selectedRow < 0) {
        selectedRow = 0;
    }
    if (virtualKey == VK_UP) {
        selectedRow = (std::max)(0, selectedRow - 1);
    } else if (virtualKey == VK_DOWN) {
        selectedRow = (std::min)(static_cast<int>(g_session.m_shopDealRows.size()) - 1, selectedRow + 1);
    } else {
        return;
    }

    g_session.m_shopSelectedDealRow = selectedRow;
    if (selectedRow < m_viewOffset) {
        m_viewOffset = selectedRow;
    } else if (selectedRow >= m_viewOffset + GetVisibleRowCount() - 1) {
        m_viewOffset = selectedRow - GetVisibleRowCount() + 2;
    }
    m_viewOffset = (std::max)(0, (std::min)(GetMaxViewOffset(), m_viewOffset));
}

int UIItemSellWnd::GetViewOffset() const
{
    return m_viewOffset;
}

int UIItemSellWnd::GetHoverRow() const
{
    return m_hoverRow;
}

int UIItemSellWnd::GetVisibleRowCountForQt() const
{
    return GetVisibleRowCount();
}

int UIItemSellWnd::GetHoverButton() const
{
    return static_cast<int>(m_hoverButton);
}

int UIItemSellWnd::GetPressedButton() const
{
    return static_cast<int>(m_pressedButton);
}

unsigned long long UIItemSellWnd::BuildDisplayStateToken() const
{
    unsigned long long hash = 1469598103934665603ull;
    shopui::HashTokenValue(&hash, static_cast<unsigned long long>(m_show));
    shopui::HashTokenValue(&hash, static_cast<unsigned long long>(m_x));
    shopui::HashTokenValue(&hash, static_cast<unsigned long long>(m_y));
    shopui::HashTokenValue(&hash, static_cast<unsigned long long>(m_w));
    shopui::HashTokenValue(&hash, static_cast<unsigned long long>(m_h));
    shopui::HashTokenValue(&hash, static_cast<unsigned long long>(m_viewOffset));
    shopui::HashTokenValue(&hash, static_cast<unsigned long long>(static_cast<unsigned int>(m_hoverRow + 1)));
    shopui::HashTokenValue(&hash, static_cast<unsigned long long>(static_cast<int>(m_hoverButton) + 1));
    shopui::HashTokenValue(&hash, static_cast<unsigned long long>(static_cast<int>(m_pressedButton) + 1));
    shopui::HashTokenValue(&hash, static_cast<unsigned long long>(g_session.m_shopSelectedDealRow + 1));
    shopui::HashTokenValue(&hash, static_cast<unsigned long long>(g_session.m_shopSelectedSourceRow + 1));
    shopui::HashTokenValue(&hash, static_cast<unsigned long long>(g_session.m_shopDealTotal));
    shopui::HashTokenValue(&hash, static_cast<unsigned long long>(g_session.m_shopDealRows.size()));
    for (const NPC_SHOP_DEAL_ROW& row : g_session.m_shopDealRows) {
        shopui::HashTokenValue(&hash, static_cast<unsigned long long>(row.itemInfo.GetItemId()));
        shopui::HashTokenValue(&hash, static_cast<unsigned long long>(row.sourceItemIndex));
        shopui::HashTokenValue(&hash, static_cast<unsigned long long>(row.unitPrice));
        shopui::HashTokenValue(&hash, static_cast<unsigned long long>(row.quantity));
    }
    return hash;
}
