#include <cstddef>
#include <cstdio>
#include <type_traits>
#include "server_packets.hpp"
static_assert(PACKETVER==20080910);
static_assert(HEADER_CZ_ACK_SELECT_DEALTYPE==0xc5);
static_assert(sizeof(PACKET_CZ_ACK_SELECT_DEALTYPE)==7);
static_assert(offsetof(PACKET_CZ_ACK_SELECT_DEALTYPE,GID)==2);
static_assert(offsetof(PACKET_CZ_ACK_SELECT_DEALTYPE,type)==6);
static_assert(sizeof(PACKET_CZ_PC_PURCHASE_ITEMLIST)==4);
static_assert(std::is_signed_v<decltype(PACKET_CZ_PC_PURCHASE_ITEMLIST::packetLength)>);
static_assert(sizeof(PACKET_CZ_PC_PURCHASE_ITEMLIST_sub)==4);
static_assert(offsetof(PACKET_CZ_PC_PURCHASE_ITEMLIST_sub,amount)==0);
static_assert(offsetof(PACKET_CZ_PC_PURCHASE_ITEMLIST_sub,itemId)==2);
static_assert(HEADER_CZ_PC_SELL_ITEMLIST==0xc9);
static_assert(sizeof(PACKET_CZ_PC_SELL_ITEMLIST)==4);
static_assert(std::is_signed_v<decltype(PACKET_CZ_PC_SELL_ITEMLIST::packetLength)>);
static_assert(sizeof(PACKET_CZ_PC_SELL_ITEMLIST_sub)==4);
static_assert(offsetof(PACKET_CZ_PC_SELL_ITEMLIST_sub,index)==0);
static_assert(offsetof(PACKET_CZ_PC_SELL_ITEMLIST_sub,amount)==2);
static_assert(HEADER_ZC_PC_PURCHASE_RESULT==0xca);
static_assert(sizeof(PACKET_ZC_PC_PURCHASE_RESULT)==3);
static_assert(offsetof(PACKET_ZC_PC_PURCHASE_RESULT,result)==2);
static_assert(IT_ARMOR==4&&IT_WEAPON==5&&IT_PETEGG==7&&IT_PETARMOR==8&&IT_DELAYCONSUME==11&&IT_SHADOWGEAR==12);
int main(){std::puts("PASS selected20080910: C5/7 NPC@2 type@6; C8/header4 row4 amount@0 itemId@2; C9/header4 row4 index@0 amount@2; C8/C9 lengths signed16; CA/3 result@2; selected stackability type values verified");}
