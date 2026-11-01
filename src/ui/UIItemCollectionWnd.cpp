#include "UiMetrics.h"
#include "UIItemCollectionWnd.h"

#include "UIWindowMgr.h"
#include "item/Item.h"

#include <algorithm>

namespace {
constexpr int kDefaultPreviewWidth = 300;
constexpr int kDefaultPreviewHeight = 400;
constexpr int kTitleBarHeight = uiMetrics::titleHeight;
constexpr int kCloseButtonSize = 12;
constexpr int kPadding = 8;
constexpr int kWindowWidth = kDefaultPreviewWidth + kPadding * 2;
constexpr int kWindowHeight = kDefaultPreviewHeight + kTitleBarHeight + kPadding * 2;

bool IsInsideRect(const RECT& rect, int x, int y)
{
    return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
}
}

UIItemCollectionWnd::UIItemCollectionWnd()
    : m_item(),
      m_hasItem(false),
      m_closeHovered(false),
      m_closePressed(false),
      m_hasStoredPlacement(false),
    m_mainUsesIllust(false),
      m_mainUsesCollection(false),
    m_mainBitmap()
{
    Create(kWindowWidth, kWindowHeight);
    Move(40, 40);
    int savedX = m_x;
    int savedY = m_y;
    if (LoadUiWindowPlacement("ItemCollectionWnd", &savedX, &savedY)) {
        g_windowMgr.ClampWindowToClient(&savedX, &savedY, m_w, m_h);
        Move(savedX, savedY);
        m_hasStoredPlacement = true;
    }
}

void UIItemCollectionWnd::OnDraw()
{
    if (m_show == 0 || !m_hasItem) {
        return;
    }

    HDC hdc = AcquireDrawTarget();
    if (!hdc) {
        return;
    }

    const RECT bounds = shopui::MakeRect(m_x, m_y, m_w, m_h);
    shopui::DrawFrameWindow(hdc, bounds, "Card illustration");

    const RECT closeRect = GetCloseButtonRect();
    shopui::FillRectColor(hdc, closeRect, m_closePressed ? RGB(189, 199, 222) : (m_closeHovered ? RGB(215, 223, 240) : RGB(231, 226, 214)));
    shopui::FrameRectColor(hdc, closeRect, RGB(127, 122, 112));
    shopui::DrawWindowTextRect(hdc, closeRect, "X", RGB(0, 0, 0), DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    const RECT previewRect = shopui::MakeRect(m_x + kPadding, m_y + kTitleBarHeight + kPadding, m_w - (kPadding * 2), m_h - kTitleBarHeight - (kPadding * 2));
    shopui::FillRectColor(hdc, previewRect, RGB(245, 242, 234));
    shopui::FrameRectColor(hdc, previewRect, RGB(166, 159, 145));
    if (m_mainBitmap.IsValid()) {
        shopui::DrawBitmapPixelsTransparent(hdc, m_mainBitmap, previewRect);
    }

    ReleaseDrawTarget(hdc);
    m_isDirty = 0;
}

void UIItemCollectionWnd::OnLBtnDown(int x, int y)
{
    m_closePressed = false;
    if (IsInsideRect(GetCloseButtonRect(), x, y)) {
        m_closePressed = true;
        Invalidate();
        return;
    }
    UIFrameWnd::OnLBtnDown(x, y);
}

void UIItemCollectionWnd::OnLBtnUp(int x, int y)
{
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

void UIItemCollectionWnd::OnMouseMove(int x, int y)
{
    UIFrameWnd::OnMouseMove(x, y);
    UpdateInteraction(x, y);
}

void UIItemCollectionWnd::OnMouseHover(int x, int y)
{
    UpdateInteraction(x, y);
}

void UIItemCollectionWnd::StoreInfo()
{
    SaveUiWindowPlacement("ItemCollectionWnd", m_x, m_y);
    m_hasStoredPlacement = true;
}

void UIItemCollectionWnd::SetItemInfo(const ITEM_INFO& item, int preferredX, int preferredY)
{
    m_item = item;
    m_hasItem = item.GetItemId() != 0;
    RefreshBitmaps();
    const int previewWidth = m_mainBitmap.IsValid() ? m_mainBitmap.width : kDefaultPreviewWidth;
    const int previewHeight = m_mainBitmap.IsValid() ? m_mainBitmap.height : kDefaultPreviewHeight;
    Resize(previewWidth + kPadding * 2, previewHeight + kTitleBarHeight + kPadding * 2);

    int nextX = m_x;
    int nextY = m_y;
    if (m_show == 0 && !m_hasStoredPlacement) {
        nextX = preferredX;
        nextY = preferredY;
    }
    g_windowMgr.ClampWindowToClient(&nextX, &nextY, m_w, m_h);
    Move(nextX, nextY);

    SetShow(m_hasItem && m_mainBitmap.IsValid() ? 1 : 0);
    Invalidate();
}

bool UIItemCollectionWnd::GetDisplayDataForQt(DisplayData* outData) const
{
    if (!outData || m_show == 0 || !m_hasItem) {
        return false;
    }

    outData->title = "Card illustration";
    outData->itemId = m_item.GetItemId();
    outData->mainUsesIllust = m_mainUsesIllust;
    outData->mainUsesCollection = m_mainUsesCollection;
    const RECT closeRect = GetCloseButtonRect();
    outData->closeButton = { closeRect.left, closeRect.top, kCloseButtonSize, kCloseButtonSize, m_closeHovered, m_closePressed, "X" };
    return true;
}

bool UIItemCollectionWnd::HasItem() const
{
    return m_hasItem;
}

RECT UIItemCollectionWnd::GetCloseButtonRect() const
{
    return shopui::MakeRect(m_x + m_w - 16, m_y + 2, kCloseButtonSize, kCloseButtonSize);
}

void UIItemCollectionWnd::UpdateInteraction(int x, int y)
{
    const bool nextHover = IsInsideRect(GetCloseButtonRect(), x, y);
    if (m_closeHovered == nextHover) {
        return;
    }
    m_closeHovered = nextHover;
    Invalidate();
}

void UIItemCollectionWnd::RefreshBitmaps()
{
    m_mainBitmap.Clear();
    m_mainUsesIllust = false;
    m_mainUsesCollection = false;
    if (!m_hasItem) {
        return;
    }

    m_mainUsesIllust = shopui::TryLoadCardIllustPixels(m_item, &m_mainBitmap);
    if (!m_mainUsesIllust) {
        m_mainUsesCollection = shopui::TryLoadItemCollectionPixels(m_item, &m_mainBitmap);
    }
}