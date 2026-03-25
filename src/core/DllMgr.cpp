#include "DllMgr.h"   // must come first: pulls in Types.h → windows.h before STL locale headers
#include <vector>

namespace {

std::string GetExeDirectory()
{
    char modulePath[MAX_PATH] = {};
    const DWORD len = GetModuleFileNameA(nullptr, modulePath, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        return std::string();
    }

    for (int i = static_cast<int>(len) - 1; i >= 0; --i) {
        if (modulePath[i] == '\\' || modulePath[i] == '/') {
            modulePath[i] = '\0';
            break;
        }
    }
    return std::string(modulePath);
}

std::string GetParentDirectory(const std::string& path)
{
    if (path.empty()) {
        return std::string();
    }

    size_t pos = path.find_last_of("\\/");
    if (pos == std::string::npos) {
        return std::string();
    }
    return path.substr(0, pos);
}

} // namespace

// Global exports structure
DLL_EXPORTS g_dllExports = { 0 };

// Static member definition
std::map<std::string, HMODULE> CDllMgr::m_dlls;

bool CDllMgr::Load(const std::string& dllName) {
    if (m_dlls.find(dllName) != m_dlls.end()) return true;

    HMODULE hMod = nullptr;

    const std::string exeDir = GetExeDirectory();
    if (!exeDir.empty() && !hMod) {
        // 1) Side-by-side deployment: next to HighPriest.exe.
        std::string sideBySide = exeDir + "\\" + dllName;
        hMod = LoadLibraryA(sideBySide.c_str());
    }

    if (!exeDir.empty() && !hMod) {
        // 2) Output-local folder: <exe>/dlls
        std::string localDlls = exeDir + "\\dlls\\" + dllName;
        hMod = LoadLibraryA(localDlls.c_str());
    }

    if (!exeDir.empty() && !hMod) {
        // 3) Dev-tree fallback: <exe>/../dlls
        std::string parentDir = GetParentDirectory(exeDir);
        if (!parentDir.empty()) {
            std::string parentDlls = parentDir + "\\dlls\\" + dllName;
            hMod = LoadLibraryA(parentDlls.c_str());
        }
    }

    if (!exeDir.empty() && !hMod) {
        // 4) Dev-tree fallback: <exe>/../../dlls (e.g. build/Debug -> repo/dlls)
        std::string parentDir = GetParentDirectory(exeDir);
        std::string grandParentDir = GetParentDirectory(parentDir);
        if (!grandParentDir.empty()) {
            std::string grandParentDlls = grandParentDir + "\\dlls\\" + dllName;
            hMod = LoadLibraryA(grandParentDlls.c_str());
        }
    }

    if (!hMod) {
        // Keep plain-name load for PATH/system DLLs.
        hMod = LoadLibraryA(dllName.c_str());
    }

    if (!hMod) {
        // Legacy working-directory fallback.
        std::string fallback = "dlls\\" + dllName;
        hMod = LoadLibraryA(fallback.c_str());
    }

    if (hMod) {
        m_dlls[dllName] = hMod;
        return true;
    }
    return false;
}

void CDllMgr::Unload(const std::string& dllName) {
    auto it = m_dlls.find(dllName);
    if (it != m_dlls.end()) {
        FreeLibrary(it->second);
        m_dlls.erase(it);
    }
}

void CDllMgr::UnloadAll() {
    for (auto& pair : m_dlls) {
        FreeLibrary(pair.second);
    }
    m_dlls.clear();
}

void* CDllMgr::GetProc(const std::string& dllName, const std::string& procName) {
    auto it = m_dlls.find(dllName);
    if (it == m_dlls.end()) return nullptr;
    return (void*)GetProcAddress(it->second, procName.c_str());
}

bool CDllMgr::LoadAll() {
    // 1. Load the actual DLL files
    std::vector<std::string> requiredDlls = {
        "granny2.dll", "Mss32.dll", "binkw32.dll", "ijl15.dll", "cps.dll", "dbghelp.dll"
    };

    bool allLoaded = true;
    for (const auto& dll : requiredDlls) {
        if (!Load(dll)) {
            allLoaded = false;
        }
    }

    // 2. Resolve exports into g_dllExports structure
    const char* GR = "granny2.dll";
    g_dllExports.GrannyReadEntireFileFromMemory = (P_GrannyReadEntireFileFromMemory)GetProc(GR, "GrannyReadEntireFileFromMemory");
    g_dllExports.GrannyGetFileInfo = (P_GrannyGetFileInfo)GetProc(GR, "GrannyGetFileInfo");
    g_dllExports.GrannyFreeFile = (P_GrannyFreeFile)GetProc(GR, "GrannyFreeFile");
    g_dllExports.GrannyGetWorldPoseComposite4x4Array = (P_GrannyGetWorldPoseComposite4x4Array)GetProc(GR, "GrannyGetWorldPoseComposite4x4Array");

    const char* MSS = "Mss32.dll";
    auto getMilesProc = [&](const char* decorated, const char* undecorated) -> void* {
        void* proc = nullptr;
        if (decorated && *decorated) {
            proc = GetProc(MSS, decorated);
        }
        if (!proc && undecorated && *undecorated) {
            proc = GetProc(MSS, undecorated);
        }
        return proc;
    };

    g_dllExports.AIL_startup = (P_AIL_startup)getMilesProc("_AIL_startup@0", "AIL_startup");
    g_dllExports.AIL_shutdown = (P_AIL_shutdown)getMilesProc("_AIL_shutdown@0", "AIL_shutdown");
    g_dllExports.AIL_set_redist_directory = (P_AIL_set_redist_directory)getMilesProc("_AIL_set_redist_directory@4", "AIL_set_redist_directory");
    g_dllExports.AIL_set_preference = (P_AIL_set_preference)getMilesProc("_AIL_set_preference@8", "AIL_set_preference");
    g_dllExports.AIL_open_digital_driver = (P_AIL_open_digital_driver)getMilesProc("_AIL_open_digital_driver@16", "AIL_open_digital_driver");
    if (!g_dllExports.AIL_open_digital_driver) {
        g_dllExports.AIL_open_digital_driver = (P_AIL_open_digital_driver)getMilesProc("_AIL_open_digital_driver@44", "AIL_open_digital_driver");
    }
    g_dllExports.AIL_close_digital_driver = (P_AIL_close_digital_driver)getMilesProc("_AIL_close_digital_driver@4", "AIL_close_digital_driver");
    g_dllExports.AIL_allocate_sample_handle = (P_AIL_allocate_sample_handle)getMilesProc("_AIL_allocate_sample_handle@4", "AIL_allocate_sample_handle");
    g_dllExports.AIL_release_sample_handle = (P_AIL_release_sample_handle)getMilesProc("_AIL_release_sample_handle@4", "AIL_release_sample_handle");
    g_dllExports.AIL_init_sample = (P_AIL_init_sample)getMilesProc("_AIL_init_sample@4", "AIL_init_sample");
    g_dllExports.AIL_set_sample_file = (P_AIL_set_sample_file)getMilesProc("_AIL_set_sample_file@12", "AIL_set_sample_file");
    g_dllExports.AIL_set_sample_volume = (P_AIL_set_sample_volume)getMilesProc("_AIL_set_sample_volume@8", "AIL_set_sample_volume");
    g_dllExports.AIL_start_sample = (P_AIL_start_sample)getMilesProc("_AIL_start_sample@4", "AIL_start_sample");
    g_dllExports.AIL_stop_sample = (P_AIL_stop_sample)getMilesProc("_AIL_stop_sample@4", "AIL_stop_sample");
    g_dllExports.AIL_sample_status = (P_AIL_sample_status)getMilesProc("_AIL_sample_status@4", "AIL_sample_status");
    g_dllExports.AIL_end_sample = (P_AIL_end_sample)getMilesProc("_AIL_end_sample@4", "AIL_end_sample");
    g_dllExports.AIL_pause_stream = (P_AIL_pause_stream)getMilesProc("_AIL_pause_stream@8", "AIL_pause_stream");
    g_dllExports.AIL_open_stream = (P_AIL_open_stream)getMilesProc("_AIL_open_stream@12", "AIL_open_stream");
    g_dllExports.AIL_close_stream = (P_AIL_close_stream)getMilesProc("_AIL_close_stream@4", "AIL_close_stream");
    g_dllExports.AIL_set_stream_volume = (P_AIL_set_stream_volume)getMilesProc("_AIL_set_stream_volume@8", "AIL_set_stream_volume");
    g_dllExports.AIL_set_stream_loop_count = (P_AIL_set_stream_loop_count)getMilesProc("_AIL_set_stream_loop_count@8", "AIL_set_stream_loop_count");
    g_dllExports.AIL_start_stream = (P_AIL_start_stream)getMilesProc("_AIL_start_stream@4", "AIL_start_stream");
    g_dllExports.AIL_stream_volume = (P_AIL_stream_volume)getMilesProc("_AIL_stream_volume@4", "AIL_stream_volume");

    const char* BNK = "binkw32.dll";
    g_dllExports.BinkOpen = (P_BinkOpen)GetProc(BNK, "_BinkOpen@8");
    if (!g_dllExports.BinkOpen) g_dllExports.BinkOpen = (P_BinkOpen)GetProc(BNK, "BinkOpen");
    g_dllExports.BinkClose = (P_BinkClose)GetProc(BNK, "_BinkClose@4");
    if (!g_dllExports.BinkClose) g_dllExports.BinkClose = (P_BinkClose)GetProc(BNK, "BinkClose");
    g_dllExports.BinkDoFrame = (P_BinkDoFrame)GetProc(BNK, "_BinkDoFrame@4");
    g_dllExports.BinkNextFrame = (P_BinkNextFrame)GetProc(BNK, "_BinkNextFrame@4");
    g_dllExports.BinkWait = (P_BinkWait)GetProc(BNK, "_BinkWait@4");
    g_dllExports.BinkPause = (P_BinkPause)GetProc(BNK, "_BinkPause@8");
    g_dllExports.BinkCopyToBuffer = (P_BinkCopyToBuffer)GetProc(BNK, "_BinkCopyToBuffer@28");

    const char* IJL = "ijl15.dll";
    g_dllExports.ijlInit = (P_ijlInit)GetProc(IJL, "ijlInit");
    g_dllExports.ijlFree = (P_ijlFree)GetProc(IJL, "ijlFree");
    g_dllExports.ijlRead = (P_ijlRead)GetProc(IJL, "ijlRead");

    const char* CPS = "cps.dll";
    g_dllExports.uncompress = (P_uncompress)GetProc(CPS, "uncompress");

    const char* DBG = "dbghelp.dll";
    g_dllExports.SymInitialize = (P_SymInitialize)GetProc(DBG, "SymInitialize");
    g_dllExports.MiniDumpWriteDump = (P_MiniDumpWriteDump)GetProc(DBG, "MiniDumpWriteDump");

    return allLoaded;
}
