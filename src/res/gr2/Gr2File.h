// GR2 container parse. MIT attribution: third_party/gr2-reader/LICENSE.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace ro::gr2 {

constexpr uint32_t kCompressionNone = 0;
constexpr uint32_t kCompressionOodle0 = 1;

struct Section {
    uint32_t compression = 0;
    uint32_t dataOffset = 0;
    uint32_t dataSize = 0;
    uint32_t expandedSize = 0;
    uint32_t internalAlignment = 0;
    uint32_t first16bit = 0;
    uint32_t first8bit = 0;
    uint32_t pointerFixupOffset = 0;
    uint32_t pointerFixupCount = 0;
    uint32_t mixedMarshallingOffset = 0;
    uint32_t mixedMarshallingCount = 0;
};

struct File {
    uint32_t version = 0;
    uint32_t totalSize = 0;
    uint32_t crc = 0;
    uint32_t sectionArrayOffset = 0;
    uint32_t typeTag = 0;
    uint32_t stringDbCrc = 0;
    uint32_t rootType[2]{};
    uint32_t rootObject[2]{};
    uint32_t extraTags[4]{};
    uint32_t reserved[3]{};
    bool byteReversed = false;
    int pointerSize = 32;
    std::vector<Section> sections;
    std::vector<uint8_t> data;

    std::vector<uint8_t> sectionBytes(size_t index) const;
};

bool ParseFile(const std::vector<uint8_t>& bytes, File& out, std::string& error);
bool DecompressSection(const Section& section, const std::vector<uint8_t>& compressed,
    std::vector<uint8_t>& out, std::string& error);

} // namespace ro::gr2
