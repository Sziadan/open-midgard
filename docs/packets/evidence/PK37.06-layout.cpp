#include <cstdio>
#include <cstddef>
#include "server_packets.hpp"
static_assert(PACKETVER == 20080910);
static_assert(sizeof(PACKET_ZC_PC_PURCHASE_MYITEMLIST)==8);
static_assert(sizeof(PACKET_ZC_PC_PURCHASE_MYITEMLIST_sub)==22);
static_assert(offsetof(PACKET_ZC_PC_PURCHASE_MYITEMLIST_sub,index)==4);
static_assert(offsetof(PACKET_ZC_PC_PURCHASE_MYITEMLIST_sub,amount)==6);
static_assert(offsetof(PACKET_ZC_PC_PURCHASE_MYITEMLIST_sub,itemId)==9);
static_assert(offsetof(PACKET_ZC_PC_PURCHASE_MYITEMLIST_sub,identified)==11);
static_assert(offsetof(PACKET_ZC_PC_PURCHASE_MYITEMLIST_sub,refine)==13);
static_assert(sizeof(PACKET_ZC_PC_PURCHASE_ITEMLIST_FROMMC)==8);
static_assert(sizeof(PACKET_ZC_PC_PURCHASE_ITEMLIST_FROMMC_sub)==22);
static_assert(offsetof(PACKET_ZC_PC_PURCHASE_ITEMLIST_FROMMC_sub,amount)==4);
static_assert(offsetof(PACKET_ZC_PC_PURCHASE_ITEMLIST_FROMMC_sub,index)==6);
static_assert(offsetof(PACKET_ZC_PC_PURCHASE_ITEMLIST_FROMMC_sub,itemId)==9);
static_assert(sizeof(PACKET_ZC_ADD_EXCHANGE_ITEM)==19);
static_assert(offsetof(PACKET_ZC_ADD_EXCHANGE_ITEM,amount)==2);
static_assert(offsetof(PACKET_ZC_ADD_EXCHANGE_ITEM,itemId)==6);
static_assert(offsetof(PACKET_ZC_ADD_EXCHANGE_ITEM,identified)==8);
static_assert(offsetof(PACKET_ZC_ADD_EXCHANGE_ITEM,refine)==10);
static_assert(offsetof(PACKET_ZC_ADD_EXCHANGE_ITEM,slot)==11);
static_assert(sizeof(PACKET_ZC_EQUIPWIN_MICROSCOPE)==43);
static_assert(offsetof(PACKET_ZC_EQUIPWIN_MICROSCOPE,list)==43);
static_assert(sizeof(EQUIPITEM_INFO)==26);
int main(){std::puts("PASS PACKETVER20080910: 0136 header8 row22 index@4 amount@6 id@9; 0133 header8 row22 amount@4 index@6 id@9; 00e9 size19 amount@2 id@6 identified@8 refine@10 slot@11, no inventory index; 02d7 header43 equiprow26");}
