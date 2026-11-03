#pragma once
#include <algorithm>
#include <cstdint>
#include <limits>

// Runtime status values are 32-bit, unlike the legacy character-list record.
struct CharacterVitals {
    std::uint32_t hp=0, maxHp=0, sp=0, maxSp=0;
    bool Update(std::uint32_t type, std::uint32_t value) {
        switch (type) {
        case 5: hp=value; return true;
        case 6: maxHp=value; return true;
        case 7: sp=value; return true;
        case 8: maxSp=value; return true;
        default: return false;
        }
    }
};

inline std::uint32_t AddVital(std::uint32_t current, std::uint32_t amount)
{
    return static_cast<std::uint32_t>((std::min)(std::uint64_t(current) + amount,
        std::uint64_t(std::numeric_limits<std::uint32_t>::max())));
}

inline int VitalBarFill(std::int64_t amount, std::int64_t maximum, int width)
{
    if (maximum <= 0 || width <= 0) return 0;
    return static_cast<int>((std::clamp<std::int64_t>(amount, 0, maximum) * width) / maximum);
}
