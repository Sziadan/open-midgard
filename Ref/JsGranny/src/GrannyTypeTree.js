// GrannyTypeTree.js — DataTypeDefinition walker + RootObject materializer.
//
// JS port of Rasetsuu/blendergranny io_scene_gr2/gr2/{fixup,types}.py
// (S5 of the granny-pipeline rollout — see plans/granny-pipeline/).
//
// Three layers, exported individually for testability :
//
//   1. fake-pointer codec  : encode/decode [section, offset] as a u32
//   2. loadGR2(file)       : decompress all sections + apply pointer fixups
//   3. parseTypeTree(...)  : walk a DataTypeDefinition chain
//   4. parseObject(...)    : materialize one instance against a type tree
//
// Binary reference : docs/gr2-format.md §8.
//
// Pre-condition assumed across the file : `header.version >= 7` (12-byte
// fixup entries, 16-byte mixed-marshalling entries). Every iRO ver12 .gr2
// in our corpus matches. A v6 fixture would need the variable-entry-size
// heuristic from blendergranny `fixup.py:_pointer_fixup_entry_size`.

import { COMPRESSION_NONE, COMPRESSION_OODLE0 } from './GrannyFile.js';
import { decompressOodle0 } from './GrannyOodle0.js';
import { readTransform } from './GrannyTransform.js';

/**
 * Typed error for malformed / hostile type-tree input (recursion breach,
 * cycle). Catch-friendly ; mirrors `DecompressionError` so callers can
 * distinguish a parser rejection from a bare `RangeError`.
 */
export class GrannyParseError extends Error {
    constructor(message) {
        super(message);
        this.name = 'GrannyParseError';
    }
}

// Local copy of the section-decompression dispatch from Granny.js. Kept
// inline (instead of re-imported from Granny.js) to keep the module DAG
// acyclic — Granny.js itself re-exports our public surface, so importing
// back from it would create a cycle.
function decompressOne(section, compressed) {
    if (section.expanded_size === 0) return new Uint8Array(0);
    if (section.compression === COMPRESSION_NONE) {
        if (compressed.length < section.expanded_size) {
            throw new RangeError(
                `section ${section.index} short: ${compressed.length} < ${section.expanded_size}`,
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
 * `[section_index, offset_within_section]` pair used everywhere as a ref.
 * @typedef {readonly [number, number]} SectionRef
 */

/**
 * `{section, offset}` dict shape returned for caller convenience.
 * @typedef {object} RefDict
 * @property {number} section
 * @property {number} offset
 */

/**
 * One MEMBER_TYPE constant value (0–22). See {@link MEMBER_TYPE_NAMES}.
 * @typedef {number} MemberTypeConstant
 */

/**
 * Member descriptor read from one 32-byte record of a DataTypeDefinition chain.
 *
 * @typedef {object} TypeMember
 * @property {MemberTypeConstant} memberType — raw `MT_*` enum value (0–22).
 * @property {string} memberTypeName — human name for {@link TypeMember.memberType}.
 * @property {string} name — member name (ASCII, decoded from the string-pointer slot).
 * @property {SectionRef | null} referenceType — decoded sub-type ref, or `null` for scalar / terminal members.
 * @property {number} arrayWidth — array width (default 1 for non-array members).
 * @property {readonly [number, number, number]} extra — three extra u32 slots in the on-disk record.
 * @property {number} offset — byte offset of this record within its containing section.
 */

/**
 * One pointer-fixup entry : rebases a writer-side pointer to a `[section, offset]` ref.
 *
 * @typedef {object} PointerFixup
 * @property {number} source_section
 * @property {number} source_offset
 * @property {SectionRef} target
 */

/**
 * One mixed-marshalling entry : describes an endian-flip the writer expects.
 *
 * @typedef {object} MixedMarshallingFixup
 * @property {number} source_section
 * @property {number} count
 * @property {number} offset
 * @property {SectionRef} type_ref
 */

/**
 * Bundle of decompressed sections + applied pointer fixups ready for walking.
 *
 * @typedef {object} LoadedGR2
 * @property {import('./GrannyFile.js').GR2File} file — the originating GR2 file (header + section table).
 * @property {readonly Uint8Array[]} sectionsOriginal — one `Uint8Array` per section, decompressed, untouched (scalar reads).
 * @property {readonly Uint8Array[]} sectionsFixed — one `Uint8Array` per section, with pointer-fixup slots overwritten by fake pointers.
 * @property {readonly PointerFixup[]} pointerFixups — all pointer fixups parsed from the file (flattened across sections).
 * @property {readonly MixedMarshallingFixup[]} mixedFixups — all mixed-marshalling fixups (typically empty for LE corpus).
 * @property {4 | 8} pointerSize — pointer width in bytes (4 for 32-bit files, 8 for 64-bit).
 */

/**
 * Common shape of a materialized field returned by {@link parseObject}.
 *
 * @typedef {object} ParsedField
 * @property {string} type — `memberTypeName` from the source {@link TypeMember}.
 * @property {number} offset — byte offset of the field inside its parent struct.
 * @property {RefDict} [reference_type] - sub-type ref, if the source member had one.
 * @property {number | string} [value] - decoded scalar value (`int*` / `uint*` / `real32` / `string`).
 * @property {RefDict | null} [target] - resolved target ref (`reference` / `*_to_array` / `*_of_references`).
 * @property {number} [count] - element count (`*_to_array` / `*_of_references` / `*_variant_array`).
 * @property {boolean} [truncated] - true when {@link ParsedField.count} exceeded `maxArrayRefs`.
 * @property {readonly RefDict[]} [element_refs] - decoded per-element refs (only `array_of_references`).
 * @property {RefDict | null} [variant_type] - resolved variant-type ref (`variant_reference` / `reference_to_variant_array`).
 * @property {ParsedObject} [inline] - materialized sub-object (only `inline` members).
 */

/**
 * Plain JS object materialized from one `[ref, typeTree]` pair, keyed by member name.
 * @typedef {{ readonly [memberName: string]: ParsedField }} ParsedObject
 */

/**
 * Options for {@link parseTypeTree}.
 * @typedef {object} ParseTypeTreeOptions
 * @property {number} [maxMembers] - cap on the number of member records walked (default 512).
 */

/**
 * Options for {@link parseObject}.
 * @typedef {object} ParseObjectOptions
 * @property {number} [maxArrayRefs] - cap on the number of `element_refs` returned per array (default 256).
 */

/**
 * Options for {@link readReferenceArrayObjects}.
 * @typedef {object} ReadReferenceArrayObjectsOptions
 * @property {number} [maxCount] - cap on the number of array elements walked (default 64).
 * @property {number} [maxArrayRefs] - cap forwarded to {@link parseObject} per element (default `maxCount`).
 */

/**
 * One element of an inline-struct array as returned by {@link readReferenceArrayObjects}.
 *
 * @typedef {object} ReferenceArrayObject
 * @property {RefDict} ref — per-object `{section, offset}` within the array's section.
 * @property {ParsedObject} fields — materialized field map for this element.
 */

// --- MEMBER_TYPE constants --------------------------------------------
// Verbatim from blendergranny io_scene_gr2/gr2/types.py:11–33.

/** Marker terminating a type-definition chain (no payload). */
export const MT_END = 0;
/** Inline sub-struct (no pointer, walks sub-type tree in place). */
export const MT_INLINE = 1;
/** Pointer to a sub-object (single instance). */
export const MT_REFERENCE = 2;
/** `(count:u32, pointer)` pair → array of `count` structs of `reference_type`. */
export const MT_REFERENCE_TO_ARRAY = 3;
/** `(count:u32, pointer)` pair → array of `count` pointers to structs of `reference_type`. */
export const MT_ARRAY_OF_REFERENCES = 4;
/** `(type_ptr, object_ptr)` pair → reference with runtime-resolved type. */
export const MT_VARIANT_REFERENCE = 5;
/** Placeholder for a type the file uses but the SDK doesn't expose. */
export const MT_UNSUPPORTED = 6;
/** `(type_ptr, count:u32, object_ptr)` triple → variant-typed array. */
export const MT_REFERENCE_TO_VARIANT_ARRAY = 7;
/** Pointer to a NUL-terminated (or length-prefixed) string. */
export const MT_STRING = 8;
/** Inline 4×4 + translation transform (68 bytes). */
export const MT_TRANSFORM = 9;
/** 32-bit IEEE float. */
export const MT_REAL32 = 10;
/** Signed 8-bit integer. */
export const MT_INT8 = 11;
/** Unsigned 8-bit integer. */
export const MT_UINT8 = 12;
/** Signed 8-bit, intended as a normalized binormal component. */
export const MT_BINORMAL_INT8 = 13;
/** Unsigned 8-bit, intended as a normalized normal component. */
export const MT_NORMAL_UINT8 = 14;
/** Signed 16-bit integer. */
export const MT_INT16 = 15;
/** Unsigned 16-bit integer. */
export const MT_UINT16 = 16;
/** Signed 16-bit, intended as a normalized binormal component. */
export const MT_BINORMAL_INT16 = 17;
/** Unsigned 16-bit, intended as a normalized normal component. */
export const MT_NORMAL_UINT16 = 18;
/** Signed 32-bit integer. */
export const MT_INT32 = 19;
/** Unsigned 32-bit integer. */
export const MT_UINT32 = 20;
/** 16-bit half-float (rare). */
export const MT_REAL16 = 21;
/** Null reference marker (occupies one pointer slot, never resolves). */
export const MT_EMPTY_REFERENCE = 22;

/** MEMBER_TYPE constant → human name.
 * @type {Readonly<Record<number, string>>} */
export const MEMBER_TYPE_NAMES = {
    0: 'end',
    1: 'inline',
    2: 'reference',
    3: 'reference_to_array',
    4: 'array_of_references',
    5: 'variant_reference',
    6: 'unsupported',
    7: 'reference_to_variant_array',
    8: 'string',
    9: 'transform',
    10: 'real32',
    11: 'int8',
    12: 'uint8',
    13: 'binormal_int8',
    14: 'normal_uint8',
    15: 'int16',
    16: 'uint16',
    17: 'binormal_int16',
    18: 'normal_uint16',
    19: 'int32',
    20: 'uint32',
    21: 'real16',
    22: 'empty_reference',
};

/** MEMBER_TYPE → on-disk size in bytes (scalars only). */
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

// --- fake-pointer codec -----------------------------------------------
// 1:1 from blendergranny fixup.py:14–15 + 66–78.
// On-disk pointers in the writer's address space are meaningless at load
// time. After decompression, we rewrite every pointer slot referenced by
// the pointer-fixup table with a synthesized « fake pointer » that encodes
// a `[section, offset]` ref. The type-tree walker then decodes those
// fake pointers back into refs.

/** Base value distinguishing fake pointers from raw u32 noise. */
export const FAKE_POINTER_BASE = 0x10000000;
/** Per-section stride inside the fake-pointer encoding. */
export const FAKE_SECTION_STRIDE = 0x100000;

/**
 * Encode a `[section, offset]` ref as a single u32 fake pointer.
 * Round-trips with {@link decodeFakePointer}.
 *
 * @param {number} section
 * @param {number} offset
 * @returns {number}
 */
export function makeFakePointer(section, offset) {
    return (FAKE_POINTER_BASE + section * FAKE_SECTION_STRIDE + offset) >>> 0;
}

/**
 * Decode a fake pointer back into `[section, offset]`. Returns `null`
 * for pointers outside the fake-pointer range or pointing at a section
 * index that doesn't exist.
 *
 * @param {number} pointer
 * @param {number} sectionCount
 * @returns {SectionRef | null}
 */
export function decodeFakePointer(pointer, sectionCount) {
    if (pointer < FAKE_POINTER_BASE) return null;
    const value = (pointer - FAKE_POINTER_BASE) >>> 0;
    const section = (value / FAKE_SECTION_STRIDE) | 0;
    const offset = value - section * FAKE_SECTION_STRIDE;
    if (section < 0 || section >= sectionCount) return null;
    return [section, offset];
}

// --- low-level binary helpers -----------------------------------------

/** Pointer width in bytes (4 for 32-bit files, 8 for 64-bit). */
function pointerSizeBytes(file) {
    return file.header.pointer_size >>> 3;
}

/**
 * Build a closure that reads a u32 from a `Uint8Array` honouring the
 * file's endianness. Throws `RangeError` on out-of-bounds reads.
 */
function makeU32Reader(byteReversed) {
    const little = !byteReversed;
    return (bytes, offset) => {
        if (offset < 0 || offset + 4 > bytes.length) {
            throw new RangeError(
                `gr2 type-tree read out of range at 0x${offset.toString(16)} (cap 0x${bytes.length.toString(16)})`,
            );
        }
        const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
        return view.getUint32(offset, little);
    };
}

/** Read one pointer (4 or 8 bytes) and return it as a JS number. */
function readPointer(bytes, offset, pointerSize, byteReversed) {
    const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
    const little = !byteReversed;
    if (pointerSize === 4) return view.getUint32(offset, little);
    // 64-bit : JS numbers are safe up to 2^53. Fake pointers stay well
    // below that ; real on-disk 64-bit pointers may overflow but we only
    // care about decoding fake pointers post-fixup.
    const lo = view.getUint32(offset, little) >>> 0;
    const hi = view.getUint32(offset + 4, little) >>> 0;
    return hi * 0x100000000 + lo;
}

/** Write a pointer-sized value (used by the fixup pass). */
function writePointer(bytes, offset, value, pointerSize, byteReversed) {
    const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
    const little = !byteReversed;
    if (pointerSize === 4) {
        view.setUint32(offset, value >>> 0, little);
        return;
    }
    const lo = (value >>> 0);
    const hi = Math.floor(value / 0x100000000) >>> 0;
    view.setUint32(offset, lo, little);
    view.setUint32(offset + 4, hi, little);
}

// --- loadGR2 ----------------------------------------------------------

/**
 * Decompress every section, apply the pointer-fixup table, and return a
 * `LoadedGR2` ready for the type-tree walker. See module header for the
 * fake-pointer encoding rationale.
 *
 * @throws Error when the file declares `byte_reversed` AND has non-empty
 *   mixed-marshalling tables — we'd need to actually flip endianness for
 *   in-section scalars, which the iRO corpus never requires. (LE assets
 *   carry empty mixed-marshalling tables ; the throw catches accidental
 *   silent corruption on a future BE asset.)
 *
 * @param {import('./GrannyFile.js').GR2File} file
 * @returns {LoadedGR2}
 */
export function loadGR2(file) {
    const sections = file.sections;
    const sectionCount = sections.length;
    const sectionsOriginal = new Array(sectionCount);
    const sectionsFixed = new Array(sectionCount);

    for (let i = 0; i < sectionCount; i++) {
        const section = sections[i];
        const compressed = file.sectionBytes(section);
        const expanded = decompressOne(section, compressed);
        sectionsOriginal[i] = expanded;
        // Clone so the fixup pass writes into a distinct buffer ; keeps
        // sectionsOriginal intact for scalar / string reads.
        sectionsFixed[i] = new Uint8Array(expanded);
    }

    const pointerSize = pointerSizeBytes(file);
    if (pointerSize !== 4 && pointerSize !== 8) {
        throw new Error(`unsupported pointer size ${file.header.pointer_size}`);
    }

    const pointerFixups = readPointerFixups(file);
    const mixedFixups = readMixedMarshallingFixups(file);

    if (file.header.byte_reversed && mixedFixups.length > 0) {
        throw new Error(
            'cross-endian asset (byte_reversed + mixed-marshalling table) ' +
            'is not supported — iRO corpus is all little-endian',
        );
    }

    // Apply pointer fixups : at sourceSection:sourceOffset, write a fake
    // pointer encoding the target ref. The type walker decodes these
    // back into [section, offset] via decodeFakePointer.
    for (let i = 0; i < pointerFixups.length; i++) {
        const fixup = pointerFixups[i];
        if (fixup.source_section >= sectionCount) continue;
        const data = sectionsFixed[fixup.source_section];
        if (fixup.source_offset + pointerSize > data.length) continue;
        const fake = makeFakePointer(fixup.target[0], fixup.target[1]);
        writePointer(data, fixup.source_offset, fake, pointerSize, file.header.byte_reversed);
    }

    return {
        file,
        sectionsOriginal,
        sectionsFixed,
        pointerFixups,
        mixedFixups,
        pointerSize,
    };
}

/**
 * Read every section's pointer-fixup table. v7+ entries are 12 bytes :
 * (source_offset:u32, target_section:u32, target_offset:u32).
 */
function readPointerFixups(file) {
    const u32 = makeU32Reader(file.header.byte_reversed);
    const data = file.data;
    const sections = file.sections;
    const fixups = [];
    for (let s = 0; s < sections.length; s++) {
        const section = sections[s];
        const count = section.pointer_fixup_count;
        const offset = section.pointer_fixup_offset;
        if (count === 0 || offset === 0) continue;
        const entrySize = 12;
        if (offset + count * entrySize > data.length) {
            throw new RangeError(
                `section ${s} pointer fixup table at 0x${offset.toString(16)} ` +
                `(${count} entries) escapes file (size 0x${data.length.toString(16)})`,
            );
        }
        for (let i = 0; i < count; i++) {
            const base = offset + i * entrySize;
            fixups.push({
                source_section: s,
                source_offset: u32(data, base),
                target: /** @type {SectionRef} */ ([u32(data, base + 4), u32(data, base + 8)]),
            });
        }
    }
    return fixups;
}

/**
 * Read every section's mixed-marshalling table. v7+ entries are 16 bytes :
 * (count:u32, offset:u32, type_section:u32, type_offset:u32).
 *
 * For the all-LE iRO corpus these tables are typically empty ; we parse
 * them only to surface non-empty ones via `loadGR2`'s defensive throw
 * (would be a silent corruption signal on a future BE asset).
 */
function readMixedMarshallingFixups(file) {
    const u32 = makeU32Reader(file.header.byte_reversed);
    const data = file.data;
    const sections = file.sections;
    const fixups = [];
    for (let s = 0; s < sections.length; s++) {
        const section = sections[s];
        const count = section.mixed_marshalling_count;
        const offset = section.mixed_marshalling_offset;
        if (count === 0 || offset === 0) continue;
        const entrySize = 16;
        if (offset + count * entrySize > data.length) {
            throw new RangeError(
                `section ${s} mixed-marshalling table at 0x${offset.toString(16)} ` +
                `(${count} entries) escapes file (size 0x${data.length.toString(16)})`,
            );
        }
        for (let i = 0; i < count; i++) {
            const base = offset + i * entrySize;
            fixups.push({
                source_section: s,
                count: u32(data, base),
                offset: u32(data, base + 4),
                type_ref: /** @type {SectionRef} */ ([u32(data, base + 8), u32(data, base + 12)]),
            });
        }
    }
    return fixups;
}

// --- string helpers ---------------------------------------------------

/** Inspect bytes to decide whether they're printable text. */
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

const TEXT_DECODER = new TextDecoder('utf-8', { fatal: false });

/**
 * Read a Granny string starting at `ref` inside `loaded.sectionsOriginal`.
 * Tries length-prefixed (u32 length + bytes), falls back to NUL-terminated.
 * Returns an empty string when the bytes don't look like text — same
 * heuristic as blendergranny `types.py:_looks_text`.
 */
function readGrannyString(loaded, ref, maxLength = 1024) {
    const section = ref[0];
    const offset = ref[1];
    if (section >= loaded.sectionsOriginal.length) return '';
    const data = loaded.sectionsOriginal[section];
    if (offset < 0 || offset >= data.length) return '';

    // Try length-prefixed first.
    if (offset + 4 <= data.length) {
        const u32 = makeU32Reader(loaded.file.header.byte_reversed);
        const length = u32(data, offset);
        const end = offset + 4 + length;
        if (length > 0 && length <= maxLength && end <= data.length) {
            const slice = data.subarray(offset + 4, end);
            if (looksText(slice)) {
                return TEXT_DECODER.decode(slice).replace(/\0+$/, '');
            }
        }
    }

    // Fall back to NUL-terminated.
    const cap = Math.min(data.length, offset + maxLength);
    let end = cap;
    for (let i = offset; i < cap; i++) {
        if (data[i] === 0) {
            end = i;
            break;
        }
    }
    const slice = data.subarray(offset, end);
    if (!looksText(slice)) return '';
    return TEXT_DECODER.decode(slice);
}

/**
 * Resolve a (possibly fake) pointer to a string. Returns an empty
 * string if the pointer doesn't decode or the bytes aren't text.
 */
function readStringPointer(loaded, pointer) {
    const ref = decodeFakePointer(pointer, loaded.sectionsOriginal.length);
    if (ref === null) return '';
    return readGrannyString(loaded, ref);
}

// --- parseTypeTree ----------------------------------------------------

/**
 * Walk a `DataTypeDefinition` chain starting at `ref` (a
 * `[section, offset]` pair, typically `file.header.root_type` or a
 * member's `referenceType`). Returns an array of member descriptors
 * terminating at the MT_END sentinel.
 *
 * Each member is read from a 32-byte record :
 *   u32 member_type
 *   u32 name_ptr        (fake pointer → string)
 *   u32 type_ptr        (fake pointer → sub-type tree, or 0)
 *   u32 array_width     (default 1 if 0)
 *   u32 extra[3]
 *   u32 _unused
 *
 * Note : the on-disk pointer slots are pointer-sized (4 or 8 bytes), but
 * for the all-32-bit iRO corpus they fit exactly in 32 bits.
 *
 * @param {LoadedGR2} loaded
 * @param {SectionRef} ref — `[section, offset]` of the type-definition chain.
 * @param {ParseTypeTreeOptions} [options]
 * @returns {readonly TypeMember[]} member descriptors in source order.
 */
export function parseTypeTree(loaded, ref, options = {}) {
    const maxMembers = options.maxMembers ?? 512;
    const section = ref[0];
    const startOffset = ref[1];
    if (section >= loaded.sectionsFixed.length) return [];
    const data = loaded.sectionsFixed[section];
    const u32 = makeU32Reader(loaded.file.header.byte_reversed);
    const sectionCount = loaded.sectionsFixed.length;
    const entries = [];
    let offset = startOffset;
    for (let i = 0; i < maxMembers; i++) {
        if (offset + 32 > data.length) break;
        const memberType = u32(data, offset);
        if (memberType === MT_END) break;
        const namePtr = u32(data, offset + 4);
        const typePtr = u32(data, offset + 8);
        const arrayWidth = u32(data, offset + 12);
        const extra0 = u32(data, offset + 16);
        const extra1 = u32(data, offset + 20);
        const extra2 = u32(data, offset + 24);
        const name = readStringPointer(loaded, namePtr) || `member_${entries.length}`;
        const referenceType = decodeFakePointer(typePtr, sectionCount);
        entries.push({
            memberType,
            memberTypeName: MEMBER_TYPE_NAMES[memberType] ?? `type_${memberType}`,
            name,
            referenceType,
            arrayWidth: arrayWidth || 1,
            extra: /** @type {[number, number, number]} */ ([extra0, extra1, extra2]),
            offset,
        });
        offset += 32;
    }
    return entries;
}

// --- member sizing ----------------------------------------------------

/** Byte size of a member in its parent struct's storage layout. */
function memberStorageSize(member, pointerSize) {
    const width = member.arrayWidth || 1;
    const t = member.memberType;
    const scalar = SCALAR_SIZES[t];
    if (scalar !== undefined) return scalar * width;
    if (t === MT_REFERENCE || t === MT_EMPTY_REFERENCE || t === MT_STRING) {
        return pointerSize;
    }
    if (t === MT_REFERENCE_TO_ARRAY || t === MT_ARRAY_OF_REFERENCES) {
        return 4 + pointerSize;
    }
    if (t === MT_VARIANT_REFERENCE) return pointerSize * 2;
    if (t === MT_REFERENCE_TO_VARIANT_ARRAY) return pointerSize + 4 + pointerSize;
    if (t === MT_TRANSFORM) return 68;
    if (t === MT_INLINE) return 0; // computed via objectStorageSize
    return pointerSize;
}

/**
 * Recursive total of `members`' storage sizes. INLINE members recurse
 * into their sub-type. The `seen` set guards against cyclic schemas
 * (Granny's schema is acyclic in practice but the guard is cheap).
 *
 * Cycle protection is per-recursion-stack : each sibling member starts
 * with a *fresh copy* of the caller's `seen` set. Without the copy,
 * structs with multiple inline siblings of the same sub-type (e.g.
 * TransformTrack's three identical CurveData inline members) collapse
 * to the size of one — a stride bug invisible to S5/S6 but fatal to
 * S7 animation walks, where `objectStorageSize` drives array stride.
 *
 * A shared sub-type in a DAG of inline members is sized once and cached in
 * `memo` (keyed by the sub-type ref), collapsing the otherwise-`B^D` re-walk
 * a crafted file could force. The memo-hit path **adds** the cached size (it
 * does not skip like the cycle guard), so sibling members of the same type
 * still sum correctly — the stride correctness above is preserved. For any
 * acyclic schema (all real assets) a type's size is context-independent, so
 * the memoized value equals the full recursion result → byte-exact.
 *
 * @param {LoadedGR2} loaded
 * @param {readonly TypeMember[]} members
 * @param {number} pointerSize
 * @param {Set<number>} seen — cycle guard ; pass a fresh `Set` at the call site.
 * @param {Record<number, number>} [memo] - per-call size cache keyed by sub-type ref ; defaults to a fresh `{}`.
 * @returns {number} total storage size in bytes.
 */
export function objectStorageSize(loaded, members, pointerSize, seen, memo = {}) {
    let size = 0;
    for (let i = 0; i < members.length; i++) {
        const member = members[i];
        if (member.memberType === MT_INLINE && member.referenceType) {
            const key = member.referenceType[0] * FAKE_SECTION_STRIDE + member.referenceType[1];
            if (seen.has(key)) continue;
            if (memo[key] !== undefined) {
                size += memo[key];
                continue;
            }
            const childSeen = new Set(seen);
            childSeen.add(key);
            const sub = parseTypeTree(loaded, member.referenceType);
            const subSize = objectStorageSize(loaded, sub, pointerSize, childSeen, memo);
            memo[key] = subSize;
            size += subSize;
        } else {
            size += memberStorageSize(member, pointerSize);
        }
    }
    return size;
}

// --- parseObject ------------------------------------------------------

// Cap on nested INLINE recursion depth in `parseObject`. INLINE members
// have storage size 0, so the offset/length guard can never stop a
// self-referential or pathologically deep inline chain — a crafted file
// would recurse until the JS stack overflows. The deepest real RO asset
// nests inline structs 1 level (measured across the 21-fixture corpus),
// so 64 is generous headroom while staying well under any stack limit.
export const MAX_INLINE_DEPTH = 64;

/**
 * Materialize one instance against its `typeTree`, walking
 * `loaded.sectionsFixed[ref[0]]` from `ref[1]`. Returns a plain JS
 * object keyed by member name :
 *
 *     {
 *       Meshes:     { type: 'array_of_references', count, target,
 *                     element_refs: [{section, offset}, ...] },
 *       Skeletons:  { ... },
 *       Animations: { ... },
 *       ...
 *     }
 *
 * Scalars are read from `sectionsOriginal` (unmodified bytes) ; pointers
 * are read from `sectionsFixed` (rewritten with fake pointers by
 * `loadGR2`).
 *
 * @param {LoadedGR2} loaded
 * @param {readonly TypeMember[]} typeTree
 * @param {SectionRef} ref — `[section, offset]` of the instance.
 * @param {ParseObjectOptions} [options]
 * @param {number} [depth] - internal: current INLINE recursion depth (caps at {@link MAX_INLINE_DEPTH}).
 * @param {Set<number> | null} [seen] - internal: INLINE type-refs on the recursion stack (cycle guard); allocated lazily.
 * @returns {ParsedObject} field map keyed by member name.
 */
export function parseObject(loaded, typeTree, ref, options = {}, depth = 0, seen = null) {
    const maxArrayRefs = options.maxArrayRefs ?? 256;
    const pointerSize = loaded.pointerSize;
    const byteReversed = loaded.file.header.byte_reversed;
    const section = ref[0];
    const startOffset = ref[1];
    if (section >= loaded.sectionsFixed.length) return /** @type {ParsedObject} */ ({});
    const fixed = loaded.sectionsFixed[section];
    const raw = loaded.sectionsOriginal[section];
    const sectionCount = loaded.sectionsFixed.length;
    const u32 = makeU32Reader(byteReversed);
    const rawView = new DataView(raw.buffer, raw.byteOffset, raw.byteLength);
    const little = !byteReversed;

    const out = /** @type {Record<string, ParsedField>} */ ({});
    let offset = startOffset;
    for (let i = 0; i < typeTree.length; i++) {
        const member = typeTree[i];
        const size = memberStorageSize(member, pointerSize);
        if (offset + size > fixed.length) break;
        const field = {
            type: member.memberTypeName,
            offset,
        };
        if (member.referenceType) {
            field.reference_type = { section: member.referenceType[0], offset: member.referenceType[1] };
        }
        const t = member.memberType;
        if (t === MT_REFERENCE || t === MT_EMPTY_REFERENCE) {
            const pointer = readPointer(fixed, offset, pointerSize, byteReversed);
            field.target = refDict(decodeFakePointer(pointer, sectionCount));
        } else if (t === MT_STRING) {
            const pointer = readPointer(fixed, offset, pointerSize, byteReversed);
            field.value = readStringPointer(loaded, pointer);
            field.target = refDict(decodeFakePointer(pointer, sectionCount));
        } else if (t === MT_REFERENCE_TO_ARRAY || t === MT_ARRAY_OF_REFERENCES) {
            const count = u32(raw, offset);
            const pointer = readPointer(fixed, offset + 4, pointerSize, byteReversed);
            const target = decodeFakePointer(pointer, sectionCount);
            field.count = count;
            field.target = refDict(target);
            field.truncated = count > maxArrayRefs;
            if (t === MT_ARRAY_OF_REFERENCES) {
                field.element_refs = readArrayReferences(loaded, target, count, maxArrayRefs);
            }
        } else if (t === MT_REFERENCE_TO_VARIANT_ARRAY) {
            const typePointer = readPointer(fixed, offset, pointerSize, byteReversed);
            const count = u32(raw, offset + pointerSize);
            const objectPointer = readPointer(fixed, offset + pointerSize + 4, pointerSize, byteReversed);
            field.variant_type = refDict(decodeFakePointer(typePointer, sectionCount));
            field.count = count;
            field.target = refDict(decodeFakePointer(objectPointer, sectionCount));
            field.truncated = count > maxArrayRefs;
        } else if (t === MT_VARIANT_REFERENCE) {
            const typePointer = readPointer(fixed, offset, pointerSize, byteReversed);
            const objectPointer = readPointer(fixed, offset + pointerSize, pointerSize, byteReversed);
            field.variant_type = refDict(decodeFakePointer(typePointer, sectionCount));
            field.target = refDict(decodeFakePointer(objectPointer, sectionCount));
        } else if (t === MT_INT32) {
            field.value = rawView.getInt32(offset, little);
        } else if (t === MT_UINT32) {
            field.value = rawView.getUint32(offset, little);
        } else if (t === MT_INT16) {
            field.value = rawView.getInt16(offset, little);
        } else if (t === MT_UINT16) {
            field.value = rawView.getUint16(offset, little);
        } else if (t === MT_INT8) {
            field.value = rawView.getInt8(offset);
        } else if (t === MT_UINT8) {
            field.value = rawView.getUint8(offset);
        } else if (t === MT_REAL32) {
            field.value = rawView.getFloat32(offset, little);
        } else if (t === MT_TRANSFORM) {
            field.value = readTransform(loaded, section, offset);
        } else if (t === MT_INLINE && member.referenceType) {
            const key = member.referenceType[0] * FAKE_SECTION_STRIDE + member.referenceType[1];
            if (depth >= MAX_INLINE_DEPTH) {
                throw new GrannyParseError(
                    `inline nesting exceeds MAX_INLINE_DEPTH (${MAX_INLINE_DEPTH}) ` +
                    `at section ${section} offset 0x${offset.toString(16)}`,
                );
            }
            if (seen !== null && seen.has(key)) {
                throw new GrannyParseError(
                    `self-referential inline type [${member.referenceType[0]}, ${member.referenceType[1]}] ` +
                    `at section ${section} offset 0x${offset.toString(16)}`,
                );
            }
            const childSeen = seen !== null ? new Set(seen) : new Set();
            childSeen.add(key);
            const subTree = parseTypeTree(loaded, member.referenceType);
            const subRef = /** @type {SectionRef} */ ([section, offset]);
            field.inline = parseObject(loaded, subTree, subRef, options, depth + 1, childSeen);
            const subSize = objectStorageSize(loaded, subTree, pointerSize, new Set());
            offset += subSize;
            out[member.name] = field;
            continue;
        }
        out[member.name] = field;
        offset += size;
    }
    return out;
}

/**
 * Walk an `ArrayOfReferences` payload : `count` pointer-sized slots
 * starting at `arrayRef`. Returns the decoded `[section, offset]`
 * targets, capped at `maxCount`.
 */
function readArrayReferences(loaded, arrayRef, count, maxCount) {
    if (arrayRef === null || count <= 0) return [];
    const pointerSize = loaded.pointerSize;
    const byteReversed = loaded.file.header.byte_reversed;
    const data = loaded.sectionsFixed[arrayRef[0]];
    if (!data) return [];
    const sectionCount = loaded.sectionsFixed.length;
    const limit = count < maxCount ? count : maxCount;
    const refs = new Array(limit);
    let n = 0;
    for (let i = 0; i < limit; i++) {
        const off = arrayRef[1] + i * pointerSize;
        if (off + pointerSize > data.length) break;
        const pointer = readPointer(data, off, pointerSize, byteReversed);
        const decoded = decodeFakePointer(pointer, sectionCount);
        if (decoded === null) continue;
        refs[n++] = { section: decoded[0], offset: decoded[1] };
    }
    refs.length = n;
    return refs;
}

/**
 * Walk an inline-struct array : `count` objects of `typeRef`-shaped struct,
 * packed back-to-back starting at `arrayRef`. Returns each as
 * `{ref, fields}` where `ref` is the per-object `{section, offset}` (needed
 * by callers to read raw bytes — e.g. SkeletonBone's 68-byte Transform —
 * located at field-offsets inside the object's section) and `fields` is the
 * `parseObject` materialization.
 *
 * Port of blendergranny `types.read_reference_array_objects`. Null-safe :
 * returns `[]` when either ref is null or count is non-positive.
 *
 * @param {LoadedGR2} loaded
 * @param {RefDict | null} arrayRef
 * @param {number} count
 * @param {RefDict | null} typeRef
 * @param {ReadReferenceArrayObjectsOptions} [options]
 * @returns {readonly ReferenceArrayObject[]}
 */
export function readReferenceArrayObjects(loaded, arrayRef, count, typeRef, options = {}) {
    if (arrayRef === null || typeRef === null || count <= 0) return [];
    const maxCount = options.maxCount ?? 64;
    const maxArrayRefs = options.maxArrayRefs ?? maxCount;
    const typeMembers = parseTypeTree(loaded, [typeRef.section, typeRef.offset]);
    const stride = objectStorageSize(loaded, typeMembers, loaded.pointerSize, new Set());
    if (stride <= 0) return [];
    const limit = count < maxCount ? count : maxCount;
    const out = new Array(limit);
    const parseOpts = { maxArrayRefs };
    for (let i = 0; i < limit; i++) {
        const offset = arrayRef.offset + i * stride;
        const ref = { section: arrayRef.section, offset };
        const fields = parseObject(loaded, typeMembers, [arrayRef.section, offset], parseOpts);
        out[i] = { ref, fields };
    }
    return out;
}

/** Stable `{section, offset}` dict shape for refs (null-safe). */
function refDict(ref) {
    if (ref === null) return null;
    return { section: ref[0], offset: ref[1] };
}

// --- internal test surface --------------------------------------------

/** Internals exposed to the unit-test suite. Not part of the public API. */
export const __test__ = {
    makeU32Reader,
    readPointer,
    writePointer,
    readGrannyString,
    readStringPointer,
    memberStorageSize,
    objectStorageSize,
    readArrayReferences,
    looksText,
    SCALAR_SIZES,
};
