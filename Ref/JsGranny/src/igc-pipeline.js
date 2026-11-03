// igc-pipeline.js — the pure-JS IGC decode pipeline : the byte-exact oracle and
// the mandatory fallback for the fused WASM `decodeIGCTexture` entry.
//
// The per-plane orchestration (plane allocation → planeDecode → 4× iDWT2D →
// alpha fill → yuvToRGB) was moved verbatim out of GrannyTextureIGC.js so the
// opt-in WASM build can dispatch the whole pipeline through the outermost seam
// (./igc-kernels.js) as a single JS→WASM crossing. This module imports the
// kernels **directly from the pure-JS oracles** — never the seam — so it stays a
// self-contained fallback with no circular import : the dual-dispatch in
// igc-kernels.wasm.js drops to it when the wasm module hasn't instantiated.
//
// It runs only the real DWT path : GrannyTextureIGC.js keeps the small-image /
// shouldBink fallback guards ahead of the call, so `src` here always describes a
// 16-aligned texture with `width * height > 256`.
//
// The loop is factored into `runIGCPipeline`, parameterized by the three kernels
// it drives : production binds the pure-JS oracles (`decodeIGCPipeline`), and the
// dev differential seams (scripts/*.tee.js) inject their WASM-vs-JS tee-wrapped
// kernels so the per-kernel gates still fire from inside the real pipeline.

import { planeDecode as jsPlaneDecode } from './igc-plane.js';
import { iDWT2D as jsIDWT2D } from './igc-idwt.js';
import { yuvToRGB as jsYuvToRGB } from './igc-yuv.js';

/**
 * The per-plane IGC decode loop, parameterized by the kernels it drives.
 *
 * @param {Uint8Array} src — the IGC bitstream (4-byte header + per-plane data).
 * @param {number} width — texture width in pixels (16-aligned).
 * @param {number} height — texture height in pixels (16-aligned).
 * @param {0 | 1 | boolean} alpha — whether the bitstream carries an A plane.
 * @param {typeof jsPlaneDecode} planeDecode — plane-decode kernel.
 * @param {typeof jsIDWT2D} iDWT2D — inverse-wavelet kernel.
 * @param {typeof jsYuvToRGB} yuvToRGB — YUV→RGBA kernel.
 * @returns {Uint8Array} RGBA8888 bytes, length = `width * height * 4`.
 */
export function runIGCPipeline(src, width, height, alpha, planeDecode, iDWT2D, yuvToRGB) {
    const planeCount = alpha ? 4 : 3;
    const planes = new Array(4);
    for (let i = 0; i < 4; i++) planes[i] = new Int16Array(width * height);

    const rowMask = new Uint8Array(height);
    const temp = new Int16Array(width * height);

    let cursor = 4;
    for (let p = 0; p < planeCount; p++) {
        const consumed = planeDecode(src, cursor, planes[p], 0, width, height,
            (p === 0) ? rowMask : null);
        cursor += consumed;

        iDWT2D(planes[p], width * 8, width >> 3, height >> 3, null, temp);
        iDWT2D(planes[p], width * 4, width >> 2, height >> 2, null, temp);
        iDWT2D(planes[p], width * 2, width >> 1, height >> 1, null, temp);
        iDWT2D(planes[p], width, width, height, (p === 0) ? rowMask : null, temp);
    }

    if (!alpha) {
        for (let i = 0; i < width * height; i++) planes[3][i] = 255;
    }

    return yuvToRGB(planes[0], planes[1], planes[2], planes[3], width, height);
}

/**
 * Decode one IGC texture's plane bitstreams to RGBA8888 (pure JS) — the
 * byte-exact oracle + mandatory WASM fallback.
 *
 * @param {Uint8Array} src — the IGC bitstream.
 * @param {number} width @param {number} height — texture dimensions (16-aligned).
 * @param {0 | 1 | boolean} alpha — whether the bitstream carries an A plane.
 * @returns {Uint8Array} RGBA8888 bytes, length = `width * height * 4`.
 */
export function decodeIGCPipeline(src, width, height, alpha) {
    return runIGCPipeline(src, width, height, alpha, jsPlaneDecode, jsIDWT2D, jsYuvToRGB);
}
