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
	metadata.resourceName = std::move(value);
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

std::string ITEM_INFO::GetDescription() const
{
	return g_ttemmgr.GetDescription(GetItemId());
}

std::string ITEM_INFO::GetResourceName() const
{
	return g_ttemmgr.GetResourceName(GetItemId());
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

std::string CItemMgr::GetDescription(unsigned int itemId)
{
	if (const ItemMetadata* metadata = GetMetadata(itemId)) {
		return metadata->description;
	}
	return std::string();
}

std::string CItemMgr::GetResourceName(unsigned int itemId)
{
	if (const ItemMetadata* metadata = GetMetadata(itemId)) {
		return metadata->resourceName;
	}
	return std::string();
}

bool CItemMgr::LoadDisplayTable()
{
	const bool loadedUnidentified = ParsePairTable("num2itemdisplaynametable.txt", AssignUnidentifiedDisplayName);
	const bool loadedIdentified = ParsePairTable("idnum2itemdisplaynametable.txt", AssignIdentifiedDisplayName);
	return loadedUnidentified || loadedIdentified;
}

bool CItemMgr::LoadResourceTable()
{
	return ParsePairTable("num2itemresnametable.txt", AssignResourceName)
		|| ParsePairTable("idnum2itemresnametable.txt", AssignResourceName);
}

bool CItemMgr::LoadDescriptionTable()
{
	return ParseDescriptionBlocks("num2itemdesctable.txt")
		|| ParseDescriptionBlocks("idnum2itemdesctable.txt");
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
