#pragma once

#include "UIFrameWnd.h"
#include "UIShopCommon.h"

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

class UIBitmapButton;
struct ITEM_INFO;

class UIEquipWnd : public UIFrameWnd {
public:
    struct DisplaySlot {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        bool occupied = false;
        bool leftColumn = false;
        std::string label;
    };

    struct DisplayData {
        std::vector<DisplaySlot> slots;
    };

    UIEquipWnd();
    ~UIEquipWnd() override;

    void SetShow(int show) override;
    void Move(int x, int y) override;
    bool IsUpdateNeed() override;
    msgresult_t SendMsg(UIWindow* sender, int msg, msgparam_t wparam, msgparam_t lparam, msgparam_t extra) override;
    void OnCreate(int x, int y) override;
    void OnDestroy() override;
    void OnDraw() override;
    void OnLBtnDown(int x, int y) override;
    void OnLBtnUp(int x, int y) override;
    void OnMouseMove(int x, int y) override;
    void OnLBtnDblClk(int x, int y) override;
    void DragAndDrop(int x, int y, const DRAG_INFO* const info) override;
    void StoreInfo() override;
    bool IsMiniMode() const;
    bool GetDisplayDataForQt(DisplayData* outData) const;

private:
    void EnsureCreated();
    void LayoutChildren();
    void LoadAssets();
    void ReleaseAssets();
    void SetMiniMode(bool miniMode);
    std::vector<const ITEM_INFO*> BuildSlotAssignments() const;
    const shopui::BitmapPixels* GetItemIcon(const ITEM_INFO& item);
    unsigned long long BuildVisualStateToken() const;

    bool m_controlsCreated;
    int m_fullHeight;
    std::array<UIBitmapButton*, 3> m_systemButtons;
    shopui::BitmapPixels m_backgroundLeft;
    shopui::BitmapPixels m_backgroundMid;
    shopui::BitmapPixels m_backgroundRight;
    shopui::BitmapPixels m_backgroundFull;
    shopui::BitmapPixels m_titleBarLeft;
    shopui::BitmapPixels m_titleBarMid;
    shopui::BitmapPixels m_titleBarRight;
    std::unordered_map<unsigned int, shopui::BitmapPixels> m_iconCache;
    bool m_dragArmed;
    POINT m_dragStartPoint;
    unsigned int m_dragItemId;
    unsigned int m_dragItemIndex;
    int m_dragItemEquipLocation;
    unsigned long long m_lastVisualStateToken;
    bool m_hasVisualStateToken;
};
