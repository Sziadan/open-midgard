#include <cstddef>
#include <cstdio>
#include <type_traits>

#include <map/packets.hpp>

static_assert(PACKETVER == 20080910);
static_assert(PACKETVER_SAK_NUM == 20080910);

#define CHECK_PACKET(Name, Opcode, Length) \
    static_assert(HEADER_##Name == Opcode && sizeof(PACKET_##Name) == Length)
#define CHECK_FIELD(Name, Field, Type, Offset) \
    static_assert(std::is_same_v<decltype(PACKET_##Name::Field), Type> \
        && offsetof(PACKET_##Name, Field) == Offset)

CHECK_PACKET(ZC_NOTIFY_SKILL, 0x01de, 33);
CHECK_FIELD(ZC_NOTIFY_SKILL, SKID, uint16, 2);
CHECK_FIELD(ZC_NOTIFY_SKILL, AID, uint32, 4);
CHECK_FIELD(ZC_NOTIFY_SKILL, targetID, uint32, 8);
CHECK_FIELD(ZC_NOTIFY_SKILL, startTime, uint32, 12);
CHECK_FIELD(ZC_NOTIFY_SKILL, attackMT, int32, 16);
CHECK_FIELD(ZC_NOTIFY_SKILL, attackedMT, int32, 20);
CHECK_FIELD(ZC_NOTIFY_SKILL, damage, int32, 24);
CHECK_FIELD(ZC_NOTIFY_SKILL, level, int16, 28);
CHECK_FIELD(ZC_NOTIFY_SKILL, count, int16, 30);
CHECK_FIELD(ZC_NOTIFY_SKILL, action, int8, 32);

CHECK_PACKET(ZC_NOTIFY_SKILL_POSITION, 0x0115, 35);
CHECK_FIELD(ZC_NOTIFY_SKILL_POSITION, xPos, int16, 24);
CHECK_FIELD(ZC_NOTIFY_SKILL_POSITION, yPos, int16, 26);
CHECK_FIELD(ZC_NOTIFY_SKILL_POSITION, damage, int16, 28);
CHECK_PACKET(ZC_NOTIFY_GROUNDSKILL, 0x0117, 18);
CHECK_FIELD(ZC_NOTIFY_GROUNDSKILL, SKID, uint16, 2);
CHECK_FIELD(ZC_NOTIFY_GROUNDSKILL, AID, uint32, 4);
CHECK_FIELD(ZC_NOTIFY_GROUNDSKILL, level, int16, 8);
CHECK_FIELD(ZC_NOTIFY_GROUNDSKILL, xPos, int16, 10);
CHECK_FIELD(ZC_NOTIFY_GROUNDSKILL, yPos, int16, 12);
CHECK_FIELD(ZC_NOTIFY_GROUNDSKILL, startTime, uint32, 14);

CHECK_PACKET(ZC_USE_SKILL, 0x011a, 15);
CHECK_FIELD(ZC_USE_SKILL, SKID, uint16, 2);
CHECK_FIELD(ZC_USE_SKILL, level, int16, 4);
CHECK_FIELD(ZC_USE_SKILL, targetAID, uint32, 6);
CHECK_FIELD(ZC_USE_SKILL, srcAID, uint32, 10);
CHECK_FIELD(ZC_USE_SKILL, result, int8, 14);

CHECK_PACKET(ZC_USESKILL_ACK, 0x013e, 24);
CHECK_FIELD(ZC_USESKILL_ACK, srcId, uint32, 2);
CHECK_FIELD(ZC_USESKILL_ACK, dstId, uint32, 6);
CHECK_FIELD(ZC_USESKILL_ACK, x, uint16, 10);
CHECK_FIELD(ZC_USESKILL_ACK, y, uint16, 12);
CHECK_FIELD(ZC_USESKILL_ACK, skillId, uint16, 14);
CHECK_FIELD(ZC_USESKILL_ACK, element, uint32, 16);
CHECK_FIELD(ZC_USESKILL_ACK, delayTime, uint32, 20);

CHECK_PACKET(ZC_DISPEL, 0x01b9, 6);
CHECK_FIELD(ZC_DISPEL, gid, uint32, 2);
CHECK_PACKET(ZC_ACK_TOUSESKILL, 0x0110, 10);
CHECK_FIELD(ZC_ACK_TOUSESKILL, skillId, uint16, 2);
CHECK_FIELD(ZC_ACK_TOUSESKILL, btype, int16, 4);
CHECK_FIELD(ZC_ACK_TOUSESKILL, itemId, uint16, 6);
CHECK_FIELD(ZC_ACK_TOUSESKILL, flag, uint8, 8);
CHECK_FIELD(ZC_ACK_TOUSESKILL, cause, uint8, 9);

CHECK_PACKET(ZC_SKILL_UPDATE, 0x01ac, 6);
CHECK_FIELD(ZC_SKILL_UPDATE, GID, uint32, 2);
CHECK_PACKET(ZC_SKILL_DISAPPEAR, 0x0120, 6);
CHECK_FIELD(ZC_SKILL_DISAPPEAR, GID, uint32, 2);

static_assert(graffiti_entryType == 0x01c9 && sizeof(packet_graffiti_entry) == 97);
static_assert(offsetof(packet_graffiti_entry, AID) == 2);
static_assert(offsetof(packet_graffiti_entry, creatorAID) == 6);
static_assert(offsetof(packet_graffiti_entry, xPos) == 10);
static_assert(offsetof(packet_graffiti_entry, yPos) == 12);
static_assert(offsetof(packet_graffiti_entry, job) == 14);
static_assert(offsetof(packet_graffiti_entry, isVisible) == 15);
static_assert(offsetof(packet_graffiti_entry, isContens) == 16);
static_assert(offsetof(packet_graffiti_entry, msg) == 17);

CHECK_PACKET(ZC_AUTORUN_SKILL, 0x0147, 39);
CHECK_FIELD(ZC_AUTORUN_SKILL, skill_id, uint16, 2);
CHECK_FIELD(ZC_AUTORUN_SKILL, skill_type, uint32, 4);
CHECK_FIELD(ZC_AUTORUN_SKILL, skill_lv, uint16, 8);
CHECK_FIELD(ZC_AUTORUN_SKILL, skill_sp, uint16, 10);
CHECK_FIELD(ZC_AUTORUN_SKILL, skill_range, uint16, 12);
CHECK_FIELD(ZC_AUTORUN_SKILL, skill_name, char[24], 14);
CHECK_FIELD(ZC_AUTORUN_SKILL, up_flag, char, 38);
CHECK_PACKET(ZC_SKILLMSG, 0x0215, 6);
CHECK_FIELD(ZC_SKILLMSG, msgId, int32, 2);
CHECK_PACKET(ZC_TALKBOX_CHATCONTENTS, 0x0191, 86);
CHECK_FIELD(ZC_TALKBOX_CHATCONTENTS, aid, uint32, 2);
CHECK_FIELD(ZC_TALKBOX_CHATCONTENTS, message, char[80], 6);

int main()
{
    std::printf("PACKETVER=%d PACKETVER_SAK_NUM=%d\n", PACKETVER, PACKETVER_SAK_NUM);
    std::puts("PASS 01DE/33 0115/35 0117/18 011A/15 013E/24 01B9/6 0110/10 01AC/6 0120/6 01C9/97 0147/39 0215/6 0191/86");
}
