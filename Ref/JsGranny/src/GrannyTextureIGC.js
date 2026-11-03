// GrannyTextureIGC.js — IGC (RAD BinkTC) texture-bitstream decoder.
//
// Clean-room port of the codec exposed as `_GrannyDecompressIGCTexture@12`
// in granny2.dll (iRO ver12), spec'd in
// `iRO_ver12.0-full-client-data/RE/granny2/IGC-FORMAT.md` and validated
// byte-exact against the parity-bake golden data in
// `tests/fixtures/baked/textures/*.rgba` (produced by S2's Wine shim).
//
// Pipeline per IGC-FORMAT.md § 7 :
//
//   1. `planeDecode(src)` — adaptive arithmetic coder + 4-level sub-band
//      traversal, produces a wavelet-transformed S16 plane (Y, U, V, A).
//      Asm cite : `granny2.dll @ 0x100045b0` (`FromBinkTC`), leaked-SDK
//      `encode.c:1884` (`plane_decode`).
//   2. `iDWT2D(plane)` — 4 passes of RAD's reversible-integer lifting-
//      scheme wavelet at increasing resolution (1/8 → 1/4 → 1/2 → full).
//      Asm cite : `granny2.dll @ 0x10009700`, leaked-SDK
//      `wavelet.c:1328` (`iDWT2D` dispatcher).
//   3. `yuvToRGB(yp, up, vp, ap)` — custom integer-reversible YUV-ish
//      colorspace inversion, writes RGBA8888 in dest. Asm cite :
//      `granny2.dll @ 0x10009a30`, leaked-SDK `granny_bink.cpp:165`.
//
// Most kernels are private (file-local). `yuvToRGB` lives behind a swappable
// seam (./igc-kernels.js) so the opt-in WASM build can dispatch it to a
// WebAssembly module ; the pure-JS implementation stays in ./igc-yuv.js as the
// mandatory fallback + byte-exact oracle. Public exports : `decodeIGCTexture`,
// `yuvToRGB` (re-exported from the seam).
import {
    yuvToRGB,
    decodeIGCPipeline,
} from './igc-kernels.js';

// Minimum DWT dimensions — also the shouldBink small-image guard below. The
// inverse wavelet kernels (`iDWT2D` + row/col/Haar variants) were extracted to
// ./igc-idwt.js (the byte-exact oracle + WASM fallback) ; `iDWT2D` now dispatches
// through the seam like `yuvToRGB` / `planeDecode`.
const SMALLEST_DWT_ROW = 16;
const SMALLEST_DWT_COL = 16;

// Untrusted-input allocation caps for the full-DWT path. `Width`/`Height`
// are read verbatim from the reflection struct and drive four Int16Array
// plane allocs + a temp (igc-pipeline.js:40,43) with no upper bound —
// Width=Height=16384 passes the 16-alignment gate yet asks for ~2 GB. RO
// textures are ≤ ~1024²/2048², so 4096² gives ≥16× headroom. The per-byte
// ratio is a loose bomb cross-check mirroring the raw path
// (GrannyTexture.js:163) : the IGC bitstream must carry at least ~1 byte
// per 4096 declared pixels. Both are O(1) checks at the choke point.
const IGC_MAX_PIXELS = 4096 * 4096; // 16 Mpix
const IGC_MAX_PIXELS_PER_BYTE = 4096;

// ============================================================================
// Public API.
//
// These types are internal to the codec : `decodeIGCTexture` / `yuvToRGB` are
// not re-exported from the package's main entry. External callers use
// `extractTextures` from `./GrannyTexture.js`, which calls `decodeIGCTexture`.

/**
 * Input to {@link decodeIGCTexture}.
 *
 * @typedef {object} IGCImage
 * @property {number} Width — image width in pixels (from the `GrannyIGCTexture` reflection struct).
 * @property {number} Height — image height in pixels.
 * @property {0 | 1} Alpha — alpha flag (`1` = `BinkEncodeAlpha`, `0` = no A plane in the bitstream).
 * @property {Uint8Array} ImageData — the IGC bitstream as stored in the .gr2 `Pixels` array (see IGC-FORMAT.md § 3).
 */

/**
 * Decode one IGC texture (RAD BinkTC : wavelet + arithmetic + YUV→RGB) to
 * RGBA8888. Clean-room port of `_GrannyDecompressIGCTexture@12`.
 *
 * Images with `Width * Height <= 256` take the small-image passthrough
 * (granny2.dll bypasses BinkTC and copies the RGBA bytes through unchanged) ;
 * larger images run the full plane-decode → 4-level inverse DWT → YUV→RGB
 * pipeline. Non-16-aligned dimensions are rejected — the iRO corpus walker
 * filters those out upstream.
 *
 * @param {IGCImage} igcImage
 * @returns {Uint8Array} RGBA8888 bytes, length = `Width * Height * 4`.
 * @throws {Error} when a small-image input carries fewer than `Width*Height*4`
 *   bytes, or when the dimensions are not 16-aligned (unsupported fallback).
 */
// Asm cite : `granny2.dll @ 0x100045b0` (`FromBinkTC`).
export function decodeIGCTexture(igcImage) {
    const { Width: width, Height: height, Alpha: alpha, ImageData: src } = igcImage;

    // Small-image fallback : granny2.dll @ fcn.10009c30 returns false when
    // `width * height <= 256` (asm `imul edx, ecx, eax ; cmp edx, 0x100 ; jle`).
    // The IGC dispatcher at fcn.10009e50 then bypasses BinkTC and hands the
    // pixelBytes to ConvertPixelFormat — which is the identity for the
    // (source RGBA8888 → dest RGBA8888) layout pair the dispatcher selects
    // here (data.1002a228, alpha=1). Verified : the wine-baked golden for
    // guildflag90_1.gr2:tex[1] (16×16) is byte-identical to its input
    // pixelBytes, so the codec writes them through unchanged.
    if (width * height <= 256) {
        const expected = width * height * 4;
        if (src.byteLength < expected) {
            throw new Error(
                `decodeIGCTexture: small-image fallback expects ${expected} ` +
                `RGBA bytes, got ${src.byteLength} (W=${width}, H=${height})`
            );
        }
        return new Uint8Array(src.buffer, src.byteOffset, expected).slice();
    }

    if (width < SMALLEST_DWT_ROW || height < SMALLEST_DWT_COL ||
        (width & 15) !== 0 || (height & 15) !== 0) {
        throw new Error(
            `decodeIGCTexture: shouldBink fallback not supported (W=${width}, ` +
            `H=${height}). iRO corpus walker should filter these out.`
        );
    }

    // Cap the plane allocation before igc-pipeline.js sizes 5× Int16Array
    // on width*height — reject an oversized/bomb texture instead of OOMing.
    const pixels = width * height;
    if (pixels > IGC_MAX_PIXELS ||
        pixels > src.byteLength * IGC_MAX_PIXELS_PER_BYTE) {
        throw new Error(
            `decodeIGCTexture: ${width}×${height} (${pixels} px) exceeds cap ` +
            `(${IGC_MAX_PIXELS} px / ${IGC_MAX_PIXELS_PER_BYTE}× of ${src.byteLength} B)`
        );
    }

    // The per-plane decode (planeDecode → 4× iDWT2D → alpha fill → yuvToRGB)
    // runs behind the outermost seam : the pure-JS pipeline oracle by default
    // (./igc-pipeline.js), or a single fused WASM entry in the opt-in `./wasm`
    // build (one JS→WASM crossing, planes resident across their iDWT passes).
    return decodeIGCPipeline(src, width, height, alpha);
}

// `yuvToRGB` is re-exported from the kernel seam (default = ./igc-yuv.js pure
// JS ; the WASM build swaps the seam for ./igc-kernels.wasm.js). The unit
// suite imports it from here to validate the kernel on synthetic planes.
export { yuvToRGB };