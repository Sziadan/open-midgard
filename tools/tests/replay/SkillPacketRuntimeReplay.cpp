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

    static CMode* Current() { return g_modeMgr.m_curMode; }
    static int CurrentType() { return g_modeMgr.m_curModeType; }
    static void Restore(CMode* mode, int type)
    {
        g_modeMgr.m_curMode = mode;
        g_modeMgr.m_curModeType = type;
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
    CMode* oldMode;
    const int oldModeType;

    Fixture()
        : target(), world(g_world), oldPlayer(g_world.m_player), oldAid(g_session.m_aid),
          oldMode(SkillPacketReplayAccess::Current()), oldModeType(SkillPacketReplayAccess::CurrentType())
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
        SkillPacketReplayAccess::Restore(oldMode, oldModeType);
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
        const std::size_t expectedLength = expectedOpcode == 0x011A ? 15u : 31u;
        Require(bytes.size() == expectedLength, "scenario packet length does not match its active opcode layout");
        Require(input.peek() == std::char_traits<char>::eof(), "scenario must contain one packet");
        return bytes;
    }
    throw std::runtime_error("scenario contains no packets");
}

void ReplayScenario(const std::string& path)
{
    u16 expectedOpcode = 0;
    const std::vector<u8> bytes = LoadOnePacket(path, expectedOpcode);
    const u16 skillId = static_cast<u16>(bytes[2]) | (static_cast<u16>(bytes[3]) << 8);
    Require(skillId == 5 || skillId == 19 || skillId == 28 || skillId == 46,
        "runtime replay only has Ref assertions for skills 5, 19, 28, and 46");
    Fixture fixture;

    CGameModePacketRouter router;
    RegisterDefaultGameModePacketHandlers(router);

    PacketView packet{};
    int consumed = 0;
    Require(TryReadPacket(bytes.data(), static_cast<int>(bytes.size()), packet, consumed),
        "production packet reader rejected Bash fixture");
    Require(packet.packetId == expectedOpcode && (packet.packetId == 0x0114 || packet.packetId == 0x011A),
        "scenario opcode does not match an active skill packet route");
    Require(consumed == static_cast<int>(bytes.size()), "packet reader left trailing scenario bytes");
    Require(router.Dispatch(fixture.mode, packet), "default client router did not dispatch skill packet");
    const u32 replayStartTick = GetTickCount();

    const std::size_t expectedWbaCount = skillId == 46 ? 2u : (skillId == 28 ? 0u : 1u);
    Require(fixture.target.m_willBeAttackedList.size() == expectedWbaCount,
        "skill packet enqueued an unexpected number of target WBAs");
    std::vector<WBA> queuedHits(fixture.target.m_willBeAttackedList.begin(),
        fixture.target.m_willBeAttackedList.end());
    for (const WBA& hit : queuedHits) {
        Require(hit.sourceSkillId == skillId && hit.damage == 100 && hit.effectLevel == 5,
            "WBA skill, per-hit damage, or level differs from packet/Ref fields");
    }
    if (skillId == 5) {
        Require(queuedHits[0].message == 96 && queuedHits[0].effectId1 == -1 && queuedHits[0].effectId2 == -1,
            "Bash WBA message/effect fields differ from Ref skill action");
    } else if (skillId == 19) {
        Require(queuedHits[0].message == 39 && queuedHits[0].effectId1 == 49 && queuedHits[0].effectId2 == -1,
            "Fire Bolt WBA message/effect fields differ from Ref action-4 target-skill path");
    } else if (skillId == 46) {
        Require(queuedHits[0].message == 111 && queuedHits[0].effectId1 == 0 && queuedHits[0].effectId2 == 1,
            "Double Strafe first WBA differs from Ref action-9 path");
        Require(queuedHits[1].message == 111 && queuedHits[1].effectId1 == 0 && queuedHits[1].effectId2 == -1,
            "Double Strafe follow-up WBA differs from Ref multi-hit path");
        Require(queuedHits[1].time - queuedHits[0].time == 200,
            "Double Strafe follow-up WBA delay differs from executable doubleAttackTerm");
    }

    std::vector<u32> expectedDelays;
    for (const WBA& hit : queuedHits) {
        expectedDelays.push_back(hit.time - replayStartTick);
    }

    std::vector<u32> deliveredAtElapsedMs;
    std::vector<u32> deliveryStepMs;
    std::size_t previousQueueSize = fixture.target.m_willBeAttackedList.size();
    u32 previousStepElapsedMs = 0;
    for (unsigned int worldTick = 24; !fixture.target.m_willBeAttackedList.empty() && worldTick <= 2000; worldTick += 24) {
        std::this_thread::sleep_for(std::chrono::milliseconds(24));
        fixture.target.ProcessWillBeAttacked();
        const u32 elapsedMs = GetTickCount() - replayStartTick;
        const u32 stepMs = elapsedMs - previousStepElapsedMs;
        const std::size_t currentQueueSize = fixture.target.m_willBeAttackedList.size();
        while (previousQueueSize > currentQueueSize) {
            deliveredAtElapsedMs.push_back(elapsedMs);
            deliveryStepMs.push_back(stepMs);
            --previousQueueSize;
        }
        previousStepElapsedMs = elapsedMs;
    }
    Require(fixture.target.m_willBeAttackedList.empty(), "skill WBA queue was not fully delivered");
    Require(deliveredAtElapsedMs.size() == expectedDelays.size(),
        "WBA delivery count differs from expected real-time 24 ms polling steps");
    for (std::size_t index = 0; index < deliveredAtElapsedMs.size(); ++index) {
        Require(deliveredAtElapsedMs[index] >= expectedDelays[index]
            && deliveredAtElapsedMs[index] <= expectedDelays[index] + deliveryStepMs[index] + 24,
            "WBA delivery elapsed time outside polling-step window: delay=" + std::to_string(expectedDelays[index])
                + " step=" + std::to_string(deliveryStepMs[index])
                + " actual=" + std::to_string(deliveredAtElapsedMs[index]));
    }

    const std::string logPath = "debug_hp_" + std::to_string(GetCurrentProcessId()) + ".log";
    std::ifstream runtimeLog(logPath);
    Require(static_cast<bool>(runtimeLog), "production runtime log was not created");
    std::string logLine;
    struct WbaLogEntry {
        int message;
        int skill;
        int effect1;
        int effect2;
        unsigned int delivery;
        int damage;
        int level;
        std::string wave;
    };
    std::vector<WbaLogEntry> wbaEvents;
    while (std::getline(runtimeLog, logLine)) {
        unsigned long processId = 0;
        unsigned long logTick = 0;
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
            wbaEvents.push_back(WbaLogEntry{
                parsedMessage, parsedSkill, parsedEffect1, parsedEffect2,
                delivery, parsedDamage, parsedLevel, parsedWave });
        }
    }
    Require(wbaEvents.size() == expectedWbaCount, "runtime log has an unexpected number of WBA events");
    for (std::size_t index = 0; index < wbaEvents.size(); ++index) {
        const WbaLogEntry& event = wbaEvents[index];
        const char* expectedWave = skillId == 5 ? "_enemy_hit_normal2.wav" : "player_clothes.wav";
        Require(event.skill == skillId && event.damage == 100 && event.level == 5
            && event.wave == expectedWave,
            "production WBA log skill/damage/level/wave differs from Ref walk-through");
        if (skillId == 5) {
            Require(event.message == 96 && event.effect1 == -1 && event.effect2 == -1,
                "Bash production WBA log differs from Ref walk-through");
        } else if (skillId == 19) {
            Require(event.message == 39 && event.effect1 == 49 && event.effect2 == -1,
                "Fire Bolt production WBA log differs from Ref action-4 path");
        } else if (index == 0) {
            Require(event.message == 111 && event.effect1 == 0 && event.effect2 == 1,
                "Double Strafe first production WBA log differs from Ref walk-through");
        } else {
            Require(event.message == 111 && event.effect1 == 0 && event.effect2 == -1,
                "Double Strafe second production WBA log differs from Ref walk-through");
        }
        Require(event.delivery == queuedHits[index].time,
            "runtime WBA log delivery timestamp differs from queued hit");
        std::cout << "WBA msg=" << event.message
            << " skill=" << event.skill
            << " effectId1=" << event.effect1
            << " effectId2=" << event.effect2
            << " wave=" << event.wave
            << " deliveredElapsedMs=" << deliveredAtElapsedMs[index]
            << " (runtime=" << event.delivery << ")\n";
    }

    if (skillId == 28) {
        runtimeLog.clear();
        runtimeLog.seekg(0);
        bool foundHealEffect = false;
        bool foundHealNumber = false;
        while (std::getline(runtimeLog, logLine)) {
            foundHealEffect = foundHealEffect || logLine.find("[REPLAY] LaunchEffect id=7 actor=2") != std::string::npos;
            foundHealNumber = foundHealNumber || logLine.find("[REPLAY] SendMsg88 actor=2 value=199 color=-16711936 kind=114") != std::string::npos;
        }
        Require(foundHealEffect, "Heal 28 low-tier packet did not launch Ref effect7 on target");
        Require(foundHealNumber, "Heal 28 packet did not send Ref damage-number message88");
        std::cout << "Heal skill=28 value=199 effect=7 target=2 message=88 color=-16711936 kind=114\n";
    }
}

} // namespace

int main(int argc, char** argv)
{
    try {
        Require(argc == 2, "expected one replay scenario path");
        ReplayScenario(argv[1]);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "production skill replay failed: " << error.what() << '\n';
        return 1;
    }
}
