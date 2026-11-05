#pragma once

#include "item/Item.h"
#include "UIFrameWnd.h"
#include "UIShopCommon.h"
#include "ItemInfoSource.h"

#include <array>
#include <string>
#include <vector>

class UIItemInfoWnd : public UIFrameWnd {
public:
    struct DisplayButton {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        bool hovered = false;
        bool pressed = false;
        bool enabled = true;
        bool visible = true;
        std::string label;
    };

    struct DisplaySlot {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        bool available = false;
        bool occupied = false;
        bool hovered = false;
        unsigned int itemId = 0;
        std::string tooltip;
    };

    struct DisplayData {
        std::string title;
        unsigned int itemId = 0;
        bool identified = false;
        std::string name;
        bool previewUsesCollection = false;
        std::vector<std::string> detailLines;
        std::string description;
        DisplayButton closeButton;
        DisplayButton graphicsButton;
        std::vector<DisplaySlot> slotEntries;
    };

    UIItemInfoWnd();
    ~UIItemInfoWnd() override = default;

    void OnDraw() override;
    void OnProcess() override;
    void DrawNativeToHdc(HDC hdc);
    void OnLBtnDown(int x, int y) override;
    void OnLBtnUp(int x, int y) override;
    void OnRBtnDown(int x, int y) override;
    void OnMouseMove(int x, int y) override;
    void OnMouseHover(int x, int y) override;
    void StoreInfo() override;
    void SetItemInfo(const ITEM_INFO& item, int preferredX, int preferredY, ItemInfoSource source = ItemInfoSource::Snapshot);
    bool RefreshBoundItem();
    bool MatchesSelection(const ITEM_INFO& item) const;
    bool GetDisplayDataForQt(DisplayData* outData) const;
    bool HasItem() const;

private:
    RECT GetCloseButtonRect() const;
    RECT GetGraphicsButtonRect() const;
    RECT GetPreviewRect() const;
    RECT GetNameRect() const;
    RECT GetDescriptionRect() const;
    RECT GetSlotRect(int slotIndex) const;
    void UpdateInteraction(int x, int y);
    void RefreshPreviewAndSlots();
    int GetDesiredWindowHeight() const;
    bool HasViewButton() const;
    int GetDeclaredSlotCount() const;
    bool ShouldShowSlots() const;
    std::string BuildTitle() const;
    std::string BuildDisplayName() const;
    std::vector<std::string> BuildDetailLines() const;
    std::string BuildDescriptionText() const;

    ITEM_INFO m_item;
    ItemInfoSource m_source = ItemInfoSource::Snapshot;
    bool m_hasItem;
    bool m_closeHovered;
    bool m_closePressed;
    bool m_graphicsHovered;
    bool m_graphicsPressed;
    bool m_hasStoredPlacement;
    bool m_previewUsesCollection;
    int m_hoveredSlotIndex;
    shopui::BitmapPixels m_previewBitmap;
    std::array<unsigned int, 4> m_slotItemIds;
    std::array<shopui::BitmapPixels, 4> m_slotBitmaps;
};
