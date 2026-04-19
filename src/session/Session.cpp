#include "Session.h"
#include "../core/File.h"
#include "../core/ClientInfoLocale.h"
#include "../core/Xml.h"
#include "../lua/LuaBridge.h"
#include "DebugLog.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <mmsystem.h>

namespace {

constexpr const char* kHumanSpriteRoot = "data\\sprite\\\xC0\xCE\xB0\xA3\xC1\xB7\\";
constexpr const char* kAccessorySpriteRoot = "data\\sprite\\\xBE\xC7\xBC\xBC\xBB\xE7\xB8\xAE\\";
constexpr const char* kBodyDir = "\xB8\xF6\xC5\xEB";
constexpr const char* kHeadDir = "\xB8\xD3\xB8\xAE\xC5\xEB";
constexpr const char* kImfRoot = "data\\imf\\";
constexpr const char* kBodyPaletteRoot = "data\\palette\\\xB8\xF6\\";
constexpr const char* kHeadPaletteRoot = "data\\palette\\\xB8\xD3\xB8\xAE\\";
constexpr const char* kFemaleSex = "\xBF\xA9";
constexpr const char* kMaleSex = "\xB3\xB2";
constexpr const char* kPlayerClothesWave = "player_clothes.wav";
constexpr const char* kPlayerWoodenMaleWave = "player_wooden_male.wav";
constexpr const char* kPlayerMetalWave = "player_metal.wav";
constexpr const char* kHitMaceWave = "_hit_mace.wav";
constexpr const char* kHitSwordWave = "_hit_sword.wav";
constexpr const char* kHitSpearWave = "_hit_spear.wav";
constexpr const char* kHitAxeWave = "_hit_axe.wav";
constexpr const char* kHitArrowWave = "_hit_arrow.wav";

constexpr const char* kWeaponTokenDagger = "\xB4\xDC\xB0\xCB";
constexpr const char* kWeaponTokenSword = "\xB0\xCB";
constexpr const char* kWeaponTokenAxe = "\xB5\xB5\xB3\xA2";
constexpr const char* kWeaponTokenSpear = "\xC3\xA2";
constexpr const char* kWeaponTokenClub = "\xC5\xAC\xB7\xB4";
constexpr const char* kWeaponTokenRod = "\xB7\xCE\xB5\xE5";
constexpr const char* kWeaponTokenBow = "\xC8\xB0";
constexpr const char* kWeaponTokenBook = "\xC3\xA5";
constexpr const char* kWeaponTokenKnuckle = "\xB3\xCA\xC5\xAC";
constexpr const char* kWeaponTokenInstrument = "\xBE\xC7\xB1\xE2";
constexpr const char* kWeaponTokenWhip = "\xC3\xA4\xC2\xEF";
constexpr const char* kWeaponTokenKatar = "\xC4\xAB\xC5\xB8\xB8\xA3";
constexpr const char* kWeaponTokenPistol = "\xB1\xC7\xC3\xD1";
constexpr const char* kWeaponTokenRifle = "\xB6\xF3\xC0\xCC\xC7\xC3";
constexpr const char* kWeaponTokenGatling = "\xB1\xE2\xB0\xFC\xC3\xD1";
constexpr const char* kWeaponTokenShotgun = "\xBC\xA6\xB0\xC7";
constexpr const char* kWeaponTokenShuriken = "\xBC\xF6\xB8\xAE\xB0\xCB";
constexpr const char* kPaletteTokenLordKnight = "\xB7\xCE\xB5\xE5\xB3\xAA\xC0\xCC\xC6\xAE";
constexpr const char* kPaletteTokenHighPriest = "\xC7\xCF\xC0\xCC\xC7\xC1\xB8\xAE\xBD\xBA\xC6\xAE";
constexpr const char* kPaletteTokenHighWizard = "\xC7\xCF\xC0\xCC\xC0\xA7\xC0\xFA\xB5\xE5";
constexpr const char* kPaletteTokenWhitesmith = "\xC8\xAD\xC0\xCC\xC6\xAE\xBD\xBA\xB9\xCC\xBD\xBA";
constexpr const char* kPaletteTokenSniper = "\xBD\xBA\xB3\xAA\xC0\xCC\xC6\xDB";
constexpr const char* kPaletteTokenAssassinCross = "\xBE\xEE\xBC\xBC\xBD\xC5\xC5\xA9\xB7\xCE\xBD\xBA";
constexpr const char* kPaletteTokenPecoLordKnight = "\xC6\xE4\xC4\xDA\xB7\xCE\xB3\xAA";
constexpr const char* kPaletteTokenPaladin = "\xC6\xC8\xB6\xF3\xB5\xF2";
constexpr const char* kPaletteTokenChampion = "\xC3\xA8\xC7\xC7\xBF\xC2";
constexpr const char* kPaletteTokenProfessor = "\xC7\xC1\xB7\xCE\xC6\xE4\xBC\xAD";
constexpr const char* kPaletteTokenStalker = "\xBD\xBA\xC5\xE4\xC4\xBF";
constexpr const char* kPaletteTokenCreator = "\xC5\xA9\xB8\xAE\xBF\xA1\xC0\xCC\xC5\xCD";
constexpr const char* kPaletteTokenClown = "\xC5\xA9\xB6\xF3\xBF\xEE";
constexpr const char* kPaletteTokenGypsy = "\xC1\xFD\xBD\xC3";
constexpr const char* kPaletteTokenPecoPaladin = "\xC6\xE4\xC4\xDA\xC6\xC8\xB6\xF3";
constexpr const char* kBodyTokenHighPriest = "\xC7\xCF\xC0\xCC\xC7\xC1\xB8\xAE";
constexpr const char* kBodyTokenAssassinCross = "\xBE\xEE\xBD\xD8\xBD\xC5\xC5\xA9\xB7\xCE\xBD\xBA";
constexpr const char* kBodyTokenClown = "\xC5\xAC\xB6\xF3\xBF\xEE";
constexpr const char* kBodyTokenPecoLordKnight = "\xC6\xE4\xC4\xDA\xC6\xE4\xC4\xDA_\xB1\xE2\xBB\xE7_h";
constexpr const char* kBodyTokenPecoPaladin = "\xC6\xE4\xC4\xDA\xC6\xC8\xB6\xF3\xB5\xF2";
constexpr u32 kServerTimeLeadMs = 72u;
constexpr u32 kServerTimeLateToleranceMs = 144u;
constexpr const char* kFogParameterTableFile = "fogparametertable.txt";

int ClampShortcutPageIndex(int page)
{
    return (std::max)(0, (std::min)(page, kShortcutPageCount - 1));
}

std::string TrimAscii(std::string value)
{
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        ++start;
    }

    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }

    return value.substr(start, end - start);
}

std::string ToLowerAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        if (ch >= 0x80u) {
            return static_cast<char>(ch);
        }
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool LoadTextFileFromGameData(const char* fileName, std::string& outText)
{
    outText.clear();
    if (!fileName || !*fileName) {
        return false;
    }

    std::vector<std::string> candidates;
    candidates.emplace_back(fileName);
    if (!std::strchr(fileName, '\\') && !std::strchr(fileName, '/')) {
        candidates.emplace_back(std::string("data\\") + fileName);
        candidates.emplace_back(std::string("data/") + fileName);
    }

    for (const std::string& candidate : candidates) {
        int size = 0;
        unsigned char* bytes = g_fileMgr.GetData(candidate.c_str(), &size);
        if (!bytes || size <= 0) {
            delete[] bytes;
            continue;
        }

        outText.assign(reinterpret_cast<const char*>(bytes), static_cast<size_t>(size));
        delete[] bytes;
        return true;
    }

    return false;
}

constexpr u32 kRosterAccentColors[] = {
    0xFF0A31B8u,
    0xFF0071B8u,
    0xFF000000u,
    0xFF7800A0u,
    0xFF007040u,
    0xFF787000u,
    0xFF780078u,
    0xFF782800u,
    0xFF787828u,
    0xFF000078u,
    0xFF0031B2u,
    0xFF319000u,
    0xFF003180u,
    0xFF319000u,
    0xFF319032u,
    0xFF7800A0u,
};

u32 ResolveRosterAccentColor(int index)
{
    if (index < 0) {
        index = 0;
    }
    constexpr size_t kRosterAccentColorCount = sizeof(kRosterAccentColors) / sizeof(kRosterAccentColors[0]);
    return kRosterAccentColors[static_cast<size_t>(index) % kRosterAccentColorCount];
}

std::string NormalizePartyMapName(std::string value)
{
    if (value.size() > 4) {
        value.resize(value.size() - 3);
        value += "rsw";
    }
    return value;
}

bool NamesEqual(const std::string& left, const char* right)
{
    if (!right) {
        return left.empty();
    }
    return left == right;
}

std::string NormalizeFogMapName(const char* rswName)
{
    std::string normalized = ToLowerAscii(TrimAscii(rswName ? rswName : ""));
    if (normalized.empty()) {
        return normalized;
    }

    if (normalized.size() < 4 || normalized.compare(normalized.size() - 4, 4, ".rsw") != 0) {
        normalized += ".rsw";
    }
    return normalized;
}

constexpr const char* kEnemyHitNormalWaves[] = {
    "_enemy_hit_normal1.wav",
    "_enemy_hit_normal2.wav",
    "_enemy_hit_normal3.wav",
    "_enemy_hit_normal4.wav",
};

constexpr const char* kEnemyHitFireWaves[] = {
    "_enemy_hit_fire1.wav",
    "_enemy_hit_fire2.wav",
};

constexpr const char* kEnemyHitWindWaves[] = {
    "_enemy_hit_wind1.wav",
    "_enemy_hit_wind2.wav",
};

struct JobWaveOverride {
    int job;
    const char* waveName;
};

constexpr JobWaveOverride kJobHitWaveOverrides[] = {
    { 1, kPlayerMetalWave },
    { 3, kPlayerWoodenMaleWave },
    { 6, kPlayerWoodenMaleWave },
    { 7, kPlayerMetalWave },
    { 11, kPlayerWoodenMaleWave },
    { 12, kPlayerWoodenMaleWave },
    { 14, kPlayerMetalWave },
    { 15, kPlayerMetalWave },
    { 17, kPlayerWoodenMaleWave },
    { 19, kPlayerWoodenMaleWave },
    { 20, kPlayerWoodenMaleWave },
    { 24, kPlayerWoodenMaleWave },
    { 25, kPlayerWoodenMaleWave },
    { 52, kPlayerMetalWave },
    { 54, kPlayerWoodenMaleWave },
    { 57, kPlayerWoodenMaleWave },
    { 58, kPlayerMetalWave },
    { 62, kPlayerWoodenMaleWave },
    { 63, kPlayerWoodenMaleWave },
    { 65, kPlayerMetalWave },
    { 66, kPlayerMetalWave },
    { 68, kPlayerWoodenMaleWave },
    { 70, kPlayerWoodenMaleWave },
    { 71, kPlayerWoodenMaleWave },
    { 74, kPlayerMetalWave },
    { 76, kPlayerWoodenMaleWave },
    { 79, kPlayerWoodenMaleWave },
    { 80, kPlayerMetalWave },
    { 84, kPlayerWoodenMaleWave },
    { 85, kPlayerWoodenMaleWave },
    { 87, kPlayerMetalWave },
    { 88, kPlayerMetalWave },
    { 90, kPlayerWoodenMaleWave },
    { 92, kPlayerWoodenMaleWave },
    { 93, kPlayerWoodenMaleWave },
    { 96, kPlayerWoodenMaleWave },
    { 97, kPlayerMetalWave },
    { 98, kPlayerMetalWave },
    { 100, kPlayerWoodenMaleWave },
    { 101, kPlayerMetalWave },
};

struct WeaponWaveOverride {
    int weaponType;
    const char* waveName;
};

constexpr WeaponWaveOverride kWeaponHitWaveOverrides[] = {
    { 0, kHitMaceWave },
    { 1, kHitSwordWave },
    { 2, kHitSwordWave },
    { 3, kHitSwordWave },
    { 4, kHitSpearWave },
    { 5, kHitSpearWave },
    { 6, kHitAxeWave },
    { 7, kHitAxeWave },
    { 8, kHitMaceWave },
    { 9, kHitMaceWave },
    { 10, kHitMaceWave },
    { 11, kHitArrowWave },
    { 15, kHitMaceWave },
    { 23, kHitMaceWave },
};

struct JobTokenEntry {
    int job;
    const char* token;
};

struct JobNameEntry {
    int job;
    const char* name;
};

constexpr JobTokenEntry kJobTokens[] = {
    { 0,  "\xC3\xCA\xBA\xB8\xC0\xDA" },
    { JT_G_MASTER, "\xBF\xEE\xBF\xB5\xC0\xDA" },
    { 1,  "\xB0\xCB\xBB\xE7" },
    { 2,  "\xB8\xB6\xB9\xFD\xBB\xE7" },
    { 3,  "\xB1\xC3\xBC\xF6" },
    { 4,  "\xBC\xBA\xC1\xF7\xC0\xDA" },
    { 5,  "\xBB\xF3\xC0\xCE" },
    { 6,  "\xB5\xB5\xB5\xCF" },
    { 7,  "\xB1\xE2\xBB\xE7" },
    { 8,  "\xC7\xC1\xB8\xAE\xBD\xBA\xC6\xAE" },
    { 9,  "\xC0\xA7\xC0\xFA\xB5\xE5" },
    { 10, "\xC1\xA6\xC3\xB6\xB0\xF8" },
    { 11, "\xC7\xE5\xC5\xCD" },
    { 12, "\xBE\xEE\xBC\xBC\xBD\xC5" },
    { 14, "\xC5\xA9\xB7\xE7\xBC\xBC\xC0\xCC\xB4\xF5" },
    { 15, "\xB8\xF9\xC5\xA9" },
    { 16, "\xBC\xBC\xC0\xCC\xC1\xF6" },
    { 17, "\xB7\xCE\xB1\xD7" },
    { 18, "\xBF\xAC\xB1\xDD\xBC\xFA\xBB\xE7" },
    { 19, "\xB9\xD9\xB5\xE5" },
};

#include "JobNameTable.generated.inc"

const char* GetSexToken(int sex)
{
    return sex ? kMaleSex : kFemaleSex;
}

const char* FindExactJobToken(int job)
{
    for (const JobTokenEntry& entry : kJobTokens) {
        if (entry.job == job) {
            return entry.token;
        }
    }
    return nullptr;
}

const char* GetJobToken(int job)
{
    if (const char* token = FindExactJobToken(job)) {
        return token;
    }
    return kJobTokens[0].token;
}

const char* GetJobCompositionToken(int job)
{
    if (job == JT_G_MASTER) {
        return GetJobToken(0);
    }
    return GetJobToken(job);
}

const char* LookupGeneratedJobName(int job)
{
    const JobNameEntry* const begin = std::begin(kGeneratedJobNames);
    const JobNameEntry* const end = std::end(kGeneratedJobNames);
    const JobNameEntry* const it = std::lower_bound(begin, end, job,
        [](const JobNameEntry& entry, int value) {
            return entry.job < value;
        });
    if (it == end || it->job != job) {
        return nullptr;
    }
    return it->name;
}

int FindGeneratedJobIdByName(const char* name)
{
    if (!name || !*name) {
        return -1;
    }

    for (const JobNameEntry& entry : kGeneratedJobNames) {
        if (entry.name && std::strcmp(entry.name, name) == 0) {
            return entry.job;
        }
    }

    return -1;
}

bool LooksLikeDisplayJobName(const std::string& value)
{
    if (value.empty()) {
        return false;
    }

    bool hasLowercase = false;
    bool hasSpace = false;
    for (char ch : value) {
        const unsigned char uch = static_cast<unsigned char>(ch);
        if (uch >= 'a' && uch <= 'z') {
            hasLowercase = true;
        }
        if (ch == ' ') {
            hasSpace = true;
        }
    }

    return hasLowercase || hasSpace;
}

bool NormalizeLuaDisplayJobName(std::string* value)
{
    if (!value || value->empty()) {
        return false;
    }

    if (value->size() > 2 && value->compare(value->size() - 2, 2, "_W") == 0) {
        value->resize(value->size() - 2);
    }

    return !value->empty();
}

bool TryAcceptLuaDisplayJobName(const std::string& candidate, std::string* outValue)
{
    if (!outValue) {
        return false;
    }

    std::string normalized = candidate;
    if (!NormalizeLuaDisplayJobName(&normalized) || !LooksLikeDisplayJobName(normalized)) {
        return false;
    }

    *outValue = normalized;
    return true;
}

bool TryGetLuaJobDisplayName(int job, int sex, std::string* outValue)
{
    if (!outValue) {
        return false;
    }

    outValue->clear();

    static const char* kJobNameScripts[] = {
        "lua files\\admin\\pcjobname.lub",
        "lua files\\datainfo\\pcjobnamegender.lub",
        "lua files\\datainfo\\jobname.lub",
    };
    for (const char* scriptPath : kJobNameScripts) {
        g_buabridge.LoadRagnarokScriptOnce(scriptPath);
    }

    static const char* kMappingTables[] = {
        "pcJobTbl2",
        "jobtbl",
    };
    static const char* kLegacyGenderTables[] = {
        "PCJobNameGenderTable",
        "PcJobNameGenderTable",
    };

    const char* preferredGenderKey = sex == 1 ? "M" : "F";
    const char* preferredGenderTable = sex == 1 ? "PCJobNameTableMan" : "PCJobNameTableWoman";
    const char* secondaryGenderTable = sex == 1 ? "PCJobNameTable" : "PCJobNameTable";
    const char* alternateGenderTable = sex == 1 ? "PCJobNameTableWoman" : "PCJobNameTableMan";
    const char* kDisplayTables[] = {
        preferredGenderTable,
        secondaryGenderTable,
        alternateGenderTable,
        "JobNameTable",
    };

    auto tryDisplayTableIntegerLookup = [&](const char* tableName, int numericKey) {
        std::string candidate;
        return g_buabridge.GetGlobalTableStringByIntegerKey(tableName, numericKey, &candidate)
            && TryAcceptLuaDisplayJobName(candidate, outValue);
    };

    auto tryDisplayTableStringLookup = [&](const char* tableName, const char* stringKey) {
        std::string candidate;
        return g_buabridge.GetGlobalTableStringByStringKey(tableName, stringKey, &candidate)
            && TryAcceptLuaDisplayJobName(candidate, outValue);
    };

    auto tryLegacyNestedGenderLookup = [&](int numericKey) {
        for (const char* tableName : kLegacyGenderTables) {
            std::string candidate;
            if (g_buabridge.GetGlobalTableNestedStringByIntegerKey(tableName, numericKey, preferredGenderKey, &candidate)
                && TryAcceptLuaDisplayJobName(candidate, outValue)) {
                return true;
            }
        }
        return false;
    };

    auto tryDisplayLookupsByInteger = [&](int numericKey) {
        for (const char* tableName : kDisplayTables) {
            if (tryDisplayTableIntegerLookup(tableName, numericKey)) {
                return true;
            }
        }
        return tryLegacyNestedGenderLookup(numericKey);
    };

    auto tryDisplayLookupsByString = [&](const char* stringKey) {
        for (const char* tableName : kDisplayTables) {
            if (tryDisplayTableStringLookup(tableName, stringKey)) {
                return true;
            }
        }
        return false;
    };

    if (tryDisplayLookupsByInteger(job)) {
        return true;
    }

    if (const char* generatedName = LookupGeneratedJobName(job)) {
        std::string keyName = "JT_";
        keyName += generatedName;

        if (tryDisplayLookupsByString(keyName.c_str())) {
            return true;
        }

        if (tryDisplayLookupsByString(generatedName)) {
            return true;
        }

        for (const char* tableName : kMappingTables) {
            int mappedJob = 0;
            if (g_buabridge.GetGlobalTableIntegerByStringKey(tableName, keyName.c_str(), &mappedJob)
                || g_buabridge.GetGlobalTableIntegerByStringKey(tableName, generatedName, &mappedJob)) {
                if (tryDisplayLookupsByInteger(mappedJob)) {
                    return true;
                }
            }
        }
    }

    return false;
}

std::string ResolvePlayerResourceJobToken(int job);
std::string ResolvePlayerImfJobToken(int job);
bool ResourceExistsLocalFirst(const char* resourcePath);

const char* ResolveClassicTransExplicitBodyToken(int job)
{
    const int normalizedJob = (job > 3950) ? (job - 3950) : job;
    switch (normalizedJob) {
    case 58: return kPaletteTokenLordKnight;
    case 59: return kBodyTokenHighPriest;
    case 60: return kPaletteTokenHighWizard;
    case 61: return kPaletteTokenWhitesmith;
    case 62: return kPaletteTokenSniper;
    case 63: return kBodyTokenAssassinCross;
    case 64: return kBodyTokenPecoLordKnight;
    case 65: return kPaletteTokenPaladin;
    case 66: return kPaletteTokenChampion;
    case 67: return kPaletteTokenProfessor;
    case 68: return kPaletteTokenStalker;
    case 69: return kPaletteTokenCreator;
    case 70: return kBodyTokenClown;
    case 71: return kPaletteTokenGypsy;
    case 72: return kBodyTokenPecoPaladin;
    default:
        return nullptr;
    }
}

void EnsureLuaJobIdentityScriptsLoaded()
{
    static const char* kIdentityScripts[] = {
        "lua files\\admin\\pcidentity.lub",
        "lua files\\admin\\pcjobname.lub",
        "lua files\\datainfo\\pcjobnamegender.lub",
        "lua files\\datainfo\\jobname.lub",
        "lua files\\datainfo\\jobidentity.lub",
    };

    for (const char* scriptPath : kIdentityScripts) {
        g_buabridge.LoadRagnarokScriptOnce(scriptPath);
    }
}

void AppendUniqueString(std::vector<std::string>* values, const std::string& value)
{
    if (!values || value.empty()) {
        return;
    }

    if (std::find(values->begin(), values->end(), value) == values->end()) {
        values->push_back(value);
    }
}

bool TryNormalizeLuaJobSymbol(const std::string& candidate, std::string* outValue)
{
    if (!outValue || candidate.empty()) {
        return false;
    }

    if (candidate.rfind("JT_", 0) == 0) {
        *outValue = candidate;
        return true;
    }

    bool sawLetter = false;
    for (char ch : candidate) {
        const unsigned char uch = static_cast<unsigned char>(ch);
        if (uch >= 'A' && uch <= 'Z') {
            sawLetter = true;
            continue;
        }
        if ((uch >= '0' && uch <= '9') || ch == '_') {
            continue;
        }
        return false;
    }

    if (!sawLetter) {
        return false;
    }

    *outValue = "JT_" + candidate;
    return true;
}

std::vector<std::string> CollectLuaJobSymbolCandidates(int job)
{
    EnsureLuaJobIdentityScriptsLoaded();

    std::vector<std::string> candidates;

    auto addCandidate = [&](const std::string& rawValue) {
        std::string normalized;
        if (TryNormalizeLuaJobSymbol(rawValue, &normalized)) {
            AppendUniqueString(&candidates, normalized);
        }
    };

    if (const char* generatedName = LookupGeneratedJobName(job)) {
        addCandidate(generatedName);
        addCandidate(std::string("JT_") + generatedName);
    }

    return candidates;
}

bool LooksLikeLuaIdentityToken(const std::string& value)
{
    return !value.empty() && value.rfind("JT_", 0) != 0;
}

bool TryGetLuaPlayerIdentityToken(int job, std::string* outValue)
{
    if (!outValue) {
        return false;
    }

    outValue->clear();
    EnsureLuaJobIdentityScriptsLoaded();

    bool traceLookup = false;
    switch (job) {
    case 4009: {
        static bool logged = false;
        if (!logged) {
            logged = true;
            traceLookup = true;
        }
        break;
    }
    case 4013: {
        static bool logged = false;
        if (!logged) {
            logged = true;
            traceLookup = true;
        }
        break;
    }
    default:
        break;
    }

    const std::vector<std::string> keyCandidates = CollectLuaJobSymbolCandidates(job);
    std::string candidate;

    if (traceLookup) {
        DbgLog("[Session] JobIdentity lookup start job=%d generated='%s' candidateCount=%u\n",
            job,
            LookupGeneratedJobName(job) ? LookupGeneratedJobName(job) : "",
            static_cast<unsigned int>(keyCandidates.size()));
    }

    auto acceptCandidate = [&](const std::string& value) {
        if (!LooksLikeLuaIdentityToken(value)) {
            if (traceLookup) {
                DbgLog("[Session] JobIdentity invalid token job=%d value='%s'\n",
                    job,
                    value.c_str());
            }
            return false;
        }
        *outValue = value;
        if (traceLookup) {
            DbgLog("[Session] JobIdentity accepted job=%d value='%s'\n",
                job,
                value.c_str());
        }
        return true;
    };

    for (const std::string& keyCandidate : keyCandidates) {
        if (g_buabridge.GetGlobalTableStringByStringKey("JobIdentity", keyCandidate.c_str(), &candidate)) {
            if (traceLookup) {
                DbgLog("[Session] JobIdentity['%s'] job=%d -> '%s'\n",
                    keyCandidate.c_str(),
                    job,
                    candidate.c_str());
            }
            if (acceptCandidate(candidate)) {
                return true;
            }
            continue;
        }

        if (traceLookup) {
            DbgLog("[Session] JobIdentity['%s'] job=%d lookup failed error='%s'\n",
                keyCandidate.c_str(),
                job,
                g_buabridge.GetLastError().c_str());
        }
    }

    if (traceLookup) {
        DbgLog("[Session] JobIdentity lookup no match job=%d\n", job);
    }

    return false;
}

std::vector<std::string> BuildPlayerBodyJobTokenCandidates(int job)
{
    std::vector<std::string> candidates;
    if (const char* explicitToken = ResolveClassicTransExplicitBodyToken(job)) {
        AppendUniqueString(&candidates, explicitToken);
    }
    AppendUniqueString(&candidates, ResolvePlayerResourceJobToken(job));
    return candidates;
}

std::vector<std::string> BuildPlayerImfJobTokenCandidates(int job)
{
    std::vector<std::string> candidates;
    AppendUniqueString(&candidates, ResolvePlayerImfJobToken(job));
    return candidates;
}

bool HasPlayerBodyResourcePairForToken(const std::string& jobToken, int sex)
{
    if (jobToken.empty()) {
        return false;
    }

    const char* sexToken = GetSexToken(sex);
    char actPath[260] = {};
    char sprPath[260] = {};
    std::sprintf(actPath, "%s%s\\%s\\%s_%s.act", kHumanSpriteRoot, kBodyDir, sexToken, jobToken.c_str(), sexToken);
    std::sprintf(sprPath, "%s%s\\%s\\%s_%s.spr", kHumanSpriteRoot, kBodyDir, sexToken, jobToken.c_str(), sexToken);
    return ResourceExistsLocalFirst(actPath) && ResourceExistsLocalFirst(sprPath);
}

std::string ResolveExistingPlayerBodyJobToken(int job, int sex)
{
    const std::vector<std::string> candidates = BuildPlayerBodyJobTokenCandidates(job);
    for (const std::string& candidate : candidates) {
        if (HasPlayerBodyResourcePairForToken(candidate, sex)) {
            return candidate;
        }
    }

    return ResolvePlayerResourceJobToken(job);
}

std::string ResolvePlayerResourceJobToken(int job)
{
    if (job == JT_G_MASTER) {
        return std::string(GetJobToken(0));
    }

    auto tryResolveClassicTransBodyToken = [&](bool secondStage) -> std::string {
        const int baseJob = (job > 3950) ? (job - 3950) : job;
        if (const char* baseToken = FindExactJobToken(baseJob)) {
            return secondStage ? (std::string(baseToken) + "_h") : std::string(baseToken);
        }
        return std::string();
    };

    switch (job) {
    case 4001: // NOVICE_H
    case 4002: // SWORDMAN_H
    case 4003: // MAGICIAN_H
    case 4004: // ARCHER_H
    case 4005: // ACOLYTE_H
    case 4006: // MERCHANT_H
    case 4007: // THIEF_H
        if (std::string token = tryResolveClassicTransBodyToken(false); !token.empty()) {
            return token;
        }
        break;
    case 4008: // KNIGHT_H
    case 4009: // PRIEST_H
    case 4010: // WIZARD_H
    case 4011: // BLACKSMITH_H
    case 4012: // HUNTER_H
    case 4013: // ASSASSIN_H
    case 4014: // CHICKEN_H
    case 4015: // CRUSADER_H
    case 4016: // MONK_H
    case 4017: // SAGE_H
    case 4018: // ROGUE_H
    case 4019: // ALCHEMIST_H
    case 4020: // BARD_H
    case 4021: // DANCER_H
    case 4022: // CHICKEN2_H
        if (std::string token = tryResolveClassicTransBodyToken(true); !token.empty()) {
            return token;
        }
        break;
    default:
        break;
    }

    if (const char* token = FindExactJobToken(job)) {
        return std::string(token);
    }

    const char* generatedName = LookupGeneratedJobName(job);
    if (generatedName && *generatedName) {
        struct JobSuffixAlias {
            const char* generatedSuffix;
            const char* resourceSuffix;
        };

        static const JobSuffixAlias kSuffixAliases[] = {
            { "_H", "_h" },
            { "_B", "_b" },
        };

        const std::string generatedNameString(generatedName);
        for (const JobSuffixAlias& alias : kSuffixAliases) {
            const size_t suffixLength = std::strlen(alias.generatedSuffix);
            if (generatedNameString.size() <= suffixLength
                || generatedNameString.compare(generatedNameString.size() - suffixLength, suffixLength, alias.generatedSuffix) != 0) {
                continue;
            }

            const std::string baseName = generatedNameString.substr(0, generatedNameString.size() - suffixLength);
            const int baseJob = FindGeneratedJobIdByName(baseName.c_str());
            if (baseJob < 0) {
                continue;
            }

            if (const char* baseToken = FindExactJobToken(baseJob)) {
                return std::string(baseToken) + alias.resourceSuffix;
            }
        }
    }

    const int normalizedJob = (job > 3950) ? (job - 3950) : job;
    if (const char* token = FindExactJobToken(normalizedJob)) {
        return std::string(token);
    }

    return std::string(GetJobToken(normalizedJob));
}

bool TryGetGeneratedBaseJobToken(int job, std::string* outToken)
{
    if (!outToken) {
        return false;
    }

    outToken->clear();

    const char* generatedName = LookupGeneratedJobName(job);
    if (!generatedName || !*generatedName) {
        return false;
    }

    static const char* kGeneratedSuffixes[] = {
        "_H",
        "_B",
    };

    const std::string generatedNameString(generatedName);
    for (const char* suffix : kGeneratedSuffixes) {
        const size_t suffixLength = std::strlen(suffix);
        if (generatedNameString.size() <= suffixLength
            || generatedNameString.compare(generatedNameString.size() - suffixLength, suffixLength, suffix) != 0) {
            continue;
        }

        const std::string baseName = generatedNameString.substr(0, generatedNameString.size() - suffixLength);
        const int baseJob = FindGeneratedJobIdByName(baseName.c_str());
        if (baseJob < 0) {
            continue;
        }

        if (const char* baseToken = FindExactJobToken(baseJob)) {
            *outToken = baseToken;
            return true;
        }
    }

    return false;
}

std::string ResolvePlayerImfJobToken(int job)
{
    if (job == JT_G_MASTER) {
        return std::string(GetJobToken(0));
    }

    if (std::string baseToken; TryGetGeneratedBaseJobToken(job, &baseToken)) {
        return baseToken;
    }

    if (const char* token = FindExactJobToken(job)) {
        return std::string(token);
    }

    const int normalizedJob = (job > 3950) ? (job - 3950) : job;
    if (const char* token = FindExactJobToken(normalizedJob)) {
        return std::string(token);
    }

    return std::string(GetJobToken(normalizedJob));
}

std::string ResolvePlayerPaletteJobToken(int job)
{
    if (job == JT_G_MASTER) {
        return std::string(GetJobToken(0));
    }

    const int normalizedJob = (job > 3950) ? (job - 3950) : job;
    switch (normalizedJob) {
    case 58: return std::string(kPaletteTokenLordKnight);
    case 59: return std::string(kPaletteTokenHighPriest);
    case 60: return std::string(kPaletteTokenHighWizard);
    case 61: return std::string(kPaletteTokenWhitesmith);
    case 62: return std::string(kPaletteTokenSniper);
    case 63: return std::string(kPaletteTokenAssassinCross);
    case 64: return std::string(kPaletteTokenPecoLordKnight);
    case 65: return std::string(kPaletteTokenPaladin);
    case 66: return std::string(kPaletteTokenChampion);
    case 67: return std::string(kPaletteTokenProfessor);
    case 68: return std::string(kPaletteTokenStalker);
    case 69: return std::string(kPaletteTokenCreator);
    case 70: return std::string(kPaletteTokenClown);
    case 71: return std::string(kPaletteTokenGypsy);
    case 72: return std::string(kPaletteTokenPecoPaladin);
    default:
        break;
    }

    return ResolvePlayerResourceJobToken(job);
}

int NormalizeHeadValue(int head, int job)
{
    if (head == 0) {
        switch (job) {
        case 1: return 2;
        case 2: return 3;
        case 3: return 4;
        case 4: return 5;
        case 5: return 6;
        case 6: return 7;
        default: return 1;
        }
    }
    if (head < 1 || head > 25) {
        return 13;
    }
    return head;
}

std::filesystem::path MakeReferenceSpriteRoot()
{
#ifdef RO_SOURCE_ROOT
    return std::filesystem::path(RO_SOURCE_ROOT) / "Ref" / "GRF-Content" / "data" / "sprite";
#else
    return std::filesystem::current_path() / "Ref" / "GRF-Content" / "data" / "sprite";
#endif
}

std::string ExtractLowByteString(const std::wstring& value)
{
    if (value.empty()) {
        return std::string();
    }

    std::string out;
    out.reserve(value.size());
    for (wchar_t ch : value) {
        if ((static_cast<unsigned int>(ch) & ~0xFFu) != 0) {
            return std::string();
        }
        out.push_back(static_cast<char>(ch & 0xFF));
    }
    return out;
}

bool IsDualWeaponJob(int job)
{
    return job == 12 || job == 4013 || job == 4035;
}

int NormalizeDualWeaponFamilyType(int weaponType)
{
    switch (weaponType) {
    case 1:
        return 1;
    case 2:
    case 3:
        return 2;
    case 6:
    case 7:
        return 6;
    default:
        return 0;
    }
}

int MakeWeaponTypeFromResolvedTypes(int primaryType, int secondaryType)
{
    if (primaryType <= 0) {
        return secondaryType > 0 ? secondaryType : 0;
    }
    if (secondaryType <= 0) {
        return primaryType;
    }

    const int primaryFamily = NormalizeDualWeaponFamilyType(primaryType);
    const int secondaryFamily = NormalizeDualWeaponFamilyType(secondaryType);
    if (primaryFamily == 0 || secondaryFamily == 0) {
        return 0;
    }

    if (primaryFamily == 1 && secondaryFamily == 1) {
        return 25;
    }
    if (primaryFamily == 2 && secondaryFamily == 2) {
        return 26;
    }
    if (primaryFamily == 6 && secondaryFamily == 6) {
        return 27;
    }
    if ((primaryFamily == 1 && secondaryFamily == 2) || (primaryFamily == 2 && secondaryFamily == 1)) {
        return 28;
    }
    if ((primaryFamily == 1 && secondaryFamily == 6) || (primaryFamily == 6 && secondaryFamily == 1)) {
        return 29;
    }
    if ((primaryFamily == 2 && secondaryFamily == 6) || (primaryFamily == 6 && secondaryFamily == 2)) {
        return 30;
    }

    return 0;
}

int ResolveWeaponTypeOrViewType(const CSession& session, int value)
{
    if (value <= 0) {
        return 0;
    }

    if (value <= 31) {
        return value;
    }

    return session.GetWeaponTypeByItemId(value & 0xFFFF);
}

int NormalizePlayerBodyJob(int job)
{
    if (job == JT_G_MASTER) {
        return job;
    }
    return (job > 3950) ? (job - 3950) : job;
}

bool ResourceExistsLocalFirst(const char* resourcePath)
{
    if (!resourcePath || !*resourcePath) {
        return false;
    }

    return g_fileMgr.IsExist(resourcePath);
}

const char* GetPlayerBodyWeaponToken(int weaponType)
{
    switch (weaponType) {
    case 1:
        return kWeaponTokenDagger;
    case 2:
    case 3:
        return kWeaponTokenSword;
    case 4:
    case 5:
        return kWeaponTokenSpear;
    case 6:
    case 7:
        return kWeaponTokenAxe;
    case 8:
    case 9:
        return kWeaponTokenClub;
    case 10:
    case 23:
        return kWeaponTokenRod;
    case 11:
        return kWeaponTokenBow;
    case 12:
        return kWeaponTokenKnuckle;
    case 13:
        return kWeaponTokenInstrument;
    case 14:
        return kWeaponTokenWhip;
    case 15:
        return kWeaponTokenBook;
    case 16:
        return kWeaponTokenKatar;
    case 17:
        return kWeaponTokenPistol;
    case 18:
        return kWeaponTokenRifle;
    case 19:
        return kWeaponTokenGatling;
    case 20:
        return kWeaponTokenShotgun;
    case 22:
        return kWeaponTokenShuriken;
    default:
        return nullptr;
    }
}

int ResolvePlayerBodyWeaponType(const CSession& session, int job, int weaponItemId)
{
    if (weaponItemId <= 0) {
        return 0;
    }

    if (weaponItemId <= 31) {
        return weaponItemId;
    }

    if (IsDualWeaponJob(job)) {
        const int primaryWeapon = weaponItemId & 0xFFFF;
        const int secondaryWeapon = static_cast<int>((static_cast<unsigned int>(weaponItemId) >> 16) & 0xFFFFu);
        if (secondaryWeapon != 0 && session.GetWeaponTypeByItemId(secondaryWeapon) > 0) {
            return session.MakeWeaponTypeByItemId(primaryWeapon, secondaryWeapon);
        }
        return session.GetWeaponTypeByItemId(primaryWeapon);
    }

    return session.GetWeaponTypeByItemId(weaponItemId & 0xFFFF);
}

char* BuildPlayerBodyResourceName(const CSession& session,
    int job,
    int sex,
    int weaponItemId,
    const char* extension,
    char* buf)
{
    const int normalizedJob = NormalizePlayerBodyJob(job);
    const char* sexToken = GetSexToken(sex);
    const std::vector<std::string> jobTokens = BuildPlayerBodyJobTokenCandidates(job);
    std::string fallbackToken = ResolvePlayerResourceJobToken(job);
    if (!jobTokens.empty()) {
        fallbackToken = jobTokens.back();
    }

    char fallbackPath[260] = {};
    std::sprintf(fallbackPath, "%s%s\\%s\\%s_%s.%s", kHumanSpriteRoot, kBodyDir, sexToken, fallbackToken.c_str(), sexToken, extension);

    auto hasMatchingBodyResourcePair = [&](const char* candidatePath) {
        if (!candidatePath || !*candidatePath || !ResourceExistsLocalFirst(candidatePath)) {
            return false;
        }

        char companionPath[260] = {};
        std::strcpy(companionPath, candidatePath);
        char* dot = std::strrchr(companionPath, '.');
        if (!dot) {
            return false;
        }

        const bool wantAct = std::strcmp(extension, "act") == 0;
        std::strcpy(dot + 1, wantAct ? "spr" : "act");
        return ResourceExistsLocalFirst(companionPath);
    };

    const int weaponType = ResolvePlayerBodyWeaponType(session, normalizedJob, weaponItemId);
    for (const std::string& jobToken : jobTokens) {
        char basePath[260] = {};
        std::sprintf(basePath, "%s%s\\%s\\%s_%s.%s", kHumanSpriteRoot, kBodyDir, sexToken, jobToken.c_str(), sexToken, extension);

        if (weaponType > 0) {
            char candidate[260] = {};
            std::sprintf(candidate, "%s%s\\%s\\%s_%s_%d.%s", kHumanSpriteRoot, kBodyDir, sexToken, jobToken.c_str(), sexToken, weaponType, extension);
            if (hasMatchingBodyResourcePair(candidate)) {
                std::strcpy(buf, candidate);
                return buf;
            }

            const std::string weaponTokenFromLua = session.GetPlayerWeaponToken(weaponType);
            if (!weaponTokenFromLua.empty()) {
                std::sprintf(candidate, "%s%s\\%s\\%s_%s_%s.%s", kHumanSpriteRoot, kBodyDir, sexToken, jobToken.c_str(), sexToken, weaponTokenFromLua.c_str(), extension);
                if (hasMatchingBodyResourcePair(candidate)) {
                    std::strcpy(buf, candidate);
                    return buf;
                }
            }

            if (const char* weaponToken = GetPlayerBodyWeaponToken(weaponType)) {
                std::sprintf(candidate, "%s%s\\%s\\%s_%s_%s.%s", kHumanSpriteRoot, kBodyDir, sexToken, jobToken.c_str(), sexToken, weaponToken, extension);
                if (hasMatchingBodyResourcePair(candidate)) {
                    std::strcpy(buf, candidate);
                    return buf;
                }
            }
        }

        if (hasMatchingBodyResourcePair(basePath)) {
            std::strcpy(buf, basePath);
            return buf;
        }
    }

    std::strcpy(buf, fallbackPath);
    return buf;
}

} // namespace

CSession::CSession() : m_aid(0), m_authCode(0), m_sex(0), m_isEffectOn(true), m_isMinEffect(false), m_fogOn(true),
    m_charServerPort(0), m_pendingReturnToCharSelect(0),
    m_playerPosX(0), m_playerPosY(0), m_playerDir(0), m_playerJob(0), m_playerHead(0), m_playerBodyPalette(0),
    m_playerHeadPalette(0), m_playerWeapon(0), m_playerShield(0), m_playerAccessory(0), m_playerAccessory2(0),
    m_playerAccessory3(0), m_serverTime(0), m_numLatePacket(0), m_hasSelectedCharacterInfo(false), m_baseExpValue(0),
    m_nextBaseExpValue(0), m_jobExpValue(0), m_nextJobExpValue(0), m_hasBaseExpValue(false),
    m_hasNextBaseExpValue(false), m_hasJobExpValue(false), m_hasNextJobExpValue(false),
    m_fogParameterTableLoaded(false), m_accessoryNameTableLoaded(false), m_GaugePacket(0)
{
    std::memset(m_userId, 0, sizeof(m_userId));
    std::memset(m_userPassword, 0, sizeof(m_userPassword));
    std::memset(m_charServerAddr, 0, sizeof(m_charServerAddr));
    std::memset(m_curMap, 0, sizeof(m_curMap));
    std::memset(m_playerName, 0, sizeof(m_playerName));
    std::memset(&m_selectedCharacterInfo, 0, sizeof(m_selectedCharacterInfo));
    ClearShortcutSlots();
    InitJobHitWaveName();
    InitWeaponHitWaveName();
}

CSession::~CSession()
{
}

bool CSession::InitAccountInfo()
{
    m_accountInfo.clear();

    XMLElement* clientInfo = GetClientInfo();
    if (!clientInfo) return false;

    XMLElement* child = clientInfo->FindChild("connection");
    if (!child) return false;

    do {
        accountInfo info;
        
        XMLElement* disp = child->FindChild("display");
        if (disp) info.display = disp->GetContents();
        
        XMLElement* desc = child->FindChild("desc");
        if (desc) info.desc = desc->GetContents();
        
        XMLElement* ball = child->FindChild("balloon");
        if (ball) info.balloon = ball->GetContents();
        
        XMLElement* addr = child->FindChild("address");
        if (addr) info.address = addr->GetContents();
        
        XMLElement* port = child->FindChild("port");
        if (port) info.port = port->GetContents();
        
        m_accountInfo.push_back(info);
        child = child->FindNext("connection");
    } while (child);

    return true;
}

void CSession::EnsureFogParameterTableLoaded()
{
    if (m_fogParameterTableLoaded) {
        return;
    }

    m_fogParameterTableLoaded = true;
    m_fogParameterTable.clear();

    std::string fogTableText;
    if (!LoadTextFileFromGameData(kFogParameterTableFile, fogTableText)) {
        DbgLog("[Session] Failed to load fog parameter table '%s'.\n", kFogParameterTableFile);
        return;
    }

    std::vector<std::string> tokens;
    std::string current;
    for (char ch : fogTableText) {
        if (ch == '#') {
            std::string token = TrimAscii(current);
            if (!token.empty()) {
                tokens.push_back(std::move(token));
            }
            current.clear();
            continue;
        }

        current.push_back(ch);
    }

    for (size_t index = 0; index + 4 < tokens.size(); index += 5) {
        const std::string mapName = NormalizeFogMapName(tokens[index].c_str());
        if (mapName.empty()) {
            continue;
        }

        SessionFogParameter parameter{};
        parameter.start = static_cast<float>(std::atof(tokens[index + 1].c_str()));
        parameter.end = static_cast<float>(std::atof(tokens[index + 2].c_str()));
        parameter.color = static_cast<u32>(std::strtoul(tokens[index + 3].c_str(), nullptr, 0));
        parameter.density = static_cast<float>(std::atof(tokens[index + 4].c_str()));
        m_fogParameterTable[mapName] = parameter;
    }

    DbgLog("[Session] Loaded %zu fog parameter entries.\n", m_fogParameterTable.size());
}

bool CSession::GetFogParameter(const char* rswName, SessionFogParameter* outParameter)
{
    if (!outParameter) {
        return false;
    }

    EnsureFogParameterTableLoaded();

    const std::string normalizedName = NormalizeFogMapName(rswName);
    const auto it = m_fogParameterTable.find(normalizedName);
    if (it == m_fogParameterTable.end()) {
        return false;
    }

    *outParameter = it->second;
    return true;
}

void CSession::SetServerTime(u32 time) 
{ 
    m_numLatePacket = 0;
    m_serverTime = timeGetTime() - time;
}

void CSession::UpdateServerTime(u32 time)
{
    const u32 now = timeGetTime();
    const u32 serverTime = GetServerTime();
    const u32 predictedStartTime = serverTime + kServerTimeLeadMs;

    if (predictedStartTime < time) {
        m_serverTime = now - time;
        m_numLatePacket = 0;
        return;
    }

    if (predictedStartTime > time + kServerTimeLateToleranceMs) {
        ++m_numLatePacket;
        if (m_numLatePacket < 4) {
            return;
        }

        m_serverTime = now - (serverTime - kServerTimeLeadMs);
    }

    m_numLatePacket = 0;
}

u32 CSession::GetServerTime() const
{
    return timeGetTime() - m_serverTime - kServerTimeLeadMs;
}

void CSession::SetPlayerPosDir(int x, int y, int dir)
{
    m_playerPosX = x;
    m_playerPosY = y;
    m_playerDir = dir;
}

void CSession::SetSelectedCharacterAppearance(const CHARACTER_INFO& info)
{
    m_selectedCharacterInfo = info;
    m_hasSelectedCharacterInfo = true;
    m_skillItems.clear();
    ClearActiveStatusIcons();
    m_hasBaseExpValue = false;
    m_hasNextBaseExpValue = false;
    m_hasJobExpValue = false;
    m_hasNextJobExpValue = false;
    m_playerJob = info.job;
    m_playerHead = info.head;
    m_playerBodyPalette = info.bodypalette;
    m_playerHeadPalette = info.headpalette;
    m_playerWeapon = info.weapon;
    m_playerShield = info.shield;
    m_playerAccessory = info.accessory;
    m_playerAccessory2 = info.accessory2;
    m_playerAccessory3 = info.accessory3;
    m_plusStr = 0;
    m_plusAgi = 0;
    m_plusVit = 0;
    m_plusInt = 0;
    m_plusDex = 0;
    m_plusLuk = 0;
    m_standardStr = 2;
    m_standardAgi = 2;
    m_standardVit = 2;
    m_standardInt = 2;
    m_standardDex = 2;
    m_standardLuk = 2;
    m_attPower = 0;
    m_refiningPower = 0;
    m_maxMatkPower = 0;
    m_minMatkPower = 0;
    m_itemDefPower = 0;
    m_plusDefPower = 0;
    m_mdefPower = 0;
    m_plusMdefPower = 0;
    m_hitSuccessValue = 0;
    m_avoidSuccessValue = 0;
    m_plusAvoidSuccessValue = 0;
    m_criticalSuccessValue = 0;
    m_aspd = 0;
    m_plusAspd = 0;
    ClearNpcShopState();
    CloseStorage();
    ClearShortcutSlots();

    std::memset(m_playerName, 0, sizeof(m_playerName));
    std::memcpy(m_playerName, info.name, sizeof(info.name));
    m_playerName[sizeof(info.name)] = '\0';
    DbgLog("[Session] selected char appearance gid=%u job=%d head=%d weapon=%d shield=%d accBottom=%d accMid=%d accTop=%d headPal=%d bodyPal=%d hairColor=%u slot=%u name='%.24s'\n",
        info.GID,
        static_cast<int>(info.job),
        static_cast<int>(info.head),
        static_cast<int>(info.weapon),
        static_cast<int>(info.shield),
        static_cast<int>(info.accessory),
        static_cast<int>(info.accessory3),
        static_cast<int>(info.accessory2),
        static_cast<int>(info.headpalette),
        static_cast<int>(info.bodypalette),
        static_cast<unsigned int>(info.haircolor),
        static_cast<unsigned int>(info.CharNum),
        reinterpret_cast<const char*>(info.name));
}

const CHARACTER_INFO* CSession::GetSelectedCharacterInfo() const
{
    return m_hasSelectedCharacterInfo ? &m_selectedCharacterInfo : nullptr;
}

CHARACTER_INFO* CSession::GetMutableSelectedCharacterInfo()
{
    return m_hasSelectedCharacterInfo ? &m_selectedCharacterInfo : nullptr;
}

void CSession::SetBaseExpValue(int value)
{
    m_baseExpValue = (std::max)(value, 0);
    m_hasBaseExpValue = true;
}

void CSession::SetNextBaseExpValue(int value)
{
    m_nextBaseExpValue = (std::max)(value, 0);
    m_hasNextBaseExpValue = true;
}

void CSession::SetJobExpValue(int value)
{
    m_jobExpValue = (std::max)(value, 0);
    m_hasJobExpValue = true;
}

void CSession::SetNextJobExpValue(int value)
{
    m_nextJobExpValue = (std::max)(value, 0);
    m_hasNextJobExpValue = true;
}

bool CSession::TryGetBaseExpPercent(int* outPercent) const
{
    if (!outPercent || !m_hasBaseExpValue || !m_hasNextBaseExpValue || m_nextBaseExpValue <= 0) {
        return false;
    }

    const int percent = static_cast<int>((100LL * m_baseExpValue) / m_nextBaseExpValue);
    *outPercent = (std::max)(0, (std::min)(percent, 100));
    return true;
}

bool CSession::TryGetJobExpPercent(int* outPercent) const
{
    if (!outPercent || !m_hasJobExpValue || !m_hasNextJobExpValue || m_nextJobExpValue <= 0) {
        return false;
    }

    const int percent = static_cast<int>((100LL * m_jobExpValue) / m_nextJobExpValue);
    *outPercent = (std::max)(0, (std::min)(percent, 100));
    return true;
}

void CSession::ClearInventoryItems()
{
    m_inventoryItems.clear();
}

void CSession::ClearEquipmentInventoryItems()
{
    for (auto it = m_inventoryItems.begin(); it != m_inventoryItems.end(); ) {
        if (it->m_location != 0) {
            it = m_inventoryItems.erase(it);
        } else {
            ++it;
        }
    }
}

void CSession::ClearStorageItems()
{
    m_storageItems.clear();
}

void CSession::ClearSkillItems()
{
    m_skillItems.clear();
}

void CSession::ClearActiveStatusIcons()
{
    m_activeStatusIcons.clear();
}

void CSession::SetActiveStatusIcon(int statusType, bool active, u32 remainingMs)
{
    if (statusType <= 0) {
        return;
    }

    const auto it = std::find_if(m_activeStatusIcons.begin(), m_activeStatusIcons.end(), [&](const ACTIVE_STATUS_ICON& entry) {
        return entry.statusType == statusType;
    });

    if (!active) {
        if (it != m_activeStatusIcons.end()) {
            m_activeStatusIcons.erase(it);
        }
        return;
    }

    ACTIVE_STATUS_ICON icon;
    icon.statusType = statusType;
    icon.hasTimer = remainingMs > 0;
    icon.expireServerTime = icon.hasTimer ? (GetServerTime() + remainingMs) : 0;

    if (it != m_activeStatusIcons.end()) {
        *it = icon;
        return;
    }

    m_activeStatusIcons.push_back(icon);
}

void CSession::PruneExpiredStatusIcons(u32 serverTime)
{
    m_activeStatusIcons.erase(
        std::remove_if(m_activeStatusIcons.begin(), m_activeStatusIcons.end(), [&](const ACTIVE_STATUS_ICON& entry) {
            return entry.hasTimer && entry.expireServerTime <= serverTime;
        }),
        m_activeStatusIcons.end());
}

void CSession::ClearHomunSkillItems()
{
    m_homunSkillItems.clear();
}

void CSession::ClearMercSkillItems()
{
    m_mercSkillItems.clear();
}

void CSession::SetInventoryItem(const ITEM_INFO& itemInfo)
{
    auto it = std::find_if(m_inventoryItems.begin(), m_inventoryItems.end(), [&](const ITEM_INFO& existing) {
        return existing.m_itemIndex == itemInfo.m_itemIndex;
    });

    if (it != m_inventoryItems.end()) {
        *it = itemInfo;
        return;
    }

    m_inventoryItems.push_back(itemInfo);
}

void CSession::OpenStorage(int currentCount, int maxCount)
{
    m_storageOpen = true;
    m_storageCurrentCount = (std::max)(0, currentCount);
    m_storageMaxCount = (std::max)(0, maxCount);
}

void CSession::CloseStorage()
{
    m_storageOpen = false;
    m_storageCurrentCount = 0;
    m_storageMaxCount = 0;
    ClearStorageItems();
}

void CSession::SetStorageItem(const ITEM_INFO& itemInfo)
{
    auto it = std::find_if(m_storageItems.begin(), m_storageItems.end(), [&](const ITEM_INFO& existing) {
        return existing.m_itemIndex == itemInfo.m_itemIndex;
    });

    if (it != m_storageItems.end()) {
        *it = itemInfo;
        return;
    }

    m_storageItems.push_back(itemInfo);
}

void CSession::AddStorageItem(const ITEM_INFO& itemInfo)
{
    auto it = std::find_if(m_storageItems.begin(), m_storageItems.end(), [&](const ITEM_INFO& existing) {
        return existing.m_itemIndex == itemInfo.m_itemIndex;
    });

    if (it != m_storageItems.end()) {
        it->m_num += itemInfo.m_num;
        if (itemInfo.GetItemId() != 0) {
            it->SetItemId(itemInfo.GetItemId());
        }
        if (itemInfo.m_itemType != 0) {
            it->m_itemType = itemInfo.m_itemType;
        }
        if (itemInfo.m_location != 0) {
            it->m_location = itemInfo.m_location;
        }
        if (itemInfo.m_wearLocation != 0) {
            it->m_wearLocation = itemInfo.m_wearLocation;
        }
        it->m_isIdentified = itemInfo.m_isIdentified;
        it->m_isDamaged = itemInfo.m_isDamaged;
        it->m_refiningLevel = itemInfo.m_refiningLevel;
        it->m_deleteTime = itemInfo.m_deleteTime;
        std::memcpy(it->m_slot, itemInfo.m_slot, sizeof(it->m_slot));
        return;
    }

    m_storageItems.push_back(itemInfo);
    if (m_storageOpen) {
        ++m_storageCurrentCount;
    }
}

void CSession::AddInventoryItem(const ITEM_INFO& itemInfo)
{
    auto it = std::find_if(m_inventoryItems.begin(), m_inventoryItems.end(), [&](const ITEM_INFO& existing) {
        return existing.m_itemIndex == itemInfo.m_itemIndex;
    });

    if (it != m_inventoryItems.end()) {
        it->m_num += itemInfo.m_num;
        if (itemInfo.m_location != 0) {
            it->m_location = itemInfo.m_location;
        }
        if (itemInfo.m_wearLocation != 0) {
            it->m_wearLocation = itemInfo.m_wearLocation;
        }
        return;
    }

    m_inventoryItems.push_back(itemInfo);
}

void CSession::SetSkillItem(const PLAYER_SKILL_INFO& skillInfo)
{
    auto it = std::find_if(m_skillItems.begin(), m_skillItems.end(), [&](const PLAYER_SKILL_INFO& existing) {
        return existing.SKID == skillInfo.SKID;
    });

    if (it != m_skillItems.end()) {
        *it = skillInfo;
        return;
    }

    m_skillItems.push_back(skillInfo);
}

void CSession::SetHomunSkillItem(const PLAYER_SKILL_INFO& skillInfo)
{
    auto it = std::find_if(m_homunSkillItems.begin(), m_homunSkillItems.end(), [&](const PLAYER_SKILL_INFO& existing) {
        return existing.SKID == skillInfo.SKID;
    });

    if (it != m_homunSkillItems.end()) {
        *it = skillInfo;
        return;
    }

    m_homunSkillItems.push_back(skillInfo);
}

void CSession::SetMercSkillItem(const PLAYER_SKILL_INFO& skillInfo)
{
    auto it = std::find_if(m_mercSkillItems.begin(), m_mercSkillItems.end(), [&](const PLAYER_SKILL_INFO& existing) {
        return existing.SKID == skillInfo.SKID;
    });

    if (it != m_mercSkillItems.end()) {
        *it = skillInfo;
        return;
    }

    m_mercSkillItems.push_back(skillInfo);
}

void CSession::RemoveInventoryItem(unsigned int itemIndex, int amount)
{
    for (auto it = m_inventoryItems.begin(); it != m_inventoryItems.end(); ++it) {
        if (it->m_itemIndex != itemIndex) {
            continue;
        }

        if (amount <= 0 || it->m_num <= amount) {
            m_inventoryItems.erase(it);
        } else {
            it->m_num -= amount;
        }
        return;
    }
}

void CSession::RemoveStorageItem(unsigned int itemIndex, int amount)
{
    for (auto it = m_storageItems.begin(); it != m_storageItems.end(); ++it) {
        if (it->m_itemIndex != itemIndex) {
            continue;
        }

        if (amount <= 0 || it->m_num <= amount) {
            m_storageItems.erase(it);
            if (m_storageOpen) {
                m_storageCurrentCount = (std::max)(0, m_storageCurrentCount - 1);
            }
        } else {
            it->m_num -= amount;
        }
        return;
    }
}

bool CSession::SetInventoryItemWearLocation(unsigned int itemIndex, int wearLocation)
{
    for (ITEM_INFO& item : m_inventoryItems) {
        if (item.m_itemIndex != itemIndex) {
            continue;
        }

        item.m_wearLocation = wearLocation;
        return true;
    }

    return false;
}

void CSession::ClearInventoryWearLocationMask(int wearMask, unsigned int exceptItemIndex)
{
    if (wearMask == 0) {
        return;
    }

    for (ITEM_INFO& item : m_inventoryItems) {
        if (item.m_itemIndex == exceptItemIndex) {
            continue;
        }
        if ((item.m_wearLocation & wearMask) == 0) {
            continue;
        }

        item.m_wearLocation &= ~wearMask;
    }
}

void CSession::RebuildPlayerEquipmentAppearanceFromInventory()
{
    int weapon = m_playerWeapon;
    int shield = m_playerShield;
    int accessoryBottom = m_playerAccessory;
    int accessoryTop = m_playerAccessory2;
    int accessoryMid = m_playerAccessory3;
    bool hasWeapon = false;
    bool hasShield = false;
    bool hasAccessoryBottom = false;
    bool hasAccessoryTop = false;
    bool hasAccessoryMid = false;

    for (const ITEM_INFO& item : m_inventoryItems) {
        if (item.m_wearLocation == 0) {
            continue;
        }

        const unsigned int itemId = item.GetItemId();
        const int viewId = g_ttemmgr.GetVisibleHeadgearViewId(itemId);
        const bool occupiesRightHand = (item.m_wearLocation & 2) != 0;
        const bool occupiesLeftHand = (item.m_wearLocation & 32) != 0;

        if (occupiesRightHand) {
            hasWeapon = true;
        }
        if (occupiesLeftHand && !occupiesRightHand) {
            hasShield = true;
        }
        if ((item.m_wearLocation & 1) != 0) {
            hasAccessoryBottom = true;
            if (viewId > 0) {
                accessoryBottom = viewId;
            }
        }
        if ((item.m_wearLocation & 256) != 0) {
            hasAccessoryTop = true;
            if (viewId > 0) {
                accessoryTop = viewId;
            }
        }
        if ((item.m_wearLocation & 512) != 0) {
            hasAccessoryMid = true;
            if (viewId > 0) {
                accessoryMid = viewId;
            }
        }
    }

    if (!hasWeapon) {
        weapon = 0;
    }
    if (!hasShield) {
        shield = 0;
    }
    if (!hasAccessoryBottom) {
        accessoryBottom = 0;
    }
    if (!hasAccessoryTop) {
        accessoryTop = 0;
    }
    if (!hasAccessoryMid) {
        accessoryMid = 0;
    }

    m_playerWeapon = weapon;
    m_playerShield = shield;
    m_playerAccessory = accessoryBottom;
    m_playerAccessory2 = accessoryTop;
    m_playerAccessory3 = accessoryMid;

    if (CHARACTER_INFO* info = GetMutableSelectedCharacterInfo()) {
        info->weapon = static_cast<s16>(weapon & 0xFFFF);
        info->shield = static_cast<s16>(shield & 0xFFFF);
        info->accessory = static_cast<s16>(accessoryBottom & 0xFFFF);
        info->accessory2 = static_cast<s16>(accessoryTop & 0xFFFF);
        info->accessory3 = static_cast<s16>(accessoryMid & 0xFFFF);
    }

    DbgLog("[Session] rebuilt inventory appearance weapon=%d shield=%d accBottom=%d accMid=%d accTop=%d items=%u\n",
        weapon,
        shield,
        accessoryBottom,
        accessoryMid,
        accessoryTop,
        static_cast<unsigned int>(m_inventoryItems.size()));
}

const std::list<ITEM_INFO>& CSession::GetInventoryItems() const
{
    return m_inventoryItems;
}

const ITEM_INFO* CSession::GetInventoryItemByIndex(unsigned int itemIndex) const
{
    for (const ITEM_INFO& item : m_inventoryItems) {
        if (item.m_itemIndex == itemIndex) {
            return &item;
        }
    }
    return nullptr;
}

const ITEM_INFO* CSession::GetInventoryItemByItemId(unsigned int itemId) const
{
    const ITEM_INFO* bestMatch = nullptr;
    for (const ITEM_INFO& item : m_inventoryItems) {
        if (item.GetItemId() != itemId || item.m_num <= 0) {
            continue;
        }
        if (item.m_wearLocation == 0) {
            return &item;
        }
        if (!bestMatch) {
            bestMatch = &item;
        }
    }
    return bestMatch;
}

const std::list<ITEM_INFO>& CSession::GetStorageItems() const
{
    return m_storageItems;
}

const ITEM_INFO* CSession::GetStorageItemByIndex(unsigned int itemIndex) const
{
    for (const ITEM_INFO& item : m_storageItems) {
        if (item.m_itemIndex == itemIndex) {
            return &item;
        }
    }
    return nullptr;
}

bool CSession::IsStorageOpen() const
{
    return m_storageOpen;
}

int CSession::GetStorageCurrentCount() const
{
    return m_storageCurrentCount;
}

int CSession::GetStorageMaxCount() const
{
    return m_storageMaxCount;
}

const std::list<PLAYER_SKILL_INFO>& CSession::GetSkillItems() const
{
    return m_skillItems;
}

const std::list<PLAYER_SKILL_INFO>& CSession::GetHomunSkillItems() const
{
    return m_homunSkillItems;
}

const std::list<PLAYER_SKILL_INFO>& CSession::GetMercSkillItems() const
{
    return m_mercSkillItems;
}

const PLAYER_SKILL_INFO* CSession::GetSkillItemBySkillId(int skillId) const
{
    for (const PLAYER_SKILL_INFO& skill : m_skillItems) {
        if (skill.SKID == skillId) {
            return &skill;
        }
    }
    return nullptr;
}

const PLAYER_SKILL_INFO* CSession::GetHomunSkillItemBySkillId(int skillId) const
{
    for (const PLAYER_SKILL_INFO& skill : m_homunSkillItems) {
        if (skill.SKID == skillId) {
            return &skill;
        }
    }
    return nullptr;
}

const PLAYER_SKILL_INFO* CSession::GetMercSkillItemBySkillId(int skillId) const
{
    for (const PLAYER_SKILL_INFO& skill : m_mercSkillItems) {
        if (skill.SKID == skillId) {
            return &skill;
        }
    }
    return nullptr;
}

void CSession::ClearNpcShopState()
{
    m_shopNpcId = 0;
    m_shopMode = NpcShopMode::None;
    m_shopSelectedSourceRow = -1;
    m_shopSelectedDealRow = -1;
    m_shopDealTotal = 0;
    m_shopRows.clear();
    m_shopDealRows.clear();
}

void CSession::SetNpcShopChoice(u32 npcId)
{
    ClearNpcShopState();
    m_shopNpcId = npcId;
}

void CSession::SetNpcShopRows(u32 npcId, NpcShopMode mode, const std::vector<NPC_SHOP_ROW>& rows)
{
    m_shopNpcId = npcId;
    m_shopMode = mode;
    m_shopRows = rows;
    m_shopDealRows.clear();
    m_shopDealTotal = 0;
    m_shopSelectedSourceRow = rows.empty() ? -1 : 0;
    m_shopSelectedDealRow = -1;
}

int CSession::GetNpcShopUnitPrice(const NPC_SHOP_ROW& row) const
{
    if (row.secondaryPrice > 0) {
        return row.secondaryPrice;
    }
    return row.price;
}

void CSession::ClearParty()
{
    m_partyList.clear();
    m_partyName.clear();
    m_amIPartyMaster = false;
    m_partyExpShare = false;
    m_itemDivType = false;
    m_itemCollectType = false;
}

unsigned int CSession::GetNumParty() const
{
    return static_cast<unsigned int>(m_partyList.size());
}

void CSession::AddMemberToParty(const FRIEND_INFO& info)
{
    FRIEND_INFO member = info;
    member.mapName = NormalizePartyMapName(member.mapName);

    int index = 0;
    for (FRIEND_INFO& existing : m_partyList) {
        if (existing.characterName == member.characterName) {
            if (member.partyHp <= 0 && member.partyMaxHp <= 0) {
                member.partyHp = existing.partyHp;
                member.partyMaxHp = existing.partyMaxHp;
            }
            member.color = ResolveRosterAccentColor(index);
            existing = member;
            return;
        }
        ++index;
    }

    member.color = ResolveRosterAccentColor(index);
    m_partyList.push_back(std::move(member));
}

unsigned int CSession::GetMemberAidFromParty(const char* characterName) const
{
    for (const FRIEND_INFO& member : m_partyList) {
        if (NamesEqual(member.characterName, characterName)) {
            return member.AID;
        }
    }
    return 0;
}

void CSession::DeleteMemberFromParty(const char* characterName)
{
    auto it = std::find_if(m_partyList.begin(), m_partyList.end(), [&](const FRIEND_INFO& member) {
        return NamesEqual(member.characterName, characterName);
    });
    if (it != m_partyList.end()) {
        m_partyList.erase(it);
    }
}

void CSession::ChangeRoleFromParty(unsigned int aid, int role)
{
    for (FRIEND_INFO& member : m_partyList) {
        if (member.AID == aid) {
            member.role = role;
            return;
        }
    }
}

bool CSession::SetPartyMemberHp(unsigned int aid, int hp, int maxHp)
{
    for (FRIEND_INFO& member : m_partyList) {
        if (member.AID == aid) {
            member.partyHp = (std::max)(0, hp);
            member.partyMaxHp = (std::max)(0, maxHp);
            return true;
        }
    }

    return false;
}

const FRIEND_INFO* CSession::FindPartyMemberByAid(unsigned int aid) const
{
    for (const FRIEND_INFO& member : m_partyList) {
        if (member.AID == aid) {
            return &member;
        }
    }

    return nullptr;
}

void CSession::RefreshPartyUI()
{
}

const std::list<FRIEND_INFO>& CSession::GetPartyList() const
{
    return m_partyList;
}

void CSession::ClearFriend()
{
    m_friendList.clear();
}

unsigned int CSession::GetNumFriend() const
{
    return static_cast<unsigned int>(m_friendList.size());
}

bool CSession::IsFriendName(const char* characterName) const
{
    return std::any_of(m_friendList.begin(), m_friendList.end(), [&](const FRIEND_INFO& info) {
        return NamesEqual(info.characterName, characterName);
    });
}

bool CSession::DeleteFriendFromList(unsigned int gid)
{
    auto it = std::find_if(m_friendList.begin(), m_friendList.end(), [&](const FRIEND_INFO& info) {
        return info.GID == gid;
    });
    if (it == m_friendList.end()) {
        return false;
    }

    m_friendList.erase(it);
    return true;
}

void CSession::AddFriendToList(const FRIEND_INFO& info)
{
    FRIEND_INFO entry = info;

    int index = 0;
    for (FRIEND_INFO& existing : m_friendList) {
        if (existing.characterName == entry.characterName) {
            entry.color = ResolveRosterAccentColor(index);
            existing = entry;
            return;
        }
        ++index;
    }

    entry.color = ResolveRosterAccentColor(index);
    m_friendList.push_back(std::move(entry));
}

bool CSession::SetFriendState(unsigned int aid, unsigned int gid, unsigned char state)
{
    for (FRIEND_INFO& info : m_friendList) {
        if (info.AID == aid && info.GID == gid) {
            info.state = state;
            return true;
        }
    }
    return false;
}

void CSession::RefreshFriendUI()
{
}

const std::list<FRIEND_INFO>& CSession::GetFriendList() const
{
    return m_friendList;
}

void CSession::ClearShortcutSlots()
{
    m_shortcutPage = 0;
    for (SHORTCUT_SLOT& slot : m_shortcutSlots) {
        slot = SHORTCUT_SLOT{};
    }
}

int CSession::GetShortcutPage() const
{
    return ClampShortcutPageIndex(m_shortcutPage);
}

const std::vector<ACTIVE_STATUS_ICON>& CSession::GetActiveStatusIcons() const
{
    return m_activeStatusIcons;
}

void CSession::SetShortcutPage(int page)
{
    m_shortcutPage = ClampShortcutPageIndex(page);
}

int CSession::GetShortcutSlotAbsoluteIndex(int visibleSlot) const
{
    if (visibleSlot < 0 || visibleSlot >= kShortcutSlotsPerPage) {
        return -1;
    }
    return GetShortcutPage() * kShortcutSlotsPerPage + visibleSlot;
}

const SHORTCUT_SLOT* CSession::GetShortcutSlotByAbsoluteIndex(int absoluteIndex) const
{
    if (absoluteIndex < 0 || absoluteIndex >= kShortcutSlotCount) {
        return nullptr;
    }
    return &m_shortcutSlots[static_cast<size_t>(absoluteIndex)];
}

const SHORTCUT_SLOT* CSession::GetShortcutSlotByVisibleIndex(int visibleSlot) const
{
    return GetShortcutSlotByAbsoluteIndex(GetShortcutSlotAbsoluteIndex(visibleSlot));
}

bool CSession::SetShortcutSlotByAbsoluteIndex(int absoluteIndex, unsigned char isSkill, unsigned int id, unsigned short count)
{
    if (absoluteIndex < 0 || absoluteIndex >= kShortcutSlotCount) {
        return false;
    }

    SHORTCUT_SLOT normalized{};
    if (id != 0) {
        normalized.isSkill = isSkill != 0 ? 1 : 0;
        normalized.id = id;
        normalized.count = count;
    }

    SHORTCUT_SLOT& slot = m_shortcutSlots[static_cast<size_t>(absoluteIndex)];
    if (slot.isSkill == normalized.isSkill
        && slot.id == normalized.id
        && slot.count == normalized.count) {
        return false;
    }

    slot = normalized;
    return true;
}

bool CSession::SetShortcutSlotByVisibleIndex(int visibleSlot, unsigned char isSkill, unsigned int id, unsigned short count)
{
    return SetShortcutSlotByAbsoluteIndex(GetShortcutSlotAbsoluteIndex(visibleSlot), isSkill, id, count);
}

bool CSession::ClearShortcutSlotByAbsoluteIndex(int absoluteIndex)
{
    return SetShortcutSlotByAbsoluteIndex(absoluteIndex, 0, 0, 0);
}

bool CSession::ClearShortcutSlotByVisibleIndex(int visibleSlot)
{
    return ClearShortcutSlotByAbsoluteIndex(GetShortcutSlotAbsoluteIndex(visibleSlot));
}

int CSession::FindShortcutSlotByItemId(unsigned int itemId) const
{
    for (size_t index = 0; index < m_shortcutSlots.size(); ++index) {
        const SHORTCUT_SLOT& slot = m_shortcutSlots[index];
        if (slot.id == itemId && slot.isSkill == 0) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

int CSession::FindShortcutSlotBySkillId(int skillId) const
{
    for (size_t index = 0; index < m_shortcutSlots.size(); ++index) {
        const SHORTCUT_SLOT& slot = m_shortcutSlots[index];
        if (slot.id == static_cast<unsigned int>(skillId) && slot.isSkill != 0) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

bool CSession::AdjustNpcShopDealBySourceRow(size_t sourceRowIndex, int deltaQuantity)
{
    if (deltaQuantity == 0 || sourceRowIndex >= m_shopRows.size()) {
        return false;
    }

    const NPC_SHOP_ROW& row = m_shopRows[sourceRowIndex];
    const int maxQuantity = row.availableCount > 0
        ? row.availableCount
        : static_cast<int>((std::min)(static_cast<long long>(std::numeric_limits<int>::max()), 30000ll));
    if (maxQuantity <= 0 && deltaQuantity > 0) {
        return false;
    }

    auto it = std::find_if(m_shopDealRows.begin(), m_shopDealRows.end(), [&](const NPC_SHOP_DEAL_ROW& dealRow) {
        return dealRow.sourceItemIndex == row.sourceItemIndex
            && dealRow.itemInfo.GetItemId() == row.itemInfo.GetItemId();
    });

    if (it == m_shopDealRows.end()) {
        if (deltaQuantity < 0) {
            return false;
        }

        NPC_SHOP_DEAL_ROW dealRow{};
        dealRow.itemInfo = row.itemInfo;
        dealRow.itemInfo.m_num = 0;
        dealRow.sourceItemIndex = row.sourceItemIndex;
        dealRow.unitPrice = GetNpcShopUnitPrice(row);
        m_shopDealRows.push_back(std::move(dealRow));
        it = std::prev(m_shopDealRows.end());
    }

    const int oldQuantity = it->quantity;
    const int unclampedQuantity = oldQuantity + deltaQuantity;
    const int newQuantity = (std::max)(0, (std::min)(maxQuantity, unclampedQuantity));
    if (newQuantity == oldQuantity) {
        return false;
    }

    if (newQuantity <= 0) {
        const size_t erasedIndex = static_cast<size_t>(std::distance(m_shopDealRows.begin(), it));
        m_shopDealRows.erase(it);
        if (m_shopDealRows.empty()) {
            m_shopSelectedDealRow = -1;
        } else if (m_shopSelectedDealRow >= static_cast<int>(m_shopDealRows.size())) {
            m_shopSelectedDealRow = static_cast<int>(m_shopDealRows.size()) - 1;
        } else if (m_shopSelectedDealRow == static_cast<int>(erasedIndex)) {
            m_shopSelectedDealRow = (std::min)(m_shopSelectedDealRow, static_cast<int>(m_shopDealRows.size()) - 1);
        }
    } else {
        it->quantity = newQuantity;
        it->itemInfo.m_num = newQuantity;
        m_shopSelectedDealRow = static_cast<int>(std::distance(m_shopDealRows.begin(), it));
    }

    long long total = 0;
    for (const NPC_SHOP_DEAL_ROW& dealRow : m_shopDealRows) {
        total += static_cast<long long>(dealRow.unitPrice) * static_cast<long long>(dealRow.quantity);
    }
    m_shopDealTotal = static_cast<int>((std::min)(total, static_cast<long long>(std::numeric_limits<int>::max())));
    m_shopSelectedSourceRow = static_cast<int>(sourceRowIndex);
    return true;
}

bool CSession::AdjustNpcShopDealByDealRow(size_t dealRowIndex, int deltaQuantity)
{
    if (dealRowIndex >= m_shopDealRows.size()) {
        return false;
    }

    const NPC_SHOP_DEAL_ROW& dealRow = m_shopDealRows[dealRowIndex];
    for (size_t sourceRowIndex = 0; sourceRowIndex < m_shopRows.size(); ++sourceRowIndex) {
        const NPC_SHOP_ROW& row = m_shopRows[sourceRowIndex];
        if (row.sourceItemIndex == dealRow.sourceItemIndex
            && row.itemInfo.GetItemId() == dealRow.itemInfo.GetItemId()) {
            return AdjustNpcShopDealBySourceRow(sourceRowIndex, deltaQuantity);
        }
    }

    return false;
}

int CSession::GetPlayerSkillPointCount() const
{
    return m_hasSelectedCharacterInfo ? static_cast<int>(m_selectedCharacterInfo.jobpoint) : 0;
}

const char* CSession::GetPlayerName() const
{
    return m_playerName;
}

const char* CSession::GetJobDisplayName(int job) const
{
    if (job == JT_G_MASTER) {
        return "JT_G_MASTER";
    }

    const std::lock_guard<std::mutex> lock(m_jobDisplayNameMutex);

    const auto cached = m_jobDisplayNameCache.find(job);
    if (cached != m_jobDisplayNameCache.end()) {
        return cached->second.c_str();
    }

    std::string displayName;
    if (TryGetLuaJobDisplayName(job, GetSex(), &displayName) && !displayName.empty()) {
        auto inserted = m_jobDisplayNameCache.emplace(job, std::move(displayName));
        DbgLog("[Session] Cached display job name job=%d name='%s'\n",
            job,
            inserted.first->second.c_str());
        return inserted.first->second.c_str();
    }

    DbgLog("[Session] Falling back to generated display job name job=%d name='%s'\n",
        job,
        LookupGeneratedJobName(job) ? LookupGeneratedJobName(job) : "");
    return LookupGeneratedJobName(job);
}

const char* CSession::GetJobName(int job) const
{
    if (job == JT_G_MASTER) {
        return "JT_G_MASTER";
    }
    return LookupGeneratedJobName(job);
}

const char* CSession::GetAttrWaveName(int attr) const
{
    switch (attr) {
    case 3:
        return kEnemyHitFireWaves[std::rand() % (sizeof(kEnemyHitFireWaves) / sizeof(kEnemyHitFireWaves[0]))];
    case 4:
        return kEnemyHitWindWaves[std::rand() % (sizeof(kEnemyHitWindWaves) / sizeof(kEnemyHitWindWaves[0]))];
    default:
        return kEnemyHitNormalWaves[std::rand() % (sizeof(kEnemyHitNormalWaves) / sizeof(kEnemyHitNormalWaves[0]))];
    }
}

const char* CSession::GetJobHitWaveName(int job) const
{
    if (job < 0 || (job >= 28 && job <= 4000)) {
        return GetWeaponHitWaveName(-1);
    }

    const int normalizedJob = NormalizeJob(job);
    if (normalizedJob < 0 || normalizedJob >= static_cast<int>(m_jobHitWaveNameTable.size())) {
        return "";
    }

    return m_jobHitWaveNameTable[normalizedJob].c_str();
}

const char* CSession::GetWeaponHitWaveName(int weapon) const
{
    if (weapon == -1) {
        return kEnemyHitNormalWaves[std::rand() % (sizeof(kEnemyHitNormalWaves) / sizeof(kEnemyHitNormalWaves[0]))];
    }

    if (weapon < 0 || weapon >= static_cast<int>(m_weaponHitWaveNameTable.size())) {
        return "";
    }

    return m_weaponHitWaveNameTable[weapon].c_str();
}

void CSession::InitJobHitWaveName()
{
    m_jobHitWaveNameTable.assign(21068, kPlayerClothesWave);
    for (const JobWaveOverride& entry : kJobHitWaveOverrides) {
        if (entry.job >= 0 && entry.job < static_cast<int>(m_jobHitWaveNameTable.size())) {
            m_jobHitWaveNameTable[entry.job] = entry.waveName;
        }
    }
}

void CSession::InitWeaponHitWaveName()
{
    m_weaponHitWaveNameTable.assign(31, kHitMaceWave);
    for (const WeaponWaveOverride& entry : kWeaponHitWaveOverrides) {
        if (entry.weaponType >= 0 && entry.weaponType < static_cast<int>(m_weaponHitWaveNameTable.size())) {
            m_weaponHitWaveNameTable[entry.weaponType] = entry.waveName;
        }
    }
}

int CSession::NormalizeJob(int job) const
{
    if (job == JT_G_MASTER) {
        return job;
    }
    return (job > 3950) ? (job - 3950) : job;
}

int CSession::GetSex() const
{
    return m_sex ? 1 : 0;
}

char* CSession::GetJobActName(int job, int sex, char* buf)
{
    const char* sexToken = GetSexToken(sex);
    const std::string jobToken = ResolveExistingPlayerBodyJobToken(job, sex);
    std::sprintf(buf, "%s%s\\%s\\%s_%s.act", kHumanSpriteRoot, kBodyDir, sexToken, jobToken.c_str(), sexToken);
    return buf;
}

char* CSession::GetJobSprName(int job, int sex, char* buf)
{
    const char* sexToken = GetSexToken(sex);
    const std::string jobToken = ResolveExistingPlayerBodyJobToken(job, sex);
    std::sprintf(buf, "%s%s\\%s\\%s_%s.spr", kHumanSpriteRoot, kBodyDir, sexToken, jobToken.c_str(), sexToken);
    return buf;
}

char* CSession::GetPlayerBodyActName(int job, int sex, int weaponItemId, char* buf)
{
    return BuildPlayerBodyResourceName(*this, job, sex, weaponItemId, "act", buf);
}

char* CSession::GetPlayerBodySprName(int job, int sex, int weaponItemId, char* buf)
{
    return BuildPlayerBodyResourceName(*this, job, sex, weaponItemId, "spr", buf);
}

char* CSession::GetHeadActName(int job, int* head, int sex, char* buf)
{
    const int normalizedJob = NormalizeJob(job);
    const int resolvedHead = NormalizeHeadValue(head ? *head : 0, normalizedJob);
    if (head) {
        *head = resolvedHead;
    }
    const char* sexToken = GetSexToken(sex);
    std::sprintf(buf, "%s%s\\%s\\%d_%s.act", kHumanSpriteRoot, kHeadDir, sexToken, resolvedHead, sexToken);
    return buf;
}

char* CSession::GetHeadSprName(int job, int* head, int sex, char* buf)
{
    const int normalizedJob = NormalizeJob(job);
    const int resolvedHead = NormalizeHeadValue(head ? *head : 0, normalizedJob);
    if (head) {
        *head = resolvedHead;
    }
    const char* sexToken = GetSexToken(sex);
    std::sprintf(buf, "%s%s\\%s\\%d_%s.spr", kHumanSpriteRoot, kHeadDir, sexToken, resolvedHead, sexToken);
    return buf;
}

void CSession::EnsureAccessoryNameTableLoaded()
{
    if (m_accessoryNameTableLoaded) {
        return;
    }

    m_accessoryNameTableLoaded = true;
    m_accessoryNameTable.clear();
    m_accessoryNameTable.emplace_back();

    const std::filesystem::path directory = MakeReferenceSpriteRoot()
        / std::filesystem::path(L"\x00BE\x00C7\x00BC\x00BC\x00BB\x00E7\x00B8\x00AE")
        / std::filesystem::path(L"\x00B3\x00B2");
    if (!std::filesystem::exists(directory) || !std::filesystem::is_directory(directory)) {
        DbgLog("[Session] accessory directory missing\n");
        return;
    }

    std::vector<std::string> names;
    const std::string prefix = std::string(kMaleSex) + "_";
    std::error_code ec;
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(directory, ec)) {
        if (ec || !entry.is_regular_file()) {
            continue;
        }

        std::string stem = ExtractLowByteString(entry.path().stem().wstring());
        if (stem.size() <= prefix.size() || stem.compare(0, prefix.size(), prefix) != 0) {
            continue;
        }

        stem.erase(0, prefix.size());
        if (!stem.empty()) {
            names.push_back(std::move(stem));
        }
    }

    std::sort(names.begin(), names.end());
    names.erase(std::unique(names.begin(), names.end()), names.end());
    m_accessoryNameTable.insert(m_accessoryNameTable.end(), names.begin(), names.end());
    DbgLog("[Session] accessory name table loaded count=%u\n",
        static_cast<unsigned int>(m_accessoryNameTable.size()));
}

char* CSession::GetAccessoryActName(int job, int* head, int sex, int accessory, char* buf)
{
    if (!buf) {
        return buf;
    }
    if (accessory == 185) {
        return GetHeadActName(job, head, sex, buf);
    }
    if (accessory <= 0) {
        *buf = 0;
        return buf;
    }

    const std::string resourceName = g_ttemmgr.GetVisibleHeadgearResourceNameByViewId(accessory);
    if (resourceName.empty()) {
        DbgLog("[Session] accessory act lookup failed accessory=%d tableSize=%u job=%d sex=%d head=%d\n",
            accessory,
            static_cast<unsigned int>(m_accessoryNameTable.size()),
            job,
            sex,
            head ? *head : 0);
        *buf = 0;
        return buf;
    }

    const char* sexToken = GetSexToken(sex);
    const char* separator = resourceName.front() == '_' ? "" : "_";
    std::sprintf(buf, "%s%s\\%s%s%s.act", kAccessorySpriteRoot, sexToken, sexToken, separator, resourceName.c_str());
    return buf;
}

char* CSession::GetAccessorySprName(int job, int* head, int sex, int accessory, char* buf)
{
    if (!buf) {
        return buf;
    }
    if (accessory == 185) {
        return GetHeadSprName(job, head, sex, buf);
    }
    if (accessory <= 0) {
        *buf = 0;
        return buf;
    }

    const std::string resourceName = g_ttemmgr.GetVisibleHeadgearResourceNameByViewId(accessory);
    if (resourceName.empty()) {
        DbgLog("[Session] accessory spr lookup failed accessory=%d tableSize=%u job=%d sex=%d head=%d\n",
            accessory,
            static_cast<unsigned int>(m_accessoryNameTable.size()),
            job,
            sex,
            head ? *head : 0);
        *buf = 0;
        return buf;
    }

    const char* sexToken = GetSexToken(sex);
    const char* separator = resourceName.front() == '_' ? "" : "_";
    std::sprintf(buf, "%s%s\\%s%s%s.spr", kAccessorySpriteRoot, sexToken, sexToken, separator, resourceName.c_str());
    return buf;
}

char* CSession::GetImfName(int job, int head, int sex, char* buf)
{
    (void)head;
    const char* sexToken = GetSexToken(sex);

    const std::string resolvedBodyToken = ResolveExistingPlayerBodyJobToken(job, sex);
    if (!resolvedBodyToken.empty()) {
        std::sprintf(buf, "%s%s_%s.imf", kImfRoot, resolvedBodyToken.c_str(), sexToken);
        if (ResourceExistsLocalFirst(buf)) {
            return buf;
        }
    }

    const std::string fallbackImfToken = ResolvePlayerImfJobToken(job);
    std::sprintf(buf, "%s%s_%s.imf", kImfRoot, fallbackImfToken.c_str(), sexToken);
    if (ResourceExistsLocalFirst(buf)) {
        return buf;
    }

    const std::vector<std::string> imfTokens = BuildPlayerImfJobTokenCandidates(job);
    for (const std::string& imfToken : imfTokens) {
        if (imfToken == resolvedBodyToken || imfToken == fallbackImfToken) {
            continue;
        }
        std::sprintf(buf, "%s%s_%s.imf", kImfRoot, imfToken.c_str(), sexToken);
        if (ResourceExistsLocalFirst(buf)) {
            return buf;
        }
    }

    std::sprintf(buf, "%s%s_%s.imf", kImfRoot, fallbackImfToken.c_str(), sexToken);
    return buf;
}

char* CSession::GetBodyPaletteName(int job, int sex, int palNum, char* buf)
{
    const char* sexToken = GetSexToken(sex);
    const std::string jobToken = ResolvePlayerPaletteJobToken(job);
    std::sprintf(buf, "%s%s_%s_%d.pal", kBodyPaletteRoot, jobToken.c_str(), sexToken, palNum);
    return buf;
}

char* CSession::GetHeadPaletteName(int head, int job, int sex, int palNum, char* buf)
{
    const int normalizedJob = NormalizeJob(job);
    const int resolvedHead = NormalizeHeadValue(head, normalizedJob);
    const char* sexToken = GetSexToken(sex);
    std::sprintf(buf, "%s\xB8\xD3\xB8\xAE%d_%s_%d.pal", kHeadPaletteRoot, resolvedHead, sexToken, palNum);
    return buf;
}

// Ref CSession::GetWeaponType — Ref/Session2.cpp
int CSession::GetWeaponTypeByItemId(int itemId) const
{
    int result = itemId;
    if (itemId == 0) {
        return result;
    }
    if (itemId >= 1100 && itemId < 1150) {
        return 2;
    }
    if (itemId >= 13400 && itemId < 13500) {
        return 2;
    }
    if (itemId >= 1150) {
        if (itemId < 1200) {
            return 3;
        }
        if (itemId < 1250) {
            return 1;
        }
        if (itemId < 1300) {
            return 16;
        }
        if (itemId < 1350) {
            return 6;
        }
        if (itemId < 1400) {
            return 7;
        }
        if (itemId < 1450) {
            return 4;
        }
        if (itemId < 1500) {
            if (itemId != 1472 && itemId != 1473) {
                return 5;
            }
            return 10;
        }
        if (itemId < 1550) {
            return 8;
        }
        if (itemId < 1600) {
            return 15;
        }
        if (itemId < 1700) {
            return 10;
        }
        if (itemId < 1750) {
            return 11;
        }
    }
    if (itemId >= 1800) {
        if (itemId < 1900) {
            return 12;
        }
        if (itemId < 1950) {
            return 13;
        }
        if (itemId < 2000) {
            return 14;
        }
        if (itemId < 2100) {
            return 23;
        }
    }
    if (itemId >= 13150 && itemId < 13200) {
        return 18;
    }
    if (itemId < 13000) {
        if (itemId < 13300 || itemId >= 13400) {
            return -1;
        }
        return 22;
    }
    if (itemId < 13100) {
        return 1;
    }
    if (itemId >= 13150) {
        if (itemId < 13300 || itemId >= 13400) {
            return -1;
        }
        return 22;
    }
    return 17;
}

int CSession::MakeWeaponTypeByItemId(int primaryWeaponItemId, int secondaryWeaponItemId) const
{
    int primary = primaryWeaponItemId;
    int secondary = secondaryWeaponItemId;
    int result = 0;

    if (primary == 0) {
        primary = secondary;
        secondary = 0;
        if (primary == 0) {
            return result;
        }
    }

    if ((primary < 1100 || primary >= 1150) && (primary < 13400 || primary >= 13500)) {
        if (primary < 1200 || primary >= 1250) {
            if (primary < 1300 || primary >= 1350) {
                if (primary >= 13000 && primary < 13100) {
                    result = 1;
                    if (secondary < 1200 || secondary >= 1250) {
                        if (secondary < 1100 || secondary >= 1150) {
                            if (secondary < 13000 || secondary >= 13100) {
                                if (secondary < 13400 || secondary >= 13500) {
                                    if (secondary >= 1300 && secondary < 1350) {
                                        return 29;
                                    }
                                } else {
                                    return 28;
                                }
                            } else {
                                return 25;
                            }
                        } else {
                            return 28;
                        }
                    } else {
                        return 25;
                    }
                }
            } else {
                result = 6;
                if (secondary >= 1300 && secondary < 1350) {
                    return 27;
                }
            }
        } else {
            result = 1;
            if (secondary < 1200 || secondary >= 1250) {
                if (secondary < 13000 || secondary >= 13100) {
                    if (secondary < 1100 || secondary >= 1150) {
                        if (secondary < 13400 || secondary >= 13500) {
                            if (secondary >= 1300 && secondary < 1350) {
                                return 29;
                            }
                        } else {
                            return 28;
                        }
                    } else {
                        return 28;
                    }
                } else {
                    return 25;
                }
            } else {
                return 25;
            }
        }
    } else {
        result = 2;
        if (secondary < 1100 || secondary >= 1150) {
            if (secondary < 13400 || secondary >= 13500) {
                if (secondary >= 1300 && secondary < 1350) {
                    return 30;
                }
            } else {
                return 26;
            }
        } else {
            return 26;
        }
    }

    return result;
}

int CSession::ResolvePackedWeaponType(int job, int weaponValue) const
{
    if (weaponValue <= 0) {
        return 0;
    }

    const int primaryValue = weaponValue & 0xFFFF;
    const int secondaryValue = static_cast<int>((static_cast<unsigned int>(weaponValue) >> 16) & 0xFFFFu);

    if (!IsDualWeaponJob(job)) {
        return ResolveWeaponTypeOrViewType(*this, primaryValue);
    }

    const int primaryType = ResolveWeaponTypeOrViewType(*this, primaryValue);
    const int secondaryType = ResolveWeaponTypeOrViewType(*this, secondaryValue);

    if (secondaryType <= 0) {
        return primaryType;
    }

    if (primaryValue > 31 || secondaryValue > 31) {
        const int combinedType = MakeWeaponTypeByItemId(primaryValue, secondaryValue);
        if (combinedType > 0) {
            return combinedType;
        }
    }

    return MakeWeaponTypeFromResolvedTypes(primaryType, secondaryType);
}

int CSession::GetCurrentPlayerWeaponValue() const
{
    if (IsDualWeaponJob(m_playerJob) && (m_playerWeapon != 0 || m_playerShield != 0)) {
        return (m_playerWeapon & 0xFFFF) | ((m_playerShield & 0xFFFF) << 16);
    }

    if (m_playerWeapon != 0) {
        return m_playerWeapon;
    }

    const unsigned int primaryWeaponItemId = GetEquippedRightHandWeaponItemId();
    const unsigned int secondaryWeaponItemId = GetEquippedLeftHandWeaponItemId();
    if (primaryWeaponItemId != 0 || secondaryWeaponItemId != 0) {
        if (IsDualWeaponJob(m_playerJob) && secondaryWeaponItemId != 0) {
            return static_cast<int>(primaryWeaponItemId & 0xFFFFu)
                | (static_cast<int>(secondaryWeaponItemId & 0xFFFFu) << 16);
        }
        return primaryWeaponItemId != 0
            ? static_cast<int>(primaryWeaponItemId)
            : static_cast<int>(secondaryWeaponItemId);
    }

    return secondaryWeaponItemId != 0 ? static_cast<int>(secondaryWeaponItemId) : 0;
}

std::string CSession::GetPlayerWeaponToken(int weaponType) const
{
    if (weaponType <= 0) {
        return std::string();
    }

    if (!g_buabridge.LoadRagnarokScriptOnce("lua files\\datainfo\\weapontable.lub")) {
        return std::string();
    }

    std::string token;
    if (!g_buabridge.GetGlobalTableStringByIntegerKey("WeaponNameTable", weaponType, &token)) {
        return std::string();
    }

    return token;
}

unsigned int CSession::GetEquippedLeftHandWeaponItemId() const
{
    for (const ITEM_INFO& item : m_inventoryItems) {
        if (item.m_wearLocation == 0) {
            continue;
        }
        if ((item.m_wearLocation & 32) != 0 && (item.m_wearLocation & 2) == 0) {
            const unsigned int itemId = item.GetItemId();
            if (GetWeaponTypeByItemId(static_cast<int>(itemId)) > 0) {
                return itemId;
            }
        }
    }
    return 0;
}

unsigned int CSession::GetEquippedRightHandWeaponItemId() const
{
    for (const ITEM_INFO& item : m_inventoryItems) {
        if (item.m_wearLocation == 0) {
            continue;
        }
        if ((item.m_wearLocation & 2) != 0) {
            return item.GetItemId();
        }
    }
    return 0;
}

bool CSession::IsSecondAttack(int job, int sex, int weaponItemId) const
{
    const int weaponType = ResolvePackedWeaponType(job, weaponItemId);

    if (job > 4001) {
        switch (job) {
        case 4002:
        case 4008:
        case 4014:
        case 4015:
        case 4022:
        case 4024:
        case 4030:
        case 4036:
        case 4037:
        case 4044:
            return weaponType >= 4 && weaponType <= 5;
        case 4003:
        case 4006:
        case 4025:
        case 4028:
            return weaponType == 1;
        case 4004:
        case 4026:
            return weaponType != 11;
        case 4007:
        case 4012:
        case 4018:
        case 4029:
        case 4034:
        case 4040:
            return weaponType == 11;
        case 4009:
        case 4031:
            return weaponType == 15;
        case 4010:
        case 4032: {
            const int value = weaponType - 1;
            if (value == 0) {
                return sex == 1;
            }
            const int nextValue = value - 9;
            if (nextValue == 0 || nextValue == 13) {
                return sex == 0;
            }
            return false;
        }
        case 4011:
        case 4019:
        case 4033:
        case 4041:
            if (weaponType == 2) {
                return true;
            }
            return weaponType > 5 && weaponType <= 8;
        case 4013:
        case 4035:
            if (weaponType == 16) {
                return true;
            }
            return weaponType > 24 && weaponType <= 30;
        case 4016:
        case 4038:
            return weaponType == 0 || weaponType == 12;
        case 4017:
        case 4039:
            switch (weaponType) {
            case 5:
            case 10:
            case 15:
            case 23:
                return true;
            default:
                return false;
            }
        case 4020:
        case 4021:
        case 4042:
        case 4043:
            return weaponType == 11;
        case 4023:
        case 4045:
            if (sex == 1) {
                switch (weaponType) {
                case 2:
                case 3:
                case 6:
                case 7:
                case 8:
                case 9:
                case 10:
                case 23:
                    return true;
                default:
                    return false;
                }
            }
            return sex == 0 && weaponType == 1;
        case 4049: {
            const int value = weaponType - 1;
            if (value == 0) {
                return sex == 1;
            }
            const int nextValue = value - 9;
            if (nextValue == 0 || nextValue == 13) {
                return sex == 0;
            }
            return false;
        }
        default:
            return false;
        }
    }

    if (job == 4001) {
        if (sex == 1) {
            switch (weaponType) {
            case 2:
            case 3:
            case 6:
            case 7:
            case 8:
            case 9:
            case 10:
            case 23:
                return true;
            default:
                return false;
            }
        }
        return sex == 0 && weaponType == 1;
    }

    switch (job) {
    case 0:
    case 23:
        if (sex == 1) {
            switch (weaponType) {
            case 2:
            case 3:
            case 6:
            case 7:
            case 8:
            case 9:
            case 10:
            case 23:
                return true;
            default:
                return false;
            }
        }
        return sex == 0 && weaponType == 1;
    case 1:
    case 7:
    case 13:
    case 14:
    case 21:
        return weaponType >= 4 && weaponType <= 5;
    case 2:
    case 5:
        return weaponType == 1;
    case 3:
        return weaponType != 11;
    case 6:
    case 11:
    case 17:
        return weaponType == 11;
    case 8:
        return weaponType == 15;
    case 9: {
        const int value = weaponType - 1;
        if (value == 0) {
            return sex == 1;
        }
        const int nextValue = value - 9;
        if (nextValue == 0 || nextValue == 13) {
            return sex == 0;
        }
        return false;
    }
    case 10:
    case 18:
        if (weaponType == 2) {
            return true;
        }
        return weaponType > 5 && weaponType <= 8;
    case 12:
        if (weaponType == 16) {
            return true;
        }
        return weaponType > 24 && weaponType <= 30;
    case 15:
        return weaponType == 0 || weaponType == 12;
    case 16:
        switch (weaponType) {
        case 5:
        case 10:
        case 15:
        case 23:
            return true;
        default:
            return false;
        }
    case 19:
    case 20:
        return weaponType == 11;
    case 24:
        return weaponType >= 18 && weaponType <= 21;
    case 25:
        return weaponType == 22;
    default:
        return false;
    }
}

float CSession::GetPCAttackMotion(int job, int sex, int weaponItemId, int isSecondAttack) const
{
    const int weaponType = ResolvePackedWeaponType(job, weaponItemId);

    if (isSecondAttack) {
        if (isSecondAttack == 1) {
            if (job <= 4013) {
                if (job != 4013) {
                    if (job == 0 || job == 23) {
                        return sex == 1 ? 5.8499999f : 6.0f;
                    }
                    if (job != 12) {
                        return 6.0f;
                    }
                }

                if (weaponType == 16 || (weaponType > 24 && weaponType <= 30)) {
                    return 3.0f;
                }
                return 6.0f;
            }

            if (job == 4035) {
                if (weaponType == 16 || (weaponType > 24 && weaponType <= 30)) {
                    return 3.0f;
                }
                return 6.0f;
            }

            if (job == 4045) {
                return sex == 1 ? 5.8499999f : 6.0f;
            }
        }

        return 6.0f;
    }

    if (job == 5) {
        return 5.8499999f;
    }
    if (job == 6) {
        return 5.75f;
    }
    return 6.0f;
}
