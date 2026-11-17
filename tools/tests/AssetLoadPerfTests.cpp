#include "core/AssetLoadPerf.h"
#include "core/SettingsIni.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>

int main(int argc, char** argv)
{
    assert(argc == 3);
    const std::string mode = argv[1];
    const std::filesystem::path originalDirectory = std::filesystem::current_path();
    const std::filesystem::path directory = std::filesystem::absolute(argv[2])
        / std::to_string(GetCurrentProcessId());
    std::filesystem::create_directories(directory);
    std::filesystem::current_path(directory);
    assert(_putenv_s("OPEN_MIDGARD_DATA_DIR", directory.string().c_str()) == 0);
    assert(_putenv_s("RO_ASSET_LOAD_PERF", mode == "environment" ? "1" : "") == 0);

    const std::filesystem::path iniPath = directory / "open-midgard.ini";
    {
        std::ofstream ini(iniPath, std::ios::trunc);
        assert(ini);
        if (mode != "default") {
            ini << "[Debug]\nAssetLoadPerf=" << (mode == "enabled" ? "1" : "0") << '\n';
        }
    }

    {
        AssetLoadPerf perf("test", "ini-setting");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    const std::string logName = "debug_hp_" + std::to_string(GetCurrentProcessId()) + ".log";
    std::ifstream log(directory / logName);
    const std::string contents{std::istreambuf_iterator<char>(log), std::istreambuf_iterator<char>()};
    const bool logged = contents.find("[AssetLoadPerf] category=test name='ini-setting'") != std::string::npos;
    assert(logged == (mode == "enabled" || mode == "environment"));
    assert(LoadSettingsIniInt("Debug", "AssetLoadPerf", -1) == (mode == "enabled" ? 1 : 0));
    log.close();
    std::filesystem::current_path(originalDirectory);
    std::filesystem::remove(iniPath);
    std::filesystem::remove(directory / "open-midgard.ini.lock");
    std::filesystem::remove(directory / logName);
    std::filesystem::remove(directory);
}
