#pragma once
#include "GuildEmblemKey.h"
#include <array>
#include <cstdint>

namespace ro::guild {
constexpr int FlagEmblemSize=24;
constexpr int FlagTextureSize=32;
inline std::array<std::uint32_t,FlagTextureSize*FlagTextureSize> FlagEmblemCanvas(const std::uint32_t* source)
{
    // Ref GetEmblemTexture creates 24x24, AdjustTextureSize rounds to 32x32,
    // then Update uploads 24x24 at (0,0). RenderGuildMark keeps authored UVs.
    std::array<std::uint32_t,FlagTextureSize*FlagTextureSize> result{};
    for(int y=0;y<FlagEmblemSize;++y)for(int x=0;x<FlagEmblemSize;++x) {
        const auto pixel=source[y*FlagEmblemSize+x];
        result[y*FlagTextureSize+x]=IsEmblemKey(pixel)?0:(pixel|0xFF000000u);
    }
    return result;
}
}
