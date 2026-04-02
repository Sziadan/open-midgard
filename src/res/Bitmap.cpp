#include "Bitmap.h"
#include "DebugLog.h"
#include "core/File.h"

#if RO_PLATFORM_WINDOWS
#include <objidl.h>
#include <wincodec.h>
#elif RO_ENABLE_QT6_UI
#include <QByteArray>
#include <QImage>
#endif

#include <algorithm>
#include <cctype>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#if RO_PLATFORM_WINDOWS
#pragma comment(lib, "windowscodecs.lib")
#endif

void ErrorMsg(const char* msg);

namespace {

constexpr bool kLogBitmap = false;

const unsigned char kGravityBmpSignature[12] = {
    0xC3, 0xD1, 0xBE, 0xCB,
    0xC0, 0xCC, 0xBB, 0xA1,
    0xC5, 0xE4, 0xB4, 0xCF,
};

u32 PackBgr(unsigned char blue, unsigned char green, unsigned char red)
{
    return static_cast<u32>(blue)
        | (static_cast<u32>(green) << 8)
        | (static_cast<u32>(red) << 16)
        | 0xFF000000u;
}

u32 PackBgra(unsigned char blue, unsigned char green, unsigned char red, unsigned char alpha)
{
    return static_cast<u32>(blue)
        | (static_cast<u32>(green) << 8)
        | (static_cast<u32>(red) << 16)
        | (static_cast<u32>(alpha) << 24);
}

bool HasRange(int size, int offset, int length)
{
    return offset >= 0 && length >= 0 && offset <= size && length <= size - offset;
}

bool ShouldLogGroundBitmap(const char* fName)
{
    if (!fName || !*fName) {
        return false;
    }

    std::string lowerName(fName);
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    return lowerName.find("pron-dun") != std::string::npos
        || lowerName.find("backside.bmp") != std::string::npos;
}

void LogBitmapSamples(const char* fName, const char* decodePath, const CBitmapRes& bitmapRes)
{
    if constexpr (!kLogBitmap) {
        return;
    }

    if (!bitmapRes.m_data || bitmapRes.m_width <= 0 || bitmapRes.m_height <= 0) {
        DbgLog("[Bitmap] terrain name='%s' decode=%s invalid-data\n",
            fName ? fName : "(null)",
            decodePath ? decodePath : "(unknown)");
        return;
    }

    const int maxX = bitmapRes.m_width - 1;
    const int maxY = bitmapRes.m_height - 1;
    const int centerX = bitmapRes.m_width / 2;
    const int centerY = bitmapRes.m_height / 2;

    const u32 tl = bitmapRes.m_data[0];
    const u32 tr = bitmapRes.m_data[static_cast<size_t>(maxX)];
    const u32 bl = bitmapRes.m_data[static_cast<size_t>(maxY) * static_cast<size_t>(bitmapRes.m_width)];
    const u32 br = bitmapRes.m_data[static_cast<size_t>(maxY) * static_cast<size_t>(bitmapRes.m_width) + static_cast<size_t>(maxX)];
    const u32 center = bitmapRes.m_data[static_cast<size_t>(centerY) * static_cast<size_t>(bitmapRes.m_width) + static_cast<size_t>(centerX)];

    DbgLog("[Bitmap] terrain name='%s' decode=%s size=%dx%d samples=(tl:%08X tr:%08X bl:%08X br:%08X c:%08X)\n",
        fName ? fName : "(null)",
        decodePath ? decodePath : "(unknown)",
        bitmapRes.m_width,
        bitmapRes.m_height,
        tl,
        tr,
        bl,
        br,
        center);
}

bool LoadGravityBmp(CBitmapRes& bitmapRes, const unsigned char* bitmap, int size)
{
    if (!bitmap || size < 17) {
        return false;
    }

    if (!HasRange(size, 0, 16)) {
        return false;
    }

    const u32 magicVal = *reinterpret_cast<const u32*>(bitmap + 12);
    const int width = static_cast<int>(((magicVal >> 1) - 26383104u) >> 16);
    const int height = static_cast<int>(static_cast<unsigned short>((magicVal >> 1) + 27904u)) - 1;
    if (width <= 0 || height <= 0) {
        return false;
    }

    const unsigned char format = bitmap[16];
    const int bitsPerPixel = format == 0 ? 24 : (format == 1 ? 8 : 0);
    if (bitsPerPixel == 0) {
        ErrorMsg("Only 16m or 8bit color supported");
        return false;
    }

    bitmapRes.m_width = width;
    bitmapRes.m_height = height;
    bitmapRes.m_data = new u32[static_cast<size_t>(width) * static_cast<size_t>(height)];

    const unsigned char* bitmapIter = bitmap + 17;
    int bitmapOffset = 17;

    if (bitsPerPixel == 8) {
        if (!HasRange(size, bitmapOffset, 4)) {
            return false;
        }

        const u32 paletteCountRaw = *reinterpret_cast<const u32*>(bitmapIter);
        const u32 paletteCount = paletteCountRaw ? paletteCountRaw : 256u;
        if (paletteCount == 0 || paletteCount > 256u) {
            return false;
        }

        bitmapIter += 4;
        bitmapOffset += 4;

        const int pairBytes = static_cast<int>(paletteCount * 2u);
        if (!HasRange(size, bitmapOffset, pairBytes * 2)) {
            return false;
        }

        std::vector<unsigned char> gbColor(static_cast<size_t>(pairBytes));
        std::vector<unsigned char> rColor(static_cast<size_t>(pairBytes));
        std::memcpy(gbColor.data(), bitmapIter, static_cast<size_t>(pairBytes));
        bitmapIter += pairBytes;
        bitmapOffset += pairBytes;
        std::memcpy(rColor.data(), bitmapIter, static_cast<size_t>(pairBytes));
        bitmapIter += pairBytes;
        bitmapOffset += pairBytes;

        u32 palette[256]{};
        for (u32 i = 0; i < paletteCount; ++i) {
            const size_t paletteIndex = static_cast<size_t>(i) * 2u;
            const unsigned char blue = gbColor[paletteIndex + 1u];
            const unsigned char green = gbColor[paletteIndex + 0u];
            const unsigned char red = rColor[paletteIndex + 1u];
            palette[i] = PackBgr(blue, green, red);
        }

        const int pitch = (width + 3) & ~3;
        std::vector<unsigned char> row(static_cast<size_t>(pitch));
        for (int y = height - 1; y >= 0; --y) {
            if (!HasRange(size, bitmapOffset, pitch)) {
                return false;
            }

            std::memcpy(row.data(), bitmapIter, static_cast<size_t>(pitch));
            bitmapIter += pitch;
            bitmapOffset += pitch;

            u32* dstRow = bitmapRes.m_data + static_cast<size_t>(y) * static_cast<size_t>(width);
            for (int x = 0; x < width; ++x) {
                dstRow[x] = palette[row[static_cast<size_t>(x)]];
            }
        }
    } else {
        const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
        const int gbPlaneBytes = width * 2;
        const int bPlaneBytes = width;
        const int totalGbBytes = static_cast<int>(pixelCount * 2u);
        const int totalBBytes = static_cast<int>(pixelCount);
        if (!HasRange(size, bitmapOffset, totalGbBytes) || !HasRange(size, bitmapOffset + totalGbBytes, totalBBytes)) {
            return false;
        }

        std::vector<unsigned char> gbPlane(pixelCount * 2u);
        std::vector<unsigned char> bPlane(pixelCount);

        for (int y = 0; y < height; ++y) {
            std::memcpy(gbPlane.data() + static_cast<size_t>(y) * static_cast<size_t>(gbPlaneBytes),
                bitmapIter,
                static_cast<size_t>(gbPlaneBytes));
            bitmapIter += gbPlaneBytes;
            bitmapOffset += gbPlaneBytes;
        }

        for (int y = 0; y < height; ++y) {
            std::memcpy(bPlane.data() + static_cast<size_t>(y) * static_cast<size_t>(bPlaneBytes),
                bitmapIter,
                static_cast<size_t>(bPlaneBytes));
            bitmapIter += bPlaneBytes;
            bitmapOffset += bPlaneBytes;
        }

        for (int y = 0; y < height; ++y) {
            u32* dstRow = bitmapRes.m_data + static_cast<size_t>(y) * static_cast<size_t>(width);
            const unsigned char* gbRow = gbPlane.data() + static_cast<size_t>(y) * static_cast<size_t>(gbPlaneBytes);
            const unsigned char* bRow = bPlane.data() + static_cast<size_t>(y) * static_cast<size_t>(bPlaneBytes);
            for (int x = 0; x < width; ++x) {
                const size_t gbIndex = static_cast<size_t>(x) * 2u;
                dstRow[x] = PackBgr(bRow[x], gbRow[gbIndex], gbRow[gbIndex + 1u]);
            }
        }
    }

    bitmapRes.m_isAlpha = 0;
    return true;
}

bool LoadStandardBmp(CBitmapRes& bitmapRes, const unsigned char* bitmap, int size)
{
    if (!bitmap || size < 54 || !HasRange(size, 0, 54)) {
        return false;
    }

    const unsigned short fileHdr = *reinterpret_cast<const unsigned short*>(bitmap);
    if (fileHdr != 19778u) {
        return false;
    }

    const int width = *reinterpret_cast<const int*>(bitmap + 18);
    const int rawHeight = *reinterpret_cast<const int*>(bitmap + 22);
    const unsigned short bitsPerPixel = *reinterpret_cast<const unsigned short*>(bitmap + 28);
    const u32 pixelOffset = *reinterpret_cast<const u32*>(bitmap + 10);
    if (width <= 0 || rawHeight == 0 || (bitsPerPixel != 8 && bitsPerPixel != 24)) {
        ErrorMsg("Only 16m or 8bit color supported");
        return false;
    }

    const bool topDown = rawHeight < 0;
    const int height = rawHeight < 0 ? -rawHeight : rawHeight;
    if (height <= 0) {
        return false;
    }

    bitmapRes.m_width = width;
    bitmapRes.m_height = height;
    bitmapRes.m_data = new u32[static_cast<size_t>(width) * static_cast<size_t>(height)];

    if (bitsPerPixel == 8) {
        int paletteCount = static_cast<int>(*reinterpret_cast<const u32*>(bitmap + 46));
        if (paletteCount <= 0) {
            paletteCount = 256;
        }
        if (paletteCount > 256) {
            return false;
        }

        const int paletteBytes = paletteCount * 4;
        if (!HasRange(size, 54, paletteBytes) || !HasRange(size, static_cast<int>(pixelOffset), 0)) {
            return false;
        }

        u32 palette[256]{};
        const unsigned char* paletteData = bitmap + 54;
        for (int i = 0; i < paletteCount; ++i) {
            const unsigned char* entry = paletteData + static_cast<size_t>(i) * 4u;
            palette[i] = PackBgr(entry[0], entry[1], entry[2]);
        }

        const int pitch = (width + 3) & ~3;
        if (!HasRange(size, static_cast<int>(pixelOffset), pitch * height)) {
            return false;
        }

        const unsigned char* pixels = bitmap + pixelOffset;
        for (int row = 0; row < height; ++row) {
            const int srcY = topDown ? row : (height - 1 - row);
            const unsigned char* srcRow = pixels + static_cast<size_t>(srcY) * static_cast<size_t>(pitch);
            u32* dstRow = bitmapRes.m_data + static_cast<size_t>(row) * static_cast<size_t>(width);
            for (int x = 0; x < width; ++x) {
                dstRow[x] = palette[srcRow[x]];
            }
        }
    } else {
        const int pitch = ((width * 3) + 3) & ~3;
        if (!HasRange(size, static_cast<int>(pixelOffset), pitch * height)) {
            return false;
        }

        const unsigned char* pixels = bitmap + pixelOffset;
        for (int row = 0; row < height; ++row) {
            const int srcY = topDown ? row : (height - 1 - row);
            const unsigned char* srcRow = pixels + static_cast<size_t>(srcY) * static_cast<size_t>(pitch);
            u32* dstRow = bitmapRes.m_data + static_cast<size_t>(row) * static_cast<size_t>(width);
            for (int x = 0; x < width; ++x) {
                const unsigned char* src = srcRow + static_cast<size_t>(x) * 3u;
                dstRow[x] = PackBgr(src[0], src[1], src[2]);
            }
        }
    }

    bitmapRes.m_isAlpha = 0;
    return true;
}

bool EnsureWicAvailable()
{
#if RO_PLATFORM_WINDOWS
    static bool s_initialized = false;
    static bool s_available = false;
    if (!s_initialized) {
        const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        s_available = SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;
        s_initialized = true;
    }
    return s_available;
#else
    return false;
#endif
}

#if RO_PLATFORM_WINDOWS
IWICImagingFactory* GetWicFactory()
{
    static IWICImagingFactory* s_factory = nullptr;
    static bool s_attempted = false;
    if (!s_attempted) {
        s_attempted = true;
        if (EnsureWicAvailable()) {
            CoCreateInstance(
                CLSID_WICImagingFactory,
                nullptr,
                CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(&s_factory));
        }
    }
    return s_factory;
}

bool DecodeImageWithWic(const unsigned char* buffer, int size, int& outW, int& outH, u32*& outData)
{
    IWICImagingFactory* factory = GetWicFactory();
    if (!factory || !buffer || size <= 0) {
        return false;
    }

    IWICStream* stream = nullptr;
    HRESULT hr = factory->CreateStream(&stream);
    if (FAILED(hr) || !stream) {
        return false;
    }

    hr = stream->InitializeFromMemory(const_cast<BYTE*>(reinterpret_cast<const BYTE*>(buffer)), static_cast<DWORD>(size));
    if (FAILED(hr)) {
        stream->Release();
        return false;
    }

    IWICBitmapDecoder* decoder = nullptr;
    hr = factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder);
    stream->Release();
    if (FAILED(hr) || !decoder) {
        return false;
    }

    IWICBitmapFrameDecode* frame = nullptr;
    hr = decoder->GetFrame(0, &frame);
    decoder->Release();
    if (FAILED(hr) || !frame) {
        return false;
    }

    UINT width = 0;
    UINT height = 0;
    hr = frame->GetSize(&width, &height);
    if (FAILED(hr) || width == 0 || height == 0) {
        frame->Release();
        return false;
    }

    IWICFormatConverter* converter = nullptr;
    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr) || !converter) {
        frame->Release();
        return false;
    }

    hr = converter->Initialize(
        frame,
        GUID_WICPixelFormat32bppBGRA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0f,
        WICBitmapPaletteTypeCustom);
    frame->Release();
    if (FAILED(hr)) {
        converter->Release();
        return false;
    }

    std::unique_ptr<u32[]> pixels(new u32[static_cast<size_t>(width) * static_cast<size_t>(height)]);
    const UINT stride = width * static_cast<UINT>(sizeof(u32));
    const UINT bufferSize = stride * height;
    hr = converter->CopyPixels(nullptr, stride, bufferSize, reinterpret_cast<BYTE*>(pixels.get()));
    converter->Release();
    if (FAILED(hr)) {
        return false;
    }

    outW = static_cast<int>(width);
    outH = static_cast<int>(height);
    outData = pixels.release();
    return true;
}
#else
bool DecodeImageWithWic(const unsigned char*, int, int& outW, int& outH, u32*& outData)
{
    outW = 0;
    outH = 0;
    outData = nullptr;
    return false;
}
#endif

bool LoadImageFromMemory(const unsigned char* buffer, int size, int& outW, int& outH, u32*& outData)
{
#if !RO_PLATFORM_WINDOWS && RO_ENABLE_QT6_UI
    outW = 0;
    outH = 0;
    outData = nullptr;

    if (!buffer || size <= 0) {
        return false;
    }

    const QByteArray encoded(reinterpret_cast<const char*>(buffer), size);
    QImage image = QImage::fromData(encoded);
    if (image.isNull()) {
        return false;
    }

    image = image.convertToFormat(QImage::Format_ARGB32);
    if (image.isNull() || image.width() <= 0 || image.height() <= 0) {
        return false;
    }

    const size_t pixelCount = static_cast<size_t>(image.width()) * static_cast<size_t>(image.height());
    std::unique_ptr<u32[]> pixels(new u32[pixelCount]);
    std::memcpy(pixels.get(), image.constBits(), pixelCount * sizeof(u32));

    outW = image.width();
    outH = image.height();
    outData = pixels.release();
    return true;
#else
    return DecodeImageWithWic(buffer, size, outW, outH, outData);
#endif
}

std::string ResolveAlphaBitmapPath(const char* fName)
{
    if (!fName || !*fName) {
        return std::string();
    }

    std::string path(fName);
    std::replace(path.begin(), path.end(), '/', '\\');
    const size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) {
        return std::string();
    }

    std::string stem = path.substr(0, dot);
    if (stem.size() >= 2 && _stricmp(stem.c_str() + stem.size() - 2, "_a") == 0) {
        return std::string();
    }

    const std::string alphaPath = stem + "_a.bmp";
    return g_fileMgr.IsDataExist(alphaPath.c_str()) ? alphaPath : std::string();
}

bool ReadTgaPixel(const unsigned char* src, int bytesPerPixel, u32* outColor)
{
    if (!src || !outColor) {
        return false;
    }

    switch (bytesPerPixel) {
    case 1:
        *outColor = PackBgra(src[0], src[0], src[0], 0xFF);
        return true;
    case 3:
        *outColor = PackBgra(src[0], src[1], src[2], 0xFF);
        return true;
    case 4:
        *outColor = PackBgra(src[0], src[1], src[2], src[3]);
        return true;
    default:
        return false;
    }
}

bool LoadTgaPixels(const unsigned char* src, int size, int offset, int imageType, int bytesPerPixel, std::vector<u32>* outPixels)
{
    if (!src || !outPixels || bytesPerPixel <= 0) {
        return false;
    }

    const size_t pixelCount = outPixels->size();
    if (imageType == 2 || imageType == 3) {
        const size_t required = pixelCount * static_cast<size_t>(bytesPerPixel);
        if (!HasRange(size, offset, static_cast<int>(required))) {
            return false;
        }

        const unsigned char* pixelSrc = src + offset;
        for (size_t i = 0; i < pixelCount; ++i) {
            if (!ReadTgaPixel(pixelSrc + i * static_cast<size_t>(bytesPerPixel), bytesPerPixel, &(*outPixels)[i])) {
                return false;
            }
        }
        return true;
    }

    if (imageType == 10 || imageType == 11) {
        size_t written = 0;
        int cursor = offset;
        while (written < pixelCount) {
            if (!HasRange(size, cursor, 1)) {
                return false;
            }

            const unsigned char header = src[cursor++];
            const size_t runLength = static_cast<size_t>((header & 0x7Fu) + 1u);
            if (runLength > pixelCount - written) {
                return false;
            }

            if (header & 0x80u) {
                if (!HasRange(size, cursor, bytesPerPixel)) {
                    return false;
                }

                u32 color = 0;
                if (!ReadTgaPixel(src + cursor, bytesPerPixel, &color)) {
                    return false;
                }
                cursor += bytesPerPixel;
                for (size_t i = 0; i < runLength; ++i) {
                    (*outPixels)[written++] = color;
                }
            } else {
                const size_t rawBytes = runLength * static_cast<size_t>(bytesPerPixel);
                if (!HasRange(size, cursor, static_cast<int>(rawBytes))) {
                    return false;
                }

                for (size_t i = 0; i < runLength; ++i) {
                    if (!ReadTgaPixel(src + cursor + i * static_cast<size_t>(bytesPerPixel), bytesPerPixel, &(*outPixels)[written++])) {
                        return false;
                    }
                }
                cursor += static_cast<int>(rawBytes);
            }
        }
        return true;
    }

    return false;
}

} // namespace

bool LoadBgraPixelsFromGameData(const char* path, u32** outPixels, int* outWidth, int* outHeight)
{
    if (!outPixels || !path || !*path) {
        return false;
    }

    *outPixels = nullptr;
    if (outWidth) {
        *outWidth = 0;
    }
    if (outHeight) {
        *outHeight = 0;
    }

    int size = 0;
    unsigned char* bytes = g_fileMgr.GetData(path, &size);
    if (!bytes || size <= 0) {
        delete[] bytes;
        return false;
    }

    int width = 0;
    int height = 0;
    u32* pixels = nullptr;
    const bool decoded = LoadImageFromMemory(bytes, size, width, height, pixels);
    delete[] bytes;
    if (!decoded || !pixels || width <= 0 || height <= 0) {
        delete[] pixels;
        return false;
    }

    *outPixels = pixels;
    if (outWidth) {
        *outWidth = width;
    }
    if (outHeight) {
        *outHeight = height;
    }
    return true;
}

CBitmapRes::CBitmapRes()
    : m_isAlpha(0), m_width(0), m_height(0), m_data(nullptr)
{
}

CBitmapRes::~CBitmapRes() {
    Reset();
}

bool CBitmapRes::Load(const char* fName) {
    return CRes::Load(fName);
}

bool CBitmapRes::LoadFromBuffer(const char* fName, const unsigned char* buffer, int size) {
    const char* ext = fName ? std::strrchr(fName, '.') : nullptr;
    if (!ext) {
        return false;
    }

    if (_stricmp(ext, ".bmp") == 0) {
        const bool isGravity = HasRange(size, 0, 12)
            && std::memcmp(buffer, kGravityBmpSignature, sizeof(kGravityBmpSignature)) == 0;
        const bool loaded = LoadBMPData(buffer, size);
        if (loaded) {
            const std::string alphaPath = ResolveAlphaBitmapPath(fName);
            if (!alphaPath.empty() && SetAlphaWithBMP(alphaPath.c_str())) {
                m_isAlpha = 1;
            }
        }
        if (loaded && ShouldLogGroundBitmap(fName)) {
            LogBitmapSamples(fName, isGravity ? "gravity-bmp" : "standard-bmp", *this);
        }
        return loaded;
    }
    if (_stricmp(ext, ".tga") == 0) {
        return LoadTGAData(buffer, size);
    }
    if (_stricmp(ext, ".jpg") == 0 || _stricmp(ext, ".jpeg") == 0) {
        return LoadJPGData(buffer, size);
    }

    ErrorMsg("Unsupported image format");
    return false;
}

CRes* CBitmapRes::Clone() {
    return new CBitmapRes();
}

void CBitmapRes::Reset() {
    m_width = 0;
    m_height = 0;
    m_isAlpha = 0;
    if (m_data) {
        delete[] m_data;
        m_data = nullptr;
    }
}

bool CBitmapRes::ChangeToGrayScaleBMP(int blackLimit, int whiteLimit) {
    if (!m_data) {
        return false;
    }

    for (int y = 0; y < m_height; ++y) {
        for (int x = 0; x < m_width; ++x) {
            u32& pixel = m_data[static_cast<size_t>(y) * static_cast<size_t>(m_width) + static_cast<size_t>(x)];
            const unsigned char blue = static_cast<unsigned char>(pixel & 0xFFu);
            const unsigned char green = static_cast<unsigned char>((pixel >> 8) & 0xFFu);
            const unsigned char red = static_cast<unsigned char>((pixel >> 16) & 0xFFu);
            const unsigned char alpha = static_cast<unsigned char>((pixel >> 24) & 0xFFu);

            unsigned char gray = static_cast<unsigned char>((static_cast<int>(red) + static_cast<int>(green) + static_cast<int>(blue)) / 3);
            if (pixel == 0xFF000000u && green == 0) {
                gray = blue;
            } else {
                gray = static_cast<unsigned char>((std::max)(blackLimit, static_cast<int>(gray)));
                if (gray > whiteLimit) {
                    gray = 255;
                }
            }

            pixel = PackBgra(gray, gray, gray, alpha);
        }
    }

    return true;
}

bool CBitmapRes::SetAlphaWithBMP(const char* fName) {
    CBitmapRes alphaBitmap;
    if (!alphaBitmap.Load(fName)) {
        ErrorMsg("Error Reading Alpha Bitmap");
        return false;
    }

    if (!m_data) {
        return false;
    }

    for (int y = 0; y < m_height; ++y) {
        for (int x = 0; x < m_width; ++x) {
            const size_t index = static_cast<size_t>(y) * static_cast<size_t>(m_width) + static_cast<size_t>(x);
            const u32 srcPixel = m_data[index];
            const u32 alphaPixel = (x >= 0 && x < alphaBitmap.m_width && y >= 0 && y < alphaBitmap.m_height)
                ? alphaBitmap.m_data[static_cast<size_t>(y) * static_cast<size_t>(alphaBitmap.m_width) + static_cast<size_t>(x)]
                : 0x00FF0000u;

            const unsigned int mix = ((alphaPixel | 0x00FF0000u) >> 16) + (alphaPixel >> 8);
            const unsigned int avg = static_cast<unsigned int>((1431655766ULL * static_cast<unsigned long long>((alphaPixel & 0xFFu) + mix)) >> 32);
            const unsigned int alpha = ((avg >> 31) + avg) & 0xFFu;
            m_data[index] = (srcPixel & 0x00FFFFFFu) | (alpha << 24);
        }
    }

    return true;
}

unsigned long CBitmapRes::GetColor(int x, int y) {
    if (x < 0) {
        return 0x00FF0000ul;
    }

    if (x >= m_width || y < 0 || y >= m_height) {
        return 0x00FF0000ul;
    }

    return m_data[static_cast<size_t>(y) * static_cast<size_t>(m_width) + static_cast<size_t>(x)];
}

void CBitmapRes::CreateNull(int w, int h) {
    Reset();
    m_width = w;
    m_isAlpha = 0;
    m_height = h;
    m_data = new u32[static_cast<size_t>(w) * static_cast<size_t>(h)];
    std::memset(m_data, 0xFF, static_cast<size_t>(w) * static_cast<size_t>(h) * sizeof(u32));
}

bool CBitmapRes::LoadBMPData(const unsigned char* buffer, int size) {
    Reset();
    if (!buffer || size <= 0) {
        return false;
    }

    if (HasRange(size, 0, 12) && std::memcmp(buffer, kGravityBmpSignature, sizeof(kGravityBmpSignature)) == 0) {
        return LoadGravityBmp(*this, buffer, size);
    }

    return LoadStandardBmp(*this, buffer, size);
}

bool CBitmapRes::LoadJPGData(const unsigned char* buffer, int size) {
    Reset();
    return LoadImageFromMemory(buffer, size, m_width, m_height, m_data);
}

bool CBitmapRes::LoadTGAData(const unsigned char* buffer, int size) {
    Reset();

    if (!buffer || size < 18 || !HasRange(size, 0, 18)) {
        return false;
    }

    const int idLength = buffer[0];
    const int colorMapType = buffer[1];
    const int imageType = buffer[2];
    const int colorMapOrigin = static_cast<int>(buffer[3] | (buffer[4] << 8));
    const int colorMapLength = static_cast<int>(buffer[5] | (buffer[6] << 8));
    const int colorMapDepth = buffer[7];
    (void)colorMapOrigin;
    const int width = static_cast<int>(buffer[12] | (buffer[13] << 8));
    const int height = static_cast<int>(buffer[14] | (buffer[15] << 8));
    const int bitsPerPixel = buffer[16];
    const int imageDescriptor = buffer[17];

    if (width <= 0 || height <= 0) {
        return false;
    }

    if (colorMapType != 0) {
        return false;
    }

    if (imageType != 2 && imageType != 3 && imageType != 10 && imageType != 11) {
        return false;
    }

    if (bitsPerPixel != 8 && bitsPerPixel != 24 && bitsPerPixel != 32) {
        return false;
    }

    const int bytesPerPixel = bitsPerPixel / 8;
    int offset = 18 + idLength;
    if (colorMapLength > 0) {
        offset += (colorMapLength * colorMapDepth + 7) / 8;
    }
    if (!HasRange(size, offset, 0)) {
        return false;
    }

    std::vector<u32> decoded(static_cast<size_t>(width) * static_cast<size_t>(height));
    if (!LoadTgaPixels(buffer, size, offset, imageType, bytesPerPixel, &decoded)) {
        return false;
    }

    m_width = width;
    m_height = height;
    m_data = new u32[static_cast<size_t>(width) * static_cast<size_t>(height)];

    const bool topOrigin = (imageDescriptor & 0x20) != 0;
    const bool rightOrigin = (imageDescriptor & 0x10) != 0;
    bool hasAlpha = false;
    for (int y = 0; y < height; ++y) {
        const int srcY = topOrigin ? y : (height - 1 - y);
        for (int x = 0; x < width; ++x) {
            const int srcX = rightOrigin ? (width - 1 - x) : x;
            const u32 color = decoded[static_cast<size_t>(srcY) * static_cast<size_t>(width) + static_cast<size_t>(srcX)];
            m_data[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)] = color;
            if ((color >> 24) != 0xFFu) {
                hasAlpha = true;
            }
        }
    }

    m_isAlpha = hasAlpha ? 1 : 0;
    return true;
}
