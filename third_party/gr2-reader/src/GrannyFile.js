// GrannyFile.js — GR2 header + section table reader.
//
// JS port of Rasetsuu/blendergranny io_scene_gr2/gr2/{file,binary,constants}.py
// (subset : header + section table only ; DataTypeDefinition / RootObject
// walking is granny-pipeline session S5).
//
// Binary reference : docs/gr2-format.md.

/** Acceptable input shapes for the parser.
 * @typedef {ArrayBuffer | Uint8Array | DataView | ArrayBufferView} GR2Input */

/** Quad of u32 magic words (4 × 4 bytes at the file start).
 * @typedef {readonly [number, number, number, number]} GR2Magic */

/** Compression tag, see {@link COMPRESSION_NAMES}.
 * @typedef {0 | 1 | 2 | 3 | 4} CompressionTag */

/** Granny section-slot index, see {@link SECTION_NAMES}.
 * @typedef {0 | 1 | 2 | 3 | 4 | 5 | 6 | 7} SectionIndex */

/**
 * One entry in the GR2 section table (44 bytes on disk — see
 * `docs/gr2-format.md` § Section record). `compression_name` and
 * `semantic_name` are computed accessors ; the rest map 1-to-1 to the
 * on-disk u32s.
 *
 * @typedef {object} GR2Section
 * @property {number} index — 0-based position in the section table.
 * @property {number} compression — compression algorithm tag (0=none, 1=Oodle0, 2=Oodle1, 3=BitKnit, 4=BitKnit2).
 * @property {number} data_offset — offset of this section's compressed bytes, relative to the file start.
 * @property {number} data_size — length of this section's compressed bytes on disk.
 * @property {number} expanded_size — target length of the section once decompressed.
 * @property {number} internal_alignment — required alignment for this section's data buffer (4 / 8 / …).
 * @property {number} first_16bit — Oodle0 block-stop 1 : decoded-byte offset where the 16-bit length context ends.
 * @property {number} first_8bit — Oodle0 block-stop 2 : decoded-byte offset where the 8-bit length context ends.
 * @property {number} pointer_fixup_offset — pointer-fixup table offset (S5+).
 * @property {number} pointer_fixup_count — pointer-fixup entry count, 12 bytes each.
 * @property {number} mixed_marshalling_offset — mixed-marshalling table offset (S5+).
 * @property {number} mixed_marshalling_count — mixed-marshalling entry count, 12 bytes each.
 * @property {string} compression_name — computed : human name for {@link GR2Section.compression}.
 * @property {string} semantic_name — computed : Granny semantic name for {@link GR2Section.index}.
 */

/**
 * Top-level GR2 file header (~72 bytes for version ≥ 7, ~60 bytes otherwise).
 *
 * @typedef {object} GR2Header
 * @property {number} version — Granny file format version — ≥ 7 across our iRO ver12 corpus.
 * @property {number} total_size — total file size as declared by the writer.
 * @property {number} crc — CRC32 of the file's contents.
 * @property {number} section_array_offset — offset of the section array relative to the end of the magic.
 * @property {number} section_count — number of entries in the section array.
 * @property {readonly [number, number]} root_type — `[section_index, offset_within_section]`.
 * @property {readonly [number, number]} root_object — `[section_index, offset_within_section]`.
 * @property {number} type_tag — type-tag identifying the .gr2 schema generation.
 * @property {readonly number[]} extra_tags — 4 user / auxiliary tag values.
 * @property {number} string_db_crc — string-database CRC (version ≥ 7).
 * @property {readonly number[]} reserved — 3 reserved u32 (version ≥ 7).
 * @property {32 | 64} pointer_size — pointer width baked into the file's serialized references.
 * @property {boolean} byte_reversed — true if u32s are stored byte-reversed.
 */

/**
 * Parsed GR2 file, ready for section decompression.
 *
 * @typedef {object} GR2File
 * @property {GR2Header} header
 * @property {readonly GR2Section[]} sections
 * @property {Uint8Array} data — raw input bytes — kept for sliced reads via {@link GR2File.sectionBytes}.
 * @property {(section: GR2Section) => Uint8Array} sectionBytes — slice of `data`
 *   carrying `section`'s on-disk compressed bytes.
 */

/**
 * Result of magic detection (precedes a full parse).
 *
 * @typedef {object} GR2DetectResult
 * @property {boolean} ok — true if the buffer's first 16 bytes match one of the known magics.
 * @property {boolean} byteReversed — true if u32s should be read big-endian.
 * @property {0 | 32 | 64} pointerSize — pointer width baked into the file (`0` when `ok === false`).
 */

// --- magic words (one quad per supported file variant) ----------------

/** `MAGIC_OLD` — earliest Granny 2.x ; LE u32s, 32-bit pointers.
 * @type {GR2Magic} */
export const MAGIC_OLD  = [0xCAB067B8, 0x0FB16DF8, 0x7E8C7284, 0x1E00195E];
/** `MAGIC_32LE` — standard Granny 2.x, LE u32s, 32-bit pointers. **All iRO ver12 .gr2 use this**.
 * @type {GR2Magic} */
export const MAGIC_32LE = [0xC06CDE29, 0x2B53A4BA, 0xA5B7F525, 0xEEE266F6];
/** `MAGIC_32BE` — `MAGIC_32LE`'s u32s each byte-reversed (big-endian on disk).
 * @type {GR2Magic} */
export const MAGIC_32BE = [0xB595110E, 0x4BB5A56A, 0x502828EB, 0x04B37825];
/** `MAGIC_64LE` — 64-bit-pointer Granny 2.x, LE u32s.
 * @type {GR2Magic} */
export const MAGIC_64LE = [0x5E499BE5, 0x141F636F, 0xA9EB131E, 0xC4EDBE90];
/** `MAGIC_64BE` — `MAGIC_64LE`'s u32s each byte-reversed.
 * @type {GR2Magic} */
export const MAGIC_64BE = [0xE3D49531, 0x624FDC20, 0x3AD036CC, 0x89FF82B1];

/** Number of bytes the magic quad occupies at the file start (16 used + 16 reserved). */
export const MAGIC_SIZE = 32;
/** Bytes per entry in the section array (= 11 × u32). */
export const SECTION_RECORD_SIZE = 44;
/** Number of `extra_tags` u32s in the header. */
export const EXTRA_TAG_COUNT = 4;

/** Compression tag — section bytes are stored raw (no decompression). */
export const COMPRESSION_NONE = 0;
/** Compression tag — RAD Oodle0 classic LZ + arithmetic codec. */
export const COMPRESSION_OODLE0 = 1;
/** Compression tag — RAD Oodle1 (not implemented ; no iRO ver12 asset uses it). */
export const COMPRESSION_OODLE1 = 2;
/** Compression tag — RAD BitKnit (not implemented). */
export const COMPRESSION_BITKNIT = 3;
/** Compression tag — RAD BitKnit2 (not implemented). */
export const COMPRESSION_BITKNIT2 = 4;

/** Compression tag → human name. Used by `GR2Section.compression_name`.
 * @type {Readonly<Record<number, 'none' | 'oodle0' | 'oodle1' | 'bitknit' | 'bitknit2'>>} */
export const COMPRESSION_NAMES = {
    0: 'none',
    1: 'oodle0',
    2: 'oodle1',
    3: 'bitknit',
    4: 'bitknit2',
};

/** Section-slot index → Granny semantic name. See `docs/gr2-format.md` § Section slots.
 * @type {Readonly<Record<number, 'main' | 'rigid_vertex' | 'rigid_index' | 'deformable_vertex' | 'deformable_index' | 'texture' | 'discardable' | 'unloaded'>>} */
export const SECTION_NAMES = {
    0: 'main',
    1: 'rigid_vertex',
    2: 'rigid_index',
    3: 'deformable_vertex',
    4: 'deformable_index',
    5: 'texture',
    6: 'discardable',
    7: 'unloaded',
};

// --- low-level helpers -------------------------------------------------

/**
 * Reverse the byte order of a u32 (big-endian ↔ little-endian).
 * Used to compare the file's first quad against the BE magic constants
 * by reading as LE then byte-swapping each word.
 */
function swapU32(value) {
    const v = value >>> 0;
    return (
        ((v & 0x000000FF) << 24) |
        ((v & 0x0000FF00) << 8)  |
        ((v & 0x00FF0000) >>> 8) |
        ((v & 0xFF000000) >>> 24)
    ) >>> 0;
}

/** Element-wise equality on two arrays of numbers. */
function arrayEq(a, b) {
    const len = a.length;
    if (len !== b.length) return false;
    for (let i = 0; i < len; i++) if (a[i] !== b[i]) return false;
    return true;
}

/**
 * Read the file's first 4 u32 words (the magic quad). When `byteReversed`
 * is `true` the words are byte-swapped on the way out, so they can be
 * compared directly to the LE magic constants.
 */
function magicWords(view, byteReversed) {
    const w = [
        view.getUint32(0,  true),
        view.getUint32(4,  true),
        view.getUint32(8,  true),
        view.getUint32(12, true),
    ];
    if (!byteReversed) return w;
    for (let i = 0; i < 4; i++) w[i] = swapU32(w[i]);
    return w;
}

/**
 * Detect whether a buffer is a known Granny2 file by inspecting its
 * first 16 bytes. Tries LE first, then BE-with-swap. Returns the
 * endianness + pointer width baked into the file so the caller can wire
 * its u32 reader accordingly.
 *
 * @param {GR2Input} buffer — the candidate .gr2 bytes.
 * @returns {GR2DetectResult}
 */
export function detectGR2(buffer) {
    const view = bufferView(buffer);
    if (view.byteLength < MAGIC_SIZE) {
        return { ok: false, byteReversed: false, pointerSize: 0 };
    }
    const little = magicWords(view, false);
    if (arrayEq(little, MAGIC_OLD) || arrayEq(little, MAGIC_32LE)) {
        return { ok: true, byteReversed: false, pointerSize: 32 };
    }
    if (arrayEq(little, MAGIC_64LE)) {
        return { ok: true, byteReversed: false, pointerSize: 64 };
    }
    const swapped = little.map(swapU32);
    if (arrayEq(swapped, MAGIC_OLD) || arrayEq(swapped, MAGIC_32BE)) {
        return { ok: true, byteReversed: true, pointerSize: 32 };
    }
    if (arrayEq(swapped, MAGIC_64BE)) {
        return { ok: true, byteReversed: true, pointerSize: 64 };
    }
    return { ok: false, byteReversed: false, pointerSize: 0 };
}

/**
 * Coerce a user-supplied buffer into a `DataView`. Accepts `ArrayBuffer`,
 * any typed array view, or a `DataView` directly. Throws `TypeError` on
 * anything else.
 */
function bufferView(buffer) {
    if (buffer instanceof DataView) return buffer;
    if (ArrayBuffer.isView(buffer)) {
        return new DataView(buffer.buffer, buffer.byteOffset, buffer.byteLength);
    }
    if (buffer instanceof ArrayBuffer) return new DataView(buffer);
    throw new TypeError('parseGR2File expects ArrayBuffer, DataView or typed array');
}

/**
 * Coerce a user-supplied buffer into a `Uint8Array`. Mirror of
 * {@link bufferView} for byte-level slicing (`sectionBytes` etc.).
 */
function bufferBytes(buffer) {
    if (buffer instanceof Uint8Array) return buffer;
    if (ArrayBuffer.isView(buffer)) {
        return new Uint8Array(buffer.buffer, buffer.byteOffset, buffer.byteLength);
    }
    if (buffer instanceof ArrayBuffer) return new Uint8Array(buffer);
    throw new TypeError('parseGR2File expects ArrayBuffer, DataView or typed array');
}

/**
 * Build a bounds-checked u32 reader closed over `view` + endianness.
 * Throws `RangeError` if the requested offset escapes the buffer ; this
 * is the single chokepoint for « truncated file » errors.
 *
 * @param {DataView} view
 * @param {boolean} byteReversed
 * @returns {(offset: number) => number}
 */
function makeReader(view, byteReversed) {
    const little = !byteReversed;
    const cap = view.byteLength;
    return (offset) => {
        if (offset < 0 || offset + 4 > cap) {
            throw new RangeError(`gr2 read out of range at 0x${offset.toString(16)} (cap 0x${cap.toString(16)})`);
        }
        return view.getUint32(offset, little);
    };
}

// --- main entry --------------------------------------------------------

/**
 * Parse a GR2 buffer into `{ header, sections, data, sectionBytes(...) }`.
 *
 * Reads the 32-byte magic, the post-magic header (60 or 72 bytes depending
 * on `version`), and the section array. Does NOT decompress section
 * payloads — call `decompressSection(section, file.sectionBytes(section))`
 * from `./Granny.js` for that.
 *
 * @param {GR2Input} buffer — the .gr2 bytes.
 * @returns {GR2File}
 * @throws {Error} on non-Granny input.
 * @throws {RangeError} when the declared section array escapes the buffer.
 */
export function parseGR2File(buffer) {
    const view = bufferView(buffer);
    const data = bufferBytes(buffer);
    const detect = detectGR2(view);
    if (!detect.ok) throw new Error('not a Granny2 file');

    const u32 = makeReader(view, detect.byteReversed);
    const h = MAGIC_SIZE;

    const version             = u32(h);
    const totalSize           = u32(h + 4);
    const crc                 = u32(h + 8);
    const sectionArrayOffset  = u32(h + 12);
    const sectionCount        = u32(h + 16);
    const rootType   = /** @type {[number, number]} */ ([u32(h + 20), u32(h + 24)]);
    const rootObject = /** @type {[number, number]} */ ([u32(h + 28), u32(h + 32)]);
    const typeTag    = u32(h + 36);

    const extraTags = new Array(EXTRA_TAG_COUNT);
    for (let i = 0; i < EXTRA_TAG_COUNT; i++) extraTags[i] = u32(h + 40 + i * 4);

    let stringDbCrc = 0;
    let reserved = [];
    if (version >= 7) {
        stringDbCrc = u32(h + 56);
        reserved = [u32(h + 60), u32(h + 64), u32(h + 68)];
    }

    const sections = new Array(sectionCount);
    const sectionsBase = MAGIC_SIZE + sectionArrayOffset;
    for (let index = 0; index < sectionCount; index++) {
        const off = sectionsBase + index * SECTION_RECORD_SIZE;
        sections[index] = {
            index,
            compression:             u32(off),
            data_offset:             u32(off + 4),
            data_size:               u32(off + 8),
            expanded_size:           u32(off + 12),
            internal_alignment:      u32(off + 16),
            first_16bit:             u32(off + 20),
            first_8bit:              u32(off + 24),
            pointer_fixup_offset:    u32(off + 28),
            pointer_fixup_count:     u32(off + 32),
            mixed_marshalling_offset: u32(off + 36),
            mixed_marshalling_count:  u32(off + 40),
            /** Computed : human name for {@link compression}. */
            get compression_name() { return COMPRESSION_NAMES[this.compression] ?? `unknown_${this.compression}`; },
            /** Computed : Granny semantic name for the slot at this {@link index}. */
            get semantic_name()    { return SECTION_NAMES[this.index] ?? `section_${this.index}`; },
        };
    }

    const header = {
        version,
        total_size: totalSize,
        crc,
        section_array_offset: sectionArrayOffset,
        section_count: sectionCount,
        root_type: rootType,
        root_object: rootObject,
        type_tag: typeTag,
        extra_tags: extraTags,
        string_db_crc: stringDbCrc,
        reserved,
        pointer_size: detect.pointerSize,
        byte_reversed: detect.byteReversed,
    };

    return /** @type {GR2File} */ ({
        header,
        sections,
        data,
        /**
         * Slice of `data` carrying `section`'s on-disk compressed bytes.
         * @throws RangeError if the declared offset/size escape the buffer
         */
        sectionBytes(section) {
            const start = section.data_offset;
            const end = start + section.data_size;
            if (start < 0 || end > data.byteLength) {
                throw new RangeError(`section ${section.index} points outside file`);
            }
            return data.subarray(start, end);
        },
    });
}
