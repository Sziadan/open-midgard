#pragma once

#include "Types.h"
#include "core/Hash.h"
#include <string>
#include <vector>
#include <map>
#include <windows.h>

class CRes {
public:
    CRes();
    virtual ~CRes();

    virtual bool LoadFromBuffer(const char* fName, const unsigned char* buffer, int size);
    virtual CRes* Clone() = 0;
    virtual bool Load(const char* fName);
    virtual void Reset();
    virtual void OnLoadError(const char* fName);

    // Memory layout from HighPriest.exe.h:9119
    int m_lockCnt;
    u32 m_timeStamp;
    int m_extIndex;
    CHash m_fName;
};

class CResMgr {
public:
    CResMgr();
    ~CResMgr();

    void RegisterType(const char* ext, const char* defaultDir, CRes* factoryObj);
    CRes* Get(const char* fNameInput, bool bRefresh = false);
    CRes* GetAlways(const char* fNameInput);
    bool IsExist(const char* fNameInput);

    void Reset();
    void Unload(CRes* res);
    void UnloadAll();
    void UnloadRarelyUsedRes();
    void UnloadResByExt(const char* ext);
    void UnloadUnlockedRes();

    template<typename T>
    T* GetAs(const char* name) {
        return static_cast<T*>(Get(name));
    }

private:
    std::map<std::string, int> m_resExt;
    std::vector<std::string> m_typeDir;
    std::vector<CRes*> m_objTypes;
    std::vector<std::map<CHash*, CRes*>> m_fileList;
    CRITICAL_SECTION m_GetResSection;
    u8 gap58[16];
    u32 m_usedForSprTexture;
    u32 m_usedForModelTexture;
    u32 m_usedForGNDTexture;
    u32 m_usedForSprite;
    u32 m_usedForSprAction;
    u32 m_usedForGAT;
    u32 m_usedForGND;
    u32 m_usedForIMF;
    u32 m_usedForModel;
    u32 m_ResMemAmount;
    u32 m_ResSprAmount;
    u32 m_ResTexAmount;
    u32 m_ResGatAmount;
    u32 m_ResGndAmount;
    u32 m_ResRswAmount;
    u32 m_ResModAmount;
    u32 m_ResWavAmount;
};

extern CResMgr g_resMgr;
