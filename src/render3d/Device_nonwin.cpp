#include "Device.h"

#if !RO_PLATFORM_WINDOWS

#include <cmath>
#include <cstring>

C3dDevice g_3dDevice;

C3dDevice::C3dDevice()
    : m_hWnd(nullptr),
      m_dwRenderWidth(0),
      m_dwRenderHeight(0),
      m_pddsFrontBuffer(nullptr),
      m_pddsBackBuffer(nullptr),
      m_pddsRenderTarget(nullptr),
      m_pddsZBuffer(nullptr),
      m_pddsRenderBackup(nullptr),
      m_pd3dDevice(nullptr),
      m_pDD(nullptr),
      m_pD3D(nullptr),
      m_dwDeviceMemType(0),
      m_bIsFullscreen(0),
      m_bIsGDIObject(0),
      m_windowBitCount(32),
      m_bSupportBltStretch(1),
      m_bSupportTextureSurface(1),
      m_pfRShiftR(16),
      m_pfRShiftL(0),
      m_pfGShiftR(8),
      m_pfGShiftL(0),
      m_pfBShiftR(0),
      m_pfBShiftL(0),
      m_pfAShiftR(24),
      m_pfAShiftL(0),
      m_pfBitCount(32),
      m_pfRBitMask(0x00FF0000u),
      m_pfGBitMask(0x0000FF00u),
      m_pfBBitMask(0x000000FFu),
      m_pfABitMask(0xFF000000u),
      m_dwMinTextureWidth(1),
      m_dwMinTextureHeight(1),
      m_dwMaxTextureWidth(4096),
      m_dwMaxTextureHeight(4096),
      m_dwMaxTextureAspectRatio(4096)
{
    std::memset(&m_rcViewportRect, 0, sizeof(m_rcViewportRect));
    std::memset(&m_rcScreenRect, 0, sizeof(m_rcScreenRect));
    std::memset(&m_ddsdFrameBuffer, 0, sizeof(m_ddsdFrameBuffer));
    std::memset(&m_ddpfZBuffer, 0, sizeof(m_ddpfZBuffer));
    std::memset(&m_ddDeviceDesc, 0, sizeof(m_ddDeviceDesc));
}

C3dDevice::~C3dDevice() = default;

int C3dDevice::Init(HWND hwnd, GUID*, GUID*, IDirectDrawClipper*, unsigned int)
{
    m_hWnd = hwnd;
    RECT clientRect{};
    GetClientRect(hwnd, &clientRect);
    m_dwRenderWidth = static_cast<unsigned int>((std::max)(1, static_cast<int>(clientRect.right - clientRect.left)));
    m_dwRenderHeight = static_cast<unsigned int>((std::max)(1, static_cast<int>(clientRect.bottom - clientRect.top)));
    return static_cast<int>(E_NOTIMPL);
}

int C3dDevice::DestroyObjects() { return 0; }
int C3dDevice::Clear(unsigned int) { return 0; }
int C3dDevice::ClearZBuffer() { return 0; }
int C3dDevice::ShowFrame(bool) { return 0; }
int C3dDevice::CloneFrame() { return 0; }
void C3dDevice::BackupFrame() {}
void C3dDevice::RestoreFrame() {}
int C3dDevice::RestoreSurfaces() { return 0; }
CRenderer* C3dDevice::CreateRenderer() { return nullptr; }
void C3dDevice::DestroyRenderer(CRenderer*) {}

CSurface* C3dDevice::CreateWallPaper(unsigned int w, unsigned int h)
{
    return new CSurfaceWallpaper(w, h);
}

void C3dDevice::AdjustTextureSize(unsigned int* w, unsigned int* h)
{
    if (!w || !h) {
        return;
    }

    unsigned int adjustedWidth = 1;
    while (adjustedWidth < *w) {
        adjustedWidth <<= 1;
    }
    unsigned int adjustedHeight = 1;
    while (adjustedHeight < *h) {
        adjustedHeight <<= 1;
    }
    *w = adjustedWidth;
    *h = adjustedHeight;
}

void C3dDevice::EnableClipper(int) {}
void C3dDevice::EnableMipmap() {}
int C3dDevice::FlipToGDISurface(int) { return 0; }

unsigned int C3dDevice::DecodePixelR(unsigned short color) const { return (color >> 10) & 0x1Fu; }
unsigned int C3dDevice::DecodePixelG(unsigned short color) const { return (color >> 5) & 0x1Fu; }
unsigned int C3dDevice::DecodePixelB(unsigned short color) const { return color & 0x1Fu; }

void C3dDevice::ConvertPalette(unsigned int* outPalette, const PALETTEENTRY* entries, int count) const
{
    if (!outPalette || !entries || count <= 0) {
        return;
    }

    for (int index = 0; index < count; ++index) {
        outPalette[index] = 0xFF000000u
            | (static_cast<unsigned int>(entries[index].peRed) << 16)
            | (static_cast<unsigned int>(entries[index].peGreen) << 8)
            | static_cast<unsigned int>(entries[index].peBlue);
    }
}

bool C3dDevice::TestScreen() { return true; }

int C3dDevice::CreateDirectDraw(GUID*, unsigned int) { return static_cast<int>(E_NOTIMPL); }
int C3dDevice::CreateDirect3D(GUID*, unsigned int) { return static_cast<int>(E_NOTIMPL); }
int C3dDevice::CreateBuffers(IDirectDrawClipper*, unsigned int) { return static_cast<int>(E_NOTIMPL); }
int C3dDevice::CreateZBuffer() { return static_cast<int>(E_NOTIMPL); }
int C3dDevice::CreateEnvironment(GUID*, GUID*, IDirectDrawClipper*, unsigned int) { return static_cast<int>(E_NOTIMPL); }

void GDIFlip() {}
void Trace(const char*, ...) {}
long CALLBACK EnumZBufferFormatsCallback(LPDDPIXELFORMAT, VOID*) { return 0; }

#endif