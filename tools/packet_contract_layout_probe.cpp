#include <cstdio>
#include <cstddef>
#include "server_packets.hpp"
int main() {
  std::printf("PACKETVER=%d\nPACKETVER_SAK_NUM=%d\nPACKETVER_MAIN_NUM=%d\n", PACKETVER, PACKETVER_SAK_NUM, PACKETVER_MAIN_NUM);
  std::printf("PACKET_ZC_UPDATE_CHARSTAT sizeof=%zu PacketType=%zu aid=%zu cid=%zu status=%zu gender=%zu hairStyle=%zu hairColor=%zu\n",
    sizeof(PACKET_ZC_UPDATE_CHARSTAT), offsetof(PACKET_ZC_UPDATE_CHARSTAT, packetType), offsetof(PACKET_ZC_UPDATE_CHARSTAT, aid), offsetof(PACKET_ZC_UPDATE_CHARSTAT, cid), offsetof(PACKET_ZC_UPDATE_CHARSTAT, status), offsetof(PACKET_ZC_UPDATE_CHARSTAT, gender), offsetof(PACKET_ZC_UPDATE_CHARSTAT, hairStyle), offsetof(PACKET_ZC_UPDATE_CHARSTAT, hairColor));
  std::printf("PACKET_ZC_PROPERTY_PET sizeof=%zu PacketType=%zu szName=%zu bModified=%zu nLevel=%zu nFullness=%zu nRelationship=%zu ITID=%zu\n",
    sizeof(PACKET_ZC_PROPERTY_PET), offsetof(PACKET_ZC_PROPERTY_PET, PacketType), offsetof(PACKET_ZC_PROPERTY_PET, szName), offsetof(PACKET_ZC_PROPERTY_PET, bModified), offsetof(PACKET_ZC_PROPERTY_PET, nLevel), offsetof(PACKET_ZC_PROPERTY_PET, nFullness), offsetof(PACKET_ZC_PROPERTY_PET, nRelationship), offsetof(PACKET_ZC_PROPERTY_PET, ITID));
}

