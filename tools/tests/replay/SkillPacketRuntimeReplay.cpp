#include "Types.h"
#include "gamemode/GameMode.h"
#include "gamemode/GameModePacket.h"
#include "gamemode/Mode.h"
#include "network/GronPacket.h"
#include "session/Session.h"
#include "world/GameActor.h"
#include "world/World.h"

#include <chrono>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
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

u16 ReadU16LE(const u8* bytes)
{
    return static_cast<u16>(bytes[0]) | (static_cast<u16>(bytes[1]) << 8);
}

u32 ReadU32LE(const u8* bytes)
{
    return static_cast<u32>(bytes[0])
        | (static_cast<u32>(bytes[1]) << 8)
        | (static_cast<u32>(bytes[2]) << 16)
        | (static_cast<u32>(bytes[3]) << 24);
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

struct ScenarioPacket {
    u16 opcode;
    std::vector<u8> bytes;
};

struct ScenarioActor {
    std::string role;
    u32 aid;
    int job;
    int cellX;
    int cellY;
};

std::vector<ScenarioActor> LoadScenarioActors(const std::string& path)
{
    std::ifstream input(path);
    Require(static_cast<bool>(input), "cannot open scenario actor setup: " + path);
    std::vector<ScenarioActor> actors;
    std::string line;
    while (std::getline(input, line)) {
        if (line.rfind("# actor\t", 0) != 0) {
            continue;
        }
        std::istringstream fields(line);
        std::string marker;
        std::string role;
        unsigned long aid = 0;
        int job = 0;
        int cellX = 0;
        int cellY = 0;
        Require(static_cast<bool>(std::getline(fields, marker, '\t'))
            && static_cast<bool>(std::getline(fields, role, '\t'))
            && static_cast<bool>(fields >> aid >> job >> cellX >> cellY),
            "actor setup row must be '# actor<TAB>role<TAB>aid job cellX cellY'");
        Require(aid <= 0xFFFFFFFFul, "scenario actor AID out of range");
        Require(role == "source" || role == "target" || role == "actor", "unsupported scenario actor role");
        actors.push_back(ScenarioActor{ role, static_cast<u32>(aid), job, cellX, cellY });
    }
    return actors;
}

std::vector<ScenarioPacket> LoadPacketSequence(const std::string& path)
{
    std::ifstream input(path);
    Require(static_cast<bool>(input), "cannot open packet sequence: " + path);
    std::vector<ScenarioPacket> packets;
    std::string line;
    while (std::getline(input, line)) {
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
            "packet sequence row must contain opcode, hexBytes, comment");
        const unsigned long parsedOpcode = std::stoul(opcodeText, nullptr, 0);
        Require(parsedOpcode <= 0xFFFF, "packet sequence opcode out of range");
        std::vector<u8> bytes = DecodeHex(hexBytes);
        Require(bytes.size() >= 2 && bytes[0] == (parsedOpcode & 0xFF)
            && bytes[1] == ((parsedOpcode >> 8) & 0xFF), "packet sequence opcode differs from packet header");
        packets.push_back(ScenarioPacket{ static_cast<u16>(parsedOpcode), std::move(bytes) });
    }
    Require(!packets.empty(), "packet sequence contains no packets");
    return packets;
}

struct Fixture {
    CGameMode mode;
    CPlayer source;
    CGameActor target;
    C3dAttr attr;
    std::vector<std::unique_ptr<CGameActor>> extraActors;
    CWorld& world;
    CPlayer* oldPlayer;
    C3dAttr* oldAttr;
    const u32 oldAid;
    CMode* oldMode;
    const int oldModeType;

    explicit Fixture(const std::string& scenarioPath)
        : target(), world(g_world), oldPlayer(g_world.m_player), oldAttr(g_world.m_attr), oldAid(g_session.m_aid),
          oldMode(SkillPacketReplayAccess::Current()), oldModeType(SkillPacketReplayAccess::CurrentType())
    {
        source.m_gid = 1;
        source.m_attackMotion = 1.0f;
        source.m_motionSpeed = 1.0f;
        target.m_gid = 2;
        target.m_pos = vector3d{ 1.0f, 0.0f, 0.0f };
        attr.m_width = 32;
        attr.m_height = 32;
        attr.m_zoom = 5;
        attr.m_cells.resize(32u * 32u, CAttrCell{ 7.5f, 7.5f, 7.5f, 7.5f, 0 });
        g_session.m_aid = source.m_gid;
        world.m_player = &source;
        world.m_attr = &attr;
        world.RegisterActor(&target);
        mode.m_world = &world;
        SkillPacketReplayAccess::SetCurrent(&mode);

        for (const ScenarioActor& actor : LoadScenarioActors(scenarioPath)) {
            CGameActor* configuredActor = nullptr;
            if (actor.role == "source") {
                configuredActor = &source;
                source.m_gid = actor.aid;
                g_session.m_aid = actor.aid;
            } else if (actor.role == "target") {
                configuredActor = &target;
                world.UnregisterActor(&target);
            } else {
                auto extra = std::make_unique<CGameActor>();
                configuredActor = extra.get();
                extraActors.push_back(std::move(extra));
            }
            configuredActor->m_gid = actor.aid;
            configuredActor->m_job = actor.job;
            mode.GetClientCoor(actor.cellX, actor.cellY, configuredActor->m_pos.x, configuredActor->m_pos.z);
            configuredActor->m_pos.y = attr.GetHeight(configuredActor->m_pos.x, configuredActor->m_pos.z);
            if (actor.role == "target" || actor.role == "actor") {
                world.RegisterActor(configuredActor);
            }
        }
    }

    ~Fixture()
    {
        SkillPacketReplayAccess::Restore(oldMode, oldModeType);
        for (const std::unique_ptr<CGameActor>& actor : extraActors) {
            world.UnregisterActor(actor.get());
        }
        world.UnregisterActor(&target);
        world.m_player = oldPlayer;
        world.m_attr = oldAttr;
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
        const std::size_t expectedLength = expectedOpcode == 0x011A ? 15u : (expectedOpcode == 0x0117 ? 18u : 31u);
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
    Require(skillId == 5 || skillId == 19 || skillId == 28 || skillId == 34 || skillId == 46 || skillId == 70
        || skillId == 89 || skillId == 136 || skillId == 137 || skillId == 271,
        "runtime replay only has Ref assertions for skills 5, 19, 28, 34, 46, 70, 89, 136, 137, and 271");
    Fixture fixture(path);

    CGameModePacketRouter router;
    RegisterDefaultGameModePacketHandlers(router);

    PacketView packet{};
    int consumed = 0;
    Require(TryReadPacket(bytes.data(), static_cast<int>(bytes.size()), packet, consumed),
        "production packet reader rejected Bash fixture");
    Require(packet.packetId == expectedOpcode && (packet.packetId == 0x0114 || packet.packetId == 0x0117 || packet.packetId == 0x011A),
        "scenario opcode does not match an active skill packet route");
    Require(consumed == static_cast<int>(bytes.size()), "packet reader left trailing scenario bytes");
    Require(router.Dispatch(fixture.mode, packet), "default client router did not dispatch skill packet");
    const u32 replayStartTick = timeGetTime();

    const std::size_t expectedTargetWbaCount = skillId == 46 ? 2u
        : ((skillId == 28 || skillId == 34 || skillId == 70 || skillId == 89 || skillId == 271) ? 0u : (skillId == 136 ? 4u : 1u));
    const std::size_t expectedSelfWbaCount = skillId == 136 ? 3u : 0u;
    const std::size_t expectedWbaCount = skillId == 271 ? 1u : expectedTargetWbaCount + expectedSelfWbaCount;
    Require(fixture.target.m_willBeAttackedList.size() == expectedTargetWbaCount,
        "skill packet enqueued an unexpected number of target WBAs");
    Require(fixture.source.m_willBeAttackedList.size() == expectedSelfWbaCount,
        "skill packet enqueued an unexpected number of source/self WBAs");
    std::vector<WBA> queuedHits(fixture.target.m_willBeAttackedList.begin(),
        fixture.target.m_willBeAttackedList.end());
    std::vector<WBA> queuedSelfHits(fixture.source.m_willBeAttackedList.begin(),
        fixture.source.m_willBeAttackedList.end());
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
    } else if (skillId == 136) {
        for (const WBA& hit : queuedHits) {
            Require(hit.message == 111 && hit.effectId2 == 122 && hit.damage == 100 && hit.effectLevel == 5,
                "Sonic Blow target WBA fields differ from Ref multi-hit path");
        }
        Require(queuedHits[0].effectId1 == 0 && queuedHits[1].effectId1 == 0
            && queuedHits[2].effectId1 == 0 && queuedHits[3].effectId1 == 1,
            "Sonic Blow target hit effect1 sequence differs from Ref fourth-hit branch");
        for (const WBA& hit : queuedSelfHits) {
            Require(hit.message == 108 && hit.effectId1 == 0 && hit.effectId2 == 122
                && hit.damage == 100 && hit.effectLevel == 5,
                "Sonic Blow self WBA fields differ from Ref attack-animation path");
        }
        Require(queuedHits[1].time - queuedHits[0].time == 200
            && queuedHits[2].time - queuedHits[1].time == 200
            && queuedHits[3].time - queuedHits[2].time == 200,
            "Sonic Blow target hit intervals differ from executable doubleAttackTerm");
    } else if (skillId == 137) {
        Require(queuedHits[0].message == 39 && queuedHits[0].effectId1 == 0 && queuedHits[0].effectId2 == 132,
            "Grimtooth WBA message/effect fields differ from Ref action-4 path: msg="
                + std::to_string(queuedHits[0].message) + " effects="
                + std::to_string(queuedHits[0].effectId1) + "/" + std::to_string(queuedHits[0].effectId2));
    } else if (skillId == 271) {
        Require(queuedHits.empty(), "Asura Strike must dispatch its hit immediately instead of enqueuing a WBA");
    }

    std::vector<u32> expectedDelays;
    for (const WBA& hit : queuedHits) {
        expectedDelays.push_back(hit.time - replayStartTick);
    }
    std::vector<u32> expectedSelfDelays;
    for (const WBA& hit : queuedSelfHits) {
        expectedSelfDelays.push_back(hit.time - replayStartTick);
    }

    std::vector<u32> deliveredAtElapsedMs;
    std::vector<u32> deliveryStepMs;
    std::vector<u32> selfDeliveredAtElapsedMs;
    std::vector<u32> selfDeliveryStepMs;
    std::size_t previousQueueSize = fixture.target.m_willBeAttackedList.size();
    std::size_t previousSelfQueueSize = fixture.source.m_willBeAttackedList.size();
    u32 previousStepElapsedMs = 0;
    for (unsigned int worldTick = 24;
        (!fixture.target.m_willBeAttackedList.empty() || !fixture.source.m_willBeAttackedList.empty()) && worldTick <= 3000;
        worldTick += 24) {
        std::this_thread::sleep_for(std::chrono::milliseconds(24));
        fixture.target.ProcessWillBeAttacked();
        fixture.source.ProcessWillBeAttacked();
        const u32 elapsedMs = timeGetTime() - replayStartTick;
        const u32 stepMs = elapsedMs - previousStepElapsedMs;
        const std::size_t currentQueueSize = fixture.target.m_willBeAttackedList.size();
        const std::size_t currentSelfQueueSize = fixture.source.m_willBeAttackedList.size();
        while (previousQueueSize > currentQueueSize) {
            deliveredAtElapsedMs.push_back(elapsedMs);
            deliveryStepMs.push_back(stepMs);
            --previousQueueSize;
        }
        while (previousSelfQueueSize > currentSelfQueueSize) {
            selfDeliveredAtElapsedMs.push_back(elapsedMs);
            selfDeliveryStepMs.push_back(stepMs);
            --previousSelfQueueSize;
        }
        previousStepElapsedMs = elapsedMs;
    }
    Require(fixture.target.m_willBeAttackedList.empty() && fixture.source.m_willBeAttackedList.empty(),
        "skill source/target WBA queues were not fully delivered");
    Require(deliveredAtElapsedMs.size() == expectedDelays.size(),
        "WBA delivery count differs from expected real-time 24 ms polling steps");
    for (std::size_t index = 0; index < deliveredAtElapsedMs.size(); ++index) {
        Require(deliveredAtElapsedMs[index] >= expectedDelays[index]
            && deliveredAtElapsedMs[index] <= expectedDelays[index] + deliveryStepMs[index] + 24,
            "WBA delivery elapsed time outside polling-step window: delay=" + std::to_string(expectedDelays[index])
                + " step=" + std::to_string(deliveryStepMs[index])
                + " actual=" + std::to_string(deliveredAtElapsedMs[index]));
    }
    Require(selfDeliveredAtElapsedMs.size() == expectedSelfDelays.size(),
        "source/self WBA delivery count differs from expected 24 ms polling steps");
    for (std::size_t index = 0; index < selfDeliveredAtElapsedMs.size(); ++index) {
        Require(selfDeliveredAtElapsedMs[index] >= expectedSelfDelays[index]
            && selfDeliveredAtElapsedMs[index] <= expectedSelfDelays[index] + selfDeliveryStepMs[index] + 24,
            "source/self WBA delivery elapsed time outside polling-step window");
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
        const int expectedDamage = skillId == 271 ? 1000 : 100;
        const char* expectedWave = event.message == 108 ? "(none)"
            : (skillId == 5 ? "_enemy_hit_normal2.wav" : "player_clothes.wav");
        Require(event.skill == skillId && event.damage == expectedDamage && event.level == 5
            && event.wave == expectedWave,
            "production WBA log skill/damage/level/wave differs from Ref walk-through");
        if (skillId == 5) {
            Require(event.message == 96 && event.effect1 == -1 && event.effect2 == -1,
                "Bash production WBA log differs from Ref walk-through");
        } else if (skillId == 19) {
            Require(event.message == 39 && event.effect1 == 49 && event.effect2 == -1,
                "Fire Bolt production WBA log differs from Ref action-4 path");
        } else if (skillId == 136) {
            if (event.message == 108) {
                Require(event.effect1 == 0 && event.effect2 == 122,
                    "Sonic Blow self WBA runtime trace differs from Ref");
            } else {
                Require(event.message == 111 && event.effect2 == 122,
                    "Sonic Blow target WBA runtime trace differs from Ref");
            }
        } else if (skillId == 271) {
            Require(event.message == 39 && event.effect1 == 0 && event.effect2 == -1,
                "Asura Strike immediate WBA differs from Ref action-4 path");
        } else if (skillId == 137) {
            Require(event.message == 39 && event.effect1 == 0 && event.effect2 == 132,
                "Grimtooth production WBA log differs from Ref action-4 path");
        } else if (index == 0) {
            Require(event.message == 111 && event.effect1 == 0 && event.effect2 == 1,
                "Double Strafe first production WBA log differs from Ref walk-through");
        } else {
            Require(event.message == 111 && event.effect1 == 0 && event.effect2 == -1,
                "Double Strafe second production WBA log differs from Ref walk-through");
        }
        u32 deliveredElapsedMs = 0;
        if (skillId == 271) {
            Require(event.delivery != 0, "Asura Strike runtime WBA log has no scheduled motion timestamp");
        } else if (event.message == 108) {
            const std::size_t selfIndex = static_cast<std::size_t>(std::count_if(wbaEvents.begin(),
                wbaEvents.begin() + static_cast<std::ptrdiff_t>(index),
                [](const WbaLogEntry& previous) { return previous.message == 108; }));
            Require(event.delivery == queuedSelfHits[selfIndex].time,
                "runtime self WBA log timestamp differs from queued hit");
            deliveredElapsedMs = selfDeliveredAtElapsedMs[selfIndex];
        } else {
            const std::size_t targetIndex = static_cast<std::size_t>(std::count_if(wbaEvents.begin(),
                wbaEvents.begin() + static_cast<std::ptrdiff_t>(index),
                [](const WbaLogEntry& previous) { return previous.message != 108; }));
            Require(event.delivery == queuedHits[targetIndex].time,
                "runtime target WBA log timestamp differs from queued hit");
            deliveredElapsedMs = deliveredAtElapsedMs[targetIndex];
        }
        std::cout << "WBA msg=" << event.message
            << " skill=" << event.skill
            << " effectId1=" << event.effect1
            << " effectId2=" << event.effect2
            << " wave=" << event.wave
            << " deliveredElapsedMs=" << deliveredElapsedMs
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
    } else if (skillId == 34) {
        runtimeLog.clear();
        runtimeLog.seekg(0);
        bool foundBlessingEffect = false;
        while (std::getline(runtimeLog, logLine)) {
            foundBlessingEffect = foundBlessingEffect || logLine.find("[REPLAY] LaunchEffect id=42 actor=2") != std::string::npos;
        }
        Require(foundBlessingEffect, "Blessing 34 success packet did not launch Ref effect42 on target");
        std::cout << "Buff skill=34 effect=42 target=2\n";
    } else if (skillId == 70 || skillId == 89) {
        runtimeLog.clear();
        runtimeLog.seekg(0);
        const int expectedEffect = skillId == 70 ? 83 : 89;
        const std::string expectedPlace = "[REPLAY] PlaceEffect id=" + std::to_string(expectedEffect)
            + " lastTime=";
        bool foundGroundEffect = false;
        while (std::getline(runtimeLog, logLine)) {
            if (logLine.find(expectedPlace) != std::string::npos) {
                const float expectedX = (10.0f - 16.0f) * 5.0f + 2.5f;
                const float expectedZ = (20.0f - 16.0f) * 5.0f + 2.5f;
                std::ostringstream expectedPosition;
                expectedPosition << std::fixed << std::setprecision(3)
                    << "extra=0 pos=(" << expectedX << ",7.500," << expectedZ << ")";
                foundGroundEffect = logLine.find(expectedPosition.str()) != std::string::npos;
            }
        }
        Require(foundGroundEffect, "ground-skill packet did not place the Ref-mapped effect at the packet cell and attr height");
        std::cout << "GroundSkill skill=" << skillId << " effect=" << expectedEffect << " cell=(10,20) pos=(-27.500,7.500,22.500)\n";
    }
}

void ReplaySkillCastCancel(const std::string& path)
{
    const std::vector<ScenarioPacket> packets = LoadPacketSequence(path);
    Require(packets.size() == 1 && packets[0].opcode == 0x01B9 && packets[0].bytes.size() == 6,
        "skill cast cancel scenario requires one 6-byte 0x01B9 packet");
    Fixture fixture(path);
    fixture.source.SendMsg(&fixture.source, 85, 7, 1000, 0);
    fixture.source.SendMsg(&fixture.source, 86, 49, 1000, 0);
    Require(fixture.source.m_beginSpellEffect != nullptr && fixture.source.m_magicTargetEffect != nullptr
        && fixture.source.m_effectList.size() == 2,
        "cast-cancel fixture failed to seed begin-spell and magic-target effects");

    CGameModePacketRouter router;
    RegisterDefaultGameModePacketHandlers(router);
    PacketView packet{};
    int consumed = 0;
    Require(TryReadPacket(packets[0].bytes.data(), static_cast<int>(packets[0].bytes.size()), packet, consumed),
        "production packet reader rejected cast-cancel packet");
    Require(packet.packetId == 0x01B9 && consumed == static_cast<int>(packets[0].bytes.size()),
        "cast-cancel packet framing differs from scenario metadata");
    Require(router.Dispatch(fixture.mode, packet), "default client router did not dispatch cast-cancel packet");
    Require(fixture.source.m_beginSpellEffect == nullptr && fixture.source.m_magicTargetEffect == nullptr
        && fixture.source.m_effectList.empty(),
        "cast-cancel packet did not clear both attached skill effects");

    std::ifstream runtimeLog("debug_hp_" + std::to_string(GetCurrentProcessId()) + ".log");
    Require(static_cast<bool>(runtimeLog), "production runtime log was not created for cast cancel");
    std::vector<int> actorMessages;
    std::string logLine;
    while (std::getline(runtimeLog, logLine)) {
        unsigned int actorAid = 0;
        int message = 0;
        if (std::sscanf(logLine.c_str(), "[pid:%*u t:%*u] [REPLAY] ActorMsg actor=%u msg=%d", &actorAid, &message) == 2
            && actorAid == fixture.source.m_gid) {
            actorMessages.push_back(message);
        }
    }
    Require(actorMessages == std::vector<int>({ 84, 87, 83 }),
        "cast-cancel actor message order differs from Ref (expected84,87,83)");
    std::cout << "SkillCastCancel opcode=0x01B9 messages=84,87,83 attachedEffectsCleared=2\n";
}

void ReplaySkillUnitLifecycle(const std::string& path)
{
    const std::vector<ScenarioPacket> packets = LoadPacketSequence(path);
    Require(packets.size() == 2, "skill-unit lifecycle requires set and remove packets");
    Require(packets[0].opcode == 0x011F && packets[0].bytes.size() == 16,
        "Firewall unit entry must use the 16-byte 0x011F packet");
    Require(packets[1].opcode == 0x0120 && packets[1].bytes.size() == 6,
        "skill-unit removal must use the 6-byte 0x0120 packet");
    const int unitJob = packets[0].bytes[14];
    Require(unitJob == 126 || unitJob == 127 || unitJob == 131 || unitJob == 132 || unitJob == 133
            || unitJob == 141 || unitJob == 142 || (unitJob >= 154 && unitJob <= 157)
            || (unitJob >= 158 && unitJob <= 175) || unitJob == 177 || unitJob == 179
            || unitJob == 180 || unitJob == 183 || unitJob == 184 || unitJob == 187 || unitJob == 188
            || (unitJob >= 189 && unitJob <= 194),
        "unsupported skill-unit replay job");
    int expectedLoopEffect = 0;
    switch (unitJob) {
    case 126: expectedLoopEffect = 315; break;
    case 127: expectedLoopEffect = 25; break;
    case 131: expectedLoopEffect = 319; break;
    case 132: expectedLoopEffect = 318; break;
    case 133: expectedLoopEffect = 0; break;
    case 141: expectedLoopEffect = 74; break;
    case 142: expectedLoopEffect = 95; break;
    case 154: expectedLoopEffect = 239; break;
    case 155: expectedLoopEffect = 240; break;
    case 156: expectedLoopEffect = 241; break;
    case 157: expectedLoopEffect = 242; break;
    case 158: expectedLoopEffect = 278; break;
    case 159: expectedLoopEffect = 279; break;
    case 160: expectedLoopEffect = 280; break;
    case 161: expectedLoopEffect = 281; break;
    case 162: expectedLoopEffect = 282; break;
    case 163: expectedLoopEffect = 283; break;
    case 164: expectedLoopEffect = 284; break;
    case 165: expectedLoopEffect = 285; break;
    case 166: expectedLoopEffect = 277; break;
    case 167: expectedLoopEffect = 286; break;
    case 168: expectedLoopEffect = 287; break;
    case 169: expectedLoopEffect = 288; break;
    case 170: expectedLoopEffect = 289; break;
    case 171: expectedLoopEffect = 290; break;
    case 172: expectedLoopEffect = 291; break;
    case 173: expectedLoopEffect = 292; break;
    case 174: expectedLoopEffect = 293; break;
    case 175: expectedLoopEffect = 294; break;
    case 177: expectedLoopEffect = 302; break;
    case 179: expectedLoopEffect = 370; break;
    case 180: expectedLoopEffect = 374; break;
    case 183: expectedLoopEffect = 404; break;
    case 184: expectedLoopEffect = 522; break;
    case 187: expectedLoopEffect = 620; break;
    case 188: expectedLoopEffect = 631; break;
    case 189: expectedLoopEffect = 634; break;
    case 190: expectedLoopEffect = 638; break;
    case 191: expectedLoopEffect = 639; break;
    case 192: expectedLoopEffect = 640; break;
    case 193: expectedLoopEffect = 641; break;
    case 194: expectedLoopEffect = 642; break;
    default: Require(false, "unsupported skill-unit replay effect mapping");
    }

    Fixture fixture(path);
    CGameActor* configuredActor = fixture.world.GetGameActorByAID(3);
    Require(configuredActor != nullptr && configuredActor->m_job == unitJob
        && configuredActor->m_pos.x == -27.5f && configuredActor->m_pos.y == 7.5f && configuredActor->m_pos.z == 22.5f,
        "scenario actor setup did not create AID3 with the unit job at configured cell(10,20)");
    CGameModePacketRouter router;
    RegisterDefaultGameModePacketHandlers(router);
    std::size_t processSteps = 0;
    u32 lifetimeElapsedMs = 0;
    for (const ScenarioPacket& scenarioPacket : packets) {
        PacketView packet{};
        int consumed = 0;
        Require(TryReadPacket(scenarioPacket.bytes.data(), static_cast<int>(scenarioPacket.bytes.size()), packet, consumed),
            "production packet reader rejected skill-unit lifecycle packet");
        Require(packet.packetId == scenarioPacket.opcode && consumed == static_cast<int>(scenarioPacket.bytes.size()),
            "skill-unit lifecycle packet framing differs from scenario metadata");
        Require(router.Dispatch(fixture.mode, packet), "default client router did not dispatch skill-unit packet");
        if (scenarioPacket.opcode == 0x011F) {
            CSkill* unit = fixture.world.GetSkillByAID(100);
            Require(unit != nullptr, "skill-unit entry did not add unit AID100 to CWorld");
            Require(unit->m_job == unitJob && unit->m_isVisibleBody == 1,
                "skill-unit job or visibility differs from Ref stand-entry handling");
            Require(std::fabs(unit->m_pos.x + 27.5f) < 0.001f
                && std::fabs(unit->m_pos.y - 7.5f) < 0.001f
                && std::fabs(unit->m_pos.z - 22.5f) < 0.001f,
                "skill-unit position differs from decoded cell and terrain height");
            Require(std::find(fixture.world.m_gameObjectList.begin(), fixture.world.m_gameObjectList.end(), unit)
                    != fixture.world.m_gameObjectList.end(),
                "skill-unit is missing from CWorld game-object ownership list");
            if (unitJob == 127) {
                const u32 unitStartTick = unit->m_SkillStartTime;
                u32 previousElapsedMs = 0;
                bool active = true;
                while (active && previousElapsedMs <= 3200) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(24));
                    lifetimeElapsedMs = timeGetTime() - unitStartTick;
                    active = unit->OnProcess() != 0;
                    ++processSteps;
                    if (!active) {
                        const u32 measuredStepMs = lifetimeElapsedMs - previousElapsedMs;
                        Require(lifetimeElapsedMs > 3000
                            && lifetimeElapsedMs <= 3000 + measuredStepMs + 24,
                            "Firewall unit expiry falls outside the measured 24 ms process-step window");
                    }
                    previousElapsedMs = lifetimeElapsedMs;
                }
                Require(!active && processSteps >= 100,
                    "Firewall unit did not expire after nominal 24 ms CSkill::OnProcess steps");
            } else if (unitJob == 142) {
                for (; processSteps < 140; ++processSteps) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(24));
                    lifetimeElapsedMs = timeGetTime() - unit->m_SkillStartTime;
                    Require(unit->OnProcess() != 0,
                        "Quagmire unit should remain active during periodic effect steps");
                }
                Require(processSteps >= 131,
                    "Quagmire unit was not stepped through the Ref periodic effect threshold");
            } else if (unitJob >= 189 && unitJob <= 194) {
                for (; processSteps < 17; ++processSteps) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(24));
                    lifetimeElapsedMs = timeGetTime() - unit->m_SkillStartTime;
                    Require(unit->OnProcess() != 0,
                        "Kaen/Ground Drift unit should remain active during periodic effect steps");
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(24));
                lifetimeElapsedMs = timeGetTime() - unit->m_SkillStartTime;
                Require(unit->OnProcess() != 0, "persistent skill unit should remain active after its first 24 ms process step");
                ++processSteps;
            }
        } else {
            Require(fixture.world.GetSkillByAID(100) == nullptr,
                "0x0120 did not remove Firewall unit AID100 from CWorld skill list");
            Require(std::none_of(fixture.world.m_gameObjectList.begin(), fixture.world.m_gameObjectList.end(),
                    [](CGameObject* object) {
                        const CSkill* skill = dynamic_cast<const CSkill*>(object);
                        return skill != nullptr && skill->m_aid == 100;
                    }),
                "0x0120 did not remove Firewall unit AID100 from CWorld object ownership list");
        }
    }

    std::ifstream runtimeLog("debug_hp_" + std::to_string(GetCurrentProcessId()) + ".log");
    Require(static_cast<bool>(runtimeLog), "production runtime log was not created for Firewall unit lifecycle");
    std::string logLine;
    bool foundExpectedWave = unitJob != 127;
    bool foundExpectedLoop = expectedLoopEffect == 0;
    bool foundAnyUnitEffect = false;
    while (std::getline(runtimeLog, logLine)) {
        foundExpectedWave = foundExpectedWave
            || logLine.find("[REPLAY] PlayWave name='effect\\EF_FireWall.wav'") != std::string::npos;
        foundExpectedLoop = foundExpectedLoop
            || logLine.find("[REPLAY] LaunchEffect id=" + std::to_string(expectedLoopEffect) + " actor=100") != std::string::npos;
        foundAnyUnitEffect = foundAnyUnitEffect
            || (logLine.find("[REPLAY] LaunchEffect id=") != std::string::npos
                && logLine.find("actor=100") != std::string::npos);
    }
    Require(foundExpectedWave, "skill-unit entry did not play the expected Ref wave");
    if (expectedLoopEffect == 0) {
        Require(!foundAnyUnitEffect,
            "Pneuma job133 follows the Ref default path and must not launch a unit effect");
    } else {
        Require(foundExpectedLoop, "skill-unit OnProcess did not launch its Ref loop effect");
    }
    std::cout << "SkillUnit job=" << unitJob << " aid=100 cell=(10,20) pos=(-27.500,7.500,22.500) loopEffect="
        << expectedLoopEffect << " steps=" << processSteps << " elapsedMs=" << lifetimeElapsedMs << " removed=1\n";
}

void ReplayTrapRevealLifecycle(const std::string& path)
{
    const std::vector<ScenarioPacket> packets = LoadPacketSequence(path);
    Require(packets.size() == 4, "trap lifecycle requires set, hide, reveal, and remove packets");
    Require(packets[0].opcode == 0x011F && packets[0].bytes.size() == 16
        && packets[1].opcode == 0x011F && packets[1].bytes.size() == 16,
        "trap lifecycle must begin with two 16-byte skill-unit entries");
    Require(packets[2].opcode == 0x01AC && packets[2].bytes.size() == 6,
        "trap lifecycle reveal must use the 6-byte 0x01AC packet");
    Require(packets[3].opcode == 0x0120 && packets[3].bytes.size() == 6,
        "trap lifecycle removal must use the 6-byte 0x0120 packet");

    Fixture fixture(path);
    CGameModePacketRouter router;
    RegisterDefaultGameModePacketHandlers(router);
    const bool expectedVisibility[] = { true, false, true, false };
    for (std::size_t index = 0; index < packets.size(); ++index) {
        PacketView packet{};
        int consumed = 0;
        Require(TryReadPacket(packets[index].bytes.data(), static_cast<int>(packets[index].bytes.size()), packet, consumed),
            "production packet reader rejected trap lifecycle packet");
        Require(packet.packetId == packets[index].opcode && consumed == static_cast<int>(packets[index].bytes.size()),
            "trap lifecycle packet framing differs from scenario metadata");
        Require(router.Dispatch(fixture.mode, packet), "default client router did not dispatch trap lifecycle packet");
        CSkill* trap = fixture.world.GetSkillByAID(100);
        if (index < 3) {
            Require(trap != nullptr, "trap lifecycle packet unexpectedly lost unit AID100");
            Require((trap->m_isVisibleBody != 0) == expectedVisibility[index],
                "trap visibility does not match set-hidden-reveal packet sequence");
            Require(trap->m_job == 143 && trap->m_pos.x == -27.5f && trap->m_pos.y == 7.5f && trap->m_pos.z == 22.5f,
                "trap job or packet-cell terrain position differs from Ref");
            Require(trap->m_3dactor == nullptr, "trap fixture unexpectedly loaded an external model resource");
        } else {
            Require(trap == nullptr, "trap removal did not erase unit AID100");
        }
    }
    std::cout << "Trap job=143 aid=100 hidden=1 revealed=1 removed=1 model_resource=not-loaded\n";
}

void ReplayLandMineTriggerLifecycle(const std::string& path)
{
    const std::vector<ScenarioPacket> packets = LoadPacketSequence(path);
    Require(packets.size() == 4, "Land Mine trigger requires entry, damage, active-look, and removal packets");
    Require(packets[0].opcode == 0x011F && packets[0].bytes.size() == 16
            && packets[0].bytes[14] == 147,
        "Land Mine trigger must begin with a job147 0x011F unit entry");
    Require(packets[1].opcode == 0x01DE && packets[1].bytes.size() == 33
            && ReadU16LE(packets[1].bytes.data() + 2) == 116,
        "Land Mine trigger must include a 33-byte HT_LANDMINE 0x01DE damage notification");
    Require(packets[2].opcode == 0x01D7 && packets[2].bytes.size() == 11
            && ReadU32LE(packets[2].bytes.data() + 2) == 100
            && ReadU32LE(packets[2].bytes.data() + 7) == 135,
        "Land Mine trigger must transform unit AID100 to active Fire Pillar job135");
    Require(packets[3].opcode == 0x0120 && packets[3].bytes.size() == 6,
        "Land Mine trigger must end with a 0x0120 removal packet");

    Fixture fixture(path);
    CGameModePacketRouter router;
    RegisterDefaultGameModePacketHandlers(router);
    bool dispatchedDamageNotify = false;
    bool foundDamageNotify = false;
    bool foundActiveEffect = false;
    for (const ScenarioPacket& scenarioPacket : packets) {
        PacketView packet{};
        int consumed = 0;
        Require(TryReadPacket(scenarioPacket.bytes.data(), static_cast<int>(scenarioPacket.bytes.size()), packet, consumed),
            "production packet reader rejected Land Mine trigger packet");
        Require(packet.packetId == scenarioPacket.opcode && consumed == static_cast<int>(scenarioPacket.bytes.size()),
            "Land Mine trigger packet framing differs from scenario metadata");
        Require(router.Dispatch(fixture.mode, packet), "default client router did not dispatch Land Mine trigger packet");
        if (scenarioPacket.opcode == 0x011F) {
            CSkill* unit = fixture.world.GetSkillByAID(100);
            Require(unit != nullptr && unit->m_job == 147 && unit->m_isVisibleBody == 1,
                "Land Mine entry did not create the visible job147 unit");
        } else if (scenarioPacket.opcode == 0x01DE) {
            dispatchedDamageNotify = true;
            Require(ReadU32LE(scenarioPacket.bytes.data() + 4) == 100
                    && ReadU32LE(scenarioPacket.bytes.data() + 8) == 2,
                "HT_LANDMINE trigger packet must report trap-unit AID100 attacking target AID2");
        } else if (scenarioPacket.opcode == 0x01D7) {
            CSkill* unit = fixture.world.GetSkillByAID(100);
            Require(unit != nullptr && unit->m_job == 135,
                "0x01D7 did not change Land Mine unit to active Fire Pillar job135");
            std::this_thread::sleep_for(std::chrono::milliseconds(24));
            Require(unit->OnProcess() != 0,
                "active Fire Pillar unit should remain alive after its first nominal process step");
        } else {
            Require(fixture.world.GetSkillByAID(100) == nullptr,
                "0x0120 did not remove triggered Land Mine from CWorld skill list");
            Require(std::none_of(fixture.world.m_gameObjectList.begin(), fixture.world.m_gameObjectList.end(),
                    [](CGameObject* object) {
                        const CSkill* skill = dynamic_cast<const CSkill*>(object);
                        return skill != nullptr && skill->m_aid == 100;
                    }),
                "0x0120 did not remove triggered Land Mine from CWorld object ownership list");
        }
    }

    std::ifstream runtimeLog("debug_hp_" + std::to_string(GetCurrentProcessId()) + ".log");
    Require(static_cast<bool>(runtimeLog), "production runtime log was not created for Land Mine trigger");
    std::string logLine;
    while (std::getline(runtimeLog, logLine)) {
        foundDamageNotify = foundDamageNotify
            || logLine.find("[GameMode] skill damage opcode=0x01DE src=100 dst=2 skill=116") != std::string::npos;
        foundActiveEffect = foundActiveEffect
            || logLine.find("[REPLAY] LaunchEffect id=138 actor=100") != std::string::npos;
    }
    Require(dispatchedDamageNotify && foundDamageNotify,
        "runtime trace did not record the HT_LANDMINE damage notification");
    Require(foundActiveEffect, "active Fire Pillar did not launch Ref effect138");
    std::cout << "LandMine trigger aid=100 skill=116 activeJob=135 effect=138 removed=1\n";
}

void ReplayGraffitiLifecycle(const std::string& path)
{
    const std::vector<ScenarioPacket> packets = LoadPacketSequence(path);
    Require(packets.size() == 2 && packets[0].opcode == 0x01C9 && packets[0].bytes.size() == 97
        && packets[1].opcode == 0x0120 && packets[1].bytes.size() == 6,
        "Graffiti lifecycle requires a 97-byte entry and 6-byte removal packet");
    Require(packets[0].bytes[16] == 1 && ReadU32LE(packets[0].bytes.data() + 2) == 200
        && ReadU32LE(packets[0].bytes.data() + 6) == 1
        && ReadU16LE(packets[0].bytes.data() + 10) == 10
        && ReadU16LE(packets[0].bytes.data() + 12) == 20,
        "Graffiti entry must identify unit AID200, creator AID1, and cell(10,20)");
    Require(std::find(packets[0].bytes.begin() + 17, packets[0].bytes.begin() + 97, 0) != packets[0].bytes.begin() + 97,
        "Graffiti text must be NUL-terminated within its 80-byte field");

    Fixture fixture(path);
    CGameModePacketRouter router;
    RegisterDefaultGameModePacketHandlers(router);
    for (std::size_t index = 0; index < packets.size(); ++index) {
        PacketView packet{};
        int consumed = 0;
        Require(TryReadPacket(packets[index].bytes.data(), static_cast<int>(packets[index].bytes.size()), packet, consumed),
            "production packet reader rejected Graffiti lifecycle packet");
        Require(packet.packetId == packets[index].opcode && consumed == static_cast<int>(packets[index].bytes.size()),
            "Graffiti packet framing differs from scenario metadata");
        Require(router.Dispatch(fixture.mode, packet), "default client router did not dispatch Graffiti lifecycle packet");

        CSkill* skill = fixture.world.GetSkillByAID(200);
        if (index == 0) {
            auto* graffiti = dynamic_cast<CGraffiSkill*>(skill);
            Require(graffiti != nullptr, "0x01C9 did not create a CGraffiSkill");
            Require(std::find(fixture.world.m_gameObjectList.begin(), fixture.world.m_gameObjectList.end(), graffiti)
                    != fixture.world.m_gameObjectList.end(),
                "Graffiti was not registered in CWorld game object ownership");
            Require(graffiti->OnProcess() != 0, "Graffiti did not remain active after entry");
        } else {
            Require(skill == nullptr, "0x0120 did not remove Graffiti from CWorld skill lookup");
            Require(std::none_of(fixture.world.m_gameObjectList.begin(), fixture.world.m_gameObjectList.end(),
                    [](CGameObject* object) {
                        const CSkill* candidate = dynamic_cast<const CSkill*>(object);
                        return candidate != nullptr && candidate->m_aid == 200;
                    }),
                "0x0120 did not remove Graffiti from CWorld object ownership");
        }
    }
    std::cout << "Graffiti aid=200 creator=1 cell=(10,20) text_field=80 active=1 removed=1\n";
}

void ReplaySavedEffectTrapTriggerLifecycle(const std::string& path, int trapJob, int skillId, int effectId,
    const std::string& trapName)
{
    const std::vector<ScenarioPacket> packets = LoadPacketSequence(path);
    Require(packets.size() == 6 && packets[0].opcode == 0x011F && packets[0].bytes.size() == 16
        && packets[1].opcode == 0x011F && packets[1].bytes.size() == 16
        && packets[2].opcode == 0x01AC && packets[2].bytes.size() == 6
        && packets[3].opcode == 0x01DE && packets[3].bytes.size() == 33
        && packets[4].opcode == 0x01D7 && packets[4].bytes.size() == 11
        && packets[5].opcode == 0x0120 && packets[5].bytes.size() == 6,
        "saved-effect trap trigger requires set, hide, reveal, damage, used-trap update, and removal packets");
    Require(packets[0].bytes[14] == trapJob && packets[1].bytes[14] == trapJob
        && ReadU16LE(packets[3].bytes.data() + 2) == skillId
        && ReadU32LE(packets[3].bytes.data() + 4) == 100 && ReadU32LE(packets[3].bytes.data() + 8) == 2
        && ReadU32LE(packets[4].bytes.data() + 2) == 100 && ReadU32LE(packets[4].bytes.data() + 7) == 140,
        "trap trigger job, damage skill, actor ids, or used-trap activation differs from Ref");

    Fixture fixture(path);
    CGameModePacketRouter router;
    RegisterDefaultGameModePacketHandlers(router);
    bool sawDamageNotify = false;
    bool sawExpectedEffect = false;
    for (std::size_t index = 0; index < packets.size(); ++index) {
        PacketView packet{};
        int consumed = 0;
        Require(TryReadPacket(packets[index].bytes.data(), static_cast<int>(packets[index].bytes.size()), packet, consumed),
            "production packet reader rejected Blast Mine trigger packet");
        Require(packet.packetId == packets[index].opcode && consumed == static_cast<int>(packets[index].bytes.size()),
            "trap packet framing differs from scenario metadata");
        Require(router.Dispatch(fixture.mode, packet), "default client router did not dispatch trap trigger packet");

        CSkill* unit = fixture.world.GetSkillByAID(100);
        if (index == 0) {
            Require(unit != nullptr && unit->m_job == trapJob && unit->m_effectId == effectId
                    && unit->m_isVisibleBody == 1,
                trapName + " entry did not initialize its Ref saved-effect trap job");
        } else if (index == 1) {
            Require(unit != nullptr && unit->m_job == trapJob && unit->m_isVisibleBody == 0,
                "duplicate trap entry did not hide " + trapName);
        } else if (index == 2) {
            Require(unit != nullptr && unit->m_isVisibleBody == 1,
                "0x01AC did not reveal hidden " + trapName);
        } else if (index == 3) {
            Require(unit != nullptr && unit->m_job == trapJob,
                trapName + " damage notification unexpectedly changed its unit job");
        } else if (index == 4) {
            Require(unit != nullptr && unit->m_job == 140,
                "0x01D7 did not activate " + trapName + " as used-trap job140");
        } else {
            Require(unit == nullptr, "0x0120 did not remove triggered " + trapName + " from CWorld skill list");
            Require(std::none_of(fixture.world.m_gameObjectList.begin(), fixture.world.m_gameObjectList.end(),
                    [](CGameObject* object) {
                        const CSkill* skill = dynamic_cast<const CSkill*>(object);
                        return skill != nullptr && skill->m_aid == 100;
                    }),
                "0x0120 did not remove triggered " + trapName + " from CWorld object ownership list");
        }
    }

    std::ifstream runtimeLog("debug_hp_" + std::to_string(GetCurrentProcessId()) + ".log");
    Require(static_cast<bool>(runtimeLog), "production runtime log was not created for " + trapName + " trigger");
    std::string logLine;
    while (std::getline(runtimeLog, logLine)) {
        sawDamageNotify = sawDamageNotify
            || logLine.find("[GameMode] skill damage opcode=0x01DE src=100 dst=2 skill=" + std::to_string(skillId)) != std::string::npos;
        sawExpectedEffect = sawExpectedEffect
            || logLine.find("[REPLAY] LaunchEffect id=" + std::to_string(effectId) + " actor=100") != std::string::npos;
    }
    Require(sawDamageNotify, "runtime trace did not record the trap damage notification");
    Require(sawExpectedEffect, "used-trap job140 did not launch the saved Ref effect");
    std::cout << trapName << " trigger aid=100 skill=" << skillId << " hidden=1 revealed=1 activeJob=140 effect="
        << effectId << " removed=1\n";
}

void ReplayWarpUnitLifecycle(const std::string& path)
{
    const std::vector<ScenarioPacket> packets = LoadPacketSequence(path);
    Require(packets.size() == 3 && packets[0].opcode == 0x011F && packets[0].bytes.size() == 16
        && packets[1].opcode == 0x01D7 && packets[1].bytes.size() == 11
        && packets[2].opcode == 0x0120 && packets[2].bytes.size() == 6,
        "Warp unit lifecycle requires entry, sprite job update, and removal packets");
    Fixture fixture(path);
    CGameModePacketRouter router;
    RegisterDefaultGameModePacketHandlers(router);
    for (std::size_t index = 0; index < packets.size(); ++index) {
        PacketView packet{};
        int consumed = 0;
        Require(TryReadPacket(packets[index].bytes.data(), static_cast<int>(packets[index].bytes.size()), packet, consumed),
            "production packet reader rejected Warp unit lifecycle packet");
        Require(packet.packetId == packets[index].opcode && consumed == static_cast<int>(packets[index].bytes.size()),
            "Warp unit lifecycle packet framing differs from scenario metadata");
        Require(router.Dispatch(fixture.mode, packet), "default client router did not dispatch Warp unit packet");
        CSkill* unit = fixture.world.GetSkillByAID(100);
        if (index == 0) {
            Require(unit != nullptr && unit->m_job == 128,
                "Warp waiting entry did not create the expected unit job128");
            std::this_thread::sleep_for(std::chrono::milliseconds(24));
            Require(unit->OnProcess() != 0 && unit->m_job == 128,
                "Warp waiting unit did not remain active through its first 24 ms step");
        } else if (index == 1) {
            Require(unit != nullptr && unit->m_job == 129,
                "0x01D7 did not route the skill-unit base-look update to message47/job129");
            std::this_thread::sleep_for(std::chrono::milliseconds(24));
            Require(unit->OnProcess() != 0 && unit->m_job == 129,
                "Warp active unit did not remain active through its first 24 ms step");
        } else {
            Require(unit == nullptr, "0x0120 did not remove the active Warp unit");
        }
    }

    std::ifstream runtimeLog("debug_hp_" + std::to_string(GetCurrentProcessId()) + ".log");
    Require(static_cast<bool>(runtimeLog), "production runtime log was not created for Warp lifecycle");
    bool waitingLoop = false;
    bool activeLoop = false;
    std::string logLine;
    while (std::getline(runtimeLog, logLine)) {
        waitingLoop = waitingLoop || logLine.find("[REPLAY] LaunchEffect id=317 actor=100") != std::string::npos
            || logLine.find("[REPLAY] LaunchEffect id=36 actor=100") != std::string::npos;
        activeLoop = activeLoop || logLine.find("[REPLAY] LaunchEffect id=316 actor=100") != std::string::npos;
    }
    Require(waitingLoop && activeLoop, "Warp waiting/active states did not launch Ref loop effects317/316 (or effect-off36)");
    std::cout << "WarpUnit aid=100 waitingJob=128 activeJob=129 loopEffects=317/316 removed=1\n";
}

bool IsFirewallUnitScenario(const std::string& path)
{
    const std::vector<ScenarioPacket> packets = LoadPacketSequence(path);
    return packets.front().opcode == 0x011F;
}

bool IsTrapRevealScenario(const std::string& path)
{
    const std::vector<ScenarioPacket> packets = LoadPacketSequence(path);
    return packets.size() == 4 && packets.front().opcode == 0x011F
        && packets.front().bytes.size() == 16 && packets.front().bytes[14] == 143;
}

bool IsSkillCastCancelScenario(const std::string& path)
{
    const std::vector<ScenarioPacket> packets = LoadPacketSequence(path);
    return packets.front().opcode == 0x01B9;
}

bool IsWarpUnitLifecycleScenario(const std::string& path)
{
    const std::vector<ScenarioPacket> packets = LoadPacketSequence(path);
    return packets.size() == 3 && packets.front().opcode == 0x011F
        && packets.front().bytes.size() == 16 && packets.front().bytes[14] == 128;
}

bool IsLandMineTriggerScenario(const std::string& path)
{
    const std::vector<ScenarioPacket> packets = LoadPacketSequence(path);
    return packets.size() == 4 && packets.front().opcode == 0x011F
        && packets.front().bytes.size() == 16 && packets.front().bytes[14] == 147
        && packets[1].opcode == 0x01DE && packets[2].opcode == 0x01D7;
}

bool IsGraffitiLifecycleScenario(const std::string& path)
{
    const std::vector<ScenarioPacket> packets = LoadPacketSequence(path);
    return packets.size() == 2 && packets.front().opcode == 0x01C9
        && packets.front().bytes.size() == 97 && packets.back().opcode == 0x0120;
}

bool IsSavedEffectTrapScenario(const std::string& path, int trapJob, int skillId)
{
    const std::vector<ScenarioPacket> packets = LoadPacketSequence(path);
    return packets.size() == 6 && packets.front().opcode == 0x011F
        && packets.front().bytes.size() == 16 && packets.front().bytes[14] == trapJob
        && packets[3].opcode == 0x01DE && ReadU16LE(packets[3].bytes.data() + 2) == skillId;
}

} // namespace

int main(int argc, char** argv)
{
    try {
        Require(argc == 2, "expected one replay scenario path");
        const std::string runtimeLogPath = "debug_hp_" + std::to_string(GetCurrentProcessId()) + ".log";
        std::ofstream freshRuntimeLog(runtimeLogPath, std::ios::trunc);
        Require(static_cast<bool>(freshRuntimeLog), "could not reset per-run production runtime log");
        freshRuntimeLog.close();
        if (IsSkillCastCancelScenario(argv[1])) {
            ReplaySkillCastCancel(argv[1]);
        } else if (IsTrapRevealScenario(argv[1])) {
            ReplayTrapRevealLifecycle(argv[1]);
        } else if (IsLandMineTriggerScenario(argv[1])) {
            ReplayLandMineTriggerLifecycle(argv[1]);
        } else if (IsGraffitiLifecycleScenario(argv[1])) {
            ReplayGraffitiLifecycle(argv[1]);
        } else if (IsSavedEffectTrapScenario(argv[1], 143, 122)) {
            ReplaySavedEffectTrapTriggerLifecycle(argv[1], 143, 122, 106, "BlastMine");
        } else if (IsSavedEffectTrapScenario(argv[1], 152, 123)) {
            ReplaySavedEffectTrapTriggerLifecycle(argv[1], 152, 123, 107, "ClaymoreTrap");
        } else if (IsWarpUnitLifecycleScenario(argv[1])) {
            ReplayWarpUnitLifecycle(argv[1]);
        } else if (IsFirewallUnitScenario(argv[1])) {
            ReplaySkillUnitLifecycle(argv[1]);
        } else {
            ReplayScenario(argv[1]);
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "production skill replay failed: " << error.what() << '\n';
        return 1;
    }
}
