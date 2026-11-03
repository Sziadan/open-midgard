// GrannyModel.js — Model extraction from a loaded GR2.
//
// A `Model` is the canonical Granny "instance" — it binds a Skeleton
// to a list of Meshes with an initial world placement. From the SDK :
//   typedef struct GrannyModel {
//       char const            *Name;
//       struct GrannySkeleton *Skeleton;
//       GrannyTransform        InitialPlacement;
//       GrannyModelMeshBinding MeshBindings[];
//   };
// Each `GrannyModelMeshBinding` carries a `GrannyMesh *` (and, in newer
// SDKs, a `ToBoneIndices` array — absent on the iRO ver12 corpus, where
// skinning falls back to the model's Skeleton bone-table).
//
// Public surface : `extractModels(loaded, options)` returning a flat
// array of `ModelInfo`. Indices into the `Skeleton` / `Mesh` lists
// produced by `extractSkeletons` / `extractMeshes` are resolved by
// matching the on-disk pointer targets — consumers can cross-reference
// without re-walking the type tree.

import { parseObject, parseTypeTree, readReferenceArrayObjects } from './GrannyTypeTree.js';
import { IDENTITY_TRANSFORM, readTransform } from './GrannyTransform.js';

/**
 * One binding of a Mesh to a Model. Indices into the array returned by
 * `extractMeshes(loaded)`. The skinning bone-table is inherited from the
 * Model's `skeletonIdx` ; per-binding `ToBoneIndices` is supported by
 * newer Granny SDK versions but absent in the iRO ver12 corpus and
 * therefore not surfaced here.
 *
 * @typedef {object} ModelMeshBinding
 * @property {number} meshIdx — index into `extractMeshes(loaded)` ; -1 if the
 *   on-disk pointer doesn't resolve to any extracted mesh (defensive fallback).
 */

/**
 * Materialized `GrannyModel` — the canonical instance struct that binds
 * a `GrannySkeleton` to a list of `GrannyMesh` with an initial world
 * placement Transform. Returned in order matching `root.Models.element_refs`.
 *
 * @typedef {object} ModelInfo
 * @property {string} name — Model.Name, or `Model_<i>` when the name string is empty.
 * @property {number} skeletonIdx — index into `extractSkeletons(loaded)` ; -1 when
 *   the on-disk pointer doesn't resolve.
 * @property {import('./GrannyTransform.js').Transform} initialPlacement — 68-byte
 *   Granny Transform decoded into `{ flags, position, orientation, scaleShear }`.
 *   Identity when the address falls outside the section.
 * @property {ModelMeshBinding[]} meshBindings — one entry per
 *   `GrannyModelMeshBinding` in the model.
 */

/**
 * Build a `{ "section:offset" → index }` lookup from a `root.<Key>` field
 * (an `MT_ARRAY_OF_REFERENCES` exposed by `parseObject`). Lets us resolve
 * a Model's Skeleton ref / MeshBinding.Mesh ref to a stable index into
 * the corresponding extractor output without re-walking the type tree.
 */
function buildRefIndex(field) {
    const out = {};
    const refs = field?.element_refs ?? [];
    for (let i = 0; i < refs.length; i++) {
        const r = refs[i];
        if (r) out[`${r.section}:${r.offset}`] = i;
    }
    return out;
}

/**
 * Walk `root.Models` and return every model with its skeleton index, its
 * initial placement Transform, and its mesh-binding list. Returns `[]`
 * for fixtures that don't carry any model (animation-only files).
 *
 * @param {import('./GrannyTypeTree.js').LoadedGR2} loaded — output of `loadGR2(file)`.
 * @param {{ maxModels?: number, maxBindings?: number }} [options]
 * @returns {ModelInfo[]}
 */
export function extractModels(loaded, options = {}) {
    const maxModels = options.maxModels ?? 64;
    const maxBindings = options.maxBindings ?? 256;

    const file = loaded.file;
    const root = parseObject(
        loaded,
        parseTypeTree(loaded, file.header.root_type),
        file.header.root_object,
    );

    const modelField = root.Models;
    if (!modelField) return [];
    const modelTypeRef = modelField.reference_type ?? null;
    const elementRefs = modelField.element_refs ?? [];
    if (modelTypeRef === null || elementRefs.length === 0) return [];

    const skeletonRefIndex = buildRefIndex(root.Skeletons);
    const meshRefIndex = buildRefIndex(root.Meshes);

    const modelTT = parseTypeTree(loaded, [modelTypeRef.section, modelTypeRef.offset]);
    const limit = elementRefs.length < maxModels ? elementRefs.length : maxModels;
    const out = new Array(limit);
    let n = 0;

    for (let i = 0; i < limit; i++) {
        const mref = elementRefs[i];
        if (!mref) continue;
        const fields = parseObject(loaded, modelTT, [mref.section, mref.offset]);

        const nameValue = fields.Name?.value;
        const name = typeof nameValue === 'string' && nameValue ? nameValue : `Model_${n}`;

        const skelTarget = fields.Skeleton?.target ?? null;
        const skeletonIdx = skelTarget
            ? skeletonRefIndex[`${skelTarget.section}:${skelTarget.offset}`] ?? -1
            : -1;

        const placement = fields.InitialPlacement?.value
            ?? readTransform(loaded, mref.section, fields.InitialPlacement?.offset ?? 0)
            ?? IDENTITY_TRANSFORM;

        const meshBindings = readModelMeshBindings(loaded, fields, meshRefIndex, maxBindings);

        out[n++] = {
            name,
            skeletonIdx,
            initialPlacement: placement,
            meshBindings,
        };
    }
    out.length = n;
    return out;
}

/**
 * Walk a Model's `MeshBindings` array and resolve each binding's `Mesh`
 * reference to an index into `root.Meshes`. Returns `[]` on a model with
 * no bindings or when the array reference is null.
 */
function readModelMeshBindings(loaded, modelFields, meshRefIndex, maxBindings) {
    const mbField = modelFields.MeshBindings;
    if (!mbField) return [];
    const mbTypeRef = mbField.reference_type ?? null;
    const mbTarget = mbField.target ?? null;
    const mbCount = mbField.count ?? 0;
    if (mbTypeRef === null || mbTarget === null || mbCount <= 0) return [];

    const mbObjects = readReferenceArrayObjects(
        loaded,
        mbTarget,
        mbCount,
        mbTypeRef,
        { maxCount: maxBindings },
    );
    const out = new Array(mbObjects.length);
    let n = 0;
    for (let i = 0; i < mbObjects.length; i++) {
        const f = mbObjects[i].fields;
        const meshTarget = f.Mesh?.target ?? null;
        const meshIdx = meshTarget
            ? meshRefIndex[`${meshTarget.section}:${meshTarget.offset}`] ?? -1
            : -1;
        out[n++] = { meshIdx };
    }
    out.length = n;
    return out;
}
