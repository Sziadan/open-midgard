// igc-codec.js — default (static) IGC codec binding.
//
// The single-file dist builds (`granny-ro.esm.js` / `.cjs`) and raw `src/`
// consumers resolve this module : `decodeIGCTexture` is imported statically,
// so `parseTextured` / `extractTextures` decode IGC textures synchronously
// with no warmup — identical to the 1.0.0 contract.
//
// The code-split build (`./split`) swaps this for `./igc-codec.lazy.js` via a
// bundler `resolveId` alias (see scripts/build-dist.mjs), moving the
// ~2 000-line IGC decoder into a lazily-loaded dynamic-import chunk.
// `loadTextureCodec()` keeps the same signature across both flavors : here
// it's a no-op (the decoder is already bundled).

export { decodeIGCTexture } from './GrannyTextureIGC.js';

/**
 * Ensure the IGC texture codec is loaded. In the default (static) build the
 * decoder is already bundled, so this resolves immediately — call it anyway
 * for forward-compat with the code-split build, where it dynamic-imports the
 * IGC chunk before the first IGC `parseTextured` / `extractTextures`.
 *
 * Idempotent ; safe to await repeatedly.
 *
 * @returns {Promise<void>}
 */
export async function loadTextureCodec() {}
