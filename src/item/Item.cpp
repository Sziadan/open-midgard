#include "Item.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>

namespace {

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

std::filesystem::path MakeReferenceRoot()
{
#ifdef RO_SOURCE_ROOT
	return std::filesystem::path(RO_SOURCE_ROOT) / "Ref" / "GRF-Content" / "data";
#else
	return std::filesystem::current_path() / "Ref" / "GRF-Content" / "data";
#endif
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

std::string ITEM_INFO::GetDisplayName() const
{
	return g_ttemmgr.GetDisplayName(GetItemId(), m_isIdentified != 0);
}

std::string ITEM_INFO::GetEquipDisplayName() const
{
	return g_ttemmgr.GetEquipDisplayName(*this);
}

std::string ITEM_INFO::GetDescription() const
{
	return g_ttemmgr.GetDescription(GetItemId());
}

std::string ITEM_INFO::GetResourceName() const
{
	return g_ttemmgr.GetResourceName(GetItemId(), m_isIdentified != 0);
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
	LoadCardPrefixTable();
	LoadCardPostfixTable();
	LoadCardItemTable();
	m_loaded = true;
}

const ItemMetadata* CItemMgr::GetMetadata(unsigned int itemId)
{
	EnsureLoaded();
	const auto it = m_metadata.find(itemId);
	return it != m_metadata.end() ? &it->second : nullptr;
}

std::string CItemMgr::GetDisplayName(unsigned int itemId, bool identified)
{
	if (const ItemMetadata* metadata = GetMetadata(itemId)) {
		const std::string& preferredName = identified
			? metadata->identifiedDisplayName
			: metadata->unidentifiedDisplayName;
		if (!preferredName.empty()) {
			return preferredName;
		}

		const std::string& fallbackName = identified
			? metadata->unidentifiedDisplayName
			: metadata->identifiedDisplayName;
		if (!fallbackName.empty()) {
			return fallbackName;
		}
	}
	return itemId != 0 ? std::to_string(itemId) : std::string();
}

std::string CItemMgr::GetEquipDisplayName(const ITEM_INFO& item)
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

	bool hasPrefix = false;
	for (int slotIndex = 0; slotIndex < 4; ++slotIndex) {
		const unsigned int cardId = static_cast<unsigned int>(item.m_slot[slotIndex]);
		if (cardId == 0 || !IsCardItem(cardId) || IsPostfixCard(cardId)) {
			continue;
		}

		std::string prefix = GetCardPrefixName(cardId);
		if (prefix.empty()) {
			prefix = GetDisplayName(cardId, true);
		}
		if (prefix.empty()) {
			continue;
		}

		if (hasPrefix) {
			text += " ";
		}
		text += prefix;
		hasPrefix = true;
	}

	if (hasPrefix) {
		text += " ";
	}

	text += GetDisplayName(itemId, item.m_isIdentified != 0);

	for (int slotIndex = 0; slotIndex < 4; ++slotIndex) {
		const unsigned int cardId = static_cast<unsigned int>(item.m_slot[slotIndex]);
		if (cardId == 0 || !IsCardItem(cardId) || !IsPostfixCard(cardId)) {
			continue;
		}

		std::string postfix = GetCardPrefixName(cardId);
		if (postfix.empty()) {
			postfix = GetDisplayName(cardId, true);
		}
		if (postfix.empty()) {
			continue;
		}

		text += " ";
		text += postfix;
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
	return ParseDescriptionBlocks("num2itemdesctable.txt")
		|| ParseDescriptionBlocks("idnum2itemdesctable.txt");
}

bool CItemMgr::LoadCardPrefixTable()
{
	const std::string path = ResolveReferencePath("cardprefixnametable.txt");
	if (path.empty()) {
		return false;
	}

	std::ifstream input(path, std::ios::binary);
	if (!input.is_open()) {
		return false;
	}

	std::string line;
	while (std::getline(input, line)) {
		unsigned int itemId = 0;
		std::string value;
		if (!ParseHashPairLine(TrimLine(line), itemId, value)) {
			continue;
		}
		m_cardPrefixNames[itemId] = NormalizeDisplayToken(std::move(value));
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

bool CItemMgr::ParsePairTable(const char* fileName, void (*assignValue)(ItemMetadata&, std::string&&))
{
	const std::string path = ResolveReferencePath(fileName);
	if (path.empty()) {
		return false;
	}

	std::ifstream input(path, std::ios::binary);
	if (!input.is_open()) {
		return false;
	}

	std::string line;
	while (std::getline(input, line)) {
		unsigned int itemId = 0;
		std::string value;
		if (!ParseHashPairLine(TrimLine(line), itemId, value)) {
			continue;
		}
		assignValue(m_metadata[itemId], std::move(value));
	}

	return true;
}

bool CItemMgr::ParseDescriptionBlocks(const char* fileName)
{
	const std::string path = ResolveReferencePath(fileName);
	if (path.empty()) {
		return false;
	}

	std::ifstream input(path, std::ios::binary);
	if (!input.is_open()) {
		return false;
	}

	std::string line;
	unsigned int currentItemId = 0;
	std::ostringstream description;
	bool collecting = false;

	while (std::getline(input, line)) {
		line = TrimLine(line);
		if (!collecting) {
			unsigned int itemId = 0;
			std::string ignored;
			if (ParseHashPairLine(line, itemId, ignored)) {
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
	const std::string path = ResolveReferencePath(fileName);
	if (path.empty()) {
		return false;
	}

	std::ifstream input(path, std::ios::binary);
	if (!input.is_open()) {
		return false;
	}

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

std::string CItemMgr::ResolveReferencePath(const char* fileName) const
{
	if (!fileName || !*fileName) {
		return std::string();
	}

	const std::filesystem::path root = MakeReferenceRoot();
	const std::filesystem::path candidate = root / fileName;
	if (std::filesystem::exists(candidate)) {
		return candidate.string();
	}

	return std::string();
}
