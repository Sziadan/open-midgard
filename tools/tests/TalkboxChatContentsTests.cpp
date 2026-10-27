#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <unordered_map>

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using msgparam_t = std::intptr_t;

struct PacketView {
    u16 packetId;
    u16 packetLength;
    const u8* data;
};

class CSkill {
public:
    u32 m_aid = 0;
    int message = -1;
    msgparam_t arg1 = 0;
    msgparam_t arg2 = -1;
    msgparam_t arg3 = -1;
    int calls = 0;

    void SendMsg(CSkill*, int messageId, msgparam_t first, msgparam_t second, msgparam_t third)
    {
        message = messageId;
        arg1 = first;
        arg2 = second;
        arg3 = third;
        ++calls;
    }
};

class CWorld {
public:
    std::unordered_map<u32, CSkill*> skills;

    CSkill* GetSkillByAID(u32 aid)
    {
        const auto found = skills.find(aid);
        return found == skills.end() ? nullptr : found->second;
    }
};

class CGameMode {
public:
    CWorld* m_world = nullptr;
};

u32 ReadLE32(const u8* data)
{
    return static_cast<u32>(data[0])
        | (static_cast<u32>(data[1]) << 8)
        | (static_cast<u32>(data[2]) << 16)
        | (static_cast<u32>(data[3]) << 24);
}

#include "TalkboxChatContentsBody.inc"

int main()
{
    std::array<u8, 86> bytes{};
    bytes[0] = 0x91;
    bytes[1] = 0x01;
    bytes[2] = 0x78;
    bytes[3] = 0x56;
    bytes[4] = 0x34;
    bytes[5] = 0x12;
    bytes[6] = 'h';
    bytes[7] = 'i';
    bytes[8] = 0;

    CSkill skill;
    skill.m_aid = 0x12345678;
    CWorld world;
    world.skills.emplace(skill.m_aid, &skill);
    CGameMode mode;
    mode.m_world = &world;

    HandleTalkboxChatContents(mode, PacketView{0x0191, 86, bytes.data()});
    assert(skill.calls == 1);
    assert(skill.message == 7);
    assert(skill.arg1 == reinterpret_cast<msgparam_t>(bytes.data() + 6));
    assert(reinterpret_cast<const char*>(skill.arg1)[0] == 'h');
    assert(reinterpret_cast<const char*>(skill.arg1)[1] == 'i');
    assert(skill.arg2 == 0 && skill.arg3 == 0);

    HandleTalkboxChatContents(mode, PacketView{0x0191, 86, bytes.data()});
    assert(skill.calls == 2);
    bytes[2] = 0x99;
    HandleTalkboxChatContents(mode, PacketView{0x0191, 86, bytes.data()});
    assert(skill.calls == 2);
    HandleTalkboxChatContents(mode, PacketView{0x0191, 85, bytes.data()});
    HandleTalkboxChatContents(mode, PacketView{0x0191, 86, nullptr});
    mode.m_world = nullptr;
    HandleTalkboxChatContents(mode, PacketView{0x0191, 86, bytes.data()});
    assert(skill.calls == 2);
    return 0;
}
