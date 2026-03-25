#pragma once

#include <windows.h>
#include <ddraw.h>
#include <d3d.h>

enum PixelFormat {
    PF_DEFAULT = 0,
    PF_A1R5G5B5 = 1,
    PF_R5G6B5 = 2,
    PF_A4R4G4B4 = 3,
    PF_A8R8G8B8 = 4,
    PF_BUMP = 5
};

class CSurface {
public:
    CSurface(unsigned int w = 0, unsigned int h = 0);
    CSurface(unsigned int w, unsigned int h, IDirectDrawSurface7* pSurface);
    virtual ~CSurface();

    bool Create(unsigned int w, unsigned int h);
    virtual void ClearSurface(RECT* rect, unsigned int color);
    virtual void DrawSurface(int x, int y, int w, int h, unsigned int color);
    virtual void DrawSurfaceStretch(int x, int y, int w, int h);
    virtual void Update(int x, int y, int w, int h, unsigned int* data, bool skipColorKey, int lPitch);

public:
    IDirectDrawSurface7* m_pddsSurface;
    unsigned int m_w, m_h;
    HBITMAP m_hBitmap;
};

class CTexture : public CSurface {
public:
    CTexture(unsigned int w = 0, unsigned int h = 0, PixelFormat pf = PF_DEFAULT);
    CTexture(unsigned int w, unsigned int h, PixelFormat pf, IDirectDrawSurface7* pSurface);
    virtual ~CTexture();

    bool Create(unsigned int w, unsigned int h, PixelFormat pf);
    bool CreateBump(unsigned int w, unsigned int h);
    bool CreateBump(unsigned int w, unsigned int h, IDirectDrawSurface7* pSurface);
    
    void SetUVAdjust(unsigned int w, unsigned int h);
    void UpdateMipmap(RECT* rect);
    bool CopyTexture(CTexture* src, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh);
    
    int Lock();
    int Unlock();

    virtual void ClearSurface(RECT* rect, unsigned int color) override;
    virtual void DrawSurface(int x, int y, int w, int h, unsigned int color) override;
    virtual void DrawSurfaceStretch(int x, int y, int w, int h) override;
    virtual void Update(int x, int y, int w, int h, unsigned int* data, bool skipColorKey, int lPitch) override;
    void UpdateSprite(int x, int y, int w, int h, struct SprImg& img, unsigned int* pal);

public:
    PixelFormat m_pf;
    int m_lockCnt;
    unsigned int m_timeStamp;
    char m_texName[128];
    unsigned int m_updateWidth, m_updateHeight;

    static float m_uOffset;
    static float m_vOffset;
};

class CSurfaceWallpaper : public CSurface {
public:
    CSurfaceWallpaper(unsigned int w = 0, unsigned int h = 0);
    virtual ~CSurfaceWallpaper();
    virtual void Update(int x, int y, int w, int h, unsigned int* data, bool skipColorKey, int lPitch) override;
};
