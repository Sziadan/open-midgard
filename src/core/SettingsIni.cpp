#include "SettingsIni.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#if RO_PLATFORM_WINDOWS
#include <windows.h>
#elif defined(__linux__)
#include <limits.h>
#include <unistd.h>
#endif

namespace {

struct IniDocument {
    std::map<std::string, std::map<std::string, std::string>> sections;
};

struct DefaultIniValue {
    const char* section;
    const char* key;
    const char* value;
};

constexpr DefaultIniValue kDefaultIniValues[] = {
    { "Graphics", "Width", "640" },
    { "Graphics", "Height", "480" },
    { "Graphics", "WindowMode", "0" },
    { "Graphics", "TextureUpscale", "1" },
    { "Graphics", "AnisotropicLevel", "1" },
    { "Graphics", "AntiAliasing", "0" },
    { "Graphics", "VSync", "0" },
    { "Graphics", "SpriteFilter", "0" },
    { "Graphics", "FrameLimit", "0" },
    { "Graphics", "RenderScale", "100" },
    { "OptionWnd", "X", "0" },
    { "OptionWnd", "Y", "0" },
    { "OptionWnd", "W", "308" },
    { "OptionWnd", "H", "238" },
    { "OptionWnd", "OrgH", "238" },
    { "OptionWnd", "Show", "1" },
    { "OptionWnd", "BgmVolume", "100" },
    { "OptionWnd", "SoundVolume", "100" },
    { "OptionWnd", "BgmOn", "1" },
    { "OptionWnd", "SoundOn", "1" },
    { "OptionWnd", "NoCtrl", "0" },
    { "OptionWnd", "AttackSnap", "0" },
    { "OptionWnd", "SkillSnap", "0" },
    { "OptionWnd", "ItemSnap", "0" },
    { "OptionWnd", "Collapsed", "0" },
    { "OptionWnd", "Tab", "0" },
    { "Login", "RememberUserIdEnabled", "0" },
    { "Login", "RememberUserId", "" },
    { "Packets", "CharListReceiveLayout", "auto" },
    { "Packets", "MapSendProfile", "packetver23" },
    { "SelectChar", "CurrentSlot", "0" },
};

std::mutex g_iniMutex;

std::string TrimAscii(std::string value)
{
    auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    });
    auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    }).base();
    if (first >= last) {
        return {};
    }
    return std::string(first, last);
}

std::filesystem::path GetExecutableDirectoryInternal()
{
#if RO_PLATFORM_WINDOWS
    char modulePath[MAX_PATH] = {};
    if (GetModuleFileNameA(nullptr, modulePath, MAX_PATH) == 0 || modulePath[0] == '\0') {
        return {};
    }
    return std::filesystem::path(modulePath).parent_path();
#elif defined(__linux__)
    char modulePath[PATH_MAX] = {};
    const ssize_t length = readlink("/proc/self/exe", modulePath, sizeof(modulePath) - 1);
    if (length <= 0) {
        return std::filesystem::current_path();
    }
    modulePath[length] = '\0';
    return std::filesystem::path(modulePath).parent_path();
#else
    return std::filesystem::current_path();
#endif
}

std::filesystem::path GetIniDirectoryOverrideInternal()
{
    if (const char* overrideRoot = std::getenv("OPEN_MIDGARD_DATA_DIR")) {
        if (*overrideRoot) {
            return std::filesystem::path(overrideRoot);
        }
    }
    return {};
}

bool LoadIniDocument(const std::filesystem::path& path, IniDocument* document)
{
    if (!document) {
        return false;
    }

    document->sections.clear();

    std::ifstream input(path);
    if (!input.is_open()) {
        return false;
    }

    std::string currentSection;
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        const std::string trimmed = TrimAscii(line);
        if (trimmed.empty() || trimmed[0] == ';' || trimmed[0] == '#') {
            continue;
        }

        if (trimmed.front() == '[' && trimmed.back() == ']') {
            currentSection = TrimAscii(trimmed.substr(1, trimmed.size() - 2));
            if (!currentSection.empty()) {
                document->sections[currentSection];
            }
            continue;
        }

        const std::string::size_type equals = trimmed.find('=');
        if (equals == std::string::npos) {
            continue;
        }

        const std::string key = TrimAscii(trimmed.substr(0, equals));
        const std::string value = TrimAscii(trimmed.substr(equals + 1));
        if (!currentSection.empty() && !key.empty()) {
            document->sections[currentSection][key] = value;
        }
    }

    return true;
}

bool SaveIniDocument(const std::filesystem::path& path, const IniDocument& document)
{
    std::error_code ec;
    const std::filesystem::path parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, ec);
    }

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }

    output << "; OpenMidgard settings\n";
    for (const auto& sectionPair : document.sections) {
        output << '\n' << '[' << sectionPair.first << "]\n";
        for (const auto& valuePair : sectionPair.second) {
            output << valuePair.first << '=' << valuePair.second << "\n";
        }
    }

    return output.good();
}

bool EnsureDefaultValues(IniDocument* document)
{
    if (!document) {
        return false;
    }

    bool changed = false;
    for (const DefaultIniValue& entry : kDefaultIniValues) {
        std::map<std::string, std::string>& sectionValues = document->sections[entry.section];
        if (sectionValues.find(entry.key) == sectionValues.end()) {
            sectionValues[entry.key] = entry.value;
            changed = true;
        }
    }
    return changed;
}

bool TryParseInt(const std::string& text, int* value)
{
    if (!value) {
        return false;
    }

    char* end = nullptr;
    const long parsed = std::strtol(text.c_str(), &end, 10);
    if (end == text.c_str() || (end && *end != '\0')) {
        return false;
    }

    *value = static_cast<int>(parsed);
    return true;
}

bool LoadDocumentWithDefaults(IniDocument* document, bool* changed)
{
    if (!document) {
        return false;
    }

    const std::filesystem::path iniPath = GetOpenMidgardIniPath();
    const bool existed = LoadIniDocument(iniPath, document);
    const bool defaultsChanged = EnsureDefaultValues(document);
    if (changed) {
        *changed = (!existed) || defaultsChanged;
    }
    if ((!existed) || defaultsChanged) {
        return SaveIniDocument(iniPath, *document);
    }
    return true;
}

} // namespace

std::filesystem::path GetOpenMidgardIniPath()
{
    const std::filesystem::path overrideDirectory = GetIniDirectoryOverrideInternal();
    if (!overrideDirectory.empty()) {
        return overrideDirectory / "open-midgard.ini";
    }
    return GetExecutableDirectoryInternal() / "open-midgard.ini";
}

void EnsureOpenMidgardIniDefaults()
{
    std::lock_guard<std::mutex> lock(g_iniMutex);
    IniDocument document;
    LoadDocumentWithDefaults(&document, nullptr);
}

bool TryLoadSettingsIniInt(const char* section, const char* key, int* value)
{
    if (!section || !*section || !key || !*key || !value) {
        return false;
    }

    std::lock_guard<std::mutex> lock(g_iniMutex);
    IniDocument document;
    LoadDocumentWithDefaults(&document, nullptr);

    const auto sectionIt = document.sections.find(section);
    if (sectionIt == document.sections.end()) {
        return false;
    }

    const auto valueIt = sectionIt->second.find(key);
    if (valueIt == sectionIt->second.end()) {
        return false;
    }

    return TryParseInt(valueIt->second, value);
}

bool TryLoadSettingsIniString(const char* section, const char* key, std::string* value)
{
    if (!section || !*section || !key || !*key || !value) {
        return false;
    }

    std::lock_guard<std::mutex> lock(g_iniMutex);
    IniDocument document;
    LoadDocumentWithDefaults(&document, nullptr);

    const auto sectionIt = document.sections.find(section);
    if (sectionIt == document.sections.end()) {
        return false;
    }

    const auto valueIt = sectionIt->second.find(key);
    if (valueIt == sectionIt->second.end()) {
        return false;
    }

    *value = valueIt->second;
    return true;
}

int LoadSettingsIniInt(const char* section, const char* key, int defaultValue)
{
    if (!section || !*section || !key || !*key) {
        return defaultValue;
    }

    std::lock_guard<std::mutex> lock(g_iniMutex);
    IniDocument document;
    LoadDocumentWithDefaults(&document, nullptr);

    std::map<std::string, std::string>& sectionValues = document.sections[section];
    auto existing = sectionValues.find(key);
    if (existing == sectionValues.end()) {
        sectionValues[key] = std::to_string(defaultValue);
        SaveIniDocument(GetOpenMidgardIniPath(), document);
        return defaultValue;
    }

    int parsedValue = defaultValue;
    if (!TryParseInt(existing->second, &parsedValue)) {
        existing->second = std::to_string(defaultValue);
        SaveIniDocument(GetOpenMidgardIniPath(), document);
        return defaultValue;
    }

    return parsedValue;
}

std::string LoadSettingsIniString(const char* section, const char* key, const char* defaultValue)
{
    const std::string fallback = defaultValue ? defaultValue : "";
    if (!section || !*section || !key || !*key) {
        return fallback;
    }

    std::lock_guard<std::mutex> lock(g_iniMutex);
    IniDocument document;
    LoadDocumentWithDefaults(&document, nullptr);

    std::map<std::string, std::string>& sectionValues = document.sections[section];
    auto existing = sectionValues.find(key);
    if (existing == sectionValues.end()) {
        sectionValues[key] = fallback;
        SaveIniDocument(GetOpenMidgardIniPath(), document);
        return fallback;
    }

    return existing->second;
}

bool SaveSettingsIniInt(const char* section, const char* key, int value)
{
    if (!section || !*section || !key || !*key) {
        return false;
    }

    std::lock_guard<std::mutex> lock(g_iniMutex);
    IniDocument document;
    LoadDocumentWithDefaults(&document, nullptr);
    document.sections[section][key] = std::to_string(value);
    return SaveIniDocument(GetOpenMidgardIniPath(), document);
}

bool SaveSettingsIniString(const char* section, const char* key, const std::string& value)
{
    if (!section || !*section || !key || !*key) {
        return false;
    }

    std::lock_guard<std::mutex> lock(g_iniMutex);
    IniDocument document;
    LoadDocumentWithDefaults(&document, nullptr);
    document.sections[section][key] = value;
    return SaveIniDocument(GetOpenMidgardIniPath(), document);
}