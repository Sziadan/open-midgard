// igc-plane.js — pure-JS plane decoder (the byte-exact oracle + mandatory
// fallback for the opt-in WASM build).
//
// Extracted verbatim from GrannyTextureIGC.js : the adaptive-arithmetic +
// 4-level sub-band traversal that turns an IGC plane bitstream into a
// wavelet-transformed S16 plane. Like ./igc-yuv.js and ./igc-arith.js, this
// module is never rewritten — the WASM build swaps the seam (./igc-kernels.js)
// so `planeDecode` dispatches to the WebAssembly module, and this stays the
// oracle the differential gate compares against + the fallback when the wasm
// module has not instantiated.
//
// Leaked-SDK ref : `encode.c:532` (read_escapes), `encode.c:1416` (fill_rect),
//                  `encode.c:1440` (decode_low), `encode.c:1576`
//                  (create_decomp_contexts), `encode.c:1611` (decode_high_1),
//                  `encode.c:1884` (plane_decode).
// Asm cite : `granny2.dll @ 0x100045b0` (`FromBinkTC`).
//
// Arith is imported from the seam (./igc-kernels.js), NOT ./igc-arith.js
// directly : in the arith differential-gate bundle the seam is swapped for the
// tee, so this oracle's per-symbol arith calls stay interceptable. In the
// default build the seam re-exports the pure-JS arith ; in the wasm build it
// dispatches to wasm (only reached here on the fallback path).
import {
    arithBitOpen,
    arithOpen,
    arithDecompress,
    arithWasEscaped,
    arithSetDecompressed,
    arithBitsGetValue,
    arithBitsGet,
    arithBitsRemove,
} from './igc-kernels.js';

// ============================================================================
// VarBits — 32-bit LE bit reader, used for the "uncompressed" stream
// (sign bits, length escapes, raw 16-bit values).
// Leaked-SDK ref : `varbits.h` macros `VarBitsOpen`, `VarBitsGet1LE`,
// `VarBitsGetLE`.
//
// State : { buf, cur, bits, bitlen }
// - `buf` : Uint8Array source
// - `cur` : byte index into buf for next U32 read
// - `bits` : U32 cache of bits read but not yet consumed (low end has the
//            unconsumed bits)
// - `bitlen` : number of valid bits remaining in `bits`

function varBitsOpen(buf, offset) {
    return { buf, cur: offset | 0, bits: 0, bitlen: 0 };
}

function varBitsReadU32LE(buf, offset) {
    // Read a 32-bit little-endian unsigned int from buf at offset.
    return (
        (buf[offset]) |
        (buf[offset + 1] << 8) |
        (buf[offset + 2] << 16) |
        (buf[offset + 3] << 24)
    ) >>> 0;
}

function varBitsGet1(vb) {
    // varbits.h — VarBitsGet1LE. Reads one bit.
    let bitlen = vb.bitlen;
    let bits;
    if (bitlen === 0) {
        const i = varBitsReadU32LE(vb.buf, vb.cur);
        vb.cur = (vb.cur + 4) | 0;
        bits = i >>> 1;
        bitlen = 31;
        // The "i" value's bit 0 is the bit being returned.
        vb.bits = bits;
        vb.bitlen = bitlen;
        return i & 1;
    }
    bits = vb.bits;
    vb.bits = bits >>> 1;
    vb.bitlen = bitlen - 1;
    return bits & 1;
}

function varBitsGet(vb, len) {
    // varbits.h — VarBitsGetLE. Reads `len` bits.
    const mask = (len === 32) ? 0xffffffff : ((1 << len) - 1) >>> 0;
    if (vb.bitlen < len) {
        const nb = varBitsReadU32LE(vb.buf, vb.cur);
        vb.cur = (vb.cur + 4) | 0;
        // result = (vb.bits | (nb << vb.bitlen)) & mask
        const merged = ((vb.bits | (nb << vb.bitlen)) >>> 0) & mask;
        vb.bits = (nb >>> (len - vb.bitlen)) >>> 0;
        vb.bitlen = vb.bitlen + 32 - len;
        return merged >>> 0;
    }
    const result = vb.bits & mask;
    vb.bits = (vb.bits >>> len) >>> 0;
    vb.bitlen -= len;
    return result >>> 0;
}

// Sign-extend a 16-bit value to signed.
function s16(v) {
    return (v & 0x8000) ? (v | 0xffff0000) | 0 : v | 0;
}

// ============================================================================
// Plane decoders.

const MIN_ZERO_LENGTH = 3;
const LIT_LENGTH_BITS = 6;
const ZERO_LENGTH_BITS = 8;
const LIT_LENGTH_LIMIT = (1 << LIT_LENGTH_BITS) - 1;
const ZERO_LENGTH_LIMIT = (1 << ZERO_LENGTH_BITS) - 1;
const EXTRA_LENGTHS = 4;
const EXTRA_LIT_LENGTHS = [128, 256, 512, 1024];
const EXTRA_ZERO_LENGTHS = [512, 1024, 2048, 3072];

function getBitLevel(value) {
    // varbits.h — getbitlevel. SDK caps at 15 for value >= 16384, and
    // operates on U32 (so wraparound on large values must keep the value
    // unsigned, not flip to negative). Use `>>> 0` to coerce to U32.
    let n = value >>> 0;
    if (n === 0) return 0;
    if (n >= 16384) return 15;
    let r = 0;
    while (n > 0) {
        r++;
        n >>>= 1;
    }
    return r;
}

function fillRect(outp, outOffset, pitch, width, height, val) {
    // encode.c:1416 — fill_rect
    const yadj = pitch - width;
    let o = outOffset;
    for (let h = 0; h < height; h++) {
        for (let w = 0; w < width; w++) {
            outp[o++] = val;
        }
        o += yadj;
    }
}

function readEscapes(ab, mask, count) {
    // encode.c:532 — read_escapes. Decodes a length-`count` byte array
    // (0 or 1 per slot) representing zero-row RLE.
    const zeros = arithBitsGetValue(ab, count + 1);

    for (let i = 0; i < count; i++) {
        if (arithBitsGet(ab, count) >= zeros) {
            mask[i] = 1;
            arithBitsRemove(ab, zeros, count - zeros, count);
        } else {
            mask[i] = 0;
            arithBitsRemove(ab, 0, zeros, count);
        }
    }
}

function decodeLow(ab, vb, outp, outOffset, pixelPitch, encWidth, encHeight) {
    // encode.c:1440 — decode_low. Low-pass plane, no per-plane prediction
    // (just pixel-to-left/above prediction within this plane).
    // Returns nothing ; populates outp at outOffset.

    // See if all bytes are a single value
    if (varBitsGet1(vb)) {
        const v = varBitsGet(vb, 16);
        fillRect(outp, outOffset, pixelPitch, encWidth, encHeight, s16(v));
        return;
    }

    const max = varBitsGet(vb, 16);
    const num = max + 1;

    const a = arithOpen(max, num);

    const yadj = pixelPitch - encWidth;

    // First pixel raw
    let prev = varBitsGet(vb, 16);
    outp[outOffset++] = s16(prev);
    prev = s16(prev);

    // First row : predict from left pixel
    for (let w = 0; w < encWidth - 1; w++) {
        let cur = arithDecompress(a, ab);

        if (arithWasEscaped(cur)) {
            const escaped = arithBitsGetValue(ab, num);
            arithSetDecompressed(cur, escaped, a);
            cur = escaped;
        }

        if (cur) {
            // sign bit
            const v = -varBitsGet1(vb) | 0;
            cur = (cur ^ v) - v;
        }

        prev = (cur + prev) | 0;
        outp[outOffset++] = prev;
    }

    // Rest of rows : predict from average of left + top
    for (let h = 0; h < encHeight - 1; h++) {
        outOffset += yadj;
        let from = outOffset - pixelPitch;

        // First pixel of row : predict from top
        let cur = arithDecompress(a, ab);
        if (arithWasEscaped(cur)) {
            const escaped = arithBitsGetValue(ab, num);
            arithSetDecompressed(cur, escaped, a);
            cur = escaped;
        }
        if (cur) {
            const v = -varBitsGet1(vb) | 0;
            cur = (cur ^ v) - v;
        }

        prev = (cur + outp[from]) | 0;
        outp[outOffset++] = prev;
        ++from;

        for (let w = 0; w < encWidth - 1; w++) {
            cur = arithDecompress(a, ab);
            if (arithWasEscaped(cur)) {
                const escaped = arithBitsGetValue(ab, num);
                arithSetDecompressed(cur, escaped, a);
                cur = escaped;
            }
            if (cur) {
                const v = -varBitsGet1(vb) | 0;
                cur = (cur ^ v) - v;
            }
            prev = (cur + (((prev + outp[from]) / 2) | 0)) | 0;
            outp[outOffset++] = prev;
            ++from;
        }
    }
}

function createDecompContexts(max, num, numl) {
    // encode.c:1576 — create_decomp_contexts. Allocate per-context arith
    // tables. Returns { contexts: Array<arith>, lits: arith, zeros: arith }.
    const contexts = new Array(numl);
    for (let i = 0; i < numl; i++) {
        contexts[i] = arithOpen(max, num);
    }
    const lits = arithOpen(LIT_LENGTH_LIMIT, LIT_LENGTH_LIMIT + 1);
    const zeros = arithOpen(ZERO_LENGTH_LIMIT, ZERO_LENGTH_LIMIT + 1);
    return { contexts, lits, zeros };
}

function radabs(v) { return v < 0 ? -v : v; }

function decodeHigh1(ab, vb, outp, outOffset, pixelPitch, encWidth, encHeight) {
    // encode.c:1611 — decode_high_1. High-pass plane with order-1
    // prediction.
    const qlevel = varBitsGet(vb, 16);

    if (varBitsGet1(vb)) {
        const v = s16(varBitsGet(vb, 16));
        fillRect(outp, outOffset, pixelPitch, encWidth, encHeight, (v * qlevel) | 0);
        return;
    }

    const max = varBitsGet(vb, 16);
    const num = max + 1;
    let numl = max * qlevel;
    numl = getBitLevel(numl) + 1;

    const { contexts: a, lits, zeros } = createDecompContexts(max, num, numl);

    const yadj = pixelPitch - encWidth;
    let h = encHeight;

    // First pixel raw
    let above = arithBitsGetValue(ab, num);
    if (above) {
        const v = -varBitsGet1(vb) | 0;
        above = (above ^ v) - v;
        above = (above * qlevel) | 0;
    }

    outp[outOffset] = above;
    let aboveLeft = above;
    let prev = above;
    let fromOffset = outOffset;
    ++outOffset;

    if (encWidth === 1) {
        // jump to after_first ; mirror via labeled-block trick
        return decodeHigh1AfterFirst(ab, vb, outp, outOffset, pixelPitch, encWidth, encHeight,
            a, lits, zeros, num, numl, qlevel, yadj, h, above, aboveLeft, prev, fromOffset);
    }

    let w = encWidth - 1;
    let litLen = 0;
    let zeroLen = 0;
    let idleIter = 0;

    outer: for (;;) {
        // Read lit_len
        let litLenRet = arithDecompress(lits, ab);
        if (arithWasEscaped(litLenRet)) {
            const escaped = varBitsGet(vb, LIT_LENGTH_BITS);
            arithSetDecompressed(litLenRet, escaped, lits);
            litLen = escaped;
        } else {
            litLen = litLenRet;
        }
        if (litLen >= (LIT_LENGTH_LIMIT - EXTRA_LENGTHS + 1)) {
            litLen = EXTRA_LIT_LENGTHS[litLen - (LIT_LENGTH_LIMIT - EXTRA_LENGTHS + 1)];
        }

        // Read zero_len
        let zeroLenRet = arithDecompress(zeros, ab);
        if (arithWasEscaped(zeroLenRet)) {
            const escaped = varBitsGet(vb, ZERO_LENGTH_BITS);
            arithSetDecompressed(zeroLenRet, escaped, zeros);
            zeroLen = escaped;
        } else {
            zeroLen = zeroLenRet;
        }

        if (zeroLen >= (ZERO_LENGTH_LIMIT - EXTRA_LENGTHS + 1)) {
            zeroLen = EXTRA_ZERO_LENGTHS[zeroLen - (ZERO_LENGTH_LIMIT - EXTRA_LENGTHS + 1)] + MIN_ZERO_LENGTH - 1;
        } else if (zeroLen) {
            zeroLen += MIN_ZERO_LENGTH - 1;
        }

        // Anti-hang : both lengths zero = neither inner while progresses h.
        // granny2.dll itself spins on the same off-corpus bitstream
        // (`1_attack.gr2` textures). 64 consecutive idle iters = throw.
        if (litLen === 0 && zeroLen === 0) {
            if (++idleIter > 64) {
                throw new Error(`decodeHigh1: stuck at h=${h} w=${w} after ${idleIter} consecutive arith reads with litLen=0/zeroLen=0 — bitstream likely off-corpus (granny2.dll hangs on the same input). Encoding=${encWidth}x${encHeight}.`);
            }
        } else {
            idleIter = 0;
        }

        // Decode literals
        while (litLen > 0) {
            if (w <= 1) {
                if (w) {
                    // Predict from prev + above_left + above (weighted).
                    // SDK computes `prev * 2` as S32 with wraparound on
                    // overflow (encode.c:1746) ; Math.imul mirrors that.
                    const __sum3 = (radabs(Math.imul(prev, 2)) + radabs(aboveLeft) + radabs(above)) >>> 0;
                    let context = (__sum3 / 4) >>> 0;
                    context = getBitLevel(context);
                    let cur = arithDecompress(a[context], ab);
                    if (arithWasEscaped(cur)) {
                        const escaped = arithBitsGetValue(ab, num);
                        arithSetDecompressed(cur, escaped, a[context]);
                        cur = escaped;
                    }
                    if (cur) {
                        const v = -varBitsGet1(vb) | 0;
                        cur = (cur ^ v) - v;
                        cur = (cur * qlevel) | 0;
                    }
                    outp[outOffset++] = cur;
                    --litLen;
                }

                // after_first label
                if (--h === 0) return;
                w = encWidth;
                outOffset += yadj;
                fromOffset = outOffset - pixelPitch;
                above = outp[fromOffset++];
                aboveLeft = above;
                prev = above;
            } else {
                const aboveRight = outp[fromOffset];
                const __sum4 = (radabs(prev) + radabs(aboveLeft) + radabs(above) + radabs(aboveRight)) >>> 0;
                let context = (__sum4 / 4) >>> 0;
                context = getBitLevel(context);
                let cur = arithDecompress(a[context], ab);
                if (arithWasEscaped(cur)) {
                    const escaped = arithBitsGetValue(ab, num);
                    arithSetDecompressed(cur, escaped, a[context]);
                    cur = escaped;
                }
                if (cur) {
                    const v = -varBitsGet1(vb) | 0;
                    cur = (cur ^ v) - v;
                    cur = (cur * qlevel) | 0;
                }
                outp[outOffset] = cur;

                aboveLeft = above;
                above = aboveRight;
                prev = cur;

                ++outOffset;
                ++fromOffset;
                --w;
                --litLen;
            }
        }

        // Decode zero runs
        while (zeroLen > 0) {
            if (zeroLen >= w) {
                zeroLen -= w;
                fromOffset += w;
                while (w-- > 0) {
                    outp[outOffset++] = 0;
                }
                if (--h === 0) return;
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

function decodeHigh1AfterFirst(ab, vb, outp, outOffset, pixelPitch, encWidth, encHeight,
    a, lits, zeros, num, numl, qlevel, yadj, hIn, aboveIn, aboveLeftIn, prevIn, fromOffsetIn) {
    // Continuation when encWidth === 1 — start at the "after_first" label.
    let h = hIn;
    let above = aboveIn;
    let aboveLeft = aboveLeftIn;
    let prev = prevIn;
    let fromOffset = fromOffsetIn;

    // after_first label
    if (--h === 0) return;
    let w = encWidth;
    outOffset += yadj;
    fromOffset = outOffset - pixelPitch;
    above = outp[fromOffset++];
    aboveLeft = above;
    prev = above;

    let litLen = 0;
    let zeroLen = 0;
    let idleIter = 0;

    for (;;) {
        let litLenRet = arithDecompress(lits, ab);
        if (arithWasEscaped(litLenRet)) {
            const escaped = varBitsGet(vb, LIT_LENGTH_BITS);
            arithSetDecompressed(litLenRet, escaped, lits);
            litLen = escaped;
        } else {
            litLen = litLenRet;
        }
        if (litLen >= (LIT_LENGTH_LIMIT - EXTRA_LENGTHS + 1)) {
            litLen = EXTRA_LIT_LENGTHS[litLen - (LIT_LENGTH_LIMIT - EXTRA_LENGTHS + 1)];
        }

        let zeroLenRet = arithDecompress(zeros, ab);
        if (arithWasEscaped(zeroLenRet)) {
            const escaped = varBitsGet(vb, ZERO_LENGTH_BITS);
            arithSetDecompressed(zeroLenRet, escaped, zeros);
            zeroLen = escaped;
        } else {
            zeroLen = zeroLenRet;
        }
        if (zeroLen >= (ZERO_LENGTH_LIMIT - EXTRA_LENGTHS + 1)) {
            zeroLen = EXTRA_ZERO_LENGTHS[zeroLen - (ZERO_LENGTH_LIMIT - EXTRA_LENGTHS + 1)] + MIN_ZERO_LENGTH - 1;
        } else if (zeroLen) {
            zeroLen += MIN_ZERO_LENGTH - 1;
        }

        if (litLen === 0 && zeroLen === 0) {
            if (++idleIter > 64) {
                throw new Error(`decodeHigh1AfterFirst: stuck at h=${h} w=${w} after ${idleIter} consecutive arith reads with litLen=0/zeroLen=0 — bitstream likely off-corpus. Encoding=${encWidth}x${encHeight}.`);
            }
        } else {
            idleIter = 0;
        }

        while (litLen > 0) {
            if (w <= 1) {
                if (w) {
                    let context = ((radabs(prev * 2) + radabs(aboveLeft) + radabs(above)) | 0) / 4 | 0;
                    context = getBitLevel(context);
                    let cur = arithDecompress(a[context], ab);
                    if (arithWasEscaped(cur)) {
                        const escaped = arithBitsGetValue(ab, num);
                        arithSetDecompressed(cur, escaped, a[context]);
                        cur = escaped;
                    }
                    if (cur) {
                        const v = -varBitsGet1(vb) | 0;
                        cur = (cur ^ v) - v;
                        cur = (cur * qlevel) | 0;
                    }
                    outp[outOffset++] = cur;
                    --litLen;
                }
                if (--h === 0) return;
                w = encWidth;
                outOffset += yadj;
                fromOffset = outOffset - pixelPitch;
                above = outp[fromOffset++];
                aboveLeft = above;
                prev = above;
            } else {
                const aboveRight = outp[fromOffset];
                const __sum4 = (radabs(prev) + radabs(aboveLeft) + radabs(above) + radabs(aboveRight)) >>> 0;
                let context = (__sum4 / 4) >>> 0;
                context = getBitLevel(context);
                let cur = arithDecompress(a[context], ab);
                if (arithWasEscaped(cur)) {
                    const escaped = arithBitsGetValue(ab, num);
                    arithSetDecompressed(cur, escaped, a[context]);
                    cur = escaped;
                }
                if (cur) {
                    const v = -varBitsGet1(vb) | 0;
                    cur = (cur ^ v) - v;
                    cur = (cur * qlevel) | 0;
                }
                outp[outOffset] = cur;
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
                if (--h === 0) return;
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

/**
 * Decode the variable-length encoded sub-band representation of one plane
 * into the S16 `output` plane. Leaked-SDK `encode.c:1884` (`plane_decode`).
 *
 * @param {Uint8Array} buf — the IGC bitstream.
 * @param {number} srcOffset — byte offset of this plane within `buf`.
 * @param {Int16Array} output — destination S16 plane.
 * @param {number} outOffset — element offset into `output`.
 * @param {number} width — plane width in pixels.
 * @param {number} height — plane height in pixels.
 * @param {Uint8Array | null} rowMask — zero-row RLE mask (plane 0 only), else null.
 * @returns {number} bytes consumed from `buf`.
 */
export function planeDecode(buf, srcOffset, output, outOffset, width, height, rowMask) {
    // encode.c:1884 — plane_decode. Decodes the variable-length encoded
    // sub-band representation into the S16 `output` plane.
    // Layout per IGC-FORMAT.md § 3 :
    //   buf[srcOffset..+4]   : arith stream length (U32 LE)
    //   buf[srcOffset+4..+8] : varbits stream length (U32 LE)
    //   buf[srcOffset+8..]   : arith stream (length = arith_len)
    //   buf[after that..]    : varbits stream (length = varbits_len)
    // Returns bytes consumed from buf.

    const arithLen = varBitsReadU32LE(buf, srcOffset);
    const varbitsStart = srcOffset + 8 + arithLen;

    const ab = arithBitOpen(buf, srcOffset + 8);
    const vb = varBitsOpen(buf, varbitsStart);

    // 4-level sub-band traversal.
    //
    // SDK signature note : `decode_low`/`decode_high_1` take `pixel_pitch`
    // in **S16 elements** (not bytes). `outp` is `S16 *`, so `outp += yadj`
    // advances by S16 elements. Confirmed by encode.c:1444 :
    //   yadj = pixel_pitch - (S32)enc_width;
    //   outp += yadj;   // outp is S16*, so adds yadj S16 elements
    //
    // SDK calls (encode.c:1908-1924) pass :
    //   level 3 : pitch = width * 16 S16 elements (= 16 buffer-row stride
    //                                                between sub-band rows)
    //   level 2 : pitch = width * 8
    //   level 1 : pitch = width * 4
    //   level 0 : pitch = width * 2
    //
    // Sub-band offsets within the plane (all in S16 elements) :
    //   LL3 : 0
    //   HL3 : W/16          LH3 : W*8           HH3 : W/16 + W*8
    //   HL2 : W/8           LH2 : W*4           HH2 : W/8  + W*4
    //   HL1 : W/4           LH1 : W*2           HH1 : W/4  + W*2
    //   HL0 : W/2           LH0 : W             HH0 : W/2  + W

    // Level 3 (innermost, W/16 x H/16)
    decodeLow  (ab, vb, output, outOffset,                          width * 16, width >> 4, height >> 4);
    decodeHigh1(ab, vb, output, outOffset + (width >> 4),           width * 16, width >> 4, height >> 4);
    decodeHigh1(ab, vb, output, outOffset + (width * 8),            width * 16, width >> 4, height >> 4);
    decodeHigh1(ab, vb, output, outOffset + (width >> 4) + (width * 8), width * 16, width >> 4, height >> 4);

    // Level 2 (W/8 x H/8)
    decodeHigh1(ab, vb, output, outOffset + (width >> 3),           width * 8,  width >> 3, height >> 3);
    decodeHigh1(ab, vb, output, outOffset + (width * 4),            width * 8,  width >> 3, height >> 3);
    decodeHigh1(ab, vb, output, outOffset + (width >> 3) + (width * 4), width * 8, width >> 3, height >> 3);

    // Level 1 (W/4 x H/4)
    decodeHigh1(ab, vb, output, outOffset + (width >> 2),           width * 4,  width >> 2, height >> 2);
    decodeHigh1(ab, vb, output, outOffset + (width * 2),            width * 4,  width >> 2, height >> 2);
    decodeHigh1(ab, vb, output, outOffset + (width >> 2) + (width * 2), width * 4, width >> 2, height >> 2);

    // Level 0 (W/2 x H/2)
    decodeHigh1(ab, vb, output, outOffset + (width >> 1),           width * 2,  width >> 1, height >> 1);
    decodeHigh1(ab, vb, output, outOffset + width,                  width * 2,  width >> 1, height >> 1);
    decodeHigh1(ab, vb, output, outOffset + (width >> 1) + width,   width * 2,  width >> 1, height >> 1);

    if (rowMask) {
        readEscapes(ab, rowMask, height);
    }

    const varbitsLen = varBitsReadU32LE(buf, srcOffset + 4);
    return arithLen + varbitsLen + 8;
}
