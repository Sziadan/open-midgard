#pragma once
#include <string>
#include <list>
#include <vector>
#include "Types.h"
#include "item/Item.h"

struct accountInfo {
    std::string display;
    std::string desc;
    std::string balloon;
    std::string address;
    std::string port;
};

constexpr int JT_G_MASTER = 20002;

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
    void SetInventoryItem(const ITEM_INFO& itemInfo);
    void AddInventoryItem(const ITEM_INFO& itemInfo);
    void RemoveInventoryItem(unsigned int itemIndex, int amount);
    bool SetInventoryItemWearLocation(unsigned int itemIndex, int wearLocation);
    void ClearInventoryWearLocationMask(int wearMask, unsigned int exceptItemIndex = 0);
    const std::list<ITEM_INFO>& GetInventoryItems() const;
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
    char* GetImfName(int job, int head, int sex, char* buf);
    char* GetBodyPaletteName(int job, int sex, int palNum, char* buf);
    char* GetHeadPaletteName(int head, int job, int sex, int palNum, char* buf);
    
private:
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
    std::vector<std::string> m_jobHitWaveNameTable;
    std::vector<std::string> m_weaponHitWaveNameTable;
};

extern CSession g_session;
