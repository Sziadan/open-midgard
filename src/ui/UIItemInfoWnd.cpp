#include "UiMetrics.h"
#include "UIItemInfoWnd.h"

#include "NpcDialogColoredText.h"
#include "UIWindowMgr.h"
#include "item/Item.h"
#include "main/WinMain.h"
#include "session/Session.h"
#include "gamemode/Commerce.h"

#include <algorithm>
#include <cctype>
#include <tuple>

namespace {
constexpr int kWindowWidth = 280;
constexpr int kWindowHeight = 120;
constexpr int kTitleBarHeight = uiMetrics::titleHeight;
constexpr int kCloseButtonSize = 12;
constexpr int kPreviewBoxSize = 75;
constexpr int kGraphicsButtonWidth = 36;
constexpr int kGraphicsButtonHeight = 16;
constexpr int kSlotSize = 24;
constexpr int kSlotGap = 1;
constexpr char kUnknownItemDescription[] = "Unknown item, can be identified by using a ^0000FFMagnifier^000000.";

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

std::string WrapCardTooltip(const std::string& text)
{
    std::string out;
    int characters=0;
    for (const unsigned char ch:text) {
        if ((ch&0xc0)!=0x80) {
            if (characters==35) { out+='\n';characters=0; }
            ++characters;
        }
        out+=static_cast<char>(ch);
    }
    return out;
}

void DrawSlotTooltip(HDC hdc, const RECT& anchorRect, const std::string& text)
{
    if (!hdc || text.empty()) {
        return;
    }

    SIZE textSize{};
    HGDIOBJ oldFont = SelectObject(hdc, shopui::GetUiFont());
    RECT measured{0,0,600,0};
    DrawTextA(hdc,text.c_str(),static_cast<int>(text.size()),&measured,DT_CALCRECT|DT_NOPREFIX);
    textSize={measured.right,measured.bottom};
    SelectObject(hdc, oldFont);

    const int paddingX = 6;
    const int paddingY = 4;
    const int tooltipWidth = textSize.cx + paddingX * 2;
    const int tooltipHeight = textSize.cy + paddingY * 2;
    RECT viewport{};GetClipBox(hdc,&viewport);
    int left=(std::max)(int(viewport.left)-3,int(anchorRect.left));
    int top=(std::max)(int(viewport.top)-3,int(anchorRect.top));
    if (left+tooltipWidth>viewport.right+3) left=viewport.right-tooltipWidth+3;
    if (top+tooltipHeight>viewport.bottom+3) top=viewport.bottom-tooltipHeight+3;

    RECT tooltipRect{ left, top, left + tooltipWidth, top + tooltipHeight };
    shopui::FillRectColor(hdc, tooltipRect, RGB(48, 48, 48));
    shopui::FrameRectColor(hdc, tooltipRect, RGB(96, 96, 96));
    // The shared Qt-backed native label helper is deliberately single-line.
    // Draw each source balloon line separately instead of flattening newlines.
    const int lines=1+static_cast<int>(std::count(text.begin(),text.end(),'\n'));
    const int lineHeight=(std::max)(1,static_cast<int>(textSize.cy)/lines);
    size_t start=0;
    for(int line=0;line<lines;++line) {
        const size_t end=text.find('\n',start);
        shopui::DrawWindowTextRect(hdc,
            shopui::MakeRect(left+paddingX,top+paddingY+line*lineHeight,
                tooltipWidth-paddingX*2,lineHeight),
            text.substr(start,end==std::string::npos ? end : end-start),
            RGB(255,255,255),DT_LEFT|DT_TOP|DT_NOPREFIX|DT_SINGLELINE);
        if(end==std::string::npos)break;
        start=end+1;
    }
}

class ItemDescriptionInput final : public UIWindow {
public:
    explicit ItemDescriptionInput(UIItemInfoWnd& owner) : m_owner(owner) {}
    void OnWheel(int delta) override { m_owner.ScrollDescriptionWheel(delta); }
private:
    UIItemInfoWnd& m_owner;
};

ITEM_INFO BuildSlottedCardInfo(unsigned int itemId)
{
    ITEM_INFO item{};
    item.SetItemId(itemId);
    item.m_num = 1;
    item.m_itemType = 6;
    item.m_isIdentified = 1;
    return item;
}

bool StartsWithCardOrEmpty(const ITEM_INFO& item)
{
    return item.m_slot[0]==0 || g_ttemmgr.IsCardItem(static_cast<unsigned>(item.m_slot[0]));
}
}

UIItemInfoWnd::UIItemInfoWnd(ItemDetailKind kind)
    : m_kind(kind),
      m_item(),
      m_hasItem(false),
      m_closeHovered(false),
      m_closePressed(false),
      m_graphicsHovered(false),
      m_graphicsPressed(false),
    m_previewUsesCollection(false),
    m_hoveredSlotIndex(-1),
    m_previewBitmap(),
    m_slotItemIds{ 0u, 0u, 0u, 0u },
    m_slotBitmaps()
{
    Create(kWindowWidth, kWindowHeight);
    int x=0,y=0;
    g_windowMgr.RestoreItemDetailWindowPlacement(&x,&y,m_kind);
    Move(x,y);
    m_descriptionInput = new ItemDescriptionInput(*this);
    const RECT viewer = GetDescriptionRect();
    m_descriptionInput->Create(viewer.right-viewer.left,viewer.bottom-viewer.top);
    m_descriptionInput->Move(viewer.left,viewer.top);
    m_descriptionInput->SetShow(1);
    AddChild(m_descriptionInput);
}

void UIItemInfoWnd::OnDraw()
{
    if (!RefreshBoundItem() || !m_show) return;
    HDC hdc = AcquireDrawTarget();
    if (!hdc) return;
    DrawNativeToHdc(hdc);
    ReleaseDrawTarget(hdc);
}

void UIItemInfoWnd::DrawNativeToHdc(HDC hdc)
{
    RefreshBoundItem();
    if (m_show == 0 || !m_hasItem || !hdc) return;

    const RECT bounds = shopui::MakeRect(m_x, m_y, m_w, m_h);
    shopui::FillRectColor(hdc,bounds,RGB(245,242,234));
    shopui::FrameRectColor(hdc,bounds,RGB(166,159,145));

    const RECT closeRect = GetCloseButtonRect();
    shopui::FillRectColor(hdc, closeRect, m_closePressed ? RGB(189, 199, 222) : (m_closeHovered ? RGB(215, 223, 240) : RGB(231, 226, 214)));
    shopui::FrameRectColor(hdc, closeRect, RGB(127, 122, 112));
    shopui::DrawWindowTextRect(hdc, closeRect, "X", RGB(0, 0, 0), DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    const RECT previewRect = GetPreviewRect();
    shopui::FillRectColor(hdc, previewRect, RGB(245, 242, 234));
    shopui::FrameRectColor(hdc, previewRect, RGB(166, 159, 145));
    if (m_previewBitmap.IsValid()) {
        const RECT imageRect = shopui::MakeRect(previewRect.left + 2, previewRect.top + 2, previewRect.right-previewRect.left-4, previewRect.bottom-previewRect.top-4);
        shopui::DrawBitmapPixelsTransparent(hdc, m_previewBitmap, imageRect);
    }

    if (HasViewButton()) {
        const RECT graphicsRect = GetGraphicsButtonRect();
        shopui::FillRectColor(hdc, graphicsRect, m_graphicsPressed ? RGB(185, 199, 222) : (m_graphicsHovered ? RGB(215, 223, 240) : RGB(233, 228, 216)));
        shopui::FrameRectColor(hdc, graphicsRect, RGB(140, 133, 120));
        shopui::DrawWindowTextRect(hdc, graphicsRect, "View", RGB(0, 0, 0), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    const RECT nameRect = GetNameRect();
    shopui::DrawWindowTextRect(hdc, nameRect, BuildDisplayName(), m_item.m_isIdentified ? RGB(0,0,0) : RGB(128,128,128), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    const RECT descriptionRect = GetDescriptionRect();
    HGDIOBJ oldDescriptionFont = SelectObject(hdc, GetItemInfoDescriptionFont());
    DrawNpcSayDialogColoredText(hdc, descriptionRect, BuildViewerText(), m_descriptionScrollOffset);
    SelectObject(hdc, oldDescriptionFont);

    if (ShouldShowSlots()) {
        const int declaredSlots = GetDeclaredSlotCount();
        for (int slotIndex = 0; slotIndex < GetDeclaredSlotCount(); ++slotIndex) {
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

    }
    DrawHoverOverlay(hdc);

    m_isDirty = 0;
}

void UIItemInfoWnd::DrawHoverOverlay(HDC hdc) const
{
    if (m_show && m_hasItem && m_hoveredSlotIndex>=0 && m_hoveredSlotIndex<GetDeclaredSlotCount()) {
        const auto slotItem=BuildSlottedCardInfo(static_cast<unsigned>(m_item.m_slot[m_hoveredSlotIndex]));
        const RECT anchor=shopui::MakeRect(m_x+4+25*m_hoveredSlotIndex,m_y+m_h-50,1,1);
        DrawSlotTooltip(hdc,anchor,WrapCardTooltip(SanitizeRoText(shopui::BuildItemHoverNameText(slotItem))));
    }

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

    if (x > m_x+60 && y < m_y+kTitleBarHeight) UIFrameWnd::OnLBtnDown(x, y);
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
        g_windowMgr.DeleteWindow(this);
        return;
    }
    if (activateGraphics && m_hasItem) {
        g_windowMgr.ShowItemCollectionWindow(m_item, m_x + m_w + 8, m_y + 24);
    }
}

void UIItemInfoWnd::OnRBtnDown(int x, int y)
{
    UpdateInteraction(x, y);
    if (m_hoveredSlotIndex<0) return;
    const unsigned int itemId=static_cast<unsigned>(m_item.m_slot[m_hoveredSlotIndex]);
    const ITEM_INFO slotItem = BuildSlottedCardInfo(itemId);
    g_windowMgr.ShowCardInfoWindow(slotItem);
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
    g_windowMgr.StoreItemDetailWindowInfo({m_x,m_y,m_w,m_h},m_kind);
}

void UIItemInfoWnd::OnProcess()
{
    RefreshBoundItem();
    UIFrameWnd::OnProcess();
}

bool UIItemInfoWnd::RefreshBoundItem()
{
    if (m_source == ItemInfoSource::Snapshot || !m_show || !m_hasItem) return m_hasItem;
    const ITEM_INFO* current = nullptr;
    if (m_source == ItemInfoSource::Inventory) current = g_session.GetInventoryItemByIndex(m_item.m_itemIndex);
    else if (m_source == ItemInfoSource::Storage && g_session.IsStorageOpen()) current = g_session.GetStorageItemByIndex(m_item.m_itemIndex);
    else if (m_source == ItemInfoSource::Cart) {
        const auto found = ro::commerce::state.cartItems.find(m_item.m_itemIndex);
        if (found != ro::commerce::state.cartItems.end()) current = &found->second;
    }
    if (!current || current->GetItemId() != m_item.GetItemId()) {
        m_item = ITEM_INFO{};m_hasItem = false;m_source = ItemInfoSource::Snapshot;
        SetShow(0);Invalidate();return false;
    }
    const auto fields=[](const ITEM_INFO& i){return std::tie(i.m_itemType,i.m_location,i.m_itemIndex,i.m_wearLocation,
        i.m_num,i.m_price,i.m_realPrice,i.m_itemName,i.m_isIdentified,i.m_isDamaged,i.m_refiningLevel,i.m_isYours,i.m_deleteTime);};
    if (fields(*current) != fields(m_item) || !std::equal(std::begin(current->m_slot),std::end(current->m_slot),std::begin(m_item.m_slot))) {
        m_item = *current;ResetDescriptionScroll();RefreshPreviewAndSlots();
        Resize(m_w,GetDesiredWindowHeight());Invalidate();
    }
    return true;
}

bool UIItemInfoWnd::MatchesSelection(const ITEM_INFO& item) const
{
    // Ref compares the complete raw name field, not its parsed numeric ID.
    return m_hasItem && m_item.m_itemName == item.m_itemName
        && m_item.m_refiningLevel == item.m_refiningLevel
        && std::equal(std::begin(m_item.m_slot), std::end(m_item.m_slot), std::begin(item.m_slot));
}

void UIItemInfoWnd::SetItemInfo(const ITEM_INFO& item, int preferredX, int preferredY, ItemInfoSource source)
{
    (void)preferredX; (void)preferredY;
    m_item = item;
    m_source = source;
    m_hasItem = item.GetItemId() != 0;
    ResetDescriptionScroll();
    RefreshPreviewAndSlots();
    Resize(m_w,m_hasItem ? GetDesiredWindowHeight() : kWindowHeight);
    SetShow(m_hasItem ? 1 : 0);
    Invalidate();
}

bool UIItemInfoWnd::GetDisplayDataForQt(DisplayData* outData) const
{
    if (!outData || m_show == 0 || !m_hasItem) {
        return false;
    }
    *outData = DisplayData{};

    outData->title = BuildTitle();
    outData->itemId = m_item.GetItemId();
    outData->identified = m_item.m_isIdentified != 0;
    outData->name = BuildDisplayName();
    outData->previewUsesCollection = m_previewUsesCollection;
    outData->detailLines = BuildDetailLines();
    outData->description = BuildDescriptionText();
    outData->previewRect = GetPreviewRect();
    outData->nameRect = GetNameRect();
    outData->descriptionRect = GetDescriptionRect();
    outData->descriptionGeneration = m_descriptionGeneration;
    if (m_hoveredSlotIndex>=0 && m_hoveredSlotIndex<GetDeclaredSlotCount()) {
        const auto item=BuildSlottedCardInfo(static_cast<unsigned>(m_item.m_slot[m_hoveredSlotIndex]));
        outData->slotTooltip=WrapCardTooltip(SanitizeRoText(shopui::BuildItemHoverNameText(item)));
        outData->slotTooltipAnchor=shopui::MakeRect(m_x+4+25*m_hoveredSlotIndex,m_y+m_h-50,1,1);
    }
    outData->closeButton = { GetCloseButtonRect().left, GetCloseButtonRect().top, kCloseButtonSize, kCloseButtonSize, m_closeHovered, m_closePressed, true, true, "X" };
    outData->graphicsButton = { GetGraphicsButtonRect().left, GetGraphicsButtonRect().top, kGraphicsButtonWidth, kGraphicsButtonHeight, m_graphicsHovered, m_graphicsPressed, true, HasViewButton(), "View" };
    if (ShouldShowSlots()) {
        outData->slotEntries.reserve(GetDeclaredSlotCount());
        const int declaredSlots = GetDeclaredSlotCount();
        for (int slotIndex = 0; slotIndex < GetDeclaredSlotCount(); ++slotIndex) {
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
    return shopui::MakeRect(m_x+6,m_y+6,kGraphicsButtonWidth,kGraphicsButtonHeight);
}

RECT UIItemInfoWnd::GetPreviewRect() const
{
    return shopui::MakeRect(m_x+10,m_y+11,kPreviewBoxSize,100);
}

RECT UIItemInfoWnd::GetNameRect() const
{
    return RECT{m_x+90,m_y+10,m_x+m_w-18,m_y+27};
}

RECT UIItemInfoWnd::GetDescriptionRect() const
{
    // Ref creates this child at280x120 and does not resize it when adding the
    // socket row. Keep its182x80 rectangle independent of outer height.
    return shopui::MakeRect(m_x+91,m_y+31,182,80);
}

RECT UIItemInfoWnd::GetSlotRect(int slotIndex) const
{
    return shopui::MakeRect(m_x+4+slotIndex*(kSlotSize+kSlotGap),m_y+m_h-26,kSlotSize,kSlotSize);
}

void UIItemInfoWnd::UpdateInteraction(int x, int y)
{
    const bool nextCloseHovered = IsInsideRect(GetCloseButtonRect(), x, y);
    const bool nextGraphicsHovered = HasViewButton() && IsInsideRect(GetGraphicsButtonRect(), x, y);
    const int nextHoveredSlot=GetCardSlotAtPoint(x,y);
    if (m_closeHovered == nextCloseHovered && m_graphicsHovered == nextGraphicsHovered && m_hoveredSlotIndex == nextHoveredSlot) {
        return;
    }
    m_closeHovered = nextCloseHovered;
    m_graphicsHovered = nextGraphicsHovered;
    m_hoveredSlotIndex = nextHoveredSlot;
    Invalidate();
}

int UIItemInfoWnd::GetCardSlotAtPoint(int x,int y) const
{
    if (!m_show || !m_hasItem || x<m_x || y<m_y
        || int64_t(x)>=int64_t(m_x)+m_w || int64_t(y)>=int64_t(m_y)+m_h) return -1;
    const int localX=x-m_x-3;
    if (y-m_y<=m_h-26) return -1;
    for (int index=0;index<GetDeclaredSlotCount();++index) {
        if (!m_item.m_slot[index]) break;
        if (25*index<localX && localX<25*index+25) return index;
    }
    return -1;
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

    if (!StartsWithCardOrEmpty(m_item)) {
        return;
    }

    for (int slotIndex = 0; slotIndex < GetDeclaredSlotCount(); ++slotIndex) {
        const unsigned int cardId = static_cast<unsigned int>(m_item.m_slot[slotIndex]);
        if (cardId == 0) {
            continue;
        }

        m_slotItemIds[static_cast<size_t>(slotIndex)] = cardId;
        const ITEM_INFO slotItem=BuildSlottedCardInfo(cardId);
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

bool UIItemInfoWnd::HasSlotRow() const
{
    // IsManufacturedItem is precisely marker255; marker254 is a different
    // naming path and must not be treated as the same predicate.
    return m_hasItem && m_item.m_isIdentified != 0 && GetDeclaredSlotCount() > 0 && m_item.m_slot[0] != 255;
}

bool UIItemInfoWnd::ShouldShowSlots() const
{
    // Painting observes current geometry, independently of the assignment-time
    // expansion predicate. Cache raw declared card words even while collapsed.
    return m_hasItem && m_h>120 && GetDeclaredSlotCount()>0 && StartsWithCardOrEmpty(m_item);
}

std::string UIItemInfoWnd::BuildTitle() const
{
    return "Item Information";
}

std::string UIItemInfoWnd::BuildDisplayName() const
{
    return SanitizeRoText(m_item.GetItemName());
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
    std::string text = m_item.GetExpiryDisplayText();
    const std::string description = m_item.m_isIdentified == 0 ? kUnknownItemDescription : NormalizeDescriptionText(m_item.GetDescription());
    if (!text.empty() && !description.empty()) text += "\n\n";
    return text + description;
}

int UIItemInfoWnd::GetDesiredWindowHeight() const
{
    // Preserve the observed144 guard versus149 target. No independent source
    // was found that justifies silently normalizing this decompiler oddity.
    return HasSlotRow() ? (m_h == 144 ? 144 : 149) : 120;
}

void UIItemInfoWnd::Move(int x,int y)
{
    UIFrameWnd::Move(x,y);
    if (m_descriptionInput) {
        const RECT rect=GetDescriptionRect();
        m_descriptionInput->Move(rect.left,rect.top);
    }
}

std::string UIItemInfoWnd::BuildViewerText() const
{
    std::string text;
    for (const auto& line:BuildDetailLines()) { text += line; text += '\n'; }
    return text+BuildDescriptionText();
}

void UIItemInfoWnd::ResetDescriptionScroll()
{
    m_descriptionScrollOffset=0;
    m_descriptionWheelRemainder=0;
    ++m_descriptionGeneration;
}

void UIItemInfoWnd::ScrollDescriptionWheel(int delta)
{
    HDC hdc=AcquireMainWindowDrawTarget();
    if (!hdc || !m_hasItem || !m_show) { if(hdc)ReleaseMainWindowDrawTarget(hdc); return; }
    const HGDIOBJ old=SelectObject(hdc,GetItemInfoDescriptionFont());
    const int height=MeasureNpcSayDialogColoredTextHeight(hdc,182,BuildViewerText());
    const int lineHeight=(std::max)(1,MeasureNpcSayDialogColoredTextHeight(hdc,182,"M"));
    SelectObject(hdc,old);ReleaseMainWindowDrawTarget(hdc);
    const int64_t total=int64_t(m_descriptionWheelRemainder)+delta;
    const int64_t notches=total/WHEEL_DELTA;
    m_descriptionWheelRemainder=static_cast<int>(total%WHEEL_DELTA);
    UINT lines=3;
    SystemParametersInfoA(SPI_GETWHEELSCROLLLINES,0,&lines,0);
    const int64_t step=lines==WHEEL_PAGESCROLL ? 80 : int64_t(lines)*lineHeight;
    const int maximum=(std::max)(0,height-80);
    m_descriptionScrollOffset=static_cast<int>((std::clamp)(int64_t(m_descriptionScrollOffset)-int64_t(notches)*step,int64_t(0),int64_t(maximum)));
    Invalidate();
}
