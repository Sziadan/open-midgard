#include "Session.h"
#include "../core/ClientInfoLocale.h"
#include "../core/Xml.h"
#include "DebugLog.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <filesystem>
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

const char* GetJobToken(int job)
{
    for (const JobTokenEntry& entry : kJobTokens) {
        if (entry.job == job) {
            return entry.token;
        }
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

} // namespace

CSession::CSession() : m_aid(0), m_authCode(0), m_sex(0), m_isEffectOn(true), m_isMinEffect(false),
    m_charServerPort(0), m_pendingReturnToCharSelect(0),
    m_playerPosX(0), m_playerPosY(0), m_playerDir(0), m_playerJob(0), m_playerHead(0), m_playerBodyPalette(0),
    m_playerHeadPalette(0), m_playerWeapon(0), m_playerShield(0), m_playerAccessory(0), m_playerAccessory2(0),
    m_playerAccessory3(0), m_serverTime(0), m_hasSelectedCharacterInfo(false), m_baseExpValue(0),
    m_nextBaseExpValue(0), m_jobExpValue(0), m_nextJobExpValue(0), m_hasBaseExpValue(false),
    m_hasNextBaseExpValue(false), m_hasJobExpValue(false), m_hasNextJobExpValue(false),
    m_accessoryNameTableLoaded(false)
{
    std::memset(m_userId, 0, sizeof(m_userId));
    std::memset(m_userPassword, 0, sizeof(m_userPassword));
    std::memset(m_charServerAddr, 0, sizeof(m_charServerAddr));
    std::memset(m_curMap, 0, sizeof(m_curMap));
    std::memset(m_playerName, 0, sizeof(m_playerName));
    std::memset(&m_selectedCharacterInfo, 0, sizeof(m_selectedCharacterInfo));
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

void CSession::SetServerTime(u32 time) 
{ 
    m_serverTime = timeGetTime() - time;
}

u32 CSession::GetServerTime() const
{
    return timeGetTime() - m_serverTime;
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

void CSession::ClearSkillItems()
{
    m_skillItems.clear();
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
    int weapon = 0;
    int shield = 0;
    int accessoryBottom = 0;
    int accessoryTop = 0;
    int accessoryMid = 0;

    for (const ITEM_INFO& item : m_inventoryItems) {
        if (item.m_wearLocation == 0) {
            continue;
        }

        const unsigned int itemId = item.GetItemId();
        const int viewId = g_ttemmgr.GetViewId(itemId);

        if ((item.m_wearLocation & 2) != 0) {
            weapon = viewId > 0 ? viewId : static_cast<int>(itemId);
        }
        if ((item.m_wearLocation & 32) != 0) {
            shield = viewId > 0 ? viewId : static_cast<int>(itemId);
        }
        if ((item.m_wearLocation & 1) != 0) {
            accessoryBottom = viewId;
        }
        if ((item.m_wearLocation & 256) != 0) {
            accessoryTop = viewId;
        }
        if ((item.m_wearLocation & 512) != 0) {
            accessoryMid = viewId;
        }
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

const std::list<PLAYER_SKILL_INFO>& CSession::GetSkillItems() const
{
    return m_skillItems;
}

int CSession::GetPlayerSkillPointCount() const
{
    return m_hasSelectedCharacterInfo ? static_cast<int>(m_selectedCharacterInfo.jobpoint) : 0;
}

const char* CSession::GetPlayerName() const
{
    return m_playerName;
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
    const int normalizedJob = NormalizeJob(job);
    const char* sexToken = GetSexToken(sex);
    const char* jobToken = GetJobToken(normalizedJob);
    std::sprintf(buf, "%s%s\\%s\\%s_%s.act", kHumanSpriteRoot, kBodyDir, sexToken, jobToken, sexToken);
    return buf;
}

char* CSession::GetJobSprName(int job, int sex, char* buf)
{
    const int normalizedJob = NormalizeJob(job);
    const char* sexToken = GetSexToken(sex);
    const char* jobToken = GetJobToken(normalizedJob);
    std::sprintf(buf, "%s%s\\%s\\%s_%s.spr", kHumanSpriteRoot, kBodyDir, sexToken, jobToken, sexToken);
    return buf;
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
    std::sprintf(buf, "%s%s\\%s_%s.act", kAccessorySpriteRoot, sexToken, sexToken, resourceName.c_str());
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
    std::sprintf(buf, "%s%s\\%s_%s.spr", kAccessorySpriteRoot, sexToken, sexToken, resourceName.c_str());
    return buf;
}

char* CSession::GetImfName(int job, int head, int sex, char* buf)
{
    (void)head;
    const int normalizedJob = NormalizeJob(job);
    const char* sexToken = GetSexToken(sex);
    const char* jobToken = GetJobCompositionToken(normalizedJob);
    std::sprintf(buf, "%s%s_%s.imf", kImfRoot, jobToken, sexToken);
    return buf;
}

char* CSession::GetBodyPaletteName(int job, int sex, int palNum, char* buf)
{
    const int normalizedJob = NormalizeJob(job);
    const char* sexToken = GetSexToken(sex);
    const char* jobToken = GetJobCompositionToken(normalizedJob);
    std::sprintf(buf, "%s%s_%s_%d.pal", kBodyPaletteRoot, jobToken, sexToken, palNum);
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
