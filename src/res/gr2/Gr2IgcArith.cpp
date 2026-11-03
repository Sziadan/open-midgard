// Arith range-coder + model. MIT attribution: third_party/gr2-reader/LICENSE.
#include "Gr2IgcInternal.h"
#include <array>
#include <algorithm>
#include <cstdint>
#include <stdexcept>

namespace ro::gr2 {
namespace igc {
namespace {

constexpr int IGC_MAX_ALPHABET = 8192;
constexpr int ARITH_RESCALE_TRIGGER = 0x4000;

static inline uint32_t U32(uint32_t x) { return x; }
static inline int32_t I32(int32_t x) { return x; }

static const uint8_t BR4[16] = {0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15};
static const uint8_t BR3[8] = {0, 4, 2, 6, 1, 5, 3, 7};
// Immutable compile-time table; first-use initialization also used to race.
static constexpr auto BR8=[] {
    std::array<uint8_t,256> table{};
    for(int i=0;i<256;++i)table[i]=uint8_t((BR4[i&0xF]<<4)|BR4[(i>>4)&0xF]);
    return table;
}();

static uint32_t ArithPullBits(ArithBits& ab, int n)
{
    int bitsLeft = ab.bitsLeft;
    uint32_t accum = ab.accum;
    uint32_t raw;
    if (bitsLeft < n) {
        const uint8_t* buf = ab.buf;
        const int p = ab.ptr;
        const uint32_t refill = uint32_t(buf[p] | (buf[p + 1] << 8) | (buf[p + 2] << 16) | (buf[p + 3] << 24));
        const uint32_t eax = (bitsLeft == 0) ? refill : uint32_t((refill << bitsLeft) | accum);
        ab.accum = uint32_t(refill >> (n - bitsLeft));
        ab.bitsLeft = bitsLeft + 32 - n;
        ab.ptr = p + 4;
        raw = eax & uint32_t((1 << n) - 1);
    } else {
        raw = accum & uint32_t((1 << n) - 1);
        ab.accum = uint32_t(accum >> n);
        ab.bitsLeft = bitsLeft - n;
    }
    if (n == 8) return BR8[raw];
    if (n == 4) return BR4[raw];
    return raw;
}

static void ArithRenorm(ArithBits& ab)
{
    uint32_t h = ab.high;
    uint32_t l = ab.low;
    uint32_t t = ab.target;
    uint32_t xh = uint32_t(l ^ h);

    if ((xh & 0x40000000u) == 0) {
        if ((xh & 0x7F800000u) == 0) {
            do {
                h = uint32_t(((h << 8) | 0xff) & 0x7FFFFFFF);
                l = uint32_t((l << 8) & 0x7FFFFFFF);
                const uint32_t b = ArithPullBits(ab, 8);
                t = uint32_t(((t << 8) | b) & 0x7FFFFFFF);
                xh = uint32_t(l ^ h);
            } while ((xh & 0x7F800000u) == 0);
        }
        if ((xh & 0x78000000u) == 0) {
            h = uint32_t(((h << 4) | 0xf) & 0x7FFFFFFF);
            l = uint32_t((l << 4) & 0x7FFFFFFF);
            const uint32_t n = ArithPullBits(ab, 4);
            t = uint32_t(((t << 4) | n) & 0x7FFFFFFF);
            xh = uint32_t(l ^ h);
        }
        if ((xh & 0x40000000u) == 0) {
            do {
                h = uint32_t(((h << 1) | 1) & 0x7FFFFFFF);
                l = uint32_t((l << 1) & 0x7FFFFFFF);
                const uint32_t bit = ArithPullBits(ab, 1);
                t = uint32_t(((t << 1) | bit) & 0x7FFFFFFF);
                xh = uint32_t(l ^ h);
            } while ((xh & 0x40000000u) == 0);
        }
    }

    if ((l & 0x20000000u) != 0) {
        while (true) {
            if ((h & 0x20000000u) != 0) break;
            l = uint32_t(((l & 0x1FFFFFFF) << 1) & 0x7FFFFFFF);
            h = uint32_t(((h << 1) | 0x40000001u) & 0x7FFFFFFF);
            t = uint32_t(t ^ 0x20000000u);
            const uint32_t bit = ArithPullBits(ab, 1);
            t = uint32_t(((t << 1) | bit) & 0x7FFFFFFF);
            if ((l & 0x20000000u) == 0) break;
        }
    }

    ab.high = h;
    ab.low = l;
    ab.target = t;
}

static void ArithInitBands(ArithModel& a, int uniqueValuesPlus1)
{
    if (uniqueValuesPlus1 < 6) {
        a.bucketSize = 0;
        a.shiftDepth = 15;
        a.bandBoundary = 0;
        return;
    }
    int bestShift = 0;
    uint32_t bestLeftover = 0xffffffffu;
    for (int cl = 0; cl < 16; ++cl) {
        const int bucketSize = 1 << cl;
        uint32_t nbBuckets = uint32_t((bucketSize + uniqueValuesPlus1 - 1) / bucketSize);
        if (nbBuckets > 16) nbBuckets = 16;
        uint32_t leftover = uint32_t(uniqueValuesPlus1 - int(nbBuckets - 1) * bucketSize);
        if (leftover < uint32_t(bucketSize)) leftover = uint32_t(bucketSize);
        if (leftover < bestLeftover) {
            bestShift = cl;
            bestLeftover = leftover;
        }
        if (bucketSize > uniqueValuesPlus1) break;
    }
    a.bucketSize = (1 << bestShift) & 0xffff;
    a.shiftDepth = bestShift & 0xff;
    a.bandBoundary = (15 * a.bucketSize) & 0xffff;
}

static void ArithUpdate(ArithModel& a, int idx, uint32_t magic)
{
    const uint32_t lo = magic & 0xffffu;
    if (idx >= a.bandBoundary) {
        a.cumCounts[15] = uint16_t((a.cumCounts[15] + lo) & 0xffffu);
        a.singleCounts[size_t(idx)] = uint16_t((a.singleCounts[size_t(idx)] + lo) & 0xffffu);
        return;
    }
    uint32_t d = uint32_t(idx) >> a.shiftDepth;
    if (d & 1u) {
        a.cumCounts[d] = uint16_t((a.cumCounts[d] + lo) & 0xffffu);
        ++d;
    }
    d >>= 1;
    if (d <= 7) {
        const uint32_t umagic = magic;
        for (uint32_t k = d; k <= 7; ++k) {
            const int j = int(k * 2);
            const uint32_t oldDword = uint32_t(a.cumCounts[j + 1] * 0x10000u + a.cumCounts[j]);
            const uint32_t newDword = uint32_t(oldDword + umagic);
            a.cumCounts[j] = uint16_t(newDword & 0xffffu);
            a.cumCounts[j + 1] = uint16_t((newDword >> 16) & 0xffffu);
        }
    }
    a.singleCounts[size_t(idx)] = uint16_t((a.singleCounts[size_t(idx)] + lo) & 0xffffu);
}

static void ArithSearch(ArithModel& a, uint32_t offset, int& outIdx, uint32_t& outLo)
{
    auto& cc = a.cumCounts;
    int bucket = 0;
    uint32_t lo = 0;

    if (offset >= cc[7]) { lo = cc[7]; bucket = 8; }
    if (offset >= cc[bucket + 3]) { lo = cc[bucket + 3]; bucket += 4; }
    if (offset >= cc[bucket + 1]) { lo = cc[bucket + 1]; bucket += 2; }
    if (offset >= cc[bucket]) { lo = cc[bucket]; bucket += 1; }

    for (int i = bucket; i < 16; ++i)
        cc[i] = uint16_t((cc[i] + 1) & 0xffffu);

    const int baseIdx = bucket * a.bucketSize;
    int idx = baseIdx;
    uint32_t cumLo = lo;
    auto& sc = a.singleCounts;
    const int end = int((std::min)(sc.size(), size_t((a.uniqueCount + 1))));
    while (idx < end) {
        const uint32_t high = uint32_t((cumLo + sc[size_t(idx)]) & 0xffffu);
        if (offset < high) {
            outIdx = idx;
            outLo = cumLo;
            return;
        }
        cumLo = high;
        ++idx;
    }
    const int fallback = (std::min)((std::max)(idx - 1, 0), (std::max)(a.uniqueCount, 0));
    outIdx = fallback;
    outLo = cumLo;
}

static void ArithReEmitCumFromScratch(ArithModel& a, uint32_t scratch[16])
{
    if (a.singlesLength != a.uniqueCount && a.singleCounts[0] == 0) {
        a.singleCounts[0] = 2;
        const int bucket0 = (0 >= a.bandBoundary) ? 15 : 0;
        scratch[bucket0] = uint32_t(scratch[bucket0] + 2);
    }
    uint32_t cum = 0;
    for (int k = 0; k < 16; ++k) {
        cum = uint32_t((cum + scratch[k]) & 0xffffu);
        a.cumCounts[k] = uint16_t(cum);
    }
}

static void ArithRescale(ArithModel& a)
{
    ArithInitBands(a, a.singlesLength + 1);
    uint32_t scratch[16] = {};

    a.singleCounts[0] = uint16_t((a.singleCounts[0] >> 1) & 0xffffu);
    const int bucket0 = (0 >= a.bandBoundary) ? 15 : 0;
    scratch[bucket0] = a.singleCounts[0];

    int max = 0;
    int maxPos = 0;
    int i = 1;
    while (i <= a.singlesLength) {
        while (a.singleCounts[size_t(i)] <= 1) {
            if (i >= a.singlesLength) {
                a.singleCounts[size_t(i)] = 0;
                a.singlesLength = a.singlesLength - 1;
                break;
            }
            a.singleCounts[size_t(i)] = a.singleCounts[size_t(a.singlesLength)];
            a.singleCounts[size_t(a.singlesLength)] = 0;
            a.values[size_t(i)] = a.values[size_t(a.singlesLength)];
            a.singlesLength = a.singlesLength - 1;
        }
        if (i > a.singlesLength) break;
        a.singleCounts[size_t(i)] = uint16_t((a.singleCounts[size_t(i)] >> 1) & 0xffffu);
        const int sc = a.singleCounts[size_t(i)];
        if (sc > max) {
            max = sc;
            maxPos = i;
        }
        const int bucket = (i >= a.bandBoundary) ? 15 : (i >> a.shiftDepth);
        scratch[bucket] = uint32_t(scratch[bucket] + uint32_t(sc));
        ++i;
    }

    if (max > 0) {
        int target;
        if (a.singlesLength >= a.bandBoundary) target = a.bandBoundary;
        else target = (a.singlesLength >> a.shiftDepth) << a.shiftDepth;
        if (target == 0) target = 1;

        if (maxPos != target) {
            const uint32_t oldScTarget = a.singleCounts[size_t(target)];
            const uint32_t scMaxPos = a.singleCounts[size_t(maxPos)];
            const int32_t delta = int32_t(scMaxPos - oldScTarget);

            const int bucketTarget = (target >= a.bandBoundary) ? 15 : (target >> a.shiftDepth);
            const int bucketMaxPos = (maxPos >= a.bandBoundary) ? 15 : (maxPos >> a.shiftDepth);

            scratch[bucketTarget] = uint32_t(scratch[bucketTarget] + uint32_t(delta));
            scratch[bucketMaxPos] = uint32_t(scratch[bucketMaxPos] - uint32_t(delta));

            a.singleCounts[size_t(target)] = uint16_t(scMaxPos);
            a.singleCounts[size_t(maxPos)] = uint16_t(oldScTarget);
            const uint16_t valTarget = a.values[size_t(target)];
            a.values[size_t(target)] = a.values[size_t(maxPos)];
            a.values[size_t(maxPos)] = valTarget;
        }
    }

    ArithReEmitCumFromScratch(a, scratch);
}

} // namespace

ArithBits ArithBitOpen(const uint8_t* buf, int offset)
{
    const int b0 = buf[offset];
    const int b1 = buf[offset + 1];
    const int b2 = buf[offset + 2];
    const int b3 = buf[offset + 3];
    const uint32_t dword = uint32_t(b0 | (b1 << 8) | (b2 << 16) | (b3 << 24));
    const uint32_t topBit = (dword >> 31) & 1u;
    const uint32_t lo31 = dword & 0x7FFFFFFFu;

    uint32_t t = BR4[(lo31 >> 4) & 0xF];
    t |= uint32_t(BR4[lo31 & 0xF] << 4);
    t = uint32_t((t << 4) | BR4[(lo31 >> 8) & 0xF]);
    t = uint32_t((t << 4) | BR4[(lo31 >> 12) & 0xF]);
    t = uint32_t((t << 4) | BR4[(lo31 >> 16) & 0xF]);
    t = uint32_t((t << 4) | BR4[(lo31 >> 20) & 0xF]);
    t = uint32_t((t << 4) | BR4[(lo31 >> 24) & 0xF]);
    t = uint32_t((t << 3) | BR3[(lo31 >> 28) & 7]);

    ArithBits ab;
    ab.buf = buf;
    ab.ptr = offset + 4;
    ab.accum = topBit;
    ab.bitsLeft = 1;
    ab.high = 0x7FFFFFFF;
    ab.low = 0;
    ab.target = t;
    return ab;
}

ArithModel ArithOpen(int /*maxValue*/, int uniqueValues)
{
    if (uniqueValues > IGC_MAX_ALPHABET)
        throw std::runtime_error("igc arith alphabet exceeds cap");
    const int countsSize = (uniqueValues + 5) & ~3;
    ArithModel a;
    a.singleCounts.assign(size_t(countsSize), 0);
    a.values.assign(size_t(countsSize), 0);
    a.singlesLength = 0;
    a.bandBoundary = 0;
    a.shiftDepth = 0;
    a.bucketSize = 0;
    a.uniqueCount = uniqueValues;
    ArithInitBands(a, uniqueValues + 1);
    ArithUpdate(a, 0, 0x30003u);
    return a;
}

void ArithBitsRemove(ArithBits& ab, uint32_t lo, uint32_t count, uint32_t scale)
{
    const uint32_t range = uint32_t(ab.high - ab.low + 1);
    // floor(range * x / scale) via 64-bit — matches Math.floor for these ranges
    const uint32_t num1 = uint32_t((uint64_t(range) * uint64_t(lo + count)) / scale);
    const uint32_t num2 = uint32_t((uint64_t(range) * uint64_t(lo)) / scale);
    const uint32_t oldLow = ab.low;
    ab.high = uint32_t(oldLow + num1 - 1);
    ab.low = uint32_t(oldLow + num2);
    ArithRenorm(ab);
}

uint32_t ArithBitsGet(ArithBits& ab, uint32_t scale)
{
    const uint32_t range = uint32_t(ab.high - ab.low + 1);
    const uint32_t tNorm = uint32_t(ab.target - ab.low + 1);
    int32_t v = int32_t((uint64_t(tNorm) * uint64_t(scale) - 1) / range);
    if (uint32_t(v) >= scale) v = int32_t(scale - 1);
    return uint32_t(v);
}

uint32_t ArithBitsGetValue(ArithBits& ab, uint32_t scale)
{
    const uint32_t range = uint32_t(ab.high - ab.low + 1);
    const uint32_t tNorm = uint32_t(ab.target - ab.low + 1);
    int32_t v = int32_t((uint64_t(tNorm) * uint64_t(scale) - 1) / range);
    if (uint32_t(v) >= scale) v = int32_t(scale - 1);
    ArithBitsRemove(ab, uint32_t(v), 1, scale);
    return uint32_t(v);
}

void ArithSetDecompressed(ArithModel& model, int slot, int value)
{
    model.values[size_t(slot)] = uint16_t(value);
}

bool ArithDecompress(ArithModel& model, ArithBits& bits, int& valueOrSlot)
{
    if (model.cumCounts[15] >= ARITH_RESCALE_TRIGGER)
        ArithRescale(model);

    const uint32_t total = model.cumCounts[15];
    const uint32_t offset = ArithBitsGet(bits, total);
    int refinedIdx = 0;
    uint32_t slotLo = 0;
    ArithSearch(model, offset, refinedIdx, slotLo);
    const uint32_t slotCount = model.singleCounts[size_t(refinedIdx)];

    ArithBitsRemove(bits, slotLo, slotCount, total);
    model.singleCounts[size_t(refinedIdx)] =
        uint16_t((model.singleCounts[size_t(refinedIdx)] + 1) & 0xffffu);

    if (refinedIdx != 0) {
        valueOrSlot = int(model.values[size_t(refinedIdx)]);
        return false;
    }

    model.singlesLength = (model.singlesLength + 1) & 0xffff;
    ArithUpdate(model, model.singlesLength, 0x20002u);

    if (model.singlesLength == model.uniqueCount) {
        const int32_t sc0 = int32_t(model.singleCounts[0]);
        const uint32_t negSc0 = uint32_t((-sc0) & 0xffff);
        const uint32_t magicHi = uint32_t(((-sc0 - 1) & 0xffff) << 16);
        const uint32_t magic = uint32_t(magicHi | negSc0);
        ArithUpdate(model, 0, magic);
    }

    valueOrSlot = model.singlesLength;
    return true;
}

} // namespace igc
} // namespace ro::gr2
