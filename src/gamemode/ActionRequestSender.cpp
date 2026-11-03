#include "ActionRequestSender.h"

#include "Connection.h"
#include "MapSendProfile.h"
#include "Packet.h"
#include "DebugLog.h"

#include <array>

namespace {
constexpr u8 kActionRequestSingleAttack = 0x00;
constexpr u8 kActionRequestContinuousAttack = 0x07;
}

bool SendActionRequestPacket(u32 targetGid, u8 action)
{
    if ((targetGid == 0) && (action == kActionRequestSingleAttack || action == kActionRequestContinuousAttack)) {
        return false;
    }

    const ro::net::MapGameplaySendProfile& profile = ro::net::GetActiveMapGameplaySendProfile();
    std::array<u8, sizeof(PACKET_CZ_ACTION_REQUEST_PACKETVER22)> packetBuffer{};
    int packetLength = 0;
    if (!ro::net::BuildActiveActionRequestPacket(
            targetGid,
            action,
            packetBuffer.data(),
            static_cast<int>(packetBuffer.size()),
            &packetLength)) {
        return false;
    }

    const bool sent = CRagConnection::instance()->SendPacket(
        reinterpret_cast<const char*>(packetBuffer.data()),
        packetLength);
    DbgLog("[GameMode] action request gid=%u action=%u opcode=0x%04X sent=%d\n",
        targetGid,
        static_cast<unsigned int>(action),
        profile.actionRequest,
        sent ? 1 : 0);
    return sent;
}
