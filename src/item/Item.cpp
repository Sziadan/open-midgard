#include "Item.h"

#include "DebugLog.h"
#include "core/File.h"
#include "lua/LuaBridge.h"
#include "gamemode/GameMode.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <utility>
#include <vector>

namespace {

constexpr const char* kAccessorySpriteRoot = "data\\sprite\\\xBE\xC7\xBC\xBC\xBB\xE7\xB8\xAE\\";
constexpr const char* kFemaleSex = "\xBF\xA9";
constexpr const char* kMaleSex = "\xB3\xB2";
constexpr const char* kAccessoryNameTableScript = "lua files\\datainfo\\accname.lub";
constexpr const char* kAccessoryNameTableName = "AccNameTable";
constexpr int kMaxAccessoryViewId = 8192;

struct AggregatedCardLabel {
	unsigned int cardId = 0;
	std::string label;
	int count = 0;
};

std::string ResolveItemProducer(const ITEM_INFO& item)
{
	const u32 characterId = u32(u16(item.m_slot[2])) | (u32(u16(item.m_slot[3])) << 16);
	CGameMode* mode = g_modeMgr.GetCurrentGameMode();
	return mode ? mode->ResolveCharacterName(characterId) : std::string();
}

std::string FormatRentalMessage(const std::string& format, const std::string& value, u32 amount = 0)
{
	std::string result;
	for (size_t i = 0; i < format.size(); ++i) {
		if (format[i] == '%' && i + 1 < format.size()) {
			const char next = format[i + 1];
			if (next == 's') { result += value; ++i; continue; }
			if (next == 'd' || next == 'u') { result += std::to_string(amount); ++i; continue; }
			if (next == '%') { result += '%'; ++i; continue; }
		}
		result += format[i];
	}
	return result;
}

std::string AppendSlotSuffix(std::string baseName, int slotCount)
{
	if (baseName.empty() || slotCount <= 0) {
		return baseName;
	}

	baseName += " [";
	baseName += std::to_string(slotCount);
	baseName += "]";
	return baseName;
}

const char* GetRepeatedCardWord(int count)
{
	switch (count) {
	case 2:
		return "Double";
	case 3:
		return "Triple";
	case 4:
		return "Quadruple";
	default:
		return nullptr;
	}
}

void AppendCardLabel(std::vector<AggregatedCardLabel>& labels, unsigned int cardId, std::string&& label)
{
	if (label.empty()) {
		return;
	}

	for (AggregatedCardLabel& existing : labels) {
		if (existing.cardId == cardId) {
			++existing.count;
			return;
		}
	}

	AggregatedCardLabel entry;
	entry.cardId = cardId;
	entry.label = std::move(label);
	entry.count = 1;
	labels.push_back(std::move(entry));
}

std::string BuildAggregatedCardPrefixText(const std::vector<AggregatedCardLabel>& labels)
{
	std::string text;
	for (const AggregatedCardLabel& entry : labels) {
		if (entry.label.empty() || entry.count <= 0) {
			continue;
		}

		if (!text.empty()) {
			text += " ";
		}

		if (const char* repeatWord = GetRepeatedCardWord(entry.count)) {
			text += repeatWord;
			text += " ";
		}

		text += entry.label;
	}
	return text;
}

std::string TrimLine(std::string value)
{
	while (!value.empty() && (value.back() == '\r' || value.back() == '\n')) {
		value.pop_back();
	}
	return value;
}

std::string NormalizeDisplayToken(std::string value)
{
	std::replace(value.begin(), value.end(), '_', ' ');
	return value;
}

std::string ToLowerAscii(std::string value)
{
	std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
		return static_cast<char>(std::tolower(ch));
	});
	return value;
}

bool ParseHashPairLine(const std::string& line, unsigned int& outId, std::string& outValue)
{
	if (line.empty() || line[0] == '/' || line[0] == '#') {
		return false;
	}

	const size_t firstHash = line.find('#');
	if (firstHash == std::string::npos || firstHash == 0) {
		return false;
	}
	const size_t secondHash = line.find('#', firstHash + 1);
	if (secondHash == std::string::npos) {
		return false;
	}

	outId = static_cast<unsigned int>(std::strtoul(line.substr(0, firstHash).c_str(), nullptr, 10));
	if (outId == 0) {
		return false;
	}

	outValue = line.substr(firstHash + 1, secondHash - firstHash - 1);
	return true;
}

bool ParseHashIdLine(const std::string& line, unsigned int& outId)
{
	if (line.empty() || line[0] == '/' || line[0] == '#') {
		return false;
	}

	const size_t firstHash = line.find('#');
	if (firstHash == std::string::npos || firstHash == 0) {
		return false;
	}

	outId = static_cast<unsigned int>(std::strtoul(line.substr(0, firstHash).c_str(), nullptr, 10));
	return outId != 0;
}

bool ParseDescriptionBlockHeaderLine(const std::string& line, unsigned int& outId)
{
	outId = 0;
	if (line.empty() || line[0] == '/' || line[0] == '#') {
		return false;
	}

	const size_t firstHash = line.find('#');
	if (firstHash == std::string::npos || firstHash == 0) {
		return false;
	}

	for (size_t index = firstHash + 1; index < line.size(); ++index) {
		if (!std::isspace(static_cast<unsigned char>(line[index]))) {
			return false;
		}
	}

	outId = static_cast<unsigned int>(std::strtoul(line.substr(0, firstHash).c_str(), nullptr, 10));
	return outId != 0;
}

void AssignUnidentifiedDisplayName(ItemMetadata& metadata, std::string&& value)
{
	metadata.unidentifiedDisplayName = NormalizeDisplayToken(std::move(value));
}

void AssignIdentifiedDisplayName(ItemMetadata& metadata, std::string&& value)
{
	metadata.identifiedDisplayName = NormalizeDisplayToken(std::move(value));
}

void AssignResourceName(ItemMetadata& metadata, std::string&& value)
{
	metadata.unidentifiedResourceName = std::move(value);
}

void AssignIdentifiedResourceName(ItemMetadata& metadata, std::string&& value)
{
	metadata.identifiedResourceName = std::move(value);
}

void AssignCardIllustName(ItemMetadata& metadata, std::string&& value)
{
	metadata.cardIllustName = std::move(value);
}

void AssignSlotCount(ItemMetadata& metadata, int value)
{
	metadata.slotCount = (std::max)(0, value);
}

bool LoadTextFileFromGameData(const char* fileName, std::string& outText)
{
	outText.clear();
	if (!fileName || !*fileName) {
		return false;
	}

	std::vector<std::string> candidates;
	candidates.emplace_back(fileName);
	if (std::strchr(fileName, '\\') == nullptr && std::strchr(fileName, '/') == nullptr) {
		candidates.emplace_back(std::string("data\\") + fileName);
		candidates.emplace_back(std::string("data/") + fileName);
	}

	for (const std::string& candidate : candidates) {
		int size = 0;
		unsigned char* bytes = g_fileMgr.GetData(candidate.c_str(), &size);
		if (!bytes || size <= 0) {
			delete[] bytes;
			continue;
		}

		outText.assign(reinterpret_cast<const char*>(bytes), static_cast<size_t>(size));
		delete[] bytes;
		return true;
	}

	return false;
}

bool HasAccessorySpriteResourceForSex(const std::string& resourceName, const char* sexToken)
{
	if (resourceName.empty() || !sexToken || !*sexToken) {
		return false;
	}

	char actPath[512] = {};
	char sprPath[512] = {};
	const char* separator = resourceName.front() == '_' ? "" : "_";
	std::snprintf(actPath, sizeof(actPath), "%s%s\\%s%s%s.act", kAccessorySpriteRoot, sexToken, sexToken, separator, resourceName.c_str());
	std::snprintf(sprPath, sizeof(sprPath), "%s%s\\%s%s%s.spr", kAccessorySpriteRoot, sexToken, sexToken, separator, resourceName.c_str());
	return g_fileMgr.IsDataExist(actPath) && g_fileMgr.IsDataExist(sprPath);
}

bool HasAccessorySpriteResource(const std::string& resourceName)
{
	return HasAccessorySpriteResourceForSex(resourceName, kMaleSex)
		|| HasAccessorySpriteResourceForSex(resourceName, kFemaleSex);
}

} // namespace

CItemMgr g_ttemmgr;

void ITEM_INFO::SetItemId(unsigned int itemId)
{
	m_itemName = std::to_string(itemId);
}

unsigned int ITEM_INFO::GetItemId() const
{
	if (m_itemName.empty()) {
		return 0;
	}
	return static_cast<unsigned int>(std::strtoul(m_itemName.c_str(), nullptr, 10));
}

std::string ITEM_INFO::GetDisplayName(bool includeSlotSuffix) const
{
	return g_ttemmgr.GetDisplayName(GetItemId(), m_isIdentified != 0, includeSlotSuffix);
}

std::string ITEM_INFO::GetEquipDisplayName(bool includeSlotSuffix) const
{
	return g_ttemmgr.GetEquipDisplayName(*this, includeSlotSuffix);
}

std::string ITEM_INFO::GetItemName(bool includeSlotSuffix) const
{
	return g_ttemmgr.GetItemName(*this, includeSlotSuffix);
}

std::string ITEM_INFO::GetDescription() const
{
	return g_ttemmgr.GetDescription(GetItemId());
}

std::string ITEM_INFO::GetExpiryDisplayText() const
{
	return g_ttemmgr.GetExpiryDisplayText(static_cast<u32>(m_deleteTime));
}

std::string ITEM_INFO::GetResourceName() const
{
	return g_ttemmgr.GetResourceName(GetItemId(), m_isIdentified != 0);
}

int ITEM_INFO::GetSlotCount() const
{
	return g_ttemmgr.GetSlotCount(GetItemId());
}

std::string ITEM_INFO::GetCardIllustName() const
{
	return g_ttemmgr.GetCardIllustName(GetItemId());
}

CItemMgr::CItemMgr()
	: m_loaded(false)
{
}

CItemMgr::~CItemMgr() = default;

void CItemMgr::EnsureLoaded()
{
	if (m_loaded) {
		return;
	}

	LoadDisplayTable();
	LoadResourceTable();
	LoadDescriptionTable();
	LoadSlotCountTable();
	LoadCardIllustTable();
	LoadCardPrefixTable();
	LoadCardPostfixTable();
	LoadCardItemTable();
	LoadItemNameMessages();
	// Primary s_cardDescMap is populated by all four card metadata tables,
	// including records that have no display affix.
	m_cardDescriptorIds.insert(m_cardItemIds.begin(), m_cardItemIds.end());
	m_cardDescriptorIds.insert(m_cardPostfixIds.begin(), m_cardPostfixIds.end());
	m_loaded = true;
}

const ItemMetadata* CItemMgr::GetMetadata(unsigned int itemId)
{
	EnsureLoaded();
	const auto it = m_metadata.find(itemId);
	return it != m_metadata.end() ? &it->second : nullptr;
}

std::string CItemMgr::GetDisplayName(unsigned int itemId, bool identified, bool includeSlotSuffix)
{
	if (const ItemMetadata* metadata = GetMetadata(itemId)) {
		const std::string& preferredName = identified
			? metadata->identifiedDisplayName
			: metadata->unidentifiedDisplayName;
		if (!preferredName.empty()) {
			return includeSlotSuffix ? AppendSlotSuffix(preferredName, metadata->slotCount) : preferredName;
		}

		const std::string& fallbackName = identified
			? metadata->unidentifiedDisplayName
			: metadata->identifiedDisplayName;
		if (!fallbackName.empty()) {
			return includeSlotSuffix ? AppendSlotSuffix(fallbackName, metadata->slotCount) : fallbackName;
		}
	}
	return itemId != 0 ? std::to_string(itemId) : std::string();
}

std::string CItemMgr::GetItemName(const ITEM_INFO& item, bool includeSlotSuffix)
{
	const auto itemId = item.GetItemId();
	if (!item.m_isIdentified) return GetDisplayName(itemId, false, includeSlotSuffix);
	EnsureLoaded();
	// Primary GetItemName handles these markers before its item-type switch.
	// Selected clif_addcards carries intimacy in word two, and itemtype()
	// normalizes eggs to armor. The renamed marker is independent of wire type.
	const bool namedPet = item.m_slot[0] == 0 && item.m_slot[1] == 0 && item.m_slot[3] == 1;
	if (namedPet)
		return ItemNameMessage(756, "Beloved") + GetDisplayName(itemId, true, includeSlotSuffix);
	if (item.m_slot[0] == 254 && item.m_slot[1] == 0) {
		std::string text = item.m_refiningLevel > 0 ? "+" + std::to_string(item.m_refiningLevel) + " " : "";
		return text + ResolveItemProducer(item) + ManufacturedSuffix(0) + GetDisplayName(itemId, true, includeSlotSuffix);
	}
	switch (item.m_itemType) {
	case 4: case 5: case 8: case 9: case 11: case 12: case 13: case 14: case 15: case 19:
		return GetEquipDisplayName(item, includeSlotSuffix);
	default:
		return GetDisplayName(itemId, true, includeSlotSuffix);
	}
}

std::string CItemMgr::GetEquipDisplayName(const ITEM_INFO& item, bool includeSlotSuffix)
{
	const unsigned int itemId = item.GetItemId();
	if (itemId == 0) {
		return std::string();
	}

	EnsureLoaded();

	std::string text;
	if (item.m_refiningLevel > 0) {
		text += "+";
		text += std::to_string(item.m_refiningLevel);
		text += " ";
	}

	// Primary GetItemName enters generic card composition only when slot zero
	// has a card descriptor. Named/crafted sentinel slots have separate contracts.
	const bool composeCards = m_cardDescriptorIds.count(static_cast<unsigned int>(item.m_slot[0])) != 0;
	if (!composeCards && item.m_slot[0] == 255) {
		switch ((u16(item.m_slot[1]) >> 8) & 0xff) {
		case 5: text += ItemNameMessage(459, "Very Strong "); break;
		case 10: text += ItemNameMessage(460, "Very Very Strong "); break;
		case 15: text += ItemNameMessage(461, "Very Very Very Strong "); break;
		default: break;
		}
		return text + ResolveItemProducer(item) + ManufacturedSuffix(u8(item.m_slot[1]))
			+ GetDisplayName(itemId, true, includeSlotSuffix);
	}
	std::vector<AggregatedCardLabel> prefixLabels, postfixLabels;
	if (composeCards) {
		for (int slotIndex = 0; slotIndex < 4; ++slotIndex) {
			const unsigned int cardId = static_cast<unsigned int>(item.m_slot[slotIndex]);
			std::string label = GetCardPrefixName(cardId);
			// Null/empty primary card labels contribute nothing; never invent an
			// affix from the card item's display name or numeric ID.
			if (label.empty()) continue;
			auto& labels = IsPostfixCard(cardId) ? postfixLabels : prefixLabels;
			AppendCardLabel(labels, cardId, std::move(label));
		}
	}

	const std::string prefixText = BuildAggregatedCardPrefixText(prefixLabels);
	if (!prefixText.empty()) {
		text += prefixText;
		text += " ";
	}

	text += GetDisplayName(itemId, item.m_isIdentified != 0, includeSlotSuffix);

	for (const auto& postfix : postfixLabels) {
		text += " ";
		text += postfix.label;
		// Unlike prefixes, Ref GetPostfix puts the repeat word after the label.
		if (const char* repeatWord = GetRepeatedCardWord(postfix.count)) {
			text += " ";
			text += repeatWord;
		}
	}

	return text;
}

std::string CItemMgr::GetDescription(unsigned int itemId)
{
	if (const ItemMetadata* metadata = GetMetadata(itemId)) {
		return metadata->description;
	}
	return std::string();
}

std::string CItemMgr::GetExpiryDisplayText(u32 unixSeconds)
{
	if (!unixSeconds) return {};
	EnsureLoaded();
	// HireExpireDate is an unsigned wire epoch, even though legacy ITEM_INFO
	// stores its bits in int. Do not sign-extend dates at/after January 2038.
	const std::time_t stamp = static_cast<std::time_t>(unixSeconds);
	if (static_cast<unsigned long long>(stamp) != unixSeconds) return {};
	const std::tm* local = std::localtime(&stamp);
	if (!local) return {};
	char date[128]{};
	const std::string format = ItemNameMessage(1260, "%m/%d %H:%M");
	if (!std::strftime(date, sizeof(date), format.c_str(), local)) return {};
	return FormatRentalMessage(ItemNameMessage(1255, " ^ff0000Expiration Date: %s^000000"), date);
}

std::vector<std::string> CItemMgr::GetRentalTimeNotices(unsigned int itemId, u32 seconds)
{
	EnsureLoaded();
	const auto name = GetDisplayName(itemId, true, false);
	const u32 minutes = seconds / 60;
	std::vector<std::string> messages;
	messages.push_back(FormatRentalMessage(minutes
		? ItemNameMessage(1256, " The '%s' item will disappear in %d minutes.")
		: ItemNameMessage(1270, " The '%s' item will disappear in %d seconds."), name, minutes ? minutes : seconds));
	if (minutes == 1)
		messages.push_back(FormatRentalMessage(ItemNameMessage(1257, "%s' item will be deleted from the Inventory in 1 minute."), name));
	return messages;
}

std::string CItemMgr::GetRentalDeleteNotice(unsigned int itemId)
{
	EnsureLoaded();
	return FormatRentalMessage(ItemNameMessage(1258, " '%s' item has been deleted from the Inventory."), GetDisplayName(itemId, true, false));
}

std::string CItemMgr::GetPickupFailureNotice(unsigned char result)
{
    EnsureLoaded();
    // Ref Zc_Item_Pickup_Ack3 uses these positional MsgString IDs.
    switch(result) {
    case 1: case 6: return ItemNameMessage(53,"You cannot get the item.");
    case 2: return ItemNameMessage(52,"You cannot carry more items because you are overweight.");
    case 4: return ItemNameMessage(220,"You can't have this item because you will exceed the weight limit.");
    case 5: return ItemNameMessage(279,"You cannot carry more than 30,000 of one kind of item.");
    default: return {};
    }
}

std::string CItemMgr::GetEquipFailureNotice()
{
    EnsureLoaded();
    return ItemNameMessage(372,"You can't put this item on.");
}

std::string CItemMgr::GetResourceName(unsigned int itemId, bool identified)
{
	if (const ItemMetadata* metadata = GetMetadata(itemId)) {
		const std::string& preferredName = identified
			? metadata->identifiedResourceName
			: metadata->unidentifiedResourceName;
		if (!preferredName.empty()) {
			return preferredName;
		}

		const std::string& fallbackName = identified
			? metadata->unidentifiedResourceName
			: metadata->identifiedResourceName;
		if (!fallbackName.empty()) {
			return fallbackName;
		}
	}
	return std::string();
}

int CItemMgr::GetSlotCount(unsigned int itemId)
{
	if (const ItemMetadata* metadata = GetMetadata(itemId)) {
		return metadata->slotCount;
	}
	return 0;
}

std::string CItemMgr::GetCardIllustName(unsigned int itemId)
{
	if (const ItemMetadata* metadata = GetMetadata(itemId)) {
		return metadata->cardIllustName;
	}
	return std::string();
}

int CItemMgr::GetVisibleHeadgearViewId(unsigned int itemId)
{
	EnsureLoaded();
	const ItemMetadata* metadata = GetMetadata(itemId);
	if (!metadata) {
		return 0;
	}

	const std::string& resourceName = !metadata->identifiedResourceName.empty()
		? metadata->identifiedResourceName
		: metadata->unidentifiedResourceName;
	if (resourceName.empty()) {
		return 0;
	}

	const auto cached = m_visibleHeadgearViewIdsByResourceName.find(resourceName);
	if (cached != m_visibleHeadgearViewIdsByResourceName.end()) {
		return cached->second;
	}

	int resolvedViewId = 0;
	std::string mappedResourceName;
	if (g_buabridge.LoadRagnarokScriptOnce(kAccessoryNameTableScript)) {
		for (int viewId = 1; viewId <= kMaxAccessoryViewId; ++viewId) {
			mappedResourceName.clear();
			if (!g_buabridge.GetGlobalTableStringByIntegerKey(kAccessoryNameTableName, viewId, &mappedResourceName)
				|| mappedResourceName != resourceName) {
				continue;
			}

			if (resolvedViewId != 0 && resolvedViewId != viewId) {
				DbgLog("[Item] ambiguous headgear view mapping item=%u resource='%s' views=%d,%d\n",
					itemId,
					resourceName.c_str(),
					resolvedViewId,
					viewId);
				resolvedViewId = 0;
				break;
			}

			resolvedViewId = viewId;
		}
	}

	m_visibleHeadgearViewIdsByResourceName[resourceName] = resolvedViewId;
	return resolvedViewId;
}

std::string CItemMgr::GetVisibleHeadgearResourceNameByViewId(int viewId)
{
	EnsureLoaded();
	if (viewId <= 0) {
		return std::string();
	}

	const auto cached = m_visibleHeadgearResourceNamesByViewId.find(viewId);
	if (cached != m_visibleHeadgearResourceNamesByViewId.end()) {
		return cached->second;
	}

	std::string resolved;
	if (g_buabridge.LoadRagnarokScriptOnce(kAccessoryNameTableScript)
		&& g_buabridge.GetGlobalTableStringByIntegerKey(kAccessoryNameTableName, viewId, &resolved)) {
		if (!HasAccessorySpriteResource(resolved)) {
			DbgLog("[Item] accessory sprite missing view=%d resource='%s'\n", viewId, resolved.c_str());
		}
	} else {
		resolved.clear();
	}

	m_visibleHeadgearResourceNamesByViewId[viewId] = resolved;
	return resolved;
}

std::string CItemMgr::GetCardPrefixName(unsigned int itemId)
{
	EnsureLoaded();
	const auto it = m_cardPrefixNames.find(itemId);
	return it != m_cardPrefixNames.end() ? it->second : std::string();
}

bool CItemMgr::IsCardItem(unsigned int itemId)
{
	EnsureLoaded();
	return m_cardItemIds.find(itemId) != m_cardItemIds.end();
}

bool CItemMgr::IsPostfixCard(unsigned int itemId)
{
	EnsureLoaded();
	return m_cardPostfixIds.find(itemId) != m_cardPostfixIds.end();
}

bool CItemMgr::LoadDisplayTable()
{
	const bool loadedUnidentified = ParsePairTable("num2itemdisplaynametable.txt", AssignUnidentifiedDisplayName);
	const bool loadedIdentified = ParsePairTable("idnum2itemdisplaynametable.txt", AssignIdentifiedDisplayName);
	return loadedUnidentified || loadedIdentified;
}

bool CItemMgr::LoadResourceTable()
{
	const bool loadedUnidentified = ParsePairTable("num2itemresnametable.txt", AssignResourceName);
	const bool loadedIdentified = ParsePairTable("idnum2itemresnametable.txt", AssignIdentifiedResourceName);
	return loadedUnidentified || loadedIdentified;
}

bool CItemMgr::LoadDescriptionTable()
{
	const bool loadedUnidentified = ParseDescriptionBlocks("num2itemdesctable.txt");
	const bool loadedIdentified = ParseDescriptionBlocks("idnum2itemdesctable.txt");
	return loadedUnidentified || loadedIdentified;
}

bool CItemMgr::LoadSlotCountTable()
{
	return ParseIntegerPairTable("itemSlotCountTable.txt", AssignSlotCount);
}

bool CItemMgr::LoadCardIllustTable()
{
	return ParsePairTable("Num2CardIllustNameTable.txt", AssignCardIllustName, &m_cardDescriptorIds);
}

bool CItemMgr::LoadCardPrefixTable()
{
	std::string text;
	if (!LoadTextFileFromGameData("cardprefixnametable.txt", text)) {
		return false;
	}

	std::istringstream input(text);
	std::string line;
	while (std::getline(input, line)) {
		unsigned int itemId = 0;
		std::string value;
		if (!ParseHashPairLine(TrimLine(line), itemId, value)) {
			continue;
		}
		m_cardPrefixNames[itemId] = NormalizeDisplayToken(std::move(value));
		m_cardDescriptorIds.insert(itemId);
	}

	return true;
}

bool CItemMgr::LoadCardPostfixTable()
{
	return ParseIdSetTable("cardpostfixnametable.txt", m_cardPostfixIds);
}

bool CItemMgr::LoadCardItemTable()
{
	return ParseIdSetTable("carditemnametable.txt", m_cardItemIds);
}

void CItemMgr::LoadItemNameMessages()
{
	std::string text;
	if (!LoadTextFileFromGameData("msgstringtable.txt", text)) return;
	size_t begin = 0;
	for (unsigned int index = 0; begin < text.size(); ++index) {
		const size_t end = text.find('#', begin);
		if (end == std::string::npos) break;
		while (begin < end && (text[begin] == '\r' || text[begin] == '\n')) ++begin;
        if (index == 52 || index == 53 || index == 220 || index == 279 || index == 372
            || (index >= 450 && index <= 454) || (index >= 459 && index <= 461) || index == 756
			|| (index >= 1255 && index <= 1258) || index == 1260 || index == 1270) {
			// Preserve spaces in positional message values, including their absence.
			m_itemNameMessages[index] = text.substr(begin, end - begin);
		}
		begin = end + 1;
	}
}

std::string CItemMgr::ItemNameMessage(unsigned int index, const char* fallback) const
{
	const auto found = m_itemNameMessages.find(index);
	return found == m_itemNameMessages.end() ? std::string(fallback) : found->second;
}

std::string CItemMgr::ManufacturedSuffix(u8 element) const
{
	switch (element) {
	case 0: return ItemNameMessage(450, "'s ");
	case 1: return ItemNameMessage(452, "'s Ice ");
	case 2: return ItemNameMessage(454, "'s Earth ");
	case 3: return ItemNameMessage(451, "'s Fire ");
	case 4: return ItemNameMessage(453, "'s Wind ");
	default: return {};
	}
}

bool CItemMgr::ParsePairTable(const char* fileName, void (*assignValue)(ItemMetadata&, std::string&&),
    std::unordered_set<unsigned int>* descriptorIds)
{
	std::string text;
	if (!LoadTextFileFromGameData(fileName, text)) {
		return false;
	}

	std::istringstream input(text);
	std::string line;
	while (std::getline(input, line)) {
		unsigned int itemId = 0;
		std::string value;
		if (!ParseHashPairLine(TrimLine(line), itemId, value)) {
			continue;
		}
		assignValue(m_metadata[itemId], std::move(value));
		if (descriptorIds) descriptorIds->insert(itemId);
	}

	return true;
}

bool CItemMgr::ParseIntegerPairTable(const char* fileName, void (*assignValue)(ItemMetadata&, int))
{
	std::string text;
	if (!LoadTextFileFromGameData(fileName, text)) {
		return false;
	}

	std::istringstream input(text);
	std::string line;
	while (std::getline(input, line)) {
		unsigned int itemId = 0;
		std::string value;
		if (!ParseHashPairLine(TrimLine(line), itemId, value)) {
			continue;
		}
		assignValue(m_metadata[itemId], std::atoi(value.c_str()));
	}

	return true;
}

bool CItemMgr::ParseDescriptionBlocks(const char* fileName)
{
	std::string text;
	if (!LoadTextFileFromGameData(fileName, text)) {
		return false;
	}

	std::istringstream input(text);
	std::string line;
	unsigned int currentItemId = 0;
	std::ostringstream description;
	bool collecting = false;

	while (std::getline(input, line)) {
		line = TrimLine(line);
		if (!collecting) {
			unsigned int itemId = 0;
			if (ParseDescriptionBlockHeaderLine(line, itemId)) {
				currentItemId = itemId;
				description.str(std::string());
				description.clear();
				collecting = true;
			}
			continue;
		}

		if (line == "#") {
			m_metadata[currentItemId].description = description.str();
			currentItemId = 0;
			collecting = false;
			continue;
		}

		if (description.tellp() > 0) {
			description << '\n';
		}
		description << line;
	}

	return true;
}

bool CItemMgr::ParseIdSetTable(const char* fileName, std::unordered_set<unsigned int>& outSet)
{
	std::string text;
	if (!LoadTextFileFromGameData(fileName, text)) {
		return false;
	}

	std::istringstream input(text);
	std::string line;
	while (std::getline(input, line)) {
		unsigned int itemId = 0;
		if (!ParseHashIdLine(TrimLine(line), itemId)) {
			continue;
		}
		outSet.insert(itemId);
	}

	return true;
}
