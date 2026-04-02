#pragma once

#include "UIFrameWnd.h"
#include "UIShopCommon.h"

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

class UIBitmapButton;
struct PLAYER_SKILL_INFO;

class UISkillListWnd : public UIFrameWnd {
public:
    struct QtButtonDisplay {
        int id = 0;
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        std::string label;
        bool visible = true;
    };

    struct DisplayRow {
        int skillId = 0;
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        bool iconVisible = true;
        bool selected = false;
        bool hovered = false;
        bool upgradeVisible = false;
        bool upgradePressed = false;
        int upgradeX = 0;
        int upgradeY = 0;
        int upgradeWidth = 0;
        int upgradeHeight = 0;
        std::string name;
        std::string levelText;
        std::string rightText;
    };

    struct DisplayButton {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        bool hovered = false;
        bool pressed = false;
        std::string label;
    };

    struct DisplayData {
        int skillPointCount = 0;
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
        std::vector<DisplayRow> rows;
        std::vector<DisplayButton> bottomButtons;
    };

    UISkillListWnd();
    ~UISkillListWnd() override;

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
    void OnWheel(int delta) override;
    void StoreInfo() override;
    bool GetDisplayDataForQt(DisplayData* outData) const;
    int GetQtSystemButtonCount() const;
    bool GetQtSystemButtonDisplayForQt(int index, QtButtonDisplay* outData) const;

private:
    struct TextButton {
        RECT rect{};
        std::string label;
        bool hovered = false;
        bool pressed = false;
    };

    struct VisibleSkill {
        const PLAYER_SKILL_INFO* skill = nullptr;
        RECT rowRect{};
        RECT upgradeRect{};
    };

    void EnsureCreated();
    void LayoutChildren();
    void LoadAssets();
    void ReleaseAssets();
    void RefreshVisibleSkillsForInteractionState();
    void UpdateHover(int globalX, int globalY);
    void DrawWindowChrome(HDC hdc) const;
    void DrawBottomButton(HDC hdc, const TextButton& button) const;
    int HitTestBottomButton(int globalX, int globalY) const;
    RECT GetScrollTrackRect() const;
    RECT GetScrollThumbRect(int skillCount) const;
    bool IsScrollBarVisible(int skillCount) const;
    void UpdateScrollFromThumbPosition(int globalY, int skillCount);
    std::vector<const PLAYER_SKILL_INFO*> GetSortedSkills() const;
    int GetMaxViewOffset(int skillCount) const;
    const shopui::BitmapPixels* GetSkillIcon(int skillId);
    const PLAYER_SKILL_INFO* GetSelectedSkill() const;
    unsigned long long BuildVisualStateToken() const;

    bool m_controlsCreated;
    int m_hoveredRow;
    int m_viewOffset;
    int m_selectedSkillId;
    int m_pressedUpgradeSkillId;
    int m_dragSkillId;
    int m_dragSkillLevel;
    bool m_dragArmed;
    POINT m_dragStartPoint;
    int m_isDraggingScrollThumb;
    int m_scrollDragOffsetY;
    std::array<UIBitmapButton*, 3> m_systemButtons;
    std::array<TextButton, 2> m_bottomButtons;
    shopui::BitmapPixels m_titleBarBitmap;
    shopui::BitmapPixels m_titleBarLeftBitmap;
    shopui::BitmapPixels m_titleBarMidBitmap;
    shopui::BitmapPixels m_titleBarRightBitmap;
    shopui::BitmapPixels m_btnBarLeftBitmap;
    shopui::BitmapPixels m_btnBarMidBitmap;
    shopui::BitmapPixels m_btnBarRightBitmap;
    shopui::BitmapPixels m_btnBarLeft2Bitmap;
    shopui::BitmapPixels m_btnBarMid2Bitmap;
    shopui::BitmapPixels m_btnBarRight2Bitmap;
    shopui::BitmapPixels m_itemRowBitmap;
    shopui::BitmapPixels m_itemInvertBitmap;
    shopui::BitmapPixels m_upgradeNormalBitmap;
    shopui::BitmapPixels m_upgradeHoverBitmap;
    shopui::BitmapPixels m_upgradePressedBitmap;
    shopui::BitmapPixels m_mesBtnLeftBitmap;
    shopui::BitmapPixels m_mesBtnMidBitmap;
    shopui::BitmapPixels m_mesBtnRightBitmap;
    std::unordered_map<int, shopui::BitmapPixels> m_iconCache;
    std::vector<VisibleSkill> m_visibleSkills;
    unsigned long long m_lastVisualStateToken;
    bool m_hasVisualStateToken;
};
