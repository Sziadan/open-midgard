#include <array>
#include <cassert>
#include <cstdint>
#include <unordered_map>

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using s16 = std::int16_t;
using u32 = std::uint32_t;

struct PacketView {
    u16 packetId;
    u16 packetLength;
    const u8* data;
};

class CGameActor {
public:
    int m_isPc = 0;
    int m_job = 0;
    int m_gdid = 0;
    int m_emblemVersion = 0;

    void SetGuildInfo(int guildId, int emblemVersion);
};

class CWorld {
public:
    std::unordered_map<u32, CGameActor*> actors;

    CGameActor* GetGameActorByAID(u32 aid)
    {
        const auto found = actors.find(aid);
        return found == actors.end() ? nullptr : found->second;
    }
};

class CGameMode {
public:
    CWorld* m_world = nullptr;
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

#include "SetGuildInfoBody.inc"
#include "ChangeGuildPacketBody.inc"

void Put16(std::array<u8, 12>& bytes, size_t offset, u16 value)
{
    bytes[offset] = static_cast<u8>(value & 0xFF);
    bytes[offset + 1] = static_cast<u8>((value >> 8) & 0xFF);
}

void Put32(std::array<u8, 12>& bytes, size_t offset, u32 value)
{
    bytes[offset] = static_cast<u8>(value & 0xFF);
    bytes[offset + 1] = static_cast<u8>((value >> 8) & 0xFF);
    bytes[offset + 2] = static_cast<u8>((value >> 16) & 0xFF);
    bytes[offset + 3] = static_cast<u8>((value >> 24) & 0xFF);
}

int main()
{
    CGameActor pc;
    pc.m_isPc = 1;
    pc.m_job = 4001;
    CGameActor ordinaryMonster;
    ordinaryMonster.m_job = 1002;
    CGameActor guardian;
    guardian.m_job = 1286;
    CGameActor emperium;
    emperium.m_job = 722;
    CWorld world;
    world.actors.emplace(10, &pc);
    world.actors.emplace(11, &ordinaryMonster);
    world.actors.emplace(12, &guardian);
    world.actors.emplace(13, &emperium);
    CGameMode mode;
    mode.m_world = &world;

    std::array<u8, 12> bytes{};
    bytes[0] = 0xB4;
    bytes[1] = 0x01;
    Put32(bytes, 2, 10);
    Put32(bytes, 6, 50);
    Put16(bytes, 10, 7);
    HandleChangeGuild(mode, PacketView{0x01B4, 12, bytes.data()});
    assert(pc.m_gdid == 50 && pc.m_emblemVersion == 7);

    Put32(bytes, 2, 11);
    Put32(bytes, 6, 60);
    Put16(bytes, 10, 4);
    HandleChangeGuild(mode, PacketView{0x01B4, 12, bytes.data()});
    assert(ordinaryMonster.m_gdid == 0 && ordinaryMonster.m_emblemVersion == 0);

    Put32(bytes, 2, 12);
    HandleChangeGuild(mode, PacketView{0x01B4, 12, bytes.data()});
    assert(guardian.m_gdid == 60 && guardian.m_emblemVersion == 4);

    Put32(bytes, 2, 13);
    Put16(bytes, 10, 0);
    HandleChangeGuild(mode, PacketView{0x01B4, 12, bytes.data()});
    assert(emperium.m_gdid == 60 && emperium.m_emblemVersion == 0);

    Put32(bytes, 2, 10);
    Put32(bytes, 6, 0);
    Put16(bytes, 10, 8);
    HandleChangeGuild(mode, PacketView{0x01B4, 12, bytes.data()});
    assert(pc.m_gdid == 50 && pc.m_emblemVersion == 7);

    mode.m_world = nullptr;
    HandleChangeGuild(mode, PacketView{0x01B4, 12, bytes.data()});
    HandleChangeGuild(mode, PacketView{0x01B4, 11, bytes.data()});
    HandleChangeGuild(mode, PacketView{0x01B4, 12, nullptr});
    assert(pc.m_gdid == 50 && pc.m_emblemVersion == 7);
    return 0;
}
