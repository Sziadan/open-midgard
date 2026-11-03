// GrannyMesh.js — Mesh + vertex layout + bone-binding + material extraction.
//
// JS port of Rasetsuu/blendergranny io_scene_gr2/gr2/geometry.py with the
// `material_map` helper from material.py inlined as private functions
// (S6 prompt deliverable surface lists only 2 files ; material handling
// is consumed solely by mesh extraction so it lives here).
//
// Public surface : a single `extractMeshes(loaded, options)` returning an
// array of `MeshGeometry`. Each mesh carries its decoded vertex layout
// (component-by-component offsets), unpacked Position / Normal / UV
// arrays, indices, per-vertex bone weights, material refs, and triangle
// groups — ready for the renderer to skin and draw.
//
// Animation-only fixtures (no `root.Meshes` ; or meshes with zero
// vertices) resolve to `[]`.
//
// Pre-condition : `loaded` must come from `GrannyTypeTree.loadGR2(file)`.

import {
    MT_BINORMAL_INT8,
    MT_BINORMAL_INT16,
    MT_INT8,
    MT_INT16,
    MT_INT32,
    MT_NORMAL_UINT8,
    MT_NORMAL_UINT16,
    MT_REAL32,
    MT_UINT8,
    MT_UINT16,
    MT_UINT32,
    parseObject,
    parseTypeTree,
    readReferenceArrayObjects,
} from './GrannyTypeTree.js';

/**
 * One declared component of a vertex row (Position, Normal, BoneWeights, …).
 *
 * @typedef {object} VertexComponent
 * @property {string} name — component name from the Granny vertex type tree.
 * @property {number} offset — byte offset within one vertex row at which this component starts.
 * @property {number} width — declared element width (typically 3 for Position, 2 for UV, 4 for weights).
 * @property {import('./GrannyTypeTree.js').MemberTypeConstant} memberType — raw `MT_*` enum value the component is stored as.
 */

/**
 * A skeleton-bone reference a mesh declares it depends on (skinning slot).
 *
 * @typedef {object} BoneBinding
 * @property {number} index — 0-based slot index within the mesh's `boneBindings` array.
 * @property {string} name — bone name (matches a `SkeletonBone.name` in the model's skeleton).
 */

/**
 * One per-vertex (bone, weight) entry for skinning.
 *
 * @typedef {object} VertexBoneWeight
 * @property {number} boneIndex — index into the mesh's `boneBindings` array (NOT a global bone index).
 * @property {number} weight — skinning weight in `[0, 1]` (already normalized for normal-uint encodings).
 */

/**
 * One material-bucketed triangle range within the mesh.
 *
 * @typedef {object} MeshTriangleGroup
 * @property {number} materialIndex — index into the mesh's `materials` array.
 * @property {number} triFirst — first triangle of the batch (index into the mesh's triangle list).
 * @property {number} triCount — number of triangles in the batch.
 */

/**
 * Material metadata + the first associated texture (file name + size).
 *
 * @typedef {object} MaterialInfo
 * @property {number} index — 0-based index in the GR2 file's Materials array.
 * @property {string} name — material name (defaults to `Material_<index>` if missing).
 * @property {string} textureFile — texture file name as authored (empty when no Texture / Maps).
 * @property {readonly [number, number] | null} textureSize — `[width, height]` in pixels when the
 *   Texture sub-object declares them ; `null` otherwise.
 */

/**
 * A fully decoded mesh ready for the renderer.
 *
 * @typedef {object} MeshGeometry
 * @property {string} name — mesh name (defaults to `Mesh_<index>` if missing).
 * @property {number} vertexCount — number of vertices in the vertex buffer.
 * @property {number} indexCount — number of indices (always divisible by 3 for triangle meshes).
 * @property {number} vertexStride — bytes per vertex row in the source vertex buffer.
 * @property {readonly VertexComponent[]} components — declared vertex layout components in source order.
 * @property {ReadonlyArray<readonly number[]>} positions — per-vertex `[x, y, z]` positions.
 * @property {ReadonlyArray<readonly number[]>} normals — per-vertex `[x, y, z]` normals.
 * @property {ReadonlyArray<readonly number[]>} uvs — per-vertex `[u, v]` texture coordinates.
 * @property {readonly number[]} indices — index buffer (16- or 32-bit indices, decoded to JS numbers).
 * @property {readonly BoneBinding[]} boneBindings — mesh-local bone binding table.
 * @property {ReadonlyArray<readonly VertexBoneWeight[]>} vertexWeights — per-vertex bone-weight list.
 * @property {readonly MaterialInfo[]} materials — material references the mesh declares it uses.
 * @property {readonly MeshTriangleGroup[]} triangleGroups — triangle batches grouped by material.
 */

/**
 * Options for {@link extractMeshes}.
 *
 * @typedef {object} ExtractMeshesOptions
 * @property {number} [maxMeshes] - cap on the number of meshes extracted (default 32).
 * @property {number} [maxMaterials] - cap on the number of materials looked up (default 4096).
 * @property {number} [maxBones] - cap on the number of bone / material bindings per mesh (default 4096).
 */

/**
 * Options for {@link extractMaterials}.
 *
 * @typedef {object} ExtractMaterialsOptions
 * @property {number} [maxMaterials] - cap on the number of materials extracted (default 4096).
 */

// --- extractMeshes ----------------------------------------------------

/**
 * Walk `root.Meshes` and decode every static mesh into a renderable
 * `MeshGeometry`. Returns `[]` for fixtures without any mesh (animation-
 * only files in the iRO corpus) and skips meshes whose vertex buffer is
 * unparseable (null ref / zero count / vertex type missing).
 *
 * @param {import('./GrannyTypeTree.js').LoadedGR2} loaded
 * @param {ExtractMeshesOptions} [options]
 * @returns {readonly MeshGeometry[]}
 */
export function extractMeshes(loaded, options = {}) {
    const maxMeshes = options.maxMeshes ?? 32;
    const maxMaterials = options.maxMaterials ?? 4096;
    const maxBones = options.maxBones ?? 4096;

    const meshSummaries = summarizeMeshes(loaded, maxMeshes);
    if (meshSummaries.length === 0) return [];

    const materialsByRef = materialMap(loaded, maxMaterials);

    const meshes = [];
    const summaryCount = meshSummaries.length;
    for (let i = 0; i < summaryCount; i++) {
        const summary = meshSummaries[i];
        const vertexData = summary.primaryVertexData;
        const topology = summary.primaryTopology;
        if (!vertexData) continue;

        const vertices = vertexData.Vertices;
        if (!vertices) continue;
        const vertexRef = vertices.target ?? null;
        const vertexTypeRef = vertices.variant_type ?? null;
        const vertexCount = vertices.count ?? 0;
        if (!vertexRef || !vertexTypeRef || vertexCount <= 0) continue;

        const typeMembers = parseTypeTree(loaded, [vertexTypeRef.section, vertexTypeRef.offset]);
        const components = vertexComponents(typeMembers);
        const stride = vertexStride(components);
        const vertexBytes = readBytes(loaded, vertexRef.section, vertexRef.offset, vertexCount * stride);
        if (vertexBytes === null) continue;

        const indexEntry = topology ? bestIndexField(topology) : null;
        const indexField = indexEntry ? indexEntry.field : null;
        const indexFieldName = indexEntry ? indexEntry.name : null;
        const indexRef = indexField?.target ?? null;
        const indexCount = indexField?.count ?? 0;
        const indexMemberType = indexMemberTypeFor(indexFieldName, indexField, MT_UINT16);
        const indices = readIndices(loaded, indexRef, indexCount, indexMemberType);

        const boneBindings = readBoneBindings(loaded, summary.fields, maxBones);
        const materials = readMaterialBindings(loaded, summary.fields, materialsByRef, maxBones);
        const triangleGroups = topology ? readTriangleGroups(loaded, topology) : [];

        const positions = readFloatComponent(vertexBytes, vertexCount, stride, components, 'Position', 3, false);
        const normals = readFloatComponent(vertexBytes, vertexCount, stride, components, 'Normal', 3, false);
        const uvs = readFloatComponent(vertexBytes, vertexCount, stride, components, 'TextureCoordinates', 2, true);
        const vertexWeights = readVertexWeights(vertexBytes, vertexCount, stride, components, boneBindings.length);

        meshes.push(/** @type {MeshGeometry} */ ({
            name: summary.name,
            vertexCount,
            indexCount: indices.length,
            vertexStride: stride,
            components,
            positions,
            normals,
            uvs,
            indices,
            boneBindings,
            vertexWeights,
            materials,
            triangleGroups,
        }));
    }
    return meshes;
}

// --- mesh summary (port of types.summarize_meshes) --------------------

/**
 * Walk `root.Meshes` and, for each mesh, materialize the mesh struct +
 * hydrate `PrimaryVertexData` and `PrimaryTopology` sub-objects. The
 * caller doesn't have to navigate the typetree manually.
 */
function summarizeMeshes(loaded, maxMeshes) {
    const file = loaded.file;
    const rootTypeTree = parseTypeTree(loaded, file.header.root_type);
    let meshMember = null;
    for (let i = 0; i < rootTypeTree.length; i++) {
        if (rootTypeTree[i].name === 'Meshes') {
            meshMember = rootTypeTree[i];
            break;
        }
    }
    if (!meshMember || !meshMember.referenceType) return [];

    const root = parseObject(loaded, rootTypeTree, file.header.root_object, { maxArrayRefs: maxMeshes });
    const meshField = root.Meshes;
    const elementRefs = meshField?.element_refs ?? [];
    if (elementRefs.length === 0) return [];

    const meshTypeRef = meshMember.referenceType;
    const meshTypeMembers = parseTypeTree(loaded, meshTypeRef);
    const meshTypeMap = {};
    for (let i = 0; i < meshTypeMembers.length; i++) {
        meshTypeMap[meshTypeMembers[i].name] = meshTypeMembers[i];
    }

    const limit = elementRefs.length < maxMeshes ? elementRefs.length : maxMeshes;
    const meshes = new Array(limit);
    let n = 0;
    for (let i = 0; i < limit; i++) {
        const meshRef = elementRefs[i];
        if (!meshRef) continue;
        const meshFields = parseObject(loaded, meshTypeMembers, [meshRef.section, meshRef.offset]);
        const nameValue = meshFields.Name?.value;
        const summary = {
            index: i,
            name: typeof nameValue === 'string' && nameValue ? nameValue : `Mesh_${i}`,
            ref: meshRef,
            fields: meshFields,
            primaryVertexData: null,
            primaryTopology: null,
        };
        hydrateSub(loaded, summary, meshFields, meshTypeMap, 'PrimaryVertexData', 'primaryVertexData');
        hydrateSub(loaded, summary, meshFields, meshTypeMap, 'PrimaryTopology', 'primaryTopology');
        meshes[n++] = summary;
    }
    meshes.length = n;
    return meshes;
}

function hydrateSub(loaded, summary, meshFields, meshTypeMap, fieldName, outputName) {
    const target = meshFields[fieldName]?.target;
    const member = meshTypeMap[fieldName];
    if (!target || !member || !member.referenceType) return;
    const subMembers = parseTypeTree(loaded, member.referenceType);
    summary[outputName] = parseObject(loaded, subMembers, [target.section, target.offset]);
}

// --- vertex component layout (port of _vertex_components / _vertex_stride) ---

function vertexComponents(members) {
    const out = new Array(members.length);
    let offset = 0;
    for (let i = 0; i < members.length; i++) {
        const member = members[i];
        const size = componentSize(member);
        out[i] = {
            name: member.name,
            offset,
            width: member.arrayWidth || 1,
            memberType: member.memberType,
        };
        offset += size;
    }
    return out;
}

function vertexStride(components) {
    if (components.length === 0) return 0;
    let max = 0;
    for (let i = 0; i < components.length; i++) {
        const end = components[i].offset + componentStorageSize(components[i]);
        if (end > max) max = end;
    }
    return max;
}

function componentSize(member) {
    const width = member.arrayWidth || 1;
    const t = member.memberType;
    if (t === MT_REAL32) return 4 * width;
    if (t === MT_INT32 || t === MT_UINT32) return 4 * width;
    if (t === MT_INT16 || t === MT_UINT16 || t === MT_BINORMAL_INT16 || t === MT_NORMAL_UINT16) return 2 * width;
    if (t === MT_INT8 || t === MT_UINT8 || t === MT_BINORMAL_INT8 || t === MT_NORMAL_UINT8) return width;
    return 4 * width;
}

function componentStorageSize(component) {
    const width = component.width;
    const t = component.memberType;
    if (t === MT_REAL32) return 4 * width;
    if (t === MT_INT32 || t === MT_UINT32) return 4 * width;
    if (t === MT_INT16 || t === MT_UINT16 || t === MT_BINORMAL_INT16 || t === MT_NORMAL_UINT16) return 2 * width;
    if (t === MT_INT8 || t === MT_UINT8 || t === MT_BINORMAL_INT8 || t === MT_NORMAL_UINT8) return width;
    return 4 * width;
}

// --- per-component readers (Position / Normal / UV / weights) ---------

function readFloatComponent(data, vertexCount, stride, components, name, width, prefix) {
    let component = null;
    for (let i = 0; i < components.length; i++) {
        const c = components[i];
        if (c.memberType !== MT_REAL32) continue;
        if (c.width < width) continue;
        if (prefix ? c.name.startsWith(name) : c.name === name) {
            component = c;
            break;
        }
    }
    if (component === null) return [];
    const view = new DataView(data.buffer, data.byteOffset, data.byteLength);
    const dataLen = data.length;
    const componentOffset = component.offset;
    const elementBytes = width * 4;
    const out = [];
    for (let v = 0; v < vertexCount; v++) {
        const base = v * stride + componentOffset;
        if (base + elementBytes > dataLen) break;
        if (width === 3) {
            out.push([view.getFloat32(base, true), view.getFloat32(base + 4, true), view.getFloat32(base + 8, true)]);
        } else if (width === 2) {
            out.push([view.getFloat32(base, true), view.getFloat32(base + 4, true)]);
        } else {
            const tuple = new Array(width);
            for (let k = 0; k < width; k++) {
                tuple[k] = view.getFloat32(base + k * 4, true);
            }
            out.push(tuple);
        }
    }
    return out;
}

function readIndices(loaded, ref, count, memberType) {
    if (!ref || count <= 0) return [];
    if (memberType === MT_INT32 || memberType === MT_UINT32) {
        const bytes = readBytes(loaded, ref.section, ref.offset, count * 4);
        if (bytes === null) return [];
        const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
        const out = new Array(count);
        for (let i = 0; i < count; i++) {
            out[i] = view.getUint32(i * 4, true);
        }
        return out;
    }
    const bytes = readBytes(loaded, ref.section, ref.offset, count * 2);
    if (bytes === null) return [];
    const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
    const out = new Array(count);
    for (let i = 0; i < count; i++) {
        out[i] = view.getUint16(i * 2, true);
    }
    return out;
}

function readVertexWeights(data, vertexCount, stride, components, bindingCount) {
    let weightsComponent = null;
    let indicesComponent = null;
    for (let i = 0; i < components.length; i++) {
        if (components[i].name === 'BoneWeights') weightsComponent = components[i];
        else if (components[i].name === 'BoneIndices') indicesComponent = components[i];
    }
    if (weightsComponent === null || indicesComponent === null || bindingCount <= 0) return [];

    const width = weightsComponent.width < indicesComponent.width
        ? weightsComponent.width
        : indicesComponent.width;
    const view = new DataView(data.buffer, data.byteOffset, data.byteLength);
    const dataLen = data.length;
    const out = new Array(vertexCount);
    const weightsOffset = weightsComponent.offset;
    const indicesOffset = indicesComponent.offset;
    for (let v = 0; v < vertexCount; v++) {
        const base = v * stride;
        const perVertex = [];
        for (let lane = 0; lane < width; lane++) {
            const weight = readWeightValue(view, dataLen, base + weightsOffset, lane, weightsComponent);
            const boneIndex = readIntValue(view, dataLen, base + indicesOffset, lane, indicesComponent);
            if (weight <= 0.0 || boneIndex < 0 || boneIndex >= bindingCount) continue;
            perVertex.push({ boneIndex, weight });
        }
        out[v] = perVertex;
    }
    return out;
}

function readWeightValue(view, dataLen, offset, lane, component) {
    const t = component.memberType;
    if (t === MT_REAL32) {
        const start = offset + lane * 4;
        if (start + 4 > dataLen) return 0.0;
        const value = view.getFloat32(start, true);
        if (value <= 0.0) return 0.0;
        return value > 1.0 ? 1.0 : value;
    }
    if (t === MT_UINT16 || t === MT_NORMAL_UINT16) {
        const start = offset + lane * 2;
        if (start + 2 > dataLen) return 0.0;
        const value = view.getUint16(start, true);
        return t === MT_NORMAL_UINT16 ? value / 65535.0 : value;
    }
    if (t === MT_UINT8 || t === MT_NORMAL_UINT8) {
        const start = offset + lane;
        if (start >= dataLen) return 0.0;
        const value = view.getUint8(start);
        return t === MT_NORMAL_UINT8 ? value / 255.0 : value;
    }
    return 0.0;
}

function readIntValue(view, dataLen, offset, lane, component) {
    const t = component.memberType;
    if (t === MT_INT32 || t === MT_UINT32) {
        const start = offset + lane * 4;
        if (start + 4 > dataLen) return -1;
        return t === MT_INT32 ? view.getInt32(start, true) : view.getUint32(start, true);
    }
    if (t === MT_INT16 || t === MT_UINT16) {
        const start = offset + lane * 2;
        if (start + 2 > dataLen) return -1;
        return t === MT_INT16 ? view.getInt16(start, true) : view.getUint16(start, true);
    }
    if (t === MT_INT8 || t === MT_UINT8) {
        const start = offset + lane;
        if (start >= dataLen) return -1;
        return t === MT_INT8 ? view.getInt8(start) : view.getUint8(start);
    }
    return -1;
}

// --- bone bindings + material bindings + triangle groups --------------

function readBoneBindings(loaded, meshFields, maxBones) {
    const bindingField = meshFields.BoneBindings;
    if (!bindingField) return [];
    const objects = readReferenceArrayObjects(
        loaded,
        bindingField.target ?? null,
        bindingField.count ?? 0,
        bindingField.reference_type ?? null,
        { maxCount: maxBones },
    );
    const out = new Array(objects.length);
    for (let i = 0; i < objects.length; i++) {
        const name = objects[i].fields.BoneName?.value;
        out[i] = {
            index: i,
            name: typeof name === 'string' && name ? name : `Bone_${i}`,
        };
    }
    return out;
}

function readMaterialBindings(loaded, meshFields, materialsByRef, maxBones) {
    const bindingField = meshFields.MaterialBindings;
    if (!bindingField) return [];
    const objects = readReferenceArrayObjects(
        loaded,
        bindingField.target ?? null,
        bindingField.count ?? 0,
        bindingField.reference_type ?? null,
        { maxCount: maxBones },
    );
    const out = new Array(objects.length);
    for (let i = 0; i < objects.length; i++) {
        const materialRef = objects[i].fields.Material?.target;
        const material = materialRef
            ? materialsByRef[`${materialRef.section}:${materialRef.offset}`]
            : null;
        out[i] = material ?? {
            index: i,
            name: `Material_${i}`,
            textureFile: '',
            textureSize: null,
        };
    }
    return out;
}

function readTriangleGroups(loaded, topologyFields) {
    const groupField = topologyFields.Groups;
    if (!groupField) return [];
    const objects = readReferenceArrayObjects(
        loaded,
        groupField.target ?? null,
        groupField.count ?? 0,
        groupField.reference_type ?? null,
        { maxCount: 4096 },
    );
    const out = new Array(objects.length);
    for (let i = 0; i < objects.length; i++) {
        const fields = objects[i].fields;
        out[i] = {
            materialIndex: typeof fields.MaterialIndex?.value === 'number' ? fields.MaterialIndex.value : 0,
            triFirst: typeof fields.TriFirst?.value === 'number' ? fields.TriFirst.value : 0,
            triCount: typeof fields.TriCount?.value === 'number' ? fields.TriCount.value : 0,
        };
    }
    return out;
}

// --- index format heuristic (Indices16 vs Indices) --------------------

function indexMemberTypeFor(fieldName, field, fallback) {
    if (!field) return fallback;
    if (fieldName === 'Indices') return MT_UINT32;
    return fallback;
}

function bestIndexField(topologyFields) {
    const indices16 = topologyFields.Indices16 ?? null;
    const indices32 = topologyFields.Indices ?? null;
    if (indices16 && (indices16.count ?? 0) > 0) return { name: 'Indices16', field: indices16 };
    if (indices32 && (indices32.count ?? 0) > 0) return { name: 'Indices', field: indices32 };
    if (indices16) return { name: 'Indices16', field: indices16 };
    if (indices32) return { name: 'Indices', field: indices32 };
    return null;
}

// --- material extraction (inlined port of material.py) -----------------

/**
 * Walk `root.Materials` once and produce both shapes : an ordered array
 * (one entry per top-level material, in element_refs order) and a
 * `"section:offset"` → MaterialInfo cache used by mesh binding lookups.
 * Recursion through `Maps[].Map` resolves nested material graphs ;
 * `readMaterial` memoizes via the shared cache.
 */
function walkRootMaterials(loaded, maxMaterials) {
    const file = loaded.file;
    const rootTypeTree = parseTypeTree(loaded, file.header.root_type);
    const root = parseObject(loaded, rootTypeTree, file.header.root_object, { maxArrayRefs: maxMaterials });
    const materialField = root.Materials;
    if (!materialField) return { materials: [], cache: {} };
    const materialType = materialField.reference_type ?? null;
    const elementRefs = materialField.element_refs ?? [];
    const cache = {};
    const limit = elementRefs.length < maxMaterials ? elementRefs.length : maxMaterials;
    const materials = new Array(limit);
    let n = 0;
    for (let i = 0; i < limit; i++) {
        const ref = elementRefs[i];
        if (!ref) continue;
        materials[n++] = readMaterial(loaded, ref, materialType, i, cache, {});
    }
    materials.length = n;
    return { materials, cache };
}

function materialMap(loaded, maxMaterials) {
    return walkRootMaterials(loaded, maxMaterials).cache;
}

/**
 * Public top-level Materials extractor. Returns one {@link MaterialInfo}
 * per `root.Materials` entry, in source order (so `materials[i].index === i`
 * for fixtures with no null refs). Mirrors `extractMeshes` / `extractTextures`
 * surface : pure function of `loaded`, idempotent, no side effects.
 *
 * For consumers that want material binding per mesh, use `extractMeshes` —
 * it already resolves materials per face-group via the shared cache.
 *
 * @param {import('./GrannyTypeTree.js').LoadedGR2} loaded
 * @param {ExtractMaterialsOptions} [options]
 * @returns {readonly MaterialInfo[]}
 */
export function extractMaterials(loaded, options = {}) {
    const maxMaterials = options.maxMaterials ?? 4096;
    return /** @type {readonly MaterialInfo[]} */ (walkRootMaterials(loaded, maxMaterials).materials);
}

function readMaterial(loaded, ref, materialType, index, cache, seen) {
    const key = `${ref.section}:${ref.offset}`;
    if (cache[key]) return cache[key];
    if (materialType === null || seen[key]) {
        return { index, name: `Material_${index}`, textureFile: '', textureSize: null };
    }
    const materialMembers = parseTypeTree(loaded, [materialType.section, materialType.offset]);
    const fields = parseObject(loaded, materialMembers, [ref.section, ref.offset], { maxArrayRefs: 16 });
    let texture = readTextureField(loaded, fields.Texture);
    if (texture === null) {
        texture = readFirstMapTexture(loaded, fields.Maps, materialType, cache, { ...seen, [key]: true });
    }
    const nameValue = fields.Name?.value;
    const info = {
        index,
        name: typeof nameValue === 'string' && nameValue ? nameValue : `Material_${index}`,
        textureFile: texture ? texture[0] : '',
        textureSize: texture ? texture[1] : null,
    };
    cache[key] = info;
    return info;
}

function readTextureField(loaded, textureField) {
    if (!textureField) return null;
    const target = textureField.target ?? null;
    const textureType = textureField.reference_type ?? null;
    if (!target || !textureType) return null;
    const textureMembers = parseTypeTree(loaded, [textureType.section, textureType.offset]);
    const fields = parseObject(loaded, textureMembers, [target.section, target.offset], { maxArrayRefs: 8 });
    const widthValue = fields.Width?.value;
    const heightValue = fields.Height?.value;
    const size = typeof widthValue === 'number' && typeof heightValue === 'number'
        ? [widthValue, heightValue]
        : null;
    const fileNameValue = fields.FromFileName?.value;
    return [typeof fileNameValue === 'string' ? fileNameValue : '', size];
}

function readFirstMapTexture(loaded, mapsField, materialType, cache, seen) {
    if (!mapsField) return null;
    const target = mapsField.target ?? null;
    const mapType = mapsField.reference_type ?? null;
    const maps = readReferenceArrayObjects(
        loaded,
        target,
        mapsField.count ?? 0,
        mapType,
        { maxCount: 16 },
    );
    for (let i = 0; i < maps.length; i++) {
        const mapRef = maps[i].fields.Map?.target;
        if (!mapRef) continue;
        const material = readMaterial(loaded, mapRef, materialType, -1, cache, seen);
        if (material.textureFile) {
            return [material.textureFile, material.textureSize];
        }
    }
    return null;
}

// --- low-level byte reader --------------------------------------------

/**
 * Subarray helper that's null-safe for out-of-range reads (mirrors
 * Python `loaded.read_ref(ref, size, fixed=False)` which raises ValueError
 * on OOR ; here we surface a null so the caller can skip the mesh).
 */
function readBytes(loaded, section, offset, size) {
    const data = loaded.sectionsOriginal[section];
    if (!data || offset < 0 || offset + size > data.length) return null;
    return data.subarray(offset, offset + size);
}
