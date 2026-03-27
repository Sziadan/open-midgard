#pragma once

#include "Types.h"

#include <string>
#include <unordered_map>
#include <unordered_set>

struct ITEM_INFO {
    int m_itemType = 0;
    int m_location = 0;
    unsigned int m_itemIndex = 0;
    int m_wearLocation = 0;
    int m_num = 0;
    int m_price = 0;
    int m_realPrice = 0;
    int m_slot[4] = { 0, 0, 0, 0 };
    std::string m_itemName;
    unsigned char m_isIdentified = 0;
    unsigned char m_isDamaged = 0;
    int m_refiningLevel = 0;
    unsigned short m_isYours = 0;
    int m_deleteTime = 0;

    void SetItemId(unsigned int itemId);
    unsigned int GetItemId() const;
    std::string GetDisplayName() const;
    std::string GetEquipDisplayName() const;
    std::string GetDescription() const;
    std::string GetResourceName() const;
};

struct ItemMetadata {
    std::string unidentifiedDisplayName;
    std::string identifiedDisplayName;
    std::string unidentifiedResourceName;
    std::string identifiedResourceName;
    std::string description;
    int itemType = 0;
    int equipLocation = 0;
    int viewId = 0;
};

class CItemMgr
{
public:
    CItemMgr();
    ~CItemMgr();

    void EnsureLoaded();
    const ItemMetadata* GetMetadata(unsigned int itemId);
    std::string GetDisplayName(unsigned int itemId, bool identified);
    std::string GetEquipDisplayName(const ITEM_INFO& item);
    std::string GetDescription(unsigned int itemId);
    std::string GetResourceName(unsigned int itemId, bool identified);
    int GetItemType(unsigned int itemId);
    int GetEquipLocation(unsigned int itemId);
    int GetViewId(unsigned int itemId);
    std::string GetVisibleHeadgearResourceNameByViewId(int viewId);
    std::string GetCardPrefixName(unsigned int itemId);
    bool IsCardItem(unsigned int itemId);
    bool IsPostfixCard(unsigned int itemId);

private:
    bool LoadDisplayTable();
    bool LoadResourceTable();
    bool LoadDescriptionTable();
    bool LoadItemDbViewTable();
    bool LoadCardPrefixTable();
    bool LoadCardPostfixTable();
    bool LoadCardItemTable();
    bool ParsePairTable(const char* fileName, void (*assignValue)(ItemMetadata&, std::string&&));
    bool ParseDescriptionBlocks(const char* fileName);
    bool ParseIdSetTable(const char* fileName, std::unordered_set<unsigned int>& outSet);
    std::string ResolveReferencePath(const char* fileName) const;
    std::string ResolveServerItemDbPath() const;

    bool m_loaded;
    std::unordered_map<unsigned int, ItemMetadata> m_metadata;
    std::unordered_map<int, std::string> m_visibleHeadgearResourceNamesByViewId;
    std::unordered_map<unsigned int, std::string> m_cardPrefixNames;
    std::unordered_set<unsigned int> m_cardPostfixIds;
    std::unordered_set<unsigned int> m_cardItemIds;
};

extern CItemMgr g_ttemmgr;
