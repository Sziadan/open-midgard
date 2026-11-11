#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

namespace ro::pet {

struct Status {
    bool valid = false;
    std::string name;
    // Selected session owners are DWORDs. Property01A2 sign-extends its
    // level/fullness/relationship words and zero-extends flag/accessory.
    std::int32_t modified = 0;
    std::int32_t level = 0;
    std::int32_t fullness = 0;
    std::int32_t relationship = 0;
    std::int32_t accessoryItemId = 0;
    std::int32_t oldFullness = 0;
    std::int32_t job = -1;
    std::uint32_t actorId = 0;
    std::int32_t eggIndex = -1;

    void ResetMapState()
    {
        const auto inventoryEgg = eggIndex;
        *this = {};
        eggIndex = inventoryEgg;
    }
};

inline std::uint16_t ReadU16(const unsigned char* p)
{
    return static_cast<std::uint16_t>(p[0] | (static_cast<std::uint16_t>(p[1]) << 8));
}

inline std::int8_t ReadI8(unsigned char value)
{
    const int decoded = (value & 0x80) ? static_cast<int>(value) - 0x100 : value;
    return static_cast<std::int8_t>(decoded);
}

inline std::int16_t ReadI16(const unsigned char* p)
{
    const std::uint16_t raw = ReadU16(p);
    const int decoded = (raw & 0x8000) ? static_cast<int>(raw) - 0x10000 : raw;
    return static_cast<std::int16_t>(decoded);
}

inline bool ReadStatus(Status& state, const unsigned char* packet, std::size_t size)
{
    if (!packet || size != 35 || packet[0] != 0xa2 || packet[1] != 0x01) return false;

    Status next = state;
    std::size_t nameLength = 0;
    while (nameLength < 24 && packet[2 + nameLength] != 0) ++nameLength;
    next.name.assign(reinterpret_cast<const char*>(packet + 2), nameLength);
    next.modified = packet[26];
    next.level = ReadI16(packet + 27);
    next.oldFullness = state.fullness;
    next.fullness = ReadI16(packet + 29);
    next.relationship = ReadI16(packet + 31);
    next.accessoryItemId = ReadU16(packet + 33);
    next.valid = true;
    state = std::move(next);
    return true;
}

} // namespace ro::pet
