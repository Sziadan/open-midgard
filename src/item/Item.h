#pragma once

#include "Types.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

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
    bool IsCard() const;
    // Primary Ref CSession::GetItemDisColor: packed AARRGGBB, for hover names.
    u32 GetNameColorArgb() const {
        if (!m_isIdentified) return 0xffaaaaaau;
        return m_isYours == 2 ? 0xffffff00u : 0xffffffffu;
    }
    std::string GetDisplayName(bool includeSlotSuffix = true) const;
    std::string GetItemName(bool includeSlotSuffix = true) const;
    std::string GetEquipDisplayName(bool includeSlotSuffix = true) const;
    std::string GetDescription() const;
    std::string GetExpiryDisplayText() const;
    std::string GetResourceName() const;
    int GetSlotCount() const;
    std::string GetCardIllustName() const;
};

struct ItemMetadata {
    std::string unidentifiedDisplayName;
    std::string identifiedDisplayName;
    std::string unidentifiedResourceName;
    std::string identifiedResourceName;
    std::string description;
    std::string recipeDescription;
    int slotCount = 0;
    std::string cardIllustName;
};

struct ItemTextSegment {
    std::string text;
    u32 color = 0xff000000u;
};

class CItemMgr
{
public:
    CItemMgr();
    ~CItemMgr();

    void EnsureLoaded();
    const ItemMetadata* GetMetadata(unsigned int itemId);
    std::string GetDisplayName(unsigned int itemId, bool identified, bool includeSlotSuffix = true);
    std::string GetItemName(const ITEM_INFO& item, bool includeSlotSuffix = true);
    std::string GetEquipDisplayName(const ITEM_INFO& item, bool includeSlotSuffix = true);
    std::string GetDescription(unsigned int itemId);
    std::string GetRecipeDescription(unsigned int itemId);
    std::string GetExpiryDisplayText(u32 unixSeconds);
    std::vector<std::string> GetRentalTimeNotices(unsigned int itemId, u32 seconds);
    std::string GetRentalDeleteNotice(unsigned int itemId);
    std::string GetPickupFailureNotice(unsigned char result);
    std::string GetEquipFailureNotice();
    std::string GetItemUseNotice(unsigned itemId);
    std::string GetItemObtainSizingText(const ITEM_INFO& item);
    std::string GetItemObtainChatText(const ITEM_INFO& item);
    std::vector<ItemTextSegment> GetItemObtainSegments(const ITEM_INFO& item);
    std::string GetResourceName(unsigned int itemId, bool identified);
    int GetSlotCount(unsigned int itemId);
    std::string GetCardIllustName(unsigned int itemId);
    int GetVisibleHeadgearViewId(unsigned int itemId);
    std::string GetVisibleHeadgearResourceNameByViewId(int viewId);
    std::string GetCardPrefixName(unsigned int itemId);
    bool IsCardItem(unsigned int itemId);
    bool IsPostfixCard(unsigned int itemId);

private:
    bool LoadDisplayTable();
    bool LoadResourceTable();
    bool LoadDescriptionTable();
    bool LoadSlotCountTable();
    bool LoadCardIllustTable();
    bool LoadCardPrefixTable();
    bool LoadCardPostfixTable();
    bool LoadCardItemTable();
    void LoadItemNameMessages();
    std::string ItemNameMessage(unsigned int index, const char* fallback) const;
    std::string ManufacturedSuffix(u8 element) const;
    bool ParsePairTable(const char* fileName, void (*assignValue)(ItemMetadata&, std::string&&),
        std::unordered_set<unsigned int>* descriptorIds = nullptr);
    bool ParseIntegerPairTable(const char* fileName, void (*assignValue)(ItemMetadata&, int));
    bool ParseDescriptionBlocks(const char* fileName, std::string ItemMetadata::* field = &ItemMetadata::description);
    bool ParseIdSetTable(const char* fileName, std::unordered_set<unsigned int>& outSet);

    bool m_loaded;
    std::unordered_map<unsigned int, ItemMetadata> m_metadata;
    std::unordered_map<int, std::string> m_visibleHeadgearResourceNamesByViewId;
    std::unordered_map<std::string, int> m_visibleHeadgearViewIdsByResourceName;
    std::unordered_map<unsigned int, std::string> m_cardPrefixNames;
    std::unordered_set<unsigned int> m_cardPostfixIds;
    std::unordered_set<unsigned int> m_cardItemIds;
    std::unordered_set<unsigned int> m_cardDescriptorIds;
    std::unordered_map<unsigned int, std::string> m_itemNameMessages;
};

extern CItemMgr g_ttemmgr;
