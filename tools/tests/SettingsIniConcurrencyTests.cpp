#include "core/SettingsIni.h"

#include <cstdlib>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <thread>

#if RO_PLATFORM_WINDOWS
#include <windows.h>
#endif

int main(int argc, char** argv)
{
    if (argc != 3) return 2;
    const std::string mode = argv[1];
    const std::filesystem::path directory = argv[2];
#if RO_PLATFORM_WINDOWS
    if (_putenv_s("OPEN_MIDGARD_DATA_DIR", directory.string().c_str()) != 0) return 2;
#else
    if (setenv("OPEN_MIDGARD_DATA_DIR", directory.string().c_str(), 1) != 0) return 2;
#endif

    if (mode == "blocked") {
#if RO_PLATFORM_WINDOWS
        const auto path = directory / "open-midgard.ini";
        std::ifstream beforeFile(path, std::ios::binary);
        const std::string before{std::istreambuf_iterator<char>(beforeFile), std::istreambuf_iterator<char>()};
        beforeFile.close();
        HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) return 3;
        const bool saved = SaveSettingsIniInt("Concurrent", "blocked", 1);
        CloseHandle(file);
        std::ifstream afterFile(path, std::ios::binary);
        const std::string after{std::istreambuf_iterator<char>(afterFile), std::istreambuf_iterator<char>()};
        afterFile.close();
        if (saved || before != after) return 4;
        file = CreateFileW(path.c_str(), GENERIC_READ, 0,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) return 3;
        int value = 0;
        const bool read = TryLoadSettingsIniInt("Sentinel", "Preserve", &value);
        const bool wrote = SaveSettingsIniInt("Concurrent", "readBlocked", 1);
        CloseHandle(file);
        std::ifstream finalFile(path, std::ios::binary);
        const std::string final{std::istreambuf_iterator<char>(finalFile), std::istreambuf_iterator<char>()};
        finalFile.close();
        if (read || wrote || before != final || !SaveSettingsIniInt("Concurrent", "afterBlocked", 1)) return 4;
#endif
        return 0;
    }

    if (mode == "initialize") {
        return LoadSettingsIniInt("Graphics", "Width", -1) == 640
            && SaveSettingsIniInt("Graphics", "Width", 1920)
            && LoadSettingsIniInt("Graphics", "Width", -1) == 1920 ? 0 : 7;
    }

    {
        std::ofstream ready(directory / (mode + ".ready"));
        if (!ready) return 8;
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (!std::filesystem::exists(directory / "start")) {
        if (std::chrono::steady_clock::now() >= deadline) return 9;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    for (int i = 0; i < 80; ++i) {
        int sentinel = 0;
        if (!TryLoadSettingsIniInt("Sentinel", "Preserve", &sentinel) || sentinel != 123456) {
            std::cerr << "sentinel lost by worker " << mode << " iteration " << i << '\n';
            return 5;
        }
        if (!SaveSettingsIniInt("Concurrent", mode.c_str(), i)) return 6;
    }
    return 0;
}
