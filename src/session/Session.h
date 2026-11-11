#pragma once
#include "core/Vitals.h"
#include "core/GuildState.h"
#include "core/PetState.h"
#include "core/SourceMapNames.h"
#include <algorithm>
#include <array>
#include <deque>
#include <mutex>
#include <unordered_map>
#include <string>
#include <list>
#include <vector>
#include "Types.h"
#include "core/Xml.h"
#include "item/Item.h"
#include "skill/Skill.h"

class CGameActor;

struct accountInfo {
    std::string display;
    std::string desc;
    std::string balloon;
    std::string address;
    std::string port;
};

// Local membership is session state, not part of the current map's actor cache.
struct LocalGuildInfo {
    u32 characterId=0;
    u32 guildId=0;
    u32 emblemVersion=0;
    std::string name;
    std::string title;
    bool isMaster=false;
    u32 permissions=0;
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

struct FRIEND_INFO {
    int isValid = 0;
    u32 AID = 0;
    u32 GID = 0;
    std::string characterName;
    std::string mapName;
    int role = 0;
    int state = 0;
    u32 color = 0;
    long long partyHp = 0;
    long long partyMaxHp = 0;
    s16 partyX = 0;
    s16 partyY = 0;
    bool partyPositionValid = false;
};

inline bool ApplyPartyMapPosition(std::list<FRIEND_INFO>& members,u32 aid,int x,int y)
{
    if(aid==0)return false;
    auto member=std::find_if(members.begin(),members.end(),[&](const FRIEND_INFO& item){return item.AID==aid;});
    if(member==members.end())return false;
    if(x==-1&&y==-1){member->partyPositionValid=false;member->partyX=0;member->partyY=0;return true;}
    if(x<0||y<0||x>0x7fff||y>0x7fff)return false;
    member->partyX=static_cast<s16>(x);member->partyY=static_cast<s16>(y);member->partyPositionValid=true;
    return true;
}

inline void PreservePartyMapPosition(const FRIEND_INFO& existing,FRIEND_INFO& updated)
{
    updated.partyPositionValid=false;
    updated.partyX=0;
    updated.partyY=0;
    if(existing.AID!=0&&existing.AID==updated.AID&&existing.mapName==updated.mapName
        &&existing.state==0&&updated.state==0&&existing.partyPositionValid){
        updated.partyX=existing.partyX;updated.partyY=existing.partyY;updated.partyPositionValid=true;
    }
}

enum class NpcShopMode : int {
    None = 0,
    Buy = 1,
    Sell = 2,
};

// Selected C6/server item-type semantics. Pet eggs and shadow gear normally
// project to type4/5 on this wire; include their raw types defensively as well.
inline bool NpcShopItemIsStackable(int itemType)
{
    return itemType != 4 && itemType != 5 && itemType != 7 && itemType != 8 && itemType != 12;
}

struct NPC_SHOP_ROW {
    ITEM_INFO itemInfo;
    unsigned int sourceItemIndex = 0;
    u32 price = 0;
    u32 secondaryPrice = 0;
    int availableCount = 0;
};

struct NPC_SHOP_DEAL_ROW {
    ITEM_INFO itemInfo;
    unsigned int sourceItemIndex = 0;
    u32 unitPrice = 0;
    int quantity = 0;
};

struct SHORTCUT_SLOT {
    unsigned char isSkill = 0;
    unsigned int id = 0;
    unsigned short count = 0;
};

struct ACTIVE_STATUS_ICON {
    int statusType = 0;
    bool hasTimer = false;
    u32 expireServerTime = 0;
};

struct SessionFogParameter {
    float start = 0.0f;
    float end = 1.0f;
    u32 color = 0;
    float density = 0.0f;
};

constexpr int kShortcutSlotsPerPage = 9;
constexpr int kShortcutPageCount = 3;
constexpr int kShortcutSlotCount = kShortcutSlotsPerPage * kShortcutPageCount;

struct CompanionCombatStatus {
    int attack = 0, magicAttack = 0, hit = 0, critical = 0;
    int defense = 0, magicDefense = 0, flee = 0, attackSpeed = 0;
    int hp = 0, maxHp = 0, sp = 0, maxSp = 0, attackRange = 0;
};

struct HomunculusStatus {
    std::string name;
    CompanionCombatStatus combat;
    int level = 0, hunger = 0, intimacy = 0, skillPoints = 0;
    int lastWarnedHunger = 0;
    int job = -1, stateId = 0;
    bool alive = false;
    u8 nameModified = 0;
    u32 accessory = 0;
    int experience = 0, nextExperience = 0;
};

struct MercenaryStatus {
    std::string name;
    CompanionCombatStatus combat;
    int level = 0, faith = 0;
    int job = -1;
    bool alive = false;
    int expiresAt = 0, calls = 0, kills = 0;
};

class CSession
{
public:
    CSession();
    ~CSession();
    bool m_isShowWhisperWnd=true,m_isShowWhisperWnd_Friend=true,m_isPlayWhisperOpenSound=true;
    bool m_onAutoMSG=false,m_isSaveChat=false;
    std::string m_strAutoMSG;
    std::vector<std::string> m_exNameList;
    void LoadWhisperOptions();
    bool SaveWhisperOptions() const;
    bool InitMapNameTable();
    std::string GetMapName(const char* rswName) const;
    ro::SourceMapNames m_mapNameTable;
    bool IsExName(const char* name) const;
    bool IsGuildMember(const char* name) const;
    bool IsCheatName(const char* name) const;
    bool IsCheatGuildName(const char* name) const;

    bool InitAccountInfo();
    
    std::vector<accountInfo> m_accountInfo;
    char m_userId[24];
    char m_userPassword[24];
    u32  m_aid;
    u32  m_authCode;
    u32  m_userLevel;
    u32  m_gid;
    // Source companion identity owners. PK28/29 receive producers remain open;
    // capture picking must compare these IDs, rather than infer ownership.
    u32 m_homunGID=0,m_merGID=0;
    HomunculusStatus m_homunStatus;
    MercenaryStatus m_merStatus;
    // Selected 0x00B0 decodes signed wire dword/10; source initialization zeros both.
    int m_weight=0,m_maxWeight=0;
    u8   m_sex;
    bool m_isEffectOn;
    bool m_isMinEffect;
    bool m_isMinEffectOld=false;
    int m_talkType=0;
    bool m_isBattleChat=false;
    std::vector<std::pair<int,int>> m_cmdOnOffSet;
    int m_isBattle=0,m_isNoTrade=0,m_isNoShift=0,m_isNoCtrl=0;
    int m_isWindow=0,m_bMakeMissEffect=1,m_isQuickSpell=0,m_isQuickSpell2=0;
    int m_isShowNameTypeOld=1,m_isLogInOut=1,m_isShopping=0;
    void LoadSourceCommandOptions();
    bool SaveSourceCommandOptions() const;
    void InitializeSourceCommandFlags(bool bgmPaused,int soundOn,bool tipPresent);
    void SetTextType(u8 shorten,u8 bold);

    int m_isSkillFail = 1;
    int m_isAura = 1;
    bool m_fogOn;
    char m_charServerAddr[64];
    int  m_charServerPort;
    int  m_pendingReturnToCharSelect;
    
    char m_curMap[24];
    int m_playerPosX;
    int m_playerPosY;
    int m_playerDir;
    // Walk delay in ms per cell. Zc_Par_Change type 0 (SP_SPEED). Lower is faster.
    u32 m_speed = 150;
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
    std::list<u32> m_exAidList;
    int m_isNoTalkMsg = 0, m_isNoTalkMsg2 = 0;
    // Ref Session initialization: the thirteen source battle-message filters start enabled.
    void LoadBattleMessageOptions();
    bool SaveBattleMessageOptions() const;
    std::array<bool, 13> m_battleMsgOnOffSet = {
        true, true, true, true, true, true, true, true, true, true, true, true, true
    };
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
    bool m_shopChoicePending = false;
    int m_shopSelectedSourceRow = -1;
    int m_shopSelectedDealRow = -1;
    unsigned long long m_shopDealTotal = 0;
    std::vector<NPC_SHOP_ROW> m_shopRows;
    std::vector<NPC_SHOP_DEAL_ROW> m_shopDealRows;
    std::list<FRIEND_INFO> m_partyList;
    std::list<FRIEND_INFO> m_friendList;
    std::string m_partyName;
    LocalGuildInfo m_localGuild;
    ro::guild::WindowState m_guildWindow;
    ro::pet::Status m_petStatus;
    bool m_amIPartyMaster = false;
    bool m_partyExpShare = false;
    bool m_itemDivType = false;
    bool m_itemCollectType = false;
    bool m_storageOpen = false;
    int m_storageCurrentCount = 0;
    int m_storageMaxCount = 0;
    int m_shortcutPage = 0;
    std::array<SHORTCUT_SLOT, kShortcutSlotCount> m_shortcutSlots{};
    int m_GaugePacket = 0;
    
    void SetServerTime(u32 time);
    void UpdateServerTime(u32 time);
    u32 GetServerTime() const;
    void RecalcAveragePingTime(u32 pingTime);
    u32 GetAveragePingTime() const;
    bool GetFogParameter(const char* rswName, SessionFogParameter* outParameter);
    void SetPlayerPosDir(int x, int y, int dir);
    void SetSelectedCharacterAppearance(const CHARACTER_INFO& info);
    const CHARACTER_INFO* GetSelectedCharacterInfo() const;
    CHARACTER_INFO* GetMutableSelectedCharacterInfo();
    const CharacterVitals& GetCharacterVitals() const { return m_characterVitals; }
    bool UpdateCharacterVital(u32 type, u32 value) { return m_characterVitals.Update(type, value); }
    void SetBaseExpValue(u32 value);
    void SetNextBaseExpValue(u32 value);
    void SetJobExpValue(u32 value);
    void SetNextJobExpValue(u32 value);
    bool TryGetBaseExpPercent(int* outPercent) const;
    bool TryGetJobExpPercent(int* outPercent) const;
    void ClearInventoryItems();
    void ClearNormalInventoryItems();
    void ClearEquipmentInventoryItems();
    void ClearStorageItems();
    void ClearSkillItems();
    void ClearGuildSkillItems();
    void InvalidateUI();
    void ClearHomunSkillItems();
    void ClearMercSkillItems();
    void SetInventoryItem(const ITEM_INFO& itemInfo);
    void AddInventoryItem(const ITEM_INFO& itemInfo);
    void OpenStorage(int currentCount, int maxCount);
    void CloseStorage();
    void SetStorageItem(const ITEM_INFO& itemInfo);
    void AddStorageItem(const ITEM_INFO& itemInfo);
    void SetSkillItem(const PLAYER_SKILL_INFO& skillInfo);
    bool AddTemporarySkillItem(const PLAYER_SKILL_INFO& skillInfo);
    bool SetGuildSkillItem(const PLAYER_SKILL_INFO& skillInfo);
    void SetHomunSkillItem(const PLAYER_SKILL_INFO& skillInfo);
    void SetMercSkillItem(const PLAYER_SKILL_INFO& skillInfo);
    void RemoveInventoryItem(unsigned int itemIndex, int amount);
    void RemoveStorageItem(unsigned int itemIndex, int amount);
    bool SetInventoryItemWearLocation(unsigned int itemIndex, int wearLocation);
    void ClearInventoryWearLocationMask(int wearMask, unsigned int exceptItemIndex = 0);
    void RebuildPlayerEquipmentAppearanceFromInventory();
    const std::list<ITEM_INFO>& GetInventoryItems() const;
    const ITEM_INFO* GetInventoryItemByIndex(unsigned int itemIndex) const;
    const ITEM_INFO* GetInventoryItemByItemId(unsigned int itemId) const;
    const std::list<ITEM_INFO>& GetStorageItems() const;
    const ITEM_INFO* GetStorageItemByIndex(unsigned int itemIndex) const;
    bool IsStorageOpen() const;
    int GetStorageCurrentCount() const;
    int GetStorageMaxCount() const;
    const std::list<PLAYER_SKILL_INFO>& GetSkillItems() const;
    const std::list<PLAYER_SKILL_INFO>& GetTemporarySkillItems() const;
    const std::list<PLAYER_SKILL_INFO>& GetGuildSkillItems() const;
    int GetGuildSkillPoint() const;
    void SetGuildSkillPoint(int points);
    const std::list<PLAYER_SKILL_INFO>& GetHomunSkillItems() const;
    const std::list<PLAYER_SKILL_INFO>& GetMercSkillItems() const;
    const PLAYER_SKILL_INFO* GetSkillItemBySkillId(int skillId) const;
    const PLAYER_SKILL_INFO* GetTemporarySkillItemBySkillId(int skillId) const;
    const PLAYER_SKILL_INFO* GetHomunSkillItemBySkillId(int skillId) const;
    const PLAYER_SKILL_INFO* GetMercSkillItemBySkillId(int skillId) const;
    int GetSkillUseLevel(int skillId) const;
    void SetSkillUseLevel(int skillId, int level);
    int GetHomunSkillUseLevel(int skillId) const;
    void SetHomunSkillUseLevel(int skillId, int level);
    int GetMerSkillUseLevel(int skillId) const;
    void SetMerSkillUseLevel(int skillId, int level);
    bool LoadSkillUseLevels(int characterSlot);
    bool SaveSkillUseLevels(int characterSlot) const;
    void ClearTemporarySkillItems();
    void ClearNpcShopState(bool clearPendingResults = true);
    NpcShopMode GetNpcShopPendingResult() const {
        return m_shopPendingResults.empty() ? NpcShopMode::None : m_shopPendingResults.front();
    }
    void QueueNpcShopResult(NpcShopMode mode) {
        if (mode != NpcShopMode::None) m_shopPendingResults.push_back(mode);
    }
    bool ConsumeNpcShopResult(NpcShopMode mode) {
        if (mode == NpcShopMode::None || GetNpcShopPendingResult() != mode) return false;
        m_shopPendingResults.pop_front();
        return true;
    }
    void SetNpcShopChoice(u32 npcId);
    void SetNpcShopRows(u32 npcId, NpcShopMode mode, const std::vector<NPC_SHOP_ROW>& rows);
    bool AdjustNpcShopDealBySourceRow(size_t sourceRowIndex, int deltaQuantity);
    bool AdjustNpcShopDealByDealRow(size_t dealRowIndex, int deltaQuantity);
    u32 GetNpcShopUnitPrice(const NPC_SHOP_ROW& row) const;
    void ClearParty();
    unsigned int GetNumParty() const;
    void AddMemberToParty(const FRIEND_INFO& info);
    unsigned int GetMemberAidFromParty(const char* characterName) const;
    void DeleteMemberFromParty(const char* characterName);
    void ChangeRoleFromParty(unsigned int aid, int role);
    bool SetPartyMemberHp(unsigned int aid, long long hp, long long maxHp);
    bool SetPartyMemberPosition(unsigned int aid, int x, int y);
    bool ReplacePartyRoster(const std::string& partyName, const std::vector<FRIEND_INFO>& members);
    void InvalidatePartyPositions();
    bool IsPartyMemberOnMap(const FRIEND_INFO& member, const std::string& currentMap) const;
    bool IsPartyMarkerEligible(const FRIEND_INFO& member, const std::string& currentMap) const;
    u64 GetPartyPositionRevision() const { return m_partyPositionRevision; }
    const FRIEND_INFO* FindPartyMemberByAid(unsigned int aid) const;
    void RefreshPartyUI();
    const std::list<FRIEND_INFO>& GetPartyList() const;
    void ClearFriend();
    unsigned int GetNumFriend() const;
    bool IsFriendName(const char* characterName) const;
    bool DeleteFriendFromList(unsigned int gid);
    void AddFriendToList(const FRIEND_INFO& info);
    bool SetFriendState(unsigned int aid, unsigned int gid, unsigned char state);
    void RefreshFriendUI();
    const std::list<FRIEND_INFO>& GetFriendList() const;
    void ClearShortcutSlots();
    void ClearActiveStatusIcons();
    void SetActiveStatusIcon(int statusType, bool active, u32 remainingMs);
    void PruneExpiredStatusIcons(u32 serverTime);
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
    const std::vector<ACTIVE_STATUS_ICON>& GetActiveStatusIcons() const;
    bool IsStatusIconActive(int statusType) const;
    int GetPlayerSkillPointCount() const;
    int GetWeaponTypeByItemId(int itemId) const;
    u32 GetForceAnimEndTick(u32 aid) const;
    void SetForceAnimEndTick(u32 aid, u32 tick);
    int MakeWeaponTypeByItemId(int primaryWeaponItemId, int secondaryWeaponItemId) const;
    int ResolvePackedWeaponType(int job, int weaponValue) const;
    int GetCurrentPlayerWeaponValue() const;
    std::string GetPlayerWeaponToken(int weaponType) const;
    bool IsSecondAttack(int job, int sex, int weaponItemId) const;
    float GetPCAttackMotion(int job, int sex, int weaponItemId, int isSecondAttack) const;
    unsigned int GetEquippedLeftHandWeaponItemId() const;
    unsigned int GetEquippedRightHandWeaponItemId() const;
    const char* GetPlayerName() const;
    const char* GetJobDisplayName(int job) const;
    const char* GetJobName(int job) const;
    const char* GetAttrWaveName(int attr) const;
    const char* GetJobHitWaveName(int job) const;
    const char* GetWeaponHitWaveName(int weapon) const;
    const char* GetWeaponSwingWaveName(int weapon) const;
    int GetAttackEffectIdBySkillId(unsigned int skillId) const;
    int GetAttackedEffectIdBySkillId(unsigned int skillId) const;
    // Ref/Session2.cpp:6673. `level` is the hit count Am_Skill passes in.
    void LaunchSkillSpecialEffect(int skillId, int level, CGameActor* source, CGameActor* target);
    // Ref/Session2.cpp:3259. `act` is set to -1 when the monster XML has no entry.
    void GetMonsterSkillInfo(const char* monsterName, int skillId, int& act, int& loopType);
    int GetSex() const;
    char* GetJobActName(int job, int sex, char* buf);
    const char* GetPetAccActName(int accessoryItemId) const;
    const char* GetPetIllustName(int job,int accessoryItemId=0) const;
    char* GetJobSprName(int job, int sex, char* buf);
    char* GetPlayerBodyActName(int job, int sex, int weaponItemId, char* buf);
    char* GetPlayerBodySprName(int job, int sex, int weaponItemId, char* buf);
    char* GetHeadActName(int job, int* head, int sex, char* buf);
    char* GetHeadSprName(int job, int* head, int sex, char* buf);
    char* GetAccessoryActName(int job, int* head, int sex, int accessory, char* buf);
    char* GetAccessorySprName(int job, int* head, int sex, int accessory, char* buf);
    char* GetImfName(int job, int head, int sex, char* buf);
    char* GetBodyPaletteName(int job, int sex, int palNum, char* buf);
    char* GetHeadPaletteName(int head, int job, int sex, int palNum, char* buf);
    
private:
    std::deque<NpcShopMode> m_shopPendingResults;
    u64 m_partyPositionRevision = 0;
    void EnsureFogParameterTableLoaded();
    void EnsureAccessoryNameTableLoaded();
    void InitJobHitWaveName();
    void InitWeaponHitWaveName();
    void InitWeaponSwingWaveName();
    int NormalizeJob(int job) const;
    u32 m_serverTime;
    int m_numLatePacket;
    std::list<u32> m_pingTimeList;
    u32 m_averagePingTime;
    CHARACTER_INFO m_selectedCharacterInfo;
    CharacterVitals m_characterVitals;
    bool m_hasSelectedCharacterInfo;
    u32 m_baseExpValue;
    u32 m_nextBaseExpValue;
    u32 m_jobExpValue;
    u32 m_nextJobExpValue;
    bool m_hasBaseExpValue;
    bool m_hasNextBaseExpValue;
    bool m_hasJobExpValue;
    bool m_hasNextJobExpValue;
    std::list<ITEM_INFO> m_inventoryItems;
    std::list<ITEM_INFO> m_storageItems;
    std::list<PLAYER_SKILL_INFO> m_skillItems;
    std::list<PLAYER_SKILL_INFO> m_temporarySkillItems;
    std::list<PLAYER_SKILL_INFO> m_guildSkillItems;
    std::array<int, 1039> m_skillUseLevelList{};
    int m_guildSkillPoint = 0;
    std::list<PLAYER_SKILL_INFO> m_homunSkillItems;
    std::list<PLAYER_SKILL_INFO> m_mercSkillItems;
    std::array<int, 17> m_homunSkillUseLevelList{};
    std::array<int, 38> m_merSkillUseLevelList{};
    mutable std::mutex m_jobDisplayNameMutex;
    mutable std::unordered_map<int, std::string> m_jobDisplayNameCache;
    std::vector<ACTIVE_STATUS_ICON> m_activeStatusIcons;
    bool m_fogParameterTableLoaded;
    std::unordered_map<std::string, SessionFogParameter> m_fogParameterTable;
    bool m_accessoryNameTableLoaded;
    std::vector<std::string> m_accessoryNameTable;
    std::vector<std::string> m_jobHitWaveNameTable;
    std::vector<std::string> m_weaponHitWaveNameTable;
    std::vector<std::string> m_weaponSwingWaveNameTable;
    struct ForceAnimTick { u32 aid; u32 tick; };
    std::vector<ForceAnimTick> m_forceAnimEndTick;
    bool m_monsterSkillInfoLoaded = false;
    XMLDocument m_monsterSkillInfo;
};

extern CSession g_session;
