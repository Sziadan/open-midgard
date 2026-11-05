#include "server_packets.hpp"
#include <cstddef>
#include <iostream>
static_assert(PACKETVER == 20080910);
static_assert(HEADER_ZC_ACK_REQNAME_BYGID == 0x0194);
static_assert(sizeof(PACKET_ZC_ACK_REQNAME_BYGID) == 30);
static_assert(offsetof(PACKET_ZC_ACK_REQNAME_BYGID, CID) == 2);
static_assert(offsetof(PACKET_ZC_ACK_REQNAME_BYGID, name) == 6);
static_assert(sizeof(PACKET_ZC_ACK_REQNAME_BYGID::name) == 24);
int main() { std::cout << "PASS selected 20080910 character-name reply: 0194, 30 bytes, CID@2, name[24]@6\n"; }
