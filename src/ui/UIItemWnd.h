#pragma once

#include "UIFrameWnd.h"
#include "UIShopCommon.h"

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

class UIBitmapButton;
struct ITEM_INFO;

class UIItemWnd : public UIFrameWnd {
public:
    struct QtButtonDisplay {
        int id = 0;
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        std::string label;
        bool visible = true;
        bool active = false;
    };

    struct DisplaySlot {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        bool occupied = false;
        bool hovered = false;
        int count = 0;
        unsigned int itemId = 0;
        std::string label;
        std::string tooltip;
    };

    struct DisplayData {
        std::string title;
        int currentTab = 0;
        int currentItemCount = 0;
        int maxItemCount = 0;
        int viewOffset = 0;
        int maxViewOffset = 0;
        bool scrollBarVisible = false;
        int scrollTrackX = 0;
        int scrollTrackY = 0;
        int scrollTrackWidth = 0;
        int scrollTrackHeight = 0;
        int scrollThumbX = 0;
        int scrollThumbY = 0;
        int scrollThumbWidth = 0;
        int scrollThumbHeight = 0;
        std::vector<DisplaySlot> displaySlots;
    };

    UIItemWnd();
    ~UIItemWnd() override;

    void SetShow(int show) override;
    void Move(int x, int y) override;
    bool IsUpdateNeed() override;
    msgresult_t SendMsg(UIWindow* sender, int msg, msgparam_t wparam, msgparam_t lparam, msgparam_t extra) override;
    void OnCreate(int x, int y) override;
    void OnDestroy() override;
    void OnDraw() override;
    void OnLBtnDblClk(int x, int y) override;
    void OnLBtnDown(int x, int y) override;
    void OnLBtnUp(int x, int y) override;
    void OnMouseMove(int x, int y) override;
    void OnMouseHover(int x, int y) override;
    void DragAndDrop(int x, int y, const DRAG_INFO* const info) override;
    void StoreInfo() override;
    void DrawHoverOverlay(HDC hdc, const RECT& clientRect) const;
    bool IsMiniMode() const;
    bool GetDisplayDataForQt(DisplayData* outData) const;
    bool GetHoveredItemForQt(shopui::ItemHoverInfo* outData) const;
    int GetQtSystemButtonCount() const;
    bool GetQtSystemButtonDisplayForQt(int index, QtButtonDisplay* outData) const;
    int GetQtTabCount() const;
    bool GetQtTabDisplayForQt(int index, QtButtonDisplay* outData) const;

private:
    struct VisibleItem {
        const ITEM_INFO* item;
        RECT rect;
    };

    void EnsureCreated();
    void LayoutChildren();
    void LoadAssets();
    void ReleaseAssets();
    void SetMiniMode(bool miniMode);
    void SetCurrentTab(int tabIndex);
    void RefreshVisibleItemsForInteractionState();
    void UpdateHoveredItem(int globalX, int globalY);
    int GetTabAtPoint(int globalX, int globalY) const;
    int GetItemColumns() const;
    int GetItemRows() const;
    int GetInventoryItemCount() const;
    int GetInventorySlotCapacity() const;
    int GetMaxViewOffset(int itemCount) const;
    std::vector<const ITEM_INFO*> GetFilteredItems() const;
    const shopui::BitmapPixels* GetItemIcon(const ITEM_INFO& item);
    std::string GetTitleText() const;
    unsigned long long BuildVisualStateToken() const;

    bool m_controlsCreated;
    int m_currentTab;
    int m_viewOffset;
    int m_hoveredItemIndex;
    int m_fullHeight;
    const ITEM_INFO* m_hoverOverlayItem;
    RECT m_hoverOverlayRect;
    std::array<UIBitmapButton*, 3> m_systemButtons;
    shopui::BitmapPixels m_backgroundLeft;
    shopui::BitmapPixels m_backgroundMid;
    shopui::BitmapPixels m_backgroundRight;
    shopui::BitmapPixels m_titleBarBitmap;
    std::array<shopui::BitmapPixels, 3> m_tabBitmaps;
    shopui::BitmapPixels m_hoverBitmap;
    std::unordered_map<unsigned int, shopui::BitmapPixels> m_iconCache;
    std::vector<VisibleItem> m_visibleItems;
    bool m_dragArmed;
    POINT m_dragStartPoint;
    unsigned int m_dragItemId;
    unsigned int m_dragItemIndex;
    int m_dragItemEquipLocation;
    unsigned long long m_lastVisualStateToken;
    bool m_hasVisualStateToken;
};