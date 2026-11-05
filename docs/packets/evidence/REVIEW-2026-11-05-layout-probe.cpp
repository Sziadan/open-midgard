#include <cstdio>
#include <cstddef>
#include "server_packets.hpp"
int main() {
  std::printf("PACKETVER=%d SAK=%d MAIN=%d\n", PACKETVER, PACKETVER_SAK_NUM, PACKETVER_MAIN_NUM);
  std::printf("inventory normal=%04x equipment=%04x cart normal=%04x equipment=%04x\n",inventorylistnormalType,inventorylistequipType,cartlistnormalType,cartlistequipType);
  std::printf("idle=%04x/%zu spawn=%04x/%zu walk=%04x/%zu fallback idle=%04x/%zu spawn=%04x/%zu\n",idle_unitType,sizeof(packet_idle_unit),spawn_unitType,sizeof(packet_spawn_unit),unit_walkingType,sizeof(packet_unit_walking),idle_unit2Type,sizeof(packet_idle_unit2),spawn_unit2Type,sizeof(packet_spawn_unit2));
  std::printf("cast=%04x/%zu src=%zu dst=%zu x=%zu skill=%zu element=%zu delay=%zu\n",HEADER_ZC_USESKILL_ACK,sizeof(PACKET_ZC_USESKILL_ACK),offsetof(PACKET_ZC_USESKILL_ACK,srcId),offsetof(PACKET_ZC_USESKILL_ACK,dstId),offsetof(PACKET_ZC_USESKILL_ACK,x),offsetof(PACKET_ZC_USESKILL_ACK,skillId),offsetof(PACKET_ZC_USESKILL_ACK,element),offsetof(PACKET_ZC_USESKILL_ACK,delayTime));
}
