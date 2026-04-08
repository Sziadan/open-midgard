#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct lua_State;

struct LuaSkillEffectInfo {
    bool hasBeginEffectId = false;
    int beginEffectId = -1;
    bool hasBeginMotionType = false;
    int beginMotionType = -1;
    bool hasTargetEffectId = false;
    int targetEffectId = -1;
    bool hasGroundEffectId = false;
    int groundEffectId = -1;
    std::vector<int> effectIds;
    bool hasOnTarget = false;
    bool onTarget = false;
    bool hasLaunchUseSkill = false;
    bool launchUseSkill = true;
};

class CLuaBridge
{
public:
    CLuaBridge();
    ~CLuaBridge();

    bool Initialize();
    void Shutdown();
    bool IsInitialized() const;

    bool LoadRagnarokScript(const char* relativePath);
    bool LoadRagnarokScriptOnce(const char* relativePath);
    bool HasLoadedScript(const char* relativePath) const;
    bool GetGlobalTableIntegerByIntegerKey(const char* tableName, int numericKey, int* outValue);
    bool GetGlobalTableIntegerByStringKey(const char* tableName, const char* stringKey, int* outValue);
    bool GetGlobalTableNestedStringByIntegerKey(const char* tableName, int numericKey, const char* nestedStringKey, std::string* outValue);
    bool GetGlobalTableStringByIntegerKey(const char* tableName, int numericKey, std::string* outValue);
    bool GetGlobalTableStringByStringKey(const char* tableName, const char* stringKey, std::string* outValue);
    bool GetSkillEffectInfoBySkillId(int skillId, LuaSkillEffectInfo* outInfo);

    const std::string& GetLastError() const;
    std::string ResolveLocalLuaPath(const char* relativePath) const;

private:
    static std::string NormalizePath(const char* path);
    bool ReadRagnarokScriptBytes(const char* relativePath,
        std::vector<unsigned char>* outBytes,
        std::string* outSourcePath,
        bool* outLoadedFromLocal) const;
    bool ExecuteBuffer(const unsigned char* bytes, size_t size, const char* chunkName);

    lua_State* m_state;
    std::vector<std::string> m_loadedScripts;
    std::unordered_map<int, LuaSkillEffectInfo> m_skillEffectInfoCache;
    std::unordered_set<int> m_missingSkillEffectInfoIds;
    std::string m_lastError;
};

extern CLuaBridge g_buabridge;
