// igc-yuv.js — the pure-JS YUV→RGBA kernel : the byte-exact oracle and the
// mandatory fallback for the WASM build.
//
// Moved verbatim out of GrannyTextureIGC.js so the codec can dispatch this
// kernel through a swappable seam (./igc-kernels.js). This module is never
// rewritten or removed — the WASM build always keeps it as the fallback the
// dual-dispatch drops to when the wasm module hasn't instantiated.

const PLANE_VALUE_OFFSET = 0;

/**
 * YUV-ish → RGB plane inversion (asm cite `granny2.dll @ 0x10009a30`).
 * Internal helper — exported so the test suite can validate it independently
 * on synthetic planes. Not part of the public package surface.
 *
 * @param {Int16Array | Int32Array | ReadonlyArray<number>} yp — Y plane, length `width * height`.
 * @param {Int16Array | Int32Array | ReadonlyArray<number>} up — U plane, length `width * height`.
 * @param {Int16Array | Int32Array | ReadonlyArray<number>} vp — V plane, length `width * height`.
 * @param {Int16Array | Int32Array | ReadonlyArray<number>} ap — A plane, length `width * height` (values already 0..255).
 * @param {number} width
 * @param {number} height
 * @returns {Uint8Array} RGBA8888 of length `width * height * 4`.
 */
export function yuvToRGB(yp, up, vp, ap, width, height) {
    const count = width * height;
    const out = new Uint8Array(count * 4);
    let o = 0;
    for (let i = 0; i < count; i++) {
        let r = up[i] + PLANE_VALUE_OFFSET;
        let g = yp[i] + PLANE_VALUE_OFFSET;
        let b = vp[i] + PLANE_VALUE_OFFSET;
        let a = ap[i];

        // Round-toward-zero integer divide by 4. Asm cite :
        // `granny2.dll @ 0x10009aa0-0x10009aac` — `cdq ; and edx,3 ; add eax,edx ;
        // sar eax,2` is the canonical signed-divide-by-4 idiom, NOT a plain
        // `sar` (which would be floor-toward -∞). JS `>>` is arith right shift
        // and diverges by 1 on negative (r+b) that aren't multiples of 4.
        g -= ((r + b) / 4) | 0;
        r += g;
        b += g;

        if (r < 0) r = 0; else if (r > 255) r = 255;
        if (g < 0) g = 0; else if (g > 255) g = 255;
        if (b < 0) b = 0; else if (b > 255) b = 255;
        if (a < 0) a = 0; else if (a > 255) a = 255;

        out[o] = r;
        out[o + 1] = g;
        out[o + 2] = b;
        out[o + 3] = a;
        o += 4;
    }
    return out;
}
