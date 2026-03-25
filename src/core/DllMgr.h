#pragma once
// windows.h must precede STL headers so struct lconv is in global namespace
// before <clocale> (pulled in by <string>/<map>) does 'using ::lconv'.
#include "DllProtos.h"  // includes Types.h -> windows.h
#include <string>
#include <map>

//===========================================================================
// CDllMgr  –  Handles dynamic loading of supporting DLLs
//===========================================================================
class CDllMgr {
public:
    // Phase 3: Load all RO supporting DLLs and resolve their exports
    static bool LoadAll();

    // Low-level DLL management
    static bool Load(const std::string& dllName);
    static void Unload(const std::string& dllName);
    static void UnloadAll();
    static void* GetProc(const std::string& dllName, const std::string& procName);

private:
    static std::map<std::string, HMODULE> m_dlls;
};

extern DLL_EXPORTS g_dllExports;
