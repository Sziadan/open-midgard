#include "DebugLog.h"
#include "core/SettingsIni.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

int main(int argc, char** argv)
{
    assert(argc == 2);
    const auto originalDirectory = std::filesystem::current_path();
    const auto directory = std::filesystem::absolute(argv[1]) / std::to_string(GetCurrentProcessId());
    std::filesystem::create_directories(directory);
    std::filesystem::current_path(directory);
    assert(_putenv_s("OPEN_MIDGARD_DATA_DIR", directory.string().c_str()) == 0);

    ConfigureDebugLoggingFromIni();
    assert(!DebugLogEnabled(DebugLogActors));
    assert(!DebugLogEnabled(DebugLogPerformance));
    DbgLog("[ActorTrace] hidden actor\n");
    DbgLog("[GameMode] hidden gameplay\n");
    DbgLog("[REPLAY] hidden replay\n");
    DbgLog("[ViewPerfHiRes] hidden performance\n");
    DbgError("[Net] visible error\n");

    assert(SaveSettingsIniInt("Debug", "ActorTrace", 1));
    assert(SaveSettingsIniInt("Debug", "EffectTrace", 1));
    assert(SaveSettingsIniInt("Debug", "RenderPerf", 1));
    ConfigureDebugLoggingFromIni();
    assert(DebugLogEnabled(DebugLogActors));
    assert(DebugLogEnabled(DebugLogPerformance));
    assert(!DebugLogEnabled(DebugLogGameplay));
    assert(DebugLogMessageEnabled("[ActorMove]"));
    assert(DebugLogMessageEnabled("[SkillWave]"));
    assert(DebugLogMessageEnabled("[GameMode] frame="));
    assert(DebugLogMessageEnabled("[GameModePerfHiRes]"));
    assert(DebugLogMessageEnabled("[OverlayPerfHiRes]"));
    assert(DebugLogMessageEnabled("[QtUiPerf]"));
    assert(!DebugLogMessageEnabled("[GameMode] actor"));
    DbgLog("[ActorTrace] enabled actor\n");
    DbgLog("[WorldEffect] enabled effect\n");
    DbgLog("[ViewPerfHiRes] enabled performance\n");
    DbgLog("[GameMode] still hidden gameplay\n");
    DbgLog("[REPLAY] still hidden replay\n");

    assert(SaveSettingsIniInt("Debug", "GameplayTrace", 1));
    assert(SaveSettingsIniInt("Debug", "ReplayTrace", 1));
    ConfigureDebugLoggingFromIni();
    DbgLog("[GameMode] enabled gameplay\n");
    DbgLog("[REPLAY] enabled replay\n");
    DbgAlways("[AssetLoadPerf] enabled separately\n");
    DbgAlways("[WorldTickPerf] enabled separately\n");

    assert(SaveSettingsIniInt("Debug", "LogInfo", 1));
    ConfigureDebugLoggingFromIni();
    DbgLog("[Other] enabled info\n");

    const auto logPath = directory / ("debug_hp_" + std::to_string(GetCurrentProcessId()) + ".log");
    std::ifstream log(logPath);
    const std::string contents{std::istreambuf_iterator<char>(log), std::istreambuf_iterator<char>()};
    assert(contents.find("hidden") == std::string::npos);
    for (const char* text : {"visible error", "enabled actor", "enabled effect",
            "enabled performance", "enabled gameplay", "enabled replay",
            "[AssetLoadPerf] enabled separately", "[WorldTickPerf] enabled separately",
            "enabled info"}) {
        assert(contents.find(text) != std::string::npos);
    }

    log.close();
    std::filesystem::current_path(originalDirectory);
    std::filesystem::remove(directory / "open-midgard.ini");
    std::filesystem::remove(directory / "open-midgard.ini.lock");
    std::filesystem::remove(logPath);
    std::filesystem::remove(directory);
}
