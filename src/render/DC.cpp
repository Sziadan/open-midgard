#include "DC.h"
#include "res/Sprite.h"
#include "res/ActRes.h"
#include <cstring>
#include <algorithm>
#include <vector>

#if RO_PLATFORM_WINDOWS
#pragma comment(lib, "msimg32.lib")
#endif

ArgbDibSurface::ArgbDibSurface()
    : m_dc(nullptr), m_bitmap(nullptr), m_oldBitmap(nullptr), m_bits(nullptr), m_width(0), m_height(0)
{
}

ArgbDibSurface::~ArgbDibSurface()
{
    Release();
}

bool ArgbDibSurface::EnsureSize(int width, int height)
{
    if (width <= 0 || height <= 0) {
        return false;
    }
    if (IsValid() && m_width == width && m_height == height) {
        return true;
    }

    Release();

    m_dc = CreateCompatibleDC(nullptr);
    if (!m_dc) {
        return false;
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    m_bitmap = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &m_bits, nullptr, 0);
    if (!m_bitmap || !m_bits) {
        Release();
        return false;
    }

    m_oldBitmap = SelectObject(m_dc, m_bitmap);
    m_width = width;
    m_height = height;
    return true;
}

void ArgbDibSurface::Release()
{
    if (m_dc && m_oldBitmap) {
        SelectObject(m_dc, m_oldBitmap);
    }
    m_oldBitmap = nullptr;

    if (m_bitmap) {
        DeleteObject(m_bitmap);
        m_bitmap = nullptr;
    }
    m_bits = nullptr;

    if (m_dc) {
        DeleteDC(m_dc);
        m_dc = nullptr;
    }

    m_width = 0;
    m_height = 0;
}

bool ArgbDibSurface::IsValid() const
{
    return m_dc != nullptr && m_bitmap != nullptr && m_bits != nullptr && m_width > 0 && m_height > 0;
}

HDC ArgbDibSurface::GetDC() const
{
    return m_dc;
}

void* ArgbDibSurface::GetBits() const
{
    return m_bits;
}

unsigned int* ArgbDibSurface::GetPixels() const
{
    return static_cast<unsigned int*>(m_bits);
}

int ArgbDibSurface::GetWidth() const
{
    return m_width;
}

int ArgbDibSurface::GetHeight() const
{
    return m_height;
}

unsigned int PremultiplyColor(unsigned int color)
{
    const unsigned int alpha = (color >> 24) & 0xFFu;
    if (alpha == 0 || alpha == 0xFFu) {
        return color;
    }

    const unsigned int red = ((color >> 16) & 0xFFu) * alpha / 255u;
    const unsigned int green = ((color >> 8) & 0xFFu) * alpha / 255u;
    const unsigned int blue = (color & 0xFFu) * alpha / 255u;
    return (alpha << 24) | (red << 16) | (green << 8) | blue;
}

unsigned int ModulateColor(unsigned int src, const CSprClip& clip)
{
    const unsigned int srcA = (src >> 24) & 0xFF;
    const unsigned int srcR = (src >> 16) & 0xFF;
    const unsigned int srcG = (src >> 8) & 0xFF;
    const unsigned int srcB = src & 0xFF;

    const unsigned int outA = srcA * clip.a / 255u;
    const unsigned int outR = srcR * clip.r / 255u;
    const unsigned int outG = srcG * clip.g / 255u;
    const unsigned int outB = srcB * clip.b / 255u;
    return (outA << 24) | (outR << 16) | (outG << 8) | outB;
}

void AlphaBlendPixel(unsigned int& dst, unsigned int src)
{
    const unsigned int srcA = (src >> 24) & 0xFF;
    if (srcA == 0) {
        return;
    }
    if (srcA == 0xFF) {
        dst = src;
        return;
    }

    const unsigned int dstA = (dst >> 24) & 0xFF;
    const unsigned int srcR = (src >> 16) & 0xFF;
    const unsigned int srcG = (src >> 8) & 0xFF;
    const unsigned int srcB = src & 0xFF;
    const unsigned int dstR = (dst >> 16) & 0xFF;
    const unsigned int dstG = (dst >> 8) & 0xFF;
    const unsigned int dstB = dst & 0xFF;

    const unsigned int invA = 255 - srcA;
    const unsigned int outA = srcA + (dstA * invA) / 255u;
    const unsigned int outR = srcR + (dstR * invA) / 255u;
    const unsigned int outG = srcG + (dstG * invA) / 255u;
    const unsigned int outB = srcB + (dstB * invA) / 255u;

    dst = (outA << 24) | (outR << 16) | (outG << 8) | outB;
}

bool TryAlphaBlendArgbToDibSection(HDC hdc,
                                   int dstX,
                                   int dstY,
                                   int dstWidth,
                                   int dstHeight,
                                   const unsigned int* pixels,
                                   int pixelWidth,
                                   int pixelHeight,
                                   int srcX,
                                   int srcY,
                                   int srcWidth,
                                   int srcHeight)
{
    if (!hdc || !pixels || dstWidth != srcWidth || dstHeight != srcHeight) {
        return false;
    }

    HGDIOBJ bitmapObject = GetCurrentObject(hdc, OBJ_BITMAP);
    if (!bitmapObject) {
        return false;
    }

    DIBSECTION dibSection{};
    if (GetObjectA(bitmapObject, sizeof(dibSection), &dibSection) != sizeof(dibSection)) {
        return false;
    }
    if (!dibSection.dsBm.bmBits || dibSection.dsBm.bmBitsPixel != 32 || dibSection.dsBm.bmWidth <= 0 || dibSection.dsBm.bmHeight == 0) {
        return false;
    }

    const int targetWidth = dibSection.dsBm.bmWidth;
    const int targetHeight = std::abs(dibSection.dsBm.bmHeight);
    if (dstX < 0 || dstY < 0 || dstX + dstWidth > targetWidth || dstY + dstHeight > targetHeight) {
        return false;
    }

    const bool topDown = dibSection.dsBmih.biHeight < 0;
    const int stridePixels = dibSection.dsBm.bmWidthBytes / static_cast<int>(sizeof(unsigned int));
    if (stridePixels < targetWidth) {
        return false;
    }

    unsigned int* targetBits = static_cast<unsigned int*>(dibSection.dsBm.bmBits);
    for (int row = 0; row < srcHeight; ++row) {
        const int destRowIndex = topDown ? (dstY + row) : (targetHeight - 1 - (dstY + row));
        unsigned int* dstRow = targetBits + static_cast<size_t>(destRowIndex) * static_cast<size_t>(stridePixels) + static_cast<size_t>(dstX);
        const unsigned int* srcRow = pixels + static_cast<size_t>(srcY + row) * static_cast<size_t>(pixelWidth) + static_cast<size_t>(srcX);
        for (int col = 0; col < srcWidth; ++col) {
            AlphaBlendPixel(dstRow[col], srcRow[col]);
        }
    }

    return true;
}

void BlitMotionToArgb(unsigned int* dest, int destW, int destH, int baseX, int baseY, CSprRes* sprRes, const CMotion* motion, unsigned int* palette)
{
    if (!dest || !sprRes || !motion || !palette) {
        return;
    }

    for (const CSprClip& clip : motion->sprClips) {
        if (clip.sprIndex < 0) {
            continue;
        }

        const SprImg* image = sprRes->GetSprite(clip.clipType, clip.sprIndex);
        if (!image || image->width <= 0 || image->height <= 0) {
            continue;
        }

        const bool flipX = (clip.flags & 1) != 0;
        const int imgOffX = clip.x - image->width / 2;
        const int imgOffY = clip.y - image->height / 2;
        for (int sy = 0; sy < image->height; ++sy) {
            const int dy = baseY + imgOffY + sy;
            if (dy < 0 || dy >= destH) {
                continue;
            }

            for (int sx = 0; sx < image->width; ++sx) {
                const int sourceX = flipX ? (image->width - 1 - sx) : sx;
                const int dx = baseX + imgOffX + sx;
                if (dx < 0 || dx >= destW) {
                    continue;
                }

                unsigned int srcColor = 0;
                if (clip.clipType == 0) {
                    const unsigned char index = image->indices[static_cast<size_t>(sy) * image->width + sourceX];
                    if (index == 0) {
                        continue;
                    }
                    srcColor = 0xFF000000u | (palette[index] & 0x00FFFFFFu);
                } else {
                    srcColor = image->rgba[static_cast<size_t>(sy) * image->width + sourceX];
                    if ((srcColor >> 24) == 0) {
                        continue;
                    }
                }

                srcColor = ModulateColor(srcColor, clip);
                srcColor = PremultiplyColor(srcColor);
                AlphaBlendPixel(dest[static_cast<size_t>(dy) * destW + dx], srcColor);
            }
        }
    }
}

bool StretchArgbToHdc(HDC hdc,
                      int dstX,
                      int dstY,
                      int dstWidth,
                      int dstHeight,
                      const unsigned int* pixels,
                      int pixelWidth,
                      int pixelHeight,
                      int srcX,
                      int srcY,
                      int srcWidth,
                      int srcHeight)
{
    if (!hdc || !pixels || pixelWidth <= 0 || pixelHeight <= 0 || dstWidth <= 0 || dstHeight <= 0) {
        return false;
    }

    if (srcWidth < 0) {
        srcWidth = pixelWidth - srcX;
    }
    if (srcHeight < 0) {
        srcHeight = pixelHeight - srcY;
    }
    if (srcX < 0 || srcY < 0 || srcWidth <= 0 || srcHeight <= 0
        || srcX + srcWidth > pixelWidth || srcY + srcHeight > pixelHeight) {
        return false;
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = pixelWidth;
    bmi.bmiHeader.biHeight = -pixelHeight;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    return StretchDIBits(hdc,
                         dstX,
                         dstY,
                         dstWidth,
                         dstHeight,
                         srcX,
                         srcY,
                         srcWidth,
                         srcHeight,
                         pixels,
                         &bmi,
                         DIB_RGB_COLORS,
                         SRCCOPY) != GDI_ERROR;
}

bool AlphaBlendArgbToHdc(HDC hdc,
                         int dstX,
                         int dstY,
                         int dstWidth,
                         int dstHeight,
                         const unsigned int* pixels,
                         int pixelWidth,
                         int pixelHeight,
                         int srcX,
                         int srcY,
                         int srcWidth,
                         int srcHeight)
{
    if (!hdc || !pixels || pixelWidth <= 0 || pixelHeight <= 0 || dstWidth <= 0 || dstHeight <= 0) {
        return false;
    }

    if (srcWidth < 0) {
        srcWidth = pixelWidth - srcX;
    }
    if (srcHeight < 0) {
        srcHeight = pixelHeight - srcY;
    }
    if (srcX < 0 || srcY < 0 || srcWidth <= 0 || srcHeight <= 0
        || srcX + srcWidth > pixelWidth || srcY + srcHeight > pixelHeight) {
        return false;
    }

    if (TryAlphaBlendArgbToDibSection(hdc,
                                      dstX,
                                      dstY,
                                      dstWidth,
                                      dstHeight,
                                      pixels,
                                      pixelWidth,
                                      pixelHeight,
                                      srcX,
                                      srcY,
                                      srcWidth,
                                      srcHeight)) {
        return true;
    }

    static thread_local ArgbDibSurface s_blendSurface;
    if (!s_blendSurface.EnsureSize(pixelWidth, pixelHeight)) {
        return false;
    }

    std::memcpy(s_blendSurface.GetBits(),
                pixels,
                static_cast<size_t>(pixelWidth) * static_cast<size_t>(pixelHeight) * sizeof(unsigned int));

    BLENDFUNCTION blend{};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    const BOOL ok = AlphaBlend(hdc,
                               dstX,
                               dstY,
                               dstWidth,
                               dstHeight,
                               s_blendSurface.GetDC(),
                               srcX,
                               srcY,
                               srcWidth,
                               srcHeight,
                               blend);
    return ok == TRUE;
}

bool DrawActMotionToHdc(HDC hdc, int x, int y, class CSprRes* sprRes, const struct CMotion* motion, unsigned int* palette)
{
    if (!hdc || !sprRes || !motion || !palette) {
        return false;
    }

    RECT clipBox{};
    bool hasClip = false;
    for (const CSprClip& clip : motion->sprClips) {
        const SprImg* image = sprRes->GetSprite(clip.clipType, clip.sprIndex);
        if (!image) {
            continue;
        }

        const int drawX = clip.x - image->width / 2;
        const int drawY = clip.y - image->height / 2;
        RECT current = { drawX, drawY, drawX + image->width, drawY + image->height };
        if (!hasClip) {
            clipBox = current;
            hasClip = true;
        } else {
            clipBox.left = (std::min)(clipBox.left, current.left);
            clipBox.top = (std::min)(clipBox.top, current.top);
            clipBox.right = (std::max)(clipBox.right, current.right);
            clipBox.bottom = (std::max)(clipBox.bottom, current.bottom);
        }
    }

    if (!hasClip) {
        return false;
    }

    const int width = (std::max)(1, static_cast<int>(clipBox.right - clipBox.left));
    const int height = (std::max)(1, static_cast<int>(clipBox.bottom - clipBox.top));
    std::vector<unsigned int> pixels(static_cast<size_t>(width) * height, 0);

    BlitMotionToArgb(pixels.data(), width, height, -clipBox.left, -clipBox.top, sprRes, motion, palette);

    return AlphaBlendArgbToHdc(hdc, x + clipBox.left, y + clipBox.top, width, height, pixels.data(), width, height);
}

bool DrawActMotionToArgb(unsigned int* dest, int destW, int destH, int x, int y, class CSprRes* sprRes, const struct CMotion* motion, unsigned int* palette)
{
    if (!dest || destW <= 0 || destH <= 0 || !sprRes || !motion || !palette) {
        return false;
    }

    RECT clipBox{};
    bool hasClip = false;
    for (const CSprClip& clip : motion->sprClips) {
        const SprImg* image = sprRes->GetSprite(clip.clipType, clip.sprIndex);
        if (!image) {
            continue;
        }

        const int drawX = clip.x - image->width / 2;
        const int drawY = clip.y - image->height / 2;
        RECT current = { drawX, drawY, drawX + image->width, drawY + image->height };
        if (!hasClip) {
            clipBox = current;
            hasClip = true;
        } else {
            clipBox.left = (std::min)(clipBox.left, current.left);
            clipBox.top = (std::min)(clipBox.top, current.top);
            clipBox.right = (std::max)(clipBox.right, current.right);
            clipBox.bottom = (std::max)(clipBox.bottom, current.bottom);
        }
    }

    if (!hasClip) {
        return false;
    }

    BlitMotionToArgb(dest, destW, destH, x, y, sprRes, motion, palette);
    return true;
}

