#include <cstddef>
#include <cstdio>
#include "server_packets.hpp"

static_assert(PACKETVER == 20080910 && PACKETVER_SAK_NUM == 20080910);
static_assert(inventorylistnormalType == 0x02e8 && inventorylistequipType == 0x02d0);
static_assert(cartlistnormalType == 0x02e9 && cartlistequipType == 0x02d2);
static_assert(storageListNormalType == 0x02ea && storageListEquipType == 0x02d1);
static_assert(offsetof(packet_itemlist_normal, list) == 4);
static_assert(offsetof(packet_itemlist_equip, list) == 4);
static_assert(offsetof(ZC_STORE_ITEMLIST_NORMAL, list) == 4);
static_assert(offsetof(ZC_STORE_ITEMLIST_EQUIP, list) == 4);
static_assert(sizeof(NORMALITEM_INFO) == 22 && sizeof(EQUIPITEM_INFO) == 26);
static_assert(MAX_INVENTORY == 100 && MAX_CART == 100);
static_assert(MAX_STORAGE == 600 && MAX_GUILD_STORAGE == 600);

int main()
{
    std::puts("PASS PACKETVER 20080910 inventory/cart/storage opcodes, four-byte headers, row strides, and capacities");
}
