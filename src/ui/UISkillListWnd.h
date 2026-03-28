#pragma once

#include "UIFrameWnd.h"

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

class UIBitmapButton;
struct PLAYER_SKILL_INFO;

class UISkillListWnd : public UIFrameWnd {
public:
    UISkillListWnd();
    ~UISkillListWnd() override;

    void SetShow(int show) override;
    void Move(int x, int y) override;
    bool IsUpdateNeed() override;
    int SendMsg(UIWindow* sender, int msg, int wparam, int lparam, int extra) override;
    void OnCreate(int x, int y) override;
    void OnDestroy() override;
    void OnDraw() override;
    void OnLBtnDown(int x, int y) override;
    void OnLBtnUp(int x, int y) override;
    void OnMouseMove(int x, int y) override;
    void OnWheel(int delta) override;
    void StoreInfo() override;

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
    HBITMAP GetSkillIcon(int skillId);
    const PLAYER_SKILL_INFO* GetSelectedSkill() const;
    unsigned long long BuildVisualStateToken() const;

    bool m_controlsCreated;
    int m_hoveredRow;
    int m_viewOffset;
    int m_selectedSkillId;
    int m_pressedUpgradeSkillId;
    int m_isDraggingScrollThumb;
    int m_scrollDragOffsetY;
    std::array<UIBitmapButton*, 3> m_systemButtons;
    std::array<TextButton, 2> m_bottomButtons;
    HBITMAP m_titleBarBitmap;
    HBITMAP m_titleBarLeftBitmap;
    HBITMAP m_titleBarMidBitmap;
    HBITMAP m_titleBarRightBitmap;
    HBITMAP m_btnBarLeftBitmap;
    HBITMAP m_btnBarMidBitmap;
    HBITMAP m_btnBarRightBitmap;
    HBITMAP m_btnBarLeft2Bitmap;
    HBITMAP m_btnBarMid2Bitmap;
    HBITMAP m_btnBarRight2Bitmap;
    HBITMAP m_itemRowBitmap;
    HBITMAP m_itemInvertBitmap;
    HBITMAP m_upgradeNormalBitmap;
    HBITMAP m_upgradeHoverBitmap;
    HBITMAP m_upgradePressedBitmap;
    HBITMAP m_mesBtnLeftBitmap;
    HBITMAP m_mesBtnMidBitmap;
    HBITMAP m_mesBtnRightBitmap;
    std::unordered_map<int, HBITMAP> m_iconCache;
    std::vector<VisibleSkill> m_visibleSkills;
    unsigned long long m_lastVisualStateToken;
    bool m_hasVisualStateToken;
};
