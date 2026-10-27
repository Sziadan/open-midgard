#pragma once

#include <list>
#include <vector>

struct PLAYER_SKILL_INFO;

// The refresh effects are supplied by the owning window. Keeping them explicit
// preserves the target's ordering while allowing the row-filtering body to be
// checked without constructing a renderer-backed UIWindow.
class GuildSkillWndRefreshHooks {
public:
    virtual ~GuildSkillWndRefreshHooks() = default;
    virtual void Invalidate() = 0;
    virtual void RecalcScrollBarPos() = 0;
    virtual void RefreshButton() = 0;
};

class GuildSkillWndRows {
public:
    void RefreshFromGuildSkillItems(
        const std::list<PLAYER_SKILL_INFO>& guildSkillItems,
        GuildSkillWndRefreshHooks& hooks);

    const std::vector<PLAYER_SKILL_INFO>& GetRows() const { return m_rows; }

private:
    std::vector<PLAYER_SKILL_INFO> m_rows;
};
