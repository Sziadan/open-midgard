// Oodle0 decompressor. MIT attribution: third_party/gr2-reader/LICENSE.
#include "Gr2Oodle0.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace ro::gr2 {
namespace {

constexpr int kHeaderSize = 36;
constexpr int kBlockCount = 3;
constexpr int kOffsetSplitShift = 2;
constexpr int kLowOffsetMask = (1 << kOffsetSplitShift) - 1;
constexpr int kMaxLens = 64;
constexpr int kLongLengths[4] = {kMaxLens * 2, kMaxLens * 3, kMaxLens * 4, kMaxLens * 8};
constexpr uint32_t kOffsetByteMask = 0x1FFu;
constexpr uint32_t kMask31 = 0x7FFFFFFFu;
constexpr size_t kMaxExpandedSize = 256u * 1024u * 1024u;
constexpr size_t kMaxExpandRatio = 1024;
constexpr int kMaxAlphabet = 1 << 21;

struct LzHeader {
    uint32_t maxOffsetAndByte = 0;
    uint32_t uniqOffsetAndByte = 0;
    uint32_t uniqLens = 0;
    uint32_t maxByteValue() const { return maxOffsetAndByte & kOffsetByteMask; }
    uint32_t maxOffset() const { return maxOffsetAndByte >> 9; }
    uint32_t uniqueByteValues() const { return uniqOffsetAndByte & kOffsetByteMask; }
    uint32_t uniqueOffsets() const { return uniqOffsetAndByte >> 9; }
    uint32_t lengthUnique(int index) const
    {
        const int group = (std::min)((index / (kMaxLens / 4)), 3);
        return (uniqLens >> ((3 - group) * 8)) & 0xFFu;
    }
};

uint32_t BitReverse(uint32_t value, int nbits)
{
    uint32_t result = 0;
    for (int i = 0; i < nbits; ++i) result = (result << 1) | ((value >> i) & 1u);
    return result;
}

uint32_t U32lePadded(const std::vector<uint8_t>& data, size_t offset)
{
    const size_t cap = data.size();
    if (offset + 4 <= cap) {
        return uint32_t(data[offset]) | (uint32_t(data[offset + 1]) << 8) |
            (uint32_t(data[offset + 2]) << 16) | (uint32_t(data[offset + 3]) << 24);
    }
    uint32_t v = 0;
    if (offset < cap) v |= data[offset];
    if (offset + 1 < cap) v |= uint32_t(data[offset + 1]) << 8;
    if (offset + 2 < cap) v |= uint32_t(data[offset + 2]) << 16;
    if (offset + 3 < cap) v |= uint32_t(data[offset + 3]) << 24;
    return v;
}

class VarBits {
public:
    VarBits(const std::vector<uint8_t>& data, size_t offset)
        : data_(data), cur_(offset) {}
    uint32_t get(int nbits)
    {
        if (nbits == 0) return 0;
        const uint32_t mask = (1u << nbits) - 1u;
        if (bitlen_ >= nbits) {
            const uint32_t value = bits_ & mask;
            bits_ >>= nbits;
            bitlen_ -= nbits;
            return value;
        }
        const uint32_t word = U32lePadded(data_, cur_);
        cur_ += 4;
        const int need = nbits - bitlen_;
        const uint32_t lowMask = (1u << need) - 1u;
        const uint32_t wordLow = word & lowMask;
        const uint32_t value = (bits_ | (wordLow << bitlen_)) & 0xFFFFFFFFu;
        bits_ = word >> need;
        bitlen_ = bitlen_ + 32 - nbits;
        return value;
    }
    uint32_t get1()
    {
        if (bitlen_) {
            const uint32_t value = bits_ & 1u;
            bits_ >>= 1;
            --bitlen_;
            return value;
        }
        const uint32_t word = U32lePadded(data_, cur_);
        cur_ += 4;
        bits_ = word >> 1;
        bitlen_ = 31;
        return word & 1u;
    }
private:
    const std::vector<uint8_t>& data_;
    size_t cur_ = 0;
    uint32_t bits_ = 0;
    int bitlen_ = 0;
};

class ArithBits {
public:
    ArithBits(const std::vector<uint8_t>& data, size_t offset)
        : vbits_(data, offset)
    {
        high_ = kMask31;
        low_ = 0;
        code_ = BitReverse(vbits_.get(31), 31);
    }
    uint32_t getCount(uint32_t scale)
    {
        if (scale == 0) return 0;
        const uint64_t width = uint64_t(high_) - uint64_t(low_) + 1ull;
        return uint32_t(((uint64_t(code_) - uint64_t(low_) + 1ull) * scale - 1ull) / width);
    }
    uint32_t getValue(uint32_t scale)
    {
        uint32_t value = getCount(scale);
        if (value >= scale) value = scale - 1;
        remove(value, 1, scale);
        return value;
    }
    void remove(uint32_t start, uint32_t count, uint32_t scale)
    {
        if (scale == 0) return;
        uint32_t high = high_;
        uint32_t low = low_;
        uint32_t code = code_;
        const uint64_t width = uint64_t(high) - uint64_t(low) + 1ull;
        high = uint32_t(uint64_t(low) + (width * (start + count)) / scale - 1ull);
        low = uint32_t(uint64_t(low) + (width * start) / scale);
        if (((high ^ low) & 0x40000000u) == 0) {
            while (((high ^ low) & 0x7F800000u) == 0) {
                low <<= 8;
                high = (high << 8) | 0xFFu;
                const uint32_t byte = vbits_.get(8);
                code = (code << 8) | (BitReverse(byte & 0xFu, 4) << 4) | BitReverse(byte >> 4, 4);
            }
            if (((high ^ low) & 0x78000000u) == 0) {
                low <<= 4;
                high = (high << 4) | 0xFu;
                code = (code << 4) | BitReverse(vbits_.get(4), 4);
            }
            while (((high ^ low) & 0x40000000u) == 0) {
                low <<= 1;
                high = (high << 1) | 1u;
                code = (code << 1) | vbits_.get1();
            }
        }
        while ((low & 0x20000000u) && !(high & 0x20000000u)) {
            code ^= 0x20000000u;
            low = (low & 0x1FFFFFFFu) << 1;
            high = (high << 1) | 0x40000001u;
            code = (code << 1) | vbits_.get1();
        }
        high_ = high & kMask31;
        low_ = low & kMask31;
        code_ = code & kMask31;
    }
private:
    VarBits vbits_;
    uint32_t high_ = 0, low_ = 0, code_ = 0;
};

int AlignedCount(int uniqueValues) { return (uniqueValues + 5) & ~3; }

void BestShift(int value, int& binSize, int& binShift, int& lastBinStart)
{
    if (value < 6) {
        binSize = 0;
        binShift = 15;
        lastBinStart = 0;
        return;
    }
    uint32_t bestMax = 0xFFFFFFFFu;
    int bestBin = 0;
    for (int index = 0; index < 16; ++index) {
        const int size = 1 << index;
        const int bins = (std::min)((value + size - 1) / size, 16);
        int last = value - (size * (bins - 1));
        if (last < size) last = size;
        if (uint32_t(last) < bestMax) {
            bestBin = index;
            bestMax = uint32_t(last);
        }
        if (size > value) break;
    }
    binSize = 1 << bestBin;
    binShift = bestBin;
    lastBinStart = 15 * binSize;
}

struct EscapeSymbol { int index = 0; };

class ArithModel {
public:
    explicit ArithModel(int uniqueValues)
    {
        if (uniqueValues > kMaxAlphabet) throw std::runtime_error("oodle0 arith alphabet exceeds cap");
        uniqueValues_ = uniqueValues;
        const int count = AlignedCount(uniqueValues);
        totals_.fill(0);
        counts_.assign(count, 0);
        values_.assign(count, 0);
        BestShift(uniqueValues + 1, binSize_, binShift_, lastBinStart_);
        quickIncrement(0, 3);
    }
    // Returns true if escape; value/outIndex set accordingly.
    bool decompress(ArithBits& bits, int& outValue, int& escapeIndex)
    {
        if (totals_[15] >= 16384) rescale();
        const uint32_t scale = totals_[15];
        const uint32_t count = bits.getCount(scale);
        int pos = 0;
        uint32_t start = 0;
        findPos(count, pos, start);
        const uint32_t oldCount = counts_[pos];
        incrementTotals(pos, 1);
        bits.remove(start, oldCount, totals_[15] - 1);
        counts_[pos] = uint16_t((counts_[pos] + 1) & 0xFFFF);
        if (pos == 0) {
            ++number_;
            if (number_ >= int(counts_.size())) throw std::runtime_error("oodle0 escape exceeded model capacity");
            quickIncrement(number_, 2);
            if (number_ == uniqueValues_) decrementCounts(0, counts_[0]);
            escapeIndex = number_;
            return true;
        }
        outValue = values_[pos];
        return false;
    }
    void setEscaped(int index, int value) { values_[index] = value & 0xFFFF; }
private:
    void findPos(uint32_t count, int& posOut, uint32_t& startOut)
    {
        int lo = 0, hi = 16;
        while (lo < hi) {
            const int mid = (lo + hi) >> 1;
            if (count < totals_[mid]) hi = mid;
            else lo = mid + 1;
        }
        const int bin = lo;
        int pos = 0;
        int end = 0;
        if (bin < 15) {
            pos = bin << binShift_;
            end = pos + (1 << binShift_);
            if (end > int(counts_.size())) end = int(counts_.size());
        } else {
            pos = lastBinStart_;
            end = int(counts_.size());
        }
        uint32_t start = bin > 0 ? totals_[bin - 1] : 0;
        while (pos < end) {
            const uint32_t entry = counts_[pos];
            if (count < start + entry) {
                posOut = pos;
                startOut = start;
                return;
            }
            start += entry;
            ++pos;
        }
        throw std::runtime_error("oodle0 model count outside total");
    }
    void quickIncrement(int value, int delta)
    {
        incrementTotals(value, delta);
        counts_[value] = uint16_t((counts_[value] + delta) & 0xFFFF);
    }
    void incrementTotals(int value, int delta)
    {
        if (value >= lastBinStart_) {
            totals_[15] = uint16_t((totals_[15] + delta) & 0xFFFF);
            return;
        }
        for (int i = value >> binShift_; i < 16; ++i) totals_[i] = uint16_t((totals_[i] + delta) & 0xFFFF);
    }
    void decrementCounts(int value, int amount)
    {
        counts_[value] = uint16_t((counts_[value] - amount) & 0xFFFF);
        if (value >= lastBinStart_) {
            totals_[15] = uint16_t((totals_[15] - amount) & 0xFFFF);
            return;
        }
        for (int i = value >> binShift_; i < 16; ++i) totals_[i] = uint16_t((totals_[i] - amount) & 0xFFFF);
    }
    void rescale()
    {
        BestShift(number_ + 1, binSize_, binShift_, lastBinStart_);
        std::array<int, 16> bins{};
        counts_[0] >>= 1;
        bins[0 < lastBinStart_ ? 0 : 15] += counts_[0];
        int maxCount = 0, maxPos = 0, index = 1;
        bool done = false;
        while (index <= number_ && !done) {
            while (counts_[index] <= 1) {
                if (index < number_) {
                    counts_[index] = counts_[number_];
                    values_[index] = values_[number_];
                    counts_[number_] = 0;
                    --number_;
                } else {
                    counts_[index] = 0;
                    --number_;
                    done = true;
                    break;
                }
            }
            if (done) break;
            counts_[index] >>= 1;
            if (counts_[index] > maxCount) {
                maxCount = counts_[index];
                maxPos = index;
            }
            const int bucket = index < lastBinStart_ ? (index >> binShift_) : 15;
            bins[bucket] += counts_[index];
            ++index;
        }
        if (maxCount) {
            int swapPos = number_ < lastBinStart_ ? ((number_ >> binShift_) << binShift_) : lastBinStart_;
            if (swapPos == 0) swapPos = 1;
            if (maxPos != swapPos) {
                const int oldCount = counts_[swapPos];
                counts_[swapPos] = counts_[maxPos];
                const int swapBucket = swapPos < lastBinStart_ ? (swapPos >> binShift_) : 15;
                const int maxBucket = maxPos < lastBinStart_ ? (maxPos >> binShift_) : 15;
                bins[swapBucket] += (-oldCount) + counts_[swapPos];
                bins[maxBucket] += oldCount - counts_[swapPos];
                counts_[maxPos] = uint16_t(oldCount);
                std::swap(values_[swapPos], values_[maxPos]);
            }
        }
        if (number_ != uniqueValues_ && counts_[0] == 0) {
            counts_[0] = uint16_t((counts_[0] + 2) & 0xFFFF);
            bins[0 < lastBinStart_ ? 0 : 15] += 2;
        }
        int running = 0;
        for (int i = 0; i < 16; ++i) {
            running += bins[i];
            totals_[i] = uint16_t(running & 0xFFFF);
        }
    }

    int uniqueValues_ = 0;
    int number_ = 0;
    int binSize_ = 0, binShift_ = 0, lastBinStart_ = 0;
    std::array<uint16_t, 16> totals_{};
    std::vector<uint16_t> counts_;
    std::vector<int> values_;
};

int ReadModelSymbol(ArithModel& model, ArithBits& bits, int escapeScale)
{
    int value = 0, escapeIndex = 0;
    if (model.decompress(bits, value, escapeIndex)) {
        const int escaped = int(bits.getValue(uint32_t(escapeScale)));
        model.setEscaped(escapeIndex, escaped);
        return escaped;
    }
    return value;
}

struct LzState {
    explicit LzState(const LzHeader& header)
        : maxBytes(int(header.maxByteValue()))
        , maxOffsets(int(header.maxOffset()))
        , maxOffsetLow((std::min)(maxOffsets, kLowOffsetMask + 1))
        , bytes(int(header.uniqueByteValues()))
        , offsetLow(maxOffsetLow)
        , offsetHigh(int(header.uniqueOffsets()))
    {
        lengths.reserve(kMaxLens + 1);
        for (int i = 0; i <= kMaxLens; ++i) lengths.emplace_back(int(header.lengthUnique(i)));
    }
    int maxBytes = 0, maxOffsets = 0, maxOffsetLow = 0;
    ArithModel bytes;
    std::vector<ArithModel> lengths;
    ArithModel offsetLow;
    ArithModel offsetHigh;
    int bytesDecompressed = 0;
    int lastLength = 0;
};

int ClampStop(int value, int expandedSize)
{
    if (value <= 0) return 0;
    if (value >= expandedSize) return expandedSize;
    return value;
}

size_t DecodeBlock(LzState& state, ArithBits& bits, std::vector<uint8_t>& output, size_t cursor, size_t stop)
{
    while (cursor < stop) {
        const int previousLength = state.lastLength;
        const int lengthSymbol = ReadModelSymbol(state.lengths[previousLength], bits, kMaxLens + 1);
        state.lastLength = lengthSymbol;
        if (lengthSymbol) {
            const int length = lengthSymbol >= kMaxLens - 3
                ? kLongLengths[lengthSymbol - (kMaxLens - 3)]
                : lengthSymbol + 1;
            const int low = ReadModelSymbol(state.offsetLow, bits, state.maxOffsetLow);
            const int highScale =
                (std::min(state.maxOffsets, state.bytesDecompressed) >> kOffsetSplitShift) + 1;
            const int high = ReadModelSymbol(state.offsetHigh, bits, highScale);
            const int distance = low + 1 + (high << kOffsetSplitShift);
            if (distance <= 0 || size_t(distance) > cursor)
                throw std::runtime_error("oodle0 invalid copy distance");
            const size_t src = cursor - size_t(distance);
            for (int k = 0; k < length; ++k) output[cursor + size_t(k)] = output[src + size_t(k)];
            cursor += size_t(length);
            state.bytesDecompressed += length;
        } else {
            const int literal = ReadModelSymbol(state.bytes, bits, state.maxBytes);
            if (literal < 0 || literal > 255) throw std::runtime_error("oodle0 invalid literal");
            output[cursor++] = uint8_t(literal);
            state.bytesDecompressed += 1;
        }
    }
    return cursor;
}

} // namespace

bool DecompressOodle0(const Section& section, const std::vector<uint8_t>& compressed,
    std::vector<uint8_t>& out, std::string& error)
{
    out.clear();
    error.clear();
    try {
        if (section.expandedSize == 0) return true;
        if (compressed.size() < size_t(kHeaderSize)) {
            error = "oodle0 data too short for 3 block headers";
            return false;
        }
        const size_t expanded = section.expandedSize;
        if (expanded > kMaxExpandedSize || expanded > compressed.size() * kMaxExpandRatio) {
            error = "oodle0 expanded_size exceeds cap";
            return false;
        }

        LzHeader headers[kBlockCount];
        for (int i = 0; i < kBlockCount; ++i) {
            headers[i].maxOffsetAndByte = U32lePadded(compressed, size_t(i) * 12);
            headers[i].uniqOffsetAndByte = U32lePadded(compressed, size_t(i) * 12 + 4);
            headers[i].uniqLens = U32lePadded(compressed, size_t(i) * 12 + 8);
        }
        const int expandedI = int(expanded);
        int first16 = ClampStop(int(section.first16bit), expandedI);
        int first8 = ClampStop(int(section.first8bit), expandedI);
        if (first8 < first16) first8 = first16;
        const int stops[4] = {0, first16, first8, expandedI};

        out.assign(expanded, 0);
        ArithBits bits(compressed, kHeaderSize);
        size_t cursor = 0;
        for (int i = 0; i < kBlockCount; ++i) {
            const int start = stops[i];
            const int end = stops[i + 1];
            if (end <= start) continue;
            LzState state(headers[i]);
            cursor = DecodeBlock(state, bits, out, cursor, size_t(end));
        }
        if (cursor != expanded) {
            error = "oodle0 decompressed length mismatch";
            out.clear();
            return false;
        }
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        out.clear();
        return false;
    }
}

} // namespace ro::gr2
