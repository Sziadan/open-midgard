#pragma once
//===========================================================================
// GronPacket.h  –  Ragnarok packet-size table and helpers
//===========================================================================
#include "Types.h"

namespace ro::net {

constexpr s16 kVariablePacketSize = -1;

// Initialize known packet sizes from the HighPriest 2008 client table.
void InitializePacketSize();

// Returns:
//  > 0  fixed packet byte length
// == -1 variable length (size is encoded in bytes 2..3)
// == 0  unknown / unregistered packet id
s16 GetPacketSize(u16 packetId);

bool IsVariableLengthPacket(u16 packetId);

} // namespace ro::net
