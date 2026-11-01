#include "UiMetrics.h"
#include "UIItemInfoWnd.h"

#include "NpcDialogColoredText.h"
#include "UIWindowMgr.h"
#include "item/Item.h"
#include "main/WinMain.h"

#include <algorithm>
#include <cctype>

namespace {
constexpr int kWindowWidth = 332;
constexpr int kWindowHeight = 308;
constexpr int kWindowMinHeight = 129;
constexpr int kTitleBarHeight = uiMetrics::titleHeight;
constexpr int kCloseButtonSize = 12;
constexpr int kPadding = 8;
constexpr int kPreviewBoxSize = 96;
constexpr int kGraphicsButtonWidth = 36;
constexpr int kGraphicsButtonHeight = 16;
constexpr int kSlotSize = 22;
constexpr int kSlotGap = 6;
constexpr int kNameHeight = 18;
constexpr int kDetailLineAdvance = 13;
constexpr int kDescriptionTopGap = 4;
constexpr int kDescriptionBottomPadding = 20;
constexpr int kSlotDescriptionGap = 24;
constexpr char kUnknownItemDescription[] = "Unknown item, can be identified by using a ^0000FFMagnifier^000000.";

std::string BuildMeasurementText(const std::string& text);

int MeasureWrappedTextHeight(HFONT font, int width, const std::string& text)
{
    if (width <= 0 || text.empty()) {
        return 0;
    }

    const std::string measureText = BuildMeasurementText(text);
    if (measureText.empty()) {
        return 0;
    }

    HDC hdc = AcquireMainWindowDrawTarget();
    if (!hdc) {
        return 0;
    }

    RECT measureRect{ 0, 0, width, 0 };
    HGDIOBJ oldFont = SelectObject(hdc, font);
    DrawTextA(hdc,
              measureText.c_str(),
              static_cast<int>(measureText.size()),
              &measureRect,
              DT_LEFT | DT_WORDBREAK | DT_EDITCONTROL | DT_CALCRECT | DT_NOPREFIX);
    SelectObject(hdc, oldFont);
    ReleaseMainWindowDrawTarget(hdc);

    const int measuredHeight = static_cast<int>(measureRect.bottom - measureRect.top);
    return (std::max)(0, measuredHeight);
}

int GetMaxItemInfoWindowHeight()
{
    RECT clientRect{};
    if (!GetClientRect(g_hMainWnd, &clientRect)) {
        return kWindowHeight;
    }

    const int clientHeight = clientRect.bottom - clientRect.top;
    return (std::max)(kWindowMinHeight, clientHeight - kPadding);
}

HFONT GetItemInfoDescriptionFont()
{
    static HFONT s_font = CreateFontA(
        -12,
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
    return s_font;
}

bool IsInsideRect(const RECT& rect, int x, int y)
{
    return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
}

bool IsEquipInfoType(int itemType)
{
    switch (itemType) {
    case 4:
    case 5:
    case 8:
    case 9:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
        return true;
    default:
        return false;
    }
}

bool IsHexDigit(char ch)
{
    return std::isxdigit(static_cast<unsigned char>(ch)) != 0;
}

std::string SanitizeRoText(const std::string& text)
{
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        const char ch = text[i];
        if (ch == '^' && i + 6 < text.size()) {
            bool isColorCode = true;
            for (size_t j = 1; j <= 6; ++j) {
                if (!IsHexDigit(text[i + j])) {
                    isColorCode = false;
                    break;
                }
            }
            if (isColorCode) {
                i += 6;
                continue;
            }
        }
        if (ch == '\r') {
            continue;
        }
        out.push_back(ch == '_' ? ' ' : ch);
    }
    return out;
}

std::string BuildMeasurementText(const std::string& text)
{
    const std::string sanitized = SanitizeRoText(text);
    std::string out;
    out.reserve(sanitized.size() + 8);
    for (char ch : sanitized) {
        if (ch == '\r') {
            continue;
        }
        if (ch == '\n') {
            out.push_back('\r');
            out.push_back('\n');
            continue;
        }
        out.push_back(ch);
    }
    return out;
}

std::string NormalizeDescriptionText(const std::string& text)
{
    std::string out;
    out.reserve(text.size());
    for (char ch : text) {
        if (ch == '\r') {
            continue;
        }
        out.push_back(ch);
    }
    return out;
}

void DrawSlotTooltip(HDC hdc, const RECT& bounds, const RECT& anchorRect, const std::string& text)
{
    if (!hdc || text.empty()) {
        return;
    }

    SIZE textSize{};
    HGDIOBJ oldFont = SelectObject(hdc, shopui::GetUiFont());
    GetTextExtentPoint32A(hdc, text.c_str(), static_cast<int>(text.size()), &textSize);
    SelectObject(hdc, oldFont);

    const int paddingX = 6;
    const int paddingY = 4;
    const int tooltipWidth = textSize.cx + paddingX * 2;
    const int tooltipHeight = textSize.cy + paddingY * 2;
    int left = anchorRect.left + ((anchorRect.right - anchorRect.left) - tooltipWidth) / 2;
    int top = anchorRect.top - tooltipHeight - 2;
    const int minLeft = static_cast<int>(bounds.left) + 2;
    const int maxLeft = static_cast<int>(bounds.right) - tooltipWidth - 2;
    const int minTop = static_cast<int>(bounds.top) + kTitleBarHeight + 2;
    left = (std::max)(minLeft, left);
    left = (std::min)(maxLeft, left);
    top = (std::max)(minTop, top);

    RECT tooltipRect{ left, top, left + tooltipWidth, top + tooltipHeight };
    shopui::FillRectColor(hdc, tooltipRect, RGB(48, 48, 48));
    shopui::FrameRectColor(hdc, tooltipRect, RGB(96, 96, 96));
    shopui::DrawWindowTextRect(hdc,
                               shopui::MakeRect(tooltipRect.left + paddingX,
                                                tooltipRect.top + paddingY,
                                                tooltipWidth - paddingX * 2,
                                                tooltipHeight - paddingY * 2),
                               text,
                               RGB(255, 255, 255),
                               DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

ITEM_INFO BuildSlottedCardInfo(unsigned int itemId)
{
    ITEM_INFO item{};
    item.SetItemId(itemId);
    item.m_num = 1;
    item.m_isIdentified = 1;
    return item;
}
}

UIItemInfoWnd::UIItemInfoWnd()
    : m_item(),
      m_hasItem(false),
      m_closeHovered(false),
      m_closePressed(false),
      m_graphicsHovered(false),
      m_graphicsPressed(false),
    m_hasStoredPlacement(false),
    m_previewUsesCollection(false),
    m_hoveredSlotIndex(-1),
    m_previewBitmap(),
    m_slotItemIds{ 0u, 0u, 0u, 0u },
    m_slotBitmaps()
{
    Create(kWindowWidth, kWindowHeight);
    Move(24, 24);
    int savedX = m_x;
    int savedY = m_y;
    if (LoadUiWindowPlacement("ItemInfoWnd", &savedX, &savedY)) {
        g_windowMgr.ClampWindowToClient(&savedX, &savedY, m_w, m_h);
        Move(savedX, savedY);
        m_hasStoredPlacement = true;
    }
}

void UIItemInfoWnd::OnDraw()
{
    if (m_show == 0 || !m_hasItem) {
        return;
    }

    HDC hdc = AcquireDrawTarget();
    if (!hdc) {
        return;
    }

    const RECT bounds = shopui::MakeRect(m_x, m_y, m_w, m_h);
    shopui::DrawFrameWindow(hdc, bounds, BuildTitle().c_str());

    const RECT closeRect = GetCloseButtonRect();
    shopui::FillRectColor(hdc, closeRect, m_closePressed ? RGB(189, 199, 222) : (m_closeHovered ? RGB(215, 223, 240) : RGB(231, 226, 214)));
    shopui::FrameRectColor(hdc, closeRect, RGB(127, 122, 112));
    shopui::DrawWindowTextRect(hdc, closeRect, "X", RGB(0, 0, 0), DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    const RECT previewRect = GetPreviewRect();
    shopui::FillRectColor(hdc, previewRect, RGB(245, 242, 234));
    shopui::FrameRectColor(hdc, previewRect, RGB(166, 159, 145));
    if (m_previewBitmap.IsValid()) {
        const RECT imageRect = shopui::MakeRect(previewRect.left + 2, previewRect.top + 2, kPreviewBoxSize - 4, kPreviewBoxSize - 4);
        shopui::DrawBitmapPixelsTransparent(hdc, m_previewBitmap, imageRect);
    }

    if (HasViewButton()) {
        const RECT graphicsRect = GetGraphicsButtonRect();
        shopui::FillRectColor(hdc, graphicsRect, m_graphicsPressed ? RGB(185, 199, 222) : (m_graphicsHovered ? RGB(215, 223, 240) : RGB(233, 228, 216)));
        shopui::FrameRectColor(hdc, graphicsRect, RGB(140, 133, 120));
        shopui::DrawWindowTextRect(hdc, graphicsRect, "View", RGB(0, 0, 0), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    const RECT nameRect = GetNameRect();
    shopui::DrawWindowTextRect(hdc, nameRect, BuildDisplayName(), RGB(0, 0, 0), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    int detailY = nameRect.bottom + 2;
    for (const std::string& line : BuildDetailLines()) {
        RECT detailRect{ nameRect.left, detailY, m_x + m_w - kPadding, detailY + 14 };
        shopui::DrawWindowTextRect(hdc, detailRect, line, RGB(64, 64, 64), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        detailY += 13;
    }

    const RECT descriptionRect = GetDescriptionRect();
    HGDIOBJ oldDescriptionFont = SelectObject(hdc, GetItemInfoDescriptionFont());
    DrawNpcSayDialogColoredText(hdc, descriptionRect, BuildDescriptionText());
    SelectObject(hdc, oldDescriptionFont);

    if (ShouldShowSlots()) {
        const int declaredSlots = GetDeclaredSlotCount();
        for (int slotIndex = 0; slotIndex < 4; ++slotIndex) {
            const RECT slotRect = GetSlotRect(slotIndex);
            const bool available = slotIndex < declaredSlots;
            const bool occupied = m_slotItemIds[static_cast<size_t>(slotIndex)] != 0;
            const COLORREF fillColor = !available
                ? RGB(166, 166, 166)
                : (occupied ? RGB(245, 242, 234) : RGB(223, 223, 223));
            shopui::FillRectColor(hdc, slotRect, fillColor);
            shopui::FrameRectColor(hdc, slotRect, occupied ? RGB(134, 129, 118) : RGB(126, 126, 126));
            if (occupied && m_slotBitmaps[static_cast<size_t>(slotIndex)].IsValid()) {
                const RECT iconRect = shopui::MakeRect(slotRect.left + 2, slotRect.top + 2, kSlotSize - 4, kSlotSize - 4);
                shopui::DrawBitmapPixelsTransparent(hdc, m_slotBitmaps[static_cast<size_t>(slotIndex)], iconRect);
            }
        }

        if (m_hoveredSlotIndex >= 0 && m_hoveredSlotIndex < 4 && m_slotItemIds[static_cast<size_t>(m_hoveredSlotIndex)] != 0) {
            const ITEM_INFO slotItem = BuildSlottedCardInfo(m_slotItemIds[static_cast<size_t>(m_hoveredSlotIndex)]);
            DrawSlotTooltip(hdc, bounds, GetSlotRect(m_hoveredSlotIndex), SanitizeRoText(shopui::BuildItemHoverNameText(slotItem)));
        }
    }

    ReleaseDrawTarget(hdc);
    m_isDirty = 0;
}

void UIItemInfoWnd::OnLBtnDown(int x, int y)
{
    m_closePressed = false;
    m_graphicsPressed = false;

    if (IsInsideRect(GetCloseButtonRect(), x, y)) {
        m_closePressed = true;
        Invalidate();
        return;
    }
    if (HasViewButton() && IsInsideRect(GetGraphicsButtonRect(), x, y)) {
        m_graphicsPressed = true;
        Invalidate();
        return;
    }

    UIFrameWnd::OnLBtnDown(x, y);
}

void UIItemInfoWnd::OnLBtnUp(int x, int y)
{
    const bool wasDragging = m_isDragging != 0;
    UIFrameWnd::OnLBtnUp(x, y);
    if (wasDragging) {
        return;
    }

    const bool activateClose = m_closePressed && IsInsideRect(GetCloseButtonRect(), x, y);
    const bool activateGraphics = HasViewButton() && m_graphicsPressed && IsInsideRect(GetGraphicsButtonRect(), x, y);
    m_closePressed = false;
    m_graphicsPressed = false;
    UpdateInteraction(x, y);

    if (activateClose) {
        SetShow(0);
        return;
    }
    if (activateGraphics && m_hasItem) {
        g_windowMgr.ShowItemCollectionWindow(m_item, m_x + m_w + 8, m_y + 24);
    }
}

void UIItemInfoWnd::OnRBtnDown(int x, int y)
{
    if (m_show == 0 || !ShouldShowSlots()) {
        return;
    }

    UpdateInteraction(x, y);
    if (m_hoveredSlotIndex < 0 || m_hoveredSlotIndex >= 4) {
        return;
    }

    const unsigned int itemId = m_slotItemIds[static_cast<size_t>(m_hoveredSlotIndex)];
    if (itemId == 0) {
        return;
    }

    const ITEM_INFO slotItem = BuildSlottedCardInfo(itemId);
    g_windowMgr.ShowItemInfoWindow(slotItem, x + 12, y + 12);
}

void UIItemInfoWnd::OnMouseMove(int x, int y)
{
    UIFrameWnd::OnMouseMove(x, y);
    UpdateInteraction(x, y);
}

void UIItemInfoWnd::OnMouseHover(int x, int y)
{
    UpdateInteraction(x, y);
}

void UIItemInfoWnd::StoreInfo()
{
    SaveUiWindowPlacement("ItemInfoWnd", m_x, m_y);
    m_hasStoredPlacement = true;
}

void UIItemInfoWnd::SetItemInfo(const ITEM_INFO& item, int preferredX, int preferredY)
{
    m_item = item;
    m_hasItem = item.GetItemId() != 0;
    RefreshPreviewAndSlots();

    Resize(kWindowWidth, m_hasItem ? GetDesiredWindowHeight() : kWindowHeight);

    int nextX = m_x;
    int nextY = m_y;
    if (m_show == 0 && !m_hasStoredPlacement) {
        nextX = preferredX;
        nextY = preferredY;
    }
    g_windowMgr.ClampWindowToClient(&nextX, &nextY, m_w, m_h);
    Move(nextX, nextY);

    SetShow(m_hasItem ? 1 : 0);
    Invalidate();
}

bool UIItemInfoWnd::GetDisplayDataForQt(DisplayData* outData) const
{
    if (!outData || m_show == 0 || !m_hasItem) {
        return false;
    }

    outData->title = BuildTitle();
    outData->itemId = m_item.GetItemId();
    outData->identified = m_item.m_isIdentified != 0;
    outData->name = BuildDisplayName();
    outData->previewUsesCollection = m_previewUsesCollection;
    outData->detailLines = BuildDetailLines();
    outData->description = BuildDescriptionText();
    outData->closeButton = { GetCloseButtonRect().left, GetCloseButtonRect().top, kCloseButtonSize, kCloseButtonSize, m_closeHovered, m_closePressed, true, true, "X" };
    outData->graphicsButton = { GetGraphicsButtonRect().left, GetGraphicsButtonRect().top, kGraphicsButtonWidth, kGraphicsButtonHeight, m_graphicsHovered, m_graphicsPressed, true, HasViewButton(), "View" };
    if (ShouldShowSlots()) {
        outData->slotEntries.reserve(4);
        const int declaredSlots = GetDeclaredSlotCount();
        for (int slotIndex = 0; slotIndex < 4; ++slotIndex) {
            const RECT slotRect = GetSlotRect(slotIndex);
            DisplaySlot slot;
            slot.x = slotRect.left;
            slot.y = slotRect.top;
            slot.width = kSlotSize;
            slot.height = kSlotSize;
            slot.available = slotIndex < declaredSlots;
            slot.itemId = m_slotItemIds[static_cast<size_t>(slotIndex)];
            slot.occupied = slot.itemId != 0;
            slot.hovered = slotIndex == m_hoveredSlotIndex;
            if (slot.occupied) {
                const ITEM_INFO hoverItem = BuildSlottedCardInfo(slot.itemId);
                slot.tooltip = SanitizeRoText(shopui::BuildItemHoverNameText(hoverItem));
            }
            outData->slotEntries.push_back(std::move(slot));
        }
    }
    return true;
}

bool UIItemInfoWnd::HasItem() const
{
    return m_hasItem;
}

RECT UIItemInfoWnd::GetCloseButtonRect() const
{
    return shopui::MakeRect(m_x + m_w - 16, m_y + 2, kCloseButtonSize, kCloseButtonSize);
}

RECT UIItemInfoWnd::GetGraphicsButtonRect() const
{
    const RECT previewRect = GetPreviewRect();
    return shopui::MakeRect(previewRect.left + 4, previewRect.top + 4, kGraphicsButtonWidth, kGraphicsButtonHeight);
}

RECT UIItemInfoWnd::GetPreviewRect() const
{
    return shopui::MakeRect(m_x + kPadding, m_y + kTitleBarHeight + kPadding, kPreviewBoxSize, kPreviewBoxSize);
}

RECT UIItemInfoWnd::GetNameRect() const
{
    const RECT previewRect = GetPreviewRect();
    return RECT{ previewRect.right + 8, previewRect.top, m_x + m_w - kPadding, previewRect.top + kNameHeight };
}

RECT UIItemInfoWnd::GetDescriptionRect() const
{
    const RECT nameRect = GetNameRect();
    const int detailY = nameRect.bottom + 2 + static_cast<int>(BuildDetailLines().size()) * kDetailLineAdvance;
    const int bottomPadding = ShouldShowSlots() ? (kSlotSize + kPadding + kSlotDescriptionGap) : kDescriptionBottomPadding;
    return RECT{ nameRect.left, detailY + kDescriptionTopGap, m_x + m_w - kPadding, m_y + m_h - bottomPadding };
}

RECT UIItemInfoWnd::GetSlotRect(int slotIndex) const
{
    const int left = m_x + kPadding + slotIndex * (kSlotSize + kSlotGap);
    const int top = m_y + m_h - kPadding - kSlotSize;
    return shopui::MakeRect(left, top, kSlotSize, kSlotSize);
}

void UIItemInfoWnd::UpdateInteraction(int x, int y)
{
    const bool nextCloseHovered = IsInsideRect(GetCloseButtonRect(), x, y);
    const bool nextGraphicsHovered = HasViewButton() && IsInsideRect(GetGraphicsButtonRect(), x, y);
    int nextHoveredSlot = -1;
    if (ShouldShowSlots()) {
        for (int slotIndex = 0; slotIndex < 4; ++slotIndex) {
            if (m_slotItemIds[static_cast<size_t>(slotIndex)] == 0) {
                continue;
            }
            if (IsInsideRect(GetSlotRect(slotIndex), x, y)) {
                nextHoveredSlot = slotIndex;
                break;
            }
        }
    }
    if (m_closeHovered == nextCloseHovered && m_graphicsHovered == nextGraphicsHovered && m_hoveredSlotIndex == nextHoveredSlot) {
        return;
    }
    m_closeHovered = nextCloseHovered;
    m_graphicsHovered = nextGraphicsHovered;
    m_hoveredSlotIndex = nextHoveredSlot;
    Invalidate();
}

void UIItemInfoWnd::RefreshPreviewAndSlots()
{
    m_previewBitmap.Clear();
    m_previewUsesCollection = false;
    m_hoveredSlotIndex = -1;
    m_slotItemIds = { 0u, 0u, 0u, 0u };
    for (shopui::BitmapPixels& bitmap : m_slotBitmaps) {
        bitmap.Clear();
    }

    if (!m_hasItem) {
        return;
    }

    m_previewUsesCollection = shopui::TryLoadItemCollectionPixels(m_item, &m_previewBitmap);
    if (!m_previewUsesCollection) {
        shopui::TryLoadItemIconPixels(m_item, &m_previewBitmap);
    }

    if (!ShouldShowSlots()) {
        return;
    }

    for (int slotIndex = 0; slotIndex < 4; ++slotIndex) {
        const unsigned int cardId = static_cast<unsigned int>(m_item.m_slot[slotIndex]);
        if (cardId == 0) {
            continue;
        }

        m_slotItemIds[static_cast<size_t>(slotIndex)] = cardId;
        ITEM_INFO slotItem{};
        slotItem.SetItemId(cardId);
        slotItem.m_isIdentified = 1;
        if (!shopui::TryLoadItemIconPixels(slotItem, &m_slotBitmaps[static_cast<size_t>(slotIndex)])) {
            shopui::TryLoadItemCollectionPixels(slotItem, &m_slotBitmaps[static_cast<size_t>(slotIndex)]);
        }
    }
}

bool UIItemInfoWnd::HasViewButton() const
{
    return m_hasItem
        && m_item.m_isIdentified != 0
        && g_ttemmgr.IsCardItem(m_item.GetItemId())
        && !m_item.GetCardIllustName().empty();
}

int UIItemInfoWnd::GetDeclaredSlotCount() const
{
    const int slotCount = m_item.GetSlotCount();
    return (std::max)(0, (std::min)(4, slotCount));
}

bool UIItemInfoWnd::ShouldShowSlots() const
{
    return m_hasItem && m_item.m_isIdentified != 0 && IsEquipInfoType(m_item.m_itemType);
}

std::string UIItemInfoWnd::BuildTitle() const
{
    return "Item Information";
}

std::string UIItemInfoWnd::BuildDisplayName() const
{
    if (m_item.m_isIdentified == 0) {
        return SanitizeRoText(m_item.GetDisplayName());
    }
    return SanitizeRoText(m_item.GetEquipDisplayName());
}

std::vector<std::string> UIItemInfoWnd::BuildDetailLines() const
{
    std::vector<std::string> out;
    if (m_item.m_isIdentified == 0) {
        return out;
    }
    if (m_item.m_num > 1 && !g_ttemmgr.IsCardItem(m_item.GetItemId())) {
        out.push_back("Quantity: " + std::to_string(m_item.m_num));
    }
    if (m_item.m_refiningLevel > 0) {
        out.push_back("Refine: +" + std::to_string(m_item.m_refiningLevel));
    }
    if (m_item.m_isDamaged != 0) {
        out.push_back("Condition: Damaged");
    } else if (m_item.m_isIdentified == 0) {
        out.push_back("Condition: Unidentified");
    }
    return out;
}

std::string UIItemInfoWnd::BuildDescriptionText() const
{
    if (m_item.m_isIdentified == 0) {
        return kUnknownItemDescription;
    }

    const std::string description = NormalizeDescriptionText(m_item.GetDescription());
    if (!description.empty()) {
        return description;
    }
    return std::string();
}

int UIItemInfoWnd::GetDesiredWindowHeight() const
{
    const int minHeight = ShouldShowSlots()
        ? kTitleBarHeight + kPadding + kPreviewBoxSize + kSlotDescriptionGap + kSlotSize + kPadding
        : kWindowMinHeight;

    const RECT nameRect = GetNameRect();
    const int descriptionTop = (nameRect.bottom - m_y) + 2 + static_cast<int>(BuildDetailLines().size()) * kDetailLineAdvance + kDescriptionTopGap;
    const int descriptionWidth = (m_x + m_w - kPadding) - nameRect.left;
    const int descriptionHeight = MeasureWrappedTextHeight(GetItemInfoDescriptionFont(), descriptionWidth, BuildDescriptionText());
    const int descriptionBottomPadding = ShouldShowSlots()
        ? (kSlotSize + kPadding + kSlotDescriptionGap)
        : kDescriptionBottomPadding;
    const int desiredHeight = descriptionTop + descriptionHeight + descriptionBottomPadding;

    return (std::max)(minHeight, (std::min)(GetMaxItemInfoWindowHeight(), desiredHeight));
}