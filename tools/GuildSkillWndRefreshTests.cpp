// Minimal session record double: only the fields read by the production refresh
// body are needed for this isolated parity test.
struct PLAYER_SKILL_INFO {
    int m_isValid = 0;
    int SKID = 0;
    int level = 0;
    int upgradable = 0;
};

#include "ui/GuildSkillWndRows.h"

#include <cassert>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "GuildSkillWndRefreshBody.inc"

namespace {

struct RefreshEvents final : GuildSkillWndRefreshHooks {
    std::vector<std::string> events;
    const GuildSkillWndRows* rows = nullptr;
    std::vector<int> observedRowIds;

    void Record(const char* event)
    {
        events.emplace_back(event);
        if (rows) {
            observedRowIds.clear();
            for (const PLAYER_SKILL_INFO& row : rows->GetRows()) {
                observedRowIds.push_back(row.SKID);
            }
        }
    }

    void Invalidate() override { Record("invalidate"); }
    void RecalcScrollBarPos() override { Record("scrollbar"); }
    void RefreshButton() override { Record("buttons"); }
};

std::vector<std::uint8_t> ReadBytes(const char* path)
{
    std::ifstream file(path, std::ios::binary);
    assert(file && "unable to read reference executable");
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(file), {});
}

std::string ReadText(const char* path)
{
    std::ifstream file(path, std::ios::binary);
    assert(file && "unable to read reference decompilation");
    return std::string(std::istreambuf_iterator<char>(file), {});
}

std::size_t FindGuildSkillSendMsgDefinition(const std::string& source)
{
    const std::string signature = "int __thiscall UIGuildSkillWnd::SendMsg(";
    std::size_t searchFrom = 0;
    while (true) {
        const std::size_t signaturePos = source.find(signature, searchFrom);
        assert(signaturePos != std::string::npos);
        const std::size_t closeParen = source.find(')', signaturePos + signature.size());
        assert(closeParen != std::string::npos);
        std::size_t bodyStart = closeParen + 1;
        while (bodyStart < source.size() && std::isspace(static_cast<unsigned char>(source[bodyStart]))) {
            ++bodyStart;
        }
        if (bodyStart < source.size() && source[bodyStart] == '{') {
            return bodyStart;
        }
        searchFrom = closeParen + 1;
    }
}

std::string FindCase23Branch(const std::string& source, std::size_t functionBodyStart)
{
    const std::size_t switchPos = source.find("switch ( message )", functionBodyStart);
    assert(switchPos != std::string::npos);
    const std::size_t switchOpen = source.find('{', switchPos);
    assert(switchOpen != std::string::npos);

    int depth = 1;
    std::size_t switchClose = switchOpen + 1;
    for (; switchClose < source.size() && depth > 0; ++switchClose) {
        if (source[switchClose] == '{') ++depth;
        else if (source[switchClose] == '}') --depth;
    }
    assert(depth == 0);
    --switchClose;

    const std::size_t branchStart = source.find("case 23:", switchOpen + 1);
    assert(branchStart != std::string::npos && branchStart < switchClose);
    const std::size_t nextCase = source.find("\n    case ", branchStart + 1);
    const std::size_t branchEnd = nextCase != std::string::npos && nextCase < switchClose
        ? nextCase : switchClose;
    return source.substr(branchStart, branchEnd - branchStart);
}

void CheckExecutableDispatchEvidence()
{
    const std::vector<std::uint8_t> exe = ReadBytes(GUILD_SKILL_EXE_PATH);
    constexpr std::size_t imageBase = 0x400000;
    constexpr std::size_t selectorTableVa = 0x4B06A8;
    constexpr std::size_t jumpTableVa = 0x4B068C;
    constexpr std::size_t message23SelectorIndex = 23 - 6;
    constexpr std::size_t selectorOffset = selectorTableVa - imageBase + message23SelectorIndex;
    assert(selectorOffset < exe.size());
    const std::uint8_t selector = exe[selectorOffset];
    assert(selector == 0x02); // message 23 -> selector 0x02

    const std::size_t targetOffset = jumpTableVa - imageBase + selector * sizeof(std::uint32_t);
    assert(targetOffset + sizeof(std::uint32_t) <= exe.size());
    const std::uint32_t targetVa = static_cast<std::uint32_t>(exe[targetOffset])
        | (static_cast<std::uint32_t>(exe[targetOffset + 1]) << 8)
        | (static_cast<std::uint32_t>(exe[targetOffset + 2]) << 16)
        | (static_cast<std::uint32_t>(exe[targetOffset + 3]) << 24);
    assert(targetVa == 0x4B01E1); // message 23 branch target

    constexpr std::uint8_t branchPrefix[] = {0x8B, 0xBB, 0xF8, 0x00, 0x00, 0x00, 0x8B, 0x37};
    const std::size_t branchOffset = targetVa - imageBase;
    assert(branchOffset + sizeof(branchPrefix) <= exe.size());
    for (std::size_t i = 0; i < sizeof(branchPrefix); ++i) {
        assert(exe[branchOffset + i] == branchPrefix[i]);
    }
}

void CheckReferenceBranchEvidence()
{
    const std::string decomp = ReadText(GUILD_SKILL_DECOMP_PATH);
    const std::string reference = ReadText(GUILD_SKILL_REF_PATH);
    const std::string condition = "skillInfo.m_isValid && (skillInfo.upgradable || skillInfo.level)";
    const std::string readSequence = "CSession::GetGuildSkillItemInfoBySequence(&g_session, &skillInfo, v17)";
    const std::string invalidate = "this->UIWindow::Invalidate(this)";
    const std::string recalc = "UIGuildSkillWnd::RecalcScrbarPos(this)";
    const std::string buttons = "UIGuildSkillWnd::RefreshButton(this)";

    for (const std::string* text : {&decomp, &reference}) {
        const std::size_t functionBodyStart = FindGuildSkillSendMsgDefinition(*text);
        const std::string branch = FindCase23Branch(*text, functionBodyStart);
        const std::size_t conditionPos = branch.find(condition);
        const std::size_t readPos = branch.find(readSequence);
        const std::size_t invalidatePos = branch.find(invalidate);
        const std::size_t recalcPos = branch.find(recalc);
        const std::size_t buttonsPos = branch.find(buttons);
        assert(readPos != std::string::npos && conditionPos != std::string::npos);
        assert(invalidatePos != std::string::npos && recalcPos != std::string::npos && buttonsPos != std::string::npos);
        assert(readPos < conditionPos && conditionPos < invalidatePos);
        assert(invalidatePos < recalcPos && recalcPos < buttonsPos);
    }
}

void CheckRefreshBehavior()
{
    GuildSkillWndRows model;
    RefreshEvents events;
    events.rows = &model;

    PLAYER_SKILL_INFO oldRow{};
    oldRow.m_isValid = 1;
    oldRow.SKID = 500;
    oldRow.level = 1; // Source predicate requires a learned or upgradeable valid row.
    model.RefreshFromGuildSkillItems({oldRow}, events);
    assert(model.GetRows().size() == 1);

    events.events.clear();
    events.observedRowIds.clear();
    PLAYER_SKILL_INFO validLearned{};
    validLearned.m_isValid = 1;
    validLearned.SKID = 10001;
    validLearned.level = 2;
    PLAYER_SKILL_INFO validUpgradeable{};
    validUpgradeable.m_isValid = 1;
    validUpgradeable.SKID = 10002;
    validUpgradeable.upgradable = 1;
    PLAYER_SKILL_INFO invalidButLearned = validLearned;
    invalidButLearned.SKID = 10003;
    invalidButLearned.m_isValid = 0;
    PLAYER_SKILL_INFO validButUnavailable{};
    validButUnavailable.m_isValid = 1;
    validButUnavailable.SKID = 10004;
    std::list<PLAYER_SKILL_INFO> input{
        validLearned, invalidButLearned, validUpgradeable, validButUnavailable};

    model.RefreshFromGuildSkillItems(input, events);
    assert((events.events == std::vector<std::string>{"invalidate", "scrollbar", "buttons"}));
    assert((events.observedRowIds == std::vector<int>{10001, 10002}));
    assert(model.GetRows().size() == 2);
    assert(model.GetRows()[0].SKID == 10001 && model.GetRows()[1].SKID == 10002);

    events.events.clear();
    events.observedRowIds.clear();
    model.RefreshFromGuildSkillItems({}, events);
    assert(model.GetRows().empty()); // old rows are cleared even for an empty refresh
    assert((events.events == std::vector<std::string>{"invalidate", "scrollbar", "buttons"}));
    assert(events.observedRowIds.empty());
}

} // namespace

int main()
{
    CheckExecutableDispatchEvidence();
    CheckReferenceBranchEvidence();
    CheckRefreshBehavior();
    return 0;
}
