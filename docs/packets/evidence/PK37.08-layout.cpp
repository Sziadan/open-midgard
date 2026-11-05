#include <type_traits>
#include <cstdio>
#include "server_packets.hpp"
static_assert(PACKETVER == 20080910);
static_assert(MAX_AMOUNT == 30000);
static_assert(sizeof(PACKET_ZC_PC_PURCHASE_ITEMLIST_sub) == 11);
static_assert(sizeof(PACKET_ZC_PC_SELL_ITEMLIST_sub) == 10);
static_assert(std::is_unsigned_v<decltype(PACKET_ZC_PC_PURCHASE_ITEMLIST_sub::price)>);
static_assert(std::is_unsigned_v<decltype(PACKET_ZC_PC_PURCHASE_ITEMLIST_sub::discountPrice)>);
static_assert(std::is_unsigned_v<decltype(PACKET_ZC_PC_SELL_ITEMLIST_sub::price)>);
static_assert(std::is_unsigned_v<decltype(PACKET_ZC_PC_SELL_ITEMLIST_sub::overcharge)>);
static_assert(sizeof(PACKET_ZC_PC_PURCHASE_ITEMLIST_sub::price) == 4);
static_assert(sizeof(PACKET_ZC_PC_PURCHASE_ITEMLIST_sub::discountPrice) == 4);
static_assert(sizeof(PACKET_ZC_PC_SELL_ITEMLIST_sub::price) == 4);
static_assert(sizeof(PACKET_ZC_PC_SELL_ITEMLIST_sub::overcharge) == 4);
static_assert(6553ull * 0xffffffffull * MAX_AMOUNT < 0x7fffffffffffffffull);
int main() { std::puts("PASS selected20080910 C6/C7: four unsigned 32-bit price fields; MAX_AMOUNT=30000; packet-bounded exact aggregate fits signed/unsigned 64 bits"); }
