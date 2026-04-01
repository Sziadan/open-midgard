#include "Device.h"
#include "D3dutil.h"
#include "main/WinMain.h"
#include "res/Texture.h"
#include "render/Renderer.h"
#include <stdio.h>

namespace {

typedef HRESULT (WINAPI* PFN_DirectDrawCreateEx)(GUID*, LPVOID*, REFIID, IUnknown*);

PFN_DirectDrawCreateEx ResolveDirectDrawCreateEx()
{
    static PFN_DirectDrawCreateEx s_proc = nullptr;
    static bool s_resolved = false;
    if (s_resolved) {
        return s_proc;
    }

    s_resolved = true;
    char systemDir[MAX_PATH] = {};
    HMODULE hDdraw = nullptr;
    const UINT dirLen = GetSystemDirectoryA(systemDir, MAX_PATH);
    if (dirLen > 0 && dirLen < MAX_PATH) {
        std::string systemDdraw = std::string(systemDir) + "\\ddraw.dll";
        hDdraw = LoadLibraryA(systemDdraw.c_str());
    }

    if (!hDdraw) {
        // Last-resort fallback if system path query fails.
        hDdraw = LoadLibraryA("ddraw.dll");
    }

    if (!hDdraw) {
        return nullptr;
    }

    s_proc = reinterpret_cast<PFN_DirectDrawCreateEx>(GetProcAddress(hDdraw, "DirectDrawCreateEx"));
    return s_proc;
}

} // namespace

C3dDevice g_3dDevice;

C3dDevice::C3dDevice() {
    m_hWnd = 0;
    m_dwRenderWidth = 0;
    m_dwRenderHeight = 0;
    m_pddsFrontBuffer = 0;
    m_pddsBackBuffer = 0;
    m_pddsRenderTarget = 0;
    m_pddsZBuffer = 0;
    m_pd3dDevice = 0;
    m_pDD = 0;
    m_pD3D = 0;
    m_dwDeviceMemType = 0;
    m_bIsFullscreen = 0;
    m_bIsGDIObject = 0;
    m_pfRShiftR = 0;
    m_pfRShiftL = 8;
    m_pfGShiftR = 0;
    m_pfGShiftL = 8;
    m_pfBShiftR = 0;
    m_pfBShiftL = 8;
    m_pfAShiftR = 0;
    m_pfAShiftL = 8;
    m_pfBitCount = 0;
    m_pfRBitMask = 0;
    m_pfGBitMask = 0;
    m_pfBBitMask = 0;
    m_pfABitMask = 0;
    m_pddsRenderBackup = 0;
    m_dwMinTextureWidth = 1;
    m_dwMinTextureHeight = 1;
    m_dwMaxTextureWidth = 2048;
    m_dwMaxTextureHeight = 2048;
    m_dwMaxTextureAspectRatio = 0;
}

C3dDevice::~C3dDevice() {
    DestroyObjects();
}

int C3dDevice::Init(HWND hwnd, GUID* pDriverGUID, GUID* pDeviceGUID, IDirectDrawClipper* pMode, unsigned int dwFlags) {
    m_hWnd = hwnd;
    if (!hwnd || !pDeviceGUID || (!pMode && (dwFlags & 1))) return 0x80070057; // E_INVALIDARG

    m_bIsFullscreen = dwFlags & 1;
    if (m_bIsFullscreen) {
        SetWindowLongPtrA(g_hMainWnd, GWL_STYLE, static_cast<LONG_PTR>(WS_POPUP));
        ShowWindow(g_hMainWnd, SW_SHOWMAXIMIZED);
    }

    int hr = CreateEnvironment(pDriverGUID, pDeviceGUID, pMode, dwFlags);
    if (hr < 0) {
        DestroyObjects();
        return hr;
    }

    if (m_pd3dDevice) {
        m_pd3dDevice->SetRenderState(D3DRENDERSTATE_ZENABLE, D3DZB_TRUE);
        m_pd3dDevice->SetRenderState(D3DRENDERSTATE_ZFUNC, D3DCMP_LESSEQUAL);
        m_pd3dDevice->SetRenderState(D3DRENDERSTATE_CULLMODE, D3DCULL_NONE);
        m_pd3dDevice->SetRenderState(D3DRENDERSTATE_TEXTUREPERSPECTIVE, TRUE);
        m_pd3dDevice->SetRenderState(D3DRENDERSTATE_ALPHAFUNC, D3DCMP_GREATER);
        m_pd3dDevice->SetRenderState(D3DRENDERSTATE_ALPHAREF, 207);
        m_pd3dDevice->SetRenderState(D3DRENDERSTATE_ALPHATESTENABLE, TRUE);
        m_pd3dDevice->SetRenderState(D3DRENDERSTATE_COLORKEYENABLE, TRUE);
        m_pd3dDevice->SetRenderState(D3DRENDERSTATE_SRCBLEND, D3DBLEND_SRCALPHA);
        m_pd3dDevice->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_INVSRCALPHA);

        m_pd3dDevice->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
        m_pd3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        m_pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        m_pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        m_pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
        m_pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
        m_pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
        m_pd3dDevice->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTFN_LINEAR);
        m_pd3dDevice->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTFG_LINEAR);
        m_pd3dDevice->SetTextureStageState(0, D3DTSS_ADDRESS, D3DTADDRESS_WRAP);
        m_pd3dDevice->SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);
        m_pd3dDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
        m_pd3dDevice->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    }

    return hr;
}

int C3dDevice::CreateEnvironment(GUID* pDriverGUID, GUID* pDeviceGUID, IDirectDrawClipper* pMode, unsigned int dwFlags) {
    if (IsEqualGUID(*pDeviceGUID, IID_IDirect3DHALDevice) || IsEqualGUID(*pDeviceGUID, IID_IDirect3DTnLHalDevice)) {
        m_dwDeviceMemType = DDSCAPS_VIDEOMEMORY;
    } else {
        m_dwDeviceMemType = DDSCAPS_SYSTEMMEMORY;
    }

    int hr = CreateDirectDraw(pDriverGUID, dwFlags);
    if (hr < 0) return hr;

    hr = CreateBuffers(pMode, dwFlags);
    if (hr < 0) return hr;

    if (pDeviceGUID) {
        hr = CreateDirect3D(pDeviceGUID, dwFlags);
        if (hr < 0) return hr;
        hr = CreateZBuffer();
        return hr;
    }

    return 0;
}

int C3dDevice::CreateDirectDraw(GUID* pDriverGUID, unsigned int dwFlags) {
    PFN_DirectDrawCreateEx pDirectDrawCreateEx = ResolveDirectDrawCreateEx();
    if (!pDirectDrawCreateEx)
        return 0x82000001;

    if (pDirectDrawCreateEx(pDriverGUID, (LPVOID*)&m_pDD, IID_IDirectDraw7, NULL) < 0)
        return 0x82000001;

    _DDSURFACEDESC2 ddsd;
    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    m_pDD->GetDisplayMode(&ddsd);
    m_windowBitCount = ddsd.ddpfPixelFormat.dwRGBBitCount;

    DWORD dwCoop = DDSCL_NORMAL;
    if (m_bIsFullscreen) {
        dwCoop = DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN | DDSCL_ALLOWREBOOT;
    }
    
    if (!(dwFlags & 0x10)) dwCoop |= DDSCL_FPUSETUP;
    dwCoop |= DDSCL_MULTITHREADED;

    if (m_pDD->SetCooperativeLevel(m_hWnd, dwCoop) < 0)
        return 0x82000002;

    return 0;
}

int C3dDevice::CreateBuffers(IDirectDrawClipper* pMode, unsigned int dwFlags) {
    _DDSURFACEDESC2 ddsd;
    _DDSCAPS2 ddscaps;

    if (dwFlags & 1) { // Fullscreen
        SetRect(&m_rcViewportRect, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
        m_dwRenderWidth = m_rcViewportRect.right;
        m_dwRenderHeight = m_rcViewportRect.bottom;
        
        if (m_pDD->SetDisplayMode(m_dwRenderWidth, m_dwRenderHeight, m_windowBitCount, 0, 0) < 0)
            return 0x82000003;

        D3DUtil_InitSurfaceDesc(&ddsd, DDSD_CAPS | DDSD_BACKBUFFERCOUNT, DDSCAPS_PRIMARYSURFACE | DDSCAPS_3DDEVICE | DDSCAPS_COMPLEX | DDSCAPS_FLIP);
        ddsd.dwBackBufferCount = 1;
        
        if (m_pDD->CreateSurface(&ddsd, &m_pddsFrontBuffer, NULL) < 0)
            return 0x82000004;

        memset(&ddscaps, 0, sizeof(ddscaps));
        ddscaps.dwCaps = DDSCAPS_BACKBUFFER;
        if (m_pddsFrontBuffer->GetAttachedSurface(&ddscaps, &m_pddsBackBuffer) < 0)
            return 0x82000005;

        m_pddsRenderTarget = m_pddsBackBuffer;
    } else { // Windowed
        GetClientRect(m_hWnd, &m_rcViewportRect);
        m_dwRenderWidth = m_rcViewportRect.right;
        m_dwRenderHeight = m_rcViewportRect.bottom;

        D3DUtil_InitSurfaceDesc(&ddsd, DDSD_CAPS, DDSCAPS_PRIMARYSURFACE);
        if (m_pDD->CreateSurface(&ddsd, &m_pddsFrontBuffer, NULL) < 0)
            return 0x82000004;

        IDirectDrawClipper* pClipper;
        if (m_pDD->CreateClipper(0, &pClipper, NULL) < 0) return 0x82000006;
        pClipper->SetHWnd(0, m_hWnd);
        m_pddsFrontBuffer->SetClipper(pClipper);
        pClipper->Release();

        D3DUtil_InitSurfaceDesc(&ddsd, DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT, DDSCAPS_OFFSCREENPLAIN | DDSCAPS_3DDEVICE);
        ddsd.dwWidth = m_dwRenderWidth;
        ddsd.dwHeight = m_dwRenderHeight;
        if (m_pDD->CreateSurface(&ddsd, &m_pddsBackBuffer, NULL) < 0)
            return 0x82000007;

        m_pddsRenderTarget = m_pddsBackBuffer;
    }

    m_pddsRenderTarget->AddRef();
    
    // Pixel format shifts
    m_pddsFrontBuffer->GetSurfaceDesc(&m_ddsdFrameBuffer);
    DDPIXELFORMAT& pf = m_ddsdFrameBuffer.ddpfPixelFormat;
    m_pfBitCount = pf.dwRGBBitCount;
    m_pfRBitMask = pf.dwRBitMask;
    m_pfGBitMask = pf.dwGBitMask;
    m_pfBBitMask = pf.dwBBitMask;
    m_pfABitMask = pf.dwRGBAlphaBitMask;

    auto GetShift = [](DWORD mask) -> DWORD {
        DWORD shift = 0;
        if (mask == 0) return 0;
        while (!(mask & 1)) { mask >>= 1; shift++; }
        return shift;
    };
    
    auto GetBits = [](DWORD mask) -> DWORD {
        DWORD bits = 0;
        if (mask == 0) return 0;
        while (!(mask & 1)) mask >>= 1;
        while (mask & 1) { mask >>= 1; bits++; }
        return bits;
    };

    m_pfRShiftR = GetShift(m_pfRBitMask); m_pfRShiftL = 8 - GetBits(m_pfRBitMask);
    m_pfGShiftR = GetShift(m_pfGBitMask); m_pfGShiftL = 8 - GetBits(m_pfGBitMask);
    m_pfBShiftR = GetShift(m_pfBBitMask); m_pfBShiftL = 8 - GetBits(m_pfBBitMask);
    m_pfAShiftR = GetShift(m_pfABitMask); m_pfAShiftL = 8 - GetBits(m_pfABitMask);

    return 0;
}

int C3dDevice::CreateDirect3D(GUID* pDeviceGUID, unsigned int dwFlags) {
    if (m_pDD->QueryInterface(IID_IDirect3D7, (LPVOID*)&m_pD3D) < 0)
        return 0x82000008;

    if (m_pD3D->CreateDevice(*pDeviceGUID, m_pddsRenderTarget, &m_pd3dDevice) < 0)
        return 0x82000009;

    _D3DVIEWPORT7 vp;
    D3DUtil_InitViewport(&vp, m_dwRenderWidth, m_dwRenderHeight);
    m_pd3dDevice->SetViewport(&vp);

    memset(&m_ddpfZBuffer, 0, sizeof(m_ddpfZBuffer));
    m_ddpfZBuffer.dwFlags = (dwFlags & 8) ? DDPF_STENCILBUFFER | DDPF_ZBUFFER : DDPF_ZBUFFER;
    m_pD3D->EnumZBufferFormats(*pDeviceGUID, EnumZBufferFormatsCallback, &m_ddpfZBuffer);

    return 0;
}

int C3dDevice::CreateZBuffer() {
    m_pd3dDevice->GetCaps(&m_ddDeviceDesc);
    if (m_ddDeviceDesc.dpcTriCaps.dwRasterCaps & D3DPRASTERCAPS_ZBUFFERLESSHSR) return 0;

    _DDSURFACEDESC2 ddsd;
    D3DUtil_InitSurfaceDesc(&ddsd, DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT, m_dwDeviceMemType | DDSCAPS_ZBUFFER);
    ddsd.dwWidth = m_dwRenderWidth;
    ddsd.dwHeight = m_dwRenderHeight;
    memcpy(&ddsd.ddpfPixelFormat, &m_ddpfZBuffer, sizeof(DDPIXELFORMAT));

    if (m_pDD->CreateSurface(&ddsd, &m_pddsZBuffer, NULL) < 0)
        return 0x8200000A;

    if (m_pddsRenderTarget->AddAttachedSurface(m_pddsZBuffer) < 0)
        return 0x8200000B;

    m_pd3dDevice->SetRenderTarget(m_pddsRenderTarget, 0);
    return 0;
}

int C3dDevice::DestroyObjects() {
    if (m_pd3dDevice) { m_pd3dDevice->Release(); m_pd3dDevice = 0; }
    if (m_pddsZBuffer) { m_pddsZBuffer->Release(); m_pddsZBuffer = 0; }
    if (m_pddsBackBuffer) { m_pddsBackBuffer->Release(); m_pddsBackBuffer = 0; }
    if (m_pddsFrontBuffer) { m_pddsFrontBuffer->Release(); m_pddsFrontBuffer = 0; }
    if (m_pddsRenderTarget) { m_pddsRenderTarget->Release(); m_pddsRenderTarget = 0; }
    if (m_pddsRenderBackup) { m_pddsRenderBackup->Release(); m_pddsRenderBackup = 0; }
    if (m_pD3D) { m_pD3D->Release(); m_pD3D = 0; }
    if (m_pDD) { m_pDD->Release(); m_pDD = 0; }
    return 0;
}

int C3dDevice::Clear(unsigned int color) {
    if (!m_pd3dDevice) return -1;
    D3DRECT rect = { 0, 0, (LONG)m_dwRenderWidth, (LONG)m_dwRenderHeight };
    return m_pd3dDevice->Clear(1, &rect, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, color, 1.0f, 0);
}

int C3dDevice::ClearZBuffer() {
    if (!m_pd3dDevice) return -1;
    D3DRECT rect = { 0, 0, (LONG)m_dwRenderWidth, (LONG)m_dwRenderHeight };
    return m_pd3dDevice->Clear(1, &rect, D3DCLEAR_ZBUFFER, 0, 1.0f, 0);
}

int C3dDevice::ShowFrame(bool vertSync) {
    if (!m_pddsFrontBuffer || !m_pddsBackBuffer) return -1;
    if (m_bIsFullscreen) {
        return m_pddsFrontBuffer->Flip(NULL, DDFLIP_WAIT);
    } else {
        RECT rcDest;
        GetClientRect(m_hWnd, &rcDest);
        ClientToScreen(m_hWnd, (LPPOINT)&rcDest.left);
        ClientToScreen(m_hWnd, (LPPOINT)&rcDest.right);
        return m_pddsFrontBuffer->Blt(&rcDest, m_pddsBackBuffer, NULL, DDBLT_WAIT, NULL);
    }
}

int C3dDevice::CloneFrame() {
    if (!m_pddsFrontBuffer || !m_pddsBackBuffer) return -1;
    return m_pddsBackBuffer->Blt(NULL, m_pddsFrontBuffer, NULL, DDBLT_WAIT, NULL);
}

void C3dDevice::BackupFrame() {
    if (m_pddsRenderBackup && m_pddsRenderTarget) {
        m_pddsRenderBackup->Blt(NULL, m_pddsRenderTarget, NULL, DDBLT_WAIT, NULL);
    }
}

void C3dDevice::RestoreFrame() {
    if (m_pddsRenderBackup && m_pddsRenderTarget) {
        m_pddsRenderTarget->Blt(NULL, m_pddsRenderBackup, NULL, DDBLT_WAIT, NULL);
    }
}

int C3dDevice::RestoreSurfaces() {
    if (!m_pDD) return -1;
    return m_pDD->RestoreAllSurfaces();
}

unsigned int C3dDevice::DecodePixelR(unsigned short color) const {
    return (color & m_pfRBitMask) >> m_pfRShiftR << m_pfRShiftL;
}

unsigned int C3dDevice::DecodePixelG(unsigned short color) const {
    return (color & m_pfGBitMask) >> m_pfGShiftR << m_pfGShiftL;
}

unsigned int C3dDevice::DecodePixelB(unsigned short color) const {
    return (color & m_pfBBitMask) >> m_pfBShiftR << m_pfBShiftL;
}

void C3dDevice::ConvertPalette(unsigned int* outPalette, const PALETTEENTRY* entries, int count) const {
    if (!outPalette || !entries || count <= 0) {
        return;
    }

    for (int i = 0; i < count; ++i) {
        outPalette[i] =
            (0xFFu << 24) |
            (static_cast<unsigned int>(entries[i].peRed) << 16) |
            (static_cast<unsigned int>(entries[i].peGreen) << 8) |
            static_cast<unsigned int>(entries[i].peBlue);
    }
}

long CALLBACK EnumZBufferFormatsCallback(LPDDPIXELFORMAT pddpf, VOID* pddpfDesired) {
    LPDDPIXELFORMAT pDesired = (LPDDPIXELFORMAT)pddpfDesired;
    if (pddpf->dwZBufferBitDepth == 16) {
        memcpy(pDesired, pddpf, sizeof(DDPIXELFORMAT));
        return D3DENUMRET_CANCEL;
    }
    return D3DENUMRET_OK;
}

void GDIFlip() {
    g_3dDevice.FlipToGDISurface(TRUE);
}

int C3dDevice::FlipToGDISurface(int bDrawFrame) {
    if (m_pDD && m_bIsFullscreen) {
        m_pDD->FlipToGDISurface();
        if (bDrawFrame) {
            DrawMenuBar(m_hWnd);
            RedrawWindow(m_hWnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE);
        }
    }
    return 0;
}

void C3dDevice::AdjustTextureSize(unsigned int* w, unsigned int* h) {
    unsigned int tw = 1, th = 1;
    while (tw < *w) tw <<= 1;
    while (th < *h) th <<= 1;
    *w = tw; *h = th;
}

void C3dDevice::EnableClipper(int bEnable) {
    if (m_bIsFullscreen) {
        if (bEnable) {
            IDirectDrawClipper* pClipper;
            m_pDD->CreateClipper(0, &pClipper, NULL);
            pClipper->SetHWnd(0, m_hWnd);
            m_pddsFrontBuffer->SetClipper(pClipper);
            pClipper->Release();
        } else {
            m_pddsFrontBuffer->SetClipper(NULL);
        }
    }
}

void C3dDevice::EnableMipmap() {
    // Stub
}

void Trace(const char* fmt, ...) {
    // Stub
}

CDynamicVB::CDynamicVB() : m_pVB(0), m_vertCount(0), m_vertOffset(0), m_vertSize(0) {}
CDynamicVB::~CDynamicVB() { Release(); }
bool CDynamicVB::Init(unsigned int theFVF, unsigned int theVertexCount, unsigned int theVertexSize) {
    Release();
    m_vertSize = theVertexSize;
    m_vertCount = theVertexCount;
    D3DVERTEXBUFFERDESC desc;
    memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    desc.dwCaps = D3DVBCAPS_WRITEONLY;
    desc.dwFVF = theFVF;
    desc.dwNumVertices = theVertexCount;
    return g_3dDevice.m_pD3D->CreateVertexBuffer(&desc, &m_pVB, 0) == S_OK;
}
bool CDynamicVB::Lock(unsigned int vertCount, void** ppLockedData) {
    if (!m_pVB) return false;
    if (m_vertOffset + vertCount > m_vertCount) m_vertOffset = 0;
    DWORD flags = m_vertOffset == 0 ? DDLOCK_DISCARDCONTENTS : DDLOCK_NOOVERWRITE;
    if (m_pVB->Lock(flags | DDLOCK_WAIT, ppLockedData, NULL) == S_OK) {
        *ppLockedData = (char*)*ppLockedData + (m_vertOffset * m_vertSize);
        return true;
    }
    return false;
}
void CDynamicVB::Unlock() { if (m_pVB) m_pVB->Unlock(); }
void CDynamicVB::DrawPri(IDirect3DDevice7* device, unsigned int vertCount) {
    device->DrawPrimitiveVB(D3DPT_TRIANGLELIST, m_pVB, m_vertOffset, vertCount, 0);
    m_vertOffset += vertCount;
}
void CDynamicVB::Release() { if (m_pVB) { m_pVB->Release(); m_pVB = 0; } }

bool C3dDevice::TestScreen() { return true; }
CRenderer* C3dDevice::CreateRenderer() { return new CRenderer(); }
void C3dDevice::DestroyRenderer(CRenderer* rc) { delete rc; }
CSurface* C3dDevice::CreateWallPaper(unsigned int w, unsigned int h) {
    (void)w;
    (void)h;
    return new CSurfaceWallpaper(m_dwRenderWidth, m_dwRenderHeight);
}
