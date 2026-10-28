#include "gamemode/GameModePacket.h"
#include "network/GronPacket.h"

#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

struct ActionMessage {
    int id = 0;
    int payload[10]{};
};

class CGameActor {
public:
    explicit CGameActor(u32 gid) : m_gid(gid) {}
    void SendMsg(CGameActor*, int message, msgparam_t payload, msgparam_t, msgparam_t)
    {
        ActionMessage entry;
        entry.id = message;
        const int* values = reinterpret_cast<const int*>(payload);
        for (int i = 0; i < 10; ++i) {
            entry.payload[i] = values[i];
        }
        m_messages.push_back(entry);
    }

    u32 m_gid;
    std::vector<ActionMessage> m_messages;
};

class CWorld {
public:
    CGameActor* m_player = nullptr;
    std::vector<CGameActor*> actors;
    CGameActor* GetGameActorByAID(u32 gid)
    {
        for (CGameActor* actor : actors) {
            if (actor && actor->m_gid == gid) {
                return actor;
            }
        }
        return nullptr;
    }
};

class CGameMode {
public:
    CWorld* m_world = nullptr;
};

struct ReplaySession {
    u32 m_aid = 0;
} g_session;

u16 ReadLE16(const u8* bytes)
{
    return static_cast<u16>(bytes[0]) | (static_cast<u16>(bytes[1]) << 8);
}

u32 ReadLE32(const u8* bytes)
{
    return static_cast<u32>(bytes[0]) | (static_cast<u32>(bytes[1]) << 8)
        | (static_cast<u32>(bytes[2]) << 16) | (static_cast<u32>(bytes[3]) << 24);
}

bool SessionIsAttack(int)
{
    return false;
}

CGameActor* FindActorByPacketGid(CWorld& world, u32 gid)
{
    return world.GetGameActorByAID(gid);
}

namespace ro::net {
s16 GetPacketSize(u16 packetId)
{
    switch (packetId) {
    case 0x008A: return 29;
    case 0x0ABC: return kVariablePacketSize;
    default: return 0;
    }
}

bool IsActiveActorActionNotifyExtendedPacket(u16)
{
    return false;
}
}

#include "ReplayPacketCore.inc"

namespace {

void Require(bool condition, const std::string& message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::vector<u8> DecodeHex(const std::string& text)
{
    Require(text.size() % 2 == 0, "packet hex field has odd length");
    std::vector<u8> bytes;
    bytes.reserve(text.size() / 2);
    for (std::size_t i = 0; i < text.size(); i += 2) {
        const unsigned long value = std::stoul(text.substr(i, 2), nullptr, 16);
        Require(value <= 0xFF, "packet byte is out of range");
        bytes.push_back(static_cast<u8>(value));
    }
    return bytes;
}

void CheckRejected(const std::vector<u8>& bytes, const char* description)
{
    PacketView packet{};
    int consumed = -1;
    Require(!TryReadPacket(bytes.empty() ? nullptr : bytes.data(), static_cast<int>(bytes.size()), packet, consumed),
        std::string("accepted ") + description);
    Require(consumed == 0, std::string("consumed bytes for ") + description);
    Require(packet.data == nullptr && packet.packetLength == 0, std::string("left output set for ") + description);
}

void CheckMalformedAndUnknownPackets()
{
    CheckRejected({}, "empty packet");
    CheckRejected({0x8A}, "short fixed header");
    CheckRejected({0xFF, 0xFF}, "unknown opcode");
    CheckRejected({0xBC, 0x0A, 0x03, 0x00}, "invalid variable packet length");
    CheckRejected({0xBC, 0x0A, 0x06, 0x00, 0x01}, "truncated variable packet");
}

void ReplayScenario(const std::string& path)
{
    std::ifstream input(path);
    Require(static_cast<bool>(input), "cannot open replay scenario: " + path);

    CGameModePacketRouter router;
    std::vector<std::string> log;
    CWorld world;
    CGameActor player(0x04030201u);
    world.m_player = &player;
    CGameMode gameMode;
    gameMode.m_world = &world;
    g_session.m_aid = player.m_gid;
    router.Register(0x008A, [&log](CGameMode& mode, const PacketView& packet) {
        HandleActorActionNotify(mode, packet);
        log.push_back("packet opcode=0x008A bytes=29 tick=24");
    });

    std::string line;
    int lineNumber = 0;
    int tick = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::istringstream fields(line);
        std::string opcodeText;
        std::string hexBytes;
        std::string comment;
        Require(static_cast<bool>(std::getline(fields, opcodeText, '\t'))
            && static_cast<bool>(std::getline(fields, hexBytes, '\t'))
            && static_cast<bool>(std::getline(fields, comment)),
            "scenario row must contain opcode, hexBytes, comment at line " + std::to_string(lineNumber));

        const unsigned long declaredOpcode = std::stoul(opcodeText, nullptr, 0);
        Require(declaredOpcode <= 0xFFFF, "opcode out of range at line " + std::to_string(lineNumber));
        const std::vector<u8> bytes = DecodeHex(hexBytes);
        PacketView packet{};
        int consumed = 0;
        Require(TryReadPacket(bytes.data(), static_cast<int>(bytes.size()), packet, consumed),
            "packet decode failed at line " + std::to_string(lineNumber) + ": " + comment);
        Require(packet.packetId == declaredOpcode, "TSV opcode differs from packet bytes at line " + std::to_string(lineNumber));
        Require(consumed == static_cast<int>(bytes.size()), "scenario row contains trailing bytes at line " + std::to_string(lineNumber));
        Require(router.Dispatch(gameMode, packet), "no replay handler for opcode at line " + std::to_string(lineNumber));
        tick += 24;
    }

    Require(tick == 24, "Bash scenario must currently contain one packet row");
    Require(log == std::vector<std::string>{"packet opcode=0x008A bytes=29 tick=24"},
        "unexpected deterministic router log");
    Require(player.m_messages.size() == 1, "actor-action handler did not emit its WBA message");
    const ActionMessage& message = player.m_messages.front();
    Require(message.id == 11, "actor-action handler emitted the wrong WBA message id");
    Require(message.payload[0] == 0x1615 && message.payload[1] == 0x1B1A,
        "actor-action damage fields did not match packet bytes");
    Require(message.payload[2] == 0x19 && message.payload[3] == 0x08070605,
        "actor-action action/target fields did not match packet bytes");
    Require(message.payload[4] == 0x0C0B0A09 && message.payload[5] == 0x1817,
        "actor-action start/count fields did not match packet bytes");
    Require(message.payload[6] == -1 && message.payload[7] == -1,
        "actor-action default cell fields changed");
    Require(message.payload[8] == 0x100F0E0D && message.payload[9] == 0x14131211,
        "actor-action motion timings did not match packet bytes");
}

} // namespace

int main(int argc, char** argv)
{
    try {
        Require(argc == 2, "expected the Bash replay TSV path");
        CheckMalformedAndUnknownPackets();
        ReplayScenario(argv[1]);
        std::cout << "skill packet replay parser/router foundation passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "skill packet replay check failed: " << error.what() << '\n';
        return 1;
    }
}
