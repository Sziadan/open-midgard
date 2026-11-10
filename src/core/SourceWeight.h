#pragma once
#include <cstdint>

namespace ro::stats {
// Selected ProcessObjClick57543C multiplies in32 bits, then CDQ/IDIV use
// the signed result and a denominator clamped to at least1.
inline int SourceWeightPercent(int weight,int maxWeight)
{
    const auto product=static_cast<std::int32_t>(static_cast<std::uint32_t>(weight)*100u);
    return product/(maxWeight>1?maxWeight:1);
}
}
