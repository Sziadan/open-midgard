#include <cstddef>
#include <cstdio>
#include <type_traits>

#include <map/packets.hpp>
#include <config/const.hpp>

static_assert(PACKETVER == 20080910);
static_assert(PACKETVER_SAK_NUM == 20080910);
static_assert(MAX_EXP == INT32_MAX);

#define CHECK_PACKET(Name, Opcode, Length) \
    static_assert(HEADER_##Name == Opcode && sizeof(PACKET_##Name) == Length)
#define CHECK_FIELD(Name, Field, Type, Offset) \
    static_assert(std::is_same_v<decltype(PACKET_##Name::Field), Type> \
        && offsetof(PACKET_##Name, Field) == Offset)

CHECK_PACKET(ZC_PAR_CHANGE, 0x00b0, 8);
CHECK_FIELD(ZC_PAR_CHANGE, varID, uint16, 2);
CHECK_FIELD(ZC_PAR_CHANGE, count, int32, 4);
CHECK_PACKET(ZC_LONGPAR_CHANGE, 0x00b1, 8);
CHECK_FIELD(ZC_LONGPAR_CHANGE, varID, uint16, 2);
CHECK_FIELD(ZC_LONGPAR_CHANGE, amount, int32, 4);
CHECK_PACKET(ZC_STATUS_CHANGE_ACK, 0x00bc, 6);
CHECK_FIELD(ZC_STATUS_CHANGE_ACK, sp, uint16, 2);
CHECK_FIELD(ZC_STATUS_CHANGE_ACK, ok, uint8, 4);
CHECK_FIELD(ZC_STATUS_CHANGE_ACK, value, uint8, 5);
CHECK_PACKET(ZC_STATUS_CHANGE, 0x00be, 5);
CHECK_FIELD(ZC_STATUS_CHANGE, statusID, uint16, 2);
CHECK_FIELD(ZC_STATUS_CHANGE, value, uint8, 4);
CHECK_PACKET(ZC_COUPLESTATUS, 0x0141, 14);
CHECK_FIELD(ZC_COUPLESTATUS, statusType, uint32, 2);
CHECK_FIELD(ZC_COUPLESTATUS, defaultStatus, int32, 6);
CHECK_FIELD(ZC_COUPLESTATUS, plusStatus, int32, 10);

CHECK_PACKET(ZC_STATUS, 0x00bd, 44);
CHECK_FIELD(ZC_STATUS, point, uint16, 2);
CHECK_FIELD(ZC_STATUS, str, uint8, 4);
CHECK_FIELD(ZC_STATUS, standardStr, uint8, 5);
CHECK_FIELD(ZC_STATUS, luk, uint8, 14);
CHECK_FIELD(ZC_STATUS, standardLuk, uint8, 15);
CHECK_FIELD(ZC_STATUS, attPower, int16, 16);
CHECK_FIELD(ZC_STATUS, refiningPower, int16, 18);
CHECK_FIELD(ZC_STATUS, max_mattPower, int16, 20);
CHECK_FIELD(ZC_STATUS, min_mattPower, int16, 22);
CHECK_FIELD(ZC_STATUS, itemdefPower, int16, 24);
CHECK_FIELD(ZC_STATUS, plusdefPower, int16, 26);
CHECK_FIELD(ZC_STATUS, mdefPower, int16, 28);
CHECK_FIELD(ZC_STATUS, plusmdefPower, int16, 30);
CHECK_FIELD(ZC_STATUS, hitSuccessValue, int16, 32);
CHECK_FIELD(ZC_STATUS, avoidSuccessValue, int16, 34);
CHECK_FIELD(ZC_STATUS, plusAvoidSuccessValue, int16, 36);
CHECK_FIELD(ZC_STATUS, criticalSuccessValue, int16, 38);
CHECK_FIELD(ZC_STATUS, ASPD, int16, 40);
CHECK_FIELD(ZC_STATUS, plusASPD, int16, 42);

CHECK_PACKET(ZC_STATE_CHANGE, 0x0229, 15);
CHECK_FIELD(ZC_STATE_CHANGE, AID, uint32, 2);
CHECK_FIELD(ZC_STATE_CHANGE, bodyState, int16, 6);
CHECK_FIELD(ZC_STATE_CHANGE, healthState, int16, 8);
CHECK_FIELD(ZC_STATE_CHANGE, effectState, int32, 10);
CHECK_FIELD(ZC_STATE_CHANGE, isPKModeON, int8, 14);
CHECK_PACKET(ZC_RECOVERY, 0x013d, 6);
CHECK_FIELD(ZC_RECOVERY, type, uint16, 2);
CHECK_FIELD(ZC_RECOVERY, amount, int16, 4);
CHECK_PACKET(ZC_NOTIFY_EFFECT3, 0x0284, 14);
CHECK_FIELD(ZC_NOTIFY_EFFECT3, aid, uint32, 2);
CHECK_FIELD(ZC_NOTIFY_EFFECT3, effectId, uint32, 6);
CHECK_FIELD(ZC_NOTIFY_EFFECT3, num, uint32, 10);

static_assert(spawn_unitType == 0x02ed && sizeof(packet_spawn_unit) == 59);
static_assert(idle_unitType == 0x02ee && sizeof(packet_idle_unit) == 60);
static_assert(spawn_unit2Type == 0x007c && sizeof(packet_spawn_unit2) == 42);
static_assert(idle_unit2Type == 0x0078 && sizeof(packet_idle_unit2) == 55);
static_assert(offsetof(packet_spawn_unit, GID) == 2);
static_assert(offsetof(packet_spawn_unit2, GID) == 3 && offsetof(packet_spawn_unit2, job) == 21);
static_assert(offsetof(packet_idle_unit2, GID) == 3 && offsetof(packet_idle_unit2, virtue) == 43);

int main()
{
    std::printf("PACKETVER=%d PACKETVER_SAK_NUM=%d MAX_EXP=%lld\n",
        PACKETVER, PACKETVER_SAK_NUM, static_cast<long long>(MAX_EXP));
    std::puts("PASS 00B0/8 00B1/8 00BC/6 00BD/44 00BE/5 0141/14 0229/15 013D/6 0284/14");
    std::puts("PASS selected actor entry 02ED/59 02EE/60 007C/42 0078/55");
}
