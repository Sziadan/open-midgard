#pragma once

#include <windows.h>
#include <ddraw.h>
#include <d3d.h>
#include <stdio.h>
#include "render/Renderer.h"
#include "res/Texture.h"
#include "render/DC.h"

// Forward declarations
class CRenderer;
class CTexture;
class CDCBitmap;
class CSurface;

class C3dDevice {
public:
    C3dDevice();
    ~C3dDevice();

    int Init(HWND hwnd, GUID* pDriverGUID, GUID* pDeviceGUID, IDirectDrawClipper* pMode, unsigned int dwFlags);
    int DestroyObjects();
    
    int Clear(unsigned int color);
    int ClearZBuffer();
    int ShowFrame(bool vertSync);
    int CloneFrame();
    
    void BackupFrame();
    void RestoreFrame();
    int RestoreSurfaces();
    
    CRenderer* CreateRenderer();
    void DestroyRenderer(CRenderer* rc);
    
    CDCBitmap* CreateDCBuffer(unsigned int w, unsigned int h);
    CSurface* CreateWallPaper(unsigned int w, unsigned int h);
    
    void AdjustTextureSize(unsigned int* w, unsigned int* h);
    void EnableClipper(int bEnable);
    void EnableMipmap();
    int FlipToGDISurface(int bDrawFrame);
    
    unsigned int DecodePixelR(unsigned short color) const;
    unsigned int DecodePixelG(unsigned short color) const;
    unsigned int DecodePixelB(unsigned short color) const;
    void ConvertPalette(unsigned int* outPalette, const PALETTEENTRY* entries, int count) const;
    
    bool TestScreen();

public:
    HWND m_hWnd;
    unsigned int m_dwRenderWidth;
    unsigned int m_dwRenderHeight;
    
    IDirectDrawSurface7* m_pddsFrontBuffer;
    IDirectDrawSurface7* m_pddsBackBuffer;
    IDirectDrawSurface7* m_pddsRenderTarget;
    IDirectDrawSurface7* m_pddsZBuffer;
    IDirectDrawSurface7* m_pddsRenderBackup;
    
    IDirect3DDevice7* m_pd3dDevice;
    IDirectDraw7* m_pDD;
    IDirect3D7* m_pD3D;
    
    unsigned int m_dwDeviceMemType;
    int m_bIsFullscreen;
    int m_bIsGDIObject;
    int m_windowBitCount;
    int m_bSupportBltStretch;
    int m_bSupportTextureSurface;

    // Pixel format info
    unsigned int m_pfRShiftR, m_pfRShiftL;
    unsigned int m_pfGShiftR, m_pfGShiftL;
    unsigned int m_pfBShiftR, m_pfBShiftL;
    unsigned int m_pfAShiftR, m_pfAShiftL;
    unsigned int m_pfBitCount;
    unsigned int m_pfRBitMask, m_pfGBitMask, m_pfBBitMask, m_pfABitMask;

    RECT m_rcViewportRect;
    RECT m_rcScreenRect;
    _DDSURFACEDESC2 m_ddsdFrameBuffer;
    _DDPIXELFORMAT m_ddpfZBuffer;
    D3DDEVICEDESC7 m_ddDeviceDesc;

    // Texture constraints
    unsigned int m_dwMinTextureWidth, m_dwMinTextureHeight;
    unsigned int m_dwMaxTextureWidth, m_dwMaxTextureHeight;
    unsigned int m_dwMaxTextureAspectRatio;

private:
    int CreateDirectDraw(GUID* pDriverGUID, unsigned int dwFlags);
    int CreateDirect3D(GUID* pDeviceGUID, unsigned int dwFlags);
    int CreateBuffers(IDirectDrawClipper* pddsd, unsigned int dwFlags);
    int CreateZBuffer();
    int CreateEnvironment(GUID* pDriverGUID, GUID* pDeviceGUID, IDirectDrawClipper* pMode, unsigned int dwFlags);
};

class CDynamicVB {
public:
    CDynamicVB();
    ~CDynamicVB();
    
    bool Init(unsigned int theFVF, unsigned int theVertexCount, unsigned int theVertexSize);
    bool Lock(unsigned int vertCount, void** ppLockedData);
    void Unlock();
    void DrawPri(IDirect3DDevice7* device, unsigned int vertCount);
    void Release();

public:
    unsigned int m_vertCount;
    unsigned int m_vertOffset;
    unsigned int m_vertSize;
    IDirect3DVertexBuffer7* m_pVB;
};

extern C3dDevice g_3dDevice;

// Global functions
void GDIFlip();
void Trace(const char* fmt, ...);
long CALLBACK EnumZBufferFormatsCallback(LPDDPIXELFORMAT pddpf, VOID* pddpfDesired);
