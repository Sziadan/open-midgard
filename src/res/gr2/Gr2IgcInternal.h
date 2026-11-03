// Shared IGC kernel declarations. MIT attribution: third_party/gr2-reader/LICENSE.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace ro::gr2 {
namespace igc {

struct ArithBits {
    const uint8_t* buf = nullptr;
    int ptr = 0;
    uint32_t accum = 0;
    int bitsLeft = 0;
    uint32_t high = 0;
    uint32_t low = 0;
    uint32_t target = 0;
};

struct ArithModel {
    uint16_t cumCounts[16]{};
    std::vector<uint16_t> singleCounts;
    std::vector<uint16_t> values;
    int singlesLength = 0;
    int bandBoundary = 0;
    int shiftDepth = 0;
    int bucketSize = 0;
    int uniqueCount = 0;
};

ArithBits ArithBitOpen(const uint8_t* buf, int offset);
ArithModel ArithOpen(int maxValue, int uniqueValues);
// Returns true if escape; valueOrSlot holds symbol or escape slot.
bool ArithDecompress(ArithModel& model, ArithBits& bits, int& valueOrSlot);
void ArithSetDecompressed(ArithModel& model, int slot, int value);
uint32_t ArithBitsGetValue(ArithBits& bits, uint32_t scale);
uint32_t ArithBitsGet(ArithBits& bits, uint32_t scale);
void ArithBitsRemove(ArithBits& bits, uint32_t lo, uint32_t count, uint32_t scale);

int PlaneDecode(const uint8_t* src, int srcOffset, int16_t* output, int outOffset,
    int width, int height, uint8_t* rowMaskOrNull);

void IDWT2D(int16_t* output, int pitch, int width, int height, const uint8_t* rowMaskOrNull,
    int16_t* temp);

void YuvToRgb(const int16_t* yp, const int16_t* up, const int16_t* vp, const int16_t* ap,
    int width, int height, uint8_t* rgba);

bool DecodeIgcPipeline(const uint8_t* src, size_t srcSize, int width, int height, bool alpha,
    std::vector<uint8_t>& rgba, std::string& error);

} // namespace igc
} // namespace ro::gr2
