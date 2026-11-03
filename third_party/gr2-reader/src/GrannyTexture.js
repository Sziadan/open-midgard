// GrannyTexture.js — Texture + image + MIP-level extraction (public surface).
//
// Walks `root.Textures → Images → MIPLevels → Pixels` and decodes each
// MIP to RGBA8888 based on the texture's `Encoding` tag :
//
//   - `1` (Raw)  — Pixels are 32 bpp BGRA per IGC-FORMAT.md § 6.
//                  Swizzle BGRA → RGBA in JS (no shim, no codec).
//   - `2` (S3TC) — Not in the iRO ver12 corpus ; throws « not supported ».
//   - `3` (IGC)  — RAD BinkTC (the codec exposed as
//                  `_GrannyDecompressIGCTexture@12` in granny2.dll). The full
//                  JS bitstream decode lives in
//                  [./GrannyTextureIGC.js](./GrannyTextureIGC.js). The default
//                  build binds it statically → IGC decodes synchronously ; the
//                  code-split (`./split`) build needs `await loadTextureCodec()`
//                  once before the first IGC decode.
//
// Public surface : `extractTextures(loaded, options)`. The walker is
// also exported as `walkTextureImages(loaded)` for tooling that needs
// the raw pre-decode records (the bake driver lifts this).
//
// Pre-condition : `loaded` must come from `GrannyTypeTree.loadGR2(file)`.

import {
    parseObject,
    parseTypeTree,
    readReferenceArrayObjects,
} from './GrannyTypeTree.js';
// IGC codec is reached through an indirection module so the code-split
// (`./split`) build can swap it for a lazy dynamic-import chunk. The default
// build binds it statically → IGC decode stays synchronous, no warmup.
// `loadTextureCodec()` is re-exported for consumers of the code-split build.
import { decodeIGCTexture, loadTextureCodec } from './igc-codec.js';

export { loadTextureCodec };

/** Raw = 32 bpp BGRA stored verbatim, no codec. */
export const ENCODING_RAW = 1;
/** S3TC = DXT-N block compression. Not in iRO corpus ; throws on decode. */
export const ENCODING_S3TC = 2;
/** IGC = RAD BinkTC (wavelet + arithmetic + YUV→RGB). The iRO ver12 default. */
export const ENCODING_IGC = 3;

/**
 * One decoded (texture, image, MIP) entry returned by {@link extractTextures}.
 *
 * @typedef {object} TextureRecord
 * @property {number} texIdx — 0-based texture index within `root.Textures`.
 * @property {number} imgIdx — 0-based image index within the texture's `Images` array (iRO uses 0).
 * @property {number} mipIdx — 0-based MIP-level index within the image's `MIPLevels` array (iRO uses 0).
 * @property {string} name — texture authored name (defaults to `tex<texIdx>` when `FromFileName` is empty).
 * @property {string} fromFileName — original `FromFileName` from the Granny struct (may include Windows paths).
 * @property {number} width — MIP width in pixels.
 * @property {number} height — MIP height in pixels.
 * @property {1 | 2 | 3} encoding — encoding tag (`1` = Raw, `2` = S3TC, `3` = IGC).
 * @property {number} subFormat — S3TC subformat (`0..3` = DXT-N) ; meaningful only for `encoding === 2`.
 * @property {0 | 1} alpha — `1` when the texture carries an alpha channel ; `0` for opaque.
 * @property {Uint8Array} pixels — decoded RGBA8888 bytes, length = `width * height * 4`.
 */

/**
 * One pre-decode record yielded by {@link walkTextureImages}.
 *
 * @typedef {object} TextureWalkRecord
 * @property {number} texIdx
 * @property {number} imgIdx
 * @property {number} mipIdx
 * @property {number} width
 * @property {number} height
 * @property {number} encoding
 * @property {number} subFormat
 * @property {0 | 1} alpha
 * @property {string} fromFileName
 * @property {Uint8Array | null} pixelBytes — raw `Pixels` bytes from the .gr2 ;
 *   `null` when the pixel array can't be resolved.
 * @property {number} pixelCount — length declared in the Granny `Pixels` /
 *   `PixelBytes` reference (may differ from `pixelBytes?.length` on broken assets).
 */

/**
 * Options for {@link extractTextures} and {@link walkTextureImages}.
 *
 * @typedef {object} ExtractTexturesOptions
 * @property {number} [maxTextures] - cap on the number of textures walked (default 256).
 * @property {number} [maxImages] - cap on the number of images per texture (default 8).
 * @property {number} [maxMips] - cap on the number of MIP levels per image (default 32 ; iRO corpus uses 1).
 */

// --- extractTextures --------------------------------------------------

/**
 * Walk `root.Textures` and decode every (texture, image, MIP) triple to
 * RGBA8888. Returns a flat array — one entry per MIP — matching the
 * `tests/fixtures/baked/textures/textures.json` manifest shape so byte-
 * exact parity tests can join by `(texIdx, imgIdx, mipIdx)`.
 *
 * Texture-less fixtures (animation-only files in the iRO corpus) resolve
 * to `[]`. IGC textures decode synchronously in the default build (the
 * code-split build needs `await loadTextureCodec()` first) ; S3TC throws
 * (no iRO asset uses it).
 *
 * @param {import('./GrannyTypeTree.js').LoadedGR2} loaded — output of `loadGR2(file)`.
 * @param {ExtractTexturesOptions} [options]
 * @returns {TextureRecord[]}
 * @throws {Error} on S3TC textures (encoding=2) — no iRO asset uses them.
 */
export function extractTextures(loaded, options = {}) {
    const records = walkTextureImages(loaded, options);
    const decoded = new Array(records.length);
    let n = 0;
    for (let i = 0; i < records.length; i++) {
        const record = records[i];
        const pixels = decodeRecord(record);
        if (pixels === null) continue;
        decoded[n++] = {
            texIdx: record.texIdx,
            imgIdx: record.imgIdx,
            mipIdx: record.mipIdx,
            name: record.fromFileName || `tex${record.texIdx}`,
            fromFileName: record.fromFileName,
            width: record.width,
            height: record.height,
            encoding: record.encoding,
            subFormat: record.subFormat,
            alpha: record.alpha,
            pixels,
        };
    }
    decoded.length = n;
    return decoded;
}

function decodeRecord(record) {
    const { encoding, width, height, pixelBytes } = record;
    if (encoding === ENCODING_RAW) {
        return decodeRaw(pixelBytes, width, height);
    }
    if (encoding === ENCODING_IGC) {
        return decodeIGCTexture({
            Width: width,
            Height: height,
            Alpha: record.alpha,
            ImageData: pixelBytes,
        });
    }
    if (encoding === ENCODING_S3TC) {
        throw new Error(
            `extractTextures: S3TC textures (encoding=2) not yet supported in granny-ro-js — ` +
            `file an issue if an iRO asset hits this. ` +
            `Texture: ${describeRecord(record)}`
        );
    }
    throw new Error(
        `extractTextures: unknown encoding=${encoding} for ${describeRecord(record)}`
    );
}

/**
 * Raw encoding=1 decode : the on-disk PixelBytes are 32 bpp BGRA per
 * IGC-FORMAT.md § 6 (Windows DIB convention). Swizzle to RGBA8888 ;
 * mirrors the swizzle the bake driver applies in `bakeRaw`.
 */
function decodeRaw(pixelBytes, width, height) {
    const expected = width * height * 4;
    if (!pixelBytes || pixelBytes.length !== expected) {
        throw new Error(
            `extractTextures: Raw texture byte count ${pixelBytes?.length} != W*H*4 (${expected})`
        );
    }
    const rgba = new Uint8Array(expected);
    for (let i = 0; i < expected; i += 4) {
        rgba[i] = pixelBytes[i + 2];      // R from B-slot
        rgba[i + 1] = pixelBytes[i + 1];  // G unchanged
        rgba[i + 2] = pixelBytes[i];      // B from R-slot
        rgba[i + 3] = pixelBytes[i + 3];  // A unchanged
    }
    return rgba;
}

function describeRecord(record) {
    return `tex${record.texIdx}-img${record.imgIdx}-mip${record.mipIdx}`;
}

// --- walkTextureImages -------------------------------------------------

/**
 * Walk `root.Textures → Images → MIPLevels` and emit one record per
 * (texture, image, MIP) triple, carrying the raw `Pixels` bytes
 * pre-decode. This is the shared traversal used by both
 * `extractTextures` (decode path) and `scripts/bake-textures.mjs` (Wine
 * shim driver).
 *
 * Animation-only fixtures (or any fixture where `root.Textures` is
 * empty / absent) resolve to `[]`.
 *
 * @param {import('./GrannyTypeTree.js').LoadedGR2} loaded — output of `loadGR2(file)`.
 * @param {ExtractTexturesOptions} [options]
 * @returns {TextureWalkRecord[]}
 */
export function walkTextureImages(loaded, options = {}) {
    const maxTextures = options.maxTextures ?? 256;
    const maxImages = options.maxImages ?? 8;
    const maxMips = options.maxMips ?? 32;

    const file = loaded.file;
    const rootTypeTree = parseTypeTree(loaded, file.header.root_type);
    const root = parseObject(loaded, rootTypeTree, file.header.root_object, { maxArrayRefs: maxTextures });
    const texField = root.Textures;
    if (!texField || !texField.reference_type) return [];

    const textureType = texField.reference_type;
    const textureRefs = texField.element_refs ?? [];
    if (textureRefs.length === 0) return [];

    const texMembers = parseTypeTree(loaded, [textureType.section, textureType.offset]);
    const records = [];

    const texCount = textureRefs.length < maxTextures ? textureRefs.length : maxTextures;
    for (let ti = 0; ti < texCount; ti++) {
        const texRef = textureRefs[ti];
        if (!texRef) continue;
        const texFields = parseObject(loaded, texMembers, [texRef.section, texRef.offset], { maxArrayRefs: 64 });
        const width = texFields.Width?.value ?? 0;
        const height = texFields.Height?.value ?? 0;
        const encoding = texFields.Encoding?.value ?? 0;
        const subFormat = texFields.SubFormat?.value ?? 0;
        const fromFileName = texFields.FromFileName?.value ?? '';
        const alpha = readAlphaFromLayout(loaded, texMembers, texFields, texRef);

        const imagesField = texFields.Images;
        const imageType = imagesField?.reference_type ?? null;
        const images = imageType ? readReferenceArrayObjects(
            loaded,
            imagesField.target ?? null,
            imagesField.count ?? 0,
            imageType,
            { maxCount: maxImages },
        ) : [];

        for (let ii = 0; ii < images.length; ii++) {
            const imgFields = images[ii].fields;
            const mipsField = imgFields.MIPLevels;
            const mipType = mipsField?.reference_type ?? null;
            const mips = mipType ? readReferenceArrayObjects(
                loaded,
                mipsField.target ?? null,
                mipsField.count ?? 0,
                mipType,
                { maxCount: maxMips },
            ) : [];

            for (let mi = 0; mi < mips.length; mi++) {
                const mipFields = mips[mi].fields;
                // The 2002 iRO reflection schema calls this `Pixels` ;
                // the leaked-SDK 2007 schema calls it `PixelBytes`. Look
                // up both for forward-compat ; see LOG.md § 2 Gotchas.
                const pixelField = mipFields.Pixels ?? mipFields.PixelBytes;
                const pixelCount = pixelField?.count ?? 0;
                const pixelTarget = pixelField?.target ?? null;
                let pixelBytes = null;
                if (pixelTarget && pixelCount > 0) {
                    const section = loaded.sectionsOriginal[pixelTarget.section];
                    if (section
                        && pixelTarget.offset >= 0
                        && pixelTarget.offset + pixelCount <= section.length) {
                        pixelBytes = section.subarray(pixelTarget.offset, pixelTarget.offset + pixelCount);
                    }
                }
                records.push(/** @type {TextureWalkRecord} */ ({
                    texIdx: ti,
                    imgIdx: ii,
                    mipIdx: mi,
                    width,
                    height,
                    encoding,
                    subFormat,
                    alpha,
                    fromFileName,
                    pixelBytes,
                    pixelCount,
                }));
            }
        }
    }
    return records;
}

/**
 * Infer Alpha (0 / 1) from the Texture's inline Layout struct. The
 * Layout's BytesPerPixel field gates the alpha channel : 4 = RGBA →
 * alpha=1 ; 3 = RGB → alpha=0. Falls back to alpha=1 (the iRO corpus
 * default — all IGC textures set BinkEncodeAlpha) when the Layout is
 * unreadable.
 */
function readAlphaFromLayout(loaded, texMembers, texFields, texRef) {
    try {
        const layoutField = texFields.Layout;
        if (!layoutField || layoutField.type !== 'inline') return 1;
        let layoutMember = null;
        for (let i = 0; i < texMembers.length; i++) {
            if (texMembers[i].name === 'Layout') {
                layoutMember = texMembers[i];
                break;
            }
        }
        if (!layoutMember || !layoutMember.referenceType) return 1;
        const refSection = layoutMember.referenceType.section;
        if (!loaded.sectionsOriginal[refSection]) return 1;
        const layoutMembers = parseTypeTree(loaded, [refSection, layoutMember.referenceType.offset]);
        const layoutOffset = layoutField.offset ?? 0;
        const layoutFields = parseObject(loaded, layoutMembers, [texRef.section, texRef.offset + layoutOffset]);
        const bpp = layoutFields.BytesPerPixel?.value;
        return bpp === 3 ? 0 : 1;
    } catch {
        return 1;
    }
}
