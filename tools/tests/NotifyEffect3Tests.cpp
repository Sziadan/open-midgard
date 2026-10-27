#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <unordered_map>
#include <utility>
#include <vector>

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using s32 = std::int32_t;

struct vector3d {
    float x;
    float y;
    float z;
};

struct PacketView {
    u16 packetId;
    u16 packetLength;
    const u8* data;
};

struct MessageCall {
    int message;
    s32 value;
    s32 color;
    int kind;
};

class CGameActor {
public:
    u32 m_aid = 0;
    u16 m_Hp = 0;
    u16 m_Sp = 0;
    std::vector<MessageCall> messages;
    std::vector<int> effects;

    void SendMsg(CGameActor*, int message, s32 value, s32 color, int kind)
    {
        messages.push_back({message, value, color, kind});
    }

    bool LaunchEffect(int effectId, vector3d delta, float rotation)
    {
        assert(delta.x == 0.0f && delta.y == 0.0f && delta.z == 0.0f);
        assert(rotation == 0.0f);
        effects.push_back(effectId);
        return true;
    }
};

class CWorld {
public:
    CGameActor* m_player = nullptr;
    std::unordered_map<u32, CGameActor*> actors;

    CGameActor* GetGameActorByAID(u32 aid) const
    {
        const auto it = actors.find(aid);
        return it == actors.end() ? nullptr : it->second;
    }
};

class CGameMode {
public:
    CWorld* m_world = nullptr;
};

struct CSession {
    u32 m_aid = 0;
};

CSession g_session;
int g_statusInvalidationCount = 0;

constexpr u32 kStatusHp = 5;
constexpr u32 kStatusSp = 7;

u32 ReadLE32(const u8* data)
{
    return static_cast<u32>(data[0])
        | (static_cast<u32>(data[1]) << 8)
        | (static_cast<u32>(data[2]) << 16)
        | (static_cast<u32>(data[3]) << 24);
}

bool ApplySelfStatusUpdate(CGameMode& mode, u32 statusType, u32 value)
{
    if (!mode.m_world || !mode.m_world->m_player) {
        return false;
    }
    if (statusType == kStatusHp) {
        mode.m_world->m_player->m_Hp = static_cast<u16>(value);
        return true;
    }
    if (statusType == kStatusSp) {
        mode.m_world->m_player->m_Sp = static_cast<u16>(value);
        return true;
    }
    return false;
}

void InvalidateBasicInfoWindow()
{
    ++g_statusInvalidationCount;
}

#include "NotifyEffect3Body.inc"

void Put32(std::array<u8, 14>& bytes, size_t offset, u32 value)
{
    bytes[offset] = static_cast<u8>(value & 0xFF);
    bytes[offset + 1] = static_cast<u8>((value >> 8) & 0xFF);
    bytes[offset + 2] = static_cast<u8>((value >> 16) & 0xFF);
    bytes[offset + 3] = static_cast<u8>((value >> 24) & 0xFF);
}

PacketView MakePacket(std::array<u8, 14>& bytes, u32 actorId, u32 effectId, u32 value)
{
    bytes[0] = 0x84;
    bytes[1] = 0x02;
    Put32(bytes, 2, actorId);
    Put32(bytes, 6, effectId);
    Put32(bytes, 10, value);
    return PacketView{0x0284, 14, bytes.data()};
}

void CheckDisplay(CGameMode& mode, CGameActor& actor, u32 actorId, u32 effectId, s32 value, u32 color)
{
    std::array<u8, 14> bytes{};
    const PacketView packet = MakePacket(bytes, actorId, effectId, static_cast<u32>(value));
    HandleNotifyEffect3(mode, packet);
    assert(actor.messages.size() == 1);
    assert(actor.messages[0].message == 88);
    assert(actor.messages[0].value == value);
    assert(static_cast<u32>(actor.messages[0].color) == color);
    assert(actor.messages[0].kind == 114);
    assert(actor.effects.size() == 1 && actor.effects[0] == static_cast<int>(effectId));
    actor.messages.clear();
    actor.effects.clear();
}

int main()
{
    CGameActor player;
    player.m_aid = 42;
    player.m_Hp = 100;
    player.m_Sp = 30;
    CWorld world;
    world.m_player = &player;
    world.actors.emplace(42, &player);
    CGameMode mode;
    mode.m_world = &world;
    g_session.m_aid = 42;

    int expectedNextRand = 0;
    std::srand(179);
    for (int index = 0; index < 22; ++index) {
        std::rand();
    }
    expectedNextRand = std::rand();
    std::srand(179);
    std::array<u8, 14> bytes{};
    HandleNotifyEffect3(mode, MakePacket(bytes, 42, 331, 25));
    assert(player.m_Hp == 125);
    assert(g_statusInvalidationCount == 1);
    assert(player.messages.size() == 1 && player.messages[0].value == 25);
    assert(static_cast<u32>(player.messages[0].color) == 0xFF00FF00u);
    assert(player.messages[0].kind == 114);
    assert(player.effects.size() == 1 && player.effects[0] == 331);
    assert(std::rand() == expectedNextRand);
    player.messages.clear();
    player.effects.clear();

    std::srand(391);
    for (int index = 0; index < 17; ++index) {
        std::rand();
    }
    expectedNextRand = std::rand();
    std::srand(391);
    HandleNotifyEffect3(mode, MakePacket(bytes, 42, 332, 7));
    assert(player.m_Sp == 37);
    assert(g_statusInvalidationCount == 2);
    assert(static_cast<u32>(player.messages[0].color) == 0xFF0000FFu);
    assert(player.effects[0] == 332);
    assert(std::rand() == expectedNextRand);
    player.messages.clear();
    player.effects.clear();

    CGameActor remote;
    remote.m_aid = 77;
    world.actors.emplace(77, &remote);
    const size_t playerMessageCount = player.messages.size();
    const size_t playerEffectCount = player.effects.size();
    HandleNotifyEffect3(mode, MakePacket(bytes, 999, 331, 12));
    assert(player.messages.size() == playerMessageCount);
    assert(player.effects.size() == playerEffectCount);
    CheckDisplay(mode, remote, 77, 331, 12, 0xFF00FF00u);
    assert(remote.m_Hp == 0);

    const std::array<std::pair<u32, u32>, 9> colors = {{
        {657, 0xFF00FF00u}, {658, 0xFF0000FFu}, {659, 0xFFFF0000u},
        {660, 0xFFFF00FFu}, {661, 0xFF000000u}, {662, 0xFFFFFFFFu},
        {663, 0xFFFFFF00u}, {664, 0xFFFF55B1u}, {999, 0xFF0000FFu},
    }};
    for (const auto& [effectId, color] : colors) {
        CheckDisplay(mode, remote, 77, effectId, 3, color);
    }

    const int messageCount = static_cast<int>(player.messages.size());
    std::array<u8, 14> malformed{};
    HandleNotifyEffect3(mode, PacketView{0x0284, 13, malformed.data()});
    HandleNotifyEffect3(mode, PacketView{0x0284, 14, nullptr});
    mode.m_world = nullptr;
    HandleNotifyEffect3(mode, PacketView{0x0284, 14, malformed.data()});
    assert(static_cast<int>(player.messages.size()) == messageCount);
    assert(g_statusInvalidationCount == 2);
    return 0;
}
