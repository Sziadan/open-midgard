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
    };

    explicit UICompanionSkillListWnd(int windowId);
    std::vector<Row> GetRows();
    std::string GetTitle() const;
    std::string GetFooter() const;
    const UIFriendOptionButton& GetCloseButton() const { return *m_close; }
    void Move(int x, int y) override;
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
    int UseLevel(int skillId) const;
    const Icon* GetIcon(int skillId);

    const int m_family;
    UIFriendOptionButton* m_close;
    std::map<int, Icon> m_icons;
    std::array<Icon, 2> m_upgradeArt;
    int m_offset = 0;
    int m_selectedSkill = 0;
    int m_dragSkill = 0;
    int m_pressedUpgradeSkill = 0;
    POINT m_dragStart{};
};
