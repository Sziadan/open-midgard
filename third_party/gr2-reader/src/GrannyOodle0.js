// GrannyOodle0.js — Oodle0 decompressor.
//
// JS port of Rasetsuu/blendergranny io_scene_gr2/gr2/decompress/oodle0.py,
// kept structure-preserving so the Python source remains a useful side
// reference during debugging.
//
// The codec is the classic RAD Tools "Oodle0" (the 2002-era LZ + adaptive
// arithmetic coder embedded in granny2.dll — distinct from modern Oodle
// Kraken / Mermaid / Leviathan). Asm-cite oracle if anything misbehaves :
// the leaked sgzwiz/misc_microsoft_gamedev_source_code path
// //jeffr/granny/rt/granny_oodle0_compression.cpp byte-matches our DLL.
//
// Binary reference : docs/gr2-format.md § Oodle0 bitstream.

/**
 * Structural subset of {@link import('./GrannyFile.js').GR2Section} actually
 * consumed by the Oodle0 codec. A full `GR2Section` is assignable wherever
 * this is expected — but a hand-built object with just these 4 fields works
 * too (useful for unit tests that don't want to mock the full section table).
 *
 * @typedef {object} Oodle0SectionInput
 * @property {number} index
 * @property {number} expanded_size
 * @property {number} first_16bit — decoded-byte offset where the 16-bit length context block ends.
 * @property {number} first_8bit — decoded-byte offset where the 8-bit length context block ends.
 */

/**
 * One of the three blocks an Oodle0 section is split into.
 *
 * @typedef {object} Oodle0Block
 * @property {0 | 1 | 2} index
 * @property {number} output_start — decoded-byte offset where this block starts emitting.
 * @property {number} output_end — decoded-byte offset where this block stops.
 * @property {number} output_size — `max(0, output_end - output_start)`.
 * @property {boolean} is_empty
 * @property {Oodle0LZHeader} header
 */

/**
 * Decode plan for one Oodle0 section — 3 blocks back-to-back.
 *
 * @typedef {object} Oodle0Plan
 * @property {number} section_index
 * @property {number} expanded_size
 * @property {readonly Oodle0Block[]} blocks — always {@link OODLE0_BLOCK_COUNT} (3) entries.
 * @property {36} bitstream_offset — constant 36 ; bitstream begins right after
 *   the 3 × 12-byte block headers.
 */

/** Size of the Oodle0 LZ header block (3 × 12 bytes = 9 × u32). */
export const OODLE0_HEADER_SIZE = 36;
/** Number of LZ blocks an Oodle0 section is split into. */
export const OODLE0_BLOCK_COUNT = 3;
/** Bit width of the low-offset alphabet ; back-distance = `low + 1 + (high << OFFSET_SPLIT_SHIFT)`. */
export const OFFSET_SPLIT_SHIFT = 2;
/** Mask covering the `OFFSET_SPLIT_SHIFT` low bits of the low-offset alphabet. */
export const LOW_OFFSET_MASK = (1 << OFFSET_SPLIT_SHIFT) - 1;
/** Largest LZ77 length-context symbol. */
export const MAX_LENS = 64;
/** Special-case length lookup for symbols ≥ `MAX_LENS - 3` (= 61, 62, 63, 64). */
export const LONG_LENGTHS = [MAX_LENS * 2, MAX_LENS * 3, MAX_LENS * 4, MAX_LENS * 8];
/** Low 9 bits select the « max byte value » / « unique byte values » sub-fields of `Oodle0LZHeader`. */
const _OFFSET_BYTE_MASK = 0x1FF;
/** 31-bit unsigned cap — the arith decoder lives in 31 bits. */
const MASK31 = 0x7FFFFFFF;

// --- untrusted-input allocation caps -----------------------------------
// Decompression-bomb guards. `expanded_size` (u32) and the arith alphabet
// sizes (23-bit `unique_offsets`) are file-controlled and drive the two
// big allocations here, both BEFORE any length cross-check fires. The caps
// are O(1) size-field checks placed outside the decode loops (once per
// section / once per model open) so they add no hot-path cost. Ceilings are
// generous — the largest real fixture is 98 KB expanded / 4.24× ratio, so
// these sit ~2700× / ~240× / ~21× above any legit RO asset and only reject
// the absurd (multi-GB alloc, billions of iterations).
/** Absolute ceiling on a section's decompressed size. 256 MiB. */
export const OODLE0_MAX_EXPANDED_SIZE = 256 * 1024 * 1024;
/** Max `expanded_size` as a multiple of the compressed input length. */
export const OODLE0_MAX_EXPAND_RATIO = 1024;
/** Ceiling on an arith model's alphabet size (blocks the 23-bit 8.4M field). */
export const OODLE0_MAX_ALPHABET = 1 << 21; // 2,097,152

/** Raised by the Oodle0 decoder on malformed or out-of-spec input. */
export class DecompressionError extends Error {
    constructor(message) {
        super(message);
        this.name = 'DecompressionError';
    }
}

// Bit-width contract reminders for the arith decoder :
//   - `high` / `low` / `code` live in 31 bits → mask with `& MASK31` after every update.
//   - intermediate arith ops use 32 bits unsigned → fold with `>>> 0`.
//   - `totals[]` / `counts[]` entries are u16 → mask with `& 0xFFFF` after every bump.
// Originally factored as `u31()` / `u32()` / `u16()` wrappers ; S4 inlined
// the call sites (profiler showed ~3% pure call overhead on the hot path).

// --- LZ header record --------------------------------------------------

/**
 * Per-block LZ header — 12 bytes laid out as 3 × u32. Three of these are
 * packed at the start of every Oodle0 section. See `docs/gr2-format.md` §
 * Oodle0 bitstream for the field-by-field bit split.
 */
export class Oodle0LZHeader {
    /**
     * @param {number} maxOffsetAndByte — raw u32 ; low 9 bits = max literal value, high 23 = max back-distance.
     * @param {number} uniqOffsetAndByte — raw u32 ; low 9 bits = literal alphabet size, high 23 = offset alphabet size.
     * @param {number} uniqLens — raw u32 ; 4 × u8 unique-symbol count, one per length-context group.
     */
    constructor(maxOffsetAndByte, uniqOffsetAndByte, uniqLens) {
        this.max_offset_and_byte = maxOffsetAndByte >>> 0;
        this.uniq_offset_and_byte = uniqOffsetAndByte >>> 0;
        this.uniq_lens = uniqLens >>> 0;
    }
    /** Max literal value the block emits — low 9 bits of `max_offset_and_byte`. */
    get max_byte_value()     { return this.max_offset_and_byte & _OFFSET_BYTE_MASK; }
    /** Max LZ77 back-distance — high 23 bits of `max_offset_and_byte`. */
    get max_offset()         { return this.max_offset_and_byte >>> 9; }
    /** Literal-alphabet size for the block's `bytes` arith model. */
    get unique_byte_values() { return this.uniq_offset_and_byte & _OFFSET_BYTE_MASK; }
    /** Offset-alphabet size for the block's `offset_high` arith model. */
    get unique_offsets()     { return this.uniq_offset_and_byte >>> 9; }
    /**
     * Per-length-context unique-symbol count. The 65 length symbols
     * (`0..MAX_LENS`) split into 4 groups of 16 ; each group gets its
     * own arith-model sizing taken from one of the 4 bytes of `uniq_lens`
     * (MSB-first per group).
     *
     * @param {number} index — 0-based length symbol (`0..MAX_LENS`).
     * @returns {number} unique-symbol count for the containing group.
     */
    length_unique(index) {
        const group = Math.min((index / (MAX_LENS / 4)) | 0, 3);
        return (this.uniq_lens >>> ((3 - group) * 8)) & 0xFF;
    }
}

/**
 * Derive the 4 decoded-byte stops that delimit the 3 blocks of an Oodle0
 * section, clamped to `[0, expanded_size]` and with `first_8bit` forced
 * to ≥ `first_16bit` (the writer is allowed to lie ; we don't trust the
 * raw values).
 */
function blockStops(section) {
    const expanded = section.expanded_size;
    const first16 = clampStop(section.first_16bit, expanded);
    let first8 = clampStop(section.first_8bit, expanded);
    if (first8 < first16) first8 = first16;
    return [0, first16, first8, expanded];
}

/** Clamp a single stop value into `[0, expandedSize]`. */
function clampStop(value, expandedSize) {
    if (value <= 0) return 0;
    if (value >= expandedSize) return expandedSize;
    return value;
}

/**
 * Parse Oodle0's 36-byte LZ header into a 3-block decode plan : per-block
 * arith-model sizing + the decoded-byte ranges each block must emit.
 *
 * Used internally by {@link decompressOodle0} ; exposed for unit testing.
 *
 * @param {Oodle0SectionInput} section
 * @param {Uint8Array} compressed — the 36-byte LZ header + bitstream.
 * @returns {Oodle0Plan}
 */
export function parseOodle0Plan(section, compressed) {
    if (compressed.length < OODLE0_HEADER_SIZE) {
        throw new RangeError(`oodle0 section ${section.index} too short for header`);
    }
    const view = new DataView(compressed.buffer, compressed.byteOffset, compressed.byteLength);
    const words = new Array(9);
    for (let i = 0; i < 9; i++) words[i] = view.getUint32(i * 4, true);
    const headers = [
        new Oodle0LZHeader(words[0], words[1], words[2]),
        new Oodle0LZHeader(words[3], words[4], words[5]),
        new Oodle0LZHeader(words[6], words[7], words[8]),
    ];
    const stops = blockStops(section);
    const blocks = new Array(OODLE0_BLOCK_COUNT);
    for (let i = 0; i < OODLE0_BLOCK_COUNT; i++) {
        const start = stops[i];
        const end = stops[i + 1];
        blocks[i] = {
            index: i,
            output_start: start,
            output_end: end,
            output_size: Math.max(0, end - start),
            is_empty: end <= start,
            header: headers[i],
        };
    }
    return {
        section_index: section.index,
        expanded_size: section.expanded_size,
        blocks,
        bitstream_offset: OODLE0_HEADER_SIZE,
    };
}

// --- bit-level + arithmetic readers ------------------------------------

/**
 * Read an unsigned u32 LE from `data` at `offset`, zero-padding past EOF.
 * The Python codec relies on this implicit pad (« else: chunk =
 * data[offset:] + b'\\x00' * ... ») ; DataView would throw at the bounds.
 */
function u32lePadded(data, offset) {
    const cap = data.length;
    if (offset + 4 <= cap) {
        return (
            (data[offset]            |
             (data[offset + 1] << 8) |
             (data[offset + 2] << 16) |
             (data[offset + 3] << 24)) >>> 0
        );
    }
    let v = 0;
    if (offset     < cap) v |= data[offset];
    if (offset + 1 < cap) v |= data[offset + 1] << 8;
    if (offset + 2 < cap) v |= data[offset + 2] << 16;
    if (offset + 3 < cap) v |= data[offset + 3] << 24;
    return v >>> 0;
}

/**
 * Reverse the low `nbits` of `value`. Used at multiple points in the arith
 * decoder where byte / nibble groups need to be read MSB-first :
 * `code = bitReverse(get(31), 31)` on init, plus byte / nibble swaps
 * inside `ArithBits.remove()`. Missing one bit-reverse = silent mismatch.
 *
 * @param {number} value
 * @param {number} nbits — 0..31.
 * @returns {number}
 */
export function bitReverse(value, nbits) {
    let result = 0;
    for (let i = 0; i < nbits; i++) {
        result = (result << 1) | ((value >>> i) & 1);
    }
    return result >>> 0;
}

/**
 * Streaming bit-level reader over an LE u32 buffer.
 *
 * - `get(n)` consumes `n` ∈ `[0, 31]` bits, refilling from the next u32
 *   when the internal `bits` cache runs dry.
 * - `get1()` consumes a single bit, fast-path.
 *
 * Past EOF the underlying read returns zero-padded u32s via
 * {@link u32lePadded} (matches the Python codec's implicit padding).
 */
class VarBits {
    constructor(data, offset) {
        this.data = data;
        this.cur = offset;
        this.bits = 0;
        this.bitlen = 0;
    }
    /** Consume `nbits` bits ; return them as an unsigned int (`0..2^nbits - 1`). */
    get(nbits) {
        if (nbits === 0) return 0;
        // nbits is at most 31 in this codec (largest call is get(31) for
        // ArithBits init). So mask fits in i32 positive.
        const mask = ((1 << nbits) >>> 0) - 1;
        if (this.bitlen >= nbits) {
            const value = this.bits & mask;
            this.bits = this.bits >>> nbits;
            this.bitlen -= nbits;
            return value;
        }
        // Need more bits — read next word and merge.
        const word = u32lePadded(this.data, this.cur);
        this.cur += 4;
        // To avoid int32 overflow on `word << bitlen` (would lose high bits
        // when bitlen > 0), pre-mask `word` to the bits we actually need
        // from it, then shift up. Result is < 2^nbits ≤ 2^31 → fits i32.
        const need = nbits - this.bitlen;          // bits needed from word, ≥ 1
        const lowMask = ((1 << need) >>> 0) - 1;
        const wordLow = word & lowMask;
        const value = (this.bits | (wordLow << this.bitlen)) >>> 0;
        // bits = word >> (nbits - bitlen) — `need` ≤ 31, plain >>> is safe.
        this.bits = word >>> need;
        this.bitlen = this.bitlen + 32 - nbits;
        return value;
    }
    /** Consume exactly 1 bit (`0` or `1`). Fast path for the inner decode loop. */
    get1() {
        if (this.bitlen) {
            const value = this.bits & 1;
            this.bits = this.bits >>> 1;
            this.bitlen -= 1;
            return value;
        }
        const word = u32lePadded(this.data, this.cur);
        this.cur += 4;
        this.bits = word >>> 1;
        this.bitlen = 31;
        return word & 1;
    }
}

/**
 * 31-bit arithmetic-coding decoder, sitting on top of {@link VarBits}.
 *
 * State lives in 31 bits everywhere (`high`, `low`, `code` are all
 * `& MASK31` after every update — see the `u31` wrapper). `code` is
 * initialized from `bitReverse(get(31), 31)` — read MSB-first.
 *
 * The decoder doesn't track symbol semantics ; an {@link ArithModel}
 * sits on top and calls `getCount(scale)` + `remove(start, count, scale)`
 * to advance.
 */
class ArithBits {
    constructor(data, offset) {
        this.vbits = new VarBits(data, offset);
        this.high = MASK31;
        this.low = 0;
        this.code = bitReverse(this.vbits.get(31), 31);
    }
    /** Compute the cumulative count `c` such that the current code falls in `[start, start+c]` for a given `scale`. */
    getCount(scale) {
        if (scale <= 0) return 0;
        // ((code - low + 1) * scale - 1) // (high - low + 1)
        const width = this.high - this.low + 1;
        return Math.floor(((this.code - this.low + 1) * scale - 1) / width);
    }
    /** Decode an integer value in `[0, scale)` directly (used for escape symbols). */
    getValue(scale) {
        let value = this.getCount(scale);
        if (value >= scale) value = scale - 1;
        this.remove(value, 1, scale);
        return value;
    }
    /**
     * Advance the decoder by removing the `[start, start+count)` interval
     * from the current `[low, high]` range. Performs the standard arith-
     * coding renormalization (8-bit + 4-bit + 1-bit unscaled rounds, then
     * the underflow loop) — see RAD's leaked source for asm-cite parity.
     */
    remove(start, count, scale) {
        if (scale <= 0) return;
        let high = this.high;
        let low = this.low;
        let code = this.code;
        const width = (high - low) + 1;
        // width up to 2^31, (start+count) up to ~scale ~ 16384 — product fits
        // in a Number ; do not >>> 0 the multiplication (would truncate).
        high = (low + Math.floor((width * (start + count)) / scale) - 1) >>> 0;
        low  = (low + Math.floor((width * start) / scale)) >>> 0;
        if (((high ^ low) & 0x40000000) === 0) {
            // 8-bit shifts as long as top byte agrees. The `>>> 0` casts
            // double as u32-type hints to V8 — dropping them regresses
            // throughput ~8% (S4 measured).
            while (((high ^ low) & 0x7F800000) === 0) {
                low  = (low << 8) >>> 0;
                high = ((high << 8) | 0xFF) >>> 0;
                const byte = this.vbits.get(8);
                code = (
                    (code << 8) |
                    (bitReverse(byte & 0xF, 4) << 4) |
                    bitReverse(byte >>> 4, 4)
                ) >>> 0;
            }
            // Then 4-bit if the next nibble agrees.
            if (((high ^ low) & 0x78000000) === 0) {
                low  = (low << 4) >>> 0;
                high = ((high << 4) | 0xF) >>> 0;
                code = ((code << 4) | bitReverse(this.vbits.get(4), 4)) >>> 0;
            }
            // Final 1-bit loop until the MSBs diverge.
            while (((high ^ low) & 0x40000000) === 0) {
                low  = (low << 1) >>> 0;
                high = ((high << 1) | 1) >>> 0;
                code = ((code << 1) | this.vbits.get1()) >>> 0;
            }
        }
        // Underflow loop — second bit straddles the midpoint.
        while ((low & 0x20000000) && !(high & 0x20000000)) {
            code = (code ^ 0x20000000) >>> 0;
            low  = ((low & 0x1FFFFFFF) << 1) >>> 0;
            high = ((high << 1) | 0x40000001) >>> 0;
            code = ((code << 1) | this.vbits.get1()) >>> 0;
        }
        this.high = high & MASK31;
        this.low  = low & MASK31;
        this.code = code & MASK31;
    }
}

// --- adaptive arithmetic model -----------------------------------------

/**
 * Storage-aligned count : `(n + 5) & ~3`. Rounds the model's `counts` /
 * `values` arrays down to a 4-aligned size with 1..4 slack slots for
 * the escape mechanism.
 */
function alignedCount(uniqueValues) {
    return (uniqueValues + 5) & ~3;
}

/**
 * Pick the best bin layout for a model dimensioned by `value`. Returns
 * `[bin_size, bin_shift, last_bin_start]` — used by `ArithModel.totals`
 * to bucket symbols into 16 cumulative tallies.
 */
function bestShift(value) {
    if (value < 6) return [0, 15, 0];
    let bestMax = 0xFFFFFFFF;
    let bestBin = 0;
    for (let index = 0; index < 16; index++) {
        const size = 1 << index;
        const bins = Math.min(Math.floor((value + size - 1) / size), 16);
        let last = value - (size * (bins - 1));
        if (last < size) last = size;
        if (last < bestMax) {
            bestBin = index;
            bestMax = last;
        }
        if (size > value) break;
    }
    const binSize = 1 << bestBin;
    return [binSize, bestBin, 15 * binSize];
}

/**
 * Sentinel returned by {@link ArithModel.decompress} when the decoded
 * symbol is an escape (new alphabet entry). The caller follows up with
 * `bits.getValue(escapeScale)` to read the actual symbol then calls
 * `setEscaped(marker, value)` to register it.
 */
class EscapeSymbol {
    constructor(index) { this.index = index; }
}

/**
 * Adaptive arithmetic model used for the literal / length / offset
 * alphabets. Each block of an Oodle0 section creates its own set of
 * models (sized by the {@link Oodle0LZHeader}) ; `decompress(bits)`
 * walks the model, emits either a known symbol or an {@link EscapeSymbol},
 * and `_rescale` halves the counts when the total saturates.
 */
class ArithModel {
    constructor(uniqueValues) {
        // Cap the alphabet before the two count/value arrays are sized —
        // `unique_offsets` (23-bit, up to 8.4M) feeds `offset_high` and
        // would otherwise drive ~hundreds of MB of transient JS arrays.
        // One guard here covers all four model kinds (bytes / lengths /
        // offset_low / offset_high) ; legit alphabets are far below the cap.
        if (uniqueValues > OODLE0_MAX_ALPHABET) {
            throw new DecompressionError(
                `oodle0 arith alphabet ${uniqueValues} exceeds cap ${OODLE0_MAX_ALPHABET}`);
        }
        this.unique_values = uniqueValues;
        const count = alignedCount(uniqueValues);
        this.totals = new Array(16).fill(0);
        this.counts = new Array(count).fill(0);
        this.values = new Array(count).fill(0);
        this.number = 0;
        const [binSize, binShift, lastBinStart] = bestShift(uniqueValues + 1);
        this.bin_size = binSize;
        this.bin_shift = binShift;
        this.last_bin_start = lastBinStart;
        this._quickIncrement(0, 3);
    }
    /**
     * Decode one symbol from `bits`. Returns the symbol value as a plain
     * number, or an {@link EscapeSymbol} marker when a new alphabet
     * entry is being introduced.
     *
     * @throws DecompressionError if the model overflows its capacity
     */
    decompress(bits) {
        if (this.totals[15] >= 16384) this._rescale();
        const scale = this.totals[15];
        const count = bits.getCount(scale);
        const [pos, start] = this._findPos(count);
        const oldCount = this.counts[pos];
        this._incrementTotals(pos, 1);
        bits.remove(start, oldCount, this.totals[15] - 1);
        this.counts[pos] = (this.counts[pos] + 1) & 0xFFFF;
        if (pos === 0) {
            this.number += 1;
            if (this.number >= this.counts.length) {
                throw new DecompressionError('oodle0 escape exceeded model capacity');
            }
            this._quickIncrement(this.number, 2);
            if (this.number === this.unique_values) {
                this._decrementCounts(0, this.counts[0]);
            }
            return new EscapeSymbol(this.number);
        }
        return this.values[pos];
    }
    /** Register the value behind an escape marker so subsequent reads find it. */
    setEscaped(marker, value) {
        this.values[marker.index] = value & 0xFFFF;
    }
    /**
     * Find the position whose cumulative count contains `count`.
     *
     * Two-stage : first a 4-compare binary search over the 16-entry
     * cumulative `totals` to find the right bin, then a bounded linear
     * scan within that bin's `counts` slice. The bin layout is what
     * {@link bestShift} was designed for — bin sizes are picked so the
     * within-bin scan stays small.
     */
    _findPos(count) {
        const totals = this.totals;
        // Binary search totals[0..15] for the smallest bin b with count < totals[b].
        let lo = 0;
        let hi = 16;
        while (lo < hi) {
            const mid = (lo + hi) >>> 1;
            if (count < totals[mid]) hi = mid;
            else lo = mid + 1;
        }
        const bin = lo;
        const counts = this.counts;
        const binShift = this.bin_shift;
        let pos;
        let end;
        if (bin < 15) {
            pos = bin << binShift;
            end = pos + (1 << binShift);
            if (end > counts.length) end = counts.length;
        } else {
            pos = this.last_bin_start;
            end = counts.length;
        }
        let start = bin > 0 ? totals[bin - 1] : 0;
        while (pos < end) {
            const entry = counts[pos];
            if (count < start + entry) return [pos, start];
            start += entry;
            pos++;
        }
        throw new DecompressionError(`oodle0 model count ${count} outside total ${start}`);
    }
    /**
     * Increment `counts[value]` AND `totals[bin(value)..15]` by `delta`.
     *
     * The original RAD code packed two parallel u16 increments into a
     * single u32 add ; in our codepaths the packed amount always had
     * identical hi and lo halves (0x10001, 0x20002, 0x30003), so a
     * straight per-bin u16 add gives the same result without the
     * packing dance. {@link _decrementCounts} handles the asymmetric
     * negative-pair case directly.
     */
    _quickIncrement(value, delta) {
        this._incrementTotals(value, delta);
        this.counts[value] = (this.counts[value] + delta) & 0xFFFF;
    }
    /** Add `delta` (u16) to `totals[bin(value)..15]` cumulatively. */
    _incrementTotals(value, delta) {
        const totals = this.totals;
        if (value >= this.last_bin_start) {
            totals[15] = (totals[15] + delta) & 0xFFFF;
            return;
        }
        for (let i = value >>> this.bin_shift; i < 16; i++) {
            totals[i] = (totals[i] + delta) & 0xFFFF;
        }
    }
    /** Decrement `counts[value]` AND `totals[bin(value)..15]` by `amount`. */
    _decrementCounts(value, amount) {
        const totals = this.totals;
        this.counts[value] = (this.counts[value] - amount) & 0xFFFF;
        if (value >= this.last_bin_start) {
            totals[15] = (totals[15] - amount) & 0xFFFF;
            return;
        }
        for (let i = value >>> this.bin_shift; i < 16; i++) {
            totals[i] = (totals[i] - amount) & 0xFFFF;
        }
    }
    /**
     * Halve every `counts` entry and rebuild `totals`. Triggered when the
     * cumulative tally hits 16384. Also drops entries that fall to ≤ 1
     * and re-bins the survivors based on the updated alphabet size.
     */
    _rescale() {
        const [binSize, binShift, lastBinStart] = bestShift(this.number + 1);
        this.bin_size = binSize;
        this.bin_shift = binShift;
        this.last_bin_start = lastBinStart;
        const bins = new Array(16).fill(0);
        this.counts[0] = this.counts[0] >>> 1;
        bins[0 < this.last_bin_start ? 0 : 15] += this.counts[0];
        let maxCount = 0;
        let maxPos = 0;
        let index = 1;
        let done = false;
        while (index <= this.number && !done) {
            while (this.counts[index] <= 1) {
                if (index < this.number) {
                    this.counts[index] = this.counts[this.number];
                    this.values[index] = this.values[this.number];
                    this.counts[this.number] = 0;
                    this.number -= 1;
                } else {
                    this.counts[index] = 0;
                    this.number -= 1;
                    done = true;
                    break;
                }
            }
            if (done) break;
            this.counts[index] = this.counts[index] >>> 1;
            if (this.counts[index] > maxCount) {
                maxCount = this.counts[index];
                maxPos = index;
            }
            const bucket = index < this.last_bin_start ? (index >>> this.bin_shift) : 15;
            bins[bucket] += this.counts[index];
            index += 1;
        }
        if (maxCount) {
            let swapPos;
            if (this.number < this.last_bin_start) {
                swapPos = (this.number >>> this.bin_shift) << this.bin_shift;
            } else {
                swapPos = this.last_bin_start;
            }
            if (swapPos === 0) swapPos = 1;
            if (maxPos !== swapPos) {
                const oldCount = this.counts[swapPos];
                this.counts[swapPos] = this.counts[maxPos];
                const swapBucket = swapPos < this.last_bin_start ? (swapPos >>> this.bin_shift) : 15;
                const maxBucket  = maxPos  < this.last_bin_start ? (maxPos  >>> this.bin_shift) : 15;
                bins[swapBucket] += (-oldCount) + this.counts[swapPos];
                bins[maxBucket]  += oldCount - this.counts[swapPos];
                this.counts[maxPos] = oldCount;
                const tmp = this.values[swapPos];
                this.values[swapPos] = this.values[maxPos];
                this.values[maxPos] = tmp;
            }
        }
        if (this.number !== this.unique_values && this.counts[0] === 0) {
            this.counts[0] = (this.counts[0] + 2) & 0xFFFF;
            bins[0 < this.last_bin_start ? 0 : 15] += 2;
        }
        let running = 0;
        for (let i = 0; i < 16; i++) {
            running += bins[i];
            this.totals[i] = running & 0xFFFF;
        }
    }
}

// --- LZ state ----------------------------------------------------------

/**
 * Per-block LZ77 + arith decoding state. One instance per non-empty
 * block of a section. Carries all the arith models the block needs
 * (literal, 65 length-context, offset-low, offset-high) plus the running
 * decode counter `bytes_decompressed` and the previous length symbol
 * `last_length` used to pick which length-context model to read next.
 */
class LZState {
    constructor(header) {
        this.max_bytes = header.max_byte_value;
        this.max_offsets = header.max_offset;
        this.max_offset_low = Math.min(this.max_offsets, LOW_OFFSET_MASK + 1);
        this.bytes = new ArithModel(header.unique_byte_values);
        const lengths = new Array(MAX_LENS + 1);
        for (let i = 0; i <= MAX_LENS; i++) lengths[i] = new ArithModel(header.length_unique(i));
        this.lengths = lengths;
        this.offset_low = new ArithModel(this.max_offset_low);
        this.offset_high = new ArithModel(header.unique_offsets);
        this.bytes_decompressed = 0;
        this.last_length = 0;
    }
}

/**
 * Read one symbol from `model`. If the model returns an escape marker,
 * read the actual value via `bits.getValue(escapeScale)` and register
 * it back into the model before returning.
 */
function readModelSymbol(model, bits, escapeScale) {
    const value = model.decompress(bits);
    if (value instanceof EscapeSymbol) {
        const escaped = bits.getValue(escapeScale);
        model.setEscaped(value, escaped);
        return escaped;
    }
    return value | 0;
}

// --- main entry --------------------------------------------------------

/**
 * Decompress one Oodle0-tagged section.
 *
 * Walks `parseOodle0Plan(section, compressed)`'s 3 blocks back-to-back,
 * each with its own {@link LZState}, into a pre-allocated
 * `Uint8Array(section.expanded_size)`. Throws if the decoded length
 * doesn't match `section.expanded_size` — full byte-exact check, no
 * "close enough" per [`feedback_no_empirical_closure_re`].
 *
 * @param {Oodle0SectionInput} section — section header from `parseGR2File(...).sections[i]`.
 * @param {Uint8Array} compressed — raw section bytes (`file.sectionBytes(section)`).
 * @returns {Uint8Array} of length `section.expanded_size`.
 * @throws {DecompressionError} on malformed input or length mismatch.
 */
export function decompressOodle0(section, compressed) {
    if (section.expanded_size === 0) return new Uint8Array(0);
    if (compressed.length < OODLE0_HEADER_SIZE) {
        throw new DecompressionError('oodle0 data too short for 3 block headers');
    }
    // Cap the decompressed size before allocating — reject a bomb (huge
    // expanded_size from a tiny section) instead of OOMing on the alloc.
    const expanded = section.expanded_size;
    if (expanded > OODLE0_MAX_EXPANDED_SIZE ||
        expanded > compressed.length * OODLE0_MAX_EXPAND_RATIO) {
        throw new DecompressionError(
            `oodle0 expanded_size ${expanded} exceeds cap ` +
            `(${OODLE0_MAX_EXPANDED_SIZE} B / ${OODLE0_MAX_EXPAND_RATIO}× of ${compressed.length} B)`);
    }
    const plan = parseOodle0Plan(section, compressed);
    const bits = new ArithBits(compressed, OODLE0_HEADER_SIZE);
    const output = new Uint8Array(section.expanded_size);
    let cursor = 0;
    for (let i = 0; i < OODLE0_BLOCK_COUNT; i++) {
        const block = plan.blocks[i];
        if (block.is_empty) continue;
        const state = new LZState(block.header);
        cursor = decodeBlock(state, bits, output, cursor, block.output_end);
    }
    if (cursor !== section.expanded_size) {
        throw new DecompressionError(`oodle0 decompressed ${cursor}, expected ${section.expanded_size}`);
    }
    return output;
}

/**
 * Inner LZ77 loop for one block — emits literals and back-references
 * into `output` until `cursor === stop`.
 *
 * Length symbol `0` = literal ; `1..MAX_LENS-4` = literal length
 * `symbol + 1` ; `MAX_LENS-3..MAX_LENS` = the {@link LONG_LENGTHS}
 * lookup `[128, 192, 256, 512]`. Distance = `low + 1 + (high << OFFSET_SPLIT_SHIFT)`.
 *
 * @throws DecompressionError on invalid distance (≤ 0 or > already-emitted bytes)
 * @throws DecompressionError on invalid literal (outside `0..255`)
 */
function decodeBlock(state, bits, output, cursor, stop) {
    while (cursor < stop) {
        const previousLength = state.last_length;
        const lengthSymbol = readModelSymbol(state.lengths[previousLength], bits, MAX_LENS + 1);
        state.last_length = lengthSymbol;
        if (lengthSymbol) {
            const length =
                lengthSymbol >= MAX_LENS - 3
                    ? LONG_LENGTHS[lengthSymbol - (MAX_LENS - 3)]
                    : lengthSymbol + 1;
            const low = readModelSymbol(state.offset_low, bits, state.max_offset_low);
            const highScale = (Math.min(state.max_offsets, state.bytes_decompressed) >>> OFFSET_SPLIT_SHIFT) + 1;
            const high = readModelSymbol(state.offset_high, bits, highScale);
            const distance = low + 1 + (high << OFFSET_SPLIT_SHIFT);
            if (distance <= 0 || distance > cursor) {
                throw new DecompressionError(`oodle0 invalid copy distance ${distance}`);
            }
            // LZ self-copy : when distance < length, freshly-written bytes
            // must propagate. Cannot vectorize with subarray()/copyWithin().
            const src = cursor - distance;
            for (let k = 0; k < length; k++) {
                output[cursor + k] = output[src + k];
            }
            cursor += length;
            state.bytes_decompressed += length;
        } else {
            const literal = readModelSymbol(state.bytes, bits, state.max_bytes);
            if (literal < 0 || literal > 255) {
                throw new DecompressionError(`oodle0 invalid literal ${literal}`);
            }
            output[cursor++] = literal;
            state.bytes_decompressed += 1;
        }
    }
    return cursor;
}

/**
 * Test-only exports — internals kept here so they don't leak into the
 * public Granny.js surface. Don't import from outside `tests/`.
 */
export const __test__ = {
    VarBits,
    ArithBits,
    ArithModel,
    EscapeSymbol,
    LZState,
    u32lePadded,
    blockStops,
    clampStop,
    alignedCount,
    bestShift,
};
