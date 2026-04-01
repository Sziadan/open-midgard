#include "DC.h"
#include "Renderer.h"
#include "res/Sprite.h"
#include "res/ActRes.h"
#include <cstring>
#include <algorithm>

#pragma comment(lib, "msimg32.lib")

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

// --- CDCBitmap Implementation ---

CDCBitmap::CDCBitmap(unsigned int w, unsigned int h) : m_image(NULL), m_w(0), m_h(0), m_dirty(false) {
    CreateDCSurface(w, h);
}

CDCBitmap::~CDCBitmap() {
    for (auto tex : m_textureList) {
        delete tex;
    }
    m_textureList.clear();
    delete[] m_image;
}

void CDCBitmap::CreateDCSurface(unsigned int w, unsigned int h) {
    if (m_w == w && m_h == h && m_image) {
        return;
    }

    delete[] m_image;
    m_image = nullptr;
    m_w = w;
    m_h = h;
    m_dirty = true;

    if (m_w > 0 && m_h > 0) {
        m_image = new unsigned int[static_cast<size_t>(m_w) * static_cast<size_t>(m_h)]{};
    }
}

bool CDCBitmap::GetDC(HDC* phdc) {
    if (phdc) {
        *phdc = nullptr;
    }
    return false;
}

void CDCBitmap::ReleaseDC(HDC hdc) {
}

void CDCBitmap::BltSprite(int x, int y, class CSprRes* sprRes, struct CMotion* curMotion, unsigned int* palette) {
    BlitMotionToArgb(m_image, static_cast<int>(m_w), static_cast<int>(m_h), x, y, sprRes, curMotion, palette);
    m_dirty = true;
}

void CDCBitmap::BltTexture2(int x, int y, class CTexture* src, int srcx, int srcy, int w, int h, int xflip, int zoomx, int zoomy) {
    // Software stretch/blit to m_image...
    m_dirty = true;
}

void CDCBitmap::ClearSurface(RECT* rect, unsigned int color) {
    m_dirty = true;
    if (!rect) {
        for (unsigned int i = 0; i < m_w * m_h; ++i) m_image[i] = color;
    } else {
        // Clear rect region...
    }
}

void CDCBitmap::CopyRect(int x, int y, int w, int h, CDC* src) {
    m_dirty = true;
}

void CDCBitmap::DrawSurface(int x, int y, int w, int h, unsigned int color) {
    UpdateSurface();
    
    int tileX = 0, tileY = 0;
    auto it = m_textureList.begin();
    
    for (unsigned int cy = 0; cy < m_h; cy += 256) {
        int th = std::min(256u, m_h - cy);
        for (unsigned int cx = 0; cx < m_w; cx += 256) {
            int tw = std::min(256u, m_w - cx);
            if (it != m_textureList.end()) {
                (*it)->DrawSurface(x + cx, y + cy, tw, th, color);
                ++it;
            }
        }
    }
}

void CDCBitmap::UpdateSurface() {
    if (!m_dirty) return;

    // Ensure we have enough textures for tiles
    unsigned int numTilesX = (m_w + 255) / 256;
    unsigned int numTilesY = (m_h + 255) / 256;
    size_t required = numTilesX * numTilesY;

    while (m_textureList.size() < required) {
        m_textureList.push_back(new CTexture());
        m_textureList.back()->Create(256, 256, PF_DEFAULT, false);
    }

    auto it = m_textureList.begin();
    for (unsigned int cy = 0; cy < m_h; cy += 256) {
        unsigned int th = std::min(256u, m_h - cy);
        for (unsigned int cx = 0; cx < m_w; cx += 256) {
            unsigned int tw = std::min(256u, m_w - cx);
            if (it != m_textureList.end()) {
                // Update 256x256 texture from m_image sub-region
                (*it)->Update(0, 0, tw, th, &m_image[cx + cy * m_w], false, m_w);
                ++it;
            }
        }
    }
    m_dirty = false;
}

void CDCBitmap::Resize(int w, int h) {
    if ((unsigned int)w > m_w || (unsigned int)h > m_h) {
        CreateDCSurface(w, h);
    }
}

void CDCBitmap::Update(int x, int y, int w, int h, unsigned int* data, bool skipColorKey) {
    m_dirty = true;
    // Copy data to m_image...
}

// --- CDCSurface Implementation ---

CDCSurface::CDCSurface(unsigned int w, unsigned int h) {
    m_surface.Create(w, h);
}

CDCSurface::CDCSurface(unsigned int w, unsigned int h, IDirectDrawSurface7* pSurface) {
    // Wrap existing surface...
}

CDCSurface::~CDCSurface() {
}

bool CDCSurface::GetDC(HDC* phdc) {
    if (!m_surface.m_pddsSurface) return false;
    return SUCCEEDED(m_surface.m_pddsSurface->GetDC(phdc));
}

void CDCSurface::ReleaseDC(HDC hdc) {
    if (m_surface.m_pddsSurface) m_surface.m_pddsSurface->ReleaseDC(hdc);
}

void CDCSurface::BltSprite(int x, int y, class CSprRes* sprRes, struct CMotion* curMotion, unsigned int* palette) {
}

void CDCSurface::BltTexture2(int x, int y, class CTexture* src, int srcx, int srcy, int w, int h, int xflip, int zoomx, int zoomy) {
}

void CDCSurface::ClearSurface(RECT* rect, unsigned int color) {
    m_surface.ClearSurface(rect, color);
}

void CDCSurface::CopyRect(int x, int y, int w, int h, CDC* src) {
}

void CDCSurface::DrawSurface(int x, int y, int w, int h, unsigned int color) {
    m_surface.DrawSurface(x, y, w, h, color);
}

void CDCSurface::Resize(int w, int h) {
    if ((unsigned int)w > m_surface.m_w || (unsigned int)h > m_surface.m_h) {
        m_surface.Create(w, h);
    }
}

void CDCSurface::Update(int x, int y, int w, int h, unsigned int* data, bool skipColorKey) {
    m_surface.Update(x, y, w, h, data, skipColorKey, 0);
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

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    BLENDFUNCTION blend{};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    void* dibBits = nullptr;
    HBITMAP dib = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &dibBits, nullptr, 0);
    if (!dib || !dibBits) {
        if (dib) {
            DeleteObject(dib);
        }
        return false;
    }

    std::memcpy(dibBits, pixels.data(), pixels.size() * sizeof(unsigned int));
    HDC memDc = CreateCompatibleDC(hdc);
    if (!memDc) {
        DeleteObject(dib);
        return false;
    }

    HGDIOBJ oldBitmap = SelectObject(memDc, dib);
    AlphaBlend(hdc, x + clipBox.left, y + clipBox.top, width, height, memDc, 0, 0, width, height, blend);
    SelectObject(memDc, oldBitmap);
    DeleteDC(memDc);
    DeleteObject(dib);
    return true;
}

