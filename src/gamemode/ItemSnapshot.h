#pragma once

#include "GameModePacket.h"

#include <array>
#include <cstddef>

namespace ro::item_snapshot {

constexpr unsigned kInventorySlots = 100;
constexpr unsigned kStorageSlots = 600;
constexpr unsigned kFirstInventoryIndex = 2;
constexpr unsigned kFirstStorageIndex = 1;
constexpr unsigned kMaxStackAmount = 30000;

inline bool IsEquipmentType(int type)
{
    return type == 4 || type == 5 || type == 7 || type == 8;
}

inline unsigned ReadWord(const u8* bytes)
{
    return bytes[0] | (static_cast<unsigned>(bytes[1]) << 8);
}

template <unsigned FirstIndex, unsigned Slots, typename StageRow>
bool Stage(const PacketView& packet, int stride, bool equipment, StageRow stageRow)
{
    if (!packet.data || packet.packetLength < 4 || ReadWord(packet.data + 2) != packet.packetLength) {
        return false;
    }
    const size_t payload = packet.packetLength - 4;
    if (stride <= 0 || payload % stride != 0 || payload / stride > Slots) {
        return false;
    }

    std::array<bool, FirstIndex + Slots> seen{};
    for (size_t offset = 4; offset < packet.packetLength; offset += stride) {
        const auto* row = packet.data + offset;
        const unsigned index = ReadWord(row);
        const unsigned quantity = equipment ? 1 : ReadWord(row + 6);
        if (index < FirstIndex || index >= seen.size() || seen[index]
            || ReadWord(row + 2) == 0 || quantity == 0 || quantity > kMaxStackAmount) {
            return false;
        }
        seen[index] = true;
        if (!stageRow(row)) return false;
    }
    return true;
}

}
