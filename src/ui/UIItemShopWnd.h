#pragma once

#include "UIFrameWnd.h"
#include "UIShopCommon.h"
#include "item/Item.h"

#include <unordered_map>

class UIItemShopWnd : public UIFrameWnd {
public:
    UIItemShopWnd();
    ~UIItemShopWnd() override;

    void SetShow(int show) override;
    bool IsUpdateNeed() override;
    void StoreInfo() override;
    void OnDraw() override;
    void OnLBtnDown(int x, int y) override;
    void OnLBtnDblClk(int x, int y) override;
    void OnLBtnUp(int x, int y) override;
    void OnMouseMove(int x, int y) override;
    void DragAndDrop(int x, int y, const DRAG_INFO* const info) override;
    void OnWheel(int delta) override;
    void HandleKeyDown(int virtualKey);
    int GetViewOffset() const;
    int GetHoverRow() const;
    int GetVisibleRowCountForQt() const;

private:
    int GetVisibleRowCount() const;
    int GetMaxViewOffset() const;
    RECT GetListRect() const;
    int HitTestSourceRow(int x, int y) const;
    const shopui::BitmapPixels* GetItemIcon(const ITEM_INFO& item);
    unsigned long long BuildDisplayStateToken() const;

    void ArmRowDrag(int rowIndex, int x, int y);
    void UpdateRowDrag(int x, int y);

    int m_viewOffset;
    int m_hoverRow;
    bool m_dragArmed;
    int m_dragStartX;
    int m_dragStartY;
    int m_dragRow;
    std::unordered_map<unsigned int, shopui::BitmapPixels> m_iconCache;
    unsigned long long m_lastDrawStateToken;
    bool m_hasDrawStateToken;
};

void BeginNpcShopItemDrag(int dragSource, int sourceRow, unsigned int itemId, int itemType, int count);
void RequestNpcShopQuantityTransfer(int sourceRow, bool removeFromCart);
int FindNpcShopSourceRow(unsigned int sourceItemIndex, unsigned int itemId);
