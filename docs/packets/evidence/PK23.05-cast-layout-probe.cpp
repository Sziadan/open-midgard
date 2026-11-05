#include <cstddef>
#include <cstdio>
#include <type_traits>
#include "server_packets.hpp"
using CastAck = PACKET_ZC_USESKILL_ACK;
static_assert(PACKETVER == 20080910);
static_assert(HEADER_ZC_USESKILL_ACK == 0x013e);
static_assert(sizeof(CastAck) == 24);
static_assert(offsetof(CastAck, srcId) == 2);
static_assert(offsetof(CastAck, dstId) == 6);
static_assert(offsetof(CastAck, x) == 10);
static_assert(offsetof(CastAck, y) == 12);
static_assert(offsetof(CastAck, skillId) == 14);
static_assert(offsetof(CastAck, element) == 16);
static_assert(offsetof(CastAck, delayTime) == 20);
static_assert(sizeof(decltype(CastAck::element)) == 4);
static_assert(sizeof(decltype(CastAck::delayTime)) == 4);
static_assert(std::is_unsigned_v<decltype(CastAck::delayTime)>);
int main() {
    std::printf("PACKETVER=%d opcode=%04x size=%zu src=2 dst=6 x=10 y=12 skill=14 element=16 delay=20 delay_unsigned=1\n",
        PACKETVER, HEADER_ZC_USESKILL_ACK, sizeof(CastAck));
}
