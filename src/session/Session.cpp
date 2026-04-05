#include "Session.h"
#include "../core/ClientInfoLocale.h"
#include "../core/Xml.h"
#include "DebugLog.h"

#include <algorithm>
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

int ClampShortcutPageIndex(int page)
{
    return (std::max)(0, (std::min)(page, kShortcutPageCount - 1));
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
    m_accessoryNameTableLoaded(false), m_GaugePacket(0)
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

void CSession::ClearSkillItems()
{
    m_skillItems.clear();
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

unsigned int CSession::GetEquippedLeftHandWeaponItemId() const
{
    for (const ITEM_INFO& item : m_inventoryItems) {
        if (item.m_wearLocation == 0) {
            continue;
        }
        if ((item.m_wearLocation & 32) != 0) {
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
    int weaponType = weaponItemId;
    if (weaponItemId >= 31) {
        if (job == 12 || job == 4013 || job == 4035) {
            const int secondaryWeapon = static_cast<int>((static_cast<unsigned int>(weaponItemId) >> 16) & 0xFFFFu);
            if (GetWeaponTypeByItemId(secondaryWeapon) <= 0) {
                weaponType = GetWeaponTypeByItemId(weaponItemId & 0xFFFF);
            } else {
                weaponType = MakeWeaponTypeByItemId(weaponItemId & 0xFFFF, secondaryWeapon);
            }
        } else {
            weaponType = GetWeaponTypeByItemId(weaponItemId & 0xFFFF);
        }
    }

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
    int weaponType = weaponItemId;
    if (weaponItemId >= 31) {
        if (job == 12 || job == 4013 || job == 4035) {
            const int secondaryWeapon = static_cast<int>((static_cast<unsigned int>(weaponItemId) >> 16) & 0xFFFFu);
            if (GetWeaponTypeByItemId(secondaryWeapon) <= 0) {
                weaponType = GetWeaponTypeByItemId(weaponItemId & 0xFFFF);
            } else {
                weaponType = MakeWeaponTypeByItemId(weaponItemId & 0xFFFF, secondaryWeapon);
            }
        } else {
            weaponType = GetWeaponTypeByItemId(weaponItemId & 0xFFFF);
        }
    }

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
