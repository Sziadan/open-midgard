#include "Types.h"
#include "gamemode/GameMode.h"
#include "gamemode/GameModePacket.h"
#include "gamemode/Mode.h"
#include "network/GronPacket.h"
#include "session/Session.h"
#include "world/GameActor.h"
#include "world/World.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

struct SkillPacketReplayAccess {
    static void SetCurrent(CGameMode* mode)
    {
        g_modeMgr.m_curMode = mode;
        g_modeMgr.m_curModeType = mode ? 1 : 0;
    }
};

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

struct Fixture {
    CGameMode mode;
    CPlayer source;
    CGameActor target;
    CWorld& world;
    CPlayer* oldPlayer;
    const u32 oldAid;

    Fixture()
        : target(), world(g_world), oldPlayer(g_world.m_player), oldAid(g_session.m_aid)
    {
        source.m_gid = 1;
        source.m_attackMotion = 1.0f;
        source.m_motionSpeed = 1.0f;
        target.m_gid = 2;
        target.m_pos = vector3d{ 1.0f, 0.0f, 0.0f };
        g_session.m_aid = source.m_gid;
        world.m_player = &source;
        world.RegisterActor(&target);
        mode.m_world = &world;
        SkillPacketReplayAccess::SetCurrent(&mode);
    }

    ~Fixture()
    {
        SkillPacketReplayAccess::SetCurrent(nullptr);
        world.UnregisterActor(&target);
        world.m_player = oldPlayer;
        g_session.m_aid = oldAid;
    }
};

std::vector<u8> LoadOnePacket(const std::string& path, u16& expectedOpcode)
{
    std::ifstream input(path);
    Require(static_cast<bool>(input), "cannot open scenario: " + path);
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::istringstream fields(line);
        std::string opcode;
        std::string hexBytes;
        std::string comment;
        Require(static_cast<bool>(std::getline(fields, opcode, '\t'))
            && static_cast<bool>(std::getline(fields, hexBytes, '\t'))
            && static_cast<bool>(std::getline(fields, comment)),
            "scenario row must contain opcode, hexBytes, comment");
        const unsigned long parsedOpcode = std::stoul(opcode, nullptr, 0);
        Require(parsedOpcode <= 0xFFFF, "scenario opcode out of range");
        expectedOpcode = static_cast<u16>(parsedOpcode);
        std::vector<u8> bytes = DecodeHex(hexBytes);
        Require(bytes.size() == 31, "Bash 5 skill-damage fixture must be 31 bytes");
        Require(input.peek() == std::char_traits<char>::eof(), "Bash scenario must contain one packet");
        return bytes;
    }
    throw std::runtime_error("scenario contains no packets");
}

void ReplayBash(const std::string& path)
{
    u16 expectedOpcode = 0;
    const std::vector<u8> bytes = LoadOnePacket(path, expectedOpcode);
    Fixture fixture;

    CGameModePacketRouter router;
    RegisterDefaultGameModePacketHandlers(router);

    PacketView packet{};
    int consumed = 0;
    Require(TryReadPacket(bytes.data(), static_cast<int>(bytes.size()), packet, consumed),
        "production packet reader rejected Bash fixture");
    Require(packet.packetId == expectedOpcode && packet.packetId == 0x0114,
        "Bash fixture opcode does not match active ZC_NOTIFY_SKILL route");
    Require(consumed == static_cast<int>(bytes.size()), "packet reader left trailing scenario bytes");
    Require(router.Dispatch(fixture.mode, packet), "default client router did not dispatch Bash packet");

    Require(fixture.target.m_willBeAttackedList.size() == 1,
        "Bash packet did not enqueue exactly one target WBA");
    const WBA hit = fixture.target.m_willBeAttackedList.front();
    Require(hit.sourceSkillId == 5 && hit.damage == 100 && hit.effectLevel == 5,
        "Bash WBA skill, damage, or level fields differ from packet bytes");
    Require(hit.message == 96 && hit.effectId1 == -1 && hit.effectId2 == -1,
        "Bash WBA message/effect fields differ from Ref skill action");

    std::this_thread::sleep_for(std::chrono::milliseconds(32));
    fixture.target.ProcessWillBeAttacked();
    Require(fixture.target.m_willBeAttackedList.empty(),
        "Bash WBA was not delivered after its 24 ms delay");

    const std::string logPath = "debug_hp_" + std::to_string(GetCurrentProcessId()) + ".log";
    std::ifstream runtimeLog(logPath);
    Require(static_cast<bool>(runtimeLog), "production runtime log was not created");
    std::string logLine;
    int wbaCount = 0;
    int logMessage = 0;
    int logSkill = 0;
    int logEffect1 = 0;
    int logEffect2 = 0;
    unsigned long logTick = 0;
    int logDamage = 0;
    int logLevel = 0;
    char logWave[128]{};
    while (std::getline(runtimeLog, logLine)) {
        unsigned long processId = 0;
        int parsedMessage = 0;
        int parsedSkill = 0;
        int parsedEffect1 = 0;
        int parsedEffect2 = 0;
        unsigned int delivery = 0;
        int parsedDamage = 0;
        int parsedLevel = 0;
        char parsedWave[128]{};
        if (std::sscanf(logLine.c_str(),
                "[pid:%lu t:%lu] [WBA] msg=%d skill=%d effect=%d/%d wave=%127s delivery=%u damage=%d level=%d",
                &processId,
                &logTick,
                &parsedMessage,
                &parsedSkill,
                &parsedEffect1,
                &parsedEffect2,
                parsedWave,
                &delivery,
                &parsedDamage,
                &parsedLevel) == 10
            && processId == GetCurrentProcessId()) {
            ++wbaCount;
            logMessage = parsedMessage;
            logSkill = parsedSkill;
            logEffect1 = parsedEffect1;
            logEffect2 = parsedEffect2;
            logDamage = parsedDamage;
            logLevel = parsedLevel;
            logTick = delivery;
            std::snprintf(logWave, sizeof(logWave), "%s", parsedWave);
        }
    }
    Require(wbaCount == 1, "expected exactly one production WBA event in runtime log");
    Require(logMessage == 96 && logSkill == 5 && logEffect1 == -1 && logEffect2 == -1,
        "production WBA log message/skill/effects differ from the queued event");
    Require(logDamage == 100 && logLevel == 5 && std::string(logWave) == "_enemy_hit_normal2.wav",
        "production WBA log damage/level/wave differs from the Ref walk-through");

    std::cout << "WBA msg=" << logMessage
        << " skill=" << logSkill
        << " effectId1=" << logEffect1
        << " effectId2=" << logEffect2
        << " wave=" << logWave
        << " deliveryTick=24 (runtime=" << logTick << ")\n";
}

} // namespace

int main(int argc, char** argv)
{
    try {
        Require(argc == 2, "expected one replay scenario path");
        ReplayBash(argv[1]);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "production skill replay failed: " << error.what() << '\n';
        return 1;
    }
}
