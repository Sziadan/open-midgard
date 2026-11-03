// GrannyPose.js — runtime pose composition for an animated skeleton.
//
// Clean-room derivation of `GrannyGetWorldPoseComposite4x4Array` :
// walk the bone hierarchy parent-first, compose each bone's local
// Transform into a 4×4 matrix (T × R × S), cascade with the parent's
// world matrix (Mworld[i] = Mworld[parent] × Mlocal[i]), then post-
// multiply by the bind-pose inverse for skinning (Mskin[i] = Mworld[i]
// × InverseWorldTransform[i]).
//
// Convention :
//   - storage  : column-major Float32Array(16) (WebGL-ready).
//   - vector   : column-vector (v' = M × v).
//   - TRS order: Mlocal = T × R × S (scale-shear applied first to v).
//   - quat order: (x, y, z, w), used as-is (the DLL does not renormalize
//     at matrix-build; the curve sampler already fast-normalized it).
//
// The bind-pose InverseWorldTransform is stored on disk as 16 floats in
// Granny's row-major byte layout. The on-disk bytes are mathematically
// equivalent to the column-major byte layout of the transposed matrix,
// so the smoke test (composeSkinningMatrices at bind pose returns I) is
// the canonical check — if it passes, no transpose is needed ; if it
// fails, flip `IWT_TRANSPOSE_ON_LOAD` below.

import { evaluateAnimation } from './GrannyAnimation.js';

/**
 * One bone's local Transform at a sampled instant : either a `Transform`
 * read from the bind pose, an `EvaluatedTransform` produced by
 * `evaluateAnimation`, or any structurally-compatible shape (length-3
 * position, length-4 quaternion, length-9 scale-shear — checked at
 * runtime ; the type only constrains the shape, not tuple length).
 *
 * @typedef {object} LooseTransform
 * @property {number} [flags]
 * @property {ArrayLike<number>} [position]
 * @property {ArrayLike<number>} [orientation]
 * @property {ArrayLike<number>} [scaleShear]
 */

/**
 * @typedef {import('./GrannyTransform.js').Transform
 *   | import('./GrannyAnimation.js').EvaluatedTransform
 *   | LooseTransform} SampledTransform
 */

/**
 * Output of {@link poseSkeletonAt} : per-bone snapshots ready for the GPU.
 *
 * @typedef {object} PoseSnapshot
 * @property {SampledTransform[]} localTransforms — per-bone local Transform :
 *   evaluated from the animation when the bone has a matching track, bind-pose
 *   fallback otherwise. Indexed by `bone.index`.
 * @property {Float32Array[]} worldMatrices — per-bone world matrix
 *   (`Mworld[i] = Mworld[parent] × Mlocal[i]`), column-major Float32Array(16).
 *   Indexed by `bone.index`.
 * @property {Float32Array[]} skinningMatrices — per-bone skinning matrix
 *   (`Mskin[i] = Mworld[i] × IWT[i]`), column-major Float32Array(16). The GPU
 *   vertex shader uses these directly to push bind-pose vertices into the
 *   frame's world space. Indexed by `bone.index`.
 */

const IDENTITY_MAT4 = Object.freeze([
    1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, 1,
]);

const IDENTITY_TRANSFORM = Object.freeze({
    flags: 0,
    position: [0.0, 0.0, 0.0],
    orientation: [0.0, 0.0, 0.0, 1.0],
    scaleShear: [1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0],
});

// Smoke-test toggle. Set by the convention check on bind-pose data.
// `false` means : reinterpret on-disk IWT bytes as column-major directly.
// `true`  means : transpose row-major → column-major at conversion time.
const IWT_TRANSPOSE_ON_LOAD = false;

/**
 * Convert one Transform `{position, orientation, scaleShear}` into a
 * column-major 4×4 matrix `Mlocal = T × R × S`. The quaternion is used
 * as-is (granny2.dll does not renormalize at matrix-build — see below) ;
 * the 9-float scale-shear (row-major 3×3) is lifted into the upper-left
 * 3×3 of the column-major 4×4. Output is a fresh Float32Array(16).
 *
 * @param {SampledTransform | null | undefined} transform
 * @returns {Float32Array} fresh column-major 4×4.
 */
export function composeLocalMatrix(transform) {
    const t = transform ?? IDENTITY_TRANSFORM;
    const pos = t.position ?? IDENTITY_TRANSFORM.position;
    // No renormalize — see {@link composeLocalMatrixF64}. granny2.dll builds the
    // bone matrix (fcn.100189a0) straight from the fast-normalized local-pose
    // quaternion ; an exact 1/√ here would diverge from the DLL on off-unit blends.
    const quat = t.orientation ?? IDENTITY_TRANSFORM.orientation;
    const ss = t.scaleShear ?? IDENTITY_TRANSFORM.scaleShear;
    const px = pos[0], py = pos[1], pz = pos[2];
    const qx = quat[0], qy = quat[1], qz = quat[2], qw = quat[3];

    // Rotation 3×3 from quaternion (column-vector convention).
    const xx = qx * qx, yy = qy * qy, zz = qz * qz;
    const xy = qx * qy, xz = qx * qz, yz = qy * qz;
    const wx = qw * qx, wy = qw * qy, wz = qw * qz;
    const r00 = 1 - 2 * (yy + zz);
    const r01 = 2 * (xy - wz);
    const r02 = 2 * (xz + wy);
    const r10 = 2 * (xy + wz);
    const r11 = 1 - 2 * (xx + zz);
    const r12 = 2 * (yz - wx);
    const r20 = 2 * (xz - wy);
    const r21 = 2 * (yz + wx);
    const r22 = 1 - 2 * (xx + yy);

    // ScaleShear 3×3 (row-major source : indices 0..8 = [m00..m22] row-by-row).
    const s00 = ss[0], s01 = ss[1], s02 = ss[2];
    const s10 = ss[3], s11 = ss[4], s12 = ss[5];
    const s20 = ss[6], s21 = ss[7], s22 = ss[8];

    // Mlocal = T × R × S. Compute (R × S) first (rotation × scale-shear),
    // then translate. The translation only writes column 3 — so we can
    // bake it in directly without a second matmul.
    const m00 = r00 * s00 + r01 * s10 + r02 * s20;
    const m01 = r00 * s01 + r01 * s11 + r02 * s21;
    const m02 = r00 * s02 + r01 * s12 + r02 * s22;
    const m10 = r10 * s00 + r11 * s10 + r12 * s20;
    const m11 = r10 * s01 + r11 * s11 + r12 * s21;
    const m12 = r10 * s02 + r11 * s12 + r12 * s22;
    const m20 = r20 * s00 + r21 * s10 + r22 * s20;
    const m21 = r20 * s01 + r21 * s11 + r22 * s21;
    const m22 = r20 * s02 + r21 * s12 + r22 * s22;

    const out = new Float32Array(16);
    out[0] = m00; out[1] = m10; out[2] = m20; out[3] = 0;
    out[4] = m01; out[5] = m11; out[6] = m21; out[7] = 0;
    out[8] = m02; out[9] = m12; out[10] = m22; out[11] = 0;
    out[12] = px; out[13] = py; out[14] = pz; out[15] = 1;
    return out;
}

/**
 * Multiply two column-major 4×4 matrices : `out = a × b`. The result is
 * written into the supplied `out` array (caller may pre-allocate for
 * hot-path reuse) ; pass `null` for `out` to allocate a fresh Float32Array.
 * `a` and `b` may alias `out`.
 *
 * @param {ArrayLike<number>} a — column-major 4×4.
 * @param {ArrayLike<number>} b — column-major 4×4.
 * @param {Float32Array | number[] | null} [out] - destination ; a plain
 *   `number[]` is accepted for the internal f64 cascade. Fresh Float32Array(16)
 *   when null/omitted.
 * @returns {Float32Array | number[]} `out` (or the freshly allocated result).
 */
export function multiplyMat4(a, b, out) {
    const dst = out ?? new Float32Array(16);
    const a00 = a[0], a10 = a[1], a20 = a[2], a30 = a[3];
    const a01 = a[4], a11 = a[5], a21 = a[6], a31 = a[7];
    const a02 = a[8], a12 = a[9], a22 = a[10], a32 = a[11];
    const a03 = a[12], a13 = a[13], a23 = a[14], a33 = a[15];
    for (let col = 0; col < 4; col++) {
        const b0 = b[col * 4 + 0];
        const b1 = b[col * 4 + 1];
        const b2 = b[col * 4 + 2];
        const b3 = b[col * 4 + 3];
        dst[col * 4 + 0] = a00 * b0 + a01 * b1 + a02 * b2 + a03 * b3;
        dst[col * 4 + 1] = a10 * b0 + a11 * b1 + a12 * b2 + a13 * b3;
        dst[col * 4 + 2] = a20 * b0 + a21 * b1 + a22 * b2 + a23 * b3;
        dst[col * 4 + 3] = a30 * b0 + a31 * b1 + a32 * b2 + a33 * b3;
    }
    return dst;
}

/**
 * Walk the bone hierarchy parent-first and produce per-bone world
 * matrices via forward kinematics : `Mworld[i] = Mworld[parent] ×
 * Mlocal[i]` (root bones use just `Mlocal[i]`). S6 guarantees
 * `parentIndex < boneIndex` so a single forward pass suffices.
 *
 * `localMatrices` must be an array of column-major 16-float matrices
 * (Float32Array or plain `Array<number>`), one per bone, in the same
 * order as `skeleton.bones`. The cascade itself runs in plain `Array`s
 * to keep f64 precision through deep skeletons (10+ level chains
 * accumulate ~5e-4 of f32 ULP otherwise — well above the 1e-4 parity
 * target). Returns a fresh array of Float32Array(16) (GPU-ready).
 *
 * @param {import('./GrannySkeleton.js').Skeleton} skeleton
 * @param {readonly ArrayLike<number>[]} localMatrices — one column-major 4×4
 *   per bone, in `skeleton.bones` order.
 * @returns {Float32Array[]} fresh per-bone world matrices.
 */
export function composeWorldPose(skeleton, localMatrices) {
    const bones = skeleton.bones;
    const count = bones.length;
    const f64Worlds = new Array(count);
    for (let i = 0; i < count; i++) {
        const bone = bones[i];
        const local = localMatrices[i];
        const parent = bone.parentIndex;
        if (parent < 0 || parent >= i) {
            f64Worlds[i] = mat4ToArray(local);
        } else {
            f64Worlds[i] = multiplyMat4(f64Worlds[parent], local, new Array(16));
        }
    }
    const out = new Array(count);
    for (let i = 0; i < count; i++) {
        out[i] = new Float32Array(f64Worlds[i]);
    }
    return out;
}

/**
 * Post-multiply each world matrix by the bone's bind-pose inverse :
 * `Mskin[i] = Mworld[i] × IWT[i]`. The result is the matrix the GPU
 * vertex shader uses to push a bind-pose vertex into the current frame's
 * world space.
 *
 * The on-disk IWT is 16 floats in Granny's row-major byte layout ;
 * `IWT_TRANSPOSE_ON_LOAD` controls whether they're transposed during the
 * row-major → column-major conversion. The bind-pose smoke test
 * (`composeSkinningMatrices(skeleton, composeWorldPose(skeleton, bind))`
 * returns identity per bone) is the canonical check.
 *
 * @param {import('./GrannySkeleton.js').Skeleton} skeleton
 * @param {readonly ArrayLike<number>[]} worldMatrices — per-bone world matrices
 *   from {@link composeWorldPose}.
 * @returns {Float32Array[]} fresh per-bone skinning matrices.
 */
export function composeSkinningMatrices(skeleton, worldMatrices) {
    const bones = skeleton.bones;
    const count = bones.length;
    const out = new Array(count);
    for (let i = 0; i < count; i++) {
        const iwt = boneIwtAsArray(bones[i]);
        // Compose in f64 (plain Array) then truncate at the boundary :
        // Mworld[i] from composeWorldPose was already f32-cast, so the
        // skinning product is bounded by that ; doing matmul in f64
        // avoids amplifying the f32 ULP through the IWT product.
        const f64Skin = multiplyMat4(worldMatrices[i], iwt, new Array(16));
        out[i] = new Float32Array(f64Skin);
    }
    return out;
}

/**
 * Top-level entry : sample one animation at time `t` and produce all
 * three pose arrays at once. The per-bone local Transform is read from
 * `evaluateAnimation(animation, t)[bone.name]` ; bones not driven by
 * the animation fall back to their bind-pose Transform (so animations
 * that omit fingers / tail bones still produce a coherent pose).
 *
 * `animation` may be `null` to request the bind pose (no animation
 * driver, every bone uses its bind-pose Transform).
 *
 * @param {import('./GrannySkeleton.js').Skeleton} skeleton
 * @param {import('./GrannyAnimation.js').Animation | null | undefined} animation
 * @param {number} t — sample time.
 * @returns {PoseSnapshot}
 */
export function poseSkeletonAt(skeleton, animation, t) {
    const bones = skeleton.bones;
    const count = bones.length;
    const evaluated = animation ? evaluateAnimation(animation, t) : null;
    const localTransforms = new Array(count);
    const localMatricesF64 = new Array(count);
    for (let i = 0; i < count; i++) {
        const bone = bones[i];
        const sampled = evaluated ? evaluated[bone.name] : null;
        const local = sampled ?? bone.transform;
        localTransforms[i] = local;
        // Build Mlocal in plain Array (f64) to feed the FK cascade
        // without the f32 truncation `composeLocalMatrix` (Float32Array)
        // would introduce. Deep skeletons (Finger0 at depth 10) lose
        // ~5e-4 accuracy under f32 cascade ; f64 holds 1e-6.
        localMatricesF64[i] = composeLocalMatrixF64(local);
    }
    const worldMatrices = composeWorldPose(skeleton, localMatricesF64);
    const skinningMatrices = composeSkinningMatrices(skeleton, worldMatrices);
    return { localTransforms, worldMatrices, skinningMatrices };
}

// --- internal helpers ---------------------------------------------------

/**
 * Mirror of {@link composeLocalMatrix} that returns a plain `Array` (f64
 * precision) instead of `Float32Array`. Used by the FK cascade so deep
 * skeletons (10+ level parent chains) don't accumulate f32 ULP error.
 */
function composeLocalMatrixF64(transform) {
    const t = transform ?? IDENTITY_TRANSFORM;
    const pos = t.position ?? IDENTITY_TRANSFORM.position;
    // No renormalize : granny2.dll builds the bone matrix (fcn.100189a0, via
    // BuildCompositeTransform4x4) straight from the local-pose quaternion, which
    // `SampleModelAnimations` already left fast-normalized. Re-normalizing here
    // (exact 1/√) would undo the DLL's fast-normalize rounding and diverge on
    // off-unit blends (8_dead skinning 0.145 → f32-order). Bind-pose fallbacks
    // are already unit.
    const quat = t.orientation ?? IDENTITY_TRANSFORM.orientation;
    const ss = t.scaleShear ?? IDENTITY_TRANSFORM.scaleShear;
    const px = pos[0], py = pos[1], pz = pos[2];
    const qx = quat[0], qy = quat[1], qz = quat[2], qw = quat[3];
    const xx = qx * qx, yy = qy * qy, zz = qz * qz;
    const xy = qx * qy, xz = qx * qz, yz = qy * qz;
    const wx = qw * qx, wy = qw * qy, wz = qw * qz;
    const r00 = 1 - 2 * (yy + zz);
    const r01 = 2 * (xy - wz);
    const r02 = 2 * (xz + wy);
    const r10 = 2 * (xy + wz);
    const r11 = 1 - 2 * (xx + zz);
    const r12 = 2 * (yz - wx);
    const r20 = 2 * (xz - wy);
    const r21 = 2 * (yz + wx);
    const r22 = 1 - 2 * (xx + yy);
    const s00 = ss[0], s01 = ss[1], s02 = ss[2];
    const s10 = ss[3], s11 = ss[4], s12 = ss[5];
    const s20 = ss[6], s21 = ss[7], s22 = ss[8];
    const m00 = r00 * s00 + r01 * s10 + r02 * s20;
    const m01 = r00 * s01 + r01 * s11 + r02 * s21;
    const m02 = r00 * s02 + r01 * s12 + r02 * s22;
    const m10 = r10 * s00 + r11 * s10 + r12 * s20;
    const m11 = r10 * s01 + r11 * s11 + r12 * s21;
    const m12 = r10 * s02 + r11 * s12 + r12 * s22;
    const m20 = r20 * s00 + r21 * s10 + r22 * s20;
    const m21 = r20 * s01 + r21 * s11 + r22 * s21;
    const m22 = r20 * s02 + r21 * s12 + r22 * s22;
    return [
        m00, m10, m20, 0,   // col 0
        m01, m11, m21, 0,   // col 1
        m02, m12, m22, 0,   // col 2
        px, py, pz, 1,      // col 3
    ];
}

/** Copy any column-major 16-float ArrayLike into a plain `Array(16)`. */
function mat4ToArray(src) {
    const out = new Array(16);
    for (let i = 0; i < 16; i++) out[i] = src[i];
    return out;
}

/**
 * Convert one bone's on-disk 16-float InverseWorldTransform into a
 * plain `Array(16)` column-major form (f64 precision) for the skinning
 * multiply. Bones with a missing / short IWT (animation-only fixtures
 * sometimes elide bind-pose data) collapse to the identity matrix so
 * downstream code never sees NaN.
 */
function boneIwtAsArray(bone) {
    const src = bone.inverseWorldTransform;
    if (!src || src.length !== 16) return [...IDENTITY_MAT4];
    const dst = new Array(16);
    if (IWT_TRANSPOSE_ON_LOAD) {
        for (let r = 0; r < 4; r++) {
            for (let c = 0; c < 4; c++) {
                dst[c * 4 + r] = src[r * 4 + c];
            }
        }
    } else {
        for (let i = 0; i < 16; i++) dst[i] = src[i];
    }
    return dst;
}

// --- internal test surface ---------------------------------------------

/** Internals exposed to the unit-test suite. Not part of the public API. */
export const __test__ = {
    IDENTITY_MAT4,
    IDENTITY_TRANSFORM,
    IWT_TRANSPOSE_ON_LOAD,
    boneIwtAsArray,
    composeLocalMatrixF64,
};
