#pragma once

#include "UIFrameWnd.h"

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

class UIBitmapButton;
struct ITEM_INFO;

class UIItemWnd : public UIFrameWnd {
public:
    UIItemWnd();
    ~UIItemWnd() override;

    void SetShow(int show) override;
    void Move(int x, int y) override;
    bool IsUpdateNeed() override;
    int SendMsg(UIWindow* sender, int msg, int wparam, int lparam, int extra) override;
    void OnCreate(int x, int y) override;
    void OnDestroy() override;
    void OnDraw() override;
    void OnLBtnDblClk(int x, int y) override;
    void OnLBtnDown(int x, int y) override;
    void OnMouseMove(int x, int y) override;
    void OnMouseHover(int x, int y) override;
    void StoreInfo() override;
    void DrawHoverOverlay(HDC hdc, const RECT& clientRect) const;

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
    void UpdateHoveredItem(int globalX, int globalY);
    int GetTabAtPoint(int globalX, int globalY) const;
    int GetItemColumns() const;
    int GetItemRows() const;
    int GetMaxViewOffset(int itemCount) const;
    std::vector<const ITEM_INFO*> GetFilteredItems() const;
    HBITMAP GetItemIcon(const ITEM_INFO& item);
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
    HBITMAP m_backgroundLeft;
    HBITMAP m_backgroundMid;
    HBITMAP m_backgroundRight;
    HBITMAP m_titleBarBitmap;
    std::array<HBITMAP, 3> m_tabBitmaps;
    HBITMAP m_hoverBitmap;
    std::unordered_map<unsigned int, HBITMAP> m_iconCache;
    std::vector<VisibleItem> m_visibleItems;
    unsigned long long m_lastVisualStateToken;
    bool m_hasVisualStateToken;
};