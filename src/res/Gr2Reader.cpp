#include "Gr2Reader.h"

namespace ro::gr2 {

// Implemented in gr2/Gr2Decode.cpp
bool DecodeNative(const std::vector<unsigned char>& bytes, Asset& asset, std::string& error,
    bool animationOnly);

bool Decode(const std::vector<unsigned char>& bytes, Asset& asset, std::string& error,
    bool animationOnly)
{
    return DecodeNative(bytes, asset, error, animationOnly);
}

} // namespace ro::gr2
