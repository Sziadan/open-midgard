#include "LuaBridge.h"

#include "core/File.h"
#include "DebugLog.h"

struct JobNameEntry {
	int job;
	const char* name;
};

#include "../session/JobNameTable.generated.inc"
#include "../skill/SkillEnumIdTable.inc"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>

#include <cmath>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

namespace {

std::string NormalizeSlashPath(std::string path)
{
	std::replace(path.begin(), path.end(), '/', '\\');
	return path;
}

int GetAbsoluteLuaIndex(lua_State* state, int index)
{
	if (!state || index > 0 || index <= LUA_REGISTRYINDEX) {
		return index;
	}

	return lua_gettop(state) + index + 1;
}

bool PathEndsWithInsensitive(const std::string& path, const char* suffix)
{
	if (!suffix) {
		return false;
	}

	const size_t suffixLength = std::strlen(suffix);
	if (path.size() < suffixLength) {
		return false;
	}

	return _stricmp(path.c_str() + (path.size() - suffixLength), suffix) == 0;
}

bool IsAccessoryNameTableScript(const std::string& path)
{
	return PathEndsWithInsensitive(path, "lua files\\datainfo\\accname.lub")
		|| PathEndsWithInsensitive(path, "lua files\\datainfo\\accname_f.lub")
		|| PathEndsWithInsensitive(path, "lua files\\datainfo\\accname_eng.lub");
}

void SetLuaGlobalInteger(lua_State* state, const char* name, int value)
{
	if (!state || !name || !*name) {
		return;
	}

	lua_pushinteger(state, value);
	lua_setglobal(state, name);
}

bool TryGetLuaGlobalInteger(lua_State* state, const char* name, int* outValue)
{
	if (!state || !name || !*name || !outValue) {
		return false;
	}

	lua_getglobal(state, name);
	if (!lua_isnumber(state, -1)) {
		lua_pop(state, 1);
		return false;
	}

	*outValue = static_cast<int>(lua_tointeger(state, -1));
	lua_pop(state, 1);
	return true;
}

bool TryGetGeneratedJobIdFromGlobalName(const char* globalName, int* outValue)
{
	if (!globalName || std::strncmp(globalName, "JT_", 3) != 0 || !outValue) {
		return false;
	}

	const char* jobName = globalName + 3;
	if (!*jobName) {
		return false;
	}

	for (const JobNameEntry& entry : kGeneratedJobNames) {
		if (entry.name && std::strcmp(entry.name, jobName) == 0) {
			*outValue = entry.job;
			return true;
		}
	}

	return false;
}

bool TryGetLuaTableIntegerField(lua_State* state, int tableIndex, const char* fieldName, int* outValue)
{
	if (!state || !fieldName || !*fieldName || !outValue) {
		return false;
	}

	const int absoluteIndex = GetAbsoluteLuaIndex(state, tableIndex);
	lua_getfield(state, absoluteIndex, fieldName);
	if (!lua_isnumber(state, -1)) {
		lua_pop(state, 1);
		return false;
	}

	*outValue = static_cast<int>(lua_tointeger(state, -1));
	lua_pop(state, 1);
	return true;
}

bool TryGetLuaTableBooleanField(lua_State* state, int tableIndex, const char* fieldName, bool* outValue)
{
	if (!state || !fieldName || !*fieldName || !outValue) {
		return false;
	}

	const int absoluteIndex = GetAbsoluteLuaIndex(state, tableIndex);
	lua_getfield(state, absoluteIndex, fieldName);
	if (lua_isboolean(state, -1)) {
		*outValue = lua_toboolean(state, -1) != 0;
		lua_pop(state, 1);
		return true;
	}
	if (lua_isnumber(state, -1)) {
		*outValue = lua_tointeger(state, -1) != 0;
		lua_pop(state, 1);
		return true;
	}

	lua_pop(state, 1);
	return false;
}

bool TryGetLuaTableIntegerArrayField(lua_State* state, int tableIndex, const char* fieldName, std::vector<int>* outValues)
{
	if (!state || !fieldName || !*fieldName || !outValues) {
		return false;
	}

	outValues->clear();
	const int absoluteIndex = GetAbsoluteLuaIndex(state, tableIndex);
	lua_getfield(state, absoluteIndex, fieldName);
	if (!lua_istable(state, -1)) {
		lua_pop(state, 1);
		return false;
	}

	const int fieldIndex = GetAbsoluteLuaIndex(state, -1);
	const size_t count = static_cast<size_t>(lua_objlen(state, fieldIndex));
	for (size_t index = 1; index <= count; ++index) {
		lua_rawgeti(state, fieldIndex, static_cast<int>(index));
		if (lua_isnumber(state, -1)) {
			outValues->push_back(static_cast<int>(lua_tointeger(state, -1)));
		}
		lua_pop(state, 1);
	}

	lua_pop(state, 1);
	return !outValues->empty();
}

int AllocateSyntheticLuaTableValue(const char* tableName, const char* keyName)
{
	if (!tableName || !*tableName || !keyName || !*keyName) {
		return 0;
	}

	static std::unordered_map<std::string, int> syntheticValues;
	static int nextSkillId = 50000;
	static int nextEffectId = 70000;
	static int nextActorState = 90000;
	static int nextJobEnum = 110000;

	std::string compositeKey = tableName;
	compositeKey += '.';
	compositeKey += keyName;

	const auto existing = syntheticValues.find(compositeKey);
	if (existing != syntheticValues.end()) {
		return existing->second;
	}

	int value = 0;
	if (_stricmp(tableName, "SKID") == 0) {
		value = nextSkillId++;
	} else if (_stricmp(tableName, "EFID") == 0) {
		value = nextEffectId++;
	} else if (_stricmp(tableName, "ACTOR_STATE") == 0) {
		value = nextActorState++;
	} else if (_stricmp(tableName, "JT_ENUM") == 0) {
		value = nextJobEnum++;
	}

	if (value != 0) {
		syntheticValues.emplace(compositeKey, value);
	}

	return value;
}

bool HasLuaTableIntegerKey(lua_State* state, const char* tableName, const char* keyName)
{
	if (!state || !tableName || !*tableName || !keyName || !*keyName) {
		return false;
	}

	lua_getglobal(state, tableName);
	if (!lua_istable(state, -1)) {
		lua_pop(state, 1);
		return false;
	}

	const int tableIndex = GetAbsoluteLuaIndex(state, -1);
	lua_getfield(state, tableIndex, keyName);
	const bool found = lua_isnumber(state, -1);
	lua_pop(state, 2);
	return found;
}

void SetLuaTableIntegerKey(lua_State* state, const char* tableName, const char* keyName, int value)
{
	if (!state || !tableName || !*tableName || !keyName || !*keyName) {
		return;
	}

	lua_getglobal(state, tableName);
	if (!lua_istable(state, -1)) {
		lua_pop(state, 1);
		return;
	}

	const int tableIndex = GetAbsoluteLuaIndex(state, -1);
	lua_pushstring(state, keyName);
	lua_pushinteger(state, value);
	lua_settable(state, tableIndex);
	lua_pop(state, 1);
	DbgLog("[Lua] Synthesizing missing table key %s.%s=%d before loading skill-effect info.\n",
		tableName,
		keyName,
		value);
	}

bool IsUppercaseEnumToken(const std::string& token)

{
	if (token.size() < 3) {
		return false;
	}

	bool hasUnderscore = false;
	for (char ch : token) {
		const unsigned char uch = static_cast<unsigned char>(ch);
		if (ch == '_') {
			hasUnderscore = true;
			continue;
		}
		if ((uch >= 'A' && uch <= 'Z') || (uch >= '0' && uch <= '9')) {
			continue;
		}
		return false;
	}

	return hasUnderscore;
}

bool ShouldTreatAsSkillEnumToken(const std::string& token)
{
	if (!IsUppercaseEnumToken(token)) {
		return false;
	}
	if (_stricmp(token.c_str(), "SKILL_EFFECT_INFO_LIST") == 0
		|| _stricmp(token.c_str(), "SKID") == 0
		|| _stricmp(token.c_str(), "ACTOR_STATE") == 0
		|| _stricmp(token.c_str(), "EFID") == 0
		|| _stricmp(token.c_str(), "LaunchZC_USE_SKILL") == 0) {
		return false;
	}
	return std::strncmp(token.c_str(), "EF_", 3) != 0
		&& std::strncmp(token.c_str(), "ST_", 3) != 0;
}

void PopulateSkillEffectCompatibilityTables(lua_State* state, const std::vector<unsigned char>& bytes)
{
	if (!state || bytes.empty()) {
		return;
	}

	std::unordered_set<std::string> seenTokens;
	std::string token;
	auto flushToken = [&]() {
		if (token.empty()) {
			return;
		}

		if (seenTokens.insert(token).second) {
			if (std::strncmp(token.c_str(), "EF_", 3) == 0) {
				if (!HasLuaTableIntegerKey(state, "EFID", token.c_str())) {
					const int value = AllocateSyntheticLuaTableValue("EFID", token.c_str());
					if (value != 0) {
						SetLuaTableIntegerKey(state, "EFID", token.c_str(), value);
					}
				}
			} else if (std::strncmp(token.c_str(), "ST_", 3) == 0) {
				if (!HasLuaTableIntegerKey(state, "ACTOR_STATE", token.c_str())) {
					const int value = AllocateSyntheticLuaTableValue("ACTOR_STATE", token.c_str());
					if (value != 0) {
						SetLuaTableIntegerKey(state, "ACTOR_STATE", token.c_str(), value);
					}
				}
			} else if (ShouldTreatAsSkillEnumToken(token)) {
				if (!HasLuaTableIntegerKey(state, "SKID", token.c_str())) {
					const int value = AllocateSyntheticLuaTableValue("SKID", token.c_str());
					if (value != 0) {
						SetLuaTableIntegerKey(state, "SKID", token.c_str(), value);
					}
				}
			}
		}

		token.clear();
	};

	for (unsigned char byte : bytes) {
		const char ch = static_cast<char>(byte);
		const bool isTokenChar = (ch >= 'A' && ch <= 'Z')
			|| (ch >= '0' && ch <= '9')
			|| ch == '_';
		if (isTokenChar) {
			token.push_back(ch);
		} else {
			flushToken();
		}
	}
	flushToken();
}

void MergeGeneratedJobCompatibility(lua_State* state);
void AddPcJobVariantAliases(lua_State* state);
void SetJobTblEntry(lua_State* state, const char* nameKey, int value);

void PopulateJobEnumCompatibilityGlobals(lua_State* state, const std::vector<unsigned char>& bytes)
{
	if (!state || bytes.empty()) {
		return;
	}

	std::unordered_set<std::string> seenTokens;
	std::string token;
	auto flushToken = [&]() {
		if (token.empty()) {
			return;
		}

		if (seenTokens.insert(token).second && std::strncmp(token.c_str(), "JT_", 3) == 0) {
			int existingValue = 0;
			if (!TryGetLuaGlobalInteger(state, token.c_str(), &existingValue)) {
				int value = 0;
				if (!TryGetGeneratedJobIdFromGlobalName(token.c_str(), &value)) {
					value = AllocateSyntheticLuaTableValue("JT_ENUM", token.c_str());
				}
				if (value != 0) {
					SetLuaGlobalInteger(state, token.c_str(), value);
					SetJobTblEntry(state, token.c_str(), value);
					DbgLog("[Lua] Synthesizing missing global %s=%d before loading job-name data.\n",
						token.c_str(),
						value);
				}
			} else {
				SetJobTblEntry(state, token.c_str(), existingValue);
			}
		}

		token.clear();
	};

	for (unsigned char byte : bytes) {
		const char ch = static_cast<char>(byte);
		const bool isTokenChar = (ch >= 'A' && ch <= 'Z')
			|| (ch >= '0' && ch <= '9')
			|| ch == '_';
		if (isTokenChar) {
			token.push_back(ch);
		} else {
			flushToken();
		}
	}
	flushToken();
}

void PrepareGeneratedJobEnumCompatibility(lua_State* state)
{
	if (!state) {
		return;
	}

	MergeGeneratedJobCompatibility(state);
	AddPcJobVariantAliases(state);
}

void EnsurePcJobTbl2Table(lua_State* state)
{
	lua_getglobal(state, "pcJobTbl2");
	if (lua_istable(state, -1)) {
		lua_pop(state, 1);
		return;
	}

	lua_pop(state, 1);
	lua_newtable(state);
	lua_setglobal(state, "pcJobTbl2");
}

void SetPcJobTbl2Entry(lua_State* state, const char* nameKey, int value)
{
	if (!state || !nameKey || !*nameKey) {
		return;
	}

	EnsurePcJobTbl2Table(state);
	lua_getglobal(state, "pcJobTbl2");
	lua_pushstring(state, nameKey);
	lua_pushinteger(state, value);
	lua_settable(state, -3);
	lua_pop(state, 1);
}

void EnsureJobTblTable(lua_State* state)
{
	lua_getglobal(state, "jobtbl");
	if (lua_istable(state, -1)) {
		lua_pop(state, 1);
		return;
	}

	lua_pop(state, 1);
	lua_newtable(state);
	lua_setglobal(state, "jobtbl");
}

void SetJobTblEntry(lua_State* state, const char* nameKey, int value)
{
	if (!state || !nameKey || !*nameKey) {
		return;
	}

	EnsureJobTblTable(state);
	lua_getglobal(state, "jobtbl");
	lua_pushstring(state, nameKey);
	lua_pushinteger(state, value);
	lua_settable(state, -3);
	lua_pop(state, 1);
}

void MergePcJobIdentityCompatibility(lua_State* state)
{
	if (!state) {
		return;
	}

	EnsurePcJobTbl2Table(state);
	lua_getglobal(state, "pcJobTbl");
	if (!lua_istable(state, -1)) {
		lua_pop(state, 1);
		return;
	}

	lua_pushnil(state);
	while (lua_next(state, -2) != 0) {
		if (lua_isstring(state, -2) && lua_isnumber(state, -1)) {
			const char* key = lua_tostring(state, -2);
			const int value = static_cast<int>(lua_tointeger(state, -1));

			SetLuaGlobalInteger(state, key, value);
			SetPcJobTbl2Entry(state, key, value);
		}

		lua_pop(state, 1);
	}

	lua_pop(state, 1);
}

void MergeGeneratedJobCompatibility(lua_State* state)
{
	if (!state) {
		return;
	}

	for (const JobNameEntry& entry : kGeneratedJobNames) {
		std::string globalName = "JT_";
		globalName += entry.name;

		SetLuaGlobalInteger(state, globalName.c_str(), entry.job);

		SetJobTblEntry(state, globalName.c_str(), entry.job);
		SetPcJobTbl2Entry(state, globalName.c_str(), entry.job);
	}
}

void AddPcJobVariantAliases(lua_State* state)
{
	if (!state) {
		return;
	}

	struct VariantAlias {
		const char* aliasName;
		const char* baseName;
	};

	static const VariantAlias kAliases[] = {
		{ "JT_RUNE_CHICKEN", "JT_RUNE_KNIGHT" },
		{ "JT_RUNE_CHICKEN_H", "JT_RUNE_KNIGHT_H" },
		{ "JT_ROYAL_CHICKEN", "JT_ROYAL_GUARD" },
		{ "JT_ROYAL_CHICKEN_H", "JT_ROYAL_GUARD_H" },
		{ "JT_WOLF_RANGER", "JT_RANGER" },
		{ "JT_WOLF_RANGER_H", "JT_RANGER_H" },
		{ "JT_MADOGEAR", "JT_MECHANIC" },
		{ "JT_MADOGEAR_H", "JT_MECHANIC_H" },
		{ "JT_RUNE_CHICKEN2", "JT_RUNE_KNIGHT" },
		{ "JT_RUNE_CHICKEN2_H", "JT_RUNE_KNIGHT_H" },
		{ "JT_RUNE_CHICKEN3", "JT_RUNE_KNIGHT" },
		{ "JT_RUNE_CHICKEN3_H", "JT_RUNE_KNIGHT_H" },
		{ "JT_RUNE_CHICKEN4", "JT_RUNE_KNIGHT" },
		{ "JT_RUNE_CHICKEN4_H", "JT_RUNE_KNIGHT_H" },
		{ "JT_RUNE_CHICKEN5", "JT_RUNE_KNIGHT" },
		{ "JT_RUNE_CHICKEN5_H", "JT_RUNE_KNIGHT_H" },
		{ "JT_RUNE_CHICKEN_B", "JT_RUNE_KNIGHT_B" },
		{ "JT_ROYAL_CHICKEN_B", "JT_ROYAL_GUARD_B" },
		{ "JT_WOLF_RANGER_B", "JT_RANGER_B" },
		{ "JT_MADOGEAR_B", "JT_MECHANIC_B" },
	};

	for (const VariantAlias& alias : kAliases) {
		int baseValue = 0;
		if (!TryGetLuaGlobalInteger(state, alias.baseName, &baseValue)) {
			continue;
		}

		SetLuaGlobalInteger(state, alias.aliasName, baseValue);
		SetJobTblEntry(state, alias.aliasName, baseValue);
		SetPcJobTbl2Entry(state, alias.aliasName, baseValue);
	}
}

void PrepareJobNameCompatibility(lua_State* state)
{
	if (!state) {
		return;
	}

	MergeGeneratedJobCompatibility(state);
	AddPcJobVariantAliases(state);

	lua_getglobal(state, "pcJobTbl");
	if (!lua_istable(state, -1)) {
		lua_pop(state, 1);
		return;
	}

	lua_pushnil(state);
	while (lua_next(state, -2) != 0) {
		if (lua_isstring(state, -2) && lua_isnumber(state, -1)) {
			const char* key = lua_tostring(state, -2);
			const int value = static_cast<int>(lua_tointeger(state, -1));
			SetJobTblEntry(state, key, value);
		}

		lua_pop(state, 1);
	}

	lua_pop(state, 1);
}

void PreparePcJobNameGenderCompatibility(lua_State* state)
{
	if (!state) {
		return;
	}

	MergePcJobIdentityCompatibility(state);
	MergeGeneratedJobCompatibility(state);
	AddPcJobVariantAliases(state);
}

void EnsureSkillIdTable(lua_State* state)
{
	if (!state) {
		return;
	}

	lua_getglobal(state, "SKID");
	if (lua_istable(state, -1)) {
		lua_pop(state, 1);
		return;
	}

	lua_pop(state, 1);
	lua_newtable(state);
	for (const SkillEnumEntry& entry : kSkillEnumEntries) {
		if (!entry.name || !*entry.name) {
			continue;
		}
		lua_pushstring(state, entry.name);
		lua_pushinteger(state, entry.id);
		lua_settable(state, -3);
	}
	lua_setglobal(state, "SKID");
}

bool ReadLocalFileBytes(const std::filesystem::path& path, std::vector<unsigned char>* outBytes)
{
	if (!outBytes) {
		return false;
	}

	std::error_code error;
	if (!std::filesystem::exists(path, error) || error) {
		return false;
	}

	FILE* file = std::fopen(path.string().c_str(), "rb");
	if (!file) {
		return false;
	}

	if (std::fseek(file, 0, SEEK_END) != 0) {
		std::fclose(file);
		return false;
	}

	const long size = std::ftell(file);
	if (size < 0 || std::fseek(file, 0, SEEK_SET) != 0) {
		std::fclose(file);
		return false;
	}

	outBytes->resize(static_cast<size_t>(size));
	const size_t bytesRead = std::fread(outBytes->data(), 1, outBytes->size(), file);
	std::fclose(file);
	if (bytesRead != outBytes->size()) {
		outBytes->clear();
		return false;
	}

	return true;
}

} // namespace

CLuaBridge g_buabridge;

CLuaBridge::CLuaBridge()
	: m_state(nullptr)
{
}

CLuaBridge::~CLuaBridge()
{
	Shutdown();
}

bool CLuaBridge::Initialize()
{
	if (m_state) {
		return true;
	}

	m_state = luaL_newstate();
	if (!m_state) {
		m_lastError = "luaL_newstate failed";
		DbgLog("[Lua] Failed to create Lua 5.1 state.\n");
		return false;
	}

	luaL_openlibs(m_state);
	m_lastError.clear();
	DbgLog("[Lua] Initialized Lua 5.1 runtime.\n");
	return true;
}

void CLuaBridge::Shutdown()
{
	if (m_state) {
		lua_close(m_state);
		m_state = nullptr;
	}
	m_loadedScripts.clear();
	m_skillEffectInfoCache.clear();
	m_missingSkillEffectInfoIds.clear();
	m_lastError.clear();
}

bool CLuaBridge::IsInitialized() const
{
	return m_state != nullptr;
}

std::string CLuaBridge::NormalizePath(const char* path)
{
	if (!path || !*path) {
		return std::string();
	}

	return NormalizeSlashPath(path);
}

std::string CLuaBridge::ResolveLocalLuaPath(const char* relativePath) const
{
	const std::string normalized = NormalizePath(relativePath);
	if (normalized.empty()) {
		return std::string();
	}

	std::error_code error;
	const std::filesystem::path current = std::filesystem::current_path(error);
	if (error) {
		return std::string();
	}

	const std::filesystem::path direct = current / normalized;
	if (std::filesystem::exists(direct, error) && !error) {
		return direct.string();
	}

	const std::filesystem::path lua514 = current / "data" / "luafiles514" / normalized;
	if (std::filesystem::exists(lua514, error) && !error) {
		return lua514.string();
	}

	return std::string();
}

bool CLuaBridge::ReadRagnarokScriptBytes(const char* relativePath,
	std::vector<unsigned char>* outBytes,
	std::string* outSourcePath,
	bool* outLoadedFromLocal) const
{
	if (!outBytes || !outSourcePath || !outLoadedFromLocal) {
		return false;
	}

	outBytes->clear();
	outSourcePath->clear();
	*outLoadedFromLocal = false;

	const std::string normalized = NormalizePath(relativePath);
	if (normalized.empty()) {
		return false;
	}

	const std::string localPath = ResolveLocalLuaPath(normalized.c_str());
	if (!localPath.empty() && ReadLocalFileBytes(std::filesystem::path(localPath), outBytes)) {
		*outSourcePath = localPath;
		*outLoadedFromLocal = true;
		return true;
	}

	std::vector<std::string> fallbackCandidates;
	fallbackCandidates.push_back(normalized);

	const char* const lua514Prefix = "data\\luafiles514\\";
	if (normalized.size() > std::strlen(lua514Prefix)
		&& _strnicmp(normalized.c_str(), lua514Prefix, std::strlen(lua514Prefix)) == 0) {
		fallbackCandidates.push_back(normalized.substr(std::strlen(lua514Prefix)));
	} else {
		fallbackCandidates.push_back(std::string(lua514Prefix) + normalized);
	}

	if (_strnicmp(normalized.c_str(), "data\\", 5) == 0) {
		fallbackCandidates.push_back(normalized.substr(5));
	} else {
		fallbackCandidates.push_back(std::string("data\\") + normalized);
	}

	for (const std::string& candidate : fallbackCandidates) {
		int size = 0;
		unsigned char* data = g_fileMgr.GetData(candidate.c_str(), &size);
		if (!data || size <= 0) {
			delete[] data;
			continue;
		}

		outBytes->assign(data, data + size);
		delete[] data;
		*outSourcePath = candidate;
		*outLoadedFromLocal = false;
		return true;
	}

	return false;
}

bool CLuaBridge::ExecuteBuffer(const unsigned char* bytes, size_t size, const char* chunkName)
{
	if (!Initialize()) {
		return false;
	}
	if (!bytes || size == 0) {
		m_lastError = "empty Lua buffer";
		return false;
	}

	const int loadResult = luaL_loadbuffer(
		m_state,
		reinterpret_cast<const char*>(bytes),
		size,
		chunkName ? chunkName : "=(buffer)");
	if (loadResult != 0) {
		const char* errorText = lua_tostring(m_state, -1);
		m_lastError = errorText ? errorText : "luaL_loadbuffer failed";
		DbgLog("[Lua] Load failed for '%s': %s\n", chunkName ? chunkName : "(buffer)", m_lastError.c_str());
		lua_settop(m_state, 0);
		return false;
	}

	const int callResult = lua_pcall(m_state, 0, LUA_MULTRET, 0);
	if (callResult != 0) {
		const char* errorText = lua_tostring(m_state, -1);
		m_lastError = errorText ? errorText : "lua_pcall failed";
		DbgLog("[Lua] Execution failed for '%s': %s\n", chunkName ? chunkName : "(buffer)", m_lastError.c_str());
		lua_settop(m_state, 0);
		return false;
	}

	lua_settop(m_state, 0);
	m_lastError.clear();
	return true;
}

bool CLuaBridge::LoadRagnarokScript(const char* relativePath)
{
	const std::string normalized = NormalizePath(relativePath);
	if (IsAccessoryNameTableScript(normalized)) {
		LoadRagnarokScriptOnce("lua files\\datainfo\\accessoryid.lub");
	}

	if (PathEndsWithInsensitive(normalized, "lua files\\datainfo\\jobname.lub")
		|| PathEndsWithInsensitive(normalized, "lua files\\datainfo\\jobname_f.lub")
		|| PathEndsWithInsensitive(normalized, "lua files\\admin\\pcjobname.lub")
		|| PathEndsWithInsensitive(normalized, "lua files\\datainfo\\pcjobnamegender.lub")
		|| PathEndsWithInsensitive(normalized, "lua files\\datainfo\\pcjobnamegender_f.lub")) {
		PrepareGeneratedJobEnumCompatibility(m_state);
	}

	if (PathEndsWithInsensitive(normalized, "lua files\\datainfo\\jobname.lub")
		|| PathEndsWithInsensitive(normalized, "lua files\\datainfo\\jobname_f.lub")) {
		PrepareJobNameCompatibility(m_state);
	}

	if (PathEndsWithInsensitive(normalized, "lua files\\datainfo\\pcjobnamegender.lub")
		|| PathEndsWithInsensitive(normalized, "lua files\\datainfo\\pcjobnamegender_f.lub")) {
		LoadRagnarokScriptOnce("lua files\\admin\\pcidentity.lub");
		PreparePcJobNameGenderCompatibility(m_state);
	}

	std::vector<unsigned char> bytes;
	std::string sourcePath;
	bool loadedFromLocal = false;
	if (!ReadRagnarokScriptBytes(relativePath, &bytes, &sourcePath, &loadedFromLocal)) {
		m_lastError = "unable to resolve Lua/LUB script";
		DbgLog("[Lua] Failed to resolve script '%s'.\n", relativePath ? relativePath : "(null)");
		return false;
	}

	if (PathEndsWithInsensitive(normalized, "lua files\\datainfo\\jobname.lub")
		|| PathEndsWithInsensitive(normalized, "lua files\\datainfo\\jobname_f.lub")
		|| PathEndsWithInsensitive(normalized, "lua files\\admin\\pcjobname.lub")
		|| PathEndsWithInsensitive(normalized, "lua files\\datainfo\\pcjobnamegender.lub")
		|| PathEndsWithInsensitive(normalized, "lua files\\datainfo\\pcjobnamegender_f.lub")) {
		PopulateJobEnumCompatibilityGlobals(m_state, bytes);
	}

	const bool executed = ExecuteBuffer(bytes.data(), bytes.size(), sourcePath.c_str());
	if (executed) {
		DbgLog("[Lua] Loaded %s script: %s\n", loadedFromLocal ? "local" : "fallback", sourcePath.c_str());
	}
	return executed;
}

bool CLuaBridge::LoadRagnarokScriptOnce(const char* relativePath)
{
	const std::string normalized = NormalizePath(relativePath);
	if (normalized.empty()) {
		m_lastError = "empty Lua path";
		return false;
	}

	if (HasLoadedScript(normalized.c_str())) {
		return true;
	}

	if (!LoadRagnarokScript(normalized.c_str())) {
		return false;
	}

	m_loadedScripts.push_back(normalized);
	return true;
}

bool CLuaBridge::HasLoadedScript(const char* relativePath) const
{
	const std::string normalized = NormalizePath(relativePath);
	return std::find(m_loadedScripts.begin(), m_loadedScripts.end(), normalized) != m_loadedScripts.end();
}

bool CLuaBridge::GetGlobalTableIntegerByIntegerKey(const char* tableName, int numericKey, int* outValue)
{
	if (!outValue) {
		return false;
	}
	*outValue = 0;

	if (!Initialize()) {
		return false;
	}
	if (!tableName || !*tableName) {
		m_lastError = "empty Lua table name";
		return false;
	}

	lua_getglobal(m_state, tableName);
	if (!lua_istable(m_state, -1)) {
		lua_settop(m_state, 0);
		m_lastError = "Lua table not found";
		return false;
	}

	lua_pushinteger(m_state, numericKey);
	lua_gettable(m_state, -2);
	if (!lua_isnumber(m_state, -1)) {
		lua_settop(m_state, 0);
		m_lastError = "Lua table value is not a number";
		return false;
	}

	*outValue = static_cast<int>(lua_tointeger(m_state, -1));
	lua_settop(m_state, 0);
	m_lastError.clear();
	return true;
}

bool CLuaBridge::GetGlobalTableIntegerByStringKey(const char* tableName, const char* stringKey, int* outValue)
{
	if (!outValue) {
		return false;
	}
	*outValue = 0;

	if (!Initialize()) {
		return false;
	}
	if (!tableName || !*tableName) {
		m_lastError = "empty Lua table name";
		return false;
	}
	if (!stringKey || !*stringKey) {
		m_lastError = "empty Lua string key";
		return false;
	}

	lua_getglobal(m_state, tableName);
	if (!lua_istable(m_state, -1)) {
		lua_settop(m_state, 0);
		m_lastError = "Lua table not found";
		return false;
	}

	lua_pushstring(m_state, stringKey);
	lua_gettable(m_state, -2);
	if (!lua_isnumber(m_state, -1)) {
		lua_settop(m_state, 0);
		m_lastError = "Lua table value is not a number";
		return false;
	}

	*outValue = static_cast<int>(lua_tointeger(m_state, -1));
	lua_settop(m_state, 0);
	m_lastError.clear();
	return true;
}

bool CLuaBridge::GetGlobalTableNestedStringByIntegerKey(const char* tableName,
	int numericKey,
	const char* nestedStringKey,
	std::string* outValue)
{
	if (!outValue) {
		return false;
	}
	outValue->clear();

	if (!Initialize()) {
		return false;
	}
	if (!tableName || !*tableName) {
		m_lastError = "empty Lua table name";
		return false;
	}
	if (!nestedStringKey || !*nestedStringKey) {
		m_lastError = "empty nested Lua string key";
		return false;
	}

	lua_getglobal(m_state, tableName);
	if (!lua_istable(m_state, -1)) {
		lua_settop(m_state, 0);
		m_lastError = "Lua table not found";
		return false;
	}

	lua_pushinteger(m_state, numericKey);
	lua_gettable(m_state, -2);
	if (!lua_istable(m_state, -1)) {
		lua_settop(m_state, 0);
		m_lastError = "Lua nested table not found";
		return false;
	}

	lua_pushstring(m_state, nestedStringKey);
	lua_gettable(m_state, -2);
	if (!lua_isstring(m_state, -1)) {
		lua_settop(m_state, 0);
		m_lastError = "Lua nested table value is not a string";
		return false;
	}

	const char* value = lua_tostring(m_state, -1);
	if (!value) {
		lua_settop(m_state, 0);
		m_lastError = "Lua string conversion failed";
		return false;
	}

	outValue->assign(value);
	lua_settop(m_state, 0);
	m_lastError.clear();
	return true;
}

bool CLuaBridge::GetGlobalTableStringByIntegerKey(const char* tableName, int numericKey, std::string* outValue)
{
	if (!outValue) {
		return false;
	}
	outValue->clear();

	if (!Initialize()) {
		return false;
	}
	if (!tableName || !*tableName) {
		m_lastError = "empty Lua table name";
		return false;
	}

	lua_getglobal(m_state, tableName);
	if (!lua_istable(m_state, -1)) {
		lua_settop(m_state, 0);
		m_lastError = "Lua table not found";
		return false;
	}

	lua_pushinteger(m_state, numericKey);
	lua_gettable(m_state, -2);
	if (!lua_isstring(m_state, -1)) {
		lua_settop(m_state, 0);
		m_lastError = "Lua table value is not a string";
		return false;
	}

	const char* value = lua_tostring(m_state, -1);
	if (!value) {
		lua_settop(m_state, 0);
		m_lastError = "Lua string conversion failed";
		return false;
	}

	outValue->assign(value);
	lua_settop(m_state, 0);
	m_lastError.clear();
	return true;
}

bool CLuaBridge::GetGlobalTableStringByStringKey(const char* tableName, const char* stringKey, std::string* outValue)
{
	if (!outValue) {
		return false;
	}
	outValue->clear();

	if (!Initialize()) {
		return false;
	}
	if (!tableName || !*tableName) {
		m_lastError = "empty Lua table name";
		return false;
	}
	if (!stringKey || !*stringKey) {
		m_lastError = "empty Lua string key";
		return false;
	}

	lua_getglobal(m_state, tableName);
	if (!lua_istable(m_state, -1)) {
		lua_settop(m_state, 0);
		m_lastError = "Lua table not found";
		return false;
	}

	lua_pushstring(m_state, stringKey);
	lua_gettable(m_state, -2);
	if (!lua_isstring(m_state, -1)) {
		lua_settop(m_state, 0);
		m_lastError = "Lua table value is not a string";
		return false;
	}

	const char* value = lua_tostring(m_state, -1);
	if (!value) {
		lua_settop(m_state, 0);
		m_lastError = "Lua string conversion failed";
		return false;
	}

	outValue->assign(value);
	lua_settop(m_state, 0);
	m_lastError.clear();
	return true;
}

bool CLuaBridge::GetSkillEffectInfoBySkillId(int skillId, LuaSkillEffectInfo* outInfo)
{
	if (outInfo) {
		*outInfo = LuaSkillEffectInfo{};
	}
	if (skillId <= 0) {
		return false;
	}

	const auto cacheIt = m_skillEffectInfoCache.find(skillId);
	if (cacheIt != m_skillEffectInfoCache.end()) {
		if (outInfo) {
			*outInfo = cacheIt->second;
		}
		return true;
	}
	if (m_missingSkillEffectInfoIds.find(skillId) != m_missingSkillEffectInfoIds.end()) {
		return false;
	}

	if (!Initialize()) {
		return false;
	}

	EnsureSkillIdTable(m_state);

	if (!LoadRagnarokScriptOnce("lua files\\skilleffectinfo\\actorstate.lub")
		|| !LoadRagnarokScriptOnce("lua files\\skilleffectinfo\\skilleffectinfo_f.lub")
		|| !LoadRagnarokScriptOnce("lua files\\skilleffectinfo\\EffectID.lub")) {
		return false;
	}

	std::vector<unsigned char> skillEffectListBytes;
	std::string skillEffectListSourcePath;
	bool skillEffectListLoadedFromLocal = false;
	if (ReadRagnarokScriptBytes("lua files\\skilleffectinfo\\SkillEffectInfoList.lub",
		&skillEffectListBytes,
		&skillEffectListSourcePath,
		&skillEffectListLoadedFromLocal)) {
		PopulateSkillEffectCompatibilityTables(m_state, skillEffectListBytes);
	}

	if (!LoadRagnarokScriptOnce("lua files\\skilleffectinfo\\SkillEffectInfoList.lub")) {
		return false;
	}

	lua_getglobal(m_state, "SKILL_EFFECT_INFO_LIST");
	if (!lua_istable(m_state, -1)) {
		lua_settop(m_state, 0);
		m_lastError = "SKILL_EFFECT_INFO_LIST not found";
		return false;
	}

	LuaSkillEffectInfo info;
	bool found = false;
	lua_pushnil(m_state);
	while (lua_next(m_state, -2) != 0) {
		if (lua_istable(m_state, -1)) {
			int entrySkillId = 0;
			if (TryGetLuaTableIntegerField(m_state, -1, "SKID", &entrySkillId) && entrySkillId == skillId) {
				info.hasBeginEffectId = TryGetLuaTableIntegerField(m_state, -1, "beginEffectID", &info.beginEffectId);
				info.hasBeginMotionType = TryGetLuaTableIntegerField(m_state, -1, "beginMotionType", &info.beginMotionType);
				info.hasTargetEffectId = TryGetLuaTableIntegerField(m_state, -1, "targetEffectID", &info.targetEffectId);
				info.hasGroundEffectId = TryGetLuaTableIntegerField(m_state, -1, "groundEffectID", &info.groundEffectId);
				TryGetLuaTableIntegerArrayField(m_state, -1, "effectIDs", &info.effectIds);
				info.hasOnTarget = TryGetLuaTableBooleanField(m_state, -1, "onTarget", &info.onTarget);
				info.hasLaunchUseSkill = TryGetLuaTableBooleanField(m_state, -1, "LaunchZC_USE_SKILL", &info.launchUseSkill);
				found = true;
				lua_pop(m_state, 1);
				break;
			}
		}

		lua_pop(m_state, 1);
	}

	lua_settop(m_state, 0);
	if (!found) {
		m_missingSkillEffectInfoIds.insert(skillId);
		return false;
	}

	m_skillEffectInfoCache.emplace(skillId, info);
	if (outInfo) {
		*outInfo = info;
	}
	return true;
}

const std::string& CLuaBridge::GetLastError() const
{
	return m_lastError;
}
