#include <cstddef>
#include <cstdio>
#include "server_packets.hpp"
static_assert(PACKETVER == 20080910);
static_assert(inventorylistnormalType == 0x02e8 && inventorylistequipType == 0x02d0);
static_assert(cartlistnormalType == 0x02e9 && cartlistequipType == 0x02d2);
static_assert(sizeof(NORMALITEM_INFO) == 22 && sizeof(EQUIPITEM_INFO) == 26);
static_assert(offsetof(NORMALITEM_INFO,index)==0 && offsetof(NORMALITEM_INFO,ITID)==2);
static_assert(offsetof(NORMALITEM_INFO,type)==4 && offsetof(NORMALITEM_INFO,IsIdentified)==5);
static_assert(offsetof(NORMALITEM_INFO,count)==6 && offsetof(NORMALITEM_INFO,WearState)==8);
static_assert(offsetof(NORMALITEM_INFO,slot)==10 && offsetof(NORMALITEM_INFO,HireExpireDate)==18);
static_assert(offsetof(EQUIPITEM_INFO,index)==0 && offsetof(EQUIPITEM_INFO,ITID)==2);
static_assert(offsetof(EQUIPITEM_INFO,type)==4 && offsetof(EQUIPITEM_INFO,IsIdentified)==5);
static_assert(offsetof(EQUIPITEM_INFO,location)==6 && offsetof(EQUIPITEM_INFO,WearState)==8);
static_assert(offsetof(EQUIPITEM_INFO,IsDamaged)==10 && offsetof(EQUIPITEM_INFO,RefiningLevel)==11);
static_assert(offsetof(EQUIPITEM_INFO,slot)==12 && offsetof(EQUIPITEM_INFO,HireExpireDate)==20);
static_assert(offsetof(EQUIPITEM_INFO,bindOnEquipType)==24);
int main(){std::printf("PACKETVER=%d routes=02e8/02d0/02e9/02d2 normal=22 equip=26 all-index-id-type-ident-location-card-expiry-wear-damage-refine-bind-offsets=PASS\n",PACKETVER);}
