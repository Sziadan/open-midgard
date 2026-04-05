#pragma once
#include <array>
#include <string>
#include <list>
#include <vector>
#include "Types.h"
#include "item/Item.h"
#include "skill/Skill.h"

struct accountInfo {
    std::string display;
    std::string desc;
    std::string balloon;
    std::string address;
    std::string port;
};

constexpr int JT_G_MASTER = 20002;

struct PLAYER_SKILL_INFO {
    int m_isValid = 0;
    int SKID = 0;
    int type = 0;
    int level = 0;
    int spcost = 0;
    int upgradable = 0;
    int attackRange = 0;
    int skillPos = 0;
    int skillMaxLv = 0;
    std::string skillIdName;
    std::string skillName;
    std::vector<std::string> descriptionLines;
    std::vector<int> needSkillList;
};

enum class NpcShopMode : int {
    None = 0,
    Buy = 1,
    Sell = 2,
};

struct NPC_SHOP_ROW {
    ITEM_INFO itemInfo;
    unsigned int sourceItemIndex = 0;
    int price = 0;
    int secondaryPrice = 0;
    int availableCount = 0;
};

struct NPC_SHOP_DEAL_ROW {
    ITEM_INFO itemInfo;
    unsigned int sourceItemIndex = 0;
    int unitPrice = 0;
    int quantity = 0;
};

struct SHORTCUT_SLOT {
    unsigned char isSkill = 0;
    unsigned int id = 0;
    unsigned short count = 0;
};

constexpr int kShortcutSlotsPerPage = 9;
constexpr int kShortcutPageCount = 3;
constexpr int kShortcutSlotCount = kShortcutSlotsPerPage * kShortcutPageCount;

class CSession
{
public:
    CSession();
    ~CSession();

    bool InitAccountInfo();
    
    std::vector<accountInfo> m_accountInfo;
    char m_userId[24];
    char m_userPassword[24];
    u32  m_aid;
    u32  m_authCode;
    u32  m_userLevel;
    u32  m_gid;
    u8   m_sex;
    bool m_isEffectOn;
    bool m_isMinEffect;
    char m_charServerAddr[64];
    int  m_charServerPort;
    int  m_pendingReturnToCharSelect;
    
    char m_curMap[24];
    int m_playerPosX;
    int m_playerPosY;
    int m_playerDir;
    int m_playerJob;
    int m_playerHead;
    int m_playerBodyPalette;
    int m_playerHeadPalette;
    int m_playerWeapon;
    int m_playerShield;
    int m_playerAccessory;
    int m_playerAccessory2;
    int m_playerAccessory3;
    char m_playerName[25];
    int m_plusStr = 0;
    int m_plusAgi = 0;
    int m_plusVit = 0;
    int m_plusInt = 0;
    int m_plusDex = 0;
    int m_plusLuk = 0;
    int m_standardStr = 2;
    int m_standardAgi = 2;
    int m_standardVit = 2;
    int m_standardInt = 2;
    int m_standardDex = 2;
    int m_standardLuk = 2;
    int m_attPower = 0;
    int m_refiningPower = 0;
    int m_maxMatkPower = 0;
    int m_minMatkPower = 0;
    int m_itemDefPower = 0;
    int m_plusDefPower = 0;
    int m_mdefPower = 0;
    int m_plusMdefPower = 0;
    int m_hitSuccessValue = 0;
    int m_avoidSuccessValue = 0;
    int m_plusAvoidSuccessValue = 0;
    int m_criticalSuccessValue = 0;
    int m_aspd = 0;
    int m_plusAspd = 0;
    u32 m_shopNpcId = 0;
    NpcShopMode m_shopMode = NpcShopMode::None;
    int m_shopSelectedSourceRow = -1;
    int m_shopSelectedDealRow = -1;
    int m_shopDealTotal = 0;
    std::vector<NPC_SHOP_ROW> m_shopRows;
    std::vector<NPC_SHOP_DEAL_ROW> m_shopDealRows;
    int m_shortcutPage = 0;
    std::array<SHORTCUT_SLOT, kShortcutSlotCount> m_shortcutSlots{};
    int m_GaugePacket = 0;
    
    void SetServerTime(u32 time);
    u32 GetServerTime() const;
    void SetPlayerPosDir(int x, int y, int dir);
    void SetSelectedCharacterAppearance(const CHARACTER_INFO& info);
    const CHARACTER_INFO* GetSelectedCharacterInfo() const;
    CHARACTER_INFO* GetMutableSelectedCharacterInfo();
    void SetBaseExpValue(int value);
    void SetNextBaseExpValue(int value);
    void SetJobExpValue(int value);
    void SetNextJobExpValue(int value);
    bool TryGetBaseExpPercent(int* outPercent) const;
    bool TryGetJobExpPercent(int* outPercent) const;
    void ClearInventoryItems();
    void ClearEquipmentInventoryItems();
    void ClearSkillItems();
    void ClearHomunSkillItems();
    void ClearMercSkillItems();
    void SetInventoryItem(const ITEM_INFO& itemInfo);
    void AddInventoryItem(const ITEM_INFO& itemInfo);
    void SetSkillItem(const PLAYER_SKILL_INFO& skillInfo);
    void SetHomunSkillItem(const PLAYER_SKILL_INFO& skillInfo);
    void SetMercSkillItem(const PLAYER_SKILL_INFO& skillInfo);
    void RemoveInventoryItem(unsigned int itemIndex, int amount);
    bool SetInventoryItemWearLocation(unsigned int itemIndex, int wearLocation);
    void ClearInventoryWearLocationMask(int wearMask, unsigned int exceptItemIndex = 0);
    void RebuildPlayerEquipmentAppearanceFromInventory();
    const std::list<ITEM_INFO>& GetInventoryItems() const;
    const ITEM_INFO* GetInventoryItemByIndex(unsigned int itemIndex) const;
    const ITEM_INFO* GetInventoryItemByItemId(unsigned int itemId) const;
    const std::list<PLAYER_SKILL_INFO>& GetSkillItems() const;
    const std::list<PLAYER_SKILL_INFO>& GetHomunSkillItems() const;
    const std::list<PLAYER_SKILL_INFO>& GetMercSkillItems() const;
    const PLAYER_SKILL_INFO* GetSkillItemBySkillId(int skillId) const;
    const PLAYER_SKILL_INFO* GetHomunSkillItemBySkillId(int skillId) const;
    const PLAYER_SKILL_INFO* GetMercSkillItemBySkillId(int skillId) const;
    void ClearNpcShopState();
    void SetNpcShopChoice(u32 npcId);
    void SetNpcShopRows(u32 npcId, NpcShopMode mode, const std::vector<NPC_SHOP_ROW>& rows);
    bool AdjustNpcShopDealBySourceRow(size_t sourceRowIndex, int deltaQuantity);
    bool AdjustNpcShopDealByDealRow(size_t dealRowIndex, int deltaQuantity);
    int GetNpcShopUnitPrice(const NPC_SHOP_ROW& row) const;
    void ClearShortcutSlots();
    int GetShortcutPage() const;
    void SetShortcutPage(int page);
    int GetShortcutSlotAbsoluteIndex(int visibleSlot) const;
    const SHORTCUT_SLOT* GetShortcutSlotByAbsoluteIndex(int absoluteIndex) const;
    const SHORTCUT_SLOT* GetShortcutSlotByVisibleIndex(int visibleSlot) const;
    bool SetShortcutSlotByAbsoluteIndex(int absoluteIndex, unsigned char isSkill, unsigned int id, unsigned short count);
    bool SetShortcutSlotByVisibleIndex(int visibleSlot, unsigned char isSkill, unsigned int id, unsigned short count);
    bool ClearShortcutSlotByAbsoluteIndex(int absoluteIndex);
    bool ClearShortcutSlotByVisibleIndex(int visibleSlot);
    int FindShortcutSlotByItemId(unsigned int itemId) const;
    int FindShortcutSlotBySkillId(int skillId) const;
    int GetPlayerSkillPointCount() const;
    int GetWeaponTypeByItemId(int itemId) const;
    int MakeWeaponTypeByItemId(int primaryWeaponItemId, int secondaryWeaponItemId) const;
    bool IsSecondAttack(int job, int sex, int weaponItemId) const;
    float GetPCAttackMotion(int job, int sex, int weaponItemId, int isSecondAttack) const;
    unsigned int GetEquippedLeftHandWeaponItemId() const;
    unsigned int GetEquippedRightHandWeaponItemId() const;
    const char* GetPlayerName() const;
    const char* GetJobName(int job) const;
    const char* GetAttrWaveName(int attr) const;
    const char* GetJobHitWaveName(int job) const;
    const char* GetWeaponHitWaveName(int weapon) const;
    int GetSex() const;
    char* GetJobActName(int job, int sex, char* buf);
    char* GetJobSprName(int job, int sex, char* buf);
    char* GetHeadActName(int job, int* head, int sex, char* buf);
    char* GetHeadSprName(int job, int* head, int sex, char* buf);
    char* GetAccessoryActName(int job, int* head, int sex, int accessory, char* buf);
    char* GetAccessorySprName(int job, int* head, int sex, int accessory, char* buf);
    char* GetImfName(int job, int head, int sex, char* buf);
    char* GetBodyPaletteName(int job, int sex, int palNum, char* buf);
    char* GetHeadPaletteName(int head, int job, int sex, int palNum, char* buf);
    
private:
    void EnsureAccessoryNameTableLoaded();
    void InitJobHitWaveName();
    void InitWeaponHitWaveName();
    int NormalizeJob(int job) const;
    u32 m_serverTime;
    CHARACTER_INFO m_selectedCharacterInfo;
    bool m_hasSelectedCharacterInfo;
    int m_baseExpValue;
    int m_nextBaseExpValue;
    int m_jobExpValue;
    int m_nextJobExpValue;
    bool m_hasBaseExpValue;
    bool m_hasNextBaseExpValue;
    bool m_hasJobExpValue;
    bool m_hasNextJobExpValue;
    std::list<ITEM_INFO> m_inventoryItems;
    std::list<PLAYER_SKILL_INFO> m_skillItems;
    std::list<PLAYER_SKILL_INFO> m_homunSkillItems;
    std::list<PLAYER_SKILL_INFO> m_mercSkillItems;
    bool m_accessoryNameTableLoaded;
    std::vector<std::string> m_accessoryNameTable;
    std::vector<std::string> m_jobHitWaveNameTable;
    std::vector<std::string> m_weaponHitWaveNameTable;
};

extern CSession g_session;
