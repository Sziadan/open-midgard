// GR2 container parse. MIT attribution: third_party/gr2-reader/LICENSE.
#include "Gr2File.h"
#include "Gr2Oodle0.h"

namespace ro::gr2 {
namespace {

constexpr size_t kMagicSize = 32;
constexpr size_t kSectionRecordSize = 44;
constexpr uint32_t kMagicOld[4] = {0xCAB067B8u, 0x0FB16DF8u, 0x7E8C7284u, 0x1E00195Eu};
constexpr uint32_t kMagic32Le[4] = {0xC06CDE29u, 0x2B53A4BAu, 0xA5B7F525u, 0xEEE266F6u};
constexpr uint32_t kMagic64Le[4] = {0x5E499BE5u, 0x141F636Fu, 0xA9EB131Eu, 0xC4EDBE90u};
constexpr uint32_t kMagic32Be[4] = {0xB595110Eu, 0x4BB5A56Au, 0x502828EBu, 0x04B37825u};
constexpr uint32_t kMagic64Be[4] = {0xE3D49531u, 0x624FDC20u, 0x3AD036CCu, 0x89FF82B1u};

uint32_t SwapU32(uint32_t value)
{
    return ((value & 0x000000FFu) << 24) | ((value & 0x0000FF00u) << 8) |
        ((value & 0x00FF0000u) >> 8) | ((value & 0xFF000000u) >> 24);
}

bool Eq4(const uint32_t* a, const uint32_t* b)
{
    return a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3];
}

bool ReadU32(const std::vector<uint8_t>& data, size_t offset, bool little, uint32_t& value)
{
    if (offset + 4 > data.size()) return false;
    if (little) {
        value = uint32_t(data[offset]) | (uint32_t(data[offset + 1]) << 8) |
            (uint32_t(data[offset + 2]) << 16) | (uint32_t(data[offset + 3]) << 24);
    } else {
        value = uint32_t(data[offset + 3]) | (uint32_t(data[offset + 2]) << 8) |
            (uint32_t(data[offset + 1]) << 16) | (uint32_t(data[offset]) << 24);
    }
    return true;
}

} // namespace

std::vector<uint8_t> File::sectionBytes(size_t index) const
{
    if (index >= sections.size()) return {};
    const Section& section = sections[index];
    const size_t start = section.dataOffset;
    const size_t end = start + static_cast<size_t>(section.dataSize);
    if (end < start || end > data.size()) return {};
    return std::vector<uint8_t>(data.begin() + static_cast<std::ptrdiff_t>(start),
        data.begin() + static_cast<std::ptrdiff_t>(end));
}

bool ParseFile(const std::vector<uint8_t>& bytes, File& out, std::string& error)
{
    out = {};
    error.clear();
    if (bytes.size() < kMagicSize) {
        error = "not a Granny2 file";
        return false;
    }

    uint32_t words[4]{};
    for (int i = 0; i < 4; ++i) {
        words[i] = uint32_t(bytes[i * 4]) | (uint32_t(bytes[i * 4 + 1]) << 8) |
            (uint32_t(bytes[i * 4 + 2]) << 16) | (uint32_t(bytes[i * 4 + 3]) << 24);
    }

    bool byteReversed = false;
    int pointerSize = 32;
    if (Eq4(words, kMagicOld) || Eq4(words, kMagic32Le)) {
        byteReversed = false;
        pointerSize = 32;
    } else if (Eq4(words, kMagic64Le)) {
        byteReversed = false;
        pointerSize = 64;
    } else {
        uint32_t swapped[4] = {SwapU32(words[0]), SwapU32(words[1]), SwapU32(words[2]), SwapU32(words[3])};
        if (Eq4(swapped, kMagicOld) || Eq4(swapped, kMagic32Be)) {
            byteReversed = true;
            pointerSize = 32;
        } else if (Eq4(swapped, kMagic64Be)) {
            byteReversed = true;
            pointerSize = 64;
        } else {
            error = "not a Granny2 file";
            return false;
        }
    }

    const bool little = !byteReversed;
    auto u32 = [&](size_t offset, uint32_t& value) { return ReadU32(bytes, offset, little, value); };

    File file;
    file.data = bytes;
    file.byteReversed = byteReversed;
    file.pointerSize = pointerSize;

    const size_t h = kMagicSize;
    uint32_t sectionCount = 0;
    if (!u32(h, file.version) || !u32(h + 4, file.totalSize) || !u32(h + 8, file.crc) ||
        !u32(h + 12, file.sectionArrayOffset) || !u32(h + 16, sectionCount) ||
        !u32(h + 20, file.rootType[0]) || !u32(h + 24, file.rootType[1]) ||
        !u32(h + 28, file.rootObject[0]) || !u32(h + 32, file.rootObject[1]) ||
        !u32(h + 36, file.typeTag)) {
        error = "gr2 header truncated";
        return false;
    }
    for (int i = 0; i < 4; ++i) {
        if (!u32(h + 40 + static_cast<size_t>(i) * 4, file.extraTags[i])) {
            error = "gr2 header truncated";
            return false;
        }
    }
    if (file.version >= 7) {
        if (!u32(h + 56, file.stringDbCrc) || !u32(h + 60, file.reserved[0]) ||
            !u32(h + 64, file.reserved[1]) || !u32(h + 68, file.reserved[2])) {
            error = "gr2 header truncated";
            return false;
        }
    }
    if (sectionCount > 64) {
        error = "gr2 section count exceeds limit";
        return false;
    }

    const size_t sectionsBase = kMagicSize + file.sectionArrayOffset;
    file.sections.resize(sectionCount);
    for (uint32_t index = 0; index < sectionCount; ++index) {
        const size_t off = sectionsBase + index * kSectionRecordSize;
        Section& section = file.sections[index];
        if (!u32(off, section.compression) || !u32(off + 4, section.dataOffset) ||
            !u32(off + 8, section.dataSize) || !u32(off + 12, section.expandedSize) ||
            !u32(off + 16, section.internalAlignment) || !u32(off + 20, section.first16bit) ||
            !u32(off + 24, section.first8bit) || !u32(off + 28, section.pointerFixupOffset) ||
            !u32(off + 32, section.pointerFixupCount) || !u32(off + 36, section.mixedMarshallingOffset) ||
            !u32(off + 40, section.mixedMarshallingCount)) {
            error = "gr2 section table truncated";
            return false;
        }
    }

    out = std::move(file);
    return true;
}

bool DecompressSection(const Section& section, const std::vector<uint8_t>& compressed,
    std::vector<uint8_t>& out, std::string& error)
{
    out.clear();
    error.clear();
    if (section.expandedSize == 0) return true;
    if (section.compression == kCompressionNone) {
        if (compressed.size() < section.expandedSize) {
            error = "section short for NoCompression";
            return false;
        }
        out.assign(compressed.begin(),
            compressed.begin() + static_cast<std::ptrdiff_t>(section.expandedSize));
        return true;
    }
    if (section.compression == kCompressionOodle0) {
        return DecompressOodle0(section, compressed, out, error);
    }
    error = "unsupported compression " + std::to_string(section.compression);
    return false;
}

} // namespace ro::gr2
