#pragma once

#include "Types.h"

#include <string>
#include <unordered_map>

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
    std::string GetDescription() const;
    std::string GetResourceName() const;
};

struct ItemMetadata {
    std::string unidentifiedDisplayName;
    std::string identifiedDisplayName;
    std::string resourceName;
    std::string description;
};

class CItemMgr
{
public:
    CItemMgr();
    ~CItemMgr();

    void EnsureLoaded();
    const ItemMetadata* GetMetadata(unsigned int itemId);
    std::string GetDisplayName(unsigned int itemId, bool identified);
    std::string GetDescription(unsigned int itemId);
    std::string GetResourceName(unsigned int itemId);

private:
    bool LoadDisplayTable();
    bool LoadResourceTable();
    bool LoadDescriptionTable();
    bool ParsePairTable(const char* fileName, void (*assignValue)(ItemMetadata&, std::string&&));
    bool ParseDescriptionBlocks(const char* fileName);
    std::string ResolveReferencePath(const char* fileName) const;

    bool m_loaded;
    std::unordered_map<unsigned int, ItemMetadata> m_metadata;
};

extern CItemMgr g_ttemmgr;
