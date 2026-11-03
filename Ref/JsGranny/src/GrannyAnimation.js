// GrannyAnimation.js — Animation / TrackGroup / TransformTrack walk +
// curve-codec decoder + evaluate-at-time API for granny-ro-js.
//
// JS port of Rasetsuu/blendergranny io_scene_gr2/gr2/animation.py (MIT,
// clean-room) — S7 of the granny-pipeline rollout. Layers on top of S5
// (typetree walker) + S6 (skeleton bind pose) without modifying either.
//
// Three layers, exported individually for testability :
//
//   1. extractAnimations(loaded, options)
//        → Animation[] : top-level walk.
//   2. evaluateTransformTrack(track, t)
//        → { position[3], orientation[4], scaleShear[9] } : one bone's
//          local Transform at time t.
//   3. evaluateAnimation(animation, t)
//        → { [trackName]: { position, orientation, scaleShear } } :
//          per-bone Transform map (joined to skeleton bones by name in S8).
//
// Curve codecs supported (no Oodle0 — all elementary quantization +
// bit-packing) : D3K16uC16u, D3I1K16uC16u, D4nK8uC7u, D4nK16uC15u,
// LegacyCurve32f, *Constant32f, DaIdentity. Unknown codec → empty
// curve (knots = controls = []) so the public shape stays stable.
//
// Curve evaluate semantics : linear search through knots, B-spline
// blend per `degree` (1 = linear, 2 = quadratic, ≥3 = nearest control),
// quaternion (dimension = 4) results re-normalized. `t <= knots[0]`
// clamps to first knot ; `t >= knots[n-1]` clamps to last knot.

import {
    parseTypeTree,
    parseObject,
    readReferenceArrayObjects,
    objectStorageSize,
    decodeFakePointer,
    MT_INLINE,
    MT_REFERENCE,
    MT_EMPTY_REFERENCE,
    MT_STRING,
    MT_REFERENCE_TO_ARRAY,
    MT_ARRAY_OF_REFERENCES,
    MT_VARIANT_REFERENCE,
    MT_REFERENCE_TO_VARIANT_ARRAY,
    MT_TRANSFORM,
} from './GrannyTypeTree.js';

/**
 * Supported curve format names ; mirrors blendergranny + LegacyCurve32f.
 * The `(string & {})` member keeps the literal-completion list while still
 * accepting `*Constant32f` and future codec names.
 *
 * @typedef {'D3K16uC16u' | 'D3I1K16uC16u' | 'D4nK8uC7u' | 'D4nK16uC15u'
 *   | 'LegacyCurve32f' | 'DaIdentity' | (string & {})} CurveCodec
 */

/**
 * Compressed B-spline curve. Decoded knots + controls live in `Float32Array`s ;
 * `controls` is flattened so consumers read it as
 * `controls[knotIndex * dimension + dim]`. For constant codecs (`*Constant32f`)
 * and identity codecs (`DaIdentity`), `knots` and `controls` are both empty and
 * `sampleValue` carries the fixed value (length = dimension).
 *
 * @typedef {object} Curve
 * @property {CurveCodec} codec — codec name decoded from the curve type's first member.
 * @property {number} format — Granny-encoded format byte (≥0 for modern codecs, -1 for legacy).
 * @property {number} degree — B-spline degree (0 = step, 1 = linear, 2 = quadratic, higher = nearest).
 * @property {number} dimension — per-knot value width (3 position, 4 orientation, 9 scale-shear).
 * @property {number} knotControlCount — combined knot + control count from the on-disk header.
 * @property {Float32Array} sampleValue — fallback value for constant / identity curves (length = dimension).
 * @property {Float32Array} knots — time-axis values in source order. Empty for constant / identity codecs.
 * @property {Float32Array} controls — flattened controls in `[knot * dimension + dim]` order.
 */

/**
 * One bone's local-Transform timeline. The three curves drive position (dim 3),
 * orientation (dim 4 quaternion), and scale-shear (dim 9 3×3 matrix)
 * independently. A curve is `null` only when the file omits it —
 * `evaluateTransformTrack` substitutes identity in that case.
 *
 * @typedef {object} TransformTrack
 * @property {number} index — index within the parent TrackGroup's `transformTracks` array.
 * @property {string} name — ASCII name from the GR2 file (joins to `skeleton.bones[i].name`).
 * @property {number} flags — Granny SDK flags (typically 0 — interpretation deferred).
 * @property {Curve | null} orientationCurve — quaternion orientation curve at this bone, or `null` if absent.
 * @property {Curve | null} positionCurve — position curve at this bone, or `null` if absent.
 * @property {Curve | null} scaleShearCurve — scale + shear curve at this bone, or `null` if absent.
 */

/**
 * A coherent set of TransformTracks. Multiple animations may share a single
 * TrackGroup (e.g. character idle + walk both reference the same skeleton's tracks).
 *
 * @typedef {object} TrackGroup
 * @property {number} index — index within the top-level `track_groups` array.
 * @property {string} name — track group name — animations reference it via this name.
 * @property {number} vectorTrackCount — reported count of vector tracks (values not decoded).
 * @property {number} transformTrackCount — reported count of transform tracks (must equal `transformTracks.length`).
 * @property {number} textTrackCount — reported count of text tracks (not decoded).
 * @property {number} accumulationFlags — Granny SDK accumulation flags.
 * @property {number} loopTranslation — per-loop translation offset along the dominant axis.
 * @property {readonly string[]} vectorTrackNames — vector-track names (decoded — values not yet).
 * @property {readonly TransformTrack[]} transformTracks — per-bone Transform timelines decoded for this group.
 */

/**
 * One playable animation : duration + references to TrackGroups by name.
 *
 * @typedef {object} Animation
 * @property {number} index — index within the top-level `animations` array.
 * @property {string} name — animation name (e.g. `attack`, `dead`, `move` for iRO assets).
 * @property {number} duration — total duration in seconds.
 * @property {number} timeStep — suggested per-frame time step (`0` when the file doesn't carry one).
 * @property {number} oversampling — curve-sampling oversampling factor (typically `1.0`).
 * @property {number} defaultLoopCount — default loop count (`0` one-shot, `-1` infinite by convention).
 * @property {number} flags — Granny SDK flags.
 * @property {readonly string[]} trackGroupNames — TrackGroup names referenced by this animation.
 * @property {readonly TrackGroup[]} trackGroups — TrackGroup objects resolved by name lookup (may be `[]`).
 */

/**
 * Options for {@link extractAnimations}.
 *
 * @typedef {object} ExtractAnimationsOptions
 * @property {number} [maxTrackGroups] - cap on the number of TrackGroups extracted (default 64).
 * @property {number} [maxTracksPerGroup] - cap on the number of TransformTracks per TrackGroup (default 512).
 * @property {number} [maxAnimations] - cap on the number of Animations extracted (default 64).
 */

/**
 * Local Transform produced by {@link evaluateTransformTrack} at a given time.
 *
 * @typedef {object} EvaluatedTransform
 * @property {readonly [number, number, number]} position — bone position in parent space (x, y, z).
 * @property {readonly [number, number, number, number]} orientation — bone orientation quaternion
 *   (x, y, z, w) ; re-normalized after blend.
 * @property {readonly number[]} scaleShear — bone scale + shear matrix (3×3 row-major, 9 floats).
 */

// --- constants --------------------------------------------------------

const EMPTY_F32 = new Float32Array(0);

const IDENTITY_TRANSFORM = Object.freeze({
    position: [0.0, 0.0, 0.0],
    orientation: [0.0, 0.0, 0.0, 1.0],
    scaleShear: [1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0],
});

// Scalar member sizes in bytes ; mirrors the private SCALAR_SIZES in
// GrannyTypeTree.js. Duplicated here (instead of imported) so the
// type-tree module's public surface stays unchanged (S7 « don't modify »
// constraint per rollout) — both tables would need to update together
// if Granny ever adds a new scalar.
const SCALAR_SIZES = {
    10: 4, // real32
    11: 1, // int8
    12: 1, // uint8
    13: 1, // binormal_int8
    14: 1, // normal_uint8
    15: 2, // int16
    16: 2, // uint16
    17: 2, // binormal_int16
    18: 2, // normal_uint16
    19: 4, // int32
    20: 4, // uint32
    21: 2, // real16
};

// Quaternion compression scale/offset table : 16 entries decoded from a
// u16 (4 nibbles, one per quaternion component). Verbatim from
// blendergranny animation.py:_quaternion_scale_offset_table. The MIT
// license attribution is carried in granny-ro-js/LICENSE.
const ONE_OVER_SQRT2 = 0.707106781;
const QUATERNION_SCALE_OFFSET_TABLE = [
    [ONE_OVER_SQRT2 * 2.0,  -ONE_OVER_SQRT2],
    [ONE_OVER_SQRT2 * 1.0,  -ONE_OVER_SQRT2 * 0.5],
    [ONE_OVER_SQRT2 * 0.5,  -ONE_OVER_SQRT2 * 0.75],
    [ONE_OVER_SQRT2 * 0.5,  -ONE_OVER_SQRT2 * 0.25],
    [ONE_OVER_SQRT2 * 0.5,   ONE_OVER_SQRT2 * 0.25],
    [ONE_OVER_SQRT2 * 0.25, -ONE_OVER_SQRT2 * 0.250],
    [ONE_OVER_SQRT2 * 0.25, -ONE_OVER_SQRT2 * 0.125],
    [ONE_OVER_SQRT2 * 0.25,  ONE_OVER_SQRT2 * 0.000],
    [-ONE_OVER_SQRT2 * 2.0,  ONE_OVER_SQRT2],
    [-ONE_OVER_SQRT2 * 1.0,  ONE_OVER_SQRT2 * 0.5],
    [-ONE_OVER_SQRT2 * 0.5,  ONE_OVER_SQRT2 * 0.75],
    [-ONE_OVER_SQRT2 * 0.5,  ONE_OVER_SQRT2 * 0.25],
    [-ONE_OVER_SQRT2 * 0.5, -ONE_OVER_SQRT2 * 0.25],
    [-ONE_OVER_SQRT2 * 0.25, ONE_OVER_SQRT2 * 0.250],
    [-ONE_OVER_SQRT2 * 0.25, ONE_OVER_SQRT2 * 0.125],
    [-ONE_OVER_SQRT2 * 0.25, -ONE_OVER_SQRT2 * 0.000],
];

const TEXT_DECODER = new TextDecoder('utf-8', { fatal: false });

// --- low-level binary helpers -----------------------------------------

/** Read a pointer-sized value at `offset` as a JS number. */
function readPointerAt(data, offset, pointerSize) {
    const view = new DataView(data.buffer, data.byteOffset, data.byteLength);
    if (pointerSize === 4) return view.getUint32(offset, true);
    const lo = view.getUint32(offset, true) >>> 0;
    const hi = view.getUint32(offset + 4, true) >>> 0;
    return hi * 0x100000000 + lo;
}

/** Member size in bytes within its parent struct's storage layout. */
function scalarMemberSize(member, pointerSize) {
    const width = member.arrayWidth || 1;
    const t = member.memberType;
    const scalar = SCALAR_SIZES[t];
    if (scalar !== undefined) return scalar * width;
    if (t === MT_REFERENCE || t === MT_EMPTY_REFERENCE || t === MT_STRING) return pointerSize;
    if (t === MT_REFERENCE_TO_ARRAY || t === MT_ARRAY_OF_REFERENCES) return 4 + pointerSize;
    if (t === MT_VARIANT_REFERENCE) return pointerSize * 2;
    if (t === MT_REFERENCE_TO_VARIANT_ARRAY) return pointerSize + 4 + pointerSize;
    if (t === MT_TRANSFORM) return 68;
    if (t === MT_INLINE) return 0;
    return pointerSize;
}

/**
 * Build a `name → byte offset within object` map for a parsed type tree.
 * INLINE members recurse into their sub-type to compute their full size,
 * matching `objectStorageSize` semantics. Used by the curve-metadata
 * reader to locate Format / Degree / KnotsControls / OneOverKnotScale
 * etc. within a CurveData header struct.
 */
function memberOffsetsFor(loaded, members) {
    const pointerSize = loaded.pointerSize;
    const out = Object.create(null);
    let offset = 0;
    for (let i = 0; i < members.length; i++) {
        const member = members[i];
        out[member.name] = offset;
        if (member.memberType === MT_INLINE && member.referenceType) {
            const sub = parseTypeTree(loaded, member.referenceType);
            offset += objectStorageSize(loaded, sub, pointerSize, new Set());
        } else {
            offset += scalarMemberSize(member, pointerSize);
        }
    }
    return out;
}

/** Decode a `float32` from a `u16` reinterpreted as the high 16 bits of a `u32`. */
function floatFromHighU16(value) {
    const buf = new ArrayBuffer(4);
    const view = new DataView(buf);
    view.setUint32(0, (value & 0xffff) << 16, true);
    return view.getFloat32(0, true);
}

// --- string reading ---------------------------------------------------

function looksText(bytes) {
    const len = bytes.length;
    if (len === 0) return false;
    for (let i = 0; i < len; i++) {
        const v = bytes[i];
        if (v === 9 || v === 10 || v === 13) continue;
        if (v < 32 || v === 127) return false;
    }
    return true;
}

/**
 * Read a Granny string (length-prefixed or NUL-terminated) at `(section,
 * offset)` inside `loaded.sectionsOriginal`. Returns `''` when the bytes
 * don't look like text — same heuristic as GrannyTypeTree's private
 * `readGrannyString`.
 */
function readGrannyString(loaded, section, offset, maxLength = 1024) {
    if (section >= loaded.sectionsOriginal.length) return '';
    const data = loaded.sectionsOriginal[section];
    if (offset < 0 || offset >= data.length) return '';
    if (offset + 4 <= data.length) {
        const view = new DataView(data.buffer, data.byteOffset, data.byteLength);
        const length = view.getUint32(offset, true);
        const end = offset + 4 + length;
        if (length > 0 && length <= maxLength && end <= data.length) {
            const slice = data.subarray(offset + 4, end);
            if (looksText(slice)) {
                return TEXT_DECODER.decode(slice).replace(/\0+$/, '');
            }
        }
    }
    const cap = Math.min(data.length, offset + maxLength);
    let end = cap;
    for (let i = offset; i < cap; i++) {
        if (data[i] === 0) { end = i; break; }
    }
    const slice = data.subarray(offset, end);
    if (!looksText(slice)) return '';
    return TEXT_DECODER.decode(slice);
}

/** Resolve a (possibly fake) pointer into a string. Empty on failure. */
function readStringFromPointer(loaded, pointer) {
    const ref = decodeFakePointer(pointer, loaded.sectionsOriginal.length);
    if (ref === null) return '';
    return readGrannyString(loaded, ref[0], ref[1]);
}

// --- curve codec dispatcher -------------------------------------------

/** Per-codec dimension hint : `Dn…` codecs (D3, D4, D9) carry that dim. */
function curveDimension(codec) {
    if (codec.length >= 2 && codec[0] === 'D') {
        const digit = codec.charCodeAt(1) - 48;
        if (digit >= 0 && digit <= 9) return digit;
    }
    return 0;
}

/**
 * Decode (knot_count) scales/offsets from one packed u16. Each quaternion
 * component picks one of 16 (scale, offset) pairs via 4-bit nibble index.
 */
function quaternionScalesOffsets(entries, quantizedMax) {
    const scales = new Float32Array(4);
    const offsets = new Float32Array(4);
    let bits = entries >>> 0;
    for (let i = 0; i < 4; i++) {
        const index = bits & 0xf;
        bits >>>= 4;
        const entry = QUATERNION_SCALE_OFFSET_TABLE[index];
        scales[i] = entry[0] / quantizedMax;
        offsets[i] = entry[1];
    }
    return { scales, offsets };
}

/** Resolve the `(count:u32, pointer)` at `array_offset` to its target ref. */
function readReferenceArrayPointer(loaded, fixed, offset) {
    const pointerSize = loaded.pointerSize;
    if (offset + 4 + pointerSize > fixed.length) return null;
    const pointer = readPointerAt(fixed, offset + 4, pointerSize);
    return decodeFakePointer(pointer, loaded.sectionsFixed.length);
}

/**
 * Decode the `KnotsControls` payload into a `{knots, controls}` pair of
 * `Float32Array`s. Dispatches on `codec` ; unknown codec → empty arrays.
 * `controls` is flattened (length = dimension × knot_count) so consumers
 * read it as `controls[knotIndex * dimension + dim]`.
 */
function readCurveKnotsControls(
    loaded, codec, dimension, raw, fixed, objectOffset, offsets, knotControlCount,
) {
    if (knotControlCount <= 0 || dimension <= 0) {
        return { knots: EMPTY_F32, controls: EMPTY_F32 };
    }
    const kcOffset = offsets.KnotsControls;
    if (kcOffset === undefined) return { knots: EMPTY_F32, controls: EMPTY_F32 };
    const arrayRef = readReferenceArrayPointer(loaded, fixed, objectOffset + kcOffset);
    if (arrayRef === null) return { knots: EMPTY_F32, controls: EMPTY_F32 };
    if (codec === 'D3K16uC16u' || codec === 'D3I1K16uC16u') {
        return decodeD3K16uC16uCurve(
            loaded, codec, dimension, raw, objectOffset, offsets, knotControlCount, arrayRef,
        );
    }
    if (codec === 'D4nK8uC7u') {
        return decodeD4nK8uC7uCurve(
            loaded, raw, objectOffset, offsets, knotControlCount, arrayRef,
        );
    }
    if (codec === 'D4nK16uC15u') {
        return decodeD4nK16uC15uCurve(
            loaded, raw, objectOffset, offsets, knotControlCount, arrayRef,
        );
    }
    return { knots: EMPTY_F32, controls: EMPTY_F32 };
}

/**
 * `D3K16uC16u` / `D3I1K16uC16u` : knots + per-dimension controls packed
 * as u16. Knots dequantize via `1 / OneOverKnotScaleTrunc` ; controls
 * dequantize via per-dimension `ControlScales` / `ControlOffsets`. The
 * `I1` variant carries 1 control per knot (broadcast across dimensions).
 */
function decodeD3K16uC16uCurve(
    loaded, codec, dimension, raw, objectOffset, offsets, knotControlCount, arrayRef,
) {
    const knotCount = (knotControlCount / (codec === 'D3I1K16uC16u' ? 2 : dimension + 1)) | 0;
    if (knotCount <= 0) return { knots: EMPTY_F32, controls: EMPTY_F32 };
    const oneOverOffset = objectOffset + (offsets.OneOverKnotScaleTrunc ?? 2);
    const scalesOffset = objectOffset + (offsets.ControlScales ?? 4);
    const offsetsOffset = objectOffset + (offsets.ControlOffsets ?? 16);
    if (offsetsOffset + dimension * 4 > raw.length) return { knots: EMPTY_F32, controls: EMPTY_F32 };
    const rawView = new DataView(raw.buffer, raw.byteOffset, raw.byteLength);
    const oneOver = floatFromHighU16(rawView.getUint16(oneOverOffset, true));
    if (oneOver === 0.0) return { knots: EMPTY_F32, controls: EMPTY_F32 };
    const controlScales = new Float32Array(dimension);
    const controlOffsets = new Float32Array(dimension);
    for (let d = 0; d < dimension; d++) {
        controlScales[d] = rawView.getFloat32(scalesOffset + d * 4, true);
        controlOffsets[d] = rawView.getFloat32(offsetsOffset + d * 4, true);
    }
    const arrayData = loaded.sectionsFixed[arrayRef[0]];
    const byteCount = knotControlCount * 2;
    if (arrayRef[1] + byteCount > arrayData.length) return { knots: EMPTY_F32, controls: EMPTY_F32 };
    const arrayView = new DataView(
        arrayData.buffer, arrayData.byteOffset + arrayRef[1], byteCount,
    );
    const knotScale = 1.0 / oneOver;
    const knots = new Float32Array(knotCount);
    for (let i = 0; i < knotCount; i++) {
        knots[i] = knotScale * arrayView.getUint16(i * 2, true);
    }
    const controls = new Float32Array(knotCount * dimension);
    if (codec === 'D3I1K16uC16u') {
        for (let i = 0; i < knotCount; i++) {
            const param = arrayView.getUint16((knotCount + i) * 2, true);
            const base = i * dimension;
            for (let d = 0; d < dimension; d++) {
                controls[base + d] = controlOffsets[d] + controlScales[d] * param;
            }
        }
    } else {
        for (let i = 0; i < knotCount; i++) {
            const base = i * dimension;
            const srcBase = (knotCount + i * dimension) * 2;
            for (let d = 0; d < dimension; d++) {
                const param = arrayView.getUint16(srcBase + d * 2, true);
                controls[base + d] = controlOffsets[d] + controlScales[d] * param;
            }
        }
    }
    return { knots, controls };
}

/**
 * `D4nK8uC7u` : 8-bit knots + 3-byte compressed quaternion controls
 * (sign bit + 7-bit magnitude). 4th component recovered via `sqrt(1 − Σ)`
 * from the unit-quat constraint.
 */
function decodeD4nK8uC7uCurve(loaded, raw, objectOffset, offsets, knotControlCount, arrayRef) {
    const knotCount = (knotControlCount / 4) | 0;
    if (knotCount <= 0) return { knots: EMPTY_F32, controls: EMPTY_F32 };
    const scaleEntriesOffset = objectOffset + (offsets.ScaleOffsetTableEntries ?? 2);
    const oneOverOffset = objectOffset + (offsets.OneOverKnotScale ?? 4);
    if (oneOverOffset + 4 > raw.length) return { knots: EMPTY_F32, controls: EMPTY_F32 };
    const rawView = new DataView(raw.buffer, raw.byteOffset, raw.byteLength);
    const oneOver = rawView.getFloat32(oneOverOffset, true);
    if (oneOver === 0.0) return { knots: EMPTY_F32, controls: EMPTY_F32 };
    const arrayData = loaded.sectionsFixed[arrayRef[0]];
    if (arrayRef[1] + knotControlCount > arrayData.length) return { knots: EMPTY_F32, controls: EMPTY_F32 };
    const knotScale = 1.0 / oneOver;
    const knots = new Float32Array(knotCount);
    for (let i = 0; i < knotCount; i++) {
        knots[i] = knotScale * arrayData[arrayRef[1] + i];
    }
    const { scales, offsets: tableOffsets } = quaternionScalesOffsets(
        rawView.getUint16(scaleEntriesOffset, true), 127.0,
    );
    const controls = new Float32Array(knotCount * 4);
    const controlBase = arrayRef[1] + knotCount;
    for (let i = 0; i < knotCount; i++) {
        const cur0 = arrayData[controlBase + i * 3 + 0];
        const cur1 = arrayData[controlBase + i * 3 + 1];
        const cur2 = arrayData[controlBase + i * 3 + 2];
        const missingNegative = (cur0 & 0x80) !== 0;
        const missingIndex = (((cur1 >> 6) & 0x2) | (cur2 >> 7)) & 0x3;
        const result = [0.0, 0.0, 0.0, 0.0];
        let dst = missingIndex;
        let summedSq = 0.0;
        const srcBytes = [cur0, cur1, cur2];
        for (let src = 0; src < 3; src++) {
            dst = (dst + 1) & 0x3;
            const v = tableOffsets[dst] + scales[dst] * (srcBytes[src] & 0x7f);
            result[dst] = v;
            summedSq += v * v;
        }
        const missing = Math.sqrt(Math.max(0.0, 1.0 - summedSq));
        result[missingIndex] = missingNegative ? -missing : missing;
        const base = i * 4;
        controls[base] = result[0];
        controls[base + 1] = result[1];
        controls[base + 2] = result[2];
        controls[base + 3] = result[3];
    }
    return { knots, controls };
}

/**
 * `D4nK16uC15u` : 16-bit knots + 3×u16 compressed quaternion controls
 * (sign bit + 15-bit magnitude). Same recovery as the 8-bit variant but
 * higher precision.
 */
function decodeD4nK16uC15uCurve(loaded, raw, objectOffset, offsets, knotControlCount, arrayRef) {
    const knotCount = (knotControlCount / 4) | 0;
    if (knotCount <= 0) return { knots: EMPTY_F32, controls: EMPTY_F32 };
    const scaleEntriesOffset = objectOffset + (offsets.ScaleOffsetTableEntries ?? 2);
    const oneOverOffset = objectOffset + (offsets.OneOverKnotScale ?? 4);
    if (oneOverOffset + 4 > raw.length) return { knots: EMPTY_F32, controls: EMPTY_F32 };
    const rawView = new DataView(raw.buffer, raw.byteOffset, raw.byteLength);
    const oneOver = rawView.getFloat32(oneOverOffset, true);
    if (oneOver === 0.0) return { knots: EMPTY_F32, controls: EMPTY_F32 };
    const arrayData = loaded.sectionsFixed[arrayRef[0]];
    const byteCount = knotControlCount * 2;
    if (arrayRef[1] + byteCount > arrayData.length) return { knots: EMPTY_F32, controls: EMPTY_F32 };
    const arrayView = new DataView(arrayData.buffer, arrayData.byteOffset + arrayRef[1], byteCount);
    const knotScale = 1.0 / oneOver;
    const knots = new Float32Array(knotCount);
    for (let i = 0; i < knotCount; i++) {
        knots[i] = knotScale * arrayView.getUint16(i * 2, true);
    }
    const { scales, offsets: tableOffsets } = quaternionScalesOffsets(
        rawView.getUint16(scaleEntriesOffset, true), 32767.0,
    );
    const controls = new Float32Array(knotCount * 4);
    const controlBaseShorts = knotCount;
    for (let i = 0; i < knotCount; i++) {
        const cur0 = arrayView.getUint16((controlBaseShorts + i * 3 + 0) * 2, true);
        const cur1 = arrayView.getUint16((controlBaseShorts + i * 3 + 1) * 2, true);
        const cur2 = arrayView.getUint16((controlBaseShorts + i * 3 + 2) * 2, true);
        const missingNegative = (cur0 & 0x8000) !== 0;
        const missingIndex = (((cur1 >> 14) & 0x2) | (cur2 >> 15)) & 0x3;
        const result = [0.0, 0.0, 0.0, 0.0];
        let dst = missingIndex;
        let summedSq = 0.0;
        const srcShorts = [cur0, cur1, cur2];
        for (let src = 0; src < 3; src++) {
            dst = (dst + 1) & 0x3;
            const v = tableOffsets[dst] + scales[dst] * (srcShorts[src] & 0x7fff);
            result[dst] = v;
            summedSq += v * v;
        }
        const missing = Math.sqrt(Math.max(0.0, 1.0 - summedSq));
        result[missingIndex] = missingNegative ? -missing : missing;
        const base = i * 4;
        controls[base] = result[0];
        controls[base + 1] = result[1];
        controls[base + 2] = result[2];
        controls[base + 3] = result[3];
    }
    return { knots, controls };
}

/**
 * Read the static value of a constant / identity curve. Returns a
 * `Float32Array` of length `dimension` ; empty if the codec doesn't
 * encode a constant.
 */
function readStaticCurveValue(codec, dimension, raw, offset) {
    if (dimension <= 0) return EMPTY_F32;
    if (codec.endsWith('Constant32f')) {
        const controlsOffset = offset + 4;
        const byteCount = dimension * 4;
        if (controlsOffset + byteCount > raw.length) return EMPTY_F32;
        const view = new DataView(raw.buffer, raw.byteOffset, raw.byteLength);
        const out = new Float32Array(dimension);
        for (let d = 0; d < dimension; d++) {
            out[d] = view.getFloat32(controlsOffset + d * 4, true);
        }
        return out;
    }
    if (codec === 'DaIdentity') {
        if (dimension === 9) return new Float32Array([1, 0, 0, 0, 1, 0, 0, 0, 1]);
        if (dimension === 4) return new Float32Array([0, 0, 0, 1]);
        return new Float32Array(dimension);
    }
    return EMPTY_F32;
}

// --- legacy curve detection -------------------------------------------

/**
 * Legacy curve format : `(degree:i32, knot_count:u32, knot_ptr,
 * control_count:u32, control_ptr)` ; total 16 bytes (32-bit pointer) or
 * 24 bytes (64-bit). `degree` doubles as a discriminator vs the modern
 * variant-ref format (degree is always 0-3 ; modern variant-ref's
 * type_ptr is a fake pointer ≥ 0x10000000).
 *
 * Returns `null` when the bytes don't fit a legacy curve — caller falls
 * back to the modern variant-ref interpretation. Always returns
 * `LegacyCurve32f` codec on success.
 */
function readLegacyCurveMetadata(loaded, section, offset) {
    const pointerSize = loaded.pointerSize;
    const raw = loaded.sectionsOriginal[section];
    const fixed = loaded.sectionsFixed[section];
    if (!raw || offset + 4 + (4 + pointerSize) * 2 > raw.length) return null;
    const rawView = new DataView(raw.buffer, raw.byteOffset, raw.byteLength);
    const degree = rawView.getInt32(offset, true);
    if (degree < 0 || degree > 3) return null;
    const knotCount = rawView.getUint32(offset + 4, true);
    const controlCount = rawView.getUint32(offset + 4 + pointerSize + 4, true);
    if (knotCount === 0 && controlCount === 0) return null;
    if (knotCount <= 0 || controlCount <= 0 || controlCount % knotCount !== 0) return null;
    const dimension = (controlCount / knotCount) | 0;
    if (dimension <= 0 || dimension > 16) return null;
    const knotsRef = decodeFakePointer(
        readPointerAt(fixed, offset + 4 + 4, pointerSize),
        loaded.sectionsFixed.length,
    );
    const controlsRef = decodeFakePointer(
        readPointerAt(fixed, offset + 4 + pointerSize + 4 + 4, pointerSize),
        loaded.sectionsFixed.length,
    );
    if (knotsRef === null || controlsRef === null) return null;
    const knotBytes = loaded.sectionsFixed[knotsRef[0]];
    const controlBytes = loaded.sectionsFixed[controlsRef[0]];
    if (knotsRef[1] + knotCount * 4 > knotBytes.length) return null;
    if (controlsRef[1] + controlCount * 4 > controlBytes.length) return null;
    const knotView = new DataView(knotBytes.buffer, knotBytes.byteOffset + knotsRef[1], knotCount * 4);
    const controlView = new DataView(controlBytes.buffer, controlBytes.byteOffset + controlsRef[1], controlCount * 4);
    const knots = new Float32Array(knotCount);
    for (let i = 0; i < knotCount; i++) knots[i] = knotView.getFloat32(i * 4, true);
    const controls = new Float32Array(controlCount);
    for (let i = 0; i < controlCount; i++) controls[i] = controlView.getFloat32(i * 4, true);
    const sampleValue = knotCount === 1 ? controls.slice(0, dimension) : EMPTY_F32;
    return {
        codec: 'LegacyCurve32f',
        format: -1,
        degree,
        dimension,
        knotControlCount: knotCount + controlCount,
        sampleValue,
        knots,
        controls,
    };
}

// --- modern curve metadata --------------------------------------------

/**
 * Walk the curve at `parentSection:parentOffset+curveOffset`. Tries the
 * legacy format first ; on miss, falls back to the modern variant-ref
 * `(type_ptr, object_ptr)` pair. Returns `null` if neither resolves.
 */
function readCurveMetadata(loaded, parentSection, curveOffset) {
    const pointerSize = loaded.pointerSize;
    const fixed = loaded.sectionsFixed[parentSection];
    if (!fixed || curveOffset + pointerSize * 2 > fixed.length) return null;
    const legacy = readLegacyCurveMetadata(loaded, parentSection, curveOffset);
    if (legacy) return legacy;
    const typeRef = decodeFakePointer(
        readPointerAt(fixed, curveOffset, pointerSize),
        loaded.sectionsFixed.length,
    );
    const objectRef = decodeFakePointer(
        readPointerAt(fixed, curveOffset + pointerSize, pointerSize),
        loaded.sectionsFixed.length,
    );
    if (typeRef === null || objectRef === null) return null;
    const members = parseTypeTree(loaded, typeRef);
    if (members.length === 0) return null;
    const codec = members[0].name.replace(/^CurveDataHeader_/, '');
    const offsets = memberOffsetsFor(loaded, members);
    const raw = loaded.sectionsOriginal[objectRef[0]];
    if (objectRef[1] + 2 > raw.length) return null;
    const rawView = new DataView(raw.buffer, raw.byteOffset, raw.byteLength);
    const fmt = rawView.getUint8(objectRef[1]);
    const degree = rawView.getUint8(objectRef[1] + 1);
    let dimension = curveDimension(codec);
    if (codec === 'DaIdentity') {
        const dimOffset = objectRef[1] + (offsets.Dimension ?? 2);
        if (dimOffset + 2 <= raw.length) {
            dimension = rawView.getInt16(dimOffset, true);
        }
    }
    let knotControlCount = 0;
    const kcOffset = offsets.KnotsControls;
    if (kcOffset !== undefined && objectRef[1] + kcOffset + 4 <= raw.length) {
        knotControlCount = rawView.getUint32(objectRef[1] + kcOffset, true);
    }
    const { knots, controls } = readCurveKnotsControls(
        loaded, codec, dimension, raw, loaded.sectionsFixed[objectRef[0]],
        objectRef[1], offsets, knotControlCount,
    );
    return {
        codec,
        format: fmt,
        degree,
        dimension,
        knotControlCount,
        sampleValue: readStaticCurveValue(codec, dimension, raw, objectRef[1]),
        knots,
        controls,
    };
}

// --- walk slice -------------------------------------------------------

function readTransformTrack(loaded, index, parsedTrack) {
    const { ref, fields } = parsedTrack;
    const nameValue = fields.Name?.value;
    const orientationField = fields.OrientationCurve ?? null;
    const positionField = fields.PositionCurve ?? null;
    const scaleShearField = fields.ScaleShearCurve ?? null;

    // Python animation.py reads `Flags` as an int32 at `offsets.get("Flags", 4)`,
    // hardcoding offset 4 when the type tree omits Flags (typical for iRO
    // assets — TransformTrack carries Name + 3 inline curves only, no Flags
    // field). The bytes at offset 4 happen to be `PositionCurve.Degree` ;
    // Python reports that value as `flags`. Mirror the behaviour byte-for-byte
    // so the live-oracle parity test stays clean.
    let flagsOffset = ref.offset + 4;
    const flagsField = fields.Flags;
    if (flagsField && typeof flagsField.offset === 'number') {
        flagsOffset = flagsField.offset;
    }
    let flags = 0;
    const raw = loaded.sectionsOriginal[ref.section];
    if (raw && flagsOffset + 4 <= raw.length) {
        const view = new DataView(raw.buffer, raw.byteOffset, raw.byteLength);
        flags = view.getInt32(flagsOffset, true);
    }

    return {
        index,
        name: typeof nameValue === 'string' && nameValue ? nameValue : `TransformTrack_${index}`,
        flags,
        orientationCurve: orientationField
            ? readCurveMetadata(loaded, ref.section, orientationField.offset)
            : null,
        positionCurve: positionField
            ? readCurveMetadata(loaded, ref.section, positionField.offset)
            : null,
        scaleShearCurve: scaleShearField
            ? readCurveMetadata(loaded, ref.section, scaleShearField.offset)
            : null,
    };
}

function readTransformTracks(loaded, field, maxTracks) {
    const target = field?.target ?? null;
    const typeRef = field?.reference_type ?? null;
    const count = field?.count ?? 0;
    if (target === null || typeRef === null || count <= 0) return [];
    const parsed = readReferenceArrayObjects(loaded, target, count, typeRef, {
        maxCount: maxTracks,
        maxArrayRefs: 64,
    });
    const out = new Array(parsed.length);
    for (let i = 0; i < parsed.length; i++) {
        out[i] = readTransformTrack(loaded, i, parsed[i]);
    }
    return out;
}

function readVectorTrackNames(loaded, field, maxCount) {
    const target = field?.target ?? null;
    const typeRef = field?.reference_type ?? null;
    const count = field?.count ?? 0;
    if (target === null || typeRef === null || count <= 0) return [];
    const parsed = readReferenceArrayObjects(loaded, target, count, typeRef, {
        maxCount,
        maxArrayRefs: 64,
    });
    const names = new Array(parsed.length);
    for (let i = 0; i < parsed.length; i++) {
        const value = parsed[i].fields.Name?.value;
        names[i] = typeof value === 'string' && value ? value : `Track_${i}`;
    }
    return names;
}

function readTrackGroup(loaded, index, ref, typeMembers, maxTracks) {
    const fields = parseObject(loaded, typeMembers, [ref.section, ref.offset], {
        maxArrayRefs: maxTracks,
    });
    const transformTracks = readTransformTracks(loaded, fields.TransformTracks, maxTracks);
    const nameValue = fields.Name?.value;
    return {
        index,
        name: typeof nameValue === 'string' && nameValue ? nameValue : `TrackGroup_${index}`,
        vectorTrackCount: fields.VectorTracks?.count ?? 0,
        transformTrackCount: fields.TransformTracks?.count ?? 0,
        textTrackCount: fields.TextTracks?.count ?? 0,
        accumulationFlags: fields.AccumulationFlags?.value ?? 0,
        loopTranslation: fields.LoopTranslation?.value ?? 0.0,
        vectorTrackNames: readVectorTrackNames(loaded, fields.VectorTracks, maxTracks),
        transformTracks,
    };
}

function readTrackGroupReferenceNames(loaded, field, maxRefs) {
    const target = field?.target ?? null;
    const typeRef = field?.reference_type ?? null;
    const count = field?.count ?? 0;
    if (target === null || count <= 0) return [];
    const elementRefs = field?.element_refs ?? [];
    const names = new Array(elementRefs.length);
    if (typeRef === null) {
        for (let i = 0; i < elementRefs.length; i++) names[i] = `TrackGroup_${i}`;
        return names;
    }
    const typeMembers = parseTypeTree(loaded, [typeRef.section, typeRef.offset]);
    const limit = Math.min(elementRefs.length, maxRefs);
    for (let i = 0; i < limit; i++) {
        const element = elementRefs[i];
        const fields = parseObject(loaded, typeMembers, [element.section, element.offset], {
            maxArrayRefs: 0,
        });
        const value = fields.Name?.value;
        names[i] = typeof value === 'string' && value ? value : `TrackGroup_${i}`;
    }
    names.length = limit;
    return names;
}

function readAnimation(loaded, index, ref, typeMembers, trackGroupByName) {
    const fields = parseObject(loaded, typeMembers, [ref.section, ref.offset], {
        maxArrayRefs: 64,
    });
    const nameValue = fields.Name?.value;
    const trackGroupNames = readTrackGroupReferenceNames(loaded, fields.TrackGroups, 64);
    const trackGroups = [];
    for (let i = 0; i < trackGroupNames.length; i++) {
        const tg = trackGroupByName[trackGroupNames[i]];
        if (tg) trackGroups.push(tg);
    }
    return {
        index,
        name: typeof nameValue === 'string' && nameValue ? nameValue : `Animation_${index}`,
        duration: fields.Duration?.value ?? 0.0,
        timeStep: fields.TimeStep?.value ?? 0.0,
        oversampling: fields.Oversampling?.value ?? 0.0,
        defaultLoopCount: fields.DefaultLoopCount?.value ?? 0,
        flags: fields.Flags?.value ?? 0,
        trackGroupNames,
        trackGroups,
    };
}

// --- public entry -----------------------------------------------------

/**
 * Walk `root.TrackGroups` and `root.Animations` and return the
 * `Animation[]` array. Each animation carries its referenced
 * `TrackGroup[]` (resolved by name), and each track group carries its
 * `TransformTrack[]` with decoded position / orientation / scale-shear
 * curves ready for `evaluateTransformTrack`.
 *
 * Returns `[]` for fixtures that don't expose any animation (typical of
 * pure model files — though some iRO model files bundle a single rest
 * animation).
 *
 * @param {import('./GrannyTypeTree.js').LoadedGR2} loaded
 * @param {ExtractAnimationsOptions} [options]
 * @returns {readonly Animation[]}
 */
export function extractAnimations(loaded, options = {}) {
    const maxTrackGroups = options.maxTrackGroups ?? 64;
    const maxTracksPerGroup = options.maxTracksPerGroup ?? 512;
    const maxAnimations = options.maxAnimations ?? 64;

    const file = loaded.file;
    const rootTypeTree = parseTypeTree(loaded, file.header.root_type);
    const root = parseObject(loaded, rootTypeTree, file.header.root_object, {
        maxArrayRefs: Math.max(maxTrackGroups, maxAnimations),
    });

    // --- Track groups -------------------------------------------------
    const trackGroupField = root.TrackGroups;
    const trackGroups = [];
    if (trackGroupField) {
        const tgTypeRef = trackGroupField.reference_type ?? null;
        const tgRefs = trackGroupField.element_refs ?? [];
        if (tgTypeRef !== null && tgRefs.length > 0) {
            const tgTypeMembers = parseTypeTree(loaded, [tgTypeRef.section, tgTypeRef.offset]);
            const limit = Math.min(tgRefs.length, maxTrackGroups);
            for (let i = 0; i < limit; i++) {
                trackGroups.push(readTrackGroup(loaded, i, tgRefs[i], tgTypeMembers, maxTracksPerGroup));
            }
        }
    }

    // --- Animations ---------------------------------------------------
    const animationField = root.Animations;
    const animations = [];
    if (animationField) {
        const animTypeRef = animationField.reference_type ?? null;
        const animRefs = animationField.element_refs ?? [];
        if (animTypeRef !== null && animRefs.length > 0) {
            const animTypeMembers = parseTypeTree(loaded, [animTypeRef.section, animTypeRef.offset]);
            const trackGroupByName = Object.create(null);
            for (let i = 0; i < trackGroups.length; i++) {
                trackGroupByName[trackGroups[i].name] = trackGroups[i];
            }
            const limit = Math.min(animRefs.length, maxAnimations);
            for (let i = 0; i < limit; i++) {
                animations.push(readAnimation(loaded, i, animRefs[i], animTypeMembers, trackGroupByName));
            }
        }
    }

    return /** @type {readonly Animation[]} */ (animations);
}

// --- evaluate ---------------------------------------------------------

/** Linear forward scan : index of the first knot > t, clamped to last knot. */
function findKnot(knots, t) {
    const n = knots.length;
    for (let i = 0; i < n; i++) {
        if (knots[i] > t) return i;
    }
    return n > 0 ? n - 1 : 0;
}

function safeDiv(numerator, denominator) {
    if (Math.abs(denominator) < 1e-12) return 0.0;
    return numerator / denominator;
}

function linearCoefficients(tiPrev, tiCurr, t) {
    const blend = safeDiv(t - tiPrev, tiCurr - tiPrev);
    return [1.0 - blend, blend];
}

function quadraticCoefficients(ti2, ti1, ti, ti1Next, t) {
    const l0 = safeDiv(t - ti1, ti - ti1);
    const l1_1 = safeDiv(t - ti2, ti - ti2);
    const l1_2 = safeDiv(t - ti1, ti1Next - ti1);
    const ci2PlusCi1 = (l1_1 + l0) - l0 * l1_1;
    const ci = l0 * l1_2;
    const ci_1 = ci2PlusCi1 - ci;
    const ci_2 = 1.0 - ci2PlusCi1;
    return [ci_2, ci_1, ci];
}

function normalizeQuaternion(values) {
    const x = values[0], y = values[1], z = values[2], w = values[3];
    const lengthSq = x * x + y * y + z * z + w * w;
    if (lengthSq <= 0.0) return values;
    const invLength = 1.0 / Math.sqrt(lengthSq);
    return [x * invLength, y * invLength, z * invLength, w * invLength];
}

/**
 * Fast quaternion renormalize used by granny2.dll's B-spline quaternion
 * samplers (`fcn.1000a3e0` and siblings, sha `befa33fb…3653570d`) : a single
 * Newton–Raphson step of inverse-sqrt seeded at 1, i.e. `q *= (3 − |q|²) / 2`.
 * The DLL applies this — NOT an exact `1/√|q|²` — after every degree-1/2/3
 * quaternion blend, so the raw local-pose it emits carries this exact rounding.
 * It matches exact normalization at `|q| = 1` but diverges as the blend drifts
 * off-unit near non-unit B-spline control points (e.g. 8_dead's death throw,
 * where an exact normalize left w saturating at 1.0 vs the DLL's 0.998). Match
 * it verbatim so `poseAt().localTransforms` is float-faithful to `LOCALPOSE`.
 */
function normalizeQuaternionFast(values) {
    const x = values[0], y = values[1], z = values[2], w = values[3];
    const lengthSq = x * x + y * y + z * z + w * w;
    const factor = (3.0 - lengthSq) * 0.5;
    return [x * factor, y * factor, z * factor, w * factor];
}

function controlAt(controls, dimension, knotIndex) {
    const knotCount = controls.length / dimension;
    const idx = knotIndex < knotCount ? knotIndex : knotCount - 1;
    const base = idx * dimension;
    const out = new Array(dimension);
    for (let d = 0; d < dimension; d++) out[d] = controls[base + d];
    return out;
}

/**
 * Evaluate a single curve at time `t`. Returns a `number[]` of length
 * `curve.dimension`. Empty curves fall back to `sampleValue` (constant
 * / identity codecs) ; absent control points → empty array.
 */
function evaluateCurve(curve, t) {
    if (!curve) return null;
    const knots = curve.knots;
    const controls = curve.controls;
    const dimension = curve.dimension;
    if (knots.length === 0 || controls.length === 0) {
        // Constant or identity curve.
        if (curve.sampleValue.length > 0) {
            const out = new Array(curve.sampleValue.length);
            for (let i = 0; i < out.length; i++) out[i] = curve.sampleValue[i];
            return out;
        }
        return null;
    }
    const degree = curve.degree > 0 ? curve.degree | 0 : 0;
    if (degree === 0 || knots.length === 1) {
        const knotIndex = findKnot(knots, t);
        return controlAt(controls, dimension, knotIndex);
    }
    const knotIndex = findKnot(knots, t);
    const clampedKnot = knotIndex < knots.length ? knotIndex : knots.length - 1;
    const window = 2 * degree;
    const base = clampedKnot - degree;
    const ti = new Array(window);
    const pi = new Array(window);
    for (let local = 0; local < window; local++) {
        let source = base + local;
        if (source < 0) source = 0;
        if (source > knots.length - 1) source = knots.length - 1;
        ti[local] = knots[source];
        pi[local] = controlAt(controls, dimension, source);
    }
    const center = degree;
    let result;
    if (degree === 1) {
        const [cPrev, cCurr] = linearCoefficients(ti[center - 1], ti[center], t);
        result = new Array(dimension);
        for (let d = 0; d < dimension; d++) {
            result[d] = cPrev * pi[center - 1][d] + cCurr * pi[center][d];
        }
    } else if (degree === 2) {
        const [c2, c1, c0] = quadraticCoefficients(
            ti[center - 2], ti[center - 1], ti[center], ti[center + 1], t,
        );
        result = new Array(dimension);
        for (let d = 0; d < dimension; d++) {
            result[d] = c2 * pi[center - 2][d] + c1 * pi[center - 1][d] + c0 * pi[center][d];
        }
    } else {
        result = pi[center];
    }
    if (dimension === 4) return normalizeQuaternionFast(result);
    return result;
}

/**
 * Pad a sampled curve to its canonical Transform shape. Granny encodes
 * uniform-scale tracks with `dimension = 3` ; we expand those to the
 * 3×3 diagonal `[sx, 0, 0, 0, sy, 0, 0, 0, sz]` so `scaleShear` always
 * delivers 9 floats. Anything that doesn't match a known dimension
 * collapses to the identity fallback.
 */
function shapeTransformComponent(values, targetLength, identity) {
    if (!values) return identity;
    if (values.length === targetLength) return values;
    if (targetLength === 9 && values.length === 3) {
        return [values[0], 0, 0, 0, values[1], 0, 0, 0, values[2]];
    }
    return identity;
}

/**
 * Evaluate all three curves of a `TransformTrack` at time `t` and
 * return the local Transform `{ position[3], orientation[4],
 * scaleShear[9] }`. Curves that are null or empty fall back to identity
 * values (zero position, identity quat, identity 3×3 scale-shear) so
 * the return shape is always populated and ready for the renderer.
 *
 * Uniform-scale tracks (dim 3) are expanded to a diagonal 3×3 matrix
 * so callers don't need to branch on the original codec dimension.
 *
 * @param {TransformTrack | null | undefined} track
 * @param {number} t — sample time.
 * @returns {EvaluatedTransform}
 */
export function evaluateTransformTrack(track, t) {
    const positionRaw = evaluateCurve(track?.positionCurve, t);
    const orientationRaw = evaluateCurve(track?.orientationCurve, t);
    const scaleShearRaw = evaluateCurve(track?.scaleShearCurve, t);
    return /** @type {EvaluatedTransform} */ ({
        position: shapeTransformComponent(positionRaw, 3, [...IDENTITY_TRANSFORM.position]),
        orientation: shapeTransformComponent(orientationRaw, 4, [...IDENTITY_TRANSFORM.orientation]),
        scaleShear: shapeTransformComponent(scaleShearRaw, 9, [...IDENTITY_TRANSFORM.scaleShear]),
    });
}

/**
 * Evaluate every TransformTrack of every TrackGroup in `animation` at
 * time `t`. Returns a map keyed by `transformTrack.name` (the name S8
 * will use to join against `skeleton.bones[i].name`). Track-group
 * boundaries are flattened — caller doesn't typically care which group
 * a track came from once the names are resolved.
 *
 * @param {Animation | null | undefined} animation
 * @param {number} t — sample time.
 * @returns {{ readonly [trackName: string]: EvaluatedTransform }}
 */
export function evaluateAnimation(animation, t) {
    const out = Object.create(null);
    if (!animation) return out;
    const groups = animation.trackGroups ?? [];
    for (let g = 0; g < groups.length; g++) {
        const tracks = groups[g].transformTracks ?? [];
        for (let i = 0; i < tracks.length; i++) {
            out[tracks[i].name] = evaluateTransformTrack(tracks[i], t);
        }
    }
    return out;
}

// --- internal test surface --------------------------------------------

/** Internals exposed to the unit-test suite. Not part of the public API. */
export const __test__ = {
    SCALAR_SIZES,
    QUATERNION_SCALE_OFFSET_TABLE,
    floatFromHighU16,
    quaternionScalesOffsets,
    curveDimension,
    findKnot,
    safeDiv,
    linearCoefficients,
    quadraticCoefficients,
    normalizeQuaternion,
    normalizeQuaternionFast,
    readLegacyCurveMetadata,
    readCurveMetadata,
    evaluateCurve,
};
