#include <cstdio>
#include <cstddef>
#include "server_packets.hpp"
static_assert(PACKETVER == 20080910);
static_assert(sizeof(PACKET_ZC_CASH_TIME_COUNTER)==8);
static_assert(offsetof(PACKET_ZC_CASH_TIME_COUNTER,itemId)==2);
static_assert(offsetof(PACKET_ZC_CASH_TIME_COUNTER,seconds)==4);
static_assert(sizeof(PACKET_ZC_CASH_ITEM_DELETE)==6);
static_assert(offsetof(PACKET_ZC_CASH_ITEM_DELETE,index)==2);
static_assert(offsetof(PACKET_ZC_CASH_ITEM_DELETE,itemId)==4);
static_assert(sizeof(NORMALITEM_INFO)==22);
static_assert(offsetof(NORMALITEM_INFO,HireExpireDate)==18);
static_assert(sizeof(EQUIPITEM_INFO)==26);
static_assert(offsetof(EQUIPITEM_INFO,HireExpireDate)==20);
int main(){std::puts("PASS PACKETVER20080910: 0298/8 itemId@2 seconds@4; 0299/6 index@2 itemId@4; normal22 expiry@18; equip26 expiry@20");}
