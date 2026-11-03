// Inverse DWT. MIT attribution: third_party/gr2-reader/LICENSE.
#include "Gr2IgcInternal.h"
#include <cstdint>

namespace ro::gr2 {
namespace igc {
namespace {

// Truncate an in-range floating-point value toward zero.
static inline int32_t I32(double x) { return int32_t(x); }

constexpr int SMALLEST_DWT_ROW = 16;
constexpr int SMALLEST_DWT_COL = 16;
constexpr int FLIPSIZE = 8;

static int32_t roundS16(double x) {
    // Fixed-point signed rounding with a sign-dependent bias.
    const int32_t xi = I32(x);
    const int32_t sign = xi >> 31;
    return I32((double(xi) + (32767 ^ sign)) / 65536.0);
}

// Fixed-size stack scratch avoids heap allocation in the wavelet kernels.
// Each row seeds its working window before use. The column recenter must
// explicitly zero missing lookahead entries at the window boundary.
// Native loading runs multiple decodes concurrently: scratch storage must
// belong to each call, never shared module storage.

static void iDWTrow(int16_t* dest, int destPitch, const int16_t* src, int srcPitch, int width, int height, const uint8_t* rowMask, int startY, int subHeight) {
    // wavelet.c:467 — iDWTrow. Combines `halfwidth` low + `halfwidth` high
    // S16s per row into a full-width S16 row.
    // - dest, src : Int16Array
    // - destPitch, srcPitch : S16 index pitch (row stride in S16 elements)
    // - width : output width in S16 elements (= 2 * halfwidth)
    // - rowMask : Uint8Array | nullptr (nullptr → process every row as non-zero)
    // - startY, subHeight : row range to process
    const auto halfwidth = width >> 1;

    auto outBase = startY * destPitch;
    auto linBase = startY * srcPitch;
    auto hinBase = startY * srcPitch + halfwidth; // hin starts at low-end + halfwidth (in S16 units)
    auto maskIdx = startY;

    // ringbuffer-like state for the row : lp[ 8 ] + hp[ 8 ] with -base
    // offsets. The signed 32-bit arrays are indexed 0..7 (lp and hp), using
    // offset +1 (lp) and +2 (hp) as the "center" of the kernel window.
    // Pooled — fully re-seeded per row by the initial-fill below.
    int32_t lp[8]{};
    int32_t hp[8]{};

    for (auto y = 0; y < subHeight; y++) {
        auto next = 1; // S16-element step (was 2 bytes in SDK)

        auto xoutIdx = outBase;
        auto xlinIdx = linBase;
        auto xhinIdx = hinBase;
        const auto linEnd = linBase + halfwidth; // boundary for clamp (was lin+width bytes = lin+halfwidth S16)

        // Initial population : 6 lp + 6 hp values.
        // SDK reads 6 low pixels into lp[0..5] (with offset +1) :
        //   lp[0+1] = lin[0]
        //   lp[-1+1] = lp[1+1] = lin[1]   (boundary mirror)
        //   lp[2+1] = lin[2]
        //   lp[2+1+1] = lin[3]
        //   lp[2+1+2] = lin[4]
        //   lp[2+1+3] = lin[5]
        //   xlin += 12 bytes = 6 S16
        lp[0 + 1] = src[xlinIdx + 0];
        lp[-1 + 1] = lp[1 + 1] = src[xlinIdx + 1];
        lp[2 + 1] = src[xlinIdx + 2];
        lp[2 + 1 + 1] = src[xlinIdx + 3];
        lp[2 + 1 + 2] = src[xlinIdx + 4];
        lp[2 + 1 + 3] = src[xlinIdx + 5];
        xlinIdx += 6;

        // 6 hp values with boundary mirror :
        //   hp[0+2] = hin[0], hp[1+2] = hin[1], hp[2+2] = hin[2],
        //   hp[2+2+1] = hin[3], hp[2+2+2] = hin[4], hp[2+2+3] = hin[5]
        //   hp[-2+2] = hp[1+2]   (boundary)
        //   hp[-1+2] = hp[0+2]
        //   xhin += 12 bytes = 6 S16
        hp[0 + 2] = src[xhinIdx + 0];
        hp[1 + 2] = src[xhinIdx + 1];
        hp[2 + 2] = src[xhinIdx + 2];
        hp[2 + 2 + 1] = src[xhinIdx + 3];
        hp[2 + 2 + 2] = src[xhinIdx + 4];
        hp[2 + 2 + 3] = src[xhinIdx + 5];
        hp[-2 + 2] = hp[1 + 2];
        hp[-1 + 2] = hp[0 + 2];
        xhinIdx += 6;

        // We've consumed 6 of the halfwidth S16 from lin + 6 from hin.
        // Remaining iterations : halfwidth - 6 of them, in groups of 4 + tail.
        // SDK : x = (halfwidth < 8) ? 0 : (halfwidth - 8) / 4. The "8" is
        // because the unrolled body reads 2 ahead (initial 6 + 2 ahead).

        const auto isNonZero = (rowMask == nullptr) || rowMask[maskIdx];

        auto x = (halfwidth < 8) ? 0 : ((halfwidth - 8) / 4);

        if (isNonZero) {
            // groups-of-4 unrolled
            while (x-- > 0) {
                // 4 pixel pairs (e1/o1, e2/o2, e3/o3, e4/o4) using lp[k+1+i]
                // and hp[k+2+i] with i=0..3
                int32_t e1, o1, e2, o2, e3, o3, e4, o4;

                e1 = (lp[0 + 1] * 51674)
                    - (((lp[-1 + 1] + lp[1 + 1]) * 2667))
                    - (((hp[-2 + 2] + hp[1 + 2]) * 1563))
                    + (((hp[-1 + 2] + hp[0 + 2]) * 24733));
                o1 = (((lp[0 + 1] + lp[1 + 1]) * 27400))
                    - (((lp[-1 + 1] + lp[2 + 1]) * 4230))
                    - (I32(hp[0 + 2] * 55882))
                    - (((hp[-2 + 2] + hp[2 + 2]) * 2479))
                    + (((hp[-1 + 2] + hp[1 + 2]) * 7250));

                e2 = (lp[0 + 1 + 1] * 51674)
                    - (((lp[-1 + 1 + 1] + lp[1 + 1 + 1]) * 2667))
                    - (((hp[-2 + 2 + 1] + hp[1 + 2 + 1]) * 1563))
                    + (((hp[-1 + 2 + 1] + hp[0 + 2 + 1]) * 24733));
                o2 = (((lp[0 + 1 + 1] + lp[1 + 1 + 1]) * 27400))
                    - (((lp[-1 + 1 + 1] + lp[2 + 1 + 1]) * 4230))
                    - (I32(hp[0 + 2 + 1] * 55882))
                    - (((hp[-2 + 2 + 1] + hp[2 + 2 + 1]) * 2479))
                    + (((hp[-1 + 2 + 1] + hp[1 + 2 + 1]) * 7250));

                e3 = (lp[0 + 1 + 2] * 51674)
                    - (((lp[-1 + 1 + 2] + lp[1 + 1 + 2]) * 2667))
                    - (((hp[-2 + 2 + 2] + hp[1 + 2 + 2]) * 1563))
                    + (((hp[-1 + 2 + 2] + hp[0 + 2 + 2]) * 24733));
                o3 = (((lp[0 + 1 + 2] + lp[1 + 1 + 2]) * 27400))
                    - (((lp[-1 + 1 + 2] + lp[2 + 1 + 2]) * 4230))
                    - (I32(hp[0 + 2 + 2] * 55882))
                    - (((hp[-2 + 2 + 2] + hp[2 + 2 + 2]) * 2479))
                    + (((hp[-1 + 2 + 2] + hp[1 + 2 + 2]) * 7250));

                e4 = (lp[0 + 1 + 3] * 51674)
                    - (((lp[-1 + 1 + 3] + lp[1 + 1 + 3]) * 2667))
                    - (((hp[-2 + 2 + 3] + hp[1 + 2 + 3]) * 1563))
                    + (((hp[-1 + 2 + 3] + hp[0 + 2 + 3]) * 24733));
                o4 = (((lp[0 + 1 + 3] + lp[1 + 1 + 3]) * 27400))
                    - (((lp[-1 + 1 + 3] + lp[2 + 1 + 3]) * 4230))
                    - (I32(hp[0 + 2 + 3] * 55882))
                    - (((hp[-2 + 2 + 3] + hp[2 + 2 + 3]) * 2479))
                    + (((hp[-1 + 2 + 3] + hp[1 + 2 + 3]) * 7250));

                e1 = roundS16(e1); o1 = roundS16(o1);
                e2 = roundS16(e2); o2 = roundS16(o2);
                e3 = roundS16(e3); o3 = roundS16(o3);
                e4 = roundS16(e4); o4 = roundS16(o4);

                // Write 8 S16 (e1, o1, e2, o2, e3, o3, e4, o4) packed LE.
                dest[xoutIdx + 0] = e1;
                dest[xoutIdx + 1] = o1;
                dest[xoutIdx + 2] = e2;
                dest[xoutIdx + 3] = o2;
                dest[xoutIdx + 4] = e3;
                dest[xoutIdx + 5] = o3;
                dest[xoutIdx + 6] = e4;
                dest[xoutIdx + 7] = o4;
                xoutIdx += 8;

                // Shift lp/hp ring buffer 4 positions left and refill.
                lp[0] = lp[4]; lp[1] = lp[5]; lp[2] = lp[6];
                lp[3] = src[xlinIdx + 0];
                lp[4] = src[xlinIdx + 1];
                lp[5] = src[xlinIdx + 2];
                lp[6] = src[xlinIdx + 3];

                hp[0] = hp[4]; hp[1] = hp[5]; hp[2] = hp[6]; hp[3] = hp[7];
                hp[4] = src[xhinIdx + 0];
                hp[5] = src[xhinIdx + 1];
                hp[6] = src[xhinIdx + 2];
                hp[7] = src[xhinIdx + 3];

                xlinIdx += 4;
                xhinIdx += 4;
            }

            // Remnants : (halfwidth & 3) + 8 single pixels (or all of
            // halfwidth if halfwidth < 8).
            auto xRem = (halfwidth < 8) ? halfwidth : ((halfwidth & 3) + 8);

            while (xRem-- > 0) {
                // Boundary check : xlinIdx == hinBase means we've reached
                // the end of the low band ; mirror.
                if (xlinIdx == linEnd) {
                    xlinIdx -= next;
                    xhinIdx -= next + next;
                    next = -next;
                }

                auto e = (lp[0 + 1] * 51674)
                    - (((lp[-1 + 1] + lp[1 + 1]) * 2667))
                    - (((hp[-2 + 2] + hp[1 + 2]) * 1563))
                    + (((hp[-1 + 2] + hp[0 + 2]) * 24733));
                auto o = (((lp[0 + 1] + lp[1 + 1]) * 27400))
                    - (((lp[-1 + 1] + lp[2 + 1]) * 4230))
                    - (I32(hp[0 + 2] * 55882))
                    - (((hp[-2 + 2] + hp[2 + 2]) * 2479))
                    + (((hp[-1 + 2] + hp[1 + 2]) * 7250));

                e = roundS16(e);
                o = roundS16(o);

                dest[xoutIdx + 0] = e;
                dest[xoutIdx + 1] = o;
                xoutIdx += 2;

                lp[0] = lp[1]; lp[1] = lp[2]; lp[2] = lp[3];
                lp[3] = lp[4]; lp[4] = lp[5]; lp[5] = lp[6];
                lp[6] = src[xlinIdx];

                hp[0] = hp[1]; hp[1] = hp[2]; hp[2] = hp[3];
                hp[3] = hp[4]; hp[4] = hp[5]; hp[5] = hp[6]; hp[6] = hp[7];
                hp[7] = src[xhinIdx];

                xlinIdx += next;
                xhinIdx += next;
            }
        } else {
            // zero row : H-plane terms dropped
            while (x-- > 0) {
                int32_t e1, o1, e2, o2, e3, o3, e4, o4;
                e1 = (lp[0 + 1] * 51674) - (((lp[-1 + 1] + lp[1 + 1]) * 2667));
                o1 = (((lp[0 + 1] + lp[1 + 1]) * 27400)) - (((lp[-1 + 1] + lp[2 + 1]) * 4230));
                e2 = (lp[0 + 1 + 1] * 51674) - (((lp[-1 + 1 + 1] + lp[1 + 1 + 1]) * 2667));
                o2 = (((lp[0 + 1 + 1] + lp[1 + 1 + 1]) * 27400)) - (((lp[-1 + 1 + 1] + lp[2 + 1 + 1]) * 4230));
                e3 = (lp[0 + 1 + 2] * 51674) - (((lp[-1 + 1 + 2] + lp[1 + 1 + 2]) * 2667));
                o3 = (((lp[0 + 1 + 2] + lp[1 + 1 + 2]) * 27400)) - (((lp[-1 + 1 + 2] + lp[2 + 1 + 2]) * 4230));
                e4 = (lp[0 + 1 + 3] * 51674) - (((lp[-1 + 1 + 3] + lp[1 + 1 + 3]) * 2667));
                o4 = (((lp[0 + 1 + 3] + lp[1 + 1 + 3]) * 27400)) - (((lp[-1 + 1 + 3] + lp[2 + 1 + 3]) * 4230));

                e1 = roundS16(e1); o1 = roundS16(o1);
                e2 = roundS16(e2); o2 = roundS16(o2);
                e3 = roundS16(e3); o3 = roundS16(o3);
                e4 = roundS16(e4); o4 = roundS16(o4);

                dest[xoutIdx + 0] = e1;
                dest[xoutIdx + 1] = o1;
                dest[xoutIdx + 2] = e2;
                dest[xoutIdx + 3] = o2;
                dest[xoutIdx + 4] = e3;
                dest[xoutIdx + 5] = o3;
                dest[xoutIdx + 6] = e4;
                dest[xoutIdx + 7] = o4;
                xoutIdx += 8;

                lp[0] = lp[4]; lp[1] = lp[5]; lp[2] = lp[6];
                lp[3] = src[xlinIdx + 0];
                lp[4] = src[xlinIdx + 1];
                lp[5] = src[xlinIdx + 2];
                lp[6] = src[xlinIdx + 3];

                xlinIdx += 4;
            }

            auto xRem = (halfwidth < 8) ? halfwidth : ((halfwidth & 3) + 8);
            while (xRem-- > 0) {
                if (xlinIdx == linEnd) {
                    xlinIdx -= 1;
                    next = -1;
                }

                auto e = (lp[0 + 1] * 51674) - (((lp[-1 + 1] + lp[1 + 1]) * 2667));
                auto o = (((lp[0 + 1] + lp[1 + 1]) * 27400)) - (((lp[-1 + 1] + lp[2 + 1]) * 4230));

                e = roundS16(e);
                o = roundS16(o);

                dest[xoutIdx + 0] = e;
                dest[xoutIdx + 1] = o;
                xoutIdx += 2;

                lp[0] = lp[1]; lp[1] = lp[2]; lp[2] = lp[3];
                lp[3] = lp[4]; lp[4] = lp[5]; lp[5] = lp[6];
                lp[6] = src[xlinIdx];

                xlinIdx += next;
            }
        }

        outBase += destPitch;
        linBase += srcPitch;
        hinBase += srcPitch;
        ++maskIdx;
    }
}

static void iDWTcol(int16_t* dest, int destPitch, const int16_t* src, int srcPitch, int width, int height, int startY, int subHeight) {
    // wavelet.c:836 — iDWTcol. Inverse-DWT along columns. Inner loop
    // unrolled to 4 cols at a time.
    const auto halfheight = subHeight >> 1;

    auto outBase = startY * destPitch;
    auto linBase = 0;          // S16 index, low band starts at col 0
    auto hinBase = srcPitch;   // high band starts at col 0 of row 1 (since each "L" row alternates with "H" row in input layout)
    // `lendCol0` = col-0 end-of-buffer marker. Per-column boundary marker is
    // `lendCol0 + colsBase` (asm cite : granny2.dll @ 0x10008981 sets
    // [var_1a4h], 0x1000949a advances by 2 bytes = 1 S16 per col-iteration).
    // Without the +colsBase term, the strict-equality check at 0x100092f6
    // misses for any group beyond colsBase=0 and the reflect never fires.
    const auto lendCol0 = srcPitch * height;
    const auto ppitch2 = srcPitch * 2;   // we increment by 2 srcPitch per pair (one L row + one H row)

    if (startY) {
        linBase += ((startY / I32(2)) - 1) * ppitch2;
        hinBase += ((startY / I32(2)) - 2) * ppitch2;
    }

    // Process 4-col groups
    auto colsBase = 0;
    const auto groupCount = I32(width / 4);

    // Ring buffer : lp[(16+4)][4], hp[(16+5)][4]. Each entry is a 4-tuple of
    // S16 columns stored in flat ring arrays of size 20*4 / 21*4.

    for (auto g = 0; g < groupCount; g++) {
        auto next = ppitch2;
        auto youtBase = outBase + colsBase;
        auto ylinBase = linBase + colsBase;
        auto yhinBase = hinBase + colsBase;
        const auto lend = lendCol0 + colsBase;

        int32_t lp[(16 + 4) * 4]{};
        int32_t hp[(16 + 5) * 4]{};

        // Initial fill — startY ? 4L+5H : 3L+3H with boundary mirror
        if (startY) {
            // lp[-1+1..2+1] from ylin (4 rows)
            for (auto k = 0; k < 4; k++) lp[(-1 + 1) * 4 + k] = src[ylinBase + k];
            ylinBase += next;
            for (auto k = 0; k < 4; k++) lp[(0 + 1) * 4 + k] = src[ylinBase + k];
            ylinBase += next;
            for (auto k = 0; k < 4; k++) lp[(1 + 1) * 4 + k] = src[ylinBase + k];
            ylinBase += next;
            for (auto k = 0; k < 4; k++) lp[(2 + 1) * 4 + k] = src[ylinBase + k];
            ylinBase += next;

            for (auto k = 0; k < 4; k++) hp[(-2 + 2) * 4 + k] = src[yhinBase + k];
            yhinBase += next;
            for (auto k = 0; k < 4; k++) hp[(-1 + 2) * 4 + k] = src[yhinBase + k];
            yhinBase += next;
            for (auto k = 0; k < 4; k++) hp[(0 + 2) * 4 + k] = src[yhinBase + k];
            yhinBase += next;
            for (auto k = 0; k < 4; k++) hp[(1 + 2) * 4 + k] = src[yhinBase + k];
            yhinBase += next;
            for (auto k = 0; k < 4; k++) hp[(2 + 2) * 4 + k] = src[yhinBase + k];
            yhinBase += next;
        } else {
            for (auto k = 0; k < 4; k++) lp[(0 + 1) * 4 + k] = src[ylinBase + k];
            ylinBase += next;
            for (auto k = 0; k < 4; k++) {
                const auto v = src[ylinBase + k];
                lp[(-1 + 1) * 4 + k] = v;
                lp[(1 + 1) * 4 + k] = v;
            }
            ylinBase += next;
            for (auto k = 0; k < 4; k++) lp[(2 + 1) * 4 + k] = src[ylinBase + k];
            ylinBase += next;

            for (auto k = 0; k < 4; k++) {
                const auto v = src[yhinBase + k];
                hp[(-1 + 2) * 4 + k] = v;
                hp[(0 + 2) * 4 + k] = v;
            }
            yhinBase += next;
            for (auto k = 0; k < 4; k++) {
                const auto v = src[yhinBase + k];
                hp[(-2 + 2) * 4 + k] = v;
                hp[(1 + 2) * 4 + k] = v;
            }
            yhinBase += next;
            for (auto k = 0; k < 4; k++) hp[(2 + 2) * 4 + k] = src[yhinBase + k];
            yhinBase += next;
        }

        // lp/hp index into a sliding window. SDK uses `lp` and `hp` pointers
        // that advance through the local array.
        auto lpOff = 0; // base index into lp[]
        auto hpOff = 0;

        for (auto y = 0; y < halfheight; y++) {
            // boundary check
            if (ylinBase == lend) {
                ylinBase -= next;
                yhinBase -= next + next;
                next = -next;
            }

            // 4 columns at once
            const auto lpC = lpOff;
            const auto hpC = hpOff;
            int32_t e1=0, e2=0, e3=0, e4=0, o1=0, o2=0, o3=0, o4=0;

            for (auto k = 0; k < 4; k++) {
                const auto lpm1 = lp[(lpC + 0) * 4 + k];
                const auto lp0 = lp[(lpC + 1) * 4 + k];
                const auto lp1 = lp[(lpC + 2) * 4 + k];
                const auto lp2 = lp[(lpC + 3) * 4 + k];

                const auto hpm2 = hp[(hpC + 0) * 4 + k];
                const auto hpm1 = hp[(hpC + 1) * 4 + k];
                const auto hp0 = hp[(hpC + 2) * 4 + k];
                const auto hp1 = hp[(hpC + 3) * 4 + k];
                const auto hp2 = hp[(hpC + 4) * 4 + k];

                const auto e = (lp0 * 51674)
                    - (((lpm1 + lp1) * 2667))
                    - (((hpm2 + hp1) * 1563))
                    + (((hpm1 + hp0) * 24733));
                const auto o = (((lp0 + lp1) * 27400))
                    - (((lpm1 + lp2) * 4230))
                    - (I32(hp0 * 55882))
                    - (((hpm2 + hp2) * 2479))
                    + (((hpm1 + hp1) * 7250));

                const auto er = roundS16(e);
                const auto oRounded = roundS16(o);

                if (k == 0) { e1 = er; o1 = oRounded; }
                else if (k == 1) { e2 = er; o2 = oRounded; }
                else if (k == 2) { e3 = er; o3 = oRounded; }
                else { e4 = er; o4 = oRounded; }
            }

            dest[youtBase + 0] = e1;
            dest[youtBase + 1] = e2;
            dest[youtBase + 2] = e3;
            dest[youtBase + 3] = e4;
            dest[youtBase + destPitch + 0] = o1;
            dest[youtBase + destPitch + 1] = o2;
            dest[youtBase + destPitch + 2] = o3;
            dest[youtBase + destPitch + 3] = o4;

            youtBase += destPitch + destPitch;

            ++lpOff;
            ++hpOff;

            // re-center if we've hit the end of the local buffers
            // SDK : if (&lp[3][0] == &a.hp[0][0]) → since lp is [16+4][4] = 20 entries, hp follows immediately at offset 20. So when lpOff = 17, lp[3] = entry 20 = hp[0].
            if (lpOff + 3 == 16 + 4) {
                for (auto k = 0; k < 4; k++) lp[0 * 4 + k] = lp[(lpOff + 1) * 4 + k];
                for (auto k = 0; k < 4; k++) lp[1 * 4 + k] = lp[(lpOff + 2) * 4 + k];
                // The lookahead entry lies past the window boundary. Zero-pad
                // explicitly rather than reading outside the local arrays.
                for (auto k = 0; k < 4; k++) lp[2 * 4 + k] = 0;
                for (auto k = 0; k < 4; k++) hp[0 * 4 + k] = hp[(hpOff + 1) * 4 + k];
                for (auto k = 0; k < 4; k++) hp[1 * 4 + k] = hp[(hpOff + 2) * 4 + k];
                for (auto k = 0; k < 4; k++) hp[2 * 4 + k] = hp[(hpOff + 3) * 4 + k];
                for (auto k = 0; k < 4; k++) hp[3 * 4 + k] = 0;
                lpOff = 0;
                hpOff = 0;
            }

            // refill : lp[3][...] = ylin, hp[4][...] = yhin
            for (auto k = 0; k < 4; k++) lp[(lpOff + 3) * 4 + k] = src[ylinBase + k];
            for (auto k = 0; k < 4; k++) hp[(hpOff + 4) * 4 + k] = src[yhinBase + k];

            ylinBase += next;
            yhinBase += next;
        }

        colsBase += 4;
    }

    // Remaining columns (width & 3)
    const auto remCols = width & 3;
    for (auto g = 0; g < remCols; g++) {
        auto next = ppitch2;
        auto youtBase = outBase + colsBase;
        auto ylinBase = linBase + colsBase;
        auto yhinBase = hinBase + colsBase;
        const auto lend = lendCol0 + colsBase;

        int32_t lp[4]{};
        int32_t hp[5]{};

        if (startY) {
            lp[-1 + 1] = src[ylinBase]; ylinBase += next;
            lp[0 + 1] = src[ylinBase]; ylinBase += next;
            lp[1 + 1] = src[ylinBase]; ylinBase += next;
            lp[2 + 1] = src[ylinBase]; ylinBase += next;
            hp[-2 + 2] = src[yhinBase]; yhinBase += next;
            hp[-1 + 2] = src[yhinBase]; yhinBase += next;
            hp[0 + 2] = src[yhinBase]; yhinBase += next;
            hp[1 + 2] = src[yhinBase]; yhinBase += next;
            hp[2 + 2] = src[yhinBase]; yhinBase += next;
        } else {
            lp[0 + 1] = src[ylinBase]; ylinBase += next;
            lp[-1 + 1] = lp[1 + 1] = src[ylinBase]; ylinBase += next;
            lp[2 + 1] = src[ylinBase]; ylinBase += next;
            hp[-1 + 2] = hp[0 + 2] = src[yhinBase]; yhinBase += next;
            hp[-2 + 2] = hp[1 + 2] = src[yhinBase]; yhinBase += next;
            hp[2 + 2] = src[yhinBase]; yhinBase += next;
        }

        for (auto y = 0; y < halfheight; y++) {
            if (ylinBase == lend) {
                ylinBase -= next;
                yhinBase -= next + next;
                next = -next;
            }

            const auto e = (lp[0 + 1] * 51674)
                - (((lp[-1 + 1] + lp[1 + 1]) * 2667))
                - (((hp[-2 + 2] + hp[1 + 2]) * 1563))
                + (((hp[-1 + 2] + hp[0 + 2]) * 24733));
            const auto o = (((lp[0 + 1] + lp[1 + 1]) * 27400))
                - (((lp[-1 + 1] + lp[2 + 1]) * 4230))
                - (I32(hp[0 + 2] * 55882))
                - (((hp[-2 + 2] + hp[2 + 2]) * 2479))
                + (((hp[-1 + 2] + hp[1 + 2]) * 7250));

            dest[youtBase] = roundS16(e);
            dest[youtBase + destPitch] = roundS16(o);

            youtBase += destPitch + destPitch;

            lp[0] = lp[1]; lp[1] = lp[2]; lp[2] = lp[3];
            lp[3] = src[ylinBase];

            hp[0] = hp[1]; hp[1] = hp[2]; hp[2] = hp[3]; hp[3] = hp[4];
            hp[4] = src[yhinBase];

            ylinBase += next;
            yhinBase += next;
        }

        colsBase += 1;
    }
}

static void iHarrrow(int16_t* dest, int destPitch, const int16_t* src, int srcPitch, int width, int height, const uint8_t* rowMask, int startY, int subHeight) {
    // wavelet.c:1175 — iHarrrow. Haar inverse row (small-width fallback).
    const auto halfwidth = width >> 1;

    auto outBase = startY * destPitch;
    auto linBase = startY * srcPitch;
    auto hinBase = startY * srcPitch + halfwidth;
    auto maskIdx = startY;

    for (auto y = 0; y < subHeight; y++) {
        auto xoutIdx = outBase;
        auto xlinIdx = linBase;
        auto xhinIdx = hinBase;

        const auto isNonZero = (rowMask == nullptr) || rowMask[maskIdx];

        if (isNonZero) {
            for (auto x = 0; x < halfwidth; x++) {
                const auto lv = src[xlinIdx];
                const auto hv = src[xhinIdx];
                auto e = I32(lv * 2 + hv);
                auto o = I32(lv * 2 - hv);
                e = ((e + (1 ^ (e >> 31))) / 2);
                o = ((o + (1 ^ (o >> 31))) / 2);
                dest[xoutIdx + 0] = e;
                dest[xoutIdx + 1] = o;
                xoutIdx += 2;
                xlinIdx += 1;
                xhinIdx += 1;
            }
        } else {
            for (auto x = 0; x < halfwidth; x++) {
                const auto e = src[xlinIdx];
                dest[xoutIdx + 0] = e;
                dest[xoutIdx + 1] = e;
                xoutIdx += 2;
                xlinIdx += 1;
            }
        }

        outBase += destPitch;
        linBase += srcPitch;
        hinBase += srcPitch;
        ++maskIdx;
    }
}

static void iHarrcol(int16_t* dest, int destPitch, const int16_t* src, int srcPitch, int width, int height, int startY, int subHeight) {
    // wavelet.c:1261 — iHarrcol.
    const auto halfheight = subHeight >> 1;

    auto outBase = startY * destPitch;
    auto linBase = 0;
    auto hinBase = srcPitch;
    const auto ppitch2 = srcPitch * 2;

    if (startY) {
        linBase += (startY / I32(2)) * ppitch2;
        hinBase += (startY / I32(2)) * ppitch2;
    }

    for (auto x = 0; x < width; x++) {
        auto youtBase = outBase + x;
        auto ylinBase = linBase + x;
        auto yhinBase = hinBase + x;

        for (auto y = 0; y < halfheight; y++) {
            const auto lv = src[ylinBase];
            const auto hv = src[yhinBase];
            auto e = I32(lv * 2 + hv);
            auto o = I32(lv * 2 - hv);
            e = ((e + (1 ^ (e >> 31))) / 2);
            o = ((o + (1 ^ (o >> 31))) / 2);
            dest[youtBase] = e;
            dest[youtBase + destPitch] = o;
            youtBase += destPitch + destPitch;
            ylinBase += ppitch2;
            yhinBase += ppitch2;
        }
    }
}

} // namespace

void IDWT2D(int16_t* output, int pitch, int width, int height, const uint8_t* rowMask, int16_t* temp) {
    // wavelet.c:1328 — iDWT2D dispatcher.
    // - output, temp : Int16Array, same length as the plane
    // - pitch : S16-index pitch
    const auto rowFn = (width >= SMALLEST_DWT_ROW) ? iDWTrow : iHarrrow;
    const auto colFn = (height >= SMALLEST_DWT_COL) ? iDWTcol : iHarrcol;

    auto ry = (height <= (FLIPSIZE + 4 + 4)) ? height : (FLIPSIZE + 4);
    auto rh = height - ry;
    auto cy = 0;
    auto ch = height;

    rowFn(temp, pitch, output, pitch, width, height, rowMask, 0, ry);

    do {
        auto next = (ch <= (FLIPSIZE + 4)) ? ch : FLIPSIZE;
        colFn(output, pitch, temp, pitch, width, height, cy, next);
        cy += next;
        ch -= next;

        if (rh) {
            next = (rh <= (FLIPSIZE + 4)) ? rh : FLIPSIZE;
            rowFn(temp, pitch, output, pitch, width, height, rowMask, ry, next);
            ry += next;
            rh -= next;
        }
    } while (ch);
}

} // namespace igc
} // namespace ro::gr2
