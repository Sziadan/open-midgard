#pragma once

#include "UIFrameWnd.h"

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

class UIBitmapButton;
struct ITEM_INFO;

class UIEquipWnd : public UIFrameWnd {
public:
    UIEquipWnd();
    ~UIEquipWnd() override;

    void SetShow(int show) override;
    void Move(int x, int y) override;
    bool IsUpdateNeed() override;
    int SendMsg(UIWindow* sender, int msg, int wparam, int lparam, int extra) override;
    void OnCreate(int x, int y) override;
    void OnDestroy() override;
    void OnDraw() override;
    void OnLBtnDblClk(int x, int y) override;
    void StoreInfo() override;

private:
    void EnsureCreated();
    void LayoutChildren();
    void LoadAssets();
    void ReleaseAssets();
    void SetMiniMode(bool miniMode);
    std::vector<const ITEM_INFO*> BuildSlotAssignments() const;
    HBITMAP GetItemIcon(const ITEM_INFO& item);
    unsigned long long BuildVisualStateToken() const;

    bool m_controlsCreated;
    int m_fullHeight;
    std::array<UIBitmapButton*, 3> m_systemButtons;
    HBITMAP m_backgroundLeft;
    HBITMAP m_backgroundMid;
    HBITMAP m_backgroundRight;
    HBITMAP m_backgroundFull;
    HBITMAP m_titleBarLeft;
    HBITMAP m_titleBarMid;
    HBITMAP m_titleBarRight;
    std::unordered_map<unsigned int, HBITMAP> m_iconCache;
    unsigned long long m_lastVisualStateToken;
    bool m_hasVisualStateToken;
};
