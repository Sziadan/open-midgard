#pragma once

#include <windows.h>
#include <ddraw.h>
#include <d3d.h>
#include <vector>
#include <list>
#include <map>
#include <string>
#include "Types.h"
#include "res/Texture.h"
#include "render3d/D3dutil.h"

// Forward declarations
class CTexture;
struct SprImg;
struct CacheInfo;
class CBitmapRes;

struct tlvertex3d {
    float x, y, z;
    float oow;
    DWORD color;
    DWORD specular;
    float tu, tv;
};

struct lmtlvertex3d {
    tlvertex3d vert;
    float tu2, tv2;
};

struct RPFace {
    D3DPRIMITIVETYPE primType;
    tlvertex3d* verts;
    tlvertex3d m_verts[6];
    int numVerts;
    unsigned short* indices;
    int numIndices;
    CTexture* tex;
    int mtPreset;
    D3DCULL cullMode;
    D3DBLEND srcAlphaMode;
    D3DBLEND destAlphaMode;
    float alphaSortKey;

    static void DrawPri(RPFace* face, IDirect3DDevice7* device);
};

struct RPLmFace {
    D3DPRIMITIVETYPE primType;
    lmtlvertex3d* lmverts;
    int numVerts;
    unsigned short* indices;
    int numIndices;
    CTexture* tex;
    CTexture* tex2;

    static void DrawPri(RPLmFace* face, IDirect3DDevice7* device);
};

struct RPRaw {
    D3DPRIMITIVETYPE primType;
    tlvertex3d* verts;
    int numVerts;
    unsigned short* indices;
    int numIndices;
    CTexture* tex;

    static void DrawPri(RPRaw* face, IDirect3DDevice7* device);
};

struct RPQuadFace {
    D3DPRIMITIVETYPE primType;
    tlvertex3d* verts;
    tlvertex3d m_verts[4];
    int numVerts;
    int numIndices;
};

struct RPLmQuadFace {
    D3DPRIMITIVETYPE primType;
    lmtlvertex3d* lmverts;
    int numVerts;
    unsigned short* indices;
    int numIndices;
    CTexture* tex;
    CTexture* tex2;
    lmtlvertex3d m_lmverts[4];
};

struct CacheSurface {
    CTexture* tex;
    DWORD lastTime;
    ~CacheSurface();
};

struct CharPrtLess {
    bool operator()(const char* a, const char* b) const {
        return strcmp(a, b) < 0;
    }
};

class CTexMgr {
public:
    CTexMgr();
    ~CTexMgr();
    
    CTexture* CreateTexture(int w, int h, unsigned long* data, PixelFormat format, bool b);
    CTexture* CreateTexture(unsigned long w, unsigned long h, PixelFormat format, IDirectDrawSurface7* pSurface);
    CTexture* GetTexture(const char* name, bool b);
    
    void DestroyAllTexture();
    void DestroyTexture(CTexture* tex);
    void UnloadRarelyUsedTexture();

    static CTexture s_dummy_texture;
    std::map<const char*, CTexture*, CharPrtLess> m_texTable;

private:
    void AddTexture(CTexture* tex);
};

extern CTexMgr g_texMgr;

class CRenderer {
public:
    CRenderer();
    ~CRenderer();

    void Init();
    bool DrawScene();
    void Clear(int color);
    void ClearBackground();
    void Flip(bool vertSync);
    
    void SetSize(int w, int h);
    void SetPixelFormat(PixelFormat format);
    void SetTexture(CTexture* tex);
    void SetLmapTexture(CTexture* tex);
    void SetMultiTextureMode(int nMode);
    
    void AddRP(RPFace* face, int renderFlag);
    void AddLmRP(RPLmFace* face, int renderFlag);
    void AddRawRP(RPRaw* face, int renderFlag);
    
    void FlushRenderList();
    
    void SetLookAt(struct vector3d& eye, struct vector3d& at, struct vector3d& up);
    void SetLight(struct vector3d* dir, struct vector3d* diffuse, struct vector3d* ambient);
    
    int TextOutScreen(int x, int y, const char* text, unsigned long colorRef, int fontHeight, char fontPrint);
    tagSIZE* GetTextSize(const char* text, int textLen, int fontType, int fontHeight, unsigned char bold);

    tlvertex3d* BorrowVerts(unsigned int vertCount);
    RPFace* BorrowNullRP();
    RPQuadFace* BorrowQuadRP();
    RPLmQuadFace* BorrowLmQuadRP();

    int Lock();
    void Unlock();

    int m_width, m_height;
    int m_halfWidth, m_halfHeight;
    float m_aspectRatio;
    float m_hpc, m_vpc;
    float m_hratio, m_vratio;
    vector3d m_eyePos;
    vector3d m_eyeAt;
    vector3d m_eyeUp;
    vector3d m_eyeRight;
    vector3d m_eyeForward;
    int m_xoffset, m_yoffset;
    float m_screenXFactor, m_screenYFactor;
    
    PixelFormat m_pf;
    IDirect3DDevice7* m_device;
    void* m_lpSurfacePtr;
    int m_lPitch;
    int m_bRGBBitCount;
    int m_nClearColor;

    bool m_isVertexFog;
    bool m_isFoggy;
    bool m_fogChanged;

    CTexture* m_oldTexture;
    CTexture* m_oldLmapTexture;
    
    unsigned int m_curFrame;
    unsigned int m_fpsFrameCount;
    unsigned int m_fpsStartTick;

    std::vector<RPFace*> m_rpFaceList;
    std::vector<RPFace*> m_rpLMGroundList;
    std::vector<RPFace*> m_rpLMLightList;
    std::vector<std::pair<float, RPFace*>> m_rpAlphaDepthList;
    std::vector<std::pair<float, RPFace*>> m_rpAlphaList;
    std::vector<std::pair<float, RPFace*>> m_rpAlphaNoDepthList;
    std::vector<std::pair<float, RPFace*>> m_rpEmissiveDepthList;
    std::vector<std::pair<float, RPFace*>> m_rpEmissiveList;
    std::vector<std::pair<float, RPFace*>> m_rpEmissiveNoDepthList;
    std::vector<RPRaw*> m_rpRawList;
    std::vector<RPRaw*> m_rpRawAlphaList;
    std::vector<RPRaw*> m_rpRawEmissiveList;
    std::vector<std::pair<float, RPFace*>> m_rpAlphaOPList;
    std::vector<RPLmFace*> m_rpLmList;
    std::vector<RPLmFace*> m_rpBumpFaceList;

    std::list<RPFace> m_rpNullFaceList;
    std::list<RPFace>::iterator m_rpNullFaceListIter;
    std::list<RPQuadFace> m_rpQuadFaceList;
    std::list<RPQuadFace>::iterator m_rpQuadFaceListIter;
    std::list<RPLmQuadFace> m_rpLmQuadFaceList;
    std::list<RPLmQuadFace>::iterator m_rpLmQuadFaceListIter;

    std::vector<tlvertex3d> m_vertBuffer;
    std::list<CTexture*> m_unusedCacheSurfaces;
    std::list<CacheSurface> m_cacheSurfaces[16];

private:
    void FlushAlphaDepthList();
    void FlushAlphaList();
    void FlushAlphaRawList();
    void FlushFaceList();
    void FlushLMGroundList();
    void FlushLMLightList();
    void FlushLmList();
    void FlushRawList();
    void FlushAlphaNoDepthList();
    void FlushEmissiveNoDepthList();
};

extern CRenderer g_renderer;
