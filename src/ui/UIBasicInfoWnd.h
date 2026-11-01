#pragma once

#include "UIFrameWnd.h"
#include "UIShopCommon.h"

#include <array>
#include <string>

class UIBitmapButton;

class UIBasicInfoWnd : public UIFrameWnd {
public:
    struct QtButtonDisplay {
        int id = 0;
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        std::string label;
        std::string icon;
        bool hovered = false;
        bool pressed = false;
        bool visible = true;
    };

    struct DisplayData {
        std::string name;
        std::string jobName;
        int level = 0;
        int jobLevel = 0;
        int hp = 0;
        int maxHp = 0;
        int sp = 0;
        int maxSp = 0;
        int money = 0;
        int weight = 0;
        int maxWeight = 0;
        int expPercent = 0;
        int jobExpPercent = 0;
    };

    UIBasicInfoWnd();
    ~UIBasicInfoWnd() override;

    void SetShow(int show) override;
    void Move(int x, int y) override;
    bool IsUpdateNeed() override;
    msgresult_t SendMsg(UIWindow* sender, int msg, msgparam_t wparam, msgparam_t lparam, msgparam_t extra) override;
    void OnCreate(int x, int y) override;
    void OnDraw() override;
    void OnLBtnDown(int x, int y) override;
    void OnLBtnUp(int x, int y) override;
    void OnLBtnDblClk(int x, int y) override;
    void OnMouseMove(int x, int y) override;
    void OnMouseHover(int x, int y) override;
    void StoreInfo() override;

    void NewHeight(int height);
    bool IsMiniMode() const;
    bool GetDisplayDataForQt(DisplayData* outData) const;
    int GetQtSystemButtonCount() const;
    bool GetQtSystemButtonDisplayForQt(int index, QtButtonDisplay* out) const;
    bool GetHoveredItemForQt(shopui::ItemHoverInfo* info) const;
    int GetQtMenuButtonCount() const;
    bool GetQtMenuButtonDisplayForQt(int index, QtButtonDisplay* out) const;

private:
    enum : int {
        kButtonIdStatus = 126,
        kButtonIdOption = 127,
        kButtonIdItems = 128,
        kButtonIdEquip = 129,
        kButtonIdSkill = 130,
        kButtonIdFriend = 133,
        kButtonIdBase = 134,
        kButtonIdMini = 136,
        kButtonIdComm = 148,
        kButtonIdMap = 153,
    };

    void EnsureCreated();
    void LayoutChildren();
    void SetMiniMode(bool miniMode);
    int HitQtButton(int x, int y) const;
    void UpdateQtButtonInteraction(int x, int y);
    DisplayData BuildDisplayData() const;
    unsigned long long BuildDisplayStateToken() const;
    void DrawCachedBitmap(HDC hdc, const shopui::BitmapPixels& bitmap, int x, int y) const;
    void DrawBar(HDC hdc, int x, int y, int percent, bool redBar) const;
    void DrawExpBar(HDC hdc, int x, int y, int percent) const;
    void DrawWindowText(HDC hdc, int x, int y, const char* text, COLORREF color, UINT format = DT_LEFT | DT_TOP | DT_SINGLELINE, HFONT font = nullptr, int height = 16) const;
    void LoadAssets();
    void ReleaseAssets();

    bool m_controlsCreated;
    std::array<UIBitmapButton*, 8> m_menuButtons;
    UIBitmapButton* m_baseButton;
    UIBitmapButton* m_miniButton;
    shopui::BitmapPixels m_backgroundFull;
    shopui::BitmapPixels m_backgroundMini;
    shopui::BitmapPixels m_titleBarBitmap;
    shopui::BitmapPixels m_barBackground;
    shopui::BitmapPixels m_redLeft;
    shopui::BitmapPixels m_redMid;
    shopui::BitmapPixels m_redRight;
    shopui::BitmapPixels m_blueLeft;
    shopui::BitmapPixels m_blueMid;
    shopui::BitmapPixels m_blueRight;
    unsigned long long m_lastDrawStateToken;
    bool m_hasDrawStateToken;
    bool m_hoverIdentity = false;
    int m_hotButtonId;
    int m_armedButtonId;
    int m_pressedButtonId;
};