#include "Session.h"
#include "../core/ClientInfoLocale.h"
#include "../core/Xml.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <mmsystem.h>

namespace {

constexpr const char* kHumanSpriteRoot = "data\\sprite\\\xC0\xCE\xB0\xA3\xC1\xB7\\";
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

} // namespace

CSession::CSession() : m_aid(0), m_authCode(0), m_sex(0), m_charServerPort(0), m_pendingReturnToCharSelect(0),
    m_playerPosX(0), m_playerPosY(0), m_playerDir(0), m_playerJob(0), m_playerHead(0), m_playerBodyPalette(0),
    m_playerHeadPalette(0), m_playerWeapon(0), m_playerShield(0), m_playerAccessory(0), m_playerAccessory2(0),
    m_playerAccessory3(0), m_serverTime(0), m_hasSelectedCharacterInfo(false), m_baseExpValue(0),
    m_nextBaseExpValue(0), m_jobExpValue(0), m_nextJobExpValue(0), m_hasBaseExpValue(false),
    m_hasNextBaseExpValue(false), m_hasJobExpValue(false), m_hasNextJobExpValue(false)
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

const std::list<ITEM_INFO>& CSession::GetInventoryItems() const
{
    return m_inventoryItems;
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
