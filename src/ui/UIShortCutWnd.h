#pragma once

#include "UIWindow.h"

#include <unordered_map>

struct ITEM_INFO;

class UIShortCutWnd : public UIWindow {
public:
    UIShortCutWnd();
    ~UIShortCutWnd() override;

    void SetShow(int show) override;
    bool IsUpdateNeed() override;
    void DragAndDrop(int x, int y, const DRAG_INFO* const info) override;
    void StoreInfo() override;
    void OnDraw() override;
    void OnLBtnDown(int x, int y) override;
    void OnLBtnDblClk(int x, int y) override;
    void OnLBtnUp(int x, int y) override;
    void OnMouseMove(int x, int y) override;
    void OnMouseHover(int x, int y) override;
    void OnRBtnDown(int x, int y) override;
    void OnWheel(int delta) override;
    int GetHoverSlot() const;

private:
    void LoadAssets();
    void ReleaseAssets();
    void UpdateHoverSlot(int globalX, int globalY);
    int GetSlotAtGlobalPoint(int globalX, int globalY) const;
    RECT GetSlotRect(int visibleSlot) const;
    void ActivateSlot(int visibleSlot);
    void DrawSlotOverlayText(HDC hdc, const RECT& slotRect, int value) const;
    HBITMAP GetItemIcon(const ITEM_INFO& item);
    HBITMAP GetSkillIcon(int skillId);
    unsigned long long BuildDisplayStateToken() const;

    HBITMAP m_backgroundBitmap;
    HBITMAP m_slotButtonBitmap;
    std::unordered_map<unsigned int, HBITMAP> m_itemIconCache;
    std::unordered_map<int, HBITMAP> m_skillIconCache;
    int m_hoverSlot;
    int m_pressedSlot;
    int m_pressedSlotAbsoluteIndex;
    int m_slotPressArmed;
    int m_isDragging;
    int m_startGlobalX;
    int m_startGlobalY;
    int m_orgX;
    int m_orgY;
    unsigned long long m_lastDrawStateToken;
    bool m_hasDrawStateToken;
};
