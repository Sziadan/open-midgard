#pragma once

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

struct SkillMetadata {
    int skillId = 0;
    std::string skillIdName;
    std::string displayName;
    std::vector<std::string> descriptionLines;
    std::vector<int> levelSpCosts;
};

class CSkillMgr {
public:
    CSkillMgr();
    ~CSkillMgr();

    bool EnsureLoaded();
    bool IsLevelUseSkill(int skillId) const;
    const SkillMetadata* GetSkillMetadata(int skillId) const;
    const SkillMetadata* GetSkillMetadataByName(const std::string& skillIdName) const;
    std::string GetSkillIconPath(int skillId) const;

private:
    bool LoadSkillMetadata();
    bool BuildSkillIdMapFromNameTable();
    void LoadSkillDisplayNames();
    void LoadSkillDescriptions();
    void LoadSkillLevelSpCosts();
    void LoadSourceLevelUseFlags();

    std::unordered_map<int, SkillMetadata> m_byId;
    std::unordered_map<std::string, int> m_idBySkillName;
    bool m_loaded;
    std::array<bool,8238> m_sourceLevelUseFlags{};
};

extern CSkillMgr g_skillMgr;

// Ref SKILL_INFO::IsSkillNotDelayed, HighPriest.exe.c:263510.
namespace SKILL_INFO {
const char* GetSkillIdNameBySkillId(int skillId);
bool IsSkillNotDelayed(int skillId);
bool IsGoodSkillForActor(int skillId);
}
