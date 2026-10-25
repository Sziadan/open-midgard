#pragma once

#include <cmath>

namespace ro::audio {

// Ref/Sound.cpp:195 (0x422E40). The two integer truncations are observable
// at the edge of the audible range; do not combine the volume operations.
inline int DigitalWaveVolume(float x, float y, float z, int maxDist, int minDist,
    int digitalVolume, float factor)
{
    const float horizontalSquared = x * x + z * z;
    const double horizontal = std::sqrt(static_cast<double>(horizontalSquared));
    const float distanceSquared = static_cast<float>(y * y + horizontal * horizontal);
    const float distance = static_cast<float>(std::sqrt(static_cast<double>(distanceSquared)));
    if (static_cast<double>(maxDist) <= distance) {
        return 0;
    }
    const int volume = distance > static_cast<double>(minDist)
        ? static_cast<int>(static_cast<double>(digitalVolume) * minDist / distance)
        : digitalVolume;
    return static_cast<int>(static_cast<double>(volume) * factor);
}

} // namespace ro::audio
