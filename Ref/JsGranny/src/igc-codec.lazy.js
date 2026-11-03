// igc-codec.lazy.js — lazy (code-split) IGC codec binding.
//
// Used ONLY by the `./split` dist build (aliased in place of `./igc-codec.js`
// by scripts/build-dist.mjs). The ~2 000-line IGC decoder lives in its own
// chunk, dynamic-import()'d on demand by `loadTextureCodec()` and cached — so
// an anim-only or raw-texture-only (encoding=1) consumer never fetches it.
//
// `decodeRecord` (GrannyTexture.js) calls `decodeIGCTexture` synchronously ;
// to keep that path sync while the codec loads asynchronously, the consumer
// must `await loadTextureCodec()` once before decoding an IGC texture. Until
// then `decodeIGCTexture` throws an actionable error rather than returning
// garbage.

let _decode = null;
let _loading = null;

/**
 * Decode one IGC image to RGBA8888. In the code-split build this requires
 * `await loadTextureCodec()` to have resolved first — throws otherwise.
 *
 * @param {{ Width:number, Height:number, Alpha:number, ImageData:Uint8Array }} igcImage
 * @returns {Uint8Array} RGBA8888 pixels
 */
export function decodeIGCTexture(igcImage) {
    if (_decode === null) {
        throw new Error(
            'granny-ro-js/split: IGC texture codec not loaded — ' +
            'call `await loadTextureCodec()` once before decoding IGC textures.'
        );
    }
    return _decode(igcImage);
}

/**
 * Dynamic-import + cache the IGC decoder chunk. Idempotent : concurrent and
 * repeat calls share one in-flight import. Await this before the first IGC
 * `parseTextured` / `extractTextures` in the code-split build.
 *
 * @returns {Promise<void>}
 */
export async function loadTextureCodec() {
    if (_decode !== null) return;
    if (_loading === null) {
        _loading = import('./GrannyTextureIGC.js').then((m) => {
            _decode = m.decodeIGCTexture;
        });
    }
    await _loading;
}
