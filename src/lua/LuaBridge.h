#pragma once

#include <string>
#include <vector>

struct lua_State;

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
    bool GetGlobalTableStringByIntegerKey(const char* tableName, int numericKey, std::string* outValue);

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
    std::string m_lastError;
};

extern CLuaBridge g_buabridge;
