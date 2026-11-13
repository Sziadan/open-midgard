#pragma once

#include "UIFrameWnd.h"
#include <array>
#include <list>
#include <map>
#include <vector>

class UIFriendOptionButton;
struct PLAYER_SKILL_INFO;

class UICompanionSkillListWnd : public UIFrameWnd {
public:
    struct Icon {
        shopui::BitmapPixels pixels;
        std::string uri;
    };
    struct SkinPiece {
        RECT bounds;
        const Icon* art;
        bool control;
    };
    struct Row {
        int id;
        int y;
        int level;
        int spCost;
        bool selected;
        std::string name;
        const Icon* icon;
        const Icon* upgrade;
        int upgradeX;
        int upgradeY;
        int upgradeSize;
        int useLevel;
        bool levelSelectable;
    };

    explicit UICompanionSkillListWnd(int windowId);
    std::vector<Row> GetRows();
    std::string GetTitle() const;
    std::string GetFooter() const;
    RECT GetScrollThumb() const;
    std::vector<SkinPiece> GetSkinPieces() const;
    const UIFriendOptionButton& GetCloseButton() const { return *m_close; }
    const UIFriendOptionButton& GetBaseButton() const { return *m_base; }
    const UIFriendOptionButton& GetMiniButton() const { return *m_mini; }
    void Move(int x, int y) override;
    void Resize(int width, int height) override;
    void OnDraw() override;
    void OnLBtnDown(int x, int y) override;
    void OnLBtnDblClk(int x, int y) override;
    void OnLBtnUp(int x, int y) override;
    void OnMouseMove(int x, int y) override;
    void OnRBtnDown(int x, int y) override;
    void OnWheel(int delta) override;
    msgresult_t SendMsg(UIWindow*, int msg, msgparam_t action, msgparam_t, msgparam_t) override;

private:
    const PLAYER_SKILL_INFO* SkillAt(int x, int y) const;
    const PLAYER_SKILL_INFO* UpgradeAt(int x, int y) const;
    const std::list<PLAYER_SKILL_INFO>& Skills() const;
    bool CanUpgrade(const PLAYER_SKILL_INFO& skill) const;
    int MaxOffset() const;
    int VisibleRows() const;
    void ScrollBy(int rows);
    int UseLevel(int skillId) const;
    int EffectiveUseLevel(const PLAYER_SKILL_INFO& skill) const;
    int LevelControlAt(int x, int y) const;
    const Icon* GetIcon(int skillId);

    const int m_family;
    std::string m_title;
    std::string m_pointsFormat;
    UIFriendOptionButton* m_close;
    UIFriendOptionButton* m_base;
    UIFriendOptionButton* m_mini;
    std::map<int, Icon> m_icons;
    std::array<Icon, 2> m_upgradeArt;
    enum SkinAsset {
        TitleLeft, TitleMiddle, TitleRight, SideLeft, SideRight,
        FooterLeft, FooterMiddle, FooterRight, ScrollUp, ScrollTrack, ScrollDown,
        ThumbTop, ThumbMiddle, ThumbBottom, ResizeGrip, SkinAssetCount
    };
    std::array<Icon, SkinAssetCount> m_skin;
    int m_offset = 0;
    int m_thumbGrab = -1;
    bool m_resizing = false;
    POINT m_resizeStart{};
    int m_resizeWidth = 0;
    int m_resizeHeight = 0;
    int m_selectedSkill = 0;
    int m_dragSkill = 0;
    int m_pressedUpgradeSkill = 0;
    int m_pressedLevelSkill = 0;
    int m_pressedLevelDelta = 0;
    POINT m_dragStart{};
};
