#include <array>
#include <cstdint>
#include <cstdio>

using u8 = std::uint8_t;
using u32 = std::uint32_t;
u32 now = 1000;
u32 timeGetTime() { return now; }

struct Session { u32 m_aid = 100, m_gid = 100; } g_session;
struct vector3d { float x = 0, y = 0, z = 0; };
struct CRenderObject {
    u8 m_forceAct = 96, m_forceMot = 3, m_forceStartMot = 3, m_forceMaxMot = 4;
    u8 m_forceAct2[5] = { 10, 20, 30, 40, 50 };
    u8 m_forceMot2[5] = { 1, 2, 3, 4, 5 };
    u8 m_forceFinishedAct = 96, m_forceFinishedMot = 4, m_forceAnimSpeed = 2, m_BodyAni = 0;
    u32 m_stateStartTick = 0, m_forceStateEndTick = 0, m_forceStateCnt = 0;
    int m_isForceState = 0, m_isForceState2 = 0, m_isForceState3 = 0;
    int m_isForceAnimation = 0, m_isForceAnimFinish = 0, m_isForceAnimLoop = 0;
    int m_baseAction = 0, m_curAction = 0, m_curMotion = 0, m_motionType = 0, m_isMotionFinished = 0;
    int m_isMotionFreezed = 0, m_isSprArgbFixed = 0, tint = 255;
    float m_roty = 0, m_motionSpeed = 1;
    int Get8Dir(float) { return 2; }
    void SetForceAnimation(int act, int motion, int maximum, int finishAct, int finishMotion,
        int animate, u32 duration, int loop);
    void ProcessForcedMotion();
    void SetArgb(int, int r, int g, int b) { tint = r == g && g == b ? r : -1; }
};
struct CGameActor : CRenderObject { u32 m_gid = 0; };
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

#include "BladeStopIntegrationBodies.inc"

int main() {
    int failures = 0;
    const auto check = [&](bool condition, const char* name) {
        if (!condition) { std::fprintf(stderr, "FAIL: %s\n", name); ++failures; }
    };
    CGameActor self, target;
    self.m_gid = 100;
    target.m_gid = 42;
    CWorld world;
    world.m_player = &self;
    world.actor = &target;
    CGameMode mode;
    mode.m_world = &world;

    std::array<u8, 14> packet{0xD1, 0x01};
    const auto writeLE32 = [&](int offset, u32 value) {
        for (int byte = 0; byte < 4; ++byte) packet[offset + byte] = static_cast<u8>(value >> (8 * byte));
    };
    writeLE32(2, 100);
    writeLE32(6, 42);
    writeLE32(10, 1);
    HandleBladeStop(mode, PacketView{packet.data(), static_cast<unsigned int>(packet.size())});

    check(self.m_isForceState == 1 && self.m_forceAct == 96 && self.m_forceMot == 4
        && self.m_forceStartMot == 4 && self.m_forceMaxMot == 4
        && self.m_forceFinishedAct == 96 && self.m_forceFinishedMot == 4
        && self.m_isForceAnimation == 1 && self.m_isForceAnimLoop == 0,
        "Blade Stop start configures the source actor's canonical forced pose");
    check(self.m_forceStateEndTick == 0x5F5E0FFu + 1000u,
        "Blade Stop source pose receives its production long deadline plus current tick");
    check(self.m_isSprArgbFixed == 1 && self.tint == 64
        && target.m_isMotionFreezed == 1 && target.m_isSprArgbFixed == 1 && target.tint == 64,
        "Blade Stop start fixes both tints and freezes its target");

    self.ProcessForcedMotion();
    check(self.m_curAction == 98 && self.m_curMotion == 4 && self.m_forceMot == 4
        && self.m_isForceState == 1,
        "Blade Stop forced-motion tick presents action96 motion4 while deadline remains live");
    self.ProcessForcedMotion();
    check(self.m_curAction == 98 && self.m_curMotion == 4 && self.m_isForceAnimFinish == 1
        && self.m_forceStateEndTick > now,
        "Blade Stop forced pose clamps its final frame without ending before its deadline");

    now = 2000;
    writeLE32(10, 0);
    HandleBladeStop(mode, PacketView{packet.data(), static_cast<unsigned int>(packet.size())});
    check(self.m_isForceState == 1 && self.m_forceStateEndTick == 2001
        && self.m_isSprArgbFixed == 0 && self.tint == 255,
        "Blade Stop release restores source tint and schedules the one-tick pose tail");
    check(target.m_isMotionFreezed == 0 && target.m_isSprArgbFixed == 0 && target.tint == 255,
        "Blade Stop release unfreezes and restores target tint");
    now = 2001;
    self.ProcessForcedMotion();
    check(self.m_isForceState == 0 && self.m_forceStateEndTick == 0
        && self.m_isMotionFinished == 1 && self.m_motionType == 1,
        "Blade Stop pose exits at the release deadline through production forced-motion processing");

    if (failures == 0) std::puts("Blade Stop packet-to-forced-pose lifecycle passed; rendered pose excluded.");
    return failures == 0 ? 0 : 1;
}
