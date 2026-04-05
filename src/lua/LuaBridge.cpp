#include "LuaBridge.h"

#include "core/File.h"
#include "DebugLog.h"

struct JobNameEntry {
	int job;
	const char* name;
};

#include "../session/JobNameTable.generated.inc"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>

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
	lua_pushinteger(state, value);
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

		int existingValue = 0;
		if (!TryGetLuaGlobalInteger(state, globalName.c_str(), &existingValue)) {
			SetLuaGlobalInteger(state, globalName.c_str(), entry.job);
		}

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
		SetPcJobTbl2Entry(state, alias.aliasName, baseValue);
	}
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

const std::string& CLuaBridge::GetLastError() const
{
	return m_lastError;
}
