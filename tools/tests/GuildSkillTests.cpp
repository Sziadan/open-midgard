#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <list>
#include <string>
#include <vector>

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using s16 = std::int16_t;

struct PacketView {
    u16 packetId;
    u16 packetLength;
    const u8* data;
};

struct PLAYER_SKILL_INFO {
    int m_isValid = 0;
    int SKID = 0;
    int type = 0;
    int level = 0;
    int spcost = 0;
    int upgradable = 0;
    int attackRange = 0;
    int skillPos = 0;
    int skillMaxLv = 0;
    std::string skillIdName;
    std::string skillName;
    std::vector<std::string> descriptionLines;
    std::vector<int> needSkillList;
};

class CSession {
public:
    void ClearGuildSkillItems();
    bool SetGuildSkillItem(const PLAYER_SKILL_INFO& skillInfo);
    const std::list<PLAYER_SKILL_INFO>& GetGuildSkillItems() const;
    int GetGuildSkillPoint() const;
    void SetGuildSkillPoint(int points);
    void InvalidateUI();
    int m_invalidationCount = 0;
    int m_lastInvalidatedGuildSkillPoint = 0;

private:
    std::list<PLAYER_SKILL_INFO> m_guildSkillItems;
    int m_guildSkillPoint = 0;
};

CSession g_session;

class CGameMode {
public:
    int m_isReqUpgradeSkillLevel = 1;
};

u16 ReadLE16(const u8* data)
{
    return static_cast<u16>(data[0]) | static_cast<u16>(data[1] << 8);
}

u32 ReadLE32(const u8* data)
{
    return static_cast<u32>(data[0])
        | (static_cast<u32>(data[1]) << 8)
        | (static_cast<u32>(data[2]) << 16)
        | (static_cast<u32>(data[3]) << 24);
}

#include "GuildSkillSessionBodies.inc"
#include "GuildSkillPacketBody.inc"

void CSession::InvalidateUI()
{
    ++m_invalidationCount;
    m_lastInvalidatedGuildSkillPoint = GetGuildSkillPoint();
}

void Put16(std::vector<u8>& bytes, size_t offset, u16 value)
{
    bytes[offset] = static_cast<u8>(value & 0xFF);
    bytes[offset + 1] = static_cast<u8>(value >> 8);
}

void Put32(std::vector<u8>& bytes, size_t offset, u32 value)
{
    bytes[offset] = static_cast<u8>(value & 0xFF);
    bytes[offset + 1] = static_cast<u8>((value >> 8) & 0xFF);
    bytes[offset + 2] = static_cast<u8>((value >> 16) & 0xFF);
    bytes[offset + 3] = static_cast<u8>((value >> 24) & 0xFF);
}

void PutRecord(std::vector<u8>& bytes, size_t offset, u16 skillId, u32 type, s16 level, s16 spCost, u8 upgradable)
{
    Put16(bytes, offset, skillId);
    Put32(bytes, offset + 2, type);
    Put16(bytes, offset + 6, static_cast<u16>(level));
    Put16(bytes, offset + 8, static_cast<u16>(spCost));
    bytes[offset + 36] = upgradable;
}

int main()
{
    CGameMode mode;
    PLAYER_SKILL_INFO oldSkill{};
    oldSkill.SKID = 10000;
    oldSkill.skillName = "old";
    g_session.SetGuildSkillItem(oldSkill);
    g_session.SetGuildSkillPoint(17);

    std::vector<u8> packet(6 + 37 * 2, 0);
    Put16(packet, 2, static_cast<u16>(packet.size()));
    Put16(packet, 4, static_cast<u16>(-3));
    PutRecord(packet, 6, 10003, 0x11223344, 7, 12, 1);
    PutRecord(packet, 43, 10003, 0x55667788, 8, 13, 0);
    HandleGuildSkillInfo(mode, PacketView{0x0162, static_cast<u16>(packet.size()), packet.data()});
    assert(mode.m_isReqUpgradeSkillLevel == 0);
    assert(g_session.GetGuildSkillPoint() == -3);
    assert(g_session.GetGuildSkillItems().size() == 1);
    const PLAYER_SKILL_INFO& duplicate = g_session.GetGuildSkillItems().front();
    assert(duplicate.m_isValid == 1 && duplicate.SKID == 10003);
    assert(duplicate.type == 0x11223344); // duplicate add preserves original fields other than level/SP/range/upgradable
    assert(duplicate.level == 8 && duplicate.spcost == 13 && duplicate.upgradable == 0);
    assert(duplicate.skillIdName == "GD_GUARDUP" && duplicate.skillName == "GD_GUARDUP");
    assert(g_session.m_invalidationCount == 1 && g_session.m_lastInvalidatedGuildSkillPoint == -3);

    std::vector<u8> invalidOnly(43, 0);
    Put16(invalidOnly, 2, static_cast<u16>(invalidOnly.size()));
    Put16(invalidOnly, 4, 9);
    PutRecord(invalidOnly, 6, 9999, 0, 1, 2, 1);
    HandleGuildSkillInfo(mode, PacketView{0x0162, static_cast<u16>(invalidOnly.size()), invalidOnly.data()});
    assert(g_session.GetGuildSkillItems().empty());
    assert(g_session.GetGuildSkillPoint() == 9);
    assert(g_session.m_invalidationCount == 2 && g_session.m_lastInvalidatedGuildSkillPoint == 9);

    std::vector<u8> noRecords(42, 0);
    Put16(noRecords, 2, static_cast<u16>(noRecords.size()));
    Put16(noRecords, 4, 11);
    HandleGuildSkillInfo(mode, PacketView{0x0162, static_cast<u16>(noRecords.size()), noRecords.data()});
    assert(g_session.GetGuildSkillItems().empty() && g_session.GetGuildSkillPoint() == 11);
    assert(g_session.m_invalidationCount == 3 && g_session.m_lastInvalidatedGuildSkillPoint == 11);

    const int previousPoints = g_session.GetGuildSkillPoint();
    HandleGuildSkillInfo(mode, PacketView{0x0162, 5, noRecords.data()});
    HandleGuildSkillInfo(mode, PacketView{0x0162, 6, nullptr});
    assert(g_session.GetGuildSkillItems().empty() && g_session.GetGuildSkillPoint() == previousPoints);
    assert(g_session.m_invalidationCount == 3);
    return 0;
}
