#include "Granny.h"
#include "GameActor.h"
#include "core/DllMgr.h"

// ============================================================================
// C3dGrannyModelRes Implementation
// ============================================================================
C3dGrannyModelRes::C3dGrannyModelRes() 
    : m_ProjectZ(0), m_pModel(nullptr), m_pBoneRes(nullptr), 
      m_nBoneType(0), m_hair(nullptr), m_pSkeleton(nullptr) 
{
    std::memset(&m_Scene, 0, sizeof(m_Scene));
}

C3dGrannyModelRes::~C3dGrannyModelRes() {
    Reset();
}

bool C3dGrannyModelRes::Load(const char* fName) {
    return true; 
}

CRes* C3dGrannyModelRes::Clone() {
    return new C3dGrannyModelRes();
}

void C3dGrannyModelRes::Reset() {
}

// ============================================================================
// CGranny DLL Wrapper Implementation (Phase 3)
// ============================================================================
CGranny::CGranny() : m_grannyFile(nullptr), m_grannyModel(nullptr) {}

CGranny::~CGranny() {
}

bool CGranny::LoadModel(const char* fName, const void* data, int size) {
    if (!g_dllExports.GrannyReadEntireFileFromMemory || !g_dllExports.GrannyGetFileInfo) return false;

    m_grannyFile = g_dllExports.GrannyReadEntireFileFromMemory(size, data);
    if (!m_grannyFile) return false;

    void* fileInfo = g_dllExports.GrannyGetFileInfo(m_grannyFile);
    if (fileInfo) {
        return true;
    }
    return false;
}

void CGranny::Tick(float elapsed) {
}

// ============================================================================
// CGrannyPc Implementation (Phase 3)
// ============================================================================
CGrannyPc::CGrannyPc() 
    : m_honor(0), m_virtue(0), m_headDir(0), m_head(0), m_headPalette(0)
    , m_weapon(0), m_accessory(0), m_accessory2(0), m_accessory3(0)
    , m_shield(0), m_shoe(0), m_renderWithoutLayer(0), m_gage(nullptr)
    , m_pk_rank(0), m_pk_total(0), m_GrannyActorRes(nullptr), m_Instance(nullptr)
    , m_GameClock(0), m_curAction(0), m_baseAction(0), m_fCurRot(0)
    , m_RenderAlpha(255), m_nVertCol(0), m_nLastActAnimation(0), m_curFrame(0)
    , m_nRenderType(0), m_pCellTex(nullptr), m_isFirstProcess(1), m_nUpdateAniFlag(0)
    , m_pWorldPose(nullptr), m_shadowTex(nullptr)
{
    std::memset(m_GrannyPartRes, 0, sizeof(m_GrannyPartRes));
    std::memset(m_rp, 0, sizeof(m_rp));
    std::memset(m_rpPart, 0, sizeof(m_rpPart));
    std::memset(m_matVer, 0, sizeof(m_matVer));
    std::memset(m_matVerPart, 0, sizeof(m_matVerPart));
    std::memset(m_fAniCnt, 0, sizeof(m_fAniCnt));
    std::memset(m_fLastAniCnt, 0, sizeof(m_fLastAniCnt));
    std::memset(m_Control, 0, sizeof(m_Control));
    std::memset(m_pTex, 0, sizeof(m_pTex));
    std::memset(m_pFaceArr, 0, sizeof(m_pFaceArr));
    std::memset(m_nIndexNo, 0, sizeof(m_nIndexNo));
    std::memset(m_matPose, 0, sizeof(m_matPose));
    std::memset(m_strJobFn, 0, sizeof(m_strJobFn));
    std::memset(m_strJobSymbol, 0, sizeof(m_strJobSymbol));
    std::memset(m_strPartSymbol, 0, sizeof(m_strPartSymbol));
    std::memset(m_strBoneSymbol, 0, sizeof(m_strBoneSymbol));
}

CGrannyPc::~CGrannyPc() {
}

void CGrannyPc::SetAction(int act, int mot, int type) {
    m_curAction = act;
}

void CGrannyPc::Render() {
}
