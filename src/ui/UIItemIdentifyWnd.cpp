#include "UiMetrics.h"
#include "UIItemIdentifyWnd.h"

#include "gamemode/GameMode.h"
#include "gamemode/Mode.h"
#include "item/Item.h"
#include "main/WinMain.h"
#include "session/Session.h"
#include "UIWindowMgr.h"

#include <algorithm>

namespace {
constexpr int kWindowWidth = 244;
constexpr int kWindowHeight = 221;
constexpr int kTitleBarHeight = uiMetrics::titleHeight;
constexpr int kCloseButtonSize = 12;
constexpr int kPadding = 8;
constexpr int kListTop = 27;
constexpr int kRowHeight = 32;
constexpr int kVisibleRows = 4;
constexpr int kIconBoxSize = 24;
constexpr int kScrollBarWidth = 10;
constexpr int kScrollBarGap = 4;
constexpr int kButtonWidth = 56;
constexpr int kButtonHeight = 20;
constexpr int kButtonGap = 8;

bool IsInsideRect(const RECT& rect, int x, int y)
{
    return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
}

unsigned long long BuildItemIconKey(const ITEM_INFO& item)
{
    return (static_cast<unsigned long long>(item.GetItemId()) << 1)
        | static_cast<unsigned long long>(item.m_isIdentified != 0 ? 1 : 0);
}
}

UIItemIdentifyWnd::UIItemIdentifyWnd()
    : m_itemIndices(),
      m_closeHovered(false),
      m_closePressed(false),
      m_okHovered(false),
      m_okPressed(false),
      m_cancelHovered(false),
      m_cancelPressed(false),
      m_hasStoredPlacement(false),
      m_hoveredItemIndex(0),
      m_selectedItemIndex(0),
      m_viewOffset(0),
      m_isDraggingScrollThumb(false),
      m_scrollDragOffsetY(0),
      m_iconCache()
{
    Create(kWindowWidth, kWindowHeight);
    Move(72, 72);

    int savedX = m_x;
    int savedY = m_y;
    if (LoadUiWindowPlacement("ItemIdentifyWnd", &savedX, &savedY)) {
        g_windowMgr.ClampWindowToClient(&savedX, &savedY, m_w, m_h);
        Move(savedX, savedY);
        m_hasStoredPlacement = true;
    }
}

RECT UIItemIdentifyWnd::GetCloseButtonRect() const
{
    return shopui::MakeRect(m_x + m_w - kPadding - kCloseButtonSize, m_y + 2, kCloseButtonSize, kCloseButtonSize);
}

RECT UIItemIdentifyWnd::GetListRect() const
{
    const int width = m_w - (kPadding * 2) - kScrollBarGap - kScrollBarWidth;
    return shopui::MakeRect(m_x + kPadding, m_y + kListTop, width, kRowHeight * kVisibleRows);
}

RECT UIItemIdentifyWnd::GetRowRect(int visibleRow) const
{
    const RECT listRect = GetListRect();
    return shopui::MakeRect(listRect.left, listRect.top + visibleRow * kRowHeight, listRect.right - listRect.left, kRowHeight);
}

RECT UIItemIdentifyWnd::GetScrollTrackRect() const
{
    const RECT listRect = GetListRect();
    return shopui::MakeRect(listRect.right + kScrollBarGap, listRect.top, kScrollBarWidth, listRect.bottom - listRect.top);
}

RECT UIItemIdentifyWnd::GetScrollThumbRect(int itemCount) const
{
    const RECT trackRect = GetScrollTrackRect();
    if (!IsScrollBarVisible(itemCount) || trackRect.bottom <= trackRect.top) {
        return shopui::MakeRect(trackRect.left, trackRect.top, trackRect.right - trackRect.left, trackRect.bottom - trackRect.top);
    }

    const int maxOffset = GetMaxViewOffset(itemCount);
    const int trackHeight = trackRect.bottom - trackRect.top;
    const int thumbHeight = (std::max)(18, (trackHeight * kVisibleRows) / (std::max)(itemCount, 1));
    const int travel = (std::max)(0, trackHeight - thumbHeight);
    const int thumbY = trackRect.top + (maxOffset > 0 ? (travel * m_viewOffset) / maxOffset : 0);
    return shopui::MakeRect(trackRect.left, thumbY, trackRect.right - trackRect.left, thumbHeight);
}

RECT UIItemIdentifyWnd::GetOkButtonRect() const
{
    const int y = m_y + m_h - kPadding - kButtonHeight;
    const int cancelLeft = m_x + m_w - kPadding - kButtonWidth;
    const int okLeft = cancelLeft - kButtonGap - kButtonWidth;
    return shopui::MakeRect(okLeft, y, kButtonWidth, kButtonHeight);
}

RECT UIItemIdentifyWnd::GetCancelButtonRect() const
{
    const int y = m_y + m_h - kPadding - kButtonHeight;
    const int left = m_x + m_w - kPadding - kButtonWidth;
    return shopui::MakeRect(left, y, kButtonWidth, kButtonHeight);
}

int UIItemIdentifyWnd::GetMaxViewOffset(int itemCount) const
{
    return (std::max)(0, itemCount - kVisibleRows);
}

bool UIItemIdentifyWnd::IsScrollBarVisible(int itemCount) const
{
    return itemCount > kVisibleRows;
}

void UIItemIdentifyWnd::ClampViewOffset(int itemCount)
{
    m_viewOffset = std::clamp(m_viewOffset, 0, GetMaxViewOffset(itemCount));
}

void UIItemIdentifyWnd::UpdateScrollFromThumbPosition(int globalY, int itemCount)
{
    const RECT trackRect = GetScrollTrackRect();
    const RECT thumbRect = GetScrollThumbRect(itemCount);
    const int maxOffset = GetMaxViewOffset(itemCount);
    if (maxOffset <= 0) {
        m_viewOffset = 0;
        return;
    }

    const int thumbHeight = thumbRect.bottom - thumbRect.top;
    const int minTop = trackRect.top;
    const int maxTop = trackRect.bottom - thumbHeight;
    const int clampedTop = std::clamp(globalY, minTop, maxTop);
    const int travel = (std::max)(1, maxTop - minTop);
    m_viewOffset = ((clampedTop - minTop) * maxOffset + (travel / 2)) / travel;
}

std::vector<const ITEM_INFO*> UIItemIdentifyWnd::GetAvailableItems() const
{
    std::vector<const ITEM_INFO*> items;
    items.reserve(m_itemIndices.size());
    for (unsigned int itemIndex : m_itemIndices) {
        const ITEM_INFO* item = g_session.GetInventoryItemByIndex(itemIndex);
        if (!item || item->m_itemIndex == 0 || item->m_num <= 0 || item->m_isIdentified != 0) {
            continue;
        }
        items.push_back(item);
    }
    return items;
}

const shopui::BitmapPixels* UIItemIdentifyWnd::GetItemIcon(const ITEM_INFO& item)
{
    const unsigned long long key = BuildItemIconKey(item);
    auto found = m_iconCache.find(key);
    if (found != m_iconCache.end()) {
        return found->second.IsValid() ? &found->second : nullptr;
    }

    shopui::BitmapPixels bitmap;
    shopui::TryLoadItemIconPixels(item, &bitmap);
    auto inserted = m_iconCache.emplace(key, std::move(bitmap));
    return inserted.first->second.IsValid() ? &inserted.first->second : nullptr;
}

bool UIItemIdentifyWnd::HasValidSelection() const
{
    if (m_selectedItemIndex == 0) {
        return false;
    }

    const ITEM_INFO* item = g_session.GetInventoryItemByIndex(static_cast<unsigned int>(m_selectedItemIndex));
    return item && item->m_itemIndex != 0 && item->m_num > 0 && item->m_isIdentified == 0;
}

int UIItemIdentifyWnd::HitTestVisibleRow(int x, int y) const
{
    const std::vector<const ITEM_INFO*> items = GetAvailableItems();
    const int itemCount = static_cast<int>(items.size());
    const int firstIndex = std::clamp(m_viewOffset, 0, GetMaxViewOffset(itemCount));
    const int visibleCount = (std::min)(kVisibleRows, itemCount - firstIndex);
    for (int index = 0; index < visibleCount; ++index) {
        if (IsInsideRect(GetRowRect(index), x, y)) {
            return firstIndex + index;
        }
    }
    return -1;
}

void UIItemIdentifyWnd::UpdateInteraction(int x, int y)
{
    const RECT closeRect = GetCloseButtonRect();
    const RECT okRect = GetOkButtonRect();
    const RECT cancelRect = GetCancelButtonRect();
    m_closeHovered = IsInsideRect(closeRect, x, y);
    m_okHovered = IsInsideRect(okRect, x, y);
    m_cancelHovered = IsInsideRect(cancelRect, x, y);

    m_hoveredItemIndex = 0;
    const int hitIndex = HitTestVisibleRow(x, y);
    if (hitIndex >= 0) {
        const std::vector<const ITEM_INFO*> items = GetAvailableItems();
        if (hitIndex < static_cast<int>(items.size()) && items[hitIndex]) {
            m_hoveredItemIndex = static_cast<int>(items[hitIndex]->m_itemIndex);
        }
    }
}

void UIItemIdentifyWnd::SetIdentifyItemIndices(const std::vector<unsigned int>& itemIndices)
{
    m_itemIndices = itemIndices;
    m_hoveredItemIndex = 0;
    m_selectedItemIndex = 0;
    m_viewOffset = 0;
    m_isDraggingScrollThumb = false;
    UpdateInteraction(-1, -1);
    Invalidate();
}

void UIItemIdentifyWnd::SubmitSelection()
{
    if (!HasValidSelection()) {
        return;
    }

    g_modeMgr.SendMsg(CGameMode::GameMsg_RequestIdentifyInventoryItem, m_selectedItemIndex, 0, 0);
    g_windowMgr.DeleteWindow(this);
}

void UIItemIdentifyWnd::CancelSelection()
{
    g_modeMgr.SendMsg(CGameMode::GameMsg_RequestIdentifyInventoryItem, static_cast<msgparam_t>(-1), 0, 0);
    g_windowMgr.DeleteWindow(this);
}

void UIItemIdentifyWnd::OnDraw()
{
    if (m_show == 0) {
        return;
    }

    HDC hdc = AcquireMainWindowDrawTarget();
    if (!hdc) {
        return;
    }

    const RECT bounds = shopui::MakeRect(m_x, m_y, m_w, m_h);
    shopui::DrawFrameWindow(hdc, bounds, "Identify Item");

    const RECT closeRect = GetCloseButtonRect();
    const RECT listRect = GetListRect();
    const RECT trackRect = GetScrollTrackRect();
    const RECT okRect = GetOkButtonRect();
    const RECT cancelRect = GetCancelButtonRect();
    const std::vector<const ITEM_INFO*> items = GetAvailableItems();
    const int itemCount = static_cast<int>(items.size());
    ClampViewOffset(itemCount);

    shopui::FillRectColor(hdc, closeRect, m_closePressed ? RGB(185, 199, 222) : (m_closeHovered ? RGB(215, 223, 240) : RGB(231, 226, 214)));
    shopui::FrameRectColor(hdc, closeRect, RGB(127, 122, 112));
    shopui::DrawWindowTextRect(hdc, closeRect, "X", RGB(0, 0, 0), DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    shopui::FillRectColor(hdc, listRect, RGB(255, 255, 255));
    shopui::FrameRectColor(hdc, listRect, RGB(196, 192, 181));

    if (itemCount <= 0) {
        shopui::DrawWindowTextRect(hdc,
                                   listRect,
                                   "No unidentified items available.",
                                   RGB(80, 80, 80),
                                   DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    } else {
        const int firstIndex = m_viewOffset;
        const int visibleCount = (std::min)(kVisibleRows, itemCount - firstIndex);
        for (int drawIndex = 0; drawIndex < visibleCount; ++drawIndex) {
            const ITEM_INFO* item = items[firstIndex + drawIndex];
            if (!item) {
                continue;
            }

            const RECT rowRect = GetRowRect(drawIndex);
            const bool hovered = m_hoveredItemIndex == static_cast<int>(item->m_itemIndex);
            const bool selected = m_selectedItemIndex == static_cast<int>(item->m_itemIndex);
            shopui::FillRectColor(hdc,
                                  rowRect,
                                  selected ? RGB(215, 223, 240) : (hovered ? RGB(236, 232, 222) : RGB(249, 247, 243)));
            shopui::FrameRectColor(hdc,
                                   rowRect,
                                   selected ? RGB(126, 149, 191) : RGB(188, 180, 167));

            const RECT iconRect = shopui::MakeRect(rowRect.left + 4,
                                                   rowRect.top + (kRowHeight - kIconBoxSize) / 2,
                                                   kIconBoxSize,
                                                   kIconBoxSize);
            shopui::FillRectColor(hdc, iconRect, RGB(245, 242, 234));
            shopui::FrameRectColor(hdc, iconRect, RGB(166, 159, 145));
            if (const shopui::BitmapPixels* icon = GetItemIcon(*item)) {
                const RECT dst = shopui::MakeRect(iconRect.left + 1, iconRect.top + 1, kIconBoxSize - 2, kIconBoxSize - 2);
                shopui::DrawBitmapPixelsTransparent(hdc, *icon, dst);
            }

            const RECT nameRect = shopui::MakeRect(rowRect.left + 34,
                                                   rowRect.top + 4,
                                                   rowRect.right - rowRect.left - 72,
                                                   12);
            shopui::DrawWindowTextRect(hdc,
                                       nameRect,
                                       item->GetDisplayName(),
                                       RGB(0, 0, 0),
                                       DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

            if (item->m_num > 1) {
                const RECT countRect = shopui::MakeRect(rowRect.right - 34, rowRect.top + 4, 28, 12);
                shopui::DrawWindowTextRect(hdc,
                                           countRect,
                                           std::to_string(item->m_num),
                                           RGB(90, 64, 32),
                                           DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
            }
        }
    }

    if (IsScrollBarVisible(itemCount)) {
        const RECT thumbRect = GetScrollThumbRect(itemCount);
        shopui::FillRectColor(hdc, trackRect, RGB(227, 231, 238));
        shopui::FrameRectColor(hdc, trackRect, RGB(164, 173, 189));
        shopui::FillRectColor(hdc, thumbRect, RGB(180, 188, 205));
        shopui::FrameRectColor(hdc, thumbRect, RGB(120, 130, 150));
    }

    shopui::FillRectColor(hdc,
                          okRect,
                          !HasValidSelection() ? RGB(208, 208, 208) : (m_okPressed ? RGB(185, 199, 222) : (m_okHovered ? RGB(215, 223, 240) : RGB(233, 228, 216))));
    shopui::FrameRectColor(hdc, okRect, RGB(140, 133, 120));
    shopui::DrawWindowTextRect(hdc, okRect, "OK", RGB(0, 0, 0), DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    shopui::FillRectColor(hdc,
                          cancelRect,
                          m_cancelPressed ? RGB(185, 199, 222) : (m_cancelHovered ? RGB(215, 223, 240) : RGB(233, 228, 216)));
    shopui::FrameRectColor(hdc, cancelRect, RGB(140, 133, 120));
    shopui::DrawWindowTextRect(hdc, cancelRect, "Cancel", RGB(0, 0, 0), DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    ReleaseMainWindowDrawTarget(hdc);
}

void UIItemIdentifyWnd::OnLBtnDblClk(int x, int y)
{
    const int hitIndex = HitTestVisibleRow(x, y);
    if (hitIndex < 0) {
        OnLBtnDown(x, y);
        return;
    }

    const std::vector<const ITEM_INFO*> items = GetAvailableItems();
    if (hitIndex < static_cast<int>(items.size()) && items[hitIndex]) {
        m_selectedItemIndex = static_cast<int>(items[hitIndex]->m_itemIndex);
        SubmitSelection();
    }
}

void UIItemIdentifyWnd::OnLBtnDown(int x, int y)
{
    UpdateInteraction(x, y);

    if (m_closeHovered) {
        m_closePressed = true;
        Invalidate();
        return;
    }
    if (m_okHovered) {
        m_okPressed = true;
        Invalidate();
        return;
    }
    if (m_cancelHovered) {
        m_cancelPressed = true;
        Invalidate();
        return;
    }

    const std::vector<const ITEM_INFO*> items = GetAvailableItems();
    const int itemCount = static_cast<int>(items.size());
    if (IsScrollBarVisible(itemCount)) {
        const RECT trackRect = GetScrollTrackRect();
        const RECT thumbRect = GetScrollThumbRect(itemCount);
        if (IsInsideRect(thumbRect, x, y)) {
            m_isDraggingScrollThumb = true;
            m_scrollDragOffsetY = y - thumbRect.top;
            return;
        }
        if (IsInsideRect(trackRect, x, y)) {
            UpdateScrollFromThumbPosition(y - ((thumbRect.bottom - thumbRect.top) / 2), itemCount);
            Invalidate();
            return;
        }
    }

    const int hitIndex = HitTestVisibleRow(x, y);
    if (hitIndex >= 0 && hitIndex < itemCount && items[hitIndex]) {
        m_selectedItemIndex = static_cast<int>(items[hitIndex]->m_itemIndex);
        Invalidate();
        return;
    }

    UIFrameWnd::OnLBtnDown(x, y);
}

void UIItemIdentifyWnd::OnLBtnUp(int x, int y)
{
    const bool closePressed = m_closePressed;
    const bool okPressed = m_okPressed;
    const bool cancelPressed = m_cancelPressed;
    m_closePressed = false;
    m_okPressed = false;
    m_cancelPressed = false;

    const bool wasDraggingScrollThumb = m_isDraggingScrollThumb;
    m_isDraggingScrollThumb = false;

    UIFrameWnd::OnLBtnUp(x, y);
    UpdateInteraction(x, y);

    if (wasDraggingScrollThumb) {
        Invalidate();
        return;
    }
    if (closePressed && m_closeHovered) {
        CancelSelection();
        return;
    }
    if (okPressed && m_okHovered) {
        SubmitSelection();
        return;
    }
    if (cancelPressed && m_cancelHovered) {
        CancelSelection();
        return;
    }

    Invalidate();
}

void UIItemIdentifyWnd::OnMouseMove(int x, int y)
{
    if (m_isDraggingScrollThumb) {
        const int itemCount = static_cast<int>(GetAvailableItems().size());
        UpdateScrollFromThumbPosition(y - m_scrollDragOffsetY, itemCount);
        Invalidate();
        return;
    }

    UIFrameWnd::OnMouseMove(x, y);
    UpdateInteraction(x, y);
    Invalidate();
}

void UIItemIdentifyWnd::OnMouseHover(int x, int y)
{
    UpdateInteraction(x, y);
    Invalidate();
}

void UIItemIdentifyWnd::OnWheel(int delta)
{
    const int itemCount = static_cast<int>(GetAvailableItems().size());
    if (delta > 0) {
        --m_viewOffset;
    } else if (delta < 0) {
        ++m_viewOffset;
    }
    ClampViewOffset(itemCount);
    Invalidate();
}

void UIItemIdentifyWnd::StoreInfo()
{
    SaveUiWindowPlacement("ItemIdentifyWnd", m_x, m_y);
}

bool UIItemIdentifyWnd::GetDisplayDataForQt(DisplayData* outData) const
{
    if (!outData) {
        return false;
    }

    DisplayData data{};
    data.title = "Identify Item";
    data.emptyText = "No unidentified items available.";

    const RECT closeRect = GetCloseButtonRect();
    data.closeButton.x = closeRect.left;
    data.closeButton.y = closeRect.top;
    data.closeButton.width = closeRect.right - closeRect.left;
    data.closeButton.height = closeRect.bottom - closeRect.top;
    data.closeButton.hovered = m_closeHovered;
    data.closeButton.pressed = m_closePressed;
    data.closeButton.label = "X";

    const RECT okRect = GetOkButtonRect();
    data.okButton.x = okRect.left;
    data.okButton.y = okRect.top;
    data.okButton.width = okRect.right - okRect.left;
    data.okButton.height = okRect.bottom - okRect.top;
    data.okButton.hovered = m_okHovered;
    data.okButton.pressed = m_okPressed;
    data.okButton.enabled = HasValidSelection();
    data.okButton.label = "OK";

    const RECT cancelRect = GetCancelButtonRect();
    data.cancelButton.x = cancelRect.left;
    data.cancelButton.y = cancelRect.top;
    data.cancelButton.width = cancelRect.right - cancelRect.left;
    data.cancelButton.height = cancelRect.bottom - cancelRect.top;
    data.cancelButton.hovered = m_cancelHovered;
    data.cancelButton.pressed = m_cancelPressed;
    data.cancelButton.label = "Cancel";

    const std::vector<const ITEM_INFO*> items = GetAvailableItems();
    const int itemCount = static_cast<int>(items.size());
    const int firstIndex = std::clamp(m_viewOffset, 0, GetMaxViewOffset(itemCount));
    const int visibleCount = (std::min)(kVisibleRows, itemCount - firstIndex);
    data.rows.reserve(static_cast<size_t>((std::max)(0, visibleCount)));
    for (int drawIndex = 0; drawIndex < visibleCount; ++drawIndex) {
        const ITEM_INFO* item = items[firstIndex + drawIndex];
        if (!item) {
            continue;
        }

        const RECT rowRect = GetRowRect(drawIndex);
        DisplayRow row{};
        row.itemIndex = item->m_itemIndex;
        row.itemId = item->GetItemId();
        row.identified = item->m_isIdentified != 0;
        row.x = rowRect.left;
        row.y = rowRect.top;
        row.width = rowRect.right - rowRect.left;
        row.height = rowRect.bottom - rowRect.top;
        row.hovered = m_hoveredItemIndex == static_cast<int>(item->m_itemIndex);
        row.selected = m_selectedItemIndex == static_cast<int>(item->m_itemIndex);
        row.count = item->m_num;
        row.label = item->GetDisplayName();
        data.rows.push_back(std::move(row));
    }

    data.scrollBar.visible = IsScrollBarVisible(itemCount);
    if (data.scrollBar.visible) {
        const RECT trackRect = GetScrollTrackRect();
        const RECT thumbRect = GetScrollThumbRect(itemCount);
        data.scrollBar.trackX = trackRect.left;
        data.scrollBar.trackY = trackRect.top;
        data.scrollBar.trackWidth = trackRect.right - trackRect.left;
        data.scrollBar.trackHeight = trackRect.bottom - trackRect.top;
        data.scrollBar.thumbX = thumbRect.left;
        data.scrollBar.thumbY = thumbRect.top;
        data.scrollBar.thumbWidth = thumbRect.right - thumbRect.left;
        data.scrollBar.thumbHeight = thumbRect.bottom - thumbRect.top;
    }

    *outData = std::move(data);
    return true;
}