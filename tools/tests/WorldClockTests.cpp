#include "core/SettingsIni.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <windows.h>

#include "WorldClockConstants.inc"

namespace {

DWORD now = 1000;
DWORD TestTickCount() { return now; }
#define GetTickCount TestTickCount
int g_isSkipFrame = 0;
struct WorldTickPerfStats {};
WorldTickPerfStats g_worldTickPerfStats;

void DbgLog(const char*, ...) {}
void DbgAlways(const char*, ...) {}

using u8 = unsigned char;
struct vector3d { float x = 0, y = 0, z = 0; };
struct CRenderObject {
    virtual ~CRenderObject() = default;
    vector3d m_pos;
    float m_roty = 0;
    int m_BeZero = 0, m_BodyLight = 0;
};
struct CSkill : CRenderObject {};
bool EffectTicksWhenNoFrameTime(int) { return false; }
struct CRagEffect {
    CRenderObject owner;
    vector3d m_cachedPos;
    float m_longitude = 0;
    int m_type = 29, m_stateCnt = 0, m_duration = 1000;
    bool m_isWorldEffect = false;
    CRenderObject* ResolveCurrentMaster() { return &owner; }
    u8 ProcessWorldEffect() { return 1; }
    bool ProcessEffectFrame() { return true; }
    u8 OnProcess();
};
#include "HealTimingBody.inc"

struct StepResult {
    int logicSteps;
    int actorSteps;
    bool clamped;
};

struct WorldClock {
    DWORD m_worldProcessTick = 0;
    float m_worldProcessCarryMs = 0;
    float m_worldProcessTickMs = 0;
    bool m_worldTickPerfEnabled = false;

    void Init()
    {
#include "WorldClockInit.inc"
    }

    StepResult Update()
    {
#include "WorldClockUpdate.inc"
        return {worldStepCount, actorSteps, worldStepClamped};
    }
};

} // namespace

int main(int argc, char** argv)
{
    assert(argc == 2);
    const std::filesystem::path directory = std::filesystem::absolute(argv[1])
        / std::to_string(GetCurrentProcessId());
    std::filesystem::create_directories(directory);
    assert(_putenv_s("OPEN_MIDGARD_DATA_DIR", directory.string().c_str()) == 0);

    WorldClock clock;
    clock.Init();
    assert(clock.m_worldProcessTickMs == 24.0f && !clock.m_worldTickPerfEnabled);
    assert(clock.Update().logicSteps == 1);
    now += 7;
    StepResult result = clock.Update();
    assert(result.logicSteps == 0 && result.actorSteps == 1 && g_isSkipFrame == 0);
    now += 17;
    result = clock.Update();
    assert(result.logicSteps == 1 && result.actorSteps == 1 && g_isSkipFrame == 1);
    now += 200;
    result = clock.Update();
    assert(result.logicSteps == 8 && !result.clamped && clock.m_worldProcessCarryMs == 8.0f);

    assert(SaveSettingsIniInt("Debug", "ExperimentalWorldTick16", 1));
    assert(SaveSettingsIniInt("Debug", "WorldTickPerf", 1));
    clock.Init();
    assert(clock.m_worldProcessTickMs == 16.0f && clock.m_worldTickPerfEnabled);
    assert(clock.Update().logicSteps == 1);
    now += 15;
    result = clock.Update();
    assert(result.logicSteps == 0 && result.actorSteps == 1);
    now += 1;
    result = clock.Update();
    assert(result.logicSteps == 1 && g_isSkipFrame == 1);
    now += 200;
    result = clock.Update();
    assert(result.logicSteps == 8 && result.actorSteps == 8 && result.clamped
        && clock.m_worldProcessCarryMs == 0.0f);

    clock.Init();
    clock.Update();
    CRagEffect fastEffect;
    for (int elapsed = 0; elapsed < 1000; elapsed += 5) {
        now += 5;
        result = clock.Update();
        for (int step = 0; step < result.actorSteps; ++step) {
            assert(fastEffect.OnProcess());
        }
    }
    assert(fastEffect.m_stateCnt == 62);

    assert(SaveSettingsIniInt("Debug", "ExperimentalWorldTick16", 0));
    clock.Init();
    assert(clock.m_worldProcessTickMs == 24.0f);
    assert(clock.Update().logicSteps == 1);
    CRagEffect defaultEffect;
    for (int elapsed = 0; elapsed < 1000; elapsed += 5) {
        now += 5;
        result = clock.Update();
        for (int step = 0; step < result.actorSteps; ++step) {
            assert(defaultEffect.OnProcess());
        }
    }
    assert(defaultEffect.m_stateCnt == 41);

    std::filesystem::remove(directory / "open-midgard.ini");
    std::filesystem::remove(directory / "open-midgard.ini.lock");
    std::filesystem::remove(directory);
}
