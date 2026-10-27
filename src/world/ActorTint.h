#pragma once

#include <algorithm>
#include <cstdint>

namespace ro::actor {

template<typename TickProvider>
inline bool IsSingleColorExpired(bool active, std::uint32_t startTick,
    TickProvider tickProvider)
{
    return active && tickProvider() > startTick + 100u;
}

inline std::uint8_t AdvanceTintChannel(std::uint8_t channel, int amount)
{
    const int advanced = static_cast<int>(channel) + amount;
    return static_cast<std::uint8_t>((std::max)(0, (std::min)(255, advanced)));
}

inline void AdvanceTint(std::uint8_t& alpha, std::uint8_t& red,
    std::uint8_t& green, std::uint8_t& blue, bool fixed)
{
    if (fixed) {
        return;
    }
    alpha = AdvanceTintChannel(alpha, 8);
    red = AdvanceTintChannel(red, 8);
    green = AdvanceTintChannel(green, 8);
    blue = AdvanceTintChannel(blue, 8);
}

inline std::uint32_t ResolveBillboardColor(std::uint32_t visibilityAlpha,
    std::uint8_t tintAlpha, std::uint8_t red, std::uint8_t green,
    std::uint8_t blue, bool singleColor, std::uint32_t singleColorSentinel)
{
    const std::uint32_t alpha = visibilityAlpha * tintAlpha / 255u;
    const std::uint32_t rgb = singleColor && singleColorSentinel != 0xFFFF0000u
        ? 0x00FF0000u
        : (static_cast<std::uint32_t>(red) << 16)
            | (static_cast<std::uint32_t>(green) << 8)
            | static_cast<std::uint32_t>(blue);
    return (alpha << 24) | rgb;
}

} // namespace ro::actor
