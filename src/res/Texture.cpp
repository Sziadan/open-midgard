#include "Texture.h"

#include "render3d/D3dutil.h"
#include "render3d/Device.h"
#include "render3d/GraphicsSettings.h"
#include "render3d/RenderDevice.h"

#include <algorithm>
#include <cstring>
#include <vector>
#include "../DebugLog.h"

namespace {

constexpr bool kLogTexture = false;

int GetTextureUpscaleFactor()
{
    return (std::max)(1, GetCachedGraphicsSettings().textureUpscaleFactor);
}

unsigned int CountTrailingZeros(unsigned int mask)
{
    if (mask == 0u) {
        return 0u;
    }

    unsigned int shift = 0u;
    while ((mask & 1u) == 0u) {
        mask >>= 1u;
        ++shift;
    }
    return shift;
}

unsigned int CountBits(unsigned int mask)
{
    unsigned int bits = 0u;
    while (mask != 0u) {
        bits += mask & 1u;
        mask >>= 1u;
    }
    return bits;
}

unsigned int PackChannel(unsigned int value, unsigned int mask)
{
    if (mask == 0u) {
        return 0u;
    }

    const unsigned int shift = CountTrailingZeros(mask);
    const unsigned int bits = CountBits(mask);
    if (bits == 0u) {
        return 0u;
    }

    const unsigned int maxValue = (1u << bits) - 1u;
    const unsigned int scaled = (value * maxValue + 127u) / 255u;
    return (scaled << shift) & mask;
}

unsigned int ConvertArgbToSurfacePixel(unsigned int argb, const DDPIXELFORMAT& pf)
{
    const unsigned int alpha = (argb >> 24) & 0xFFu;
    const unsigned int red = (argb >> 16) & 0xFFu;
    const unsigned int green = (argb >> 8) & 0xFFu;
    const unsigned int blue = argb & 0xFFu;

    if (pf.dwRGBBitCount == 32
        && pf.dwRBitMask == 0x00FF0000u
        && pf.dwGBitMask == 0x0000FF00u
        && pf.dwBBitMask == 0x000000FFu
        && pf.dwRGBAlphaBitMask == 0xFF000000u) {
        return argb;
    }

    return PackChannel(alpha, pf.dwRGBAlphaBitMask)
        | PackChannel(red, pf.dwRBitMask)
        | PackChannel(green, pf.dwGBitMask)
        | PackChannel(blue, pf.dwBBitMask);
}

unsigned int GetSurfaceColorKey(const DDPIXELFORMAT& pf)
{
    return pf.dwRBitMask | pf.dwBBitMask;
}

void WritePackedPixel(unsigned char* dst, unsigned int bytesPerPixel, unsigned int value)
{
    switch (bytesPerPixel) {
    case 4:
        *reinterpret_cast<unsigned int*>(dst) = value;
        break;
    case 3:
        dst[0] = static_cast<unsigned char>(value & 0xFFu);
        dst[1] = static_cast<unsigned char>((value >> 8) & 0xFFu);
        dst[2] = static_cast<unsigned char>((value >> 16) & 0xFFu);
        break;
    case 2:
        *reinterpret_cast<unsigned short*>(dst) = static_cast<unsigned short>(value & 0xFFFFu);
        break;
    case 1:
        *dst = static_cast<unsigned char>(value & 0xFFu);
        break;
    default:
        break;
    }
}

} // namespace

float CTexture::m_uOffset = 0.0f;
float CTexture::m_vOffset = 0.0f;

CSurface::CSurface(unsigned int w, unsigned int h)
    : m_pddsSurface(nullptr), m_w(w), m_h(h), m_hBitmap(nullptr) {}

CSurface::CSurface(unsigned int w, unsigned int h, IDirectDrawSurface7* pSurface)
    : m_pddsSurface(pSurface), m_w(w), m_h(h), m_hBitmap(nullptr) {}

CSurface::~CSurface() {
    if (m_pddsSurface) {
        m_pddsSurface->Release();
        m_pddsSurface = nullptr;
    }
    if (m_hBitmap) {
        DeleteObject(m_hBitmap);
        m_hBitmap = nullptr;
    }
}

bool CSurface::Create(unsigned int w, unsigned int h) {
    m_w = w;
    m_h = h;
    return true;
}

void CSurface::ClearSurface(RECT* r, unsigned int col) {(void)r; (void)col;}
void CSurface::DrawSurface(int x, int y, int w, int h, unsigned int flags) {(void)x; (void)y; (void)w; (void)h; (void)flags;}

void CSurface::DrawSurfaceStretch(int x, int y, int w, int h) {
    DbgLog("[DrawSurfaceStretch] hBitmap=%p hWnd=%p x=%d y=%d w=%d h=%d\n",
           (void*)m_hBitmap, (void*)GetRenderDevice().GetWindowHandle(), x, y, w, h);
    if (!m_hBitmap || !GetRenderDevice().GetWindowHandle()) {
        DbgLog("[DrawSurfaceStretch] SKIP: null handle\n");
        return;
    }

    HDC target = GetDC(GetRenderDevice().GetWindowHandle());
    if (!target) {
        return;
    }

    HDC src = CreateCompatibleDC(target);
    if (!src) {
        ReleaseDC(GetRenderDevice().GetWindowHandle(), target);
        return;
    }

    BITMAP bm{};
    GetObjectA(m_hBitmap, sizeof(bm), &bm);

    HGDIOBJ old = SelectObject(src, m_hBitmap);
    SetStretchBltMode(target, HALFTONE);
    BOOL blitOk = StretchBlt(target, x, y, w, h, src, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
    DbgLog("[DrawSurfaceStretch] StretchBlt(%d,%d,%d,%d from %dx%d) -> %d (err=%lu)\n",
           x, y, w, h, bm.bmWidth, bm.bmHeight, (int)blitOk, blitOk ? 0UL : GetLastError());
    SelectObject(src, old);

    DeleteDC(src);
    ReleaseDC(GetRenderDevice().GetWindowHandle(), target);
}

void CSurface::Update(int x, int y, int w, int h, unsigned int* data, bool b, int p) {
    (void)x; (void)y; (void)w; (void)h; (void)data; (void)b; (void)p;
}

CSurfaceWallpaper::CSurfaceWallpaper(unsigned int w, unsigned int h)
    : CSurface(w, h) {}

CSurfaceWallpaper::~CSurfaceWallpaper() {}

void CSurfaceWallpaper::Update(int x, int y, int w, int h, unsigned int* data, bool b, int p) {
    (void)x; (void)y; (void)b; (void)p;

    if (!data || w <= 0 || h <= 0) {
        return;
    }

    if (m_hBitmap) {
        DeleteObject(m_hBitmap);
        m_hBitmap = nullptr;
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    m_hBitmap = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!m_hBitmap || !bits) {
        return;
    }

    std::copy(data, data + (w * h), static_cast<unsigned int*>(bits));
    m_w = static_cast<unsigned int>(w);
    m_h = static_cast<unsigned int>(h);
}

CTexture::CTexture(unsigned int w, unsigned int h, PixelFormat format)
    : CSurface(w, h), m_pf(format), m_lockCnt(0), m_timeStamp(0), m_updateWidth(0), m_updateHeight(0),
            m_backendTextureObject(nullptr), m_backendTextureView(nullptr), m_backendTextureUpload(nullptr) {
    m_texName[0] = '\0';
}

CTexture::CTexture(unsigned int w, unsigned int h, PixelFormat format, IDirectDrawSurface7* pSurface)
    : CSurface(w, h, pSurface), m_pf(format), m_lockCnt(0), m_timeStamp(0), m_updateWidth(0), m_updateHeight(0),
            m_backendTextureObject(nullptr), m_backendTextureView(nullptr), m_backendTextureUpload(nullptr) {
    m_texName[0] = '\0';
}

CTexture::~CTexture() {
    GetRenderDevice().ReleaseTextureResource(this);
}

bool CTexture::Create(unsigned int w, unsigned int h, PixelFormat format) {
    GetRenderDevice().ReleaseTextureResource(this);

    unsigned int textureW = 0;
    unsigned int textureH = 0;
    if (!GetRenderDevice().CreateTextureResource(this, w, h, static_cast<int>(format), &textureW, &textureH)) {
        if constexpr (kLogTexture) {
            DbgLog("[Texture] CreateTextureResource failed name='%s' requested=%ux%u\n", m_texName, w, h);
        }
        return false;
    }

    m_w = textureW;
    m_h = textureH;
    m_pf = format;
    const unsigned int scale = static_cast<unsigned int>(GetTextureUpscaleFactor());
    SetUVAdjust(w * scale, h * scale);
    return true;
}
bool CTexture::CreateBump(unsigned int w, unsigned int h) { return Create(w, h, PF_BUMP); }
bool CTexture::CreateBump(unsigned int w, unsigned int h, IDirectDrawSurface7* pSurface) { (void)pSurface; return Create(w, h, PF_BUMP); }
void CTexture::SetUVAdjust(unsigned int width, unsigned int height)
{
    unsigned int adjustedWidth = width;
    while (adjustedWidth > m_w) {
        adjustedWidth >>= 1;
    }

    unsigned int adjustedHeight = height;
    while (adjustedHeight > m_h) {
        adjustedHeight >>= 1;
    }

    m_updateWidth = adjustedWidth;
    m_updateHeight = adjustedHeight;
}
void CTexture::UpdateMipmap(RECT* r) {(void)r;}

bool CTexture::CopyTexture(CTexture* tex, int, int, int, int, int, int, int, int) { return tex != nullptr; }
int CTexture::Lock() { return 0; }
int CTexture::Unlock() { return 0; }

void CTexture::ClearSurface(RECT* r, unsigned int col) { CSurface::ClearSurface(r, col); }
void CTexture::DrawSurface(int x, int y, int w, int h, unsigned int flags) { CSurface::DrawSurface(x, y, w, h, flags); }
void CTexture::DrawSurfaceStretch(int x, int y, int w, int h) { CSurface::DrawSurfaceStretch(x, y, w, h); }
void CTexture::Update(int x, int y, int w, int h, unsigned int* data, bool b, int p) {
    const int scale = GetTextureUpscaleFactor();
    const int scaledX = x * scale;
    const int scaledY = y * scale;
    const int scaledW = w * scale;
    const int scaledH = h * scale;

    const unsigned int* uploadData = data;
    int uploadPitch = p;
    std::vector<unsigned int> scaledPixels;
    if (scale > 1 && data && w > 0 && h > 0) {
        const int srcPitch = p > 0 ? p : w * static_cast<int>(sizeof(unsigned int));
        scaledPixels.resize(static_cast<size_t>(scaledW) * static_cast<size_t>(scaledH));
        for (int dstY = 0; dstY < scaledH; ++dstY) {
            const int srcY = dstY / scale;
            const unsigned int* srcRow = reinterpret_cast<const unsigned int*>(reinterpret_cast<const unsigned char*>(data) + static_cast<size_t>(srcY) * static_cast<size_t>(srcPitch));
            unsigned int* dstRow = scaledPixels.data() + static_cast<size_t>(dstY) * static_cast<size_t>(scaledW);
            for (int dstX = 0; dstX < scaledW; ++dstX) {
                dstRow[dstX] = srcRow[dstX / scale];
            }
        }
        uploadData = scaledPixels.data();
        uploadPitch = scaledW * static_cast<int>(sizeof(unsigned int));
    }

    if (!GetRenderDevice().UpdateTextureResource(this, scaledX, scaledY, scaledW, scaledH, uploadData, b, uploadPitch)) {
        return;
    }
    m_updateWidth = (std::max)(m_updateWidth, static_cast<unsigned int>(scaledX + scaledW));
    m_updateHeight = (std::max)(m_updateHeight, static_cast<unsigned int>(scaledY + scaledH));
}

void CTexture::UpdateSprite(int a, int b, int c, int d, SprImg& img, unsigned int* pal) { (void)a; (void)b; (void)c; (void)d; (void)img; (void)pal; }
