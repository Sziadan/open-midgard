// IGC (BinkTC) texture decode. MIT attribution: third_party/gr2-reader/LICENSE.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace ro::gr2 {

// Decode IGC bitstream to RGBA8888 (length = width*height*4).
// Small images (width*height <= 256) are treated as raw RGBA passthrough.
bool DecodeIgcTexture(const uint8_t* src, size_t srcSize, int width, int height, bool alpha,
    std::vector<uint8_t>& rgba, std::string& error);

} // namespace ro::gr2
