// igc-kernels.js — default (pure-JS) IGC kernel seam.
//
// The single-file dist builds (`granny-ro.esm.js` / `.cjs` / `.global.js`),
// the code-split build and raw `src/` consumers resolve this module : the
// kernels are the plain-JS implementations, decode stays synchronous, and
// `initKernels()` is a no-op (nothing to instantiate).
//
// The opt-in WASM build (`./wasm`) swaps this for `./igc-kernels.wasm.js` via a
// bundler `resolveId` alias (see scripts/build-dist.mjs stage 6), routing the
// kernels through a WebAssembly module while keeping the JS versions as the
// mandatory fallback. `initKernels()` there awaits the wasm instantiation.

export { yuvToRGB } from './igc-yuv.js';
export { planeDecode } from './igc-plane.js';
export { iDWT2D } from './igc-idwt.js';
export { decodeIGCPipeline } from './igc-pipeline.js';
export {
    arithBitOpen,
    arithOpen,
    arithDecompress,
    arithWasEscaped,
    arithSetDecompressed,
    arithBitsGetValue,
    arithBitsGet,
    arithBitsRemove,
} from './igc-arith.js';

/**
 * Ensure the IGC compute kernels are ready. In the default (pure-JS) build
 * there is nothing to instantiate, so this resolves immediately — call it
 * anyway (via `Granny.ready()`) for forward-compat with the WASM build, where
 * it awaits `WebAssembly.instantiate` before the first decode.
 *
 * Idempotent ; safe to await repeatedly.
 *
 * @returns {Promise<void>}
 */
export function initKernels() {
    return Promise.resolve();
}
