// Plane decoder. MIT attribution: third_party/gr2-reader/LICENSE.
#include "Gr2IgcInternal.h"
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace ro::gr2 {
namespace igc {
namespace {

struct VarBits {
    const uint8_t* buf = nullptr;
    int cur = 0;
    uint32_t bits = 0;
    int bitlen = 0;
};

static uint32_t VarBitsReadU32LE(const uint8_t* buf, int offset)
{
    return uint32_t(buf[offset] | (buf[offset + 1] << 8) | (buf[offset + 2] << 16) | (buf[offset + 3] << 24));
}

static VarBits VarBitsOpen(const uint8_t* buf, int offset)
{
    return VarBits{buf, offset, 0, 0};
}

static uint32_t VarBitsGet1(VarBits& vb)
{
    int bitlen = vb.bitlen;
    if (bitlen == 0) {
        const uint32_t i = VarBitsReadU32LE(vb.buf, vb.cur);
        vb.cur += 4;
        vb.bits = i >> 1;
        vb.bitlen = 31;
        return i & 1u;
    }
    const uint32_t bits = vb.bits;
    vb.bits = bits >> 1;
    vb.bitlen = bitlen - 1;
    return bits & 1u;
}

static uint32_t VarBitsGet(VarBits& vb, int len)
{
    const uint32_t mask = (len == 32) ? 0xffffffffu : uint32_t((1u << len) - 1u);
    if (vb.bitlen < len) {
        const uint32_t nb = VarBitsReadU32LE(vb.buf, vb.cur);
        vb.cur += 4;
        const uint32_t merged = uint32_t((vb.bits | (nb << vb.bitlen)) & mask);
        vb.bits = uint32_t(nb >> (len - vb.bitlen));
        vb.bitlen = vb.bitlen + 32 - len;
        return merged;
    }
    const uint32_t result = vb.bits & mask;
    vb.bits = uint32_t(vb.bits >> len);
    vb.bitlen -= len;
    return result;
}

static int32_t S16(uint32_t v)
{
    return (v & 0x8000u) ? int32_t(v | 0xffff0000u) : int32_t(v);
}

constexpr int MIN_ZERO_LENGTH = 3;
constexpr int LIT_LENGTH_BITS = 6;
constexpr int ZERO_LENGTH_BITS = 8;
constexpr int LIT_LENGTH_LIMIT = (1 << LIT_LENGTH_BITS) - 1;
constexpr int ZERO_LENGTH_LIMIT = (1 << ZERO_LENGTH_BITS) - 1;
constexpr int EXTRA_LENGTHS = 4;
static const int EXTRA_LIT_LENGTHS[4] = {128, 256, 512, 1024};
static const int EXTRA_ZERO_LENGTHS[4] = {512, 1024, 2048, 3072};

static int GetBitLevel(uint32_t value)
{
    uint32_t n = value;
    if (n == 0) return 0;
    if (n >= 16384) return 15;
    int r = 0;
    while (n > 0) {
        ++r;
        n >>= 1;
    }
    return r;
}

static void FillRect(int16_t* outp, int outOffset, int pitch, int width, int height, int16_t val)
{
    const int yadj = pitch - width;
    int o = outOffset;
    for (int h = 0; h < height; ++h) {
        for (int w = 0; w < width; ++w) outp[o++] = val;
        o += yadj;
    }
}

static void ReadEscapes(ArithBits& ab, uint8_t* mask, int count)
{
    const uint32_t zeros = ArithBitsGetValue(ab, uint32_t(count + 1));
    for (int i = 0; i < count; ++i) {
        if (ArithBitsGet(ab, uint32_t(count)) >= zeros) {
            mask[i] = 1;
            ArithBitsRemove(ab, zeros, uint32_t(count) - zeros, uint32_t(count));
        } else {
            mask[i] = 0;
            ArithBitsRemove(ab, 0, zeros, uint32_t(count));
        }
    }
}

static int32_t DecompressDelta(ArithModel& a, ArithBits& ab, VarBits& vb, uint32_t num)
{
    int cur = 0;
    if (ArithDecompress(a, ab, cur)) {
        const int escaped = int(ArithBitsGetValue(ab, num));
        ArithSetDecompressed(a, cur, escaped);
        cur = escaped;
    }
    if (cur) {
        const int32_t v = -int32_t(VarBitsGet1(vb));
        cur = (cur ^ v) - v;
    }
    return cur;
}

static void DecodeLow(ArithBits& ab, VarBits& vb, int16_t* outp, int outOffset, int pixelPitch,
    int encWidth, int encHeight)
{
    if (VarBitsGet1(vb)) {
        const uint32_t v = VarBitsGet(vb, 16);
        FillRect(outp, outOffset, pixelPitch, encWidth, encHeight, int16_t(S16(v)));
        return;
    }

    const uint32_t max = VarBitsGet(vb, 16);
    const uint32_t num = max + 1;
    ArithModel a = ArithOpen(int(max), int(num));
    const int yadj = pixelPitch - encWidth;

    int32_t prev = S16(VarBitsGet(vb, 16));
    outp[outOffset++] = int16_t(prev);

    for (int w = 0; w < encWidth - 1; ++w) {
        int32_t cur = DecompressDelta(a, ab, vb, num);
        prev = cur + prev;
        outp[outOffset++] = int16_t(prev);
    }

    for (int h = 0; h < encHeight - 1; ++h) {
        outOffset += yadj;
        int from = outOffset - pixelPitch;

        int32_t cur = DecompressDelta(a, ab, vb, num);
        prev = cur + outp[from];
        outp[outOffset++] = int16_t(prev);
        ++from;

        for (int w = 0; w < encWidth - 1; ++w) {
            cur = DecompressDelta(a, ab, vb, num);
            prev = cur + ((prev + outp[from]) / 2);
            outp[outOffset++] = int16_t(prev);
            ++from;
        }
    }
}

struct DecompContexts {
    std::vector<ArithModel> contexts;
    ArithModel lits;
    ArithModel zeros;
};

static DecompContexts CreateDecompContexts(uint32_t max, uint32_t num, int numl)
{
    DecompContexts d;
    d.contexts.resize(size_t(numl));
    for (int i = 0; i < numl; ++i) d.contexts[size_t(i)] = ArithOpen(int(max), int(num));
    d.lits = ArithOpen(LIT_LENGTH_LIMIT, LIT_LENGTH_LIMIT + 1);
    d.zeros = ArithOpen(ZERO_LENGTH_LIMIT, ZERO_LENGTH_LIMIT + 1);
    return d;
}

static int RadAbs(int32_t v) { return v < 0 ? -v : v; }

static int32_t Imul(int32_t a, int32_t b)
{
    return int32_t(uint32_t(a) * uint32_t(b)); // wrap like Math.imul
}

static int DecodeLength(ArithModel& model, ArithBits& ab, VarBits& vb, int bitWidth, bool isLit)
{
    int ret = 0;
    if (ArithDecompress(model, ab, ret)) {
        const int escaped = int(VarBitsGet(vb, bitWidth));
        ArithSetDecompressed(model, ret, escaped);
        ret = escaped;
    }
    if (isLit) {
        if (ret >= (LIT_LENGTH_LIMIT - EXTRA_LENGTHS + 1))
            ret = EXTRA_LIT_LENGTHS[ret - (LIT_LENGTH_LIMIT - EXTRA_LENGTHS + 1)];
    } else {
        if (ret >= (ZERO_LENGTH_LIMIT - EXTRA_LENGTHS + 1))
            ret = EXTRA_ZERO_LENGTHS[ret - (ZERO_LENGTH_LIMIT - EXTRA_LENGTHS + 1)] + MIN_ZERO_LENGTH - 1;
        else if (ret)
            ret += MIN_ZERO_LENGTH - 1;
    }
    return ret;
}

static void DecodeHigh1Body(ArithBits& ab, VarBits& vb, int16_t* outp, int& outOffset, int pixelPitch,
    int encWidth, int encHeight, DecompContexts& ctx, uint32_t num, int /*numl*/, int qlevel,
    int yadj, int& h, int32_t& above, int32_t& aboveLeft, int32_t& prev, int& fromOffset, int& w)
{
    int litLen = 0;
    int zeroLen = 0;
    int idleIter = 0;

    for (;;) {
        litLen = DecodeLength(ctx.lits, ab, vb, LIT_LENGTH_BITS, true);
        zeroLen = DecodeLength(ctx.zeros, ab, vb, ZERO_LENGTH_BITS, false);

        if (litLen == 0 && zeroLen == 0) {
            if (++idleIter > 64)
                throw std::runtime_error("decodeHigh1: stuck with litLen=0/zeroLen=0");
        } else {
            idleIter = 0;
        }

        while (litLen > 0) {
            if (w <= 1) {
                if (w) {
                    const uint32_t sum3 = uint32_t(RadAbs(Imul(prev, 2)) + RadAbs(aboveLeft) + RadAbs(above));
                    int context = GetBitLevel(sum3 / 4);
                    int cur = 0;
                    if (ArithDecompress(ctx.contexts[size_t(context)], ab, cur)) {
                        const int escaped = int(ArithBitsGetValue(ab, num));
                        ArithSetDecompressed(ctx.contexts[size_t(context)], cur, escaped);
                        cur = escaped;
                    }
                    if (cur) {
                        const int32_t v = -int32_t(VarBitsGet1(vb));
                        cur = (cur ^ v) - v;
                        cur = cur * qlevel;
                    }
                    outp[outOffset++] = int16_t(cur);
                    --litLen;
                }
                if (--h == 0) return;
                w = encWidth;
                outOffset += yadj;
                fromOffset = outOffset - pixelPitch;
                above = outp[fromOffset++];
                aboveLeft = above;
                prev = above;
            } else {
                const int32_t aboveRight = outp[fromOffset];
                const uint32_t sum4 = uint32_t(RadAbs(prev) + RadAbs(aboveLeft) + RadAbs(above) + RadAbs(aboveRight));
                int context = GetBitLevel(sum4 / 4);
                int cur = 0;
                if (ArithDecompress(ctx.contexts[size_t(context)], ab, cur)) {
                    const int escaped = int(ArithBitsGetValue(ab, num));
                    ArithSetDecompressed(ctx.contexts[size_t(context)], cur, escaped);
                    cur = escaped;
                }
                if (cur) {
                    const int32_t v = -int32_t(VarBitsGet1(vb));
                    cur = (cur ^ v) - v;
                    cur = cur * qlevel;
                }
                outp[outOffset] = int16_t(cur);
                aboveLeft = above;
                above = aboveRight;
                prev = cur;
                ++outOffset;
                ++fromOffset;
                --w;
                --litLen;
            }
        }

        while (zeroLen > 0) {
            if (zeroLen >= w) {
                zeroLen -= w;
                fromOffset += w;
                while (w-- > 0) outp[outOffset++] = 0;
                if (--h == 0) return;
                w = encWidth;
                outOffset += yadj;
                fromOffset = outOffset - pixelPitch;
                above = outp[fromOffset++];
                aboveLeft = above;
                prev = above;
            } else {
                w -= zeroLen;
                fromOffset += zeroLen;
                do {
                    outp[outOffset++] = 0;
                } while (--zeroLen > 0);
                prev = 0;
                above = outp[fromOffset - 1];
                aboveLeft = outp[fromOffset - 2];
            }
        }
    }
}

static void DecodeHigh1(ArithBits& ab, VarBits& vb, int16_t* outp, int outOffset, int pixelPitch,
    int encWidth, int encHeight)
{
    const int qlevel = int(VarBitsGet(vb, 16));
    if (VarBitsGet1(vb)) {
        const int32_t v = S16(VarBitsGet(vb, 16));
        FillRect(outp, outOffset, pixelPitch, encWidth, encHeight, int16_t(v * qlevel));
        return;
    }

    const uint32_t max = VarBitsGet(vb, 16);
    const uint32_t num = max + 1;
    int numl = int(max * uint32_t(qlevel));
    numl = GetBitLevel(uint32_t(numl)) + 1;

    DecompContexts ctx = CreateDecompContexts(max, num, numl);
    const int yadj = pixelPitch - encWidth;
    int h = encHeight;

    int32_t above = int32_t(ArithBitsGetValue(ab, num));
    if (above) {
        const int32_t v = -int32_t(VarBitsGet1(vb));
        above = (above ^ v) - v;
        above = above * qlevel;
    }
    outp[outOffset] = int16_t(above);
    int32_t aboveLeft = above;
    int32_t prev = above;
    int fromOffset = outOffset;
    ++outOffset;

    int w;
    if (encWidth == 1) {
        if (--h == 0) return;
        w = encWidth;
        outOffset += yadj;
        fromOffset = outOffset - pixelPitch;
        above = outp[fromOffset++];
        aboveLeft = above;
        prev = above;
    } else {
        w = encWidth - 1;
    }

    DecodeHigh1Body(ab, vb, outp, outOffset, pixelPitch, encWidth, encHeight, ctx, num, numl, qlevel,
        yadj, h, above, aboveLeft, prev, fromOffset, w);
}

} // namespace

int PlaneDecode(const uint8_t* src, int srcOffset, int16_t* output, int outOffset,
    int width, int height, uint8_t* rowMaskOrNull)
{
    const uint32_t arithLen = VarBitsReadU32LE(src, srcOffset);
    const int varbitsStart = srcOffset + 8 + int(arithLen);

    ArithBits ab = ArithBitOpen(src, srcOffset + 8);
    VarBits vb = VarBitsOpen(src, varbitsStart);

    DecodeLow  (ab, vb, output, outOffset,                          width * 16, width >> 4, height >> 4);
    DecodeHigh1(ab, vb, output, outOffset + (width >> 4),           width * 16, width >> 4, height >> 4);
    DecodeHigh1(ab, vb, output, outOffset + (width * 8),            width * 16, width >> 4, height >> 4);
    DecodeHigh1(ab, vb, output, outOffset + (width >> 4) + (width * 8), width * 16, width >> 4, height >> 4);

    DecodeHigh1(ab, vb, output, outOffset + (width >> 3),           width * 8,  width >> 3, height >> 3);
    DecodeHigh1(ab, vb, output, outOffset + (width * 4),            width * 8,  width >> 3, height >> 3);
    DecodeHigh1(ab, vb, output, outOffset + (width >> 3) + (width * 4), width * 8, width >> 3, height >> 3);

    DecodeHigh1(ab, vb, output, outOffset + (width >> 2),           width * 4,  width >> 2, height >> 2);
    DecodeHigh1(ab, vb, output, outOffset + (width * 2),            width * 4,  width >> 2, height >> 2);
    DecodeHigh1(ab, vb, output, outOffset + (width >> 2) + (width * 2), width * 4, width >> 2, height >> 2);

    DecodeHigh1(ab, vb, output, outOffset + (width >> 1),           width * 2,  width >> 1, height >> 1);
    DecodeHigh1(ab, vb, output, outOffset + width,                  width * 2,  width >> 1, height >> 1);
    DecodeHigh1(ab, vb, output, outOffset + (width >> 1) + width,   width * 2,  width >> 1, height >> 1);

    if (rowMaskOrNull)
        ReadEscapes(ab, rowMaskOrNull, height);

    const uint32_t varbitsLen = VarBitsReadU32LE(src, srcOffset + 4);
    return int(arithLen + varbitsLen + 8);
}

} // namespace igc
} // namespace ro::gr2
