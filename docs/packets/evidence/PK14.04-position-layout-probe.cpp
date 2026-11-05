#include <cstddef>
#include <cstdio>
#include <type_traits>
#include "server_packets.hpp"
static_assert(PACKETVER == 20080910);
static_assert(sizeof(PACKET_ZC_NOTIFY_POSITION_TO_GROUPM) == 10);
static_assert(sizeof(decltype(PACKET_ZC_NOTIFY_POSITION_TO_GROUPM::AID)) == 4);
static_assert(std::is_signed_v<decltype(PACKET_ZC_NOTIFY_POSITION_TO_GROUPM::AID)>);
static_assert(offsetof(PACKET_ZC_NOTIFY_POSITION_TO_GROUPM, AID) == 2);
static_assert(offsetof(PACKET_ZC_NOTIFY_POSITION_TO_GROUPM, xPos) == 6);
static_assert(offsetof(PACKET_ZC_NOTIFY_POSITION_TO_GROUPM, yPos) == 8);
static_assert(std::is_signed_v<decltype(PACKET_ZC_NOTIFY_POSITION_TO_GROUPM::xPos)>);
int main() {
    std::printf("PACKETVER=%d opcode=%04x size=%zu AID=%zu x=%zu y=%zu signed=1\n",
        PACKETVER, HEADER_ZC_NOTIFY_POSITION_TO_GROUPM, sizeof(PACKET_ZC_NOTIFY_POSITION_TO_GROUPM),
        offsetof(PACKET_ZC_NOTIFY_POSITION_TO_GROUPM, AID),
        offsetof(PACKET_ZC_NOTIFY_POSITION_TO_GROUPM, xPos),
        offsetof(PACKET_ZC_NOTIFY_POSITION_TO_GROUPM, yPos));
}
