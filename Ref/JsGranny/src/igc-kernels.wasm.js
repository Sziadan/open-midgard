// igc-kernels.wasm.js — WASM IGC kernel seam (the opt-in `./wasm` build).
//
// Swapped in for ./igc-kernels.js by scripts/build-dist.mjs stage 6. Holds the
// singleton WebAssembly instance + scratch base, instantiates it from the
// inlined base64 (./wasm/kernels-b64.js) in `initKernels()` — awaited via
// `Granny.ready()` before the first decode — and dispatches each kernel to the
// wasm module, falling back to the pure-JS implementation (./igc-yuv.js) when
// the module has not instantiated. The JS fallback is mandatory : if
// instantiation is skipped or fails, decode still produces byte-exact pixels.
//
// Session 1 wires only yuvToRGB. Sessions 2-4 add arith / planeDecode / iDWT
// against the same instance + linear-memory scratch base.

import { yuvToRGB as yuvJS } from './igc-yuv.js';
import * as jsArith from './igc-arith.js';
import * as jsPlane from './igc-plane.js';
import * as jsIdwt from './igc-idwt.js';
import { decodeIGCPipeline as jsPipeline } from './igc-pipeline.js';
import { createArithDriver } from './wasm/arith-driver.js';
import { createPlaneDriver } from './wasm/plane-driver.js';
import { createIdwtDriver } from './wasm/idwt-driver.js';
import { createPipelineDriver } from './wasm/pipeline-driver.js';
import KERNELS_WASM_B64 from './wasm/kernels-b64.js';

/**
 * Exports of kernels.wasm (the linear-memory ABI). Sessions 3-4 extend this
 * with the planeDecode / iDWT kernels against the same `memory`.
 *
 * @typedef {object} KernelExports
 * @property {WebAssembly.Memory} memory — the shared linear memory.
 * @property {() => number} scratchBase — first byte free of AS static data.
 * @property {(planesPtr: number, count: number, rgbaPtr: number) => void} yuvToRGB
 * @property {(abPtr: number, bufPtr: number, offset: number) => void} arithBitOpen
 * @property {(aPtr: number, uniqueValues: number) => number} arithOpen
 * @property {(aPtr: number, abPtr: number) => number} arithDecompress
 * @property {(aPtr: number, slot: number, value: number) => void} arithSetDecompressed
 * @property {(abPtr: number, scale: number) => number} arithBitsGetValue
 * @property {(abPtr: number, scale: number) => number} arithBitsGet
 * @property {(abPtr: number, lo: number, count: number, scale: number) => void} arithBitsRemove
 * @property {(bufPtr: number, srcOffset: number, outPtr: number, width: number, height: number, rowMaskPtr: number, workPtr: number) => number} planeDecode
 * @property {(outPtr: number, pitch: number, width: number, height: number, rowMaskPtr: number, tempPtr: number) => void} iDWT2D
 * @property {(bufPtr: number, width: number, height: number, alpha: number, rgbaPtr: number, workBase: number) => number} decodeIGCTexture — fused whole-pipeline entry (compressed bytes → RGBA8888, one crossing).
 */

/** @type {KernelExports | null} — kernel exports once instantiated. */
let wasm = null;
/** First linear-memory byte free of AS static data (from `scratchBase()`). */
let base = 0;
/** @type {ReturnType<typeof createArithDriver> | null} — arith kernel driver. */
let arith = null;
/** @type {ReturnType<typeof createPlaneDriver> | null} — planeDecode driver. */
let plane = null;
/** @type {ReturnType<typeof createIdwtDriver> | null} — iDWT2D driver. */
let idwt = null;
/** @type {ReturnType<typeof createPipelineDriver> | null} — fused-decode driver. */
let pipeline = null;

/** Decode a base64 string to bytes (global `atob` : Node ≥ 16 + browsers). */
function b64ToBytes(b64) {
    const bin = atob(b64);
    const n = bin.length;
    const bytes = new Uint8Array(n);
    for (let i = 0; i < n; i++) bytes[i] = bin.charCodeAt(i);
    return bytes;
}

/**
 * Instantiate the kernel wasm module (idempotent). Awaited via
 * `Granny.ready()` before the first decode ; browser main-thread
 * `WebAssembly.instantiate` is async and the sync form is capped at 4 KB, so
 * this must be awaited, not resolved synchronously.
 *
 * @returns {Promise<void>}
 */
export async function initKernels() {
    if (wasm) return;
    const { instance } = await WebAssembly.instantiate(b64ToBytes(KERNELS_WASM_B64), {});
    wasm = /** @type {KernelExports} */ (/** @type {unknown} */ (instance.exports));
    base = wasm.scratchBase() >>> 0;
    arith = createArithDriver(instance);
    plane = createPlaneDriver(instance);
    idwt = createIdwtDriver(instance);
    pipeline = createPipelineDriver(instance);
}

/** Grow `memory` so `[base, base+bytes)` is addressable ; returns the buffer. */
function ensure(memory, bytes) {
    const end = base + bytes;
    const have = memory.buffer.byteLength;
    if (have < end) memory.grow(Math.ceil((end - have) / 65536));
    return memory.buffer;
}

/**
 * YUV-ish → RGBA8888 — WASM when instantiated, else the JS oracle (fallback).
 * Signature identical to {@link yuvJS} so `decodeIGCTexture`'s call site is
 * build-agnostic.
 *
 * @param {Int16Array} yp @param {Int16Array} up @param {Int16Array} vp @param {Int16Array} ap
 * @param {number} width @param {number} height
 * @returns {Uint8Array}
 */
export function yuvToRGB(yp, up, vp, ap, width, height) {
    if (!wasm) return yuvJS(yp, up, vp, ap, width, height);

    const count = width * height;
    const planeBytes = count * 8; // 4 planes × count × i16
    const rgbaPtr = base + planeBytes;
    const buf = ensure(wasm.memory, planeBytes + count * 4);

    // Planes written contiguously (Y, U, V, A) from `base` per the ABI.
    const planes = new Int16Array(buf, base, count * 4);
    planes.set(yp, 0);
    planes.set(up, count);
    planes.set(vp, count * 2);
    planes.set(ap, count * 3);

    wasm.yuvToRGB(base, count, rgbaPtr);

    // Copy out : the next decode reuses the same scratch region.
    return new Uint8Array(buf, rgbaPtr, count * 4).slice();
}

// --- Arith range-coder + model ----------------------------------------------
//
// WASM when instantiated, else the pure-JS oracle (./igc-arith.js). Handles
// (`a` model, `ab` coder, `cur` decode result) are opaque to the caller
// (GrannyTextureIGC.js) : pointers/ints in wasm mode, objects in the fallback —
// never mixed within a process (the mode is fixed after `initKernels`). The
// escape sentinel is a negative return : `cur < 0` ⇒ escape, slot = `-cur - 1`.

/** @param {Uint8Array} buf @param {number} offset */
export function arithBitOpen(buf, offset) {
    return arith ? arith.bitOpen(buf, offset) : jsArith.arithBitOpen(buf, offset);
}

/** @param {number} max @param {number} num */
export function arithOpen(max, num) {
    if (arith) {
        // Mirror the JS oracle's untrusted-input cap (igc-arith.js) : the wasm
        // model-open bypasses it otherwise. `num` is visible here, so an O(1)
        // JS guard suffices — no need for a kernel-side check on this seam.
        if (num > jsArith.IGC_MAX_ALPHABET) {
            throw new Error(`igc arith alphabet ${num} exceeds cap ${jsArith.IGC_MAX_ALPHABET}`);
        }
        return arith.open(num);
    }
    return jsArith.arithOpen(max, num);
}

export function arithDecompress(a, ab) {
    return arith ? arith.decompress(a, ab) : jsArith.arithDecompress(a, ab);
}

export function arithWasEscaped(cur) {
    return arith ? cur < 0 : jsArith.arithWasEscaped(cur);
}

export function arithSetDecompressed(cur, escaped, a) {
    if (arith) arith.setDecompressed(a, -cur - 1, escaped);
    else jsArith.arithSetDecompressed(cur, escaped, a);
}

export function arithBitsGetValue(ab, scale) {
    return arith ? arith.bitsGetValue(ab, scale) : jsArith.arithBitsGetValue(ab, scale);
}

export function arithBitsGet(ab, scale) {
    return arith ? arith.bitsGet(ab, scale) : jsArith.arithBitsGet(ab, scale);
}

export function arithBitsRemove(ab, lo, count, scale) {
    if (arith) arith.bitsRemove(ab, lo, count, scale);
    else jsArith.arithBitsRemove(ab, lo, count, scale);
}

// --- planeDecode ------------------------------------------------------------
//
// WASM when instantiated (one crossing per plane — the arith coder runs
// entirely in-wasm, no per-symbol boundary), else the pure-JS oracle
// (./igc-plane.js). Signature matches the oracle so `decodeIGCTexture`'s call
// site is build-agnostic.

/**
 * Decode one IGC plane bitstream into the S16 `output` plane.
 *
 * @param {Uint8Array} src @param {number} srcOffset
 * @param {Int16Array} output @param {number} outOffset
 * @param {number} width @param {number} height
 * @param {Uint8Array | null} rowMask
 * @returns {number} bytes consumed from `src`.
 */
export function planeDecode(src, srcOffset, output, outOffset, width, height, rowMask) {
    if (!plane) return jsPlane.planeDecode(src, srcOffset, output, outOffset, width, height, rowMask);

    const r = plane.planeDecode(src, srcOffset, width, height, rowMask != null);
    output.set(r.plane, outOffset);
    if (rowMask) rowMask.set(r.mask);
    return r.consumed;
}

// --- iDWT2D -----------------------------------------------------------------
//
// WASM when instantiated (a whole plane's inverse-wavelet pass runs in-wasm),
// else the pure-JS oracle (./igc-idwt.js). Signature matches the oracle so
// `decodeIGCTexture`'s 4×-per-plane call site is build-agnostic. The `temp`
// scratch plane is the oracle's ; the wasm driver uses its own linear-memory
// scratch and ignores it.

/**
 * Inverse-wavelet-transform one plane in place.
 *
 * @param {Int16Array} output — the plane, transformed in place.
 * @param {number} pitch — S16-index row stride for this pass.
 * @param {number} width @param {number} height — sub-band dimensions.
 * @param {Uint8Array | null} rowMask — zero-row mask, or null.
 * @param {Int16Array} temp — scratch plane (used by the JS oracle only).
 */
export function iDWT2D(output, pitch, width, height, rowMask, temp) {
    if (!idwt) return jsIdwt.iDWT2D(output, pitch, width, height, rowMask, temp);

    output.set(idwt.iDWT2D(output, pitch, width, height, rowMask));
}

// --- decodeIGCPipeline (fused) ----------------------------------------------
//
// The whole per-plane decode (planeDecode + 4× iDWT2D + yuvToRGB) as one
// JS→WASM crossing — each plane stays resident in linear memory, no per-kernel
// boundary copies. WASM when instantiated, else the pure-JS pipeline oracle
// (./igc-pipeline.js). The fallback is mandatory : if instantiation was skipped
// or failed, decode still produces byte-exact pixels.

/**
 * Decode one IGC texture (compressed bytes → RGBA8888) — WASM fused entry when
 * instantiated, else the JS pipeline oracle. Signature matches the oracle so
 * `decodeIGCTexture`'s call site is build-agnostic.
 *
 * @param {Uint8Array} src — the IGC bitstream.
 * @param {number} width @param {number} height — texture dimensions.
 * @param {0 | 1 | boolean} alpha — whether the bitstream carries an A plane.
 * @returns {Uint8Array} RGBA8888, length = width*height*4.
 */
export function decodeIGCPipeline(src, width, height, alpha) {
    return pipeline
        ? pipeline.decode(src, width, height, alpha)
        : jsPipeline(src, width, height, alpha);
}
