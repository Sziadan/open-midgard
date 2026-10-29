#include <array>
#include <cstdint>
#include <cstdio>
#include <vector>

using u8 = std::uint8_t;
using u32 = std::uint32_t;
struct vector3d { float x = 0, y = 0, z = 0; };

u32 now = 1000;
u32 timeGetTime() { return now; }

struct ForceAnimTick { u32 aid = 0, tick = 0; };
struct CSession {
    u32 m_aid = 77;
    std::vector<ForceAnimTick> m_forceAnimEndTick;
    u32 GetForceAnimEndTick(u32 aid) const;
    void SetForceAnimEndTick(u32 aid, u32 tick);
} g_session;

struct CRenderObject {
    u8 m_forceAct = 0, m_forceMot = 0, m_forceStartMot = 0, m_forceMaxMot = 0;
    u8 m_forceFinishedAct = 0, m_forceFinishedMot = 0;
    int m_isForceAnimLoop = 0, m_isForceAnimation = 0, m_isForceAnimFinish = 0;
    int m_isForceState = 0, m_isForceState2 = 0, m_isForceState3 = 0;
    u32 m_forceStateCnt = 0, m_forceStateEndTick = 0;
    void SetForceAnimation(int act, int mot, int maxMot, int finishAct, int finishMot,
        int isAnim, u32 endTick, int isLoop);
    void SetForceAnimEndTick(u32 tick);
};

struct EffectEvent { const void* actor = nullptr; int effect = 0; };
std::vector<EffectEvent> effectEvents;
struct CGameActor : CRenderObject {
    u32 m_gid = 0;
    int forceAnimationCalls = 0;
    int lastAct = 0, lastMotion = 0, lastMaximum = 0;
    int lastFinishAct = 0, lastFinishMotion = 0, lastAnimate = 0, lastLoop = 0;
    void SetForceAnimation(int act, int mot, int maxMot, int finishAct, int finishMot,
        int isAnim, u32 endTick, int isLoop) {
        ++forceAnimationCalls;
        lastAct = act; lastMotion = mot; lastMaximum = maxMot;
        lastFinishAct = finishAct; lastFinishMotion = finishMot;
        lastAnimate = isAnim; lastLoop = isLoop;
        CRenderObject::SetForceAnimation(act, mot, maxMot, finishAct, finishMot, isAnim, endTick, isLoop);
    }
    void LaunchEffect(int effectId, vector3d, float) { effectEvents.push_back({this, effectId}); }
    void ApplyComboSkill272(CGameActor* target);
};

struct CWorld {
    CGameActor* m_player = nullptr;
    CGameActor* actor = nullptr;
    CGameActor* GetGameActorByAID(u32 aid) { return aid == 42 ? actor : nullptr; }
};
struct CGameMode { CWorld* m_world = nullptr; };
struct PacketView { const u8* data = nullptr; unsigned int packetLength = 0; };
u32 ReadLE32(const u8* data) {
    return u32(data[0]) | (u32(data[1]) << 8) | (u32(data[2]) << 16) | (u32(data[3]) << 24);
}

#include "ComboDelayIntegrationBodies.inc"

int main() {
    int failures = 0;
    const auto check = [&](bool condition, const char* name) {
        if (!condition) { std::fprintf(stderr, "FAIL: %s\n", name); ++failures; }
    };

    CGameActor actor, target;
    actor.m_gid = 42;
    target.m_gid = 84;
    CWorld world;
    world.actor = &actor;
    CGameMode mode;
    mode.m_world = &world;

    std::array<u8, 10> packet{0xD2, 0x01};
    const auto writeLE32 = [&](int offset, u32 value) {
        for (int byte = 0; byte < 4; ++byte) packet[offset + byte] = static_cast<u8>(value >> (8 * byte));
    };
    writeLE32(2, 42);
    writeLE32(6, 123);
    HandleComboDelay(mode, PacketView{packet.data(), static_cast<unsigned int>(packet.size())});

    check(g_session.GetForceAnimEndTick(42) == 123,
        "0x01D2 stores the raw duration under the target AID");
    check(actor.m_forceStateEndTick == 1123,
        "0x01D2 actor deadline is relative to the deterministic tick");

    effectEvents.clear();
    actor.ApplyComboSkill272(&target);
    check(actor.forceAnimationCalls == 1 && actor.lastAct == 96 && actor.lastMotion == 5
        && actor.lastMaximum == 5 && actor.lastFinishAct == 96 && actor.lastFinishMotion == 5
        && actor.lastAnimate == 1 && actor.lastLoop == 0,
        "skill272 forwards canonical action/motion and animation parameters");
    check(actor.m_forceStateEndTick == 1123,
        "skill272 uses stored combo duration to set the deadline at tick 1123");
    check(effectEvents.size() == 3
        && effectEvents[0].actor == &target && effectEvents[0].effect == 262
        && effectEvents[1].actor == &actor && effectEvents[1].effect == 263
        && effectEvents[2].actor == &target && effectEvents[2].effect == 273,
        "skill272 launches target262, self263, target273 in source order");

    if (failures == 0) std::puts("Combo delay to skill272 integration passed; no pose/render/input acceptance claimed.");
    return failures == 0 ? 0 : 1;
}
