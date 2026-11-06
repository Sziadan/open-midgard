#pragma once
#include <cstdint>

namespace ro::corpse {
// Ref CGameActor::ProcessState and CSession::IsMerOrHomun. Player-style
// actors have a different virtual state-3 path and do not use this predicate.
constexpr bool IsEligible(int job, int objectType)
{
    return (job > 28 && job < 4000) || objectType == 9 || (job > 6000 && job < 6048);
}

// Original GetZFromOow0x413330. Preserve its double evaluation and final
// float store, including a slightly negative depth for the float value0.1f.
inline float DeviceDepth(float reciprocal)
{
    return static_cast<float>(1500.0 / (1500.0 - 10.0)
        * (1.0 / reciprocal - 10.0) * reciprocal);
}

inline float ElapsedFrames(std::uint32_t now, std::uint32_t start)
{
    return static_cast<float>(static_cast<double>(static_cast<std::uint32_t>(now - start)) * static_cast<double>(0.041666668f));
}
}
