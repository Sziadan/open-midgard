// Read-only selected-server header probe; compile with the PK01 packet-audit wrapper.
#include "server_packets.hpp"
#include <cstddef>
#include <iostream>
static_assert(PACKETVER == 20080910);
#ifdef NEW_CARTS
#error Selected 20080910 must use legacy actor OPTION cart state
#endif
#define OFFSET(type, field, value) static_assert(offsetof(type, field) == value)
static_assert(HEADER_ZC_NOTIFY_CARTITEM_COUNTINFO == 0x0121);
static_assert(sizeof(PACKET_ZC_NOTIFY_CARTITEM_COUNTINFO) == 14);
OFFSET(PACKET_ZC_NOTIFY_CARTITEM_COUNTINFO, curCount, 2);
OFFSET(PACKET_ZC_NOTIFY_CARTITEM_COUNTINFO, maxCount, 4);
OFFSET(PACKET_ZC_NOTIFY_CARTITEM_COUNTINFO, curWeight, 6);
OFFSET(PACKET_ZC_NOTIFY_CARTITEM_COUNTINFO, maxWeight, 10);
static_assert(HEADER_ZC_ADD_ITEM_TO_CART == 0x01c5);
static_assert(sizeof(PACKET_ZC_ADD_ITEM_TO_CART) == 22);
OFFSET(PACKET_ZC_ADD_ITEM_TO_CART, index, 2);
OFFSET(PACKET_ZC_ADD_ITEM_TO_CART, amount, 4);
OFFSET(PACKET_ZC_ADD_ITEM_TO_CART, itemId, 8);
OFFSET(PACKET_ZC_ADD_ITEM_TO_CART, itemType, 10);
OFFSET(PACKET_ZC_ADD_ITEM_TO_CART, identified, 11);
OFFSET(PACKET_ZC_ADD_ITEM_TO_CART, damaged, 12);
OFFSET(PACKET_ZC_ADD_ITEM_TO_CART, refine, 13);
OFFSET(PACKET_ZC_ADD_ITEM_TO_CART, slot, 14);
static_assert(HEADER_ZC_DELETE_ITEM_FROM_CART == 0x0125);
static_assert(sizeof(PACKET_ZC_DELETE_ITEM_FROM_CART) == 8);
OFFSET(PACKET_ZC_DELETE_ITEM_FROM_CART, index, 2);
OFFSET(PACKET_ZC_DELETE_ITEM_FROM_CART, amount, 4);
static_assert(HEADER_ZC_ACK_ADDITEM_TO_CART == 0x012c);
static_assert(sizeof(PACKET_ZC_ACK_ADDITEM_TO_CART) == 3);
OFFSET(PACKET_ZC_ACK_ADDITEM_TO_CART, result, 2);
static_assert(HEADER_ZC_CARTOFF == 0x012b);
static_assert(sizeof(PACKET_ZC_CARTOFF) == 2);
int main() { std::cout << "PASS selected 20080910 cart sizes, IDs, fields and legacy OPTION model\n"; }
