// igc-arith.js — IGC arith range-coder + adaptive model (pure-JS oracle).
//
// Extracted verbatim from GrannyTextureIGC.js so the opt-in WASM build can
// dispatch the arith kernel to a WebAssembly module via the ./igc-kernels.js
// seam, while this pure-JS implementation stays the mandatory fallback +
// byte-exact oracle. NEVER rewritten — the WASM port in src/wasm/kernels.ts
// must match it byte-for-byte.
//
// Public surface (the 8 seam fns planeDecode consumes) : arithBitOpen,
// arithOpen, arithDecompress, arithWasEscaped, arithSetDecompressed,
// arithBitsGetValue, arithBitsGet, arithBitsRemove.

// The arithBits multiply sites use plain f64 math instead of BigInt. This is
// provably exact : `range ≤ 2^31` (31-bit coder) and `scale < 0x4000`
// (ARITH_RESCALE_TRIGGER), so every product `range·scale < 2^45 < 2^53` is
// representable exactly ; the floor is exact because the nearest-integer
// distance of the quotient (≥ 1/scale ≥ 2^-14) dwarfs the f64 rounding error
// (≤ 2^-22). Set IGC_ARITH_VERIFY=1 to cross-check every f64 result against
// the BigInt reference call-by-call (see scripts/replay-arith-selfcheck.mjs).
// The flag is a module const : when off, the guarded branches are a single
// correctly-predicted no-op per call.
const IGC_ARITH_VERIFY =
    typeof process !== 'undefined' && !!process.env && process.env.IGC_ARITH_VERIFY === '1';

let __arithCall = 0;
function __arithVerify(site, fast, ref) {
    __arithCall++;
    if (ref !== fast) {
        throw new Error(
            `[IGC_ARITH_VERIFY] ${site} divergence at call #${__arithCall}: ` +
            `f64=${fast} bigint=${ref}`);
    }
}

// ============================================================================
// ArithBit — RAD ArithBits range coder (DLL-faithful, 31-bit precision).
//
// State : { buf, ptr, accum, bitsLeft, high, low, target }
//
// Three independent 31-bit fields — high (upper bound), low (lower bound),
// target (encoded value, always in [low, high]). This matches the DLL ab
// struct layout exactly (offsets +0x10/+0x14/+0x18). `accum` + `bitsLeft`
// buffer the next bits to be pulled from `buf` during renorm.
//
// Asm cite — init : `granny2.dll @ 0x10006650` (= fcn.10006650).
//            primitives : `fcn.1000e020` (GetValue), `fcn.1000ddc0` (Remove +
//            renorm). Confirmed S3.14 via direct disasm + manual computation
//            against kg7-tex0 trace (`abT = 0x08BF0093` for input bytes
//            `88 7E 80 64` — bit-reverse of bottom 31 bits of LE DWORD).
//
// Bytes are bit-reversed when shifted into target. The DLL uses two lookup
// tables : a 4-bit reverse at `0x1002a4b4` and a 3-bit reverse at
// `0x1002a4c4`. We materialize BR4 (16 entries) inline and a BR8 (256
// entries) precomputed for byte-mode renorm.

const BR4 = new Uint8Array([0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15]);
const BR3 = new Uint8Array([0, 4, 2, 6, 1, 5, 3, 7]);
const BR8 = (() => {
    const t = new Uint8Array(256);
    for (let i = 0; i < 256; i++) {
        t[i] = (BR4[i & 0xF] << 4) | BR4[(i >>> 4) & 0xF];
    }
    return t;
})();

function arithBitOpen(buf, offset) {
    // fcn.10006650 @ 0x10006683-0x10006746 — read first LE DWORD, strip top
    // bit into accumulator (bitsLeft = 1), bit-reverse bottom 31 bits into
    // target. Initial high = 0x7FFFFFFF, low = 0.
    const b0 = buf[offset] | 0, b1 = buf[offset + 1] | 0,
          b2 = buf[offset + 2] | 0, b3 = buf[offset + 3] | 0;
    const dword = ((b0 | (b1 << 8) | (b2 << 16) | (b3 << 24)) >>> 0);
    const topBit = (dword >>> 31) & 1;
    const lo31 = dword & 0x7FFFFFFF;

    // Bit-reverse via 7 BR4 lookups + 1 BR3 lookup (asm @ 0x100066b5-0x10006746).
    // Order from LSB up : BR4[bits 0-3], BR4[bits 4-7], ..., BR4[bits 24-27],
    // BR3[bits 28-30] — each placed at the top of the accumulating result.
    let t = BR4[(lo31 >>> 4) & 0xF];
    t |= BR4[lo31 & 0xF] << 4;
    t = ((t << 4) >>> 0) | BR4[(lo31 >>> 8) & 0xF];
    t = ((t << 4) >>> 0) | BR4[(lo31 >>> 12) & 0xF];
    t = ((t << 4) >>> 0) | BR4[(lo31 >>> 16) & 0xF];
    t = ((t << 4) >>> 0) | BR4[(lo31 >>> 20) & 0xF];
    t = ((t << 4) >>> 0) | BR4[(lo31 >>> 24) & 0xF];
    t = ((t << 3) >>> 0) | BR3[(lo31 >>> 28) & 7];

    return {
        buf,
        ptr: (offset + 4) | 0,
        accum: topBit,
        bitsLeft: 1,
        high: 0x7FFFFFFF,
        low: 0,
        target: t >>> 0,
    };
}

// Pull `n` bits from input (n in {1, 4, 8}) and return them as an int,
// already bit-reversed (matching the DLL's BR4/BR8 lookup pattern).
// Refills `accum` from `buf[ptr..ptr+4]` when bitsLeft drops below n.
function arithPullBits(ab, n) {
    let bitsLeft = ab.bitsLeft | 0;
    let accum = ab.accum >>> 0;
    let raw;
    if (bitsLeft < n) {
        const buf = ab.buf;
        const p = ab.ptr | 0;
        const refill = ((buf[p] | (buf[p + 1] << 8) | (buf[p + 2] << 16) | (buf[p + 3] << 24)) >>> 0);
        // asm fcn.1000ddc0 @ 0x1000de61-67 : eax = (DWORD << bitsLeft) | accum ;
        //                                    new_accum = DWORD >> (n - bitsLeft).
        // (n - bitsLeft) is always in [1, n] here ; no JS shift-by-32 edge case.
        const eax = (bitsLeft === 0) ? refill : (((refill << bitsLeft) | accum) >>> 0);
        ab.accum = (refill >>> (n - bitsLeft)) >>> 0;
        ab.bitsLeft = bitsLeft + 32 - n;
        ab.ptr = (p + 4) | 0;
        raw = eax & ((1 << n) - 1);
    } else {
        raw = accum & ((1 << n) - 1);
        ab.accum = (accum >>> n) >>> 0;
        ab.bitsLeft = bitsLeft - n;
    }
    return (n === 8) ? BR8[raw] : (n === 4) ? BR4[raw] : raw;
}

// Renorm — fcn.1000ddc0 @ 0x1000de15-0x1000dff5. Runs after every state
// update in arithBitsRemove. Three loops (byte / nibble / bit) shift out
// matched prefix bits, plus an E3 (underflow) carry loop. All three of
// (high, low, target) get shifted in lock-step ; target gets new bits from
// the input stream (bit-reversed via BR4/BR8).
function arithRenorm(ab) {
    let h = ab.high >>> 0;
    let l = ab.low >>> 0;
    let t = ab.target >>> 0;

    // Renorm structure mirrors fcn.1000ddc0 @ 0x1000de15-0x1000df8b. Sequential
    // mode checks — NOT an outer loop. The asm doesn't go back to byte mode
    // after nibble mode ; it tests bit 30 once at the end of nibble mode and
    // either enters bit mode or jumps to E3.
    let xh = (l ^ h) >>> 0;

    if ((xh & 0x40000000) === 0) {
        if ((xh & 0x7F800000) === 0) {
            do {
                h = (((h << 8) | 0xff) >>> 0) & 0x7FFFFFFF;
                l = ((l << 8) >>> 0) & 0x7FFFFFFF;
                const b = arithPullBits(ab, 8);
                t = (((t << 8) | b) >>> 0) & 0x7FFFFFFF;
                xh = (l ^ h) >>> 0;
            } while ((xh & 0x7F800000) === 0);
        }
        if ((xh & 0x78000000) === 0) {
            h = (((h << 4) | 0xf) >>> 0) & 0x7FFFFFFF;
            l = ((l << 4) >>> 0) & 0x7FFFFFFF;
            const n = arithPullBits(ab, 4);
            t = (((t << 4) | n) >>> 0) & 0x7FFFFFFF;
            xh = (l ^ h) >>> 0;
        }
        if ((xh & 0x40000000) === 0) {
            do {
                h = (((h << 1) | 1) >>> 0) & 0x7FFFFFFF;
                l = ((l << 1) >>> 0) & 0x7FFFFFFF;
                const bit = arithPullBits(ab, 1);
                t = (((t << 1) | bit) >>> 0) & 0x7FFFFFFF;
                xh = (l ^ h) >>> 0;
            } while ((xh & 0x40000000) === 0);
        }
    }

    // E3 (underflow) carry — fcn.1000ddc0 @ 0x1000df8d-0x1000dff5.
    //   Entry condition (L168-169) : low<29>=1   (else skip to clamp).
    //   Loop top (L170-171, 0x1000df95) : exit when high<29>=1.
    //   Body (L172+, 0x1000df9d) : clear bits 29-30 of LOW, shift LOW left, shift
    //   HIGH left then OR 0x40000001, flip bit 29 of TARGET, pull 1 bit into TARGET.
    //   Bottom check (L201-202, 0x1000dfed) : loop again iff new low<29>=1.
    if ((l & 0x20000000) !== 0) {
        while (true) {
            if ((h & 0x20000000) !== 0) break;
            l = (((l & 0x1FFFFFFF) << 1) >>> 0) & 0x7FFFFFFF;
            h = (((h << 1) | 0x40000001) >>> 0) & 0x7FFFFFFF;
            t = (t ^ 0x20000000) >>> 0;
            const bit = arithPullBits(ab, 1);
            t = (((t << 1) | bit) >>> 0) & 0x7FFFFFFF;
            if ((l & 0x20000000) === 0) break;
        }
    }

    ab.high = h;
    ab.low = l;
    ab.target = t;
}

function arithBitsGet(ab, scale) {
    // fcn.1000e6f0 @ 0x1000e730-0x1000e744 — used by arithDecompress for the
    // offset-into-cumCounts compute.
    //   offset = ((target - low + 1) * scale - 1) / (high - low + 1)
    // The asm at 0x1000e73a-3d does `sub eax, 1 ; sbb edx, 0` on the 64-bit
    // product (tNorm * scale) before dividing by range — same -1 pattern as
    // arithBitsGetValue. The previous comment claimed "without the trailing
    // -1 from GetValue" but the asm shows the -1 IS present (S3.17 fix).
    const range = (ab.high - ab.low + 1) >>> 0;
    const tNorm = (ab.target - ab.low + 1) >>> 0;
    // f64-exact : tNorm·scale < 2^45, quotient floored via `| 0` (positive,
    // < 2^31). See the note at IGC_ARITH_VERIFY.
    const v = ((tNorm * scale - 1) / range) | 0;
    if (IGC_ARITH_VERIFY) __arithVerify('Get', v, Number((BigInt(tNorm) * BigInt(scale) - 1n) / BigInt(range)) | 0);
    return (v >= scale) ? (scale - 1) : v;
}

function arithBitsRemove(ab, lo, count, scale) {
    // fcn.1000ddc0 @ 0x1000ddff-0x1000de15 — adjust (high, low) then renorm.
    //   new_low_offset  = floor(range * lo / scale)
    //   new_high_offset = floor(range * (lo+count) / scale) - 1
    //   new_low_dll  = low + new_low_offset
    //   new_high_dll = low + new_high_offset
    const range = (ab.high - ab.low + 1) >>> 0;
    // f64-exact : range·(lo+count) < 2^45 ; quotient can reach ~2^31 so we
    // floor with Math.floor (not `| 0`, which would wrap past int32).
    const num1 = Math.floor((range * (lo + count)) / scale);
    const num2 = Math.floor((range * lo) / scale);
    if (IGC_ARITH_VERIFY) {
        const sBig = BigInt(scale);
        __arithVerify('Remove.num1', num1, Number((BigInt(range) * BigInt(lo + count)) / sBig));
        __arithVerify('Remove.num2', num2, Number((BigInt(range) * BigInt(lo)) / sBig));
    }
    const oldLow = ab.low >>> 0;
    ab.high = (oldLow + num1 - 1) >>> 0;
    ab.low = (oldLow + num2) >>> 0;
    arithRenorm(ab);
}

function arithBitsGetValue(ab, scale) {
    // fcn.1000e020 — value = ((target - low + 1) * scale - 1) / (high - low + 1)
    // then arithBitsRemove(ab, value, 1, scale).
    const range = (ab.high - ab.low + 1) >>> 0;
    const tNorm = (ab.target - ab.low + 1) >>> 0;
    // f64-exact — see the note at IGC_ARITH_VERIFY.
    let v = ((tNorm * scale - 1) / range) | 0;
    if (IGC_ARITH_VERIFY) __arithVerify('GetValue', v, Number((BigInt(tNorm) * BigInt(scale) - 1n) / BigInt(range)) | 0);
    if (v >= scale) v = scale - 1;
    arithBitsRemove(ab, v, 1, scale);
    return v;
}

function arithBitsGetBits(ab, bits, scale) {
    // Same as Get with scale = 2^bits ; the actual returned offset is
    // truncated to `scale` (caller may pass scale != 2^bits).
    const range = (ab.high - ab.low + 1) >>> 0;
    const tNorm = (ab.target - ab.low + 1) >>> 0;
    const num = BigInt(tNorm) << BigInt(bits);
    const v = Number(num / BigInt(range)) | 0;
    return (v >= scale) ? (scale - 1) : v;
}

function arithBitsGetBitsValue(ab, bits, scale) {
    // Same as GetValue with scale = 2^bits.
    const range = (ab.high - ab.low + 1) >>> 0;
    const tNorm = (ab.target - ab.low + 1) >>> 0;
    const num = (BigInt(tNorm) << BigInt(bits)) - 1n;
    let v = Number(num / BigInt(range)) | 0;
    if (v >= scale) v = scale - 1;
    arithBitsRemove(ab, v, 1, scale);
    return v;
}

// Legacy compat — `arithDecRenorm` was a no-op-able SDK construct ; the DLL
// renorms inside `arithBitsRemove`. Stubbed so call sites don't crash if any
// linger from earlier ports.
function arithDecRenorm(_ab) {}

// ============================================================================
// Arith — DLL-faithful adaptive arithmetic modeller (decode side).
//
// Port of granny2.dll Arith family — NOT the leaked SDK `radarith.c`. The
// two diverge structurally :
//
//   - SDK uses a linear `summed_counts[]` (cumulative sum normalized to
//     NORM_COUNT) + a `table_walks[]` of pre-computed binary-search steps.
//     update_counts() rebuilds summed_counts only periodically (every
//     update_tot bytes), so the model "forgets" recent symbols between
//     rebuilds. Escape handling has a "previously sent index" branch when
//     `singlesLength != summedLength`.
//
//   - DLL uses an inline 16-bucket `cumCounts[16]` cumulative-sum table
//     stored at struct +0x00..+0x1f, maintained ACTIVELY during a 4-level
//     binary search through the buckets (each level's `add dword [edx+N],
//     0x10001` bumps two adjacent cumCount entries by 1). No table_walks,
//     no update_tot, no summed/singles distinction. Escape always means
//     "new symbol" — there's no "previously sent" branch.
//
// Cumulative-sum invariant : `cumCounts[k]` = running cumulative count of
// single_counts entries up to slot `(k+1)*bucketSize - 1` (or
// uniqueValues-1 for k=15). Rescale triggers when cumCounts[15] >= 0x4000.
//
// Functions ported here :
//
//   - `arithInitBands`     (fcn.1000e130) — picks shiftDepth / bucketSize /
//     bandBoundary that minimize the leftover of a 16-bucket split of
//     `uniqueValues + 1` slots.
//   - `arithUpdate`        (fcn.1000e1c0) — leaf path (idx >= bandBoundary :
//     bump cumCounts[15] + single_counts[idx]) and cached path (cumCounts
//     pair-bumps via 8-case fallthrough switch).
//   - `arithSearch`        (fcn.1000e7f0) — 4-level binary search through
//     cumCounts[0..15] with inline `cumCounts[bucket..15] += 1` maintenance,
//     followed by a linear scan through single_counts within the bucket.
//   - `arithBitsGetOffsetDLL` / `arithBitsRemoveDLL` — bitstream offset /
//     remove primitives matching the asm formula
//     `offset = ((low + 1) * total - 1) / range` (fcn.1000e6f0 @
//     0x1000e734-0x1000e744) and the (range * lo / total) → low/range
//     update (fcn.1000ddc0). BigInt arithmetic — `low * total` overflows
//     U32 when total approaches NORM_COUNT and range stays > 0x800000.
//   - `arithRescale`       (fcn.1000ec30) — halves single_counts, drops
//     count<=1 slots via swap-with-last, halves single_counts[0] separately,
//     accumulates per-bucket scratch, then re-emits cumCounts via cumulative
//     sum. Reorder step places max at start of bucket containing
//     singlesLength.
//   - `arithReEmitCumFromScratch` (fcn.1000e690) — escape-adjust
//     (if singlesLength != uniqueCount AND single_counts[0] == 0,
//     restore escape = 2) + cumulative-sum emit.
//   - `arithOpen`          (fcn.1000e0c0) — Arith_open. The DLL's init
//     prime call `fcn.1000e1c0(ctx, 0, 0x30003)` is reproduced via
//     `arithUpdate(a, 0, 0x30003)` — net effect on init :
//       cumCounts[0..15] = 3 (all entries equal, satisfies cum-sum
//       invariant for {single_counts[0]=3, rest=0})
//       single_counts[0] = 3
//
// Plane-0 byte-exact-vs-DLL preservation : the `arithBit` range coder
// (L48-148 above) is structurally identical between SDK and DLL — we
// keep that. Only the model layer is replaced.
//
// Reference : /tmp/igc-probe/dll-arith-struct.md + /tmp/igc-probe/fcn-*.disasm.

const ARITH_RESCALE_TRIGGER = 0x4000;

// (S3.14) `arithBitsGetOffsetDLL` + `arithBitsRemoveDLL` removed. The DLL
// formulas now live in `arithBitsGet` and `arithBitsRemove` directly (using
// the 3-field {high, low, target} ab struct). arithDecompress consumes them
// the same way the DLL does.

// fcn.1000e130(ctx, uniqueValues + 1) — picks shiftDepth that minimizes
// leftover of 16-bucket split. Sets bucketSize = 1<<shiftDepth ;
// bandBoundary = 15 * bucketSize.

function arithInitBands(a, uniqueValuesPlus1) {
    if (uniqueValuesPlus1 < 6) {
        // Tiny case (asm @ 0x1000e138 jae fallback) : all updates take
        // the leaf path because bandBoundary = 0.
        a.bucketSize = 0;
        a.shiftDepth = 15;
        a.bandBoundary = 0;
        return;
    }
    let bestShift = 0;
    let bestLeftover = 0xffffffff >>> 0;
    for (let cl = 0; cl < 16; cl++) {
        const bucketSize = 1 << cl;
        // nbBuckets = ceil(uniqueValuesPlus1 / bucketSize), clamped to 16
        // (asm @ 0x1000e169-179).
        let nbBuckets = (((bucketSize + uniqueValuesPlus1 - 1) / bucketSize) | 0) >>> 0;
        if (nbBuckets > 16) nbBuckets = 16;
        // leftover = uniqueValuesPlus1 - (nbBuckets - 1) * bucketSize
        // (asm @ 0x1000e17c-183). When `leftover < bucketSize` (last bucket is
        // partial), saturate the score UP to bucketSize. The loop minimizes
        // score, so a small partial-bucket counts as "bad" (bucketSize-sized
        // penalty). Asm @ 0x1000e183 : `cmp eax,esi ; jae skip ; mov eax,esi`
        // — jae taken when leftover >= bucketSize means SKIP the mov, keeping
        // leftover ; the mov only fires when leftover < bucketSize.
        let leftover = (uniqueValuesPlus1 - (nbBuckets - 1) * bucketSize) >>> 0;
        if (leftover < bucketSize) leftover = bucketSize >>> 0;
        if (leftover < bestLeftover) {
            bestShift = cl;
            bestLeftover = leftover;
        }
        if (bucketSize > uniqueValuesPlus1) break;  // asm @ 0x1000e191
    }
    a.bucketSize = (1 << bestShift) & 0xffff;
    a.shiftDepth = bestShift & 0xff;
    a.bandBoundary = (15 * a.bucketSize) & 0xffff;
}

// fcn.1000e1c0(ctx, idx, magic) — cumCounts/single_counts updater.
//
//   Leaf path (idx >= bandBoundary, asm @ 0x1000e1d3-1e1) :
//     cumCounts[15]     += magic_lo  (cx)
//     single_counts[idx] += magic_lo  (cx)
//
//   Cached path (idx < bandBoundary, asm @ 0x1000e1e2-222) :
//     d = idx >> shiftDepth
//     if (d & 1) { cumCounts[d] += magic_lo ; d++ }   (asm @ 0x1000e1ed-1f6)
//     d >>= 1
//     if (d > 7) goto default                          (asm @ 0x1000e1f9-1fc)
//     ; switch cases d..7 FALL THROUGH (no break statements — compiler
//     ; emitted consecutive `add dword [eax+N], ecx` with NO jumps between
//     ; them, so case d's body runs case d, then case d+1's body, etc.) :
//     for k in d..7 :
//       cumCounts[2k]     += magic_lo  (low U16 of DWORD add)
//       cumCounts[2k+1]   += magic_hi  (high U16 of DWORD add)
//     ; default fallthrough (asm @ 0x1000e21c) :
//     single_counts[idx]  += magic_lo

function arithUpdate(a, idx, magic) {
    const lo = magic & 0xffff;
    const hi = (magic >>> 16) & 0xffff;
    if (idx >= a.bandBoundary) {
        a.cumCounts[15]      = (a.cumCounts[15]      + lo) & 0xffff;
        a.singleCounts[idx]  = (a.singleCounts[idx]  + lo) & 0xffff;
        return;
    }
    let d = idx >>> a.shiftDepth;
    if (d & 1) {
        a.cumCounts[d] = (a.cumCounts[d] + lo) & 0xffff;
        d++;
    }
    d >>>= 1;
    if (d <= 7) {
        // DWORD-level pair add — the asm fall-through chain is
        //   case k: add dword [ctx + 4k], magic
        // which adds magic as a 32-bit unsigned word, so a low-half overflow
        // carries +1 into the high half. Independent 16-bit adds miss that
        // carry and end up with cum[odd] one short whenever (cum[even] + lo16)
        // crosses 0x10000 — the exact pattern that escape-remove magic
        // (lo16 = -sc0, hi16 = -sc0 - 1) triggers when cum[0] >= sc0.
        const umagic = magic >>> 0;
        for (let k = d; k <= 7; k++) {
            const j = (k * 2) | 0;
            const oldDword = (a.cumCounts[j + 1] * 0x10000 + a.cumCounts[j]) >>> 0;
            const newDword = (oldDword + umagic) >>> 0;
            a.cumCounts[j]     = newDword & 0xffff;
            a.cumCounts[j + 1] = (newDword >>> 16) & 0xffff;
        }
    }
    a.singleCounts[idx] = (a.singleCounts[idx] + lo) & 0xffff;
}

// fcn.1000e7f0(ctx, offset, *outLo) — 4-level binary search through
// cumCounts[0..15], with inline cumulative-sum bumps. After narrowing to
// a bucket, linear refinement through single_counts within the bucket.
//
// Cumulative-sum maintenance : for ALL leaf paths, the asm's various
// `add dword [edx+N], 0x10001` chains net out to "cumCounts[bucket..15]
// each += 1". The descent COMPARISONS read OLD cumCounts (no bumps applied
// during descent), so my JS port reads cc[] for the 4 levels BEFORE doing
// the trailing bump loop.
//
// Returns [refinedIdx, slotLo]. The caller (arithDecompress) then calls
// `arithBitsRemoveDLL(ab, slotLo, single_counts[refinedIdx], oldTotal)`.

function arithSearch(a, offset) {
    const cc = a.cumCounts;
    let bucket = 0;
    let lo = 0;

    // Level 0 : pivot = cumCounts[7] (asm @ 0x1000e7fa-e805)
    if (offset >= cc[7]) { lo = cc[7]; bucket = 8; }
    // Level 1 : pivot = cumCounts[bucket + 3]    (cc[3] or cc[11])
    if (offset >= cc[bucket + 3]) { lo = cc[bucket + 3]; bucket += 4; }
    // Level 2 : pivot = cumCounts[bucket + 1]    (cc[1], cc[5], cc[9], cc[13])
    if (offset >= cc[bucket + 1]) { lo = cc[bucket + 1]; bucket += 2; }
    // Level 3 : pivot = cumCounts[bucket]        (cc[0], cc[2], ..., cc[14])
    if (offset >= cc[bucket]) { lo = cc[bucket]; bucket += 1; }

    // Inline cumulative-sum bumps : cumCounts[bucket..15] each += 1.
    // (Each asm leaf path executes a different sequence of DWORD adds that
    //  all net out to this. Verified by tracing buckets 0, 1, 4, 8, 15.)
    for (let i = bucket; i < 16; i++) {
        cc[i] = (cc[i] + 1) & 0xffff;
    }

    // Linear refinement within bucket. baseIdx = bucket * bucketSize ;
    // for bucket 15 (or shiftDepth=15 degenerate), the range extends up
    // to uniqueCount. The slot is found when `offset < lo + sc[idx]`.
    // Bound by uniqueCount + 1 — bucket 15 with uniqueCount < bandBoundary
    // would otherwise step beyond the allocated single_counts buffer.
    // Use bucketSize directly — for the tiny case (uC < 5) shiftDepth=15
    // but bucketSize=0, so `bucket << shiftDepth` would overflow into the
    // millions ; the asm uses the stored bucketSize from ctx[+0x28].
    const baseIdx = (bucket * a.bucketSize) | 0;
    let idx = baseIdx;
    let cumLo = lo;
    const sc = a.singleCounts;
    const end = Math.min(sc.length, (a.uniqueCount + 1) | 0);
    while (idx < end) {
        const high = (cumLo + sc[idx]) & 0xffff;
        if (offset < high) {
            return [idx, cumLo];
        }
        cumLo = high;
        idx++;
    }
    // Fallback (shouldn't occur in valid bitstream). Clamp idx to within
    // single_counts AND uniqueCount so callers see a usable slot index.
    const fallback = Math.min(Math.max(idx - 1, 0), Math.max(a.uniqueCount, 0));
    return [fallback, cumLo];
}

// fcn.1000e690(ctx, scratch) — escape-adjust + cumulative-sum re-emit.
// Called from rescale (fcn.1000ec30 @ 0x1000ee16).

function arithReEmitCumFromScratch(a, scratch) {
    // Escape adjust : if not all symbols seen AND escape count is 0,
    // restore escape = 2 (so subsequent picks can still escape).
    // (asm @ 0x1000e6b1-0x1000e6c8)
    if (a.singlesLength !== a.uniqueCount && a.singleCounts[0] === 0) {
        a.singleCounts[0] = 2;
        const bucket0 = (0 >= a.bandBoundary) ? 15 : 0;
        scratch[bucket0] = (scratch[bucket0] + 2) >>> 0;
    }
    // Cumulative-sum emit : cumCounts[0] = scratch[0] ;
    //                       cumCounts[k] = cumCounts[k-1] + scratch[k]
    // (asm @ 0x1000e6cd-0x1000e6e7, U16 arithmetic).
    let cum = 0;
    for (let k = 0; k < 16; k++) {
        cum = (cum + scratch[k]) & 0xffff;
        a.cumCounts[k] = cum;
    }
}

// fcn.1000ec30 — rescale. Triggered when cumCounts[15] >= 0x4000.
//
// Steps :
//   1. Re-init band layout : arithInitBands(a, singlesLength + 1).
//   2. scratch[16] = 0  (asm @ 0x1000ec51-5a, 64-byte memset).
//   3. single_counts[0] >>= 1 ; scratch[bucket(0)] = single_counts[0].
//   4. Main loop i = 1..singlesLength :
//       a. While single_counts[i] <= 1 :
//          - If i >= singlesLength : single_counts[i] = 0,
//            singlesLength--, break.
//          - Else : swap-with-last (single_counts[i] = sc[singlesLength],
//            zero sc[singlesLength], values[i] = values[singlesLength],
//            singlesLength--), do NOT advance i.
//       b. single_counts[i] >>= 1
//          Track (max, maxPos)
//          scratch[bucket(i)] += single_counts[i]
//       c. i++
//   5. Reorder : if max > 0 AND maxPos != target, swap into target slot.
//      target = (singlesLength >> shiftDepth) << shiftDepth  (= bucket
//      start for singlesLength), or bandBoundary if singlesLength >=
//      bandBoundary. Special case : if target = 0, set target = 1
//      (preserve escape slot at index 0).
//      Update scratch[bucket(target)] += delta,
//             scratch[bucket(maxPos)] -= delta.
//   6. Call arithReEmitCumFromScratch (= fcn.1000e690).

function arithRescale(a) {
    // 1. Re-init bands.
    arithInitBands(a, (a.singlesLength + 1) | 0);

    // 2. Scratch.
    const scratch = new Uint32Array(16);

    // 3. Halve single_counts[0], seed scratch.
    a.singleCounts[0] = (a.singleCounts[0] >>> 1) & 0xffff;
    const bucket0 = (0 >= a.bandBoundary) ? 15 : 0;
    scratch[bucket0] = a.singleCounts[0] >>> 0;

    // 4. Main loop.
    let max = 0;
    let maxPos = 0;
    let i = 1;
    while (i <= a.singlesLength) {
        // 4a. Swap-with-last loop.
        while (a.singleCounts[i] <= 1) {
            if (i >= a.singlesLength) {
                // At last position (asm @ 0x1000ed37-3e).
                a.singleCounts[i] = 0;
                a.singlesLength = (a.singlesLength - 1) | 0;
                // Exit both loops — the outer `while (i <= singlesLength)`
                // will also break since i > new singlesLength.
                break;
            }
            // Standard swap (asm @ 0x1000ecb4-d2).
            a.singleCounts[i] = a.singleCounts[a.singlesLength];
            a.singleCounts[a.singlesLength] = 0;
            a.values[i] = a.values[a.singlesLength];
            a.singlesLength = (a.singlesLength - 1) | 0;
        }
        if (i > a.singlesLength) break;
        // 4b. Halve + accumulate.
        a.singleCounts[i] = (a.singleCounts[i] >>> 1) & 0xffff;
        const sc = a.singleCounts[i];
        if (sc > max) {
            max = sc;
            maxPos = i;
        }
        const bucket = (i >= a.bandBoundary) ? 15 : (i >>> a.shiftDepth);
        scratch[bucket] = (scratch[bucket] + sc) >>> 0;
        i++;
    }

    // 5. Reorder max into bucket-start of singlesLength's bucket.
    if (max > 0) {
        let target;
        if (a.singlesLength >= a.bandBoundary) {
            target = a.bandBoundary;
        } else {
            target = (a.singlesLength >>> a.shiftDepth) << a.shiftDepth;
        }
        // Special case (asm @ 0x1000ed7c-7e) : if target = 0, set to 1 to
        // preserve the escape slot at index 0.
        if (target === 0) target = 1;

        if (maxPos !== target) {
            const oldScTarget = a.singleCounts[target] >>> 0;
            const scMaxPos = a.singleCounts[maxPos] >>> 0;
            // Signed delta (one of these can be < the other).
            const delta = (scMaxPos - oldScTarget) | 0;

            const bucketTarget = (target >= a.bandBoundary) ? 15 : (target >>> a.shiftDepth);
            const bucketMaxPos = (maxPos >= a.bandBoundary) ? 15 : (maxPos >>> a.shiftDepth);

            // U32 wrap-around for the scratch updates — the asm uses
            // 32-bit `add dword [ecx], edi` / `add dword [ecx], edx` with
            // signed delta (asm @ 0x1000edcc, 0x1000edf1).
            scratch[bucketTarget] = (scratch[bucketTarget] + delta) >>> 0;
            scratch[bucketMaxPos] = (scratch[bucketMaxPos] - delta) >>> 0;

            // Swap single_counts + values.
            a.singleCounts[target] = scMaxPos;
            a.singleCounts[maxPos] = oldScTarget;
            const valTarget = a.values[target];
            a.values[target] = a.values[maxPos];
            a.values[maxPos] = valTarget;
        }
    }

    // 6. Re-emit cumCounts.
    arithReEmitCumFromScratch(a, scratch);
}

// fcn.1000e0c0(ptr, NULL, uniqueValues, maxValue) — Arith_open (decode side).
//
// Layout :
//   ptr[+0x00..+0x1f] : cumCounts[16] (U16 each)
//   ptr[+0x20]        : singlesLength (U16)
//   ptr[+0x24]        : bandBoundary (U16)
//   ptr[+0x26]        : shiftDepth (byte)
//   ptr[+0x28]        : bucketSize (U16)
//   ptr[+0x2c]        : uniqueCount (U16)
//   ptr[+0x30]        : values* (after inline single_counts)
//   ptr[+0x34]        : compress_temp_buf* (NULL for decompress)
//   ptr[+0x38..]      : inline single_counts[countsSize], then values[countsSize]
//
// JS port splits inline tables into two typed arrays.
//
// `maxValue` is unused on decompress (no compress_temp_buf allocation
// needed) — kept in the signature for backward compat with planeDecode
// callers L1274+.

// Untrusted-input cap on the arith alphabet size. `uniqueValues` (= `max +
// 1`, `max` a 16-bit bitstream field) sizes two Uint16Array tables ; the
// callers (igc-plane.js:174 decodeLow, :247 createDecompContexts) feed it
// attacker-controlled values up to 65536. A plane-delta alphabet is small,
// so 8192 is generous ; the fixed lit(64)/zero(256) opens pass unaffected.
// O(1) guard, once per model open — not an inner-loop check.
export const IGC_MAX_ALPHABET = 8192;

function arithOpen(_maxValue, uniqueValues) {
    if (uniqueValues > IGC_MAX_ALPHABET) {
        throw new Error(`igc arith alphabet ${uniqueValues} exceeds cap ${IGC_MAX_ALPHABET}`);
    }
    const countsSize = ((uniqueValues + 5) & ~3) | 0;
    const a = {
        cumCounts: new Uint16Array(16),
        singleCounts: new Uint16Array(countsSize),
        values: new Uint16Array(countsSize),
        singlesLength: 0,
        bandBoundary: 0,
        shiftDepth: 0,
        bucketSize: 0,
        uniqueCount: uniqueValues,
    };
    arithInitBands(a, (uniqueValues + 1) | 0);
    // Init prime call : fcn.1000e1c0(ctx, 0, 0x30003)  (asm @ 0x1000e11c).
    // Net effect with idx=0 in cached path : cumCounts[0..15] = 3 each
    // (case 0 fall-through bumps all 16 by lo=hi=3 via DWORD adds), and
    // single_counts[0] = 3.
    arithUpdate(a, 0, 0x30003);
    return a;
}

// Sentinel for escape returns. Symbol values in the decoder are 16-bit
// (max_value < 65536) so the DLL's escape return (a pointer to values[idx])
// would fall outside the U16 raw-value range. In JS we tag escapes with an
// object so `typeof return === 'object'` means "escape, write me a value".

function arithMakeEscape(slot) {
    return { __escape: true, slot };
}

function arithWasEscaped(ret) {
    return typeof ret === 'object';
}

function arithSetDecompressed(escapeRet, value, a) {
    a.values[escapeRet.slot] = value;
}

// fcn.1000e6f0(ctx, ab) — Arith_decompress.
//
//   1. If cumCounts[15] >= 0x4000 : rescale (asm @ 0x1000e6f8-e702).
//   2. Compute offset = ((low + 1) * total - 1) / range
//      (asm @ 0x1000e734-744 ; total = OLD cumCounts[15] read at 0x1000e71a).
//   3. Search : [refinedIdx, slotLo] = arithSearch(a, offset).
//      Side-effect : cumCounts[bucket(refinedIdx)..15] each += 1 (so after
//      this, cumCounts[15] = total + 1).
//   4. arithBitsRemoveDLL(ab, slotLo, single_counts[refinedIdx], total)
//      (asm @ 0x1000e75a-76f ; scale = cumCounts[15] - 1 = total since
//       cumCounts[15] was just bumped to total + 1).
//   5. single_counts[refinedIdx] += 1  (asm @ 0x1000e772).
//   6. If refinedIdx > 0 : return values[refinedIdx] (asm @ 0x1000e7d7-e2).
//   7. Else (escape) :
//      a. singlesLength += 1   (asm @ 0x1000e77c).
//      b. arithUpdate(a, singlesLength, 0x20002)  (asm @ 0x1000e792).
//      c. If singlesLength == uniqueCount : remove escape from cumCounts[0]
//         via arithUpdate(a, 0, ((-sc0 - 1) << 16) | (-sc0 & 0xffff))
//         (asm @ 0x1000e7a4-bd).
//      d. Return arithMakeEscape(singlesLength) — caller writes the new
//         symbol via arithSetDecompressed.

function arithDecompress(a, ab) {
    if (a.cumCounts[15] >= ARITH_RESCALE_TRIGGER) {
        arithRescale(a);
    }

    // Trace replay assertion — gated on globalThis.__igcTrace. Set
    // globalThis.__igcStrictThrow=true to throw on first divergence ;
    // otherwise collects into __igcDivergences and continues. Used by
    // /tmp/igc-replay.mjs to validate against the macOS-wine ARITH trace.
    if (globalThis.__igcTrace) {
        const idx = globalThis.__igcTraceIdx | 0;
        const exp = globalThis.__igcTrace[idx];
        if (exp) {
            const errs = [];
            const actH = ab.high >>> 0, actL = ab.low >>> 0, actT = ab.target >>> 0;
            const expH = parseInt(exp.abH, 16), expL = parseInt(exp.abL, 16), expT = parseInt(exp.abT, 16);
            if (actH !== expH) errs.push(`abH 0x${actH.toString(16)} vs 0x${expH.toString(16)}`);
            if (actL !== expL) errs.push(`abL 0x${actL.toString(16)} vs 0x${expL.toString(16)}`);
            if (actT !== expT) errs.push(`abT 0x${actT.toString(16)} vs 0x${expT.toString(16)}`);
            for (let k = 0; k < 16; k++) {
                if (a.cumCounts[k] !== exp.cum[k]) {
                    errs.push(`cum[${k}] ${a.cumCounts[k]} vs ${exp.cum[k]}`);
                }
            }
            if (a.singlesLength !== exp.sL) errs.push(`sL ${a.singlesLength} vs ${exp.sL}`);
            if (a.bandBoundary !== exp.bB) errs.push(`bB ${a.bandBoundary} vs ${exp.bB}`);
            if (a.shiftDepth !== exp.sD) errs.push(`sD ${a.shiftDepth} vs ${exp.sD}`);
            if (a.uniqueCount !== exp.uC) errs.push(`uC ${a.uniqueCount} vs ${exp.uC}`);
            if (errs.length > 0) {
                if (!globalThis.__igcDivergences) globalThis.__igcDivergences = [];
                globalThis.__igcDivergences.push({ call: exp.call, errs });
                if (globalThis.__igcDivergences.length === 1 || globalThis.__igcStrictThrow) {
                    const e = new Error(`__igcTrace: call ${exp.call} divergence:\n  ${errs.join('\n  ')}`);
                    /** @type {any} */ (e).callIdx = exp.call;
                    if (globalThis.__igcStrictThrow) throw e;
                    process.stderr.write(`!! First divergence at call ${exp.call}:\n  ${errs.join('\n  ')}\n`);
                }
            }
        }
        globalThis.__igcTraceIdx = idx + 1;
    }

    const total = a.cumCounts[15];
    const offset = arithBitsGet(ab, total);
    const [refinedIdx, slotLo] = arithSearch(a, offset);
    const slotCount = a.singleCounts[refinedIdx];

    arithBitsRemove(ab, slotLo, slotCount, total);
    a.singleCounts[refinedIdx] = (a.singleCounts[refinedIdx] + 1) & 0xffff;

    if (refinedIdx !== 0) {
        return a.values[refinedIdx];
    }

    // Escape : new symbol.
    a.singlesLength = (a.singlesLength + 1) & 0xffff;
    arithUpdate(a, a.singlesLength, 0x20002);

    if (a.singlesLength === a.uniqueCount) {
        // Remove escape contribution from cumCounts[0]. The magic =
        //   ((-sc0 - 1) << 16) | (-sc0 & 0xffff)
        // matches the asm at 0x1000e7a4-b7 exactly. With sc0 = current
        // single_counts[0], the DWORD add of magic to cumCounts[0..1]
        // subtracts (sc0+1) from cumCounts[1] and sc0 from cumCounts[0],
        // effectively zeroing the escape weight.
        const sc0 = a.singleCounts[0] | 0;
        const negSc0 = (-sc0) & 0xffff;
        const magicHi = ((-sc0 - 1) & 0xffff) << 16;
        const magic = (magicHi | negSc0) >>> 0;
        arithUpdate(a, 0, magic);
    }

    return arithMakeEscape(a.singlesLength);
}

export {
    arithBitOpen,
    arithOpen,
    arithDecompress,
    arithWasEscaped,
    arithSetDecompressed,
    arithBitsGetValue,
    arithBitsGet,
    arithBitsRemove,
};
