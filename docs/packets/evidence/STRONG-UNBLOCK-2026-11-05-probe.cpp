#include <cstdio>
#include <cstddef>
#include "server_packets.hpp"
int main() {
  std::printf("PACKETVER=%d SAK=%d MAIN=%d\n",PACKETVER,PACKETVER_SAK_NUM,PACKETVER_MAIN_NUM);
  std::printf("NPCSPRITE opcode=%04x size=%zu GID=%zu type=%zu class=%zu\n",HEADER_ZC_NPCSPRITE_CHANGE,sizeof(PACKET_ZC_NPCSPRITE_CHANGE),offsetof(PACKET_ZC_NPCSPRITE_CHANGE,GID),offsetof(PACKET_ZC_NPCSPRITE_CHANGE,type),offsetof(PACKET_ZC_NPCSPRITE_CHANGE,class_));
  std::printf("SHOWEFST opcode=%04x size=%zu gid=%zu option=%zu level=%zu opt3=%zu\n",HEADER_ZC_NPC_SHOWEFST_UPDATE,sizeof(PACKET_ZC_NPC_SHOWEFST_UPDATE),offsetof(PACKET_ZC_NPC_SHOWEFST_UPDATE,gid),offsetof(PACKET_ZC_NPC_SHOWEFST_UPDATE,effectState),offsetof(PACKET_ZC_NPC_SHOWEFST_UPDATE,level),offsetof(PACKET_ZC_NPC_SHOWEFST_UPDATE,showEFST));
  std::printf("selected_status opcode=%04x size=%zu alternate_status opcode=%04x size=%zu Left=%zu val1=%zu val2=%zu val3=%zu\n",status_changeType,sizeof(packet_status_change),status_change2Type,sizeof(packet_status_change2),offsetof(packet_status_change2,Left),offsetof(packet_status_change2,val1),offsetof(packet_status_change2,val2),offsetof(packet_status_change2,val3));
  std::printf("MAIL_MAX_INBOX=%d MAIL_TITLE_LENGTH=%d MAIL_BODY_LENGTH=%d MAIL_MAX_ITEM=%d\n",MAIL_MAX_INBOX,MAIL_TITLE_LENGTH,MAIL_BODY_LENGTH,MAIL_MAX_ITEM);
#if PACKETVER >= 20090121
  std::printf("clif_status_change_sub timed-043f emitter gate=enabled\n");
#else
  std::printf("clif_status_change_sub timed-043f emitter gate=disabled\n");
#endif
}
