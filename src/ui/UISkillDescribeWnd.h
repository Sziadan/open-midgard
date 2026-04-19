#pragma once

#include "session/Session.h"
#include "UIFrameWnd.h"
#include "UIShopCommon.h"

#include <string>
#include <vector>

class UISkillDescribeWnd : public UIFrameWnd {
public:
    struct DisplayButton {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        bool hovered = false;
        bool pressed = false;
        std::string label;
    };

    struct DisplayScrollBar {
        bool visible = false;
        int trackX = 0;
        int trackY = 0;
        int trackWidth = 0;
        int trackHeight = 0;
        int thumbX = 0;
        int thumbY = 0;
        int thumbWidth = 0;
        int thumbHeight = 0;
    };

    struct DisplayData {
        std::string title;
        int skillId = 0;
        std::string name;
        std::vector<std::string> detailLines;
        std::string description;
        int descriptionX = 0;
        int descriptionY = 0;
        int descriptionWidth = 0;
        int descriptionHeight = 0;
        int descriptionContentHeight = 0;
        int descriptionScrollOffset = 0;
        DisplayScrollBar descriptionScrollBar;
        DisplayButton closeButton;
    };

    UISkillDescribeWnd();
    ~UISkillDescribeWnd() override = default;

    void OnDraw() override;
    void OnLBtnDown(int x, int y) override;
    void OnLBtnUp(int x, int y) override;
    void OnMouseMove(int x, int y) override;
    void OnMouseHover(int x, int y) override;
    void OnWheel(int delta) override;
    void StoreInfo() override;
    void SetSkillInfo(const PLAYER_SKILL_INFO& skillInfo, int preferredX, int preferredY);
    bool GetDisplayDataForQt(DisplayData* outData) const;
    bool HasSkill() const;

private:
    struct DescriptionLayout {
        RECT viewportRect{ 0, 0, 0, 0 };
        RECT scrollTrackRect{ 0, 0, 0, 0 };
        RECT scrollThumbRect{ 0, 0, 0, 0 };
        int contentHeight = 0;
        int maxScrollOffset = 0;
        bool scrollBarVisible = false;
    };

    RECT GetCloseButtonRect() const;
    DescriptionLayout GetDescriptionLayout() const;
    void ClampDescriptionScroll();
    void AdjustDescriptionScroll(int delta);
    void UpdateDescriptionScrollFromThumbPosition(int globalY);
    void UpdateInteraction(int x, int y);
    std::string BuildDescriptionText() const;

    PLAYER_SKILL_INFO m_skillInfo;
    bool m_hasSkill;
    bool m_closeHovered;
    bool m_closePressed;
    bool m_hasStoredPlacement;
    int m_descriptionScrollOffset;
    bool m_isDraggingDescriptionScroll;
    int m_descriptionScrollDragOffsetY;
    shopui::BitmapPixels m_iconBitmap;
};