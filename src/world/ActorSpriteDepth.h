#pragma once

#include <algorithm>
#include <cmath>

// Shared by actor billboards and shells composited directly over their owner.
constexpr float kActorBillboardDepthBias = 0.0005f;

inline float SampleActorShellDepth(float y, float topY, float bottomY,
    float topZ, float bottomZ)
{
    const float t = bottomY != topY ? (y - topY) / (bottomY - topY) : 0.0f;
    const float ownerZ = (std::max)(0.0f,
        topZ + (bottomZ - topZ) * t - kActorBillboardDepthBias);
    // Match the owner's depth plane, including beyond its cropped bounds.
    // One representable step puts the shell in front without defeating scene depth.
    return std::nextafter(ownerZ, 0.0f);
}
