// igc-idwt.js — pure-JS inverse wavelet transform (the byte-exact oracle +
// mandatory fallback for the opt-in WASM build).
//
// Extracted verbatim from GrannyTextureIGC.js : the RAD reversible-integer
// lifting-scheme iDWT that turns a wavelet-transformed S16 plane back into
// pixel-space, in 4 passes at increasing resolution. Like ./igc-yuv.js,
// ./igc-arith.js and ./igc-plane.js, this module is never rewritten — the WASM
// build swaps the seam (./igc-kernels.js) so `iDWT2D` dispatches to the
// WebAssembly module, and this stays the oracle the differential gate compares
// against + the fallback when the wasm module has not instantiated.
//
// Leaked-SDK ref : `wavelet.c:467-1170` (iDWTrow + iDWTcol),
//                  `wavelet.c:1175-1325` (iHarrrow + iHarrcol),
//                  `wavelet.c:1328-1363` (iDWT2D dispatcher).
// Asm cite : `granny2.dll @ 0x10009700`.
//
// All buffers are Int16Array. The SDK uses byte pitches throughout (pointers
// are char*) ; we pre-divide by 2 so all pitch / offset arithmetic in this
// section is in S16 indices (not bytes).
//
// The 9 lifting coefficients (51674, 2667, 1563, 24733, 27400, 4230, 55882,
// 2479, 7250) are the RAD reversible-integer DWT (granny_bink0). Off-by-one
// in any addend = wrong output.
//
// The round-to-S16 trick `(x + (32767 ^ (x >> 31))) / 65536` = signed
// round-half-away-from-zero.

const SMALLEST_DWT_ROW = 16;
const SMALLEST_DWT_COL = 16;
const FLIPSIZE = 8;

function roundS16(x) {
    // (x + (32767 ^ (x >> 31))) / 65536, truncated.
    const sign = x >> 31;
    return ((x + (32767 ^ sign)) / 65536) | 0;
}

// iDWT ring buffers hoisted out of the per-row / per-4-col-group loops.
// `iDWTcol` re-allocated `lp`/`hp` on every 4-col group (the hottest IGC
// function) ; pooling kills that allocation churn on the hottest path.
//
// Two invariants make reuse byte-exact (verified by the content manifest
// across all 17 IGC textures) :
//   1. Each kernel fully (re)initialises every slot it reads before reading
//      it — the initial-fill block seeds the first window, then the
//      just-in-time refill writes entry (k+3) one iteration before it is
//      read. Stale values from a prior group/row are always overwritten
//      first, so no per-iteration `.fill(0)` is needed.
//   2. The `iDWTcol` recenter intentionally reads one entry PAST the end
//      (OOB → undefined → coerced to 0 on store). Sizes must stay EXACTLY
//      the original per-iteration allocation sizes — oversizing would turn
//      that 0 into a stale value and change decoded bytes.
// The kernels are non-reentrant leaves (they never call each other, and
// decode is sequential), so module-scoped pools are safe.
const _rowLp = new Int32Array(8);
const _rowHp = new Int32Array(8);
const _colLp = new Int32Array((16 + 4) * 4);
const _colHp = new Int32Array((16 + 5) * 4);
const _remLp = new Int32Array(4);
const _remHp = new Int32Array(5);

function iDWTrow(dest, destPitch, src, srcPitch, width, height, rowMask, startY, subHeight) {
    // wavelet.c:467 — iDWTrow. Combines `halfwidth` low + `halfwidth` high
    // S16s per row into a full-width S16 row.
    // - dest, src : Int16Array
    // - destPitch, srcPitch : S16 index pitch (row stride in S16 elements)
    // - width : output width in S16 elements (= 2 * halfwidth)
    // - rowMask : Uint8Array | null (null → process every row as non-zero)
    // - startY, subHeight : row range to process
    const halfwidth = width >> 1;

    let outBase = startY * destPitch;
    let linBase = startY * srcPitch;
    let hinBase = startY * srcPitch + halfwidth; // hin starts at low-end + halfwidth (in S16 units)
    let maskIdx = startY;

    // ringbuffer-like state for the row : lp[ 8 ] + hp[ 8 ] with -base
    // offsets. We pack into Int32Array indexed 0..7 (lp) and 0..7 (hp), using
    // offset +1 (lp) and +2 (hp) as the "center" of the kernel window.
    // Pooled — fully re-seeded per row by the initial-fill below.
    const lp = _rowLp;
    const hp = _rowHp;

    for (let y = 0; y < subHeight; y++) {
        let next = 1; // S16-element step (was 2 bytes in SDK)

        let xoutIdx = outBase;
        let xlinIdx = linBase;
        let xhinIdx = hinBase;
        const linEnd = linBase + halfwidth; // boundary for clamp (was lin+width bytes = lin+halfwidth S16)

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

        const isNonZero = (rowMask === null) || rowMask[maskIdx];

        let x = (halfwidth < 8) ? 0 : ((halfwidth - 8) / 4) | 0;

        if (isNonZero) {
            // groups-of-4 unrolled
            while (x-- > 0) {
                // 4 pixel pairs (e1/o1, e2/o2, e3/o3, e4/o4) using lp[k+1+i]
                // and hp[k+2+i] with i=0..3
                let e1, o1, e2, o2, e3, o3, e4, o4;

                e1 = (lp[0 + 1] * 51674)
                    - (((lp[-1 + 1] + lp[1 + 1]) * 2667) | 0)
                    - (((hp[-2 + 2] + hp[1 + 2]) * 1563) | 0)
                    + (((hp[-1 + 2] + hp[0 + 2]) * 24733) | 0);
                o1 = (((lp[0 + 1] + lp[1 + 1]) * 27400) | 0)
                    - (((lp[-1 + 1] + lp[2 + 1]) * 4230) | 0)
                    - ((hp[0 + 2] * 55882) | 0)
                    - (((hp[-2 + 2] + hp[2 + 2]) * 2479) | 0)
                    + (((hp[-1 + 2] + hp[1 + 2]) * 7250) | 0);

                e2 = (lp[0 + 1 + 1] * 51674)
                    - (((lp[-1 + 1 + 1] + lp[1 + 1 + 1]) * 2667) | 0)
                    - (((hp[-2 + 2 + 1] + hp[1 + 2 + 1]) * 1563) | 0)
                    + (((hp[-1 + 2 + 1] + hp[0 + 2 + 1]) * 24733) | 0);
                o2 = (((lp[0 + 1 + 1] + lp[1 + 1 + 1]) * 27400) | 0)
                    - (((lp[-1 + 1 + 1] + lp[2 + 1 + 1]) * 4230) | 0)
                    - ((hp[0 + 2 + 1] * 55882) | 0)
                    - (((hp[-2 + 2 + 1] + hp[2 + 2 + 1]) * 2479) | 0)
                    + (((hp[-1 + 2 + 1] + hp[1 + 2 + 1]) * 7250) | 0);

                e3 = (lp[0 + 1 + 2] * 51674)
                    - (((lp[-1 + 1 + 2] + lp[1 + 1 + 2]) * 2667) | 0)
                    - (((hp[-2 + 2 + 2] + hp[1 + 2 + 2]) * 1563) | 0)
                    + (((hp[-1 + 2 + 2] + hp[0 + 2 + 2]) * 24733) | 0);
                o3 = (((lp[0 + 1 + 2] + lp[1 + 1 + 2]) * 27400) | 0)
                    - (((lp[-1 + 1 + 2] + lp[2 + 1 + 2]) * 4230) | 0)
                    - ((hp[0 + 2 + 2] * 55882) | 0)
                    - (((hp[-2 + 2 + 2] + hp[2 + 2 + 2]) * 2479) | 0)
                    + (((hp[-1 + 2 + 2] + hp[1 + 2 + 2]) * 7250) | 0);

                e4 = (lp[0 + 1 + 3] * 51674)
                    - (((lp[-1 + 1 + 3] + lp[1 + 1 + 3]) * 2667) | 0)
                    - (((hp[-2 + 2 + 3] + hp[1 + 2 + 3]) * 1563) | 0)
                    + (((hp[-1 + 2 + 3] + hp[0 + 2 + 3]) * 24733) | 0);
                o4 = (((lp[0 + 1 + 3] + lp[1 + 1 + 3]) * 27400) | 0)
                    - (((lp[-1 + 1 + 3] + lp[2 + 1 + 3]) * 4230) | 0)
                    - ((hp[0 + 2 + 3] * 55882) | 0)
                    - (((hp[-2 + 2 + 3] + hp[2 + 2 + 3]) * 2479) | 0)
                    + (((hp[-1 + 2 + 3] + hp[1 + 2 + 3]) * 7250) | 0);

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
            let xRem = (halfwidth < 8) ? halfwidth : ((halfwidth & 3) + 8);

            while (xRem-- > 0) {
                // Boundary check : xlinIdx === hinBase means we've reached
                // the end of the low band ; mirror.
                if (xlinIdx === linEnd) {
                    xlinIdx -= next;
                    xhinIdx -= next + next;
                    next = -next;
                }

                let e = (lp[0 + 1] * 51674)
                    - (((lp[-1 + 1] + lp[1 + 1]) * 2667) | 0)
                    - (((hp[-2 + 2] + hp[1 + 2]) * 1563) | 0)
                    + (((hp[-1 + 2] + hp[0 + 2]) * 24733) | 0);
                let o = (((lp[0 + 1] + lp[1 + 1]) * 27400) | 0)
                    - (((lp[-1 + 1] + lp[2 + 1]) * 4230) | 0)
                    - ((hp[0 + 2] * 55882) | 0)
                    - (((hp[-2 + 2] + hp[2 + 2]) * 2479) | 0)
                    + (((hp[-1 + 2] + hp[1 + 2]) * 7250) | 0);

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
                let e1, o1, e2, o2, e3, o3, e4, o4;
                e1 = (lp[0 + 1] * 51674) - (((lp[-1 + 1] + lp[1 + 1]) * 2667) | 0);
                o1 = (((lp[0 + 1] + lp[1 + 1]) * 27400) | 0) - (((lp[-1 + 1] + lp[2 + 1]) * 4230) | 0);
                e2 = (lp[0 + 1 + 1] * 51674) - (((lp[-1 + 1 + 1] + lp[1 + 1 + 1]) * 2667) | 0);
                o2 = (((lp[0 + 1 + 1] + lp[1 + 1 + 1]) * 27400) | 0) - (((lp[-1 + 1 + 1] + lp[2 + 1 + 1]) * 4230) | 0);
                e3 = (lp[0 + 1 + 2] * 51674) - (((lp[-1 + 1 + 2] + lp[1 + 1 + 2]) * 2667) | 0);
                o3 = (((lp[0 + 1 + 2] + lp[1 + 1 + 2]) * 27400) | 0) - (((lp[-1 + 1 + 2] + lp[2 + 1 + 2]) * 4230) | 0);
                e4 = (lp[0 + 1 + 3] * 51674) - (((lp[-1 + 1 + 3] + lp[1 + 1 + 3]) * 2667) | 0);
                o4 = (((lp[0 + 1 + 3] + lp[1 + 1 + 3]) * 27400) | 0) - (((lp[-1 + 1 + 3] + lp[2 + 1 + 3]) * 4230) | 0);

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

            let xRem = (halfwidth < 8) ? halfwidth : ((halfwidth & 3) + 8);
            while (xRem-- > 0) {
                if (xlinIdx === linEnd) {
                    xlinIdx -= 1;
                    next = -1;
                }

                let e = (lp[0 + 1] * 51674) - (((lp[-1 + 1] + lp[1 + 1]) * 2667) | 0);
                let o = (((lp[0 + 1] + lp[1 + 1]) * 27400) | 0) - (((lp[-1 + 1] + lp[2 + 1]) * 4230) | 0);

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

function iDWTcol(dest, destPitch, src, srcPitch, width, height, startY, subHeight) {
    // wavelet.c:836 — iDWTcol. Inverse-DWT along columns. Inner loop
    // unrolled to 4 cols at a time.
    const halfheight = subHeight >> 1;

    let outBase = startY * destPitch;
    let linBase = 0;          // S16 index, low band starts at col 0
    let hinBase = srcPitch;   // high band starts at col 0 of row 1 (since each "L" row alternates with "H" row in input layout)
    // `lendCol0` = col-0 end-of-buffer marker. Per-column boundary marker is
    // `lendCol0 + colsBase` (asm cite : granny2.dll @ 0x10008981 sets
    // [var_1a4h], 0x1000949a advances by 2 bytes = 1 S16 per col-iteration).
    // Without the +colsBase term, the strict-equality check at 0x100092f6
    // misses for any group beyond colsBase=0 and the reflect never fires.
    const lendCol0 = srcPitch * height;
    const ppitch2 = srcPitch * 2;   // we increment by 2 srcPitch per pair (one L row + one H row)

    if (startY) {
        linBase += ((startY / 2 | 0) - 1) * ppitch2;
        hinBase += ((startY / 2 | 0) - 2) * ppitch2;
    }

    // Process 4-col groups
    let colsBase = 0;
    const groupCount = (width / 4) | 0;

    // Ring buffer : lp[(16+4)][4], hp[(16+5)][4]. Each entry is a 4-tuple of
    // S16 columns. In JS we flatten to Int32Array of size 20*4 / 21*4.

    for (let g = 0; g < groupCount; g++) {
        let next = ppitch2;
        let youtBase = outBase + colsBase;
        let ylinBase = linBase + colsBase;
        let yhinBase = hinBase + colsBase;
        const lend = lendCol0 + colsBase;

        const lp = _colLp;   // pooled — see notes above iDWTrow
        const hp = _colHp;

        // Initial fill — startY ? 4L+5H : 3L+3H with boundary mirror
        if (startY) {
            // lp[-1+1..2+1] from ylin (4 rows)
            for (let k = 0; k < 4; k++) lp[(-1 + 1) * 4 + k] = src[ylinBase + k];
            ylinBase += next;
            for (let k = 0; k < 4; k++) lp[(0 + 1) * 4 + k] = src[ylinBase + k];
            ylinBase += next;
            for (let k = 0; k < 4; k++) lp[(1 + 1) * 4 + k] = src[ylinBase + k];
            ylinBase += next;
            for (let k = 0; k < 4; k++) lp[(2 + 1) * 4 + k] = src[ylinBase + k];
            ylinBase += next;

            for (let k = 0; k < 4; k++) hp[(-2 + 2) * 4 + k] = src[yhinBase + k];
            yhinBase += next;
            for (let k = 0; k < 4; k++) hp[(-1 + 2) * 4 + k] = src[yhinBase + k];
            yhinBase += next;
            for (let k = 0; k < 4; k++) hp[(0 + 2) * 4 + k] = src[yhinBase + k];
            yhinBase += next;
            for (let k = 0; k < 4; k++) hp[(1 + 2) * 4 + k] = src[yhinBase + k];
            yhinBase += next;
            for (let k = 0; k < 4; k++) hp[(2 + 2) * 4 + k] = src[yhinBase + k];
            yhinBase += next;
        } else {
            for (let k = 0; k < 4; k++) lp[(0 + 1) * 4 + k] = src[ylinBase + k];
            ylinBase += next;
            for (let k = 0; k < 4; k++) {
                const v = src[ylinBase + k];
                lp[(-1 + 1) * 4 + k] = v;
                lp[(1 + 1) * 4 + k] = v;
            }
            ylinBase += next;
            for (let k = 0; k < 4; k++) lp[(2 + 1) * 4 + k] = src[ylinBase + k];
            ylinBase += next;

            for (let k = 0; k < 4; k++) {
                const v = src[yhinBase + k];
                hp[(-1 + 2) * 4 + k] = v;
                hp[(0 + 2) * 4 + k] = v;
            }
            yhinBase += next;
            for (let k = 0; k < 4; k++) {
                const v = src[yhinBase + k];
                hp[(-2 + 2) * 4 + k] = v;
                hp[(1 + 2) * 4 + k] = v;
            }
            yhinBase += next;
            for (let k = 0; k < 4; k++) hp[(2 + 2) * 4 + k] = src[yhinBase + k];
            yhinBase += next;
        }

        // lp/hp index into a sliding window. SDK uses `lp` and `hp` pointers
        // that advance through the local array.
        let lpOff = 0; // base index into lp[]
        let hpOff = 0;

        for (let y = 0; y < halfheight; y++) {
            // boundary check
            if (ylinBase === lend) {
                ylinBase -= next;
                yhinBase -= next + next;
                next = -next;
            }

            // 4 columns at once
            const lpC = lpOff;
            const hpC = hpOff;
            let e1, e2, e3, e4, o1, o2, o3, o4;

            for (let k = 0; k < 4; k++) {
                const lpm1 = lp[(lpC + 0) * 4 + k];
                const lp0 = lp[(lpC + 1) * 4 + k];
                const lp1 = lp[(lpC + 2) * 4 + k];
                const lp2 = lp[(lpC + 3) * 4 + k];

                const hpm2 = hp[(hpC + 0) * 4 + k];
                const hpm1 = hp[(hpC + 1) * 4 + k];
                const hp0 = hp[(hpC + 2) * 4 + k];
                const hp1 = hp[(hpC + 3) * 4 + k];
                const hp2 = hp[(hpC + 4) * 4 + k];

                const e = (lp0 * 51674)
                    - (((lpm1 + lp1) * 2667) | 0)
                    - (((hpm2 + hp1) * 1563) | 0)
                    + (((hpm1 + hp0) * 24733) | 0);
                const o = (((lp0 + lp1) * 27400) | 0)
                    - (((lpm1 + lp2) * 4230) | 0)
                    - ((hp0 * 55882) | 0)
                    - (((hpm2 + hp2) * 2479) | 0)
                    + (((hpm1 + hp1) * 7250) | 0);

                const er = roundS16(e);
                const or = roundS16(o);

                if (k === 0) { e1 = er; o1 = or; }
                else if (k === 1) { e2 = er; o2 = or; }
                else if (k === 2) { e3 = er; o3 = or; }
                else { e4 = er; o4 = or; }
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
            if (lpOff + 3 === 16 + 4) {
                for (let k = 0; k < 4; k++) lp[0 * 4 + k] = lp[(lpOff + 1) * 4 + k];
                for (let k = 0; k < 4; k++) lp[1 * 4 + k] = lp[(lpOff + 2) * 4 + k];
                for (let k = 0; k < 4; k++) lp[2 * 4 + k] = lp[(lpOff + 3) * 4 + k];
                for (let k = 0; k < 4; k++) hp[0 * 4 + k] = hp[(hpOff + 1) * 4 + k];
                for (let k = 0; k < 4; k++) hp[1 * 4 + k] = hp[(hpOff + 2) * 4 + k];
                for (let k = 0; k < 4; k++) hp[2 * 4 + k] = hp[(hpOff + 3) * 4 + k];
                for (let k = 0; k < 4; k++) hp[3 * 4 + k] = hp[(hpOff + 4) * 4 + k];
                lpOff = 0;
                hpOff = 0;
            }

            // refill : lp[3][...] = ylin, hp[4][...] = yhin
            for (let k = 0; k < 4; k++) lp[(lpOff + 3) * 4 + k] = src[ylinBase + k];
            for (let k = 0; k < 4; k++) hp[(hpOff + 4) * 4 + k] = src[yhinBase + k];

            ylinBase += next;
            yhinBase += next;
        }

        colsBase += 4;
    }

    // Remaining columns (width & 3)
    const remCols = width & 3;
    for (let g = 0; g < remCols; g++) {
        let next = ppitch2;
        let youtBase = outBase + colsBase;
        let ylinBase = linBase + colsBase;
        let yhinBase = hinBase + colsBase;
        const lend = lendCol0 + colsBase;

        const lp = _remLp;   // pooled — re-seeded per remainder col below
        const hp = _remHp;

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

        for (let y = 0; y < halfheight; y++) {
            if (ylinBase === lend) {
                ylinBase -= next;
                yhinBase -= next + next;
                next = -next;
            }

            const e = (lp[0 + 1] * 51674)
                - (((lp[-1 + 1] + lp[1 + 1]) * 2667) | 0)
                - (((hp[-2 + 2] + hp[1 + 2]) * 1563) | 0)
                + (((hp[-1 + 2] + hp[0 + 2]) * 24733) | 0);
            const o = (((lp[0 + 1] + lp[1 + 1]) * 27400) | 0)
                - (((lp[-1 + 1] + lp[2 + 1]) * 4230) | 0)
                - ((hp[0 + 2] * 55882) | 0)
                - (((hp[-2 + 2] + hp[2 + 2]) * 2479) | 0)
                + (((hp[-1 + 2] + hp[1 + 2]) * 7250) | 0);

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

function iHarrrow(dest, destPitch, src, srcPitch, width, height, rowMask, startY, subHeight) {
    // wavelet.c:1175 — iHarrrow. Haar inverse row (small-width fallback).
    const halfwidth = width >> 1;

    let outBase = startY * destPitch;
    let linBase = startY * srcPitch;
    let hinBase = startY * srcPitch + halfwidth;
    let maskIdx = startY;

    for (let y = 0; y < subHeight; y++) {
        let xoutIdx = outBase;
        let xlinIdx = linBase;
        let xhinIdx = hinBase;

        const isNonZero = (rowMask === null) || rowMask[maskIdx];

        if (isNonZero) {
            for (let x = 0; x < halfwidth; x++) {
                const lv = src[xlinIdx];
                const hv = src[xhinIdx];
                let e = (lv * 2 + hv) | 0;
                let o = (lv * 2 - hv) | 0;
                e = ((e + (1 ^ (e >> 31))) / 2) | 0;
                o = ((o + (1 ^ (o >> 31))) / 2) | 0;
                dest[xoutIdx + 0] = e;
                dest[xoutIdx + 1] = o;
                xoutIdx += 2;
                xlinIdx += 1;
                xhinIdx += 1;
            }
        } else {
            for (let x = 0; x < halfwidth; x++) {
                const e = src[xlinIdx];
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

function iHarrcol(dest, destPitch, src, srcPitch, width, height, startY, subHeight) {
    // wavelet.c:1261 — iHarrcol.
    const halfheight = subHeight >> 1;

    let outBase = startY * destPitch;
    let linBase = 0;
    let hinBase = srcPitch;
    const ppitch2 = srcPitch * 2;

    if (startY) {
        linBase += (startY / 2 | 0) * ppitch2;
        hinBase += (startY / 2 | 0) * ppitch2;
    }

    for (let x = 0; x < width; x++) {
        let youtBase = outBase + x;
        let ylinBase = linBase + x;
        let yhinBase = hinBase + x;

        for (let y = 0; y < halfheight; y++) {
            const lv = src[ylinBase];
            const hv = src[yhinBase];
            let e = (lv * 2 + hv) | 0;
            let o = (lv * 2 - hv) | 0;
            e = ((e + (1 ^ (e >> 31))) / 2) | 0;
            o = ((o + (1 ^ (o >> 31))) / 2) | 0;
            dest[youtBase] = e;
            dest[youtBase + destPitch] = o;
            youtBase += destPitch + destPitch;
            ylinBase += ppitch2;
            yhinBase += ppitch2;
        }
    }
}

function iDWT2D(output, pitch, width, height, rowMask, temp) {
    // wavelet.c:1328 — iDWT2D dispatcher.
    // - output, temp : Int16Array, same length as the plane
    // - pitch : S16-index pitch
    const rowFn = (width >= SMALLEST_DWT_ROW) ? iDWTrow : iHarrrow;
    const colFn = (height >= SMALLEST_DWT_COL) ? iDWTcol : iHarrcol;

    let ry = (height <= (FLIPSIZE + 4 + 4)) ? height : (FLIPSIZE + 4);
    let rh = height - ry;
    let cy = 0;
    let ch = height;

    rowFn(temp, pitch, output, pitch, width, height, rowMask, 0, ry);

    do {
        let next = (ch <= (FLIPSIZE + 4)) ? ch : FLIPSIZE;
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

export { iDWT2D };
