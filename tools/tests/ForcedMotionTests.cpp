#include <cstdint>
#include <cstdio>

using u8 = std::uint8_t;
using u32 = std::uint32_t;
u32 now = 100;
u32 timeGetTime() { return now; }
struct CRenderObject {
    u8 m_forceAct = 96, m_forceMot = 3, m_forceStartMot = 3, m_forceMaxMot = 4;
    u8 m_forceAct2[5] = { 10, 20, 30, 40, 50 };
    u8 m_forceMot2[5] = { 1, 2, 3, 4, 5 };
    u8 m_forceFinishedAct = 96, m_forceFinishedMot = 4, m_forceAnimSpeed = 2, m_BodyAni = 0;
    u32 m_stateStartTick = 0, m_forceStateEndTick = 500, m_forceStateCnt = 2;
    int m_isForceState = 1, m_isForceState2 = 0, m_isForceState3 = 0;
    int m_isForceAnimation = 1, m_isForceAnimFinish = 0, m_isForceAnimLoop = 0;
    int m_baseAction = 0, m_curAction = 0, m_curMotion = 0, m_motionType = 0, m_isMotionFinished = 0;
    float m_roty = 0, m_motionSpeed = 1;
    int Get8Dir(float) { return 2; }
    void ProcessForcedMotion();
};

#include "ForcedMotionBody.inc"

int main()
{
    int failures = 0;
    auto check = [&](bool condition, const char* name) {
        if (!condition) { std::fprintf(stderr, "FAIL: %s\n", name); ++failures; }
    };
    CRenderObject actor;
    actor.ProcessForcedMotion();
    check(actor.m_curAction == 98 && actor.m_curMotion == 3 && actor.m_forceMot == 4
        && actor.m_forceStateCnt == 3, "force displays current frame before advancing");
    actor.ProcessForcedMotion();
    check(actor.m_forceMot == 4 && actor.m_forceStateCnt == 4, "force speed gate retains frame");
    actor.ProcessForcedMotion();
    check(actor.m_isForceAnimFinish == 1 && actor.m_forceMot == 4, "nonloop force clamps and marks finish");
    now = 500;
    actor.ProcessForcedMotion();
    check(actor.m_isForceState == 0 && actor.m_isMotionFinished == 1 && actor.m_motionType == 1
        && actor.m_forceStateEndTick == 0, "force deadline releases state");
    actor = CRenderObject{}; now = 100;
    actor.m_isForceAnimation = 0;
    actor.ProcessForcedMotion();
    check(actor.m_forceStateCnt == 2 && actor.m_forceMot == 3, "static force does not advance");
    actor = CRenderObject{};
    actor.m_isForceAnimLoop = 1; actor.m_forceMot = 4;
    actor.ProcessForcedMotion();
    check(actor.m_forceMot == 3 && actor.m_forceStateCnt == 3, "loop returns to start frame");
    actor = CRenderObject{};
    actor.m_isForceAnimLoop = 1; actor.m_forceFinishedAct = 0; actor.m_forceFinishedMot = 0;
    actor.m_forceMot = 4;
    actor.ProcessForcedMotion();
    check(actor.m_forceMot == 4 && actor.m_forceFinishedMot == 1, "ping-pong sets reverse flag");
    actor.m_forceStateCnt = 4;
    actor.ProcessForcedMotion();
    check(actor.m_forceMot == 3 && actor.m_forceFinishedMot == 0, "ping-pong reverse step");
    actor = CRenderObject{};
    actor.m_isForceState2 = 1; actor.m_forceStartMot = 1;
    actor.ProcessForcedMotion();
    check(actor.m_curAction == 22 && actor.m_curMotion == 2 && actor.m_forceStartMot == 2,
        "force sequence uses indexed action/motion before advance");
    actor.m_forceStartMot = 4; actor.m_forceStateCnt = 4;
    actor.ProcessForcedMotion();
    check(actor.m_isForceAnimFinish == 1 && actor.m_forceStartMot == 4, "sequence clamps final index");
    actor.m_forceStateEndTick = 0;
    actor.ProcessForcedMotion();
    check(actor.m_curAction == 52 && actor.m_curMotion == 5 && actor.m_isForceState2 == 0,
        "finished sequence uses slot four and releases");
    actor = CRenderObject{};
    actor.m_isForceState3 = 1; actor.m_BodyAni = 2;
    actor.ProcessForcedMotion();
    check(actor.m_BodyAni == 3 && actor.m_forceMot == 0 && actor.m_forceStateCnt == 3,
        "body-animation branch reverses after maximum");
    actor = CRenderObject{};
    actor.m_forceMot = 255; actor.m_forceMaxMot = 255;
    actor.ProcessForcedMotion();
    check(actor.m_forceMot == 0 && actor.m_isForceAnimFinish == 0, "byte-sized force motion wraps");
    if (!failures) std::puts("13 forced-motion checks passed; no assets loaded or rendered.");
    return failures ? 1 : 0;
}
