#pragma once

#include "Types.h"

// Sends an active map-profile action request through the production connection.
// This is the shared entry used by GameMode input paths and packet replay checks.
bool SendActionRequestPacket(u32 targetGid, u8 action);
