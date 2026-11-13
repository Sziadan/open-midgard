#include <cstddef>
#include <cstdio>
#include "server_packets.hpp"

static_assert(PACKETVER == 20080910);
static_assert(sizeof(PACKET_ZC_ACK_ITEMREFINING) == 8);
static_assert(offsetof(PACKET_ZC_ACK_ITEMREFINING, value) == 6);
static_assert(sizeof(PACKET_ZC_MAKABLEITEMLIST) == 4);
static_assert(sizeof(PACKET_ZC_MAKABLEITEMLIST_sub) == 8);
static_assert(sizeof(PACKET_CZ_REQMAKINGITEM) == 10);
static_assert(sizeof(PACKET_ZC_ACK_REQMAKINGITEM) == 6);
static_assert(sizeof(PACKET_ZC_NOTIFY_WEAPONITEMLIST) == 4);
static_assert(sizeof(PACKET_ZC_NOTIFY_WEAPONITEMLIST_sub) == 13);
static_assert(sizeof(PACKET_ZC_ACK_WEAPONREFINE) == 8);
static_assert(sizeof(PACKET_ZC_REPAIRITEMLIST) == 4);
static_assert(sizeof(REPAIRITEM_INFO1) == 13);
static_assert(sizeof(PACKET_CZ_REQ_ITEMREPAIR1) == 15);
static_assert(sizeof(PACKET_ZC_ACK_ITEMREPAIR) == 5);
static_assert(sizeof(PACKET_ZC_MAKINGITEM_LIST) == 6);
static_assert(sizeof(PACKET_ZC_MAKINGITEM_LIST_sub) == 2);
static_assert(sizeof(PACKET_CZ_REQ_MAKINGITEM) == 6);

int main()
{
    std::puts("PASS PACKETVER 20080910 refine, production, repair, and cooking sizes");
}
