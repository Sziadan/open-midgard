// IGC texture entry + pipeline. MIT attribution: third_party/gr2-reader/LICENSE.
#include "Gr2Igc.h"
#include "Gr2IgcInternal.h"
#include <stdexcept>

namespace ro::gr2 {
namespace {

constexpr int kSmallestDwtRow = 16;
constexpr int kSmallestDwtCol = 16;
constexpr int kIgcMaxPixels = 4096 * 4096;
constexpr int kIgcMaxPixelsPerByte = 4096;

} // namespace

namespace igc {

bool DecodeIgcPipeline(const uint8_t* src, size_t srcSize, int width, int height, bool alpha,
    std::vector<uint8_t>& rgba, std::string& error)
{
    (void)srcSize;
    try {
        const int planeCount = alpha ? 4 : 3;
        const int pixels = width * height;
        std::vector<int16_t> planes[4];
        for (int i = 0; i < 4; ++i) planes[i].assign(static_cast<size_t>(pixels), 0);

        std::vector<uint8_t> rowMask(static_cast<size_t>(height), 0);
        std::vector<int16_t> temp(static_cast<size_t>(pixels), 0);

        int cursor = 4;
        for (int p = 0; p < planeCount; ++p) {
            const int consumed = PlaneDecode(src, cursor, planes[p].data(), 0, width, height,
                (p == 0) ? rowMask.data() : nullptr);
            if (consumed <= 0) {
                error = "IGC planeDecode consumed no bytes";
                return false;
            }
            cursor += consumed;

            IDWT2D(planes[p].data(), width * 8, width >> 3, height >> 3, nullptr, temp.data());
            IDWT2D(planes[p].data(), width * 4, width >> 2, height >> 2, nullptr, temp.data());
            IDWT2D(planes[p].data(), width * 2, width >> 1, height >> 1, nullptr, temp.data());
            IDWT2D(planes[p].data(), width, width, height, (p == 0) ? rowMask.data() : nullptr,
                temp.data());
        }

        if (!alpha) {
            for (int i = 0; i < pixels; ++i) planes[3][static_cast<size_t>(i)] = 255;
        }

        rgba.resize(static_cast<size_t>(pixels) * 4u);
        YuvToRgb(planes[0].data(), planes[1].data(), planes[2].data(), planes[3].data(),
            width, height, rgba.data());
        return true;
    } catch (const std::exception& ex) {
        error = ex.what();
        rgba.clear();
        return false;
    }
}

} // namespace igc

bool DecodeIgcTexture(const uint8_t* src, size_t srcSize, int width, int height, bool alpha,
    std::vector<uint8_t>& rgba, std::string& error)
{
    rgba.clear();
    error.clear();
    if (!src || width <= 0 || height <= 0) {
        error = "decodeIGCTexture: invalid arguments";
        return false;
    }

    const int pixels = width * height;
    if (pixels <= 256) {
        const size_t expected = static_cast<size_t>(pixels) * 4u;
        if (srcSize < expected) {
            error = "decodeIGCTexture: small-image fallback expects RGBA bytes";
            return false;
        }
        rgba.assign(src, src + expected);
        return true;
    }

    if (width < kSmallestDwtRow || height < kSmallestDwtCol || (width & 15) != 0 || (height & 15) != 0) {
        error = "decodeIGCTexture: shouldBink fallback not supported";
        return false;
    }

    if (pixels > kIgcMaxPixels ||
        static_cast<size_t>(pixels) > srcSize * static_cast<size_t>(kIgcMaxPixelsPerByte)) {
        error = "decodeIGCTexture: texture exceeds allocation cap";
        return false;
    }

    return igc::DecodeIgcPipeline(src, srcSize, width, height, alpha, rgba, error);
}

} // namespace ro::gr2
