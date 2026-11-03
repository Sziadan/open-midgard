#include "GameModePacket.h"
#include "network/GronPacket.h"

namespace {
u16 ReadStreamLE16(const u8* bytes) {
    return static_cast<u16>(bytes[0]) | (static_cast<u16>(bytes[1]) << 8);
}
}

bool TryReadPacket(const u8* stream, int availableBytes, PacketView& outPacket, int& consumedBytes)
{
    consumedBytes = 0;
    outPacket = {};
    if (!stream || availableBytes < 2) return false;

    const u16 packetId = ReadStreamLE16(stream);
    const s16 knownSize = ro::net::GetPacketSize(packetId);
    if (knownSize == 0) return false;

    int packetSize = knownSize;
    if (knownSize == ro::net::kVariablePacketSize) {
        if (availableBytes < 4) return false;
        packetSize = ReadStreamLE16(stream + 2);
        if (packetSize < 4 || packetSize > 0x7FFF) return false;
    }
    if (availableBytes < packetSize) return false;

    outPacket.packetId = packetId;
    outPacket.packetLength = static_cast<u16>(packetSize);
    outPacket.data = stream;
    consumedBytes = packetSize;
    return true;
}
