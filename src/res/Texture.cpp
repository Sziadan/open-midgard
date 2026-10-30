#include "Texture.h"

#include "render3d/D3dutil.h"
#include "render3d/Device.h"
#include "render3d/GraphicsSettings.h"
#include "render3d/RenderDevice.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>
#include "../DebugLog.h"

namespace {

constexpr bool kLogTexture = false;

constexpr unsigned int kMaxUpscaledEdge = 512u;

unsigned int GetTextureUpscaleFactor(bool allowUpscale, unsigned int minUpscaleFactor)
{
    if (!allowUpscale) {
        return 1u;
    }

    const unsigned int configuredFactor = static_cast<unsigned int>((std::max)(1, GetCachedGraphicsSettings().textureUpscaleFactor));
    return (std::max)(configuredFactor, (std::max)(1u, minUpscaleFactor));
}

unsigned int ClampTextureUpscaleFactor(unsigned int width, unsigned int height, unsigned int scale)
{
    if (scale <= 1u || width == 0u || height == 0u) {
        return 1u;
    }

    // Keep the enlarged texture inside a size the frame can still sample.
    // Smaller sprites can still reach 4x; a 256 source stops at 2x.
    while (scale > 1u && (width > kMaxUpscaledEdge / scale || height > kMaxUpscaledEdge / scale)) {
        --scale;
    }
    return scale;
}

bool IsLightmapAtlasName(const char* name)
{
    return name != nullptr && std::strncmp(name, "__lightmap_atlas_", 17) == 0;
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

enum class TextureUpscaleKind {
    Nearest,
    Bilinear,
    PixelArt,
    Smooth,
};

unsigned int LoadPackedPixel(const unsigned int* data, int width, int height, int pitch, int x, int y)
{
    x = (std::max)(0, (std::min)(width - 1, x));
    y = (std::max)(0, (std::min)(height - 1, y));
    const unsigned char* row = reinterpret_cast<const unsigned char*>(data) + static_cast<size_t>(y) * static_cast<size_t>(pitch);
    return reinterpret_cast<const unsigned int*>(row)[x];
}

bool IsColorKeyPixel(unsigned int pixel)
{
    return (pixel & 0x00FFFFFFu) == 0x00FF00FFu;
}

unsigned int PixelForUpscale(unsigned int pixel, bool convertColorKey)
{
    if (convertColorKey && IsColorKeyPixel(pixel)) {
        return 0u;
    }
    return pixel;
}

void StoreChannel(float* pixel, int channel, float value)
{
    pixel[channel] = value;
}

float ClampByteFloat(float value)
{
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 255.0f) {
        return 255.0f;
    }
    return value;
}

unsigned int PackFilteredPixel(float alpha, float red, float green, float blue, bool restoreColorKey)
{
    alpha = ClampByteFloat(alpha);
    if (restoreColorKey && alpha < 8.0f) {
        return 0x00FF00FFu;
    }

    const auto channel = [](float value) {
        return static_cast<unsigned int>(ClampByteFloat(value) + 0.5f);
    };
    const unsigned int packedAlpha = channel(alpha);
    if (packedAlpha == 0u) {
        return restoreColorKey ? 0x00FF00FFu : 0u;
    }

    const float invAlpha = 255.0f / static_cast<float>(packedAlpha);
    return (packedAlpha << 24)
        | (channel(red * invAlpha) << 16)
        | (channel(green * invAlpha) << 8)
        | channel(blue * invAlpha);
}

TextureUpscaleKind ChooseTextureUpscaleKind(const char* textureName, const unsigned int* pixels, int width, int height, bool convertColorKey)
{
    if (IsLightmapAtlasName(textureName)) {
        return TextureUpscaleKind::Bilinear;
    }
    if (!pixels || width <= 0 || height <= 0) {
        return TextureUpscaleKind::Nearest;
    }

    const int pixelCount = width * height;
    const int stride = (std::max)(1, pixelCount / 4096);
    int samples = 0;
    int emptySamples = 0;
    int partialSamples = 0;
    for (int index = 0; index < pixelCount; index += stride) {
        const unsigned int pixel = PixelForUpscale(pixels[index], convertColorKey);
        const unsigned int alpha = (pixel >> 24) & 0xFFu;
        ++samples;
        if (alpha == 0u) {
            ++emptySamples;
        } else if (alpha < 250u) {
            ++partialSamples;
        }
    }
    if (samples <= 0) {
        return TextureUpscaleKind::Nearest;
    }

    // Soft images (ground, gradients, glows) reconstruct better with a cubic filter.
    // Hard-edged sprites and effects keep a pixel-art scaler so outlines stay crisp.
    if (partialSamples * 8 > samples) {
        return TextureUpscaleKind::Smooth;
    }
    if (emptySamples * 25 > samples) {
        return TextureUpscaleKind::PixelArt;
    }
    return TextureUpscaleKind::Smooth;
}

void BuildNearestUpscale(const unsigned int* src, int srcWidth, int srcHeight, int scale, bool convertColorKey, bool restoreColorKey, std::vector<unsigned int>* dst)
{
    const int dstWidth = srcWidth * scale;
    const int dstHeight = srcHeight * scale;
    dst->resize(static_cast<size_t>(dstWidth) * static_cast<size_t>(dstHeight));
    for (int y = 0; y < dstHeight; ++y) {
        const unsigned int* srcRow = src + static_cast<size_t>(y / scale) * static_cast<size_t>(srcWidth);
        unsigned int* dstRow = dst->data() + static_cast<size_t>(y) * static_cast<size_t>(dstWidth);
        for (int x = 0; x < dstWidth; ++x) {
            unsigned int pixel = PixelForUpscale(srcRow[x / scale], convertColorKey);
            if (restoreColorKey && ((pixel >> 24) & 0xFFu) == 0u) {
                pixel = 0x00FF00FFu;
            }
            dstRow[x] = pixel;
        }
    }
}

unsigned int LerpByte(unsigned int a, unsigned int b, unsigned int weight)
{
    const int delta = static_cast<int>(b) - static_cast<int>(a);
    return static_cast<unsigned int>(static_cast<int>(a) + (delta * static_cast<int>(weight)) / 255);
}

unsigned int LerpPixel(unsigned int p0, unsigned int p1, unsigned int weight)
{
    return (LerpByte((p0 >> 24) & 0xFFu, (p1 >> 24) & 0xFFu, weight) << 24)
        | (LerpByte((p0 >> 16) & 0xFFu, (p1 >> 16) & 0xFFu, weight) << 16)
        | (LerpByte((p0 >> 8) & 0xFFu, (p1 >> 8) & 0xFFu, weight) << 8)
        | LerpByte(p0 & 0xFFu, p1 & 0xFFu, weight);
}

void BuildBilinearUpscale(const unsigned int* src, int srcWidth, int srcHeight, int scale, bool convertColorKey, bool restoreColorKey, std::vector<unsigned int>* dst)
{
    const int dstWidth = srcWidth * scale;
    const int dstHeight = srcHeight * scale;
    dst->resize(static_cast<size_t>(dstWidth) * static_cast<size_t>(dstHeight));
    for (int y = 0; y < dstHeight; ++y) {
        const int sampleY = y * 256 + 128;
        const int srcYFixed = sampleY / scale - 128;
        const int y0 = srcYFixed < 0 ? 0 : (std::min)(srcHeight - 1, srcYFixed / 256);
        const int y1 = (std::min)(srcHeight - 1, y0 + 1);
        const unsigned int fy = static_cast<unsigned int>(srcYFixed < 0 ? 0 : srcYFixed & 255);
        const unsigned int* row0 = src + static_cast<size_t>(y0) * static_cast<size_t>(srcWidth);
        const unsigned int* row1 = src + static_cast<size_t>(y1) * static_cast<size_t>(srcWidth);
        unsigned int* dstRow = dst->data() + static_cast<size_t>(y) * static_cast<size_t>(dstWidth);
        for (int x = 0; x < dstWidth; ++x) {
            const int sampleX = x * 256 + 128;
            const int srcXFixed = sampleX / scale - 128;
            const int x0 = srcXFixed < 0 ? 0 : (std::min)(srcWidth - 1, srcXFixed / 256);
            const int x1 = (std::min)(srcWidth - 1, x0 + 1);
            const unsigned int fx = static_cast<unsigned int>(srcXFixed < 0 ? 0 : srcXFixed & 255);
            unsigned int pixel = LerpPixel(
                LerpPixel(PixelForUpscale(row0[x0], convertColorKey), PixelForUpscale(row0[x1], convertColorKey), fx),
                LerpPixel(PixelForUpscale(row1[x0], convertColorKey), PixelForUpscale(row1[x1], convertColorKey), fx),
                fy);
            if (restoreColorKey && ((pixel >> 24) & 0xFFu) == 0u) {
                pixel = 0x00FF00FFu;
            }
            dstRow[x] = pixel;
        }
    }
}

void Scale2xImage(const unsigned int* src, int srcWidth, int srcHeight, std::vector<unsigned int>* dst)
{
    const int dstWidth = srcWidth * 2;
    const int dstHeight = srcHeight * 2;
    dst->resize(static_cast<size_t>(dstWidth) * static_cast<size_t>(dstHeight));
    for (int y = 0; y < srcHeight; ++y) {
        unsigned int* dstRow0 = dst->data() + static_cast<size_t>(y * 2) * static_cast<size_t>(dstWidth);
        unsigned int* dstRow1 = dstRow0 + dstWidth;
        for (int x = 0; x < srcWidth; ++x) {
            const unsigned int b = LoadPackedPixel(src, srcWidth, srcHeight, srcWidth * static_cast<int>(sizeof(unsigned int)), x, y - 1);
            const unsigned int d = LoadPackedPixel(src, srcWidth, srcHeight, srcWidth * static_cast<int>(sizeof(unsigned int)), x - 1, y);
            const unsigned int e = src[static_cast<size_t>(y) * static_cast<size_t>(srcWidth) + static_cast<size_t>(x)];
            const unsigned int f = LoadPackedPixel(src, srcWidth, srcHeight, srcWidth * static_cast<int>(sizeof(unsigned int)), x + 1, y);
            const unsigned int h = LoadPackedPixel(src, srcWidth, srcHeight, srcWidth * static_cast<int>(sizeof(unsigned int)), x, y + 1);
            dstRow0[x * 2] = (d == b && b != f && d != h) ? d : e;
            dstRow0[x * 2 + 1] = (b == f && b != d && f != h) ? f : e;
            dstRow1[x * 2] = (d == h && d != b && h != f) ? d : e;
            dstRow1[x * 2 + 1] = (h == f && d != h && b != f) ? f : e;
        }
    }
}

void Scale3xImage(const unsigned int* src, int srcWidth, int srcHeight, std::vector<unsigned int>* dst)
{
    const int dstWidth = srcWidth * 3;
    const int dstHeight = srcHeight * 3;
    dst->resize(static_cast<size_t>(dstWidth) * static_cast<size_t>(dstHeight));
    const int srcPitch = srcWidth * static_cast<int>(sizeof(unsigned int));
    for (int y = 0; y < srcHeight; ++y) {
        unsigned int* row0 = dst->data() + static_cast<size_t>(y * 3) * static_cast<size_t>(dstWidth);
        unsigned int* row1 = row0 + dstWidth;
        unsigned int* row2 = row1 + dstWidth;
        for (int x = 0; x < srcWidth; ++x) {
            const unsigned int a = LoadPackedPixel(src, srcWidth, srcHeight, srcPitch, x - 1, y - 1);
            const unsigned int b = LoadPackedPixel(src, srcWidth, srcHeight, srcPitch, x, y - 1);
            const unsigned int c = LoadPackedPixel(src, srcWidth, srcHeight, srcPitch, x + 1, y - 1);
            const unsigned int d = LoadPackedPixel(src, srcWidth, srcHeight, srcPitch, x - 1, y);
            const unsigned int e = src[static_cast<size_t>(y) * static_cast<size_t>(srcWidth) + static_cast<size_t>(x)];
            const unsigned int f = LoadPackedPixel(src, srcWidth, srcHeight, srcPitch, x + 1, y);
            const unsigned int g = LoadPackedPixel(src, srcWidth, srcHeight, srcPitch, x - 1, y + 1);
            const unsigned int h = LoadPackedPixel(src, srcWidth, srcHeight, srcPitch, x, y + 1);
            const unsigned int i = LoadPackedPixel(src, srcWidth, srcHeight, srcPitch, x + 1, y + 1);
            const int dx = x * 3;
            row0[dx] = (d == b && d != h && b != f) ? d : e;
            row0[dx + 1] = ((d == b && d != h && b != f && e != c) || (b == f && b != d && f != h && e != a)) ? b : e;
            row0[dx + 2] = (b == f && b != d && f != h) ? f : e;
            row1[dx] = ((d == b && d != h && b != f && e != g) || (d == h && d != b && h != f && e != a)) ? d : e;
            row1[dx + 1] = e;
            row1[dx + 2] = ((b == f && b != d && f != h && e != i) || (h == f && d != h && b != f && e != c)) ? f : e;
            row2[dx] = (d == h && d != b && h != f) ? d : e;
            row2[dx + 1] = ((d == h && d != b && h != f && e != i) || (h == f && d != h && b != f && e != g)) ? h : e;
            row2[dx + 2] = (h == f && d != h && b != f) ? f : e;
        }
    }
}

void BuildPixelArtUpscale(const unsigned int* src, int srcWidth, int srcHeight, int scale, bool convertColorKey, std::vector<unsigned int>* dst)
{
    std::vector<unsigned int> packed(static_cast<size_t>(srcWidth) * static_cast<size_t>(srcHeight));
    for (int index = 0; index < srcWidth * srcHeight; ++index) {
        packed[static_cast<size_t>(index)] = PixelForUpscale(src[index], convertColorKey);
    }

    if (scale == 3) {
        Scale3xImage(packed.data(), srcWidth, srcHeight, dst);
        return;
    }

    std::vector<unsigned int> current = std::move(packed);
    int currentWidth = srcWidth;
    int currentHeight = srcHeight;
    int steps = 0;
    int produced = 1;
    while (produced < scale && steps < 2) {
        std::vector<unsigned int> next;
        Scale2xImage(current.data(), currentWidth, currentHeight, &next);
        currentWidth *= 2;
        currentHeight *= 2;
        produced *= 2;
        current.swap(next);
        ++steps;
    }
    dst->swap(current);
}

void BuildSmoothUpscale(const unsigned int* src, int srcWidth, int srcHeight, int scale, bool convertColorKey, bool restoreColorKey, std::vector<unsigned int>* dst)
{
    const int dstWidth = srcWidth * scale;
    const int dstHeight = srcHeight * scale;
    std::vector<float> source(static_cast<size_t>(srcWidth) * static_cast<size_t>(srcHeight) * 4u);
    for (int index = 0; index < srcWidth * srcHeight; ++index) {
        const unsigned int pixel = PixelForUpscale(src[index], convertColorKey);
        const float alpha = static_cast<float>((pixel >> 24) & 0xFFu);
        const float scaleAlpha = alpha / 255.0f;
        float* out = source.data() + static_cast<size_t>(index) * 4u;
        out[0] = alpha;
        out[1] = static_cast<float>((pixel >> 16) & 0xFFu) * scaleAlpha;
        out[2] = static_cast<float>((pixel >> 8) & 0xFFu) * scaleAlpha;
        out[3] = static_cast<float>(pixel & 0xFFu) * scaleAlpha;
    }

    std::vector<float> horizontal(static_cast<size_t>(dstWidth) * static_cast<size_t>(srcHeight) * 4u);
    for (int y = 0; y < srcHeight; ++y) {
        const float* srcRow = source.data() + static_cast<size_t>(y) * static_cast<size_t>(srcWidth) * 4u;
        float* dstRow = horizontal.data() + static_cast<size_t>(y) * static_cast<size_t>(dstWidth) * 4u;
        for (int x = 0; x < dstWidth; ++x) {
            const float srcX = (static_cast<float>(x) + 0.5f) / static_cast<float>(scale) - 0.5f;
            const int x1 = static_cast<int>(std::floor(srcX));
            const float t = srcX - static_cast<float>(x1);
            const float t2 = t * t;
            const float t3 = t2 * t;
            const float w0 = 0.5f * ((-t3) + (2.0f * t2) - t);
            const float w1 = 0.5f * ((3.0f * t3) - (5.0f * t2) + 2.0f);
            const float w2 = 0.5f * ((-3.0f * t3) + (4.0f * t2) + t);
            const float w3 = 0.5f * (t3 - t2);
            const int xs[4] = {
                (std::max)(0, (std::min)(srcWidth - 1, x1 - 1)),
                (std::max)(0, (std::min)(srcWidth - 1, x1)),
                (std::max)(0, (std::min)(srcWidth - 1, x1 + 1)),
                (std::max)(0, (std::min)(srcWidth - 1, x1 + 2)),
            };
            const float weights[4] = { w0, w1, w2, w3 };
            float* out = dstRow + static_cast<size_t>(x) * 4u;
            out[0] = out[1] = out[2] = out[3] = 0.0f;
            for (int tap = 0; tap < 4; ++tap) {
                const float* sample = srcRow + static_cast<size_t>(xs[tap]) * 4u;
                for (int channel = 0; channel < 4; ++channel) {
                    StoreChannel(out, channel, out[channel] + sample[channel] * weights[tap]);
                }
            }
        }
    }

    dst->resize(static_cast<size_t>(dstWidth) * static_cast<size_t>(dstHeight));
    for (int y = 0; y < dstHeight; ++y) {
        const float srcY = (static_cast<float>(y) + 0.5f) / static_cast<float>(scale) - 0.5f;
        const int y1 = static_cast<int>(std::floor(srcY));
        const float t = srcY - static_cast<float>(y1);
        const float t2 = t * t;
        const float t3 = t2 * t;
        const float w0 = 0.5f * ((-t3) + (2.0f * t2) - t);
        const float w1 = 0.5f * ((3.0f * t3) - (5.0f * t2) + 2.0f);
        const float w2 = 0.5f * ((-3.0f * t3) + (4.0f * t2) + t);
        const float w3 = 0.5f * (t3 - t2);
        const int ys[4] = {
            (std::max)(0, (std::min)(srcHeight - 1, y1 - 1)),
            (std::max)(0, (std::min)(srcHeight - 1, y1)),
            (std::max)(0, (std::min)(srcHeight - 1, y1 + 1)),
            (std::max)(0, (std::min)(srcHeight - 1, y1 + 2)),
        };
        const float weights[4] = { w0, w1, w2, w3 };
        unsigned int* dstRow = dst->data() + static_cast<size_t>(y) * static_cast<size_t>(dstWidth);
        for (int x = 0; x < dstWidth; ++x) {
            float filtered[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            for (int tap = 0; tap < 4; ++tap) {
                const float* sample = horizontal.data()
                    + (static_cast<size_t>(ys[tap]) * static_cast<size_t>(dstWidth) + static_cast<size_t>(x)) * 4u;
                for (int channel = 0; channel < 4; ++channel) {
                    filtered[channel] += sample[channel] * weights[tap];
                }
            }
            dstRow[x] = PackFilteredPixel(filtered[0], filtered[1], filtered[2], filtered[3], restoreColorKey);
        }
    }
}

bool BuildTextureUpscale(const unsigned int* src, int srcWidth, int srcHeight, int srcPitch, int scale, const char* textureName,
    bool convertColorKey, std::vector<unsigned int>* dst)
{
    if (!src || !dst || srcWidth <= 0 || srcHeight <= 0 || scale <= 1) {
        return false;
    }

    const int packedPitch = srcWidth * static_cast<int>(sizeof(unsigned int));
    std::vector<unsigned int> packed;
    const unsigned int* packedSrc = src;
    if (srcPitch != packedPitch) {
        packed.resize(static_cast<size_t>(srcWidth) * static_cast<size_t>(srcHeight));
        for (int y = 0; y < srcHeight; ++y) {
            const unsigned char* srcRow = reinterpret_cast<const unsigned char*>(src) + static_cast<size_t>(y) * static_cast<size_t>(srcPitch);
            std::memcpy(packed.data() + static_cast<size_t>(y) * static_cast<size_t>(srcWidth), srcRow, static_cast<size_t>(packedPitch));
        }
        packedSrc = packed.data();
    }

    const TextureUpscaleKind kind = ChooseTextureUpscaleKind(textureName, packedSrc, srcWidth, srcHeight, convertColorKey);
    if (kind == TextureUpscaleKind::Nearest) {
        BuildNearestUpscale(packedSrc, srcWidth, srcHeight, scale, convertColorKey, convertColorKey, dst);
        return true;
    }
    if (kind == TextureUpscaleKind::Bilinear) {
        BuildBilinearUpscale(packedSrc, srcWidth, srcHeight, scale, convertColorKey, convertColorKey, dst);
        return true;
    }
    if (kind == TextureUpscaleKind::PixelArt) {
        BuildPixelArtUpscale(packedSrc, srcWidth, srcHeight, scale, convertColorKey, dst);
        if (convertColorKey) {
            for (unsigned int& pixel : *dst) {
                if (((pixel >> 24) & 0xFFu) == 0u) {
                    pixel = 0x00FF00FFu;
                }
            }
        }
        return true;
    }

    BuildSmoothUpscale(packedSrc, srcWidth, srcHeight, scale, convertColorKey, convertColorKey, dst);
    return true;
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
    : m_pddsSurface(nullptr), m_w(w), m_h(h) {}

CSurface::CSurface(unsigned int w, unsigned int h, IDirectDrawSurface7* pSurface)
    : m_pddsSurface(pSurface), m_w(w), m_h(h) {}

CSurface::~CSurface() {
#if RO_PLATFORM_WINDOWS
    if (m_pddsSurface) {
        m_pddsSurface->Release();
        m_pddsSurface = nullptr;
    }
#else
    m_pddsSurface = nullptr;
#endif
}

bool CSurface::Create(unsigned int w, unsigned int h) {
    m_w = w;
    m_h = h;
    return true;
}

void CSurface::ClearSurface(RECT* r, unsigned int col) {(void)r; (void)col;}
void CSurface::DrawSurface(int x, int y, int w, int h, unsigned int flags) {(void)x; (void)y; (void)w; (void)h; (void)flags;}

void CSurface::DrawSurfaceStretch(int x, int y, int w, int h) {
#if !RO_PLATFORM_WINDOWS
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    return;
#else
    DbgLog("[DrawSurfaceStretch] pixels=%p hWnd=%p x=%d y=%d w=%d h=%d\n",
           (const void*)GetSoftwarePixels(), (void*)GetRenderDevice().GetWindowHandle(), x, y, w, h);
    if (!HasSoftwarePixels() || !GetRenderDevice().GetWindowHandle()) {
        DbgLog("[DrawSurfaceStretch] SKIP: null backing data\n");
        return;
    }

    HDC target = GetDC(GetRenderDevice().GetWindowHandle());
    if (!target) {
        return;
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = static_cast<LONG>(m_w);
    bmi.bmiHeader.biHeight = -static_cast<LONG>(m_h);
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    SetStretchBltMode(target, HALFTONE);
    const int blitOk = StretchDIBits(target,
        x,
        y,
        w,
        h,
        0,
        0,
        static_cast<int>(m_w),
        static_cast<int>(m_h),
        GetSoftwarePixels(),
        &bmi,
        DIB_RGB_COLORS,
        SRCCOPY);
    DbgLog("[DrawSurfaceStretch] StretchBlt(%d,%d,%d,%d from %dx%d) -> %d (err=%lu)\n",
           x, y, w, h, static_cast<int>(m_w), static_cast<int>(m_h), blitOk, blitOk == GDI_ERROR ? GetLastError() : 0UL);
    ReleaseDC(GetRenderDevice().GetWindowHandle(), target);
#endif
}

void CSurface::Update(int x, int y, int w, int h, unsigned int* data, bool b, int p) {
    (void)x; (void)y; (void)w; (void)h; (void)data; (void)b; (void)p;
}

CSurfaceWallpaper::CSurfaceWallpaper(unsigned int w, unsigned int h)
    : CSurface(w, h) {}

CSurfaceWallpaper::~CSurfaceWallpaper() {}

void CSurfaceWallpaper::Update(int x, int y, int w, int h, unsigned int* data, bool b, int p) {
    (void)x; (void)y; (void)b;

    if (!data || w <= 0 || h <= 0) {
        return;
    }

    m_softwarePixels.resize(static_cast<size_t>(w) * static_cast<size_t>(h));
    const int sourcePitch = p > 0 ? p : w * static_cast<int>(sizeof(unsigned int));
    for (int row = 0; row < h; ++row) {
        const unsigned char* srcRow = reinterpret_cast<const unsigned char*>(data) + static_cast<size_t>(row) * static_cast<size_t>(sourcePitch);
        unsigned int* dstRow = m_softwarePixels.data() + static_cast<size_t>(row) * static_cast<size_t>(w);
        std::memcpy(dstRow, srcRow, static_cast<size_t>(w) * sizeof(unsigned int));
    }
    m_w = static_cast<unsigned int>(w);
    m_h = static_cast<unsigned int>(h);
}

CTexture::CTexture(unsigned int w, unsigned int h, PixelFormat format)
    : CSurface(w, h), m_pf(format), m_lockCnt(0), m_timeStamp(0), m_updateWidth(0), m_updateHeight(0), m_surfaceUpdateWidth(0), m_surfaceUpdateHeight(0),
            m_upscaleFactor(1), m_backendTextureObject(nullptr), m_backendTextureView(nullptr), m_backendTextureUpload(nullptr) {
    m_texName[0] = '\0';
}

CTexture::CTexture(unsigned int w, unsigned int h, PixelFormat format, IDirectDrawSurface7* pSurface)
    : CSurface(w, h, pSurface), m_pf(format), m_lockCnt(0), m_timeStamp(0), m_updateWidth(0), m_updateHeight(0), m_surfaceUpdateWidth(0), m_surfaceUpdateHeight(0),
            m_upscaleFactor(1), m_backendTextureObject(nullptr), m_backendTextureView(nullptr), m_backendTextureUpload(nullptr) {
    m_texName[0] = '\0';
}

CTexture::~CTexture() {
    GetRenderDevice().ReleaseTextureResource(this);
}

bool CTexture::Create(unsigned int w, unsigned int h, PixelFormat format, bool allowUpscale, unsigned int minUpscaleFactor) {
    GetRenderDevice().ReleaseTextureResource(this);

    m_upscaleFactor = GetTextureUpscaleFactor(allowUpscale, minUpscaleFactor);
    if (IsLightmapAtlasName(m_texName)) {
        // Lightmap atlases are large and already filtered on the GPU. Keep them
        // at a small integer enlarge so neighboring cells do not get expensive
        // reconstruction, and so 4x does not turn every atlas into a 1K texture.
        m_upscaleFactor = (std::min)(m_upscaleFactor, 2u);
    }
    m_upscaleFactor = ClampTextureUpscaleFactor(w, h, m_upscaleFactor);

    const unsigned int scaledWidth = (std::max)(1u, w * m_upscaleFactor);
    const unsigned int scaledHeight = (std::max)(1u, h * m_upscaleFactor);

    unsigned int textureW = 0;
    unsigned int textureH = 0;
    if (!GetRenderDevice().CreateTextureResource(this, scaledWidth, scaledHeight, static_cast<int>(format), &textureW, &textureH)) {
        if constexpr (kLogTexture) {
            DbgLog("[Texture] CreateTextureResource failed name='%s' requested=%ux%u\n", m_texName, w, h);
        }
        return false;
    }

    m_w = textureW;
    m_h = textureH;
    m_pf = format;
    const unsigned int contentWidth = (std::min)(textureW, scaledWidth);
    const unsigned int contentHeight = (std::min)(textureH, scaledHeight);
    // Both extents cover the uploaded texels. UV code divides either field by
    // the GPU texture size, so leaving the logical size here would crop the
    // enlarged image down to its top-left corner.
    m_updateWidth = contentWidth;
    m_updateHeight = contentHeight;
    m_surfaceUpdateWidth = contentWidth;
    m_surfaceUpdateHeight = contentHeight;
    return true;
}
bool CTexture::CreateBump(unsigned int w, unsigned int h, bool allowUpscale, unsigned int minUpscaleFactor) { return Create(w, h, PF_BUMP, allowUpscale, minUpscaleFactor); }
bool CTexture::CreateBump(unsigned int w, unsigned int h, IDirectDrawSurface7* pSurface, bool allowUpscale, unsigned int minUpscaleFactor) { (void)pSurface; return Create(w, h, PF_BUMP, allowUpscale, minUpscaleFactor); }
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
    m_surfaceUpdateWidth = adjustedWidth;
    m_surfaceUpdateHeight = adjustedHeight;
}
void CTexture::UpdateMipmap(RECT* r) {(void)r;}

bool CTexture::CopyTexture(CTexture* tex, int, int, int, int, int, int, int, int) { return tex != nullptr; }
int CTexture::Lock() { return 0; }
int CTexture::Unlock() { return 0; }

void CTexture::ClearSurface(RECT* r, unsigned int col) { CSurface::ClearSurface(r, col); }
void CTexture::DrawSurface(int x, int y, int w, int h, unsigned int flags) { CSurface::DrawSurface(x, y, w, h, flags); }
void CTexture::DrawSurfaceStretch(int x, int y, int w, int h) { CSurface::DrawSurfaceStretch(x, y, w, h); }
void CTexture::Update(int x, int y, int w, int h, unsigned int* data, bool b, int p) {
    const int scale = static_cast<int>((std::max)(1u, m_upscaleFactor));
    const int scaledX = x * scale;
    const int scaledY = y * scale;
    const int scaledW = w * scale;
    const int scaledH = h * scale;

    const unsigned int* uploadData = data;
    int uploadPitch = p;
    std::vector<unsigned int> scaledPixels;
    if (scale > 1 && data && w > 0 && h > 0) {
        const int srcPitch = p > 0 ? p : w * static_cast<int>(sizeof(unsigned int));
        if (!BuildTextureUpscale(data, w, h, srcPitch, scale, m_texName, !b, &scaledPixels)) {
            return;
        }
        uploadData = scaledPixels.data();
        uploadPitch = scaledW * static_cast<int>(sizeof(unsigned int));
    }

    if (!GetRenderDevice().UpdateTextureResource(this, scaledX, scaledY, scaledW, scaledH, uploadData, b, uploadPitch)) {
        return;
    }
    const unsigned int contentRight = static_cast<unsigned int>((std::max)(0, scaledX + scaledW));
    const unsigned int contentBottom = static_cast<unsigned int>((std::max)(0, scaledY + scaledH));
    m_updateWidth = (std::max)(m_updateWidth, contentRight);
    m_updateHeight = (std::max)(m_updateHeight, contentBottom);
    m_surfaceUpdateWidth = m_updateWidth;
    m_surfaceUpdateHeight = m_updateHeight;
}

void CTexture::UpdateSprite(int a, int b, int c, int d, SprImg& img, unsigned int* pal) { (void)a; (void)b; (void)c; (void)d; (void)img; (void)pal; }
