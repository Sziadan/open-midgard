#include "SettingsIni.h"
#include "DebugLog.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#if RO_PLATFORM_WINDOWS
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#if defined(__linux__)
#include <limits.h>
#endif
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
    { "Debug", "AssetLoadPerf", "0" },
    { "Debug", "ExperimentalWorldTick16", "0" },
    { "Debug", "WorldTickPerf", "0" },
    { "Debug", "LogInfo", "0" },
    { "Debug", "ActorTrace", "0" },
    { "Debug", "EffectTrace", "0" },
    { "Debug", "GameplayTrace", "0" },
    { "Debug", "RenderPerf", "0" },
    { "Debug", "ReplayTrace", "0" },
    { "Graphics", "Width", "640" },
    { "Graphics", "Height", "480" },
    { "Graphics", "WindowMode", "0" },
    { "Graphics", "TextureUpscale", "1" },
    { "Graphics", "TextureUpscaleAlgorithm", "0" },
    { "Graphics", "SSAO", "0" },
    { "Graphics", "ConfusionWaves", "1" },
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
    { "Option", "isEffectOn", "1" },
    { "Option", "m_bMakeMissEffect", "1" },
    { "Option", "m_isNoCtrl", "0" },
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

class SettingsIniLock {
public:
    explicit SettingsIniLock(const std::filesystem::path& path) : m_threadLock(g_iniMutex)
    {
        std::error_code error;
        const auto parent = path.parent_path();
        if (!parent.empty()) {
            std::filesystem::create_directories(parent, error);
            if (error) {
                DbgError("[Settings] cannot create directory '%s': %s\n", parent.string().c_str(), error.message().c_str());
                return;
            }
        }
        auto lockPath = path;
        lockPath += ".lock";
#if RO_PLATFORM_WINDOWS
        m_handle = CreateFileW(lockPath.c_str(), GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (m_handle == INVALID_HANDLE_VALUE) {
            DbgError("[Settings] cannot open lock '%s' (error %lu)\n", lockPath.string().c_str(), GetLastError());
            return;
        }
        OVERLAPPED overlapped{};
        if (!LockFileEx(m_handle, LOCKFILE_EXCLUSIVE_LOCK, 0, 1, 0, &overlapped)) {
            DbgError("[Settings] cannot lock '%s' (error %lu)\n", lockPath.string().c_str(), GetLastError());
            CloseHandle(m_handle);
            m_handle = INVALID_HANDLE_VALUE;
            return;
        }
#else
        m_handle = open(lockPath.c_str(), O_CREAT | O_RDWR, 0666);
        if (m_handle < 0) {
            DbgError("[Settings] cannot open lock '%s' (error %d)\n", lockPath.string().c_str(), errno);
            return;
        }
        int result;
        do {
            result = flock(m_handle, LOCK_EX);
        } while (result < 0 && errno == EINTR);
        if (result < 0) {
            DbgError("[Settings] cannot lock '%s' (error %d)\n", lockPath.string().c_str(), errno);
            close(m_handle);
            m_handle = -1;
            return;
        }
#endif
    }

    ~SettingsIniLock()
    {
#if RO_PLATFORM_WINDOWS
        if (m_handle != INVALID_HANDLE_VALUE) {
            OVERLAPPED overlapped{};
            UnlockFileEx(m_handle, 0, 1, 0, &overlapped);
            CloseHandle(m_handle);
        }
#else
        if (m_handle >= 0) {
            flock(m_handle, LOCK_UN);
            close(m_handle);
        }
#endif
    }

    bool acquired() const
    {
#if RO_PLATFORM_WINDOWS
        return m_handle != INVALID_HANDLE_VALUE;
#else
        return m_handle >= 0;
#endif
    }

private:
    std::unique_lock<std::mutex> m_threadLock;
#if RO_PLATFORM_WINDOWS
    HANDLE m_handle = INVALID_HANDLE_VALUE;
#else
    int m_handle = -1;
#endif
};

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

enum class IniLoadResult { Loaded, Missing, Error };

IniLoadResult LoadIniDocument(const std::filesystem::path& path, IniDocument* document)
{
    if (!document) {
        return IniLoadResult::Error;
    }

    document->sections.clear();

    std::ifstream input(path);
    if (!input.is_open()) {
        std::error_code error;
        const bool exists = std::filesystem::exists(path, error);
        if (!exists && (!error || error == std::errc::no_such_file_or_directory)) {
            return IniLoadResult::Missing;
        }
        DbgError("[Settings] cannot read '%s'%s%s\n", path.string().c_str(),
            error ? ": " : "", error ? error.message().c_str() : "");
        return IniLoadResult::Error;
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

    if (input.bad()) {
        DbgError("[Settings] error reading '%s'\n", path.string().c_str());
        return IniLoadResult::Error;
    }
    return IniLoadResult::Loaded;
}

bool SaveIniDocument(const std::filesystem::path& path, const IniDocument& document)
{
    std::ostringstream contents;
    contents << "; OpenMidgard settings\n";
    for (const auto& sectionPair : document.sections) {
        contents << '\n' << '[' << sectionPair.first << "]\n";
        for (const auto& valuePair : sectionPair.second) {
            contents << valuePair.first << '=' << valuePair.second << "\n";
        }
    }
    if (!contents) {
        DbgError("[Settings] cannot serialize '%s'\n", path.string().c_str());
        return false;
    }

#if RO_PLATFORM_WINDOWS
    wchar_t temporaryName[MAX_PATH] = {};
    const auto parent = path.parent_path().empty() ? std::filesystem::path(L".") : path.parent_path();
    if (!GetTempFileNameW(parent.c_str(), L"omi", 0, temporaryName)) {
        DbgError("[Settings] cannot create temporary file for '%s' (error %lu)\n", path.string().c_str(), GetLastError());
        return false;
    }
    const std::filesystem::path temporary(temporaryName);
#else
    std::string pattern = (path.string() + ".tmp.XXXXXX");
    std::vector<char> name(pattern.begin(), pattern.end());
    name.push_back('\0');
    const int fd = mkstemp(name.data());
    if (fd < 0) {
        DbgError("[Settings] cannot create temporary file for '%s' (error %d)\n", path.string().c_str(), errno);
        return false;
    }
    struct stat existing{};
    if (stat(path.c_str(), &existing) == 0) {
        if (fchmod(fd, existing.st_mode & 0777) != 0) {
            DbgError("[Settings] cannot preserve permissions for '%s' (error %d)\n", path.string().c_str(), errno);
            close(fd);
            if (unlink(name.data()) != 0)
                DbgError("[Settings] cannot remove temporary file '%s' (error %d)\n", name.data(), errno);
            return false;
        }
    } else if (errno != ENOENT) {
        DbgError("[Settings] cannot inspect permissions for '%s' (error %d)\n", path.string().c_str(), errno);
        close(fd);
        if (unlink(name.data()) != 0)
            DbgError("[Settings] cannot remove temporary file '%s' (error %d)\n", name.data(), errno);
        return false;
    }
    if (close(fd) != 0) {
        DbgError("[Settings] cannot close temporary file for '%s' (error %d)\n", path.string().c_str(), errno);
        if (unlink(name.data()) != 0)
            DbgError("[Settings] cannot remove temporary file '%s' (error %d)\n", name.data(), errno);
        return false;
    }
    const std::filesystem::path temporary(name.data());
#endif

    bool success = false;
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (output.is_open()) {
            output << contents.str();
            output.close();
            success = !output.fail();
        }
    }
    if (!success) {
        DbgError("[Settings] cannot write temporary file for '%s'\n", path.string().c_str());
    } else {
#if RO_PLATFORM_WINDOWS
        success = MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
        if (!success) DbgError("[Settings] cannot replace '%s' (error %lu)\n", path.string().c_str(), GetLastError());
#else
        std::error_code error;
        std::filesystem::rename(temporary, path, error);
        success = !error;
        if (!success) DbgError("[Settings] cannot replace '%s': %s\n", path.string().c_str(), error.message().c_str());
#endif
    }
    if (!success) {
        std::error_code error;
        std::filesystem::remove(temporary, error);
        if (error) DbgError("[Settings] cannot remove temporary file '%s': %s\n", temporary.string().c_str(), error.message().c_str());
    }
    return success;
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
    const IniLoadResult result = LoadIniDocument(iniPath, document);
    if (result == IniLoadResult::Error) return false;
    const bool existed = result == IniLoadResult::Loaded;
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

void ConfigureDebugLoggingFromIni()
{
    unsigned int flags = 0;
    if (LoadSettingsIniInt("Debug", "LogInfo", 0)) flags |= DebugLogInfo;
    if (LoadSettingsIniInt("Debug", "ActorTrace", 0)) flags |= DebugLogActors;
    if (LoadSettingsIniInt("Debug", "EffectTrace", 0)) flags |= DebugLogEffects;
    if (LoadSettingsIniInt("Debug", "GameplayTrace", 0)) flags |= DebugLogGameplay;
    if (LoadSettingsIniInt("Debug", "RenderPerf", 0)) flags |= DebugLogPerformance;
    if (LoadSettingsIniInt("Debug", "ReplayTrace", 0)) flags |= DebugLogReplay;
    SetDebugLogFlags(flags);
}

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
    SettingsIniLock lock(GetOpenMidgardIniPath());
    if (!lock.acquired()) return;
    IniDocument document;
    LoadDocumentWithDefaults(&document, nullptr);
}

bool TryLoadSettingsIniInt(const char* section, const char* key, int* value)
{
    if (!section || !*section || !key || !*key || !value) {
        return false;
    }

    SettingsIniLock lock(GetOpenMidgardIniPath());
    if (!lock.acquired()) return false;
    IniDocument document;
    if (!LoadDocumentWithDefaults(&document, nullptr)) return false;

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

    SettingsIniLock lock(GetOpenMidgardIniPath());
    if (!lock.acquired()) return false;
    IniDocument document;
    if (!LoadDocumentWithDefaults(&document, nullptr)) return false;

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

bool HasSettingsIniSection(const char* section)
{
    if (!section || !*section) return false;
    SettingsIniLock lock(GetOpenMidgardIniPath());
    if (!lock.acquired()) return false;
    IniDocument document;
    if (!LoadDocumentWithDefaults(&document, nullptr)) return false;
    return document.sections.find(section) != document.sections.end();
}

int LoadSettingsIniInt(const char* section, const char* key, int defaultValue)
{
    if (!section || !*section || !key || !*key) {
        return defaultValue;
    }

    SettingsIniLock lock(GetOpenMidgardIniPath());
    if (!lock.acquired()) return defaultValue;
    IniDocument document;
    if (!LoadDocumentWithDefaults(&document, nullptr)) return defaultValue;

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

    SettingsIniLock lock(GetOpenMidgardIniPath());
    if (!lock.acquired()) return fallback;
    IniDocument document;
    if (!LoadDocumentWithDefaults(&document, nullptr)) return fallback;

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

    SettingsIniLock lock(GetOpenMidgardIniPath());
    if (!lock.acquired()) return false;
    IniDocument document;
    if (!LoadDocumentWithDefaults(&document, nullptr)) return false;
    document.sections[section][key] = std::to_string(value);
    return SaveIniDocument(GetOpenMidgardIniPath(), document);
}

bool SaveSettingsIniString(const char* section, const char* key, const std::string& value)
{
    if (!section || !*section || !key || !*key) {
        return false;
    }

    SettingsIniLock lock(GetOpenMidgardIniPath());
    if (!lock.acquired()) return false;
    IniDocument document;
    if (!LoadDocumentWithDefaults(&document, nullptr)) return false;
    document.sections[section][key] = value;
    return SaveIniDocument(GetOpenMidgardIniPath(), document);
}

bool LoadSettingsIniIntValues(const char* section, std::vector<std::pair<std::string, int>>& values)
{
    if (!section || !*section) {
        return false;
    }
    for (const auto& entry : values) {
        if (entry.first.empty()) {
            return false;
        }
    }

    SettingsIniLock lock(GetOpenMidgardIniPath());
    if (!lock.acquired()) return false;
    IniDocument document;
    if (!LoadDocumentWithDefaults(&document, nullptr)) return false;

    const auto sectionIt = document.sections.find(section);
    if (sectionIt == document.sections.end()) {
        return false;
    }

    std::vector<std::pair<std::string, int>> loadedValues = values;
    for (auto& entry : loadedValues) {
        const auto valueIt = sectionIt->second.find(entry.first);
        if (valueIt == sectionIt->second.end()) {
            continue;
        }
        int parsedValue = entry.second;
        if (TryParseInt(valueIt->second, &parsedValue)) {
            entry.second = parsedValue;
        }
    }
    values.swap(loadedValues);
    return true;
}

bool SaveSettingsIniIntValues(const char* section, const std::vector<std::pair<std::string, int>>& values)
{
    if (!section || !*section) {
        return false;
    }
    for (const auto& entry : values) {
        if (entry.first.empty()) {
            return false;
        }
    }

    SettingsIniLock lock(GetOpenMidgardIniPath());
    if (!lock.acquired()) return false;
    IniDocument document;
    if (!LoadDocumentWithDefaults(&document, nullptr)) return false;
    for (const auto& entry : values) {
        document.sections[section][entry.first] = std::to_string(entry.second);
    }
    return SaveIniDocument(GetOpenMidgardIniPath(), document);
}
