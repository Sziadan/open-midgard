#pragma once

#include "UIFrameWnd.h"

#include <array>
#include <string>

class UIBitmapButton;

class UIStatusWnd : public UIFrameWnd {
public:
    struct DisplayData {
        std::array<int, 6> baseStats{};
        std::array<int, 6> plusStats{};
        std::array<int, 6> statCosts{};
        int attack = 0;
        int refineAttack = 0;
        int matkMin = 0;
        int matkMax = 0;
        int itemDef = 0;
        int plusDef = 0;
        int itemMdef = 0;
        int plusMdef = 0;
        int hit = 0;
        int flee = 0;
        int plusFlee = 0;
        int critical = 0;
        int aspd = 0;
        int plusAspd = 0;
        int statusPoint = 0;
        int guildId = 0;
    };

    UIStatusWnd();
    ~UIStatusWnd() override;

    void SetShow(int show) override;
    void Move(int x, int y) override;
    bool IsUpdateNeed() override;
    msgresult_t SendMsg(UIWindow* sender, int msg, msgparam_t wparam, msgparam_t lparam, msgparam_t extra) override;
    void OnCreate(int x, int y) override;
    void OnDraw() override;
    void OnLBtnDblClk(int x, int y) override;
    void OnLBtnDown(int x, int y) override;
    void OnMouseHover(int x, int y) override;
    void StoreInfo() override;
    bool IsMiniMode() const;
    int GetPageForQt() const;
    bool GetDisplayDataForQt(DisplayData* outData) const;

private:
    void EnsureCreated();
    void LayoutChildren();
    void RefreshIncrementButtons();
    void SetMiniMode(bool miniMode);
    void SetPage(int page);
    DisplayData BuildDisplayData() const;
    unsigned long long BuildDisplayStateToken() const;
    void DrawWindowText(HDC hdc, int x, int y, const char* text, COLORREF color, UINT format = DT_LEFT | DT_TOP | DT_SINGLELINE) const;
    void DrawRightAlignedValue(HDC hdc, int right, int y, const std::string& text) const;
    void LoadAssets();
    void ReleaseAssets();

    bool m_controlsCreated;
    int m_page;
    std::array<UIBitmapButton*, 3> m_systemButtons;
    std::array<UIBitmapButton*, 6> m_incrementButtons;
    HBITMAP m_titleBarBitmap;
    std::array<HBITMAP, 2> m_pageBackgrounds;
    unsigned long long m_lastDrawStateToken;
    bool m_hasDrawStateToken;
};
