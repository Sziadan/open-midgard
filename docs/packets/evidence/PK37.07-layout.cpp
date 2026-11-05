#include <cstddef>
#include <cstdio>
#include "server_packets.hpp"
static_assert(PACKETVER==20080910);
static_assert(HEADER_ZC_PC_PURCHASE_ITEMLIST==0x00c6);
static_assert(sizeof(PACKET_ZC_PC_PURCHASE_ITEMLIST)==4);
static_assert(sizeof(PACKET_ZC_PC_PURCHASE_ITEMLIST_sub)==11);
static_assert(offsetof(PACKET_ZC_PC_PURCHASE_ITEMLIST_sub,price)==0);
static_assert(offsetof(PACKET_ZC_PC_PURCHASE_ITEMLIST_sub,discountPrice)==4);
static_assert(offsetof(PACKET_ZC_PC_PURCHASE_ITEMLIST_sub,itemType)==8);
static_assert(offsetof(PACKET_ZC_PC_PURCHASE_ITEMLIST_sub,itemId)==9);
static_assert(HEADER_ZC_SELECT_DEALTYPE==0x00c4);
static_assert(sizeof(PACKET_ZC_SELECT_DEALTYPE)==6);
static_assert(offsetof(PACKET_ZC_SELECT_DEALTYPE,npcId)==2);
static_assert(HEADER_ZC_PC_SELL_ITEMLIST==0x00c7);
static_assert(sizeof(PACKET_ZC_PC_SELL_ITEMLIST)==4);
static_assert(sizeof(PACKET_ZC_PC_SELL_ITEMLIST_sub)==10);
static_assert(offsetof(PACKET_ZC_PC_SELL_ITEMLIST_sub,index)==0);
static_assert(offsetof(PACKET_ZC_PC_SELL_ITEMLIST_sub,price)==2);
static_assert(offsetof(PACKET_ZC_PC_SELL_ITEMLIST_sub,overcharge)==6);
int main(){std::puts("PASS selected20080910: 00c4/6 npcId@2; 00c6/header4 row11 price@0 discount@4 type@8 id@9; 00c7/header4 row10 index@0 price@2 overcharge@6");}
