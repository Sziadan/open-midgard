// Granny.js — public entry point for granny-ro-js.
//
// S3' surface : parse a .gr2 buffer + decompress each section (NONE / OODLE0).
// S5 adds parse(buffer) → { file, typeTree, root } : full DataTypeDefinition
// walk + RootObject materialization. See ./GrannyTypeTree.js for details.

import {
    parseGR2File,
    COMPRESSION_NONE,
    COMPRESSION_OODLE0,
    COMPRESSION_NAMES,
    SECTION_NAMES,
} from './GrannyFile.js';
import { decompressOodle0 } from './GrannyOodle0.js';
import {
    loadGR2,
    parseTypeTree,
    parseObject,
    objectStorageSize,
    readReferenceArrayObjects,
} from './GrannyTypeTree.js';
import { extractSkeletons } from './GrannySkeleton.js';
import { extractMeshes, extractMaterials } from './GrannyMesh.js';
import { extractModels } from './GrannyModel.js';
import { extractTextures, walkTextureImages, loadTextureCodec } from './GrannyTexture.js';
import { initKernels } from './igc-kernels.js';
import { extractAnimations, evaluateTransformTrack, evaluateAnimation } from './GrannyAnimation.js';
import { readTransform, IDENTITY_TRANSFORM } from './GrannyTransform.js';
import {
    composeLocalMatrix,
    multiplyMat4,
    composeWorldPose,
    composeSkinningMatrices,
    poseSkeletonAt,
} from './GrannyPose.js';

export { parseGR2File, COMPRESSION_NAMES, SECTION_NAMES };
export { COMPRESSION_NONE, COMPRESSION_OODLE0 };
export { loadGR2, parseTypeTree, parseObject, objectStorageSize, readReferenceArrayObjects };
export { extractSkeletons, extractMeshes, extractMaterials, extractModels };
export { extractTextures, walkTextureImages, loadTextureCodec };
export { extractAnimations, evaluateTransformTrack, evaluateAnimation };
export { readTransform, IDENTITY_TRANSFORM };
export {
    composeLocalMatrix,
    multiplyMat4,
    composeWorldPose,
    composeSkinningMatrices,
    poseSkeletonAt,
};

// --- public type surface -----------------------------------------------
// The Parse* result/option shapes below are the entry's own types. Types
// pulled from sibling modules are referenced inline via `import(...)` so the
// rolled declarations resolve to a single canonical definition (no re-export
// aliases). The extractor/pose types are surfaced on the package entry by the
// src/index.d.ts barrel (named `export type` re-exports).

/**
 * Structural subset of {@link import('./GrannyFile.js').GR2Section} the
 * dispatcher needs : the codec inputs + the `compression` tag for routing.
 *
 * @typedef {import('./GrannyOodle0.js').Oodle0SectionInput
 *   & { compression: number }} DispatchSectionInput
 */

/**
 * Result of the top-level {@link parse} entry.
 *
 * @typedef {object} ParseResult
 * @property {import('./GrannyFile.js').GR2File} file — header + section table for the input buffer.
 * @property {ReadonlyArray<import('./GrannyTypeTree.js').TypeMember>} typeTree — members
 *   of the root type tree (terminates at MT_END).
 * @property {import('./GrannyTypeTree.js').ParsedObject} root — materialized root
 *   object, keyed by member name.
 */

/**
 * Result of {@link parseModel} : {@link ParseResult} + skeleton + mesh extraction.
 *
 * @typedef {ParseResult & {
 *   skeletons: ReadonlyArray<import('./GrannySkeleton.js').Skeleton>,
 *   meshes: ReadonlyArray<import('./GrannyMesh.js').MeshGeometry>,
 * }} ParseModelResult
 */

/**
 * Result of {@link parseTextured} : {@link ParseModelResult} + texture extraction.
 *
 * @typedef {ParseModelResult & {
 *   textures: ReadonlyArray<import('./GrannyTexture.js').TextureRecord>,
 * }} ParseTexturedResult
 */

/**
 * Result of {@link parseAnimated} : {@link ParseModelResult} + animation extraction.
 *
 * @typedef {ParseModelResult & {
 *   animations: ReadonlyArray<import('./GrannyAnimation.js').Animation>,
 * }} ParseAnimatedResult
 */

/**
 * Result of {@link parseAll} : {@link ParseTexturedResult} + animation + model
 * extraction — the union every high-level consumer needs from a single
 * `loadGR2`. `models[0].initialPlacement` is the InitialPlacement Transform a
 * caller previously reached only via the low-level graph.
 *
 * @typedef {ParseTexturedResult & {
 *   animations: ReadonlyArray<import('./GrannyAnimation.js').Animation>,
 *   models: import('./GrannyModel.js').ModelInfo[],
 * }} ParseAllResult
 */

/**
 * Combined options for {@link parseModel} (forwarded to skeleton + mesh extractors).
 *
 * @typedef {import('./GrannySkeleton.js').ExtractSkeletonsOptions
 *   & import('./GrannyMesh.js').ExtractMeshesOptions} ParseModelOptions
 */

/**
 * Combined options for {@link parseTextured} (skeleton + mesh + texture extractors).
 *
 * @typedef {import('./GrannySkeleton.js').ExtractSkeletonsOptions
 *   & import('./GrannyMesh.js').ExtractMeshesOptions
 *   & import('./GrannyTexture.js').ExtractTexturesOptions} ParseTexturedOptions
 */

/**
 * Combined options for {@link parseAnimated} (skeleton + mesh + animation extractors).
 *
 * @typedef {import('./GrannySkeleton.js').ExtractSkeletonsOptions
 *   & import('./GrannyMesh.js').ExtractMeshesOptions
 *   & import('./GrannyAnimation.js').ExtractAnimationsOptions} ParseAnimatedOptions
 */

/**
 * Combined options for {@link parseAll} (skeleton + mesh + texture + animation +
 * model extractors). The model extractor's options are inline (`maxModels`,
 * `maxBindings`) — it has no exported Options typedef.
 *
 * @typedef {import('./GrannySkeleton.js').ExtractSkeletonsOptions
 *   & import('./GrannyMesh.js').ExtractMeshesOptions
 *   & import('./GrannyTexture.js').ExtractTexturesOptions
 *   & import('./GrannyAnimation.js').ExtractAnimationsOptions
 *   & { maxModels?: number, maxBindings?: number }} ParseAllOptions
 */

// --- async init seam ---------------------------------------------------

let _readyPromise = null;

/**
 * Idempotent async init seam. In the default JS-only build `initKernels()`
 * resolves immediately — there is nothing to instantiate. The opt-in WASM
 * build (`./wasm`) swaps the kernel seam so `initKernels()` here awaits
 * `WebAssembly.instantiate` (browser main-thread instantiate is async and
 * capped at 4 KB synchronously). Instantiation is lazy — deferred to the first
 * `ready()` call and cached — so importing `Granny` never compiles wasm. Await
 * it once at startup ; decode calls stay synchronous afterward.
 *
 * @returns {Promise<void>}
 */
export function ready() {
    return (_readyPromise ??= initKernels());
}

/**
 * Namespace facade — intentionally minimal (`ready` only) so that importing
 * `Granny` never pulls the texture / IGC graph into a bundle, preserving
 * tree-shaking and the `./split` code-split. Use the flat named exports
 * (`parseModel`, `parseTextured`, …) for the decode API.
 *
 * @type {{ readonly ready: typeof ready }}
 */
export const Granny = { ready };

/**
 * Decompress a single section, dispatching by `section.compression`.
 *
 * Supported : `COMPRESSION_NONE` (0) and `COMPRESSION_OODLE0` (1).
 * `COMPRESSION_OODLE1` (2), `COMPRESSION_BITKNIT` (3), `COMPRESSION_BITKNIT2` (4)
 * throw — none of the iRO ver12 corpus uses them ; codec ports will land
 * in a later session if an asset ever needs them.
 *
 * Empty sections (`expanded_size === 0`) short-circuit to an empty
 * Uint8Array without dispatching.
 *
 * @param {DispatchSectionInput} section — codec inputs + `compression` tag.
 * @param {Uint8Array} compressed — bytes from `file.sectionBytes(section)`.
 * @returns {Uint8Array} decompressed bytes of length `section.expanded_size`.
 * @throws {RangeError} when NoCompression input is shorter than `expanded_size`.
 * @throws {Error} on an unsupported compression tag.
 */
export function decompressSection(section, compressed) {
    if (section.expanded_size === 0) return new Uint8Array(0);
    if (section.compression === COMPRESSION_NONE) {
        if (compressed.length < section.expanded_size) {
            throw new RangeError(
                `section ${section.index} short: ${compressed.length} < ${section.expanded_size}`
            );
        }
        return compressed.subarray(0, section.expanded_size);
    }
    if (section.compression === COMPRESSION_OODLE0) {
        return decompressOodle0(section, compressed);
    }
    throw new Error(`unsupported compression ${section.compression}`);
}

/**
 * Full pipeline : parse the GR2 file, decompress + fixup all sections,
 * walk the root type tree, materialize the root object.
 *
 * After this returns, `result.root` is a plain JS graph keyed by member
 * name (`root.Meshes`, `root.Skeletons`, …) ready for downstream
 * skeleton / mesh extraction to navigate without touching binary again.
 *
 * @param {import('./GrannyFile.js').GR2Input} buffer — the .gr2 bytes.
 * @returns {ParseResult}
 * @throws {Error} on non-Granny input, unsupported compression, or
 *   cross-endian asset with non-empty mixed-marshalling table.
 * @throws {RangeError} when a declared section / fixup table escapes the buffer.
 */
export function parse(buffer) {
    const file = parseGR2File(buffer);
    const loaded = loadGR2(file);
    const typeTree = parseTypeTree(loaded, file.header.root_type);
    const root = parseObject(loaded, typeTree, file.header.root_object);
    return { file, typeTree, root };
}

/**
 * Full model pipeline : `parse(buffer)` + skeleton + mesh extraction.
 * On any of the 6 model fixtures, `result.skeletons` carries the bind-
 * pose bones (with local Transforms + InverseWorldTransform) and
 * `result.meshes` carries decoded vertex / index / weight / material
 * buffers — ready for the renderer without any further binary parsing.
 *
 * Re-runs the pipeline locally (does not call {@link parse}) so the
 * lean `parse()` return shape stays unchanged for callers that don't
 * need the multi-MB `loaded` buffers.
 *
 * Animation-only fixtures resolve to empty arrays for both `skeletons`
 * and `meshes` — same throw / error surface as {@link parse}.
 *
 * @param {import('./GrannyFile.js').GR2Input} buffer — the .gr2 bytes.
 * @param {ParseModelOptions} [options]
 * @returns {ParseModelResult}
 */
export function parseModel(buffer, options = {}) {
    const file = parseGR2File(buffer);
    const loaded = loadGR2(file);
    const typeTree = parseTypeTree(loaded, file.header.root_type);
    const root = parseObject(loaded, typeTree, file.header.root_object);
    const skeletons = extractSkeletons(loaded, options);
    const meshes = extractMeshes(loaded, options);
    return { file, typeTree, root, skeletons, meshes };
}

/**
 * Full animated-asset pipeline : `parseModel(buffer)` + animation
 * extraction. On the 15 animation-only fixtures, `result.animations`
 * carries every `Animation` with its resolved `TrackGroup`s + decoded
 * `TransformTrack`s ; call `evaluateTransformTrack(track, t)` (or
 * `evaluateAnimation(anim, t)`) to sample local-Transform values at any
 * point in time, ready for S8 pose composition.
 *
 * Pure model fixtures resolve to `animations = []` (they carry skeleton
 * + mesh but no curve data). Mirrors {@link parseModel}'s contract :
 * re-runs the lower-level pipeline locally so {@link parse} stays lean.
 *
 * @param {import('./GrannyFile.js').GR2Input} buffer — the .gr2 bytes.
 * @param {ParseAnimatedOptions} [options]
 * @returns {ParseAnimatedResult}
 */
export function parseAnimated(buffer, options = {}) {
    const file = parseGR2File(buffer);
    const loaded = loadGR2(file);
    const typeTree = parseTypeTree(loaded, file.header.root_type);
    const root = parseObject(loaded, typeTree, file.header.root_object);
    const skeletons = extractSkeletons(loaded, options);
    const meshes = extractMeshes(loaded, options);
    const animations = extractAnimations(loaded, options);
    return { file, typeTree, root, skeletons, meshes, animations };
}

/**
 * Full textured-model pipeline : `parseModel(buffer)` + texture
 * extraction. On any model fixture with textures, `result.textures`
 * carries each decoded `(texture, image, MIP)` as RGBA8888 ready for
 * upload to the GPU. Animation-only fixtures resolve `result.textures`
 * to `[]`.
 *
 * Re-runs the lower-level pipeline locally (does not call
 * {@link parseModel}) so the lean shape stays consistent with the other
 * pipeline entries.
 *
 * @param {import('./GrannyFile.js').GR2Input} buffer — the .gr2 bytes.
 * @param {ParseTexturedOptions} [options]
 * @returns {ParseTexturedResult}
 * @throws {Error} on textures with `encoding=2` (S3TC, not in iRO corpus).
 * @throws {Error} on an IGC (`encoding=3`) decode in the code-split (`./split`)
 *   build when `await loadTextureCodec()` has not resolved yet — the default
 *   build decodes IGC synchronously with no warmup.
 */
export function parseTextured(buffer, options = {}) {
    const file = parseGR2File(buffer);
    const loaded = loadGR2(file);
    const typeTree = parseTypeTree(loaded, file.header.root_type);
    const root = parseObject(loaded, typeTree, file.header.root_object);
    const skeletons = extractSkeletons(loaded, options);
    const meshes = extractMeshes(loaded, options);
    const textures = extractTextures(loaded, options);
    return { file, typeTree, root, skeletons, meshes, textures };
}

/**
 * Single-pass pipeline : one `parseGR2File`→`loadGR2`, then every extractor on
 * that single decompressed graph — skeleton + mesh + texture + animation +
 * model. Returns the superset of {@link parseTextured} and {@link parseAnimated}
 * plus `models` (each with `initialPlacement`), so a consumer that needs
 * textured + animated + model(InitialPlacement) pays the expensive Oodle0
 * decompress (`loadGR2`) **once** instead of three times.
 *
 * Additive — {@link parseTextured} / {@link parseAnimated} are unchanged. On an
 * animation-only fixture, `meshes` / `skeletons` / `textures` / `models` resolve
 * to empty arrays and `animations` carries the curves ; on a pure model fixture
 * the reverse. Same throw / error surface as {@link parse}.
 *
 * @param {import('./GrannyFile.js').GR2Input} buffer — the .gr2 bytes.
 * @param {ParseAllOptions} [options]
 * @returns {ParseAllResult}
 */
export function parseAll(buffer, options = {}) {
    const file = parseGR2File(buffer);
    const loaded = loadGR2(file);
    const typeTree = parseTypeTree(loaded, file.header.root_type);
    const root = parseObject(loaded, typeTree, file.header.root_object);
    const skeletons = extractSkeletons(loaded, options);
    const meshes = extractMeshes(loaded, options);
    const textures = extractTextures(loaded, options);
    const animations = extractAnimations(loaded, options);
    const models = extractModels(loaded, options);
    return { file, typeTree, root, skeletons, meshes, textures, animations, models };
}

/**
 * Sample the first skeleton of `parsed` at time `t` against
 * `parsed.animations[animationIndex]`. Returns a {@link PoseSnapshot}
 * with per-bone local Transforms + world matrices + skinning matrices
 * (column-major Float32Array(16), GPU-ready).
 *
 * The iRO ver12 layout keeps model + animations in separate `.gr2`
 * files ; callers typically graft an animation array onto a parsed
 * model before calling `poseAt`. Pure model fixtures (no animation
 * available) accept `animationIndex` outside the available range — the
 * pose collapses to the bind pose. The `animations` array can also be
 * grafted from a sibling `parseAnimated` call when the animation lives
 * in a separate `.gr2` (model + N animations keyed by mob ID).
 *
 * @param {ParseModelResult | ParseAnimatedResult} parsed — a `parseModel` /
 *   `parseAnimated` result (optionally with a grafted `animations` array).
 * @param {number} animationIndex — index into `parsed.animations`.
 * @param {number} t — sample time.
 * @returns {import('./GrannyPose.js').PoseSnapshot}
 */
export function poseAt(parsed, animationIndex, t) {
    const skeleton = parsed.skeletons?.[0];
    if (!skeleton) {
        throw new Error('poseAt: parsed.skeletons[0] is missing');
    }
    const animations = /** @type {ParseAnimatedResult} */ (parsed).animations ?? [];
    const animation = animations[animationIndex] ?? null;
    return poseSkeletonAt(skeleton, animation, t);
}
