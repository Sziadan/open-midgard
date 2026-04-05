#include "Res.h"
#include "core/File.h"
#include "DebugLog.h"
#include <algorithm>
#include <cstring>
#include <windows.h>

#if RO_PLATFORM_WINDOWS
#pragma comment(lib, "winmm.lib")
#endif

namespace {
constexpr bool kLogCRes = false;
constexpr bool kLogResMgr = false;
}

#define LOG_CRES(...) do { if constexpr (kLogCRes) { DbgLog(__VA_ARGS__); } } while (0)
#define LOG_RESMGR(...) do { if constexpr (kLogResMgr) { DbgLog(__VA_ARGS__); } } while (0)

CResMgr g_resMgr;

//===========================================================================
// CRes Implementation
//===========================================================================
CRes::CRes() : m_lockCnt(0), m_timeStamp(0), m_extIndex(-1)
{
}

CRes::~CRes() = default;

bool CRes::LoadFromBuffer(const char* fName, const unsigned char* buffer, int size)
{
    return false; // To be implemented by derived classes
}

bool CRes::Load(const char* fName)
{
    int size = 0;
    LOG_CRES("[CRes] Load('%s')\n", fName ? fName : "(null)");
    unsigned char* buf = g_fileMgr.GetData(fName, &size);
    LOG_CRES("[CRes] GetData returned buf=%p size=%d\n", (void*)buf, size);
    if (!buf) return false;

    bool result = LoadFromBuffer(fName, buf, size);
    LOG_CRES("[CRes] LoadFromBuffer returned %d\n", (int)result);
    delete[] buf;
    return result;
}

void CRes::Reset()
{
}

void CRes::OnLoadError(const char* fName)
{
}

//===========================================================================
// CResMgr Implementation
//===========================================================================
CResMgr::CResMgr()
{
    InitializeCriticalSection(&m_GetResSection);
    
    m_usedForSprTexture = 0;
    m_usedForModelTexture = 0;
    m_usedForGNDTexture = 0;
    m_usedForSprite = 0;
    m_usedForSprAction = 0;
    m_usedForGAT = 0;
    m_usedForGND = 0;
    m_usedForIMF = 0;
    m_usedForModel = 0;
    
    m_ResMemAmount = 0;
    m_ResSprAmount = 0;
    m_ResTexAmount = 0;
    m_ResGatAmount = 0;
    m_ResGndAmount = 0;
    m_ResRswAmount = 0;
    m_ResModAmount = 0;
    m_ResWavAmount = 0;
}

CResMgr::~CResMgr()
{
    UnloadAll();
    DeleteCriticalSection(&m_GetResSection);
    for (CRes* factory : m_objTypes) {
        delete factory;
    }
}

void CResMgr::RegisterType(const char* ext, const char* defaultDir, CRes* factoryObj)
{
    int index = (int)m_objTypes.size();
    m_resExt[std::string(ext)] = index;
    m_objTypes.push_back(factoryObj);
    m_typeDir.push_back(std::string(defaultDir));
    m_fileList.emplace_back();
}

CRes* CResMgr::Get(const char* fNameInput, bool bRefresh)
{
    if (!fNameInput) return nullptr;

    EnterCriticalSection(&m_GetResSection);
    
    // Find extension
    const char* dot = std::strrchr(fNameInput, '.');
    if (!dot) {
        LeaveCriticalSection(&m_GetResSection);
        return nullptr;
    }
    const char* ext = dot + 1;
    
    auto itExt = m_resExt.find(std::string(ext));
    if (itExt == m_resExt.end()) {
        LeaveCriticalSection(&m_GetResSection);
        return nullptr;
    }
    int extIndex = itExt->second;

    // Build full name with directory prefix if needed
    char fullPath[260];
    const std::string& typeDir = m_typeDir[extIndex];
    if (std::strncmp(fNameInput, typeDir.c_str(), typeDir.size()) == 0) {
        std::strncpy(fullPath, fNameInput, 259);
    } else {
        std::strncpy(fullPath, typeDir.c_str(), 259);
        fullPath[259] = '\0';
        std::strncat(fullPath, fNameInput, 259 - std::strlen(fullPath));
    }
    fullPath[259] = '\0';

    CHash normalizedPath(fullPath);

    // Check if already loaded
    auto& resMap = m_fileList[extIndex];
    for (auto& pair : resMap) {
        if (std::strcmp(pair.first->m_String, normalizedPath.m_String) == 0) {
            if (!bRefresh) {
                pair.second->m_timeStamp = GetTickCount();
                LeaveCriticalSection(&m_GetResSection);
                return pair.second;
            }
            break;
        }
    }

    // Load new
    CRes* factory = m_objTypes[extIndex];
    LOG_RESMGR("[ResMgr] Load new: '%s' extIndex=%d\n", fullPath, extIndex);
    CRes* newRes = factory->Clone();
    LOG_RESMGR("[ResMgr] Clone=%p\n", (void*)newRes);
    if (newRes) {
        LOG_RESMGR("[ResMgr] Calling Load...\n");
        bool loadOk = newRes->Load(fullPath);
        LOG_RESMGR("[ResMgr] Load returned %d\n", (int)loadOk);
        if (loadOk) {
            newRes->m_extIndex = extIndex;
            newRes->m_fName.SetString(fullPath);
            newRes->m_timeStamp = GetTickCount();
            resMap[&newRes->m_fName] = newRes;
        } else {
            newRes->OnLoadError(fullPath);
            delete newRes;
            newRes = nullptr;
        }
    }

    LeaveCriticalSection(&m_GetResSection);
    LOG_RESMGR("[ResMgr] Get returning %p\n", (void*)newRes);
    return newRes;
}

CRes* CResMgr::GetAlways(const char* fNameInput)
{
    return Get(fNameInput, true);
}

bool CResMgr::IsExist(const char* fNameInput)
{
    return g_fileMgr.IsDataExist(fNameInput);
}

void CResMgr::Reset()
{
    UnloadAll();
}

void CResMgr::Unload(CRes* res)
{
    if (!res) return;
    EnterCriticalSection(&m_GetResSection);
    
    auto& resMap = m_fileList[res->m_extIndex];
    resMap.erase(&res->m_fName);
    delete res;
    
    LeaveCriticalSection(&m_GetResSection);
}

void CResMgr::UnloadAll()
{
    EnterCriticalSection(&m_GetResSection);
    for (auto& resMap : m_fileList) {
        for (auto& pair : resMap) {
            delete pair.second;
        }
        resMap.clear();
    }
    LeaveCriticalSection(&m_GetResSection);
}

void CResMgr::UnloadRarelyUsedRes()
{
}

void CResMgr::UnloadResByExt(const char* ext)
{
    auto it = m_resExt.find(ext);
    if (it != m_resExt.end()) {
        EnterCriticalSection(&m_GetResSection);
        auto& resMap = m_fileList[it->second];
        for (auto& pair : resMap) {
            delete pair.second;
        }
        resMap.clear();
        LeaveCriticalSection(&m_GetResSection);
    }
}

void CResMgr::UnloadUnlockedRes()
{
    EnterCriticalSection(&m_GetResSection);
    for (auto& resMap : m_fileList) {
        for (auto it = resMap.begin(); it != resMap.end(); ) {
            if (it->second->m_lockCnt == 0) {
                delete it->second;
                it = resMap.erase(it);
            } else {
                ++it;
            }
        }
    }
    LeaveCriticalSection(&m_GetResSection);
}
