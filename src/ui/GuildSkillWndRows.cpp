#include "session/Session.h"
#include "GuildSkillWndRows.h"

void GuildSkillWndRows::RefreshFromGuildSkillItems(
    const std::list<PLAYER_SKILL_INFO>& guildSkillItems,
    GuildSkillWndRefreshHooks& hooks)
{
    m_rows.clear();
    for (const PLAYER_SKILL_INFO& skillInfo : guildSkillItems) {
        if (skillInfo.m_isValid && (skillInfo.upgradable || skillInfo.level)) {
            m_rows.push_back(skillInfo);
        }
    }

    hooks.Invalidate();
    hooks.RecalcScrollBarPos();
    hooks.RefreshButton();
}
