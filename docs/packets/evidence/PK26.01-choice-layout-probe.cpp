#include <cstddef>
#include <cstdio>
#include <type_traits>

#include <map/packets.hpp>

static_assert(PACKETVER == 20080910 && PACKETVER_SAK_NUM == 20080910);

#define CHECK_PACKET(Name, Opcode, Length) \
    static_assert(HEADER_##Name == Opcode && sizeof(PACKET_##Name) == Length)
#define CHECK_FIELD(Name, Field, Type, Offset) \
    static_assert(std::is_same_v<decltype(PACKET_##Name::Field), Type> \
        && offsetof(PACKET_##Name, Field) == Offset)

CHECK_PACKET(ZC_ACK_REMEMBER_WARPPOINT, 0x011e, 3);
CHECK_FIELD(ZC_ACK_REMEMBER_WARPPOINT, type, uint8, 2);
CHECK_PACKET(ZC_MONSTER_INFO, 0x018c, 29);
CHECK_FIELD(ZC_MONSTER_INFO, class_, uint16, 2);
CHECK_FIELD(ZC_MONSTER_INFO, level, uint16, 4);
CHECK_FIELD(ZC_MONSTER_INFO, size, uint16, 6);
CHECK_FIELD(ZC_MONSTER_INFO, hp, uint32, 8);
CHECK_FIELD(ZC_MONSTER_INFO, def, int16, 12);
CHECK_FIELD(ZC_MONSTER_INFO, race, uint16, 14);
CHECK_FIELD(ZC_MONSTER_INFO, mdef, int16, 16);
CHECK_FIELD(ZC_MONSTER_INFO, element, uint16, 18);
CHECK_FIELD(ZC_MONSTER_INFO, water, uint8, 20);
CHECK_FIELD(ZC_MONSTER_INFO, undead, uint8, 28);

CHECK_PACKET(ZC_WARPLIST, 0x011c, 68);
CHECK_FIELD(ZC_WARPLIST, skillId, uint16, 2);
CHECK_FIELD(ZC_WARPLIST, maps, PACKET_ZC_WARPLIST_sub[4], 4);
CHECK_PACKET(CZ_REQ_MAKINGARROW, 0x01ae, 4);
CHECK_FIELD(CZ_REQ_MAKINGARROW, itemId, uint16, 2);
CHECK_PACKET(ZC_AUTOSPELLLIST, 0x01cd, 30);
CHECK_FIELD(ZC_AUTOSPELLLIST, skills, int[7], 2);
CHECK_PACKET(CZ_SELECTAUTOSPELL, 0x01ce, 6);
CHECK_FIELD(CZ_SELECTAUTOSPELL, skill_id, uint32, 2);

int main()
{
    std::printf("PACKETVER=%d PACKETVER_SAK_NUM=%d\n", PACKETVER, PACKETVER_SAK_NUM);
    std::puts("PASS 011E/3 018C/29 011C/68 01AE/4 01CD/30 01CE/6");
}
