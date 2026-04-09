#include "Sprite.h"

#include "render3d/Device.h"
#include "DebugLog.h"

#include <algorithm>
#include <cstring>

namespace {

template <typename T>
bool ReadValue(const unsigned char*& cursor, const unsigned char* end, T& out)
{
    if (cursor + sizeof(T) > end) {
        return false;
    }
    std::memcpy(&out, cursor, sizeof(T));
    cursor += sizeof(T);
    return true;
}

bool ReadBytes(const unsigned char*& cursor, const unsigned char* end, void* out, size_t size)
{
    if (cursor + size > end) {
        return false;
    }
    std::memcpy(out, cursor, size);
    cursor += size;
    return true;
}

struct SprHeader {
    unsigned short id;
    unsigned short ver;
    unsigned short count;
};

struct SprChunk {
    unsigned short width;
    unsigned short height;
};

} // namespace

CSprRes::CSprRes() {
    m_count = 0;
    std::memset(m_pal, 0, sizeof(m_pal));
}

CSprRes::~CSprRes() {
    Reset();
}

unsigned char* CSprRes::HalfImage(unsigned char* src, int w, int h, int dw, int dh) {
    if (!src || w <= 0 || h <= 0) {
        return nullptr;
    }

    const int xStep = dw ? 2 : 1;
    const int yStep = dh ? 2 : 1;
    const int outW = std::max(1, w / xStep);
    const int outH = std::max(1, h / yStep);

    unsigned char* out = new unsigned char[outW * outH];
    for (int y = 0; y < outH; ++y) {
        for (int x = 0; x < outW; ++x) {
            out[y * outW + x] = src[(y * yStep) * w + (x * xStep)];
        }
    }
    return out;
}

unsigned char* CSprRes::ZeroCompression(unsigned char* src, int w, int h, unsigned short& outSize) {
    if (!src || w <= 0 || h <= 0) {
        outSize = 0;
        return nullptr;
    }

    std::vector<unsigned char> compressed;
    compressed.reserve(static_cast<size_t>(w) * static_cast<size_t>(h));
    const int count = w * h;
    for (int i = 0; i < count; ++i) {
        const unsigned char value = src[i];
        if (value != 0) {
            compressed.push_back(value);
            continue;
        }

        unsigned char run = 1;
        while (i + run < count && src[i + run] == 0 && run < 255) {
            ++run;
        }
        compressed.push_back(0);
        compressed.push_back(run);
        i += run - 1;
    }

    outSize = static_cast<unsigned short>(compressed.size());
    unsigned char* out = new unsigned char[outSize];
    if (outSize != 0) {
        std::memcpy(out, compressed.data(), outSize);
    }
    return out;
}

unsigned char* CSprRes::ZeroDecompression(unsigned char* src, int w, int h) {
    if (!src || w <= 0 || h <= 0) {
        return nullptr;
    }

    const int pixelCount = w * h;
    unsigned char* out = new unsigned char[pixelCount];
    int outPos = 0;
    while (outPos < pixelCount) {
        const unsigned char value = *src++;
        if (value == 0) {
            const unsigned char run = *src++;
            const int remaining = pixelCount - outPos;
            const int copyCount = std::min<int>(run, remaining);
            std::memset(out + outPos, 0, copyCount);
            outPos += copyCount;
        } else {
            out[outPos++] = value;
        }
    }
    return out;
}

bool CSprRes::LoadFromBuffer(const char* fName, const unsigned char* buffer, int size) {
    DbgLog("[SprLoad] LoadFromBuffer: '%s' size=%d\n", fName ? fName : "(null)", size);
    (void)fName;

    Reset();
    if (!buffer || size < 6) {
        DbgLog("[SprLoad] FAIL: null buffer or size<6\n");
        return false;
    }

    const unsigned char* cursor = buffer;
    const unsigned char* end = buffer + size;

    SprHeader header{};
    if (!ReadValue(cursor, end, header)) {
        DbgLog("[SprLoad] FAIL: could not read header\n");
        return false;
    }
    DbgLog("[SprLoad] id=0x%04X ver=0x%04X count=%d\n", header.id, header.ver, header.count);
    if (header.id != 20563 || header.ver > 0x201u) {
        DbgLog("[SprLoad] FAIL: bad id or ver\n");
        return false;
    }

    unsigned short rgbaCount = 0;
    if (header.ver >= 0x200u && !ReadValue(cursor, end, rgbaCount)) {
        DbgLog("[SprLoad] FAIL: could not read rgbaCount\n");
        return false;
    }
    DbgLog("[SprLoad] rgbaCount=%d\n", (int)rgbaCount);

    const unsigned char* palettePos = nullptr;
    if (header.ver >= 0x101u) {
        DbgLog("[SprLoad] Reading palette (ver>=0x101), size=%d\n", size);
        if (size < 1024) {
            DbgLog("[SprLoad] FAIL: size<1024 for palette\n");
            return false;
        }
        palettePos = end - 1024;
        unsigned char paletteBytes[1024];
        std::memcpy(paletteBytes, palettePos, sizeof(paletteBytes));
        DbgLog("[SprLoad] Calling ConvertPalette m_pal=%p\n", (void*)m_pal);
        g_3dDevice.ConvertPalette(m_pal, reinterpret_cast<const PALETTEENTRY*>(paletteBytes), 256);
        DbgLog("[SprLoad] ConvertPalette done\n");
        end = palettePos;
    }

    m_count = header.count;
    DbgLog("[SprLoad] Reading %d indexed sprites...\n", m_count);
    for (int i = 0; i < m_count; ++i) {
        SprChunk chunk{};
        if (!ReadValue(cursor, end, chunk)) {
            Reset();
            return false;
        }

        SprImg* image = new SprImg();
        image->width = chunk.width;
        image->height = chunk.height;
        image->isHalfW = 0;
        image->isHalfH = 0;
        image->tex = nullptr;

        const size_t pixelCount = static_cast<size_t>(image->width) * static_cast<size_t>(image->height);
        if (header.ver < 0x201u) {
            image->indices.resize(pixelCount);
            if (!ReadBytes(cursor, end, image->indices.data(), pixelCount)) {
                delete image;
                Reset();
                return false;
            }
        } else {
            unsigned short compressedSize = 0;
            if (!ReadValue(cursor, end, compressedSize) || cursor + compressedSize > end) {
                delete image;
                Reset();
                return false;
            }
            unsigned char* decoded = ZeroDecompression(const_cast<unsigned char*>(cursor), image->width, image->height);
            cursor += compressedSize;
            if (!decoded) {
                delete image;
                Reset();
                return false;
            }
            image->indices.assign(decoded, decoded + pixelCount);
            delete[] decoded;
        }

        m_sprites[0].push_back(image);
    }

    for (unsigned short i = 0; i < rgbaCount; ++i) {
        SprChunk chunk{};
        if (!ReadValue(cursor, end, chunk)) {
            Reset();
            return false;
        }

        SprImg* image = new SprImg();
        image->width = chunk.width;
        image->height = chunk.height;
        image->isHalfW = 0;
        image->isHalfH = 0;
        image->tex = nullptr;

        const size_t pixelCount = static_cast<size_t>(image->width) * static_cast<size_t>(image->height);
        image->rgba.resize(pixelCount);
        for (int y = image->height - 1; y >= 0; --y) {
            for (int x = 0; x < image->width; ++x) {
                unsigned char rgba[4];
                if (!ReadBytes(cursor, end, rgba, sizeof(rgba))) {
                    delete image;
                    Reset();
                    return false;
                }
                image->rgba[static_cast<size_t>(y) * image->width + x] =
                    (static_cast<unsigned int>(rgba[0]) << 24) |
                    (static_cast<unsigned int>(rgba[3]) << 16) |
                    (static_cast<unsigned int>(rgba[2]) << 8) |
                    static_cast<unsigned int>(rgba[1]);
            }
        }

        m_sprites[1].push_back(image);
    }

    DbgLog("[SprLoad] LoadFromBuffer SUCCESS: %d indexed, %d rgba sprites\n",
           (int)m_sprites[0].size(), (int)m_sprites[1].size());
    return true;
}

bool CSprRes::Load(const char* fName) {
    return CRes::Load(fName);
}

CRes* CSprRes::Clone() {
    return new CSprRes();
}

void CSprRes::Reset() {
    for (int bank = 0; bank < 2; ++bank) {
        for (SprImg* image : m_sprites[bank]) {
            if (image) {
                if (image->tex) {
                    delete image->tex;
                    image->tex = nullptr;
                }
                delete image;
            }
        }
        m_sprites[bank].clear();
    }
    m_count = 0;
    std::memset(m_pal, 0, sizeof(m_pal));
}

const SprImg* CSprRes::GetSprite(int clipType, int index) const {
    if (clipType < 0 || clipType > 1 || index < 0) {
        return nullptr;
    }
    const std::vector<SprImg*>& bank = m_sprites[clipType];
    if (index >= static_cast<int>(bank.size())) {
        return nullptr;
    }
    return bank[index];
}
