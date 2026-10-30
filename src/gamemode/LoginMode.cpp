#include <winsock2.h>
#include "CursorRenderer.h"
#include "LoginMode.h"
#include "ui/UIWindowMgr.h"
#include "ui/UILoginWnd.h"
#include "ui/UIMakeCharWnd.h"
#include "ui/UISelectCharWnd.h"
#include "ui/UIWaitWnd.h"
#include "ui/UISelectServerWnd.h"
#include "render/Renderer.h"
#include "render3d/Device.h"
#include "render3d/RenderDevice.h"
#include "core/ClientInfoLocale.h"
#include "core/File.h"
#include "core/Globals.h"
#include "core/MainLoopWatchdog.h"
#include "core/SettingsIni.h"
#include "cipher/Md5.h"
#include "main/WinMain.h"
#include "network/Connection.h"
#include "network/MapSendProfile.h"
#include "network/Packet.h"
#include "audio/Audio.h"
#include "session/Session.h"
#include "res/Sprite.h"
#include "res/ActRes.h"
#include "render/DC.h"
#include "qtui/QtUiRuntime.h"
#include "DebugLog.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <vector>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <string_view>

namespace {

constexpr u32 kPasswordHashFallbackDelayMs = 1500;
constexpr size_t kCompactCharListHeaderSize = 4;
constexpr size_t kLegacyCharListHeaderSize = 24;
constexpr size_t kExpandedCharListHeaderSize = 27;

void FormatPacketBytes(const u8* data, int len, char* out, size_t outSize)
{
    if (!out || outSize == 0) {
        return;
    }

    out[0] = 0;
    if (!data || len <= 0) {
        return;
    }

    int cursor = 0;
    for (int i = 0; i < len; ++i) {
        const size_t remaining = (cursor >= 0) ? (outSize - static_cast<size_t>(cursor)) : 0;
        if (remaining <= 1) {
            break;
        }

        cursor += std::snprintf(out + cursor, remaining, "%02X%s",
            static_cast<unsigned int>(data[i]), (i + 1 < len) ? " " : "");
        if (cursor < 0) {
            out[0] = 0;
            return;
        }
    }
}

static_assert(sizeof(CHARACTER_INFO) == 108, "CHARACTER_INFO size mismatch for classic char-list parsing");

#pragma pack(push, 1)
struct LEGACY_CHARACTER_INFO_106
{
    u32 GID;
    int exp;
    int money;
    int jobexp;
    int joblevel;
    int bodystate;
    int healthstate;
    int effectstate;
    int virtue;
    int honor;
    s16 jobpoint;
    s16 hp;
    s16 maxhp;
    s16 sp;
    s16 maxsp;
    s16 speed;
    s16 job;
    s16 head;
    s16 weapon;
    s16 level;
    s16 sppoint;
    s16 accessory;
    s16 shield;
    s16 accessory2;
    s16 accessory3;
    s16 headpalette;
    s16 bodypalette;
    u8  name[24];
    u8  Str;
    u8  Agi;
    u8  Vit;
    u8  Int;
    u8  Dex;
    u8  Luk;
    u16 CharNum;
};
#pragma pack(pop)

static_assert(sizeof(LEGACY_CHARACTER_INFO_106) == 106, "Legacy 0x006B character record size mismatch");

enum class CharListLayout
{
    CompactLegacy,
    Legacy,
    Expanded,
};

enum class CharListLayoutOverride
{
    Auto,
    CompactLegacy,
    Legacy,
    Expanded,
};

constexpr u8 kStockMainHash[16] = {
    0x65, 0xE3, 0x64, 0x31, 0x03, 0x1C, 0x03, 0x11,
    0x1E, 0x50, 0x6D, 0x13, 0x39, 0x69, 0x2D, 0x7A,
};

constexpr u8 kStockSakrayHash[16] = {
    0x82, 0xD1, 0x2C, 0x91, 0x4F, 0x5A, 0xD4, 0x8F,
    0xD9, 0x6F, 0xCF, 0x7E, 0xF4, 0xCC, 0x49, 0x2D,
};

constexpr u8 kIndonesiaSakrayHash[16] = {
    0xC7, 0x0A, 0x94, 0xC2, 0x7A, 0xCC, 0x38, 0x9A,
    0x47, 0xF5, 0x54, 0x39, 0x7C, 0xA4, 0xD0, 0x39,
};

bool CanReturnToCharacterSelect()
{
    return g_session.m_charServerAddr[0] != '\0' && g_session.m_charServerPort > 0;
}

void ClearSelectedCharacterIfMatches(u32 deletedGid)
{
    if (deletedGid == 0) {
        return;
    }

    CHARACTER_INFO* selected = g_session.GetMutableSelectedCharacterInfo();
    if (selected && selected->GID == deletedGid) {
        std::memset(selected, 0, sizeof(*selected));
    }
}

std::string ChooseLoginWallpaperName()
{
    static const char* kUiKorPrefix =
        "texture\\"
        "\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA"
        "\\";

    const char* baseNames[] = {
        "ad_title.jpg",
        "rag_title.jpg",
        "title.bmp",
        "title.jpg",
        "login_background.jpg",
        "login_background.bmp",
        nullptr
    };

    const char* prefixes[] = {
        "",
        "texture\\",
        "texture\\interface\\",
        "texture\\interface\\basic_interface\\",
        "texture\\login_interface\\",
        kUiKorPrefix,
        "texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\login_interface\\",
        "ui\\",
        nullptr
    };

    for (int b = 0; baseNames[b]; ++b) {
        for (int p = 0; prefixes[p]; ++p) {
            std::string candidate = std::string(prefixes[p]) + baseNames[b];
            if (g_fileMgr.IsDataExist(candidate.c_str())) {
                return baseNames[b];
            }
        }
    }

    return "title.bmp";
}

void CopyCString(char* dst, size_t dstSize, const char* src)
{
    if (!dst || dstSize == 0) {
        return;
    }

    if (!src) {
        dst[0] = '\0';
        return;
    }

    std::strncpy(dst, src, dstSize - 1);
    dst[dstSize - 1] = '\0';
}

CHARACTER_INFO ExpandLegacyCharacterInfo(const LEGACY_CHARACTER_INFO_106& legacy)
{
    CHARACTER_INFO info{};
    std::memcpy(&info, &legacy, sizeof(legacy));
    info.CharNum = static_cast<u8>(legacy.CharNum & 0xFFu);
    info.haircolor = static_cast<u8>(std::clamp<int>(legacy.headpalette, 0, 0xFF));
    info.bIsChangedCharName = 1;
    return info;
}

std::string NormalizeAsciiLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

CharListLayoutOverride LoadCharListLayoutOverride()
{
    std::string value = NormalizeAsciiLower(LoadSettingsIniString("Packets", "CharListReceiveLayout", "auto"));
    if (value == "legacy_4_106" || value == "compact_legacy" || value == "compact106") {
        return CharListLayoutOverride::CompactLegacy;
    }
    if (value == "legacy_24_108" || value == "legacy108") {
        return CharListLayoutOverride::Legacy;
    }
    if (value == "expanded_27_108" || value == "expanded108" || value == "2010") {
        return CharListLayoutOverride::Expanded;
    }
    return CharListLayoutOverride::Auto;
}

bool TryResolveSpecificCharListLayout(size_t packetSize,
                                      CharListLayout desiredLayout,
                                      size_t* headerSize,
                                      size_t* charInfoSize)
{
    if (!headerSize || !charInfoSize) {
        return false;
    }

    switch (desiredLayout) {
    case CharListLayout::Expanded:
        if (packetSize >= kExpandedCharListHeaderSize
            && ((packetSize - kExpandedCharListHeaderSize) % sizeof(CHARACTER_INFO)) == 0) {
            *headerSize = kExpandedCharListHeaderSize;
            *charInfoSize = sizeof(CHARACTER_INFO);
            return true;
        }
        break;
    case CharListLayout::Legacy:
        if (packetSize >= kLegacyCharListHeaderSize
            && ((packetSize - kLegacyCharListHeaderSize) % sizeof(CHARACTER_INFO)) == 0) {
            *headerSize = kLegacyCharListHeaderSize;
            *charInfoSize = sizeof(CHARACTER_INFO);
            return true;
        }
        break;
    case CharListLayout::CompactLegacy:
        if (packetSize >= kCompactCharListHeaderSize
            && ((packetSize - kCompactCharListHeaderSize) % sizeof(LEGACY_CHARACTER_INFO_106)) == 0) {
            *headerSize = kCompactCharListHeaderSize;
            *charInfoSize = sizeof(LEGACY_CHARACTER_INFO_106);
            return true;
        }
        break;
    }

    return false;
}

bool TryResolveCharListLayout(u16 packetLength,
                              CharListLayout* layout,
                              size_t* headerSize,
                              size_t* charInfoSize)
{
    if (!layout || !headerSize || !charInfoSize) {
        return false;
    }

    const size_t packetSize = static_cast<size_t>(packetLength);
    switch (LoadCharListLayoutOverride()) {
    case CharListLayoutOverride::CompactLegacy:
        *layout = CharListLayout::CompactLegacy;
        return TryResolveSpecificCharListLayout(packetSize, *layout, headerSize, charInfoSize);
    case CharListLayoutOverride::Legacy:
        *layout = CharListLayout::Legacy;
        return TryResolveSpecificCharListLayout(packetSize, *layout, headerSize, charInfoSize);
    case CharListLayoutOverride::Expanded:
        *layout = CharListLayout::Expanded;
        return TryResolveSpecificCharListLayout(packetSize, *layout, headerSize, charInfoSize);
    case CharListLayoutOverride::Auto:
        break;
    }

    if (TryResolveSpecificCharListLayout(packetSize, CharListLayout::Expanded, headerSize, charInfoSize)) {
        *layout = CharListLayout::Expanded;
        return true;
    }

    if (TryResolveSpecificCharListLayout(packetSize, CharListLayout::Legacy, headerSize, charInfoSize)) {
        *layout = CharListLayout::Legacy;
        return true;
    }

    if (TryResolveSpecificCharListLayout(packetSize, CharListLayout::CompactLegacy, headerSize, charInfoSize)) {
        *layout = CharListLayout::CompactLegacy;
        return true;
    }

    return false;
}

std::string ExtractPacketText(const std::vector<u8>& raw, size_t offset)
{
    if (raw.size() <= offset) {
        return {};
    }

    size_t end = raw.size();
    while (end > offset && raw[end - 1] == '\0') {
        --end;
    }

    return std::string(reinterpret_cast<const char*>(raw.data() + offset), end - offset);
}

std::string ExtractFixedText(const char* bytes, size_t size)
{
    if (!bytes || size == 0) {
        return {};
    }

    size_t end = size;
    while (end > 0 && bytes[end - 1] == '\0') {
        --end;
    }

    return std::string(bytes, end);
}

void ShowLoginErrorDialog(const char* message)
{
    if (!message || !*message) {
        return;
    }

    ErrorMsg(message);
}

// Ref/LoginMode.cpp:996, 1139, 1629: every failed Connect() shows
// MsgStr(MSI_SERVER_CONNECTION_FAILED) before returning to the login screen.
// The rebuilt client adds the address and the socket reason so a wrong
// char_ip/map_ip in the server config is visible without reading the log.
void ShowConnectionFailedDialog(const char* serverKind)
{
    const char* reason = CRagConnection::instance()->GetLastConnectError();
    char message[512] = {};
    if (reason && *reason) {
        std::snprintf(message, sizeof(message),
            "Failed to connect to the %s.\n\n%s\n\n"
            "The address comes from the server you are logging in to "
            "(rAthena: char_ip in conf/char_athena.conf, map_ip in conf/map_athena.conf).",
            serverKind, reason);
    } else {
        std::snprintf(message, sizeof(message), "Failed to connect to the %s.", serverKind);
    }
    ShowLoginErrorDialog(message);
}

u8 GetAccountType()
{
    if (g_serverType == ServerSakray) {
        switch (g_serviceType) {
        case ServiceAmerica: return 14;
        case ServiceJapan: return 9;
        case ServiceChina: return 4;
        case ServiceTaiwan: return 5;
        case ServiceThai: return 10;
        case ServiceIndonesia: return 13;
        case ServicePhilippine: return 18;
        case ServiceMalaysia: return 16;
        case ServiceSingapore: return 17;
        case ServiceGermany: return 20;
        case ServiceIndia: return 21;
        case ServiceBrazil: return 22;
        case ServiceAustralia: return 23;
        case ServiceVietnam: return 33;
        default: return 2;
        }
    }

    if (g_serverType == ServerPK) {
        switch (g_serviceType) {
        case ServiceJapan: return 28;
        case ServiceThai: return 29;
        case ServiceIndonesia: return 34;
        case ServicePhilippine: return 27;
        case ServiceVietnam: return 32;
        default: return 0;
        }
    }

    switch (g_serviceType) {
    case ServiceAmerica: return 1;
    case ServiceJapan: return 3;
    case ServiceChina: return 4;
    case ServiceTaiwan: return 5;
    case ServiceThai: return 7;
    case ServiceIndonesia: return 12;
    case ServicePhilippine: return 15;
    case ServiceMalaysia: return 16;
    case ServiceSingapore: return 17;
    case ServiceGermany: return 20;
    case ServiceIndia: return 21;
    case ServiceBrazil: return 22;
    case ServiceAustralia: return 23;
    case ServiceRussia: return 25;
    case ServiceVietnam: return 26;
    case ServiceChile: return 30;
    case ServiceFrance: return 31;
    default: return 0;
    }
}

u32 ResolveAccountLoginVersion()
{
    int configuredVersionOverride = 0;
    if (TryLoadSettingsIniInt("Login", "ClientVersionOverride", &configuredVersionOverride)) {
        if (configuredVersionOverride > 0) {
            return static_cast<u32>(configuredVersionOverride);
        }

        DbgLog("[Login] Ignoring invalid ClientVersionOverride=%d; expected a positive integer\n",
               configuredVersionOverride);
    }

    int configuredOverride = 0;
    if (TryLoadSettingsIniInt("Login", "ClientDateOverride", &configuredOverride)) {
        const u32 overrideVersion = configuredOverride > 0 ? static_cast<u32>(configuredOverride) : 0;
        if (overrideVersion >= 20000000u) {
            return overrideVersion;
        }

        DbgLog("[Login] Ignoring invalid ClientDateOverride=%d; expected yyyymmdd-style date\n",
               configuredOverride);
    }

    const u32 configuredVersion = g_version > 0 ? static_cast<u32>(g_version) : 0;
    int useRawClientInfoVersion = 0;
    if (TryLoadSettingsIniInt("Login", "UseRawClientInfoVersion", &useRawClientInfoVersion)
        && useRawClientInfoVersion != 0
        && configuredVersion > 0) {
        return configuredVersion;
    }

    if (configuredVersion >= 20000000u) {
        return configuredVersion;
    }

    return ro::net::GetActiveAccountLoginPacketProfile().clientDate;
}

bool TryLoadLoginSettingInt(const char* key, int* value)
{
    return TryLoadSettingsIniInt("Login", key, value);
}

bool ShouldSendConnectInfoChangedOnLogin()
{
    int sendConnectInfoChanged = 0;
    if (TryLoadLoginSettingInt("SendConnectInfoChanged", &sendConnectInfoChanged)) {
        return sendConnectInfoChanged != 0;
    }

    return false;
}

bool UsesLoginChannelPacket();

enum class AccountLoginPacketKind {
    Legacy,
    LoginChannel,
    LoginPcBang,
    Login4,
};

AccountLoginPacketKind ResolveAccountLoginPacketKind()
{
    std::string overrideValue = LoadSettingsIniString("Login", "AccountLoginPacket", "");
    std::transform(overrideValue.begin(), overrideValue.end(), overrideValue.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    if (!overrideValue.empty()) {
        if (overrideValue == "64" || overrideValue == "0x64" || overrideValue == "plain" || overrideValue == "legacy") {
            return AccountLoginPacketKind::Legacy;
        }
        if (overrideValue == "2b0" || overrideValue == "0x2b0" || overrideValue == "channel" || overrideValue == "login_channel") {
            return AccountLoginPacketKind::LoginChannel;
        }
        if (overrideValue == "277" || overrideValue == "0x277" || overrideValue == "pcbang" || overrideValue == "login_pcbang") {
            return AccountLoginPacketKind::LoginPcBang;
        }
        if (overrideValue == "27c" || overrideValue == "0x27c" || overrideValue == "login4") {
            return AccountLoginPacketKind::Login4;
        }

        DbgLog("[Login] Ignoring invalid AccountLoginPacket override '%s'\n", overrideValue.c_str());
    }

    return UsesLoginChannelPacket() ? AccountLoginPacketKind::LoginChannel : AccountLoginPacketKind::Legacy;
}

u8 ResolveAccountClientType()
{
    int configuredClientType = -1;
    if (TryLoadLoginSettingInt("ClientTypeOverride", &configuredClientType)
        && configuredClientType >= 0
        && configuredClientType <= 255) {
        return static_cast<u8>(configuredClientType);
    }

    return GetAccountType();
}

bool UsesLoginChannelPacket()
{
    int forceLoginChannel = -1;
    if (TryLoadLoginSettingInt("ForceLoginChannel", &forceLoginChannel)) {
        return forceLoginChannel != 0;
    }

    return g_serviceType == ServiceKorea;
}

bool ShouldRequestPasswordHashOnConnect()
{
    int forcePasswordHash = 0;
    if (TryLoadLoginSettingInt("ForcePasswordHashRequest", &forcePasswordHash)) {
        return forcePasswordHash != 0;
    }

    return g_passwordEncrypt != 0;
}

bool HasConfiguredClientHashSetting()
{
    return !LoadSettingsIniString("Login", "ClientHashOverrideMd5", "").empty()
        || !LoadSettingsIniString("Login", "ClientHashSourceExe", "").empty();
}

bool ShouldSendExeHash()
{
    return g_serviceType == ServiceKorea || g_serviceType == ServiceIndonesia || HasConfiguredClientHashSetting();
}

int HexNibbleValue(char ch)
{
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'A' && ch <= 'F') {
        return 10 + (ch - 'A');
    }
    if (ch >= 'a' && ch <= 'f') {
        return 10 + (ch - 'a');
    }
    return -1;
}

bool TryParseMd5HexString(const std::string& text, std::array<u8, 16>* digest)
{
    if (!digest) {
        return false;
    }

    std::string compact;
    compact.reserve(text.size());
    for (char ch : text) {
        if (HexNibbleValue(ch) >= 0) {
            compact.push_back(ch);
        }
    }

    if (compact.size() != 32) {
        return false;
    }

    for (size_t index = 0; index < digest->size(); ++index) {
        const int high = HexNibbleValue(compact[index * 2]);
        const int low = HexNibbleValue(compact[index * 2 + 1]);
        if (high < 0 || low < 0) {
            return false;
        }
        (*digest)[index] = static_cast<u8>((high << 4) | low);
    }
    return true;
}

std::array<u8, 16> MakeDigestFromBytes(const u8 (&bytes)[16])
{
    std::array<u8, 16> digest{};
    std::memcpy(digest.data(), bytes, sizeof(bytes));
    return digest;
}

bool TryComputeFileMd5(const std::filesystem::path& filePath, std::array<u8, 16>* digest)
{
    if (!digest) {
        return false;
    }

    const std::string nativePath = filePath.string();
    if (nativePath.empty()) {
        return false;
    }

    FILE* file = std::fopen(nativePath.c_str(), "rb");
    if (!file) {
        return false;
    }

    if (std::fseek(file, 0, SEEK_END) != 0) {
        std::fclose(file);
        return false;
    }

    const long fileSize = std::ftell(file);
    if (fileSize <= 0 || std::fseek(file, 0, SEEK_SET) != 0) {
        std::fclose(file);
        return false;
    }

    std::vector<u8> fileBytes(static_cast<size_t>(fileSize));
    const size_t bytesRead = std::fread(fileBytes.data(), 1, fileBytes.size(), file);
    std::fclose(file);
    if (bytesRead != fileBytes.size()) {
        return false;
    }

    *digest = ro::cipher::ComputeMd5(fileBytes.data(), fileBytes.size());
    return true;
}

bool TryResolveConfiguredClientHash(std::array<u8, 16>* digest)
{
    if (!digest) {
        return false;
    }

    const std::string overrideMd5 = LoadSettingsIniString("Login", "ClientHashOverrideMd5", "");
    if (!overrideMd5.empty() && TryParseMd5HexString(overrideMd5, digest)) {
        DbgLog("[Login] Using configured ClientHashOverrideMd5 value\n");
        return true;
    }
    if (!overrideMd5.empty()) {
        DbgLog("[Login] Ignoring invalid ClientHashOverrideMd5 value: '%s'\n", overrideMd5.c_str());
    }

    const std::string overrideExe = LoadSettingsIniString("Login", "ClientHashSourceExe", "");
    if (overrideExe.empty()) {
        return false;
    }

    std::filesystem::path sourcePath(overrideExe);
    if (sourcePath.is_relative()) {
        sourcePath = GetOpenMidgardIniPath().parent_path() / sourcePath;
    }
    if (!TryComputeFileMd5(sourcePath, digest)) {
        DbgLog("[Login] Failed to compute MD5 from ClientHashSourceExe='%s'\n", sourcePath.string().c_str());
        return false;
    }

    DbgLog("[Login] Using configured ClientHashSourceExe='%s'\n", sourcePath.string().c_str());
    return true;
}

bool TryResolveExecutableHashPayload(std::array<u8, 16>* digest)
{
    if (!digest) {
        return false;
    }

    if (TryResolveConfiguredClientHash(digest)) {
        return true;
    }

    if (g_serviceType == ServiceIndonesia || (g_serviceType == ServiceKorea && g_serverType == ServerSakray)) {
        char exePath[260] = {};
        if (GetModuleFileNameA(nullptr, exePath, static_cast<DWORD>(sizeof(exePath))) > 0
            && TryComputeFileMd5(std::filesystem::path(exePath), digest)) {
            return true;
        }
    }

    if (g_serviceType == ServiceIndonesia && g_serverType == ServerSakray) {
        *digest = MakeDigestFromBytes(kIndonesiaSakrayHash);
        return true;
    }
    if (g_serverType == ServerSakray) {
        *digest = MakeDigestFromBytes(kStockSakrayHash);
        return true;
    }
    if (g_serverType == ServerNormal) {
        *digest = MakeDigestFromBytes(kStockMainHash);
        return true;
    }

    return false;
}

void FormatDigestHex(const std::array<u8, 16>& digest, char* buffer, size_t bufferSize)
{
    if (!buffer || bufferSize == 0) {
        return;
    }

    buffer[0] = '\0';
    size_t offset = 0;
    for (u8 byte : digest) {
        if (offset + 2 >= bufferSize) {
            break;
        }
        const int written = std::snprintf(buffer + offset, bufferSize - offset, "%02X", static_cast<unsigned int>(byte));
        if (written <= 0) {
            break;
        }
        offset += static_cast<size_t>(written);
    }
}

void FillLocalIpString(char* destination, size_t destinationSize)
{
    CopyCString(destination, destinationSize, "127.0.0.1");
    char hostName[100] = {};
    if (gethostname(hostName, sizeof(hostName)) != 0) {
        return;
    }

    hostent* host = gethostbyname(hostName);
    if (!host || !host->h_addr_list || !host->h_addr_list[0]) {
        return;
    }

    const char* ip = inet_ntoa(*reinterpret_cast<in_addr*>(host->h_addr_list[0]));
    if (ip && *ip) {
        CopyCString(destination, destinationSize, ip);
    }
}

void FillMacString(char* destination, size_t destinationSize)
{
    CopyCString(destination, destinationSize, "111111111111");
}

constexpr unsigned int kMenuOverlayTransparentKey = 0x00FF00FFu;

bool EnsureOverlayComposeSurface(int width, int height, ArgbDibSurface* composeSurface)
{
    return composeSurface && composeSurface->EnsureSize(width, height);
}

void ClearOverlayComposeBits(void* composeBits, int width, int height)
{
    if (!composeBits || width <= 0 || height <= 0) {
        return;
    }

    unsigned int* pixels = static_cast<unsigned int*>(composeBits);
    std::fill_n(pixels, static_cast<size_t>(width) * static_cast<size_t>(height), kMenuOverlayTransparentKey);
}

void ConvertOverlayComposeBitsToAlpha(void* composeBits, int width, int height)
{
    if (!composeBits || width <= 0 || height <= 0) {
        return;
    }

    unsigned int* pixels = static_cast<unsigned int*>(composeBits);
    const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
    for (size_t index = 0; index < pixelCount; ++index) {
        const unsigned int rgb = pixels[index] & 0x00FFFFFFu;
        pixels[index] = (rgb == (kMenuOverlayTransparentKey & 0x00FFFFFFu)) ? 0u : (0xFF000000u | rgb);
    }
}

void HashTokenValue(std::uint64_t* hash, std::uint64_t value)
{
    if (!hash) {
        return;
    }
    *hash ^= value;
    *hash *= 1099511628211ull;
}

void HashTokenString(std::uint64_t* hash, const std::string& value)
{
    if (!hash) {
        return;
    }
    for (unsigned char ch : value) {
        HashTokenValue(hash, static_cast<std::uint64_t>(ch));
    }
    HashTokenValue(hash, 0xFFull);
}

std::uint64_t ComputeLoginUiStateToken(int clientWidth, int clientHeight)
{
    std::uint64_t hash = 1469598103934665603ull;
    HashTokenValue(&hash, static_cast<std::uint64_t>(clientWidth));
    HashTokenValue(&hash, static_cast<std::uint64_t>(clientHeight));
    HashTokenString(&hash, g_windowMgr.m_loadedWallpaperPath);
    HashTokenString(&hash, g_windowMgr.GetLoginStatus());
    HashTokenValue(&hash, static_cast<std::uint64_t>(GetSelectedClientInfoIndex()));
    if (g_windowMgr.m_selectServerWnd && g_windowMgr.m_selectServerWnd->m_show != 0) {
        HashTokenValue(&hash, static_cast<std::uint64_t>(g_windowMgr.m_selectServerWnd->GetHoverIndex()));
    }
    if (g_windowMgr.m_loginWnd && g_windowMgr.m_loginWnd->m_show != 0) {
        HashTokenString(&hash, g_windowMgr.m_loginWnd->GetLoginText());
        HashTokenValue(&hash, static_cast<std::uint64_t>(g_windowMgr.m_loginWnd->GetPasswordLength()));
        HashTokenValue(&hash, static_cast<std::uint64_t>(g_windowMgr.m_loginWnd->IsSaveAccountChecked() ? 1 : 0));
        HashTokenValue(&hash, static_cast<std::uint64_t>(g_windowMgr.m_loginWnd->IsPasswordFocused() ? 1 : 0));
    }
    if (g_windowMgr.m_selectCharWnd && g_windowMgr.m_selectCharWnd->m_show != 0) {
        HashTokenValue(&hash, static_cast<std::uint64_t>(g_windowMgr.m_selectCharWnd->GetSelectedSlotNumber()));
        HashTokenValue(&hash, static_cast<std::uint64_t>(g_windowMgr.m_selectCharWnd->GetCurrentPage()));
        UISelectCharWnd::SelectedCharacterDisplay selected{};
        if (g_windowMgr.m_selectCharWnd->GetSelectedCharacterDisplay(&selected) && selected.valid) {
            HashTokenString(&hash, selected.name);
            HashTokenString(&hash, selected.job);
            HashTokenValue(&hash, static_cast<std::uint64_t>(selected.level));
        }
    }
    if (g_windowMgr.m_makeCharWnd && g_windowMgr.m_makeCharWnd->m_show != 0) {
        UIMakeCharWnd::MakeCharDisplay makeChar{};
        if (g_windowMgr.m_makeCharWnd->GetMakeCharDisplay(&makeChar)) {
            HashTokenString(&hash, makeChar.name);
            HashTokenValue(&hash, static_cast<std::uint64_t>(makeChar.nameFocused ? 1 : 0));
            for (int i = 0; i < 6; ++i) {
                HashTokenValue(&hash, static_cast<std::uint64_t>(makeChar.stats[i]));
            }
            HashTokenValue(&hash, static_cast<std::uint64_t>(makeChar.hairIndex));
            HashTokenValue(&hash, static_cast<std::uint64_t>(makeChar.hairColor));
        }
    }
    for (UIWindow* child : g_windowMgr.m_children) {
        if (!child) {
            continue;
        }
        HashTokenValue(&hash, static_cast<std::uint64_t>(static_cast<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(child))));
        HashTokenValue(&hash, static_cast<std::uint64_t>(child->m_id));
        HashTokenValue(&hash, static_cast<std::uint64_t>(child->m_show));
        HashTokenValue(&hash, static_cast<std::uint64_t>(child->m_x));
        HashTokenValue(&hash, static_cast<std::uint64_t>(child->m_y));
        HashTokenValue(&hash, static_cast<std::uint64_t>(child->m_w));
        HashTokenValue(&hash, static_cast<std::uint64_t>(child->m_h));
        HashTokenValue(&hash, static_cast<std::uint64_t>(child->m_isDirty));
    }
    return hash;
}

void SyncRendererToWindowSize()
{
    GetRenderDevice().RefreshRenderSize();
    const int renderWidth = GetRenderDevice().GetRenderWidth();
    const int renderHeight = GetRenderDevice().GetRenderHeight();
    if (renderWidth <= 0 || renderHeight <= 0) {
        return;
    }

    if (g_renderer.m_width != renderWidth || g_renderer.m_height != renderHeight) {
        g_renderer.SetSize(renderWidth, renderHeight);
    }
}

bool QueueFullScreenOverlayQuad(CTexture* texture, int width, int height, float sortKey, int mtPreset)
{
    if (!texture || width <= 0 || height <= 0) {
        return false;
    }

    RPFace* face = g_renderer.BorrowNullRP();
    if (!face) {
        return false;
    }

    const float left = OverlayScreenEdge(0);
    const float top = OverlayScreenEdge(0);
    const float right = OverlayScreenEdge(width);
    const float bottom = OverlayScreenEdge(height);
    const unsigned int overlayContentWidth = texture->m_surfaceUpdateWidth > 0 ? texture->m_surfaceUpdateWidth : static_cast<unsigned int>(width);
    const unsigned int overlayContentHeight = texture->m_surfaceUpdateHeight > 0 ? texture->m_surfaceUpdateHeight : static_cast<unsigned int>(height);
    const float maxU = texture->m_w != 0 ? static_cast<float>(overlayContentWidth) / static_cast<float>(texture->m_w) : 1.0f;
    const float maxV = texture->m_h != 0 ? static_cast<float>(overlayContentHeight) / static_cast<float>(texture->m_h) : 1.0f;

    face->primType = D3DPT_TRIANGLESTRIP;
    face->verts = face->m_verts;
    face->numVerts = 4;
    face->indices = nullptr;
    face->numIndices = 0;
    face->tex = texture;
    face->mtPreset = mtPreset;
    face->cullMode = D3DCULL_NONE;
    face->srcAlphaMode = D3DBLEND_SRCALPHA;
    face->destAlphaMode = D3DBLEND_INVSRCALPHA;
    face->alphaSortKey = sortKey;

    face->m_verts[0] = { left, top, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, 0.0f, 0.0f };
    face->m_verts[1] = { right, top, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, maxU, 0.0f };
    face->m_verts[2] = { left, bottom, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, 0.0f, maxV };
    face->m_verts[3] = { right, bottom, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, maxU, maxV };
    g_renderer.AddRP(face, 1 | 8);
    return true;
}

bool QueueRectOverlayQuad(CTexture* texture, int left, int top, int width, int height, float sortKey, int mtPreset)
{
    if (!texture || width <= 0 || height <= 0) {
        return false;
    }

    RPFace* face = g_renderer.BorrowNullRP();
    if (!face) {
        return false;
    }

    const float quadLeft = OverlayScreenEdge(left);
    const float quadTop = OverlayScreenEdge(top);
    const float quadRight = OverlayScreenEdge(left + width);
    const float quadBottom = OverlayScreenEdge(top + height);
    const unsigned int overlayContentWidth = texture->m_surfaceUpdateWidth > 0 ? texture->m_surfaceUpdateWidth : static_cast<unsigned int>(width);
    const unsigned int overlayContentHeight = texture->m_surfaceUpdateHeight > 0 ? texture->m_surfaceUpdateHeight : static_cast<unsigned int>(height);
    const float maxU = texture->m_w != 0 ? static_cast<float>(overlayContentWidth) / static_cast<float>(texture->m_w) : 1.0f;
    const float maxV = texture->m_h != 0 ? static_cast<float>(overlayContentHeight) / static_cast<float>(texture->m_h) : 1.0f;

    face->primType = D3DPT_TRIANGLESTRIP;
    face->verts = face->m_verts;
    face->numVerts = 4;
    face->indices = nullptr;
    face->numIndices = 0;
    face->tex = texture;
    face->mtPreset = mtPreset;
    face->cullMode = D3DCULL_NONE;
    face->srcAlphaMode = D3DBLEND_SRCALPHA;
    face->destAlphaMode = D3DBLEND_INVSRCALPHA;
    face->alphaSortKey = sortKey;

    face->m_verts[0] = { quadLeft, quadTop, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, 0.0f, 0.0f };
    face->m_verts[1] = { quadRight, quadTop, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, maxU, 0.0f };
    face->m_verts[2] = { quadLeft, quadBottom, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, 0.0f, maxV };
    face->m_verts[3] = { quadRight, quadBottom, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, maxU, maxV };
    g_renderer.AddRP(face, 1 | 8);
    return true;
}

bool QueueLoginUiQuad()
{
    if (!g_hMainWnd) {
        return false;
    }

    RECT clientRect{};
    GetClientRect(g_hMainWnd, &clientRect);
    const int clientWidth = clientRect.right - clientRect.left;
    const int clientHeight = clientRect.bottom - clientRect.top;
    if (clientWidth <= 0 || clientHeight <= 0) {
        return false;
    }

    static ArgbDibSurface s_uiComposeSurface;
    static std::vector<unsigned int> s_qtUiComposePixels;
    static std::uint64_t s_uiStateToken = 0ull;
    static bool s_uiTextureValid = false;

    static CTexture* s_uiTexture = nullptr;
    static int s_uiTextureWidth = 0;
    static int s_uiTextureHeight = 0;
    static CTexture* s_qtUiOverlayTexture = nullptr;
    static int s_qtUiOverlayTextureWidth = 0;
    static int s_qtUiOverlayTextureHeight = 0;
    static bool s_qtUiOverlayTextureValid = false;
    if (!s_uiTexture || s_uiTextureWidth != clientWidth || s_uiTextureHeight != clientHeight) {
        delete s_uiTexture;
        s_uiTexture = new CTexture();
        if (!s_uiTexture || !s_uiTexture->Create(clientWidth, clientHeight, PF_A8R8G8B8, false)) {
            delete s_uiTexture;
            s_uiTexture = nullptr;
            s_uiTextureWidth = 0;
            s_uiTextureHeight = 0;
            return false;
        }
        s_uiTextureWidth = clientWidth;
        s_uiTextureHeight = clientHeight;
        s_uiTextureValid = false;
        s_uiStateToken = 0ull;
    }
    if (!s_qtUiOverlayTexture || s_qtUiOverlayTextureWidth != clientWidth || s_qtUiOverlayTextureHeight != clientHeight) {
        delete s_qtUiOverlayTexture;
        s_qtUiOverlayTexture = new CTexture();
        if (!s_qtUiOverlayTexture
            || !s_qtUiOverlayTexture->Create(clientWidth, clientHeight, PF_A8R8G8B8, false)) {
            delete s_qtUiOverlayTexture;
            s_qtUiOverlayTexture = nullptr;
            s_qtUiOverlayTextureWidth = 0;
            s_qtUiOverlayTextureHeight = 0;
            s_qtUiOverlayTextureValid = false;
        } else {
            s_qtUiOverlayTextureWidth = clientWidth;
            s_qtUiOverlayTextureHeight = clientHeight;
            s_qtUiOverlayTextureValid = false;
        }
    }

    const bool uiDirty = g_windowMgr.HasDirtyVisualState();
    const std::uint64_t uiStateToken = ComputeLoginUiStateToken(clientWidth, clientHeight);
    const bool needUiRefresh = !s_uiTextureValid || uiDirty || uiStateToken != s_uiStateToken;
    if (needUiRefresh) {
        const bool qtMenuRuntimeEnabled = IsQtUiRuntimeEnabled();
        const bool allowQtCpuBridge = qtMenuRuntimeEnabled
            && GetRenderDevice().GetBackendType() != RenderBackendType::Vulkan;
        bool renderedQtMenuOverlay = false;
        if (qtMenuRuntimeEnabled && s_qtUiOverlayTexture) {
            renderedQtMenuOverlay = RenderQtUiMenuOverlayTexture(
                s_qtUiOverlayTexture,
                clientWidth,
                clientHeight);
        }

        {
            static int s_lastLoggedMenuPath = -1;
            const int menuPath = renderedQtMenuOverlay ? 2 : (allowQtCpuBridge ? 1 : 0);
            if (menuPath != s_lastLoggedMenuPath) {
                DbgLog("[LoginMode] menu overlay path=%s qtEnabled=%d texture=%p size=%dx%d\n",
                    renderedQtMenuOverlay ? "native_texture" : (allowQtCpuBridge ? "cpu_bridge" : "legacy_gdi"),
                    qtMenuRuntimeEnabled ? 1 : 0,
                    s_qtUiOverlayTexture,
                    clientWidth,
                    clientHeight);
                s_lastLoggedMenuPath = menuPath;
            }
        }

        if (renderedQtMenuOverlay) {
            g_windowMgr.ClearDirtyVisualState();
            s_uiTextureValid = false;
        } else {
            if (!allowQtCpuBridge) {
                const bool composeReady = EnsureOverlayComposeSurface(clientWidth, clientHeight, &s_uiComposeSurface);
                if (!composeReady) {
                    return false;
                }

                if (g_windowMgr.m_wallpaperSurface && g_windowMgr.m_wallpaperSurface->HasSoftwarePixels()) {
                    g_windowMgr.DrawWallpaperToDC(s_uiComposeSurface.GetDC(), clientWidth, clientHeight);
                } else {
                    HBRUSH clearBrush = CreateSolidBrush(RGB(0, 0, 0));
                    FillRect(s_uiComposeSurface.GetDC(), &clientRect, clearBrush);
                    DeleteObject(clearBrush);
                }

                g_windowMgr.DrawVisibleWindowsToHdc(s_uiComposeSurface.GetDC(), true);
            } else {
                g_windowMgr.ClearDirtyVisualState();
            }

            unsigned int* overlayPixels = nullptr;
            if (allowQtCpuBridge) {
                const size_t pixelCount = static_cast<size_t>(clientWidth) * static_cast<size_t>(clientHeight);
                if (s_qtUiComposePixels.size() != pixelCount) {
                    s_qtUiComposePixels.assign(pixelCount, 0u);
                } else {
                    std::fill(s_qtUiComposePixels.begin(), s_qtUiComposePixels.end(), 0u);
                }
                overlayPixels = s_qtUiComposePixels.data();
            } else {
                ConvertOverlayComposeBitsToAlpha(s_uiComposeSurface.GetBits(), clientWidth, clientHeight);
                overlayPixels = s_uiComposeSurface.GetPixels();
            }
            if (allowQtCpuBridge) {
                CompositeQtUiMenuOverlay(
                    overlayPixels,
                    clientWidth,
                    clientHeight,
                    clientWidth * static_cast<int>(sizeof(unsigned int)));
            }
            s_uiTexture->Update(0,
                0,
                clientWidth,
                clientHeight,
                overlayPixels,
                true,
                clientWidth * static_cast<int>(sizeof(unsigned int)));
            s_uiTextureValid = true;
        }
        s_qtUiOverlayTextureValid = renderedQtMenuOverlay;
        s_uiStateToken = uiStateToken;
    }

    bool queuedAnyUi = false;
    if (s_uiTextureValid) {
        queuedAnyUi = QueueFullScreenOverlayQuad(s_uiTexture, clientWidth, clientHeight, 1.0f, 3);
    }
    if (s_qtUiOverlayTextureValid) {
        queuedAnyUi = QueueFullScreenOverlayQuad(s_qtUiOverlayTexture, clientWidth, clientHeight, 2.0f, 3) || queuedAnyUi;
    }
    return queuedAnyUi;
}

bool QueueMenuCursorOverlayQuad(int cursorActNum, u32 mouseAnimStartTick)
{
    if (!g_hMainWnd) {
        return false;
    }

    RECT clientRect{};
    GetClientRect(g_hMainWnd, &clientRect);
    const int clientWidth = clientRect.right - clientRect.left;
    const int clientHeight = clientRect.bottom - clientRect.top;
    if (clientWidth <= 0 || clientHeight <= 0) {
        return false;
    }

    POINT cursorPos{};
    if (!GetModeCursorClientPos(&cursorPos)) {
        return false;
    }

    RECT cursorBounds{};
    const bool hasCustomBounds = GetModeCursorDrawBounds(cursorActNum, mouseAnimStartTick, &cursorBounds);
    const int textureWidth = hasCustomBounds ? (std::max)(1, static_cast<int>(cursorBounds.right - cursorBounds.left)) : 32;
    const int textureHeight = hasCustomBounds ? (std::max)(1, static_cast<int>(cursorBounds.bottom - cursorBounds.top)) : 32;
    const int drawOriginX = hasCustomBounds ? -(std::min)(0, static_cast<int>(cursorBounds.left)) : 0;
    const int drawOriginY = hasCustomBounds ? -(std::min)(0, static_cast<int>(cursorBounds.top)) : 0;
    const int left = hasCustomBounds ? cursorPos.x + (std::min)(0, static_cast<int>(cursorBounds.left)) : cursorPos.x;
    const int top = hasCustomBounds ? cursorPos.y + (std::min)(0, static_cast<int>(cursorBounds.top)) : cursorPos.y;
    static std::vector<unsigned int> s_cursorComposePixels;
    static bool s_cursorTextureValid = false;
    static CTexture* s_cursorTexture = nullptr;
    static int s_cursorTextureWidth = 0;
    static int s_cursorTextureHeight = 0;
    static std::uint64_t s_cursorStateToken = 0ull;
    if (!s_cursorTexture || s_cursorTextureWidth != textureWidth || s_cursorTextureHeight != textureHeight) {
        delete s_cursorTexture;
        s_cursorTexture = new CTexture();
        if (!s_cursorTexture || !s_cursorTexture->Create(textureWidth, textureHeight, PF_A8R8G8B8, false)) {
            delete s_cursorTexture;
            s_cursorTexture = nullptr;
            s_cursorTextureWidth = 0;
            s_cursorTextureHeight = 0;
            return false;
        }
        s_cursorTextureWidth = textureWidth;
        s_cursorTextureHeight = textureHeight;
        s_cursorTextureValid = false;
        s_cursorStateToken = 0ull;
        s_cursorComposePixels.assign(static_cast<size_t>(textureWidth) * static_cast<size_t>(textureHeight), 0u);
    } else if (s_cursorComposePixels.size() != static_cast<size_t>(textureWidth) * static_cast<size_t>(textureHeight)) {
        s_cursorComposePixels.assign(static_cast<size_t>(textureWidth) * static_cast<size_t>(textureHeight), 0u);
    }

    std::uint64_t cursorStateToken = 1469598103934665603ull;
    HashTokenValue(&cursorStateToken, static_cast<std::uint64_t>(cursorActNum));
    HashTokenValue(&cursorStateToken, static_cast<std::uint64_t>(GetModeCursorVisualFrame(cursorActNum, mouseAnimStartTick)));
    HashTokenValue(&cursorStateToken, static_cast<std::uint64_t>(static_cast<unsigned int>(textureWidth)));
    HashTokenValue(&cursorStateToken, static_cast<std::uint64_t>(static_cast<unsigned int>(textureHeight)));

    if (!s_cursorTextureValid || cursorStateToken != s_cursorStateToken) {
        std::fill(s_cursorComposePixels.begin(), s_cursorComposePixels.end(), 0u);
        if (!DrawModeCursorAtToArgb(
            s_cursorComposePixels.data(),
            textureWidth,
            textureHeight,
            drawOriginX,
            drawOriginY,
            cursorActNum,
            mouseAnimStartTick)) {
            return false;
        }
        s_cursorTexture->Update(0,
            0,
            textureWidth,
            textureHeight,
            s_cursorComposePixels.data(),
            true,
            textureWidth * static_cast<int>(sizeof(unsigned int)));
        s_cursorTextureValid = true;
        s_cursorStateToken = cursorStateToken;
    }

    RPFace* face = g_renderer.BorrowNullRP();
    if (!face) {
        return false;
    }

    const unsigned int overlayContentWidth = s_cursorTexture->m_surfaceUpdateWidth > 0 ? s_cursorTexture->m_surfaceUpdateWidth : static_cast<unsigned int>(textureWidth);
    const unsigned int overlayContentHeight = s_cursorTexture->m_surfaceUpdateHeight > 0 ? s_cursorTexture->m_surfaceUpdateHeight : static_cast<unsigned int>(textureHeight);
    const float maxU = s_cursorTexture->m_w != 0 ? static_cast<float>(overlayContentWidth) / static_cast<float>(s_cursorTexture->m_w) : 1.0f;
    const float maxV = s_cursorTexture->m_h != 0 ? static_cast<float>(overlayContentHeight) / static_cast<float>(s_cursorTexture->m_h) : 1.0f;
    const float quadLeft = static_cast<float>(left);
    const float quadTop = static_cast<float>(top);
    const float quadRight = static_cast<float>(left + textureWidth);
    const float quadBottom = static_cast<float>(top + textureHeight);

    face->primType = D3DPT_TRIANGLESTRIP;
    face->verts = face->m_verts;
    face->numVerts = 4;
    face->indices = nullptr;
    face->numIndices = 0;
    face->tex = s_cursorTexture;
    face->mtPreset = 0;
    face->cullMode = D3DCULL_NONE;
    face->srcAlphaMode = D3DBLEND_SRCALPHA;
    face->destAlphaMode = D3DBLEND_INVSRCALPHA;
    // The Qt menu image is a full-screen quad at sort key 2. Equal keys are
    // ordered by face pointer, so the menu was covering this cursor. The
    // in-game HUD sits at 1 and leaves 2 for the cursor.
    face->alphaSortKey = 3.0f;
    face->m_verts[0] = { quadLeft, quadTop, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, 0.0f, 0.0f };
    face->m_verts[1] = { quadRight, quadTop, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, maxU, 0.0f };
    face->m_verts[2] = { quadLeft, quadBottom, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, 0.0f, maxV };
    face->m_verts[3] = { quadRight, quadBottom, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, maxU, maxV };
    g_renderer.AddRP(face, 1 | 8);
    return true;
}

} // namespace

CLoginMode::CLoginMode() 
        : m_numServer(0), m_serverSelected(0), m_numChar(0),
            m_selectedCharIndex(0), m_selectedCharSlot(0), m_pendingDeleteCharGid(0), m_pendingDeleteCharSlot(-1), m_subModeStartTime(0),
    m_syncRequestTime(0), m_waitingForPasswordHash(false), m_plainAccountLoginSent(false), m_wndWait(nullptr), m_multiLang(0), 
      m_nSelectedAccountNo(0), m_nSelectedAccountNo2(0),
    m_zonePort(0)
{
    std::memset(m_charParam, 0, sizeof(m_charParam));
    std::memset(m_makingCharName, 0, sizeof(m_makingCharName));
    std::memset(m_emaiAddress, 0, sizeof(m_emaiAddress));
    std::memset(m_userPassword, 0, sizeof(m_userPassword));
    std::memset(m_userId, 0, sizeof(m_userId));
    std::memset(m_serverInfo, 0, sizeof(m_serverInfo));
    std::memset(m_charInfo, 0, sizeof(m_charInfo));
    std::memset(m_zoneAddr, 0, sizeof(m_zoneAddr));
}

CLoginMode::~CLoginMode() {
}

void CLoginMode::OnInit(const char* worldName) {
    (void)worldName;

    m_loopCond = 1;
    m_isConnected = 0;
    m_nextSubMode = -1;
    m_subModeCnt = 0;

    if (!LoadClientInfoCandidates()) {
        m_strErrorInfo = "Unable to load client info; using fallback account endpoint 127.0.0.1:6900.";
        g_accountAddr = "127.0.0.1";
        g_accountPort = "6900";
    } else if (GetClientInfoConnectionCount() > 1) {
        SetLoginStatus("Login: select a server before connecting.");
    }

    m_wallPaperBmpName = ChooseLoginWallpaperName();
    g_session.ClearInventoryItems();
    g_windowMgr.ClearChatEvents();
    g_windowMgr.RemoveAllWindows();
    g_windowMgr.SetLoginWallpaper(m_wallPaperBmpName);

    m_subMode = LoginSubMode_Login;
    if (g_session.m_pendingReturnToCharSelect != 0 && CanReturnToCharacterSelect()) {
        m_subMode = LoginSubMode_ConnectChar;
        g_session.m_pendingReturnToCharSelect = 0;
    }
    m_selectedCharIndex = 0;
    m_selectedCharSlot = 0;
    if (GetClientInfoConnectionCount() <= 1) {
        SetLoginStatus("Login: ready.");
    }

    CAudio* audio = CAudio::GetInstance();
    if (audio) {
        audio->PlayBGM("bgm\\01.mp3");
    }

    OnChangeState(m_subMode);
}

void CLoginMode::OnExit() {
    RefreshMainWindowTitle();
}

int CLoginMode::OnRun() {
    if (m_nextSubMode != -1) {
        const int nextSubMode = m_nextSubMode;
        DbgLog("[Login] switching state %d -> %d\n", m_subMode, nextSubMode);
        m_subMode = nextSubMode;
        m_subModeCnt = 0;
        m_nextSubMode = -1;
        OnChangeState(nextSubMode);
    }

    OnUpdate();

    ++m_subModeCnt;
    return 1;
}

void CLoginMode::OnUpdate() {
    MainLoopWatchdogSetStage("Login:SyncRendererToWindowSize");
    SyncRendererToWindowSize();

    if (m_isConnected) {
        MainLoopWatchdogSetStage("Login:PollNetwork");
        PollNetwork();
        if (m_isConnected && !CRagConnection::instance()->IsOpen()) {
            const char* message = (m_subMode == LoginSubMode_ConnectAccount)
                ? (m_waitingForPasswordHash
                    ? "Login server closed the connection while waiting for the password-hash challenge response."
                    : (m_plainAccountLoginSent
                        ? "Login server closed the connection after the account login request. The server likely rejected the login packet shape, client type, or password mode without returning a RO error packet."
                        : "Login server closed the connection before authentication completed."))
                : (m_subMode == LoginSubMode_ConnectChar)
                    ? "Char server closed the connection before returning the character list."
                    : "Server closed the connection.";
            DbgLog("[Login] connection dropped in submode %d\n", m_subMode);
            SetLoginStatus(message);
            ShowLoginErrorDialog(message);
            m_isConnected = 0;
            m_nextSubMode = LoginSubMode_Login;
            return;
        }
        if (m_isConnected
            && m_subMode == LoginSubMode_ConnectAccount
            && m_waitingForPasswordHash
            && !m_plainAccountLoginSent
            && GetTickCount() - m_syncRequestTime >= kPasswordHashFallbackDelayMs) {
            DbgLog("[Login] password hash challenge timed out, falling back to CA_LOGIN\n");
            SendPlainAccountLogin();
        }
    }

    MainLoopWatchdogSetStage("Login:UIWindowMgr::OnProcess");
    g_windowMgr.OnProcess();
    const bool hasLegacyDevice = GetRenderDevice().GetLegacyDevice() != nullptr;
    if (!hasLegacyDevice) {
        MainLoopWatchdogSetStage("Login:Clear");
        g_renderer.ClearBackground();
        g_renderer.Clear(0);
        MainLoopWatchdogSetStage("Login:QueueLoginUiQuad");
        const bool queuedUi = QueueLoginUiQuad();
        if (queuedUi) {
            MainLoopWatchdogSetStage("Login:QueueMenuCursorOverlayQuad");
            QueueMenuCursorOverlayQuad(m_cursorActNum, m_mouseAnimStartTick);
            MainLoopWatchdogSetStage("Login:DrawScene");
            g_renderer.DrawScene();
            MainLoopWatchdogSetStage("Login:Flip");
            g_renderer.Flip(false);
        } else {
            MainLoopWatchdogSetStage("Login:UIWindowMgr::OnDraw");
            g_windowMgr.OnDraw();
            if (!IsQtUiRuntimeEnabled()) {
                MainLoopWatchdogSetStage("Login:DrawModeCursor");
                DrawModeCursor(m_cursorActNum, m_mouseAnimStartTick);
            }
        }
    } else {
        MainLoopWatchdogSetStage("Login:UIWindowMgr::OnDraw(legacy)");
        g_windowMgr.OnDraw();
    }
    if (hasLegacyDevice) {
        MainLoopWatchdogSetStage("Login:DrawModeCursor(legacy)");
        DrawModeCursor(m_cursorActNum, m_mouseAnimStartTick);
    }

    MainLoopWatchdogSetStage("Login:Sleep");
    Sleep(1);
}

msgresult_t CLoginMode::SendMsg(int msg, msgparam_t wparam, msgparam_t lparam, msgparam_t extra) {
    switch (msg) {
    case LoginMsg_RequestConnect:
        m_nextSubMode = LoginSubMode_ConnectAccount;
        SetLoginStatus("Login: connect requested.");
        return 1;

    case LoginMsg_SelectClientInfo:
        if (wparam >= 0 && wparam < GetClientInfoConnectionCount()) {
            const int clientIndex = static_cast<int>(wparam);
            SelectClientInfo(clientIndex);
            const std::vector<ClientInfoConnection>& connections = GetClientInfoConnections();
            const char* display = connections[clientIndex].display.empty()
                ? connections[clientIndex].address.c_str()
                : connections[clientIndex].display.c_str();
            char status[160] = {};
            std::snprintf(status, sizeof(status), "Login: selected server '%s'.", display ? display : "");
            SetLoginStatus(status);
            return 1;
        }
        return 0;

    case LoginMsg_SetPassword:
        CopyCString(m_userPassword, sizeof(m_userPassword), reinterpret_cast<const char*>(wparam));
        return 1;

    case LoginMsg_SetUserId:
        CopyCString(m_userId, sizeof(m_userId), reinterpret_cast<const char*>(wparam));
        return 1;

    case LoginMsg_ReturnToLogin:
    case LoginMsg_Disconnect:
        CRagConnection::instance()->Disconnect();
        m_isConnected = 0;
        m_nextSubMode = LoginSubMode_Login;
        SetLoginStatus("Login: disconnected.");
        return 1;

    case LoginMsg_Quit:
        g_modeMgr.Quit();
        return 1;

    case LoginMsg_RequestAccount:
        SetLoginStatus("Login: account request flow not implemented yet.");
        return 1;

    case LoginMsg_Intro:
        SetLoginStatus("Login: intro action not implemented yet.");
        return 1;

    case LoginMsg_SaveAccount:
        SetLoginStatus(extra != 0 ? "Login: save account enabled." : "Login: save account disabled.");
        return 1;

    case LoginMsg_SetEmail:
        CopyCString(m_emaiAddress, sizeof(m_emaiAddress), reinterpret_cast<const char*>(wparam));
        return 1;

    case LoginMsg_GetEmail:
        return reinterpret_cast<msgresult_t>(GetEmail());

    case LoginMsg_GetMakingCharName:
        return reinterpret_cast<msgresult_t>(GetMakingCharName());

    case LoginMsg_SetMakingCharName:
        CopyCString(m_makingCharName, sizeof(m_makingCharName), reinterpret_cast<const char*>(wparam));
        return 1;

    case LoginMsg_GetCharParam:
        return reinterpret_cast<msgresult_t>(GetCharParam());

    case LoginMsg_SetCharParam:
        if (wparam != 0) {
            std::memcpy(m_charParam, reinterpret_cast<const void*>(wparam), sizeof(m_charParam));
        }
        return 1;

    case LoginMsg_GetCharInfo:
        return reinterpret_cast<msgresult_t>(GetCharInfo());

    case LoginMsg_GetCharCount:
        return m_numChar;

    case LoginMsg_SelectCharacter:
        if (wparam >= 0 && wparam < m_numChar) {
            const int selectedIndex = static_cast<int>(wparam);
            m_selectedCharIndex = selectedIndex;
            m_selectedCharSlot = static_cast<int>(m_charInfo[selectedIndex].CharNum);
            g_session.SetSelectedCharacterAppearance(m_charInfo[selectedIndex]);
            m_nextSubMode = LoginSubMode_SelectChar;
            return 1;
        }
        return 0;

    case LoginMsg_RequestMakeCharacter: {
        m_selectedCharSlot = static_cast<int>(wparam);
        m_nextSubMode = LoginSubMode_MakeChar;
        return 1;
    }

    case LoginMsg_RequestDeleteCharacter: {
        if (!m_isConnected) {
            SetLoginStatus("Login: not connected to the char server.");
            return 0;
        }

        const int requestedSlot = static_cast<int>(wparam);
        int deleteIndex = static_cast<int>(lparam);
        if (deleteIndex < 0 || deleteIndex >= m_numChar || static_cast<int>(m_charInfo[deleteIndex].CharNum) != requestedSlot) {
            deleteIndex = -1;
            for (int index = 0; index < m_numChar; ++index) {
                if (static_cast<int>(m_charInfo[index].CharNum) == requestedSlot) {
                    deleteIndex = index;
                    break;
                }
            }
        }

        if (deleteIndex < 0 || deleteIndex >= m_numChar) {
            SetLoginStatus("Login: no character in the selected slot to delete.");
            return 0;
        }

        m_selectedCharIndex = deleteIndex;
        m_selectedCharSlot = static_cast<int>(m_charInfo[deleteIndex].CharNum);
        m_pendingDeleteCharGid = m_charInfo[deleteIndex].GID;
        m_pendingDeleteCharSlot = m_selectedCharSlot;
        m_nextSubMode = LoginSubMode_DeleteChar;
        return 1;
    }

    case LoginMsg_CreateCharacter: {
        if (!m_isConnected) {
            SetLoginStatus("Login: not connected to the char server.");
            return 0;
        }

        PACKET_CZ_MAKE_CHAR pkt{};
        pkt.PacketType = ro::net::GetActiveCharacterPacketProfile().makeCharacter;
        CopyCString(pkt.name, sizeof(pkt.name), m_makingCharName);
        pkt.Str = static_cast<u8>(m_charParam[0]);
        pkt.Agi = static_cast<u8>(m_charParam[1]);
        pkt.Vit = static_cast<u8>(m_charParam[2]);
        pkt.Int = static_cast<u8>(m_charParam[3]);
        pkt.Dex = static_cast<u8>(m_charParam[4]);
        pkt.Luk = static_cast<u8>(m_charParam[5]);
        pkt.CharNum = static_cast<u8>(m_selectedCharSlot);
        pkt.hairColor = static_cast<u16>(m_charParam[6]);
        pkt.hairStyle = static_cast<u16>(m_charParam[7]);

        CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(&pkt), static_cast<int>(sizeof(pkt)));
        DbgLog("[Login] CH_MAKE_CHAR sent: name='%.24s' stats=%d/%d/%d/%d/%d/%d slot=%d hairColor=%d hairStyle=%d\n",
               pkt.name,
               (int)pkt.Str, (int)pkt.Agi, (int)pkt.Vit, (int)pkt.Int, (int)pkt.Dex, (int)pkt.Luk,
               (int)pkt.CharNum, (int)pkt.hairColor, (int)pkt.hairStyle);

        m_wndWait = static_cast<UIWaitWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_WAITWND));
        if (m_wndWait) {
            m_wndWait->SetMsg("Creating character...", 16, 1);
        }

        SetLoginStatus("Login: requesting character creation...");
        return 1;
    }

    case LoginMsg_ReturnToCharSelect:
        m_nextSubMode = LoginSubMode_CharSelect;
        return 1;

    default:
        break;
    }

    return 0;
}

void CLoginMode::OnChangeState(int newState) {
    m_subModeStartTime = GetTickCount();
    ResetAccountLoginHandshake();
    g_windowMgr.RemoveAllWindows();
    g_windowMgr.SetLoginWallpaper(m_wallPaperBmpName);
    m_wndWait = nullptr;

    switch (newState) {
    case LoginSubMode_Login: {
        UIWindow* loginWnd = g_windowMgr.MakeWindow(UIWindowMgr::WID_LOGINWND);
        if (loginWnd) {
            loginWnd->SetShow(1);
        }
        if (GetClientInfoConnectionCount() > 1) {
            UIWindow* serverWnd = g_windowMgr.MakeWindow(UIWindowMgr::WID_SELECTSERVERWND);
            if (serverWnd) {
                serverWnd->SetShow(1);
            }
            SetLoginStatus("Login: select a server before connecting.");
        } else {
            SetLoginStatus("Login: ready.");
        }
        break;
    }

    case LoginSubMode_ConnectAccount: {
        const int port = g_accountPort.empty() ? 6900 : std::atoi(g_accountPort.c_str());
        m_wndWait = static_cast<UIWaitWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_WAITWND));
        if (m_wndWait) {
            m_wndWait->SetMsg("Connecting to account server...", 16, 1);
        }

        m_isConnected = CRagConnection::instance()->Connect(
            g_accountAddr.empty() ? "127.0.0.1" : g_accountAddr.c_str(),
            port > 0 ? port : 6900) ? 1 : 0;

        if (!m_isConnected) {
            CRagConnection::instance()->Disconnect();
            SetLoginStatus("Login: account server connection failed.");
            ShowConnectionFailedDialog("account server");
            m_nextSubMode = LoginSubMode_Login;
            break;
        }

        if (ShouldSendConnectInfoChangedOnLogin()) {
            SendConnectInfoChanged();
        }
        SendExeHashCheck();
        if (ShouldRequestPasswordHashOnConnect()) {
            SendPasswordHashRequest();
        } else {
            SendPlainAccountLogin();
        }
        break;
    }

    case LoginSubMode_ConnectChar: {
        const char* charIp = nullptr;
        int charPort = 0;
        if (m_serverSelected >= 0 && m_serverSelected < m_numServer) {
            const SERVER_ADDR& sv = m_serverInfo[m_serverSelected];
            in_addr addrIn{};
            addrIn.s_addr = sv.ip;
            charIp = inet_ntoa(addrIn);
            charPort = static_cast<int>(static_cast<u16>(sv.port));
        } else if (CanReturnToCharacterSelect()) {
            charIp = g_session.m_charServerAddr;
            charPort = g_session.m_charServerPort;
        }

        if (!charIp || !*charIp || charPort <= 0) {
            SetLoginStatus("Login: no char server to connect to.");
            m_nextSubMode = LoginSubMode_Login;
            break;
        }

        DbgLog("[Login] Connecting to char server: %s:%d (name='%.20s')\n",
               charIp, charPort,
               (m_serverSelected >= 0 && m_serverSelected < m_numServer)
                   ? reinterpret_cast<const char*>(m_serverInfo[m_serverSelected].name)
                   : "return");

        CopyCString(g_session.m_charServerAddr, sizeof(g_session.m_charServerAddr), charIp);
        g_session.m_charServerPort = charPort;

        // Disconnect from account server before connecting to char server.
        CRagConnection::instance()->Disconnect();
        m_isConnected = 0;

        m_wndWait = static_cast<UIWaitWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_WAITWND));
        if (m_wndWait) {
            m_wndWait->SetMsg("Connecting to char server...", 16, 1);
        }

        m_isConnected = CRagConnection::instance()->Connect(charIp, charPort) ? 1 : 0;
        if (!m_isConnected) {
            CRagConnection::instance()->Disconnect();
            SetLoginStatus("Login: char server connection failed.");
            ShowConnectionFailedDialog("char server");
            m_nextSubMode = LoginSubMode_Login;
            break;
        }

        // Send CA_ENTER (0x0065) to char server
        PACKET_CA_ENTER ePkt{};
        ePkt.PacketType = ro::net::GetActiveCharacterPacketProfile().charServerEnter;
        ePkt.AID        = g_session.m_aid;
        ePkt.AuthCode   = g_session.m_authCode;
        ePkt.UserLevel  = g_session.m_userLevel;
        ePkt.unused     = 0;
        ePkt.Sex        = g_session.m_sex;
        CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(&ePkt), static_cast<int>(sizeof(ePkt)));
        DbgLog("[Login] CA_ENTER sent: aid=%u authCode=%u userLevel=%u sex=%d\n",
               ePkt.AID, ePkt.AuthCode, ePkt.UserLevel, (int)ePkt.Sex);
        SetLoginStatus("Login: waiting for character list...");
        break;
    }

    case LoginSubMode_CharSelect: {
        DbgLog("[Login] Entering LoginSubMode_CharSelect\n");
        UIWindow* charSelectWnd = g_windowMgr.MakeWindow(UIWindowMgr::WID_SELECTCHARWND);
        DbgLog("[Login] MakeWindow(WID_SELECTCHARWND) returned=%p\n", charSelectWnd);
        if (charSelectWnd) {
            charSelectWnd->SetShow(1);
            DbgLog("[Login] Select char window show enabled\n");
        }
        g_windowMgr.SetLoginStatus("");
        DbgLog("[Login] LoginSubMode_CharSelect ready\n");
        break;
    }

    case LoginSubMode_MakeChar: {
        UIWindow* makeCharWnd = g_windowMgr.MakeWindow(UIWindowMgr::WID_MAKECHARWND);
        if (makeCharWnd) {
            makeCharWnd->SetShow(1);
        }
        SetLoginStatus("Login: create a new character.");
        break;
    }

    case LoginSubMode_SelectChar: {
        if (m_numChar <= 0 || m_selectedCharIndex < 0 || m_selectedCharIndex >= m_numChar) {
            SetLoginStatus("Login: no characters on this account.");
            m_nextSubMode = LoginSubMode_Login;
            break;
        }

        const u8 charNum = m_charInfo[m_selectedCharIndex].CharNum;
        g_session.SetSelectedCharacterAppearance(m_charInfo[m_selectedCharIndex]);
        DbgLog("[Login] Selecting char slot %d (name='%.24s')\n",
               (int)charNum, reinterpret_cast<const char*>(m_charInfo[m_selectedCharIndex].name));

        PACKET_CZ_SELECT_CHAR selPkt{};
        selPkt.PacketType = ro::net::GetActiveCharacterPacketProfile().selectCharacter;
        selPkt.CharNum    = charNum;
        CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(&selPkt), static_cast<int>(sizeof(selPkt)));
        SetLoginStatus("Login: selecting character...");
        break;
    }

    case LoginSubMode_DeleteChar: {
        if (!m_isConnected) {
            m_pendingDeleteCharGid = 0;
            m_pendingDeleteCharSlot = -1;
            SetLoginStatus("Login: char server connection lost before delete request.");
            m_nextSubMode = LoginSubMode_Login;
            break;
        }
        if (m_pendingDeleteCharGid == 0 || m_pendingDeleteCharSlot < 0) {
            SetLoginStatus("Login: no character selected for deletion.");
            m_nextSubMode = LoginSubMode_CharSelect;
            break;
        }

        m_wndWait = static_cast<UIWaitWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_WAITWND));
        if (m_wndWait) {
            m_wndWait->SetMsg("Deleting character...", 16, 1);
        }

        PACKET_CH_DELETE_CHAR pkt{};
        pkt.PacketType = ro::net::GetActiveCharacterPacketProfile().deleteCharacter;
        pkt.GID = m_pendingDeleteCharGid;
        CopyCString(pkt.key, sizeof(pkt.key), m_emaiAddress);

        const bool sent = CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(&pkt), static_cast<int>(sizeof(pkt)));
        DbgLog("[Login] CH_DELETE_CHAR sent: gid=%u slot=%d keyProvided=%d sent=%d\n",
               pkt.GID,
               m_pendingDeleteCharSlot,
               pkt.key[0] != '\0' ? 1 : 0,
               sent ? 1 : 0);

        if (!sent) {
            m_pendingDeleteCharGid = 0;
            m_pendingDeleteCharSlot = -1;
            SetLoginStatus("Login: failed to send delete-character request.");
            m_nextSubMode = LoginSubMode_CharSelect;
            break;
        }

        if (pkt.key[0] != '\0') {
            SetLoginStatus("Login: waiting for delete-character response...");
        } else {
            SetLoginStatus("Login: delete request sent without an email/delete key; server may refuse it.");
        }
        break;
    }

    case LoginSubMode_ZoneConnect: {
        // Disconnect from char server.
        CRagConnection::instance()->Disconnect();
        m_isConnected = 0;

        DbgLog("[Login] Connecting to zone server: %s:%d map=%s\n",
               m_zoneAddr, m_zonePort, g_session.m_curMap);

        m_wndWait = static_cast<UIWaitWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_WAITWND));
        if (m_wndWait) {
            m_wndWait->SetMsg("Connecting to zone server...", 16, 1);
        }

        m_isConnected = CRagConnection::instance()->Connect(m_zoneAddr, m_zonePort) ? 1 : 0;
        if (!m_isConnected) {
            CRagConnection::instance()->Disconnect();
            SetLoginStatus("Login: zone server connection failed.");
            ShowConnectionFailedDialog("zone server");
            m_nextSubMode = LoginSubMode_Login;
            break;
        }

        std::array<u8, sizeof(PACKET_CZ_ENTER_PACKETVER22)> zonePacket{};
        int zonePacketLength = 0;
        if (!ro::net::BuildActiveWantToConnectionPacket(
                g_session.m_aid,
                g_session.m_gid,
                g_session.m_authCode,
                timeGetTime(),
                g_session.m_sex,
                zonePacket.data(),
                static_cast<int>(zonePacket.size()),
                &zonePacketLength)) {
            SetLoginStatus("Login: failed to build zone-entry packet.");
            m_nextSubMode = LoginSubMode_Login;
            break;
        }

        const u16 zoneOpcode = static_cast<u16>(zonePacket[0]) | (static_cast<u16>(zonePacket[1]) << 8);
        CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(zonePacket.data()), zonePacketLength);
    char zoneHex[3 * sizeof(PACKET_CZ_ENTER_PACKETVER22) + 1] = {};
     FormatPacketBytes(zonePacket.data(), zonePacketLength, zoneHex, sizeof(zoneHex));
     DbgLog("[Login] wanttoconnection sent to zone: opcode=0x%04X aid=%u gid=%u len=%d bytes=%s\n",
               zoneOpcode,
               g_session.m_aid,
               g_session.m_gid,
         zonePacketLength,
         zoneHex);
        SetLoginStatus("Login: waiting for zone server...");
        break;
    }

    case LoginSubMode_ServerSelect:
    case LoginSubMode_Notice:
    case LoginSubMode_Licence:
    case LoginSubMode_AccountList:
    case LoginSubMode_SubAccount:
    case LoginSubMode_CharSelectReturn:
    default:
        g_windowMgr.MakeWindow(UIWindowMgr::WID_LOGINWND);
        SetLoginStatus("Login: sub-mode not implemented.");
        break;
    }
}

void CLoginMode::SetLoginStatus(const char* status)
{
    if (!status) {
        return;
    }

    if (m_strErrorInfo == status) {
        return;
    }

    m_strErrorInfo = status;
    g_windowMgr.SetLoginStatus(m_strErrorInfo);
    RefreshMainWindowTitle(status);
}

bool CLoginMode::LoadClientInfoCandidates()
{
    const DWORD dataAttrs = GetFileAttributesA("data");
    const bool hasDataFolder = dataAttrs != INVALID_FILE_ATTRIBUTES && (dataAttrs & FILE_ATTRIBUTE_DIRECTORY) != 0;

    std::vector<const char*> candidates;
    if (hasDataFolder) {
        candidates.push_back("data\\clientinfo.xml");
        candidates.push_back("data\\sclientinfo.xml");
    }
    candidates.push_back("data\\clientinfo.xml");
    candidates.push_back("data\\sclientinfo.xml");
    candidates.push_back("clientinfo.xml");
    candidates.push_back("sclientinfo.xml");
    candidates.push_back("System\\clientinfo.xml");
    candidates.push_back("System\\sclientinfo.xml");

    for (const char* path : candidates) {
        if (InitClientInfo(path)) {
            g_session.InitAccountInfo();
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Network polling  – called each frame while the socket is open
// ---------------------------------------------------------------------------
void CLoginMode::PollNetwork()
{
    std::vector<u8> pkt;
    for (int budget = 32; budget > 0; --budget) {
        if (!CRagConnection::instance()->RecvPacket(pkt)) break;
        if (pkt.size() < 2) continue;

        const u16 id = static_cast<u16>(pkt[0]) | (static_cast<u16>(pkt[1]) << 8);
        const ro::net::AccountLoginPacketProfile& loginProfile = ro::net::GetActiveAccountLoginPacketProfile();
        const ro::net::MapReceiveProfile& mapReceiveProfile = ro::net::GetActiveMapReceiveProfile();
        char status[96];
        std::snprintf(status, sizeof(status), "Login: received packet 0x%04X (%d bytes)", id, static_cast<int>(pkt.size()));
        SetLoginStatus(status);
        if (id == loginProfile.passwordHashChallenge) {
            OnAckHash(pkt);
            continue;
        }
        if (id == mapReceiveProfile.acceptEnterLegacy || id == mapReceiveProfile.acceptEnterModern) {
            OnZcAcceptEnter(pkt);
            return;
        }
        if (id == loginProfile.notifyError) {
            OnNotifyError(pkt);
            continue;
        }
        switch (id) {
        case 0x0069: OnAcceptLogin(pkt);    break;
        case 0x006A: OnRefuseLogin(pkt);    break;
        case 0x006B: OnAcceptChar(pkt);     break;
        case 0x006C: OnRefuseChar(pkt);     break;
        case 0x006D: OnAcceptMakeChar(pkt); break;
        case 0x006E: OnRefuseMakeChar(pkt); break;
        case 0x006F: OnAcceptDeleteChar(pkt); break;
        case 0x0070: OnRefuseDeleteChar(pkt); break;
        case 0x0071: OnNotifyZonesvr(pkt);  break;
        case 0x0081: OnDisconnectMsg(pkt);  break;
        case 0x0283: break;
        case 0x8482: break;
        case 0x8483: break;
        default:
            DbgLog("[Login] Unknown packet 0x%04X (%d bytes)\n", id, (int)pkt.size());
            break;
        }
    }
}

void CLoginMode::ResetAccountLoginHandshake()
{
    m_syncRequestTime = 0;
    m_waitingForPasswordHash = false;
    m_plainAccountLoginSent = false;
}

void CLoginMode::SendExeHashCheck()
{
    if (!ShouldSendExeHash()) {
        return;
    }

    PACKET_CA_EXE_HASHCHECK pkt{};
    pkt.PacketType = ro::net::GetActiveAccountLoginPacketProfile().exeHashCheck;
    std::array<u8, 16> digest{};
    if (!TryResolveExecutableHashPayload(&digest)) {
        DbgLog("[Login] CA_EXE_HASHCHECK skipped: no valid hash payload for serviceType=%d serverType=%d\n",
               static_cast<int>(g_serviceType),
               static_cast<int>(g_serverType));
        return;
    }
    std::memcpy(pkt.HashValue, digest.data(), digest.size());

    if (!CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(&pkt), static_cast<int>(sizeof(pkt)))) {
        DbgLog("[Login] CA_EXE_HASHCHECK send failed\n");
        return;
    }

    char hashText[33] = {};
    FormatDigestHex(digest, hashText, sizeof(hashText));
    DbgLog("[Login] CA_EXE_HASHCHECK sent: hash=%s serviceType=%d serverType=%d\n",
           hashText,
           static_cast<int>(g_serviceType),
           static_cast<int>(g_serverType));
}

void CLoginMode::SendConnectInfoChanged()
{
    PACKET_CA_CONNECT_INFO_CHANGED pkt{};
    pkt.PacketType = ro::net::GetActiveAccountLoginPacketProfile().connectInfoChanged;
    CopyCString(pkt.ID, sizeof(pkt.ID), m_userId);

    if (!CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(&pkt), static_cast<int>(sizeof(pkt)))) {
        DbgLog("[Login] CA_CONNECT_INFO_CHANGED send failed: user='%s'\n", pkt.ID);
        return;
    }

    DbgLog("[Login] CA_CONNECT_INFO_CHANGED sent: user='%s'\n", pkt.ID);
}

void CLoginMode::SendPasswordHashRequest()
{
    PACKET_CA_REQ_HASH pkt{};
    pkt.PacketType = ro::net::GetActiveAccountLoginPacketProfile().requestPasswordHash;
    if (!CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(&pkt), static_cast<int>(sizeof(pkt)))) {
        DbgLog("[Login] CA_REQ_HASH send failed\n");
        return;
    }
    m_syncRequestTime = GetTickCount();
    m_waitingForPasswordHash = true;
    m_plainAccountLoginSent = false;
    DbgLog("[Login] CA_REQ_HASH sent\n");
    SetLoginStatus("Login: waiting for account hash challenge...");
}

void CLoginMode::SendPlainAccountLogin()
{
    switch (ResolveAccountLoginPacketKind()) {
    case AccountLoginPacketKind::LoginChannel:
        SendLoginChannel();
        return;
    case AccountLoginPacketKind::LoginPcBang:
        SendLoginPcBang();
        return;
    case AccountLoginPacketKind::Login4:
        SendLogin4();
        return;
    case AccountLoginPacketKind::Legacy:
    default:
        break;
    }

    PACKET_CA_LOGIN pkt{};
    pkt.PacketType = ro::net::GetActiveAccountLoginPacketProfile().accountLogin;
    pkt.Version = ResolveAccountLoginVersion();
    CopyCString(pkt.ID, sizeof(pkt.ID), m_userId);
    CopyCString(pkt.Passwd, sizeof(pkt.Passwd), m_userPassword);
    pkt.clienttype = ResolveAccountClientType();
    if (!CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(&pkt), static_cast<int>(sizeof(pkt)))) {
        DbgLog("[Login] CA_LOGIN send failed: user='%s' version=%u clienttype=%u (clientinfo=%d)\n",
               pkt.ID,
               pkt.Version,
               static_cast<unsigned int>(pkt.clienttype),
               g_version);
        return;
    }
    m_plainAccountLoginSent = true;
    DbgLog("[Login] CA_LOGIN sent: user='%s' version=%u clienttype=%u (clientinfo=%d)\n",
           pkt.ID,
           pkt.Version,
           static_cast<unsigned int>(pkt.clienttype),
           g_version);
    SetLoginStatus("Login: waiting for account server response...");
}

void CLoginMode::SendLoginPcBang()
{
    PACKET_CA_LOGIN_PCBANG pkt{};
    pkt.PacketType = PACKETID_CA_LOGIN_PCBANG;
    pkt.Version = ResolveAccountLoginVersion();
    CopyCString(pkt.ID, sizeof(pkt.ID), m_userId);
    CopyCString(pkt.Passwd, sizeof(pkt.Passwd), m_userPassword);
    pkt.clienttype = ResolveAccountClientType();
    FillLocalIpString(pkt.IP, sizeof(pkt.IP));
    FillMacString(pkt.Mac, sizeof(pkt.Mac));

    if (!CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(&pkt), static_cast<int>(sizeof(pkt)))) {
        DbgLog("[Login] CA_LOGIN_PCBANG send failed: user='%s' version=%u clienttype=%u ip=%s\n",
               pkt.ID,
               pkt.Version,
               static_cast<unsigned int>(pkt.clienttype),
               pkt.IP);
        return;
    }

    m_plainAccountLoginSent = true;
    DbgLog("[Login] CA_LOGIN_PCBANG sent: user='%s' version=%u clienttype=%u ip=%s\n",
           pkt.ID,
           pkt.Version,
           static_cast<unsigned int>(pkt.clienttype),
           pkt.IP);
    SetLoginStatus("Login: waiting for account server response...");
}

void CLoginMode::SendLogin4()
{
    PACKET_CA_LOGIN4 pkt{};
    pkt.PacketType = PACKETID_CA_LOGIN4;
    pkt.Version = ResolveAccountLoginVersion();
    CopyCString(pkt.ID, sizeof(pkt.ID), m_userId);
    const std::array<u8, 16> digest = ro::cipher::ComputeMd5(m_userPassword, std::strlen(m_userPassword));
    std::memcpy(pkt.PasswdMD5, digest.data(), digest.size());
    pkt.clienttype = ResolveAccountClientType();
    FillMacString(pkt.Mac, sizeof(pkt.Mac));

    if (!CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(&pkt), static_cast<int>(sizeof(pkt)))) {
        DbgLog("[Login] CA_LOGIN4 send failed: user='%s' version=%u clienttype=%u\n",
               pkt.ID,
               pkt.Version,
               static_cast<unsigned int>(pkt.clienttype));
        return;
    }

    m_plainAccountLoginSent = true;
    DbgLog("[Login] CA_LOGIN4 sent: user='%s' version=%u clienttype=%u\n",
           pkt.ID,
           pkt.Version,
           static_cast<unsigned int>(pkt.clienttype));
    SetLoginStatus("Login: waiting for account server response...");
}

void CLoginMode::SendLoginChannel()
{
    PACKET_CA_LOGIN_CHANNEL pkt{};
    pkt.PacketType = ro::net::GetActiveAccountLoginPacketProfile().accountLoginChannel;
    pkt.Version = ResolveAccountLoginVersion();
    CopyCString(pkt.ID, sizeof(pkt.ID), m_userId);
    CopyCString(pkt.Passwd, sizeof(pkt.Passwd), m_userPassword);
    pkt.clienttype = ResolveAccountClientType();
    FillLocalIpString(pkt.IP, sizeof(pkt.IP));
    FillMacString(pkt.Mac, sizeof(pkt.Mac));
    pkt.IsGravity = 0;

    if (!CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(&pkt), static_cast<int>(sizeof(pkt)))) {
        DbgLog("[Login] CA_LOGIN_CHANNEL send failed: user='%s' version=%u clienttype=%u ip=%s\n",
               pkt.ID,
               pkt.Version,
               static_cast<unsigned int>(pkt.clienttype),
               pkt.IP);
        return;
    }

    m_plainAccountLoginSent = true;
    DbgLog("[Login] CA_LOGIN_CHANNEL sent: user='%s' version=%u clienttype=%u ip=%s\n",
           pkt.ID,
           pkt.Version,
           static_cast<unsigned int>(pkt.clienttype),
           pkt.IP);
    SetLoginStatus("Login: waiting for account server response...");
}

void CLoginMode::OnAckHash(const std::vector<u8>& raw)
{
    if (raw.size() < 4) {
        DbgLog("[Login] AC_ACK_HASH too short (%d bytes)\n", static_cast<int>(raw.size()));
        if (!m_plainAccountLoginSent) {
            SendPlainAccountLogin();
        }
        return;
    }

    const u16 packetLength = static_cast<u16>(raw[2]) | (static_cast<u16>(raw[3]) << 8);
    const size_t payloadLength = packetLength >= 4 && packetLength <= raw.size()
        ? static_cast<size_t>(packetLength - 4)
        : raw.size() - 4;

    std::vector<u8> digestInput;
    digestInput.reserve(payloadLength + std::strlen(m_userPassword));
    if (g_passwordEncrypt2 != 0) {
        digestInput.insert(digestInput.end(), m_userPassword, m_userPassword + std::strlen(m_userPassword));
        digestInput.insert(digestInput.end(), raw.begin() + 4, raw.begin() + 4 + payloadLength);
    } else {
        digestInput.insert(digestInput.end(), raw.begin() + 4, raw.begin() + 4 + payloadLength);
        digestInput.insert(digestInput.end(), m_userPassword, m_userPassword + std::strlen(m_userPassword));
    }
    const std::array<u8, 16> digest = ro::cipher::ComputeMd5(digestInput.data(), digestInput.size());

    PACKET_CA_LOGIN_HASH pkt{};
    pkt.PacketType = ro::net::GetActiveAccountLoginPacketProfile().passwordHashLogin;
    pkt.Version = ResolveAccountLoginVersion();
    CopyCString(pkt.ID, sizeof(pkt.ID), m_userId);
    std::memcpy(pkt.PasswdMD5, digest.data(), digest.size());
    pkt.clienttype = ResolveAccountClientType();

    if (!CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(&pkt), static_cast<int>(sizeof(pkt)))) {
        DbgLog("[Login] CA_LOGIN_HASH send failed: user='%s' version=%u challengeBytes=%d clienttype=%u\n",
               pkt.ID,
               pkt.Version,
               static_cast<int>(payloadLength),
               static_cast<unsigned int>(pkt.clienttype));
        return;
    }
    ResetAccountLoginHandshake();
        DbgLog("[Login] CA_LOGIN_HASH sent: user='%s' version=%u challengeBytes=%d clienttype=%u mode=%s\n",
           pkt.ID,
           pkt.Version,
           static_cast<int>(payloadLength),
            static_cast<unsigned int>(pkt.clienttype),
            g_passwordEncrypt2 != 0 ? "password+challenge" : "challenge+password");
    SetLoginStatus("Login: waiting for account server response...");
}

// ---------------------------------------------------------------------------
// 0x0069  AC_ACCEPT_LOGIN  – account server accepted credentials
// Header: type(2)+len(2)+authCode(4)+AID(4)+userLevel(4)+lastIP(4)+lastTime(26)+sex(1) = 47 bytes
// Followed by SERVER_ADDR entries (32 bytes each).
// ---------------------------------------------------------------------------
void CLoginMode::OnAcceptLogin(const std::vector<u8>& raw)
{
    ResetAccountLoginHandshake();
    if (raw.size() < 47) {
        SetLoginStatus("Login error: AC_ACCEPT_LOGIN too short.");
        m_nextSubMode = LoginSubMode_Login;
        return;
    }
    const u8* p = raw.data();

    const u16 packetLen = static_cast<u16>(p[2]) | (static_cast<u16>(p[3]) << 8);
    g_session.m_authCode   = u32(p[4])|(u32(p[5])<<8)|(u32(p[6])<<16)|(u32(p[7])<<24);
    g_session.m_aid        = u32(p[8])|(u32(p[9])<<8)|(u32(p[10])<<16)|(u32(p[11])<<24);
    g_session.m_userLevel  = u32(p[12])|(u32(p[13])<<8)|(u32(p[14])<<16)|(u32(p[15])<<24);

    // Sex byte — some regions add 10; normalise to 0/1.
    u8 rawSex = p[46];
    g_session.m_sex = (rawSex >= 10) ? rawSex - 10 : rawSex;

    // Server list: each SERVER_ADDR is 32 bytes, starts at offset 47.
    m_numServer = 0;
    const int serverListBytes = static_cast<int>(packetLen) - 47;
    if (serverListBytes > 0) {
        const int count = serverListBytes / 32;
        const int toRead = (count < 100) ? count : 100;
        // Direct memcpy: SERVER_ADDR fields are in network byte order in the packet,
        // and Types.h/SERVER_ADDR matches the on-wire layout (32 bytes).
        std::memcpy(m_serverInfo, p + 47, static_cast<size_t>(toRead) * 32);
        m_numServer = toRead;
    }

    DbgLog("[Login] AC_ACCEPT_LOGIN: aid=%u authCode=%u userLevel=%u sex=%d servers=%d\n",
           g_session.m_aid, g_session.m_authCode, g_session.m_userLevel,
           (int)g_session.m_sex, m_numServer);
    SetLoginStatus("Login: account accepted by server.");

    if (m_numServer == 0) {
        SetLoginStatus("Login error: no char servers in login response.");
        m_nextSubMode = LoginSubMode_Login;
        return;
    }

    // Auto-select first char server.
    m_serverSelected = 0;
    m_nextSubMode = LoginSubMode_ConnectChar;
}

// ---------------------------------------------------------------------------
// 0x006A  AC_REFUSE_LOGIN  – account server rejected credentials
// ---------------------------------------------------------------------------
void CLoginMode::OnRefuseLogin(const std::vector<u8>& raw)
{
    ResetAccountLoginHandshake();
    const u8 code = (raw.size() > 2) ? raw[2] : 0;
    const std::string detail = raw.size() >= sizeof(PACKET_AC_REFUSE_LOGIN)
        ? ExtractFixedText(reinterpret_cast<const char*>(raw.data() + 3), sizeof(PACKET_AC_REFUSE_LOGIN::BlockDate))
        : std::string();
    char msg[160];
    switch (code) {
    case 0:  std::snprintf(msg, sizeof(msg), "Login refused: account suspended."); break;
    case 1:  std::snprintf(msg, sizeof(msg), "Login refused: ID not found."); break;
    case 2:  std::snprintf(msg, sizeof(msg), "Login refused: wrong password."); break;
    case 3:  std::snprintf(msg, sizeof(msg), "Login refused: account already logged in."); break;
    case 4:  std::snprintf(msg, sizeof(msg), "Login refused: account not approved."); break;
    case 5:  std::snprintf(msg, sizeof(msg), "Login refused: client version is not accepted by the server."); break;
    case 6:
        if (!detail.empty()) {
            std::snprintf(msg, sizeof(msg), "Login refused: account blocked until %s.", detail.c_str());
        } else {
            std::snprintf(msg, sizeof(msg), "Login refused: account is temporarily blocked.");
        }
        break;
    default: std::snprintf(msg, sizeof(msg), "Login refused (code %d).", (int)code); break;
    }
    DbgLog("[Login] AC_REFUSE_LOGIN: code=%d\n", (int)code);
    SetLoginStatus(msg);
    ShowLoginErrorDialog(msg);
    CRagConnection::instance()->Disconnect();
    m_isConnected = 0;
    m_nextSubMode = LoginSubMode_Login;
}

// ---------------------------------------------------------------------------
// 0x006B  HC_ACCEPT_ENTER  – char server returned character list
// Legacy header:  type(2)+len(2)+extension[20] = 24 bytes before char data.
// 2010 header:    type(2)+len(2)+total(1)+premium_start(1)+premium_end(1)+extension[20] = 27 bytes.
// CHARACTER_INFO stays 108 bytes for the 2010-06-23 client window we are targeting here.
// ---------------------------------------------------------------------------
void CLoginMode::OnAcceptChar(const std::vector<u8>& raw)
{
    if (raw.size() < 4) return;
    const u8* p = raw.data();
    const u16 pktLen = static_cast<u16>(p[2]) | (static_cast<u16>(p[3]) << 8);

    CharListLayout layout = CharListLayout::Legacy;
    size_t headerSize = 0;
    size_t charInfoSize = 0;
    if (!TryResolveCharListLayout(pktLen, &layout, &headerSize, &charInfoSize)) {
        DbgLog("[Login] HC_ACCEPT_ENTER: unsupported packet length=%u for CHARACTER_INFO sizes=%zu/%zu\n",
               pktLen,
               sizeof(CHARACTER_INFO),
               sizeof(LEGACY_CHARACTER_INFO_106));
        SetLoginStatus("Login error: unsupported char-list packet layout.");
        m_numChar = 0;
        m_nextSubMode = LoginSubMode_Login;
        return;
    }

    const int charAreaLen = static_cast<int>(pktLen - static_cast<u16>(headerSize));
    m_numChar = 0;
    if (charAreaLen >= static_cast<int>(charInfoSize)) {
        const int count = charAreaLen / static_cast<int>(charInfoSize);
        const int toRead = (count < static_cast<int>(std::size(m_charInfo)))
            ? count
            : static_cast<int>(std::size(m_charInfo));
        if (static_cast<size_t>(raw.size()) >= headerSize + static_cast<size_t>(toRead) * charInfoSize) {
            std::memset(m_charInfo, 0, sizeof(m_charInfo));
            if (layout == CharListLayout::CompactLegacy) {
                for (int index = 0; index < toRead; ++index) {
                    LEGACY_CHARACTER_INFO_106 legacy{};
                    std::memcpy(&legacy,
                                p + headerSize + static_cast<size_t>(index) * sizeof(legacy),
                                sizeof(legacy));
                    m_charInfo[index] = ExpandLegacyCharacterInfo(legacy);
                }
            } else {
                std::memcpy(m_charInfo,
                            p + headerSize,
                            static_cast<size_t>(toRead) * sizeof(CHARACTER_INFO));
            }
            m_numChar = toRead;
        }
    } else if (charAreaLen == 0) {
        m_numChar = 0;
    }

    if (layout == CharListLayout::Expanded) {
        DbgLog("[Login] HC_ACCEPT_ENTER: numChar=%d (pktLen=%u header=%zu total=%u premium_start=%u premium_end=%u)\n",
               m_numChar,
               pktLen,
               headerSize,
               static_cast<unsigned int>(p[4]),
               static_cast<unsigned int>(p[5]),
               static_cast<unsigned int>(p[6]));
        } else if (layout == CharListLayout::CompactLegacy) {
         DbgLog("[Login] HC_ACCEPT_ENTER: numChar=%d (pktLen=%u header=%zu charSize=%zu layout=legacy_4_106)\n",
             m_numChar,
             pktLen,
             headerSize,
             charInfoSize);
    } else {
         DbgLog("[Login] HC_ACCEPT_ENTER: numChar=%d (pktLen=%u header=%zu charSize=%zu layout=legacy_24_108)\n",
               m_numChar,
               pktLen,
             headerSize,
             charInfoSize);
    }
    SetLoginStatus("Login: character list received from char server.");

    if (m_numChar == 0) {
        SetLoginStatus("Login: no characters on account.");
        m_selectedCharIndex = 0;
        m_nextSubMode = LoginSubMode_CharSelect;
        return;
    }

    DbgLog("[Login] First char: name='%.24s' GID=%u slot=%d\n",
           reinterpret_cast<const char*>(m_charInfo[0].name),
           m_charInfo[0].GID, (int)m_charInfo[0].CharNum);
    DbgLog("[Login] First char appearance: job=%d head=%d weapon=%d shield=%d accBottom=%d accMid=%d accTop=%d headPal=%d bodyPal=%d hairColor=%u\n",
           static_cast<int>(m_charInfo[0].job),
           static_cast<int>(m_charInfo[0].head),
           static_cast<int>(m_charInfo[0].weapon),
           static_cast<int>(m_charInfo[0].shield),
           static_cast<int>(m_charInfo[0].accessory),
           static_cast<int>(m_charInfo[0].accessory3),
           static_cast<int>(m_charInfo[0].accessory2),
           static_cast<int>(m_charInfo[0].headpalette),
           static_cast<int>(m_charInfo[0].bodypalette),
           static_cast<unsigned int>(m_charInfo[0].haircolor));

    for (int index = 0; index < m_numChar; ++index) {
        const int job = static_cast<int>(m_charInfo[index].job);
        const char* const displayJobName = g_session.GetJobDisplayName(job);
        DbgLog("[Login] Warmed char display name index=%d slot=%d job=%d name='%s'\n",
            index,
            static_cast<int>(m_charInfo[index].CharNum),
            job,
            displayJobName ? displayJobName : "");
    }

    m_selectedCharIndex = 0;
    m_nextSubMode = LoginSubMode_CharSelect;
}

// ---------------------------------------------------------------------------
// 0x006C  HC_REFUSE_ENTER  – char server refused entry
// ---------------------------------------------------------------------------
void CLoginMode::OnRefuseChar(const std::vector<u8>& raw)
{
    const u8 code = (raw.size() > 2) ? raw[2] : 0;
    char msg[64];
    std::snprintf(msg, sizeof(msg), "Char server refused entry (code %d).", (int)code);
    DbgLog("[Login] HC_REFUSE_ENTER: code=%d\n", (int)code);
    SetLoginStatus(msg);
    ShowLoginErrorDialog(msg);
    CRagConnection::instance()->Disconnect();
    m_isConnected = 0;
    m_nextSubMode = LoginSubMode_Login;
}

// ---------------------------------------------------------------------------
// 0x006D  HC_ACCEPT_MAKECHAR  – char server accepted the new character
// Layout: type(2) + CHARACTER_INFO(108) = 110 bytes for this client family.
// ---------------------------------------------------------------------------
void CLoginMode::OnAcceptMakeChar(const std::vector<u8>& raw)
{
    if (raw.size() < sizeof(PACKET_HC_ACCEPT_MAKECHAR)) {
        SetLoginStatus("Login error: HC_ACCEPT_MAKECHAR too short.");
        m_nextSubMode = LoginSubMode_CharSelect;
        return;
    }

    CHARACTER_INFO created{};
    std::memcpy(&created, raw.data() + sizeof(u16), sizeof(created));

    if (m_numChar >= 0 && m_numChar < static_cast<int>(std::size(m_charInfo))) {
        m_charInfo[m_numChar] = created;
        ++m_numChar;
    }

    DbgLog("[Login] HC_ACCEPT_MAKECHAR: name='%.24s' gid=%u slot=%d totalChars=%d\n",
           reinterpret_cast<const char*>(created.name), created.GID, (int)created.CharNum, m_numChar);
    SetLoginStatus("Login: character created successfully.");
    m_nextSubMode = LoginSubMode_CharSelect;
}

// ---------------------------------------------------------------------------
// 0x006E  HC_REFUSE_MAKECHAR  – char server refused the new character
// ---------------------------------------------------------------------------
void CLoginMode::OnRefuseMakeChar(const std::vector<u8>& raw)
{
    const u8 code = (raw.size() > 2) ? raw[2] : 0;
    const char* reason = nullptr;
    switch (code) {
    case 0:
        reason = "character name already exists";
        break;
    case 1:
        reason = "age restriction";
        break;
    case 2:
        reason = "character deletion restriction";
        break;
    case 3:
        reason = "invalid character slot";
        break;
    case 11:
        reason = "premium service required";
        break;
    default:
        reason = "character creation denied";
        break;
    }

    char msg[96];
    std::snprintf(msg, sizeof(msg), "Login: character creation failed (%s, code %d).", reason, (int)code);
    DbgLog("[Login] HC_REFUSE_MAKECHAR: code=%d (%s)\n", (int)code, reason);
    SetLoginStatus(msg);
    ShowLoginErrorDialog(msg);
    m_nextSubMode = LoginSubMode_CharSelect;
}

// ---------------------------------------------------------------------------
// 0x006F  HC_ACCEPT_DELETECHAR  – char server deleted the selected character
// ---------------------------------------------------------------------------
void CLoginMode::OnAcceptDeleteChar(const std::vector<u8>& raw)
{
    if (raw.size() < 2) {
        SetLoginStatus("Login error: HC_ACCEPT_DELETECHAR too short.");
        m_nextSubMode = LoginSubMode_CharSelect;
        return;
    }

    const u32 deletedGid = m_pendingDeleteCharGid;
    const int deletedSlot = m_pendingDeleteCharSlot;
    int writeIndex = 0;
    bool removed = false;
    for (int readIndex = 0; readIndex < m_numChar; ++readIndex) {
        const CHARACTER_INFO& info = m_charInfo[readIndex];
        const bool matchesPendingDelete = deletedGid != 0
            ? info.GID == deletedGid
            : static_cast<int>(info.CharNum) == deletedSlot;
        if (matchesPendingDelete && !removed) {
            removed = true;
            continue;
        }
        if (writeIndex != readIndex) {
            m_charInfo[writeIndex] = info;
        }
        ++writeIndex;
    }

    for (int index = writeIndex; index < m_numChar; ++index) {
        std::memset(&m_charInfo[index], 0, sizeof(m_charInfo[index]));
    }
    m_numChar = writeIndex;
    m_selectedCharSlot = deletedSlot >= 0 ? deletedSlot : 0;
    m_selectedCharIndex = -1;
    m_pendingDeleteCharGid = 0;
    m_pendingDeleteCharSlot = -1;
    ClearSelectedCharacterIfMatches(deletedGid);

    DbgLog("[Login] HC_ACCEPT_DELETECHAR: gid=%u slot=%d removed=%d totalChars=%d\n",
           deletedGid,
           deletedSlot,
           removed ? 1 : 0,
           m_numChar);
    SetLoginStatus(removed ? "Login: character deleted successfully." : "Login: delete acknowledged, but the character was not found locally.");
    m_nextSubMode = LoginSubMode_CharSelect;
}

// ---------------------------------------------------------------------------
// 0x0070  HC_REFUSE_DELETECHAR  – char server refused to delete the character
// ---------------------------------------------------------------------------
void CLoginMode::OnRefuseDeleteChar(const std::vector<u8>& raw)
{
    const u8 code = (raw.size() > 2) ? raw[2] : 0;
    char msg[128];
    switch (code) {
    case 0:
        std::snprintf(msg,
                      sizeof(msg),
                      m_emaiAddress[0] != '\0'
                          ? "Login: character deletion failed because the delete key/email was rejected."
                          : "Login: character deletion failed because no delete key/email is set in this rebuild.");
        break;
    case 1:
        std::snprintf(msg, sizeof(msg), "Login: character deletion failed because the slot is no longer valid.");
        break;
    default:
        std::snprintf(msg, sizeof(msg), "Login: character deletion failed (code %u).", static_cast<unsigned int>(code));
        break;
    }

    DbgLog("[Login] HC_REFUSE_DELETECHAR: gid=%u slot=%d code=%u\n",
           m_pendingDeleteCharGid,
           m_pendingDeleteCharSlot,
           static_cast<unsigned int>(code));
    m_pendingDeleteCharGid = 0;
    m_pendingDeleteCharSlot = -1;
    SetLoginStatus(msg);
    ShowLoginErrorDialog(msg);
    m_nextSubMode = LoginSubMode_CharSelect;
}

// ---------------------------------------------------------------------------
// 0x0071  HC_NOTIFY_ZONESVR  – char server providing zone server address
// Layout: type(2)+GID(4)+mapName(16)+IP(4)+port(2) = 28 bytes
// ---------------------------------------------------------------------------
void CLoginMode::OnNotifyZonesvr(const std::vector<u8>& raw)
{
    if (raw.size() < 28) return;
    const u8* p = raw.data();

    g_session.m_gid = u32(p[2])|(u32(p[3])<<8)|(u32(p[4])<<16)|(u32(p[5])<<24);

    // Map name: 16 bytes starting at offset 6, may include .gat extension.
    char mapName[17] = {};
    std::memcpy(mapName, p + 6, 16);
    mapName[16] = '\0';

    // Strip extension (.gat or .rsw) to get the bare map name.
    char* dot = std::strrchr(mapName, '.');
    if (dot) *dot = '\0';
    std::strncpy(g_session.m_curMap, mapName, sizeof(g_session.m_curMap) - 1);
    g_session.m_curMap[sizeof(g_session.m_curMap) - 1] = '\0';

    // Zone server IP (network byte order in packet; inet_ntoa handles it).
    u32 zoneIpRaw = u32(p[22])|(u32(p[23])<<8)|(u32(p[24])<<16)|(u32(p[25])<<24);
    in_addr za{};
    za.s_addr = zoneIpRaw;
    std::strncpy(m_zoneAddr, inet_ntoa(za), sizeof(m_zoneAddr) - 1);

    // RO packets store this field little-endian on the wire, which lands in host order here.
    const u16 portRaw = static_cast<u16>(p[26]) | (static_cast<u16>(p[27]) << 8);
    m_zonePort = static_cast<int>(portRaw);

    DbgLog("[Login] HC_NOTIFY_ZONESVR: gid=%u map='%s' zone=%s:%d\n",
           g_session.m_gid, g_session.m_curMap, m_zoneAddr, m_zonePort);

    CRagConnection::instance()->Disconnect();
    m_isConnected = 0;
    m_nextSubMode = LoginSubMode_ZoneConnect;
}

// ---------------------------------------------------------------------------
// 0x0073 / 0x02EB  ZC_ACCEPT_ENTER / ZC_ACCEPT_ENTER2
// Shared core layout: type(2)+serverTick(4)+posX_Y_dir(3 packed bytes)
// ---------------------------------------------------------------------------
void CLoginMode::OnZcAcceptEnter(const std::vector<u8>& raw)
{
    if (raw.size() < 9) return;
    const u8* p = raw.data();
    const u16 packetId = static_cast<u16>(p[0]) | (static_cast<u16>(p[1]) << 8);

    const u32 serverTick = u32(p[2])|(u32(p[3])<<8)|(u32(p[4])<<16)|(u32(p[5])<<24);
    g_session.SetServerTime(serverTick);

    // Position is packed into 3 bytes (10-bit X, 10-bit Y, 4-bit Dir).
    const int startX   = (int(p[6]) << 2)          | (p[7] >> 6);
    const int startY   = ((p[7] & 0x3F) << 4)      | (p[8] >> 4);
    const int startDir = p[8] & 0x0F;
    g_session.SetPlayerPosDir(startX, startY, startDir);

    char curMap[40];
    std::snprintf(curMap, sizeof(curMap), "%s.rsw", g_session.m_curMap);

        DbgLog("[Login] ZC_ACCEPT_ENTER: opcode=0x%04X map=%s x=%d y=%d dir=%d tick=%u -> switching to GameMode\n",
            packetId, curMap, startX, startY, startDir, serverTick);

    g_modeMgr.Switch(1, curMap);
}

// ---------------------------------------------------------------------------
// 0x0081  SC_NOTIFY_BAN  – server disconnecting us
// ---------------------------------------------------------------------------
void CLoginMode::OnDisconnectMsg(const std::vector<u8>& raw)
{
    ResetAccountLoginHandshake();
    const u8 code = (raw.size() > 2) ? raw[2] : 0;
    char msg[64];
    std::snprintf(msg, sizeof(msg), "Disconnected by server (code %d).", (int)code);
    DbgLog("[Login] SC_NOTIFY_BAN: code=%d\n", (int)code);
    SetLoginStatus(msg);
    ShowLoginErrorDialog(msg);
    CRagConnection::instance()->Disconnect();
    m_isConnected = 0;
    m_nextSubMode = LoginSubMode_Login;
}

void CLoginMode::OnNotifyError(const std::vector<u8>& raw)
{
    ResetAccountLoginHandshake();
    const std::string message = ExtractPacketText(raw, 4);
    const char* dialogText = message.empty() ? "Login failed: server returned an empty error message." : message.c_str();

    DbgLog("[Login] AC_NOTIFY_ERROR: len=%d message='%s'\n",
           static_cast<int>(raw.size()),
           dialogText);

    SetLoginStatus(dialogText);
    ShowLoginErrorDialog(dialogText);
    CRagConnection::instance()->Disconnect();
    m_isConnected = 0;
    m_nextSubMode = LoginSubMode_Login;
}
