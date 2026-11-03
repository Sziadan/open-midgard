// Oodle0 section decompressor. MIT attribution: third_party/gr2-reader/LICENSE.
#pragma once
#include "Gr2File.h"
#include <string>
#include <vector>

namespace ro::gr2 {

bool DecompressOodle0(const Section& section, const std::vector<uint8_t>& compressed,
    std::vector<uint8_t>& out, std::string& error);

} // namespace ro::gr2
