#include "GronPacket.h"

#include <array>
#include <mutex>

namespace ro::net {
namespace {

std::array<s16, 0x10000> g_packetSize{};
std::once_flag g_initOnce;

void SetPacketSize(u16 packetId, s16 size)
{
    g_packetSize[packetId] = size;
}

void FillPacketSizeTable()
{
    g_packetSize.fill(0);

    // Table aligned to Ref/RunningServer/packet_db.txt (packet_db_ver 23).
    SetPacketSize(0x0064, 55);              // CA_LOGIN  (client sends)
    SetPacketSize(0x0065, 17);              // CA_ENTER  (client sends)
    SetPacketSize(0x0066, 3);               // CZ_SELECT_CHAR (classic client)
    SetPacketSize(0x0067, 37);
    SetPacketSize(0x0069, kVariablePacketSize); // AC_ACCEPT_LOGIN (variable, has PacketLength)
    SetPacketSize(0x006A, 23);              // AC_REFUSE_LOGIN
    SetPacketSize(0x006B, kVariablePacketSize); // HC_ACCEPT_ENTER (variable char list)
    SetPacketSize(0x006C, 3);               // HC_REFUSE_ENTER
    SetPacketSize(0x006D, 110);             // HC_ACCEPT_MAKECHAR
    SetPacketSize(0x006E, 3);               // HC_REFUSE_MAKECHAR
    SetPacketSize(0x006F, 2);               // HC_ACCEPT_DELETECHAR
    SetPacketSize(0x0070, 3);               // HC_REFUSE_DELETECHAR
    SetPacketSize(0x0071, 28);
    SetPacketSize(0x0072, 25);
    SetPacketSize(0x0073, 11);
    SetPacketSize(0x0078, 55);
    SetPacketSize(0x0079, 53);
    SetPacketSize(0x007A, 58);
    SetPacketSize(0x007B, 60);
    SetPacketSize(0x007C, 42);
    SetPacketSize(0x01D8, 54);
    SetPacketSize(0x01D9, 53);
    SetPacketSize(0x01DA, 60);
    SetPacketSize(0x007E, 6);
    SetPacketSize(0x007F, 6);
    SetPacketSize(0x0080, 7);
    SetPacketSize(0x0085, 11);
    SetPacketSize(0x0081, 3);
    SetPacketSize(0x0086, 16);
    SetPacketSize(0x0087, 12);
    SetPacketSize(0x0088, 10);
    SetPacketSize(0x0089, 8);
    SetPacketSize(0x008A, 29);
    SetPacketSize(0x008D, kVariablePacketSize);
    SetPacketSize(0x008E, kVariablePacketSize);
    SetPacketSize(0x0091, 22);
    SetPacketSize(0x0092, 28);
    SetPacketSize(0x0097, kVariablePacketSize);
    SetPacketSize(0x0098, 3);
    SetPacketSize(0x0099, kVariablePacketSize);
    SetPacketSize(0x009A, kVariablePacketSize);
    SetPacketSize(0x009B, 26);
    SetPacketSize(0x009C, 9);
    SetPacketSize(0x009D, 17);
    SetPacketSize(0x009E, 17);
    SetPacketSize(0x0095, 30);
    SetPacketSize(0x00A0, 23);
    SetPacketSize(0x00A1, 6);
    SetPacketSize(0x00A3, kVariablePacketSize);
    SetPacketSize(0x00A4, kVariablePacketSize);
    SetPacketSize(0x00A7, 8);
    SetPacketSize(0x00A8, 7);
    SetPacketSize(0x00A9, 6);
    SetPacketSize(0x00AA, 7);
    SetPacketSize(0x00AC, 7);
    SetPacketSize(0x00AF, 6);
    SetPacketSize(0x00B0, 8);
    SetPacketSize(0x00B1, 8);
    SetPacketSize(0x00B2, 3);
    SetPacketSize(0x00B3, 3);
    SetPacketSize(0x00B4, kVariablePacketSize);
    SetPacketSize(0x00BA, 2);
    SetPacketSize(0x00BB, 5);
    SetPacketSize(0x00BC, 6);
    SetPacketSize(0x00BD, 44);
    SetPacketSize(0x00BE, 5);
    SetPacketSize(0x00BF, 3);
    SetPacketSize(0x00C0, 7);
    SetPacketSize(0x00C1, 2);
    SetPacketSize(0x00C2, 6);
    SetPacketSize(0x00C3, 8);
    SetPacketSize(0x00C4, 6);
    SetPacketSize(0x00D1, 4);
    SetPacketSize(0x00D2, 4);
    SetPacketSize(0x00D3, 2);
    SetPacketSize(0x00D4, kVariablePacketSize);
    SetPacketSize(0x00D6, 3);
    SetPacketSize(0x00D7, kVariablePacketSize);
    SetPacketSize(0x00D8, 6);
    SetPacketSize(0x00D9, 14);
    SetPacketSize(0x00DA, 3);
    SetPacketSize(0x00DC, 28);
    SetPacketSize(0x00DD, 29);
    SetPacketSize(0x00DE, kVariablePacketSize);
    SetPacketSize(0x00DF, kVariablePacketSize);
    SetPacketSize(0x00E0, 30);
    SetPacketSize(0x00E1, 30);
    SetPacketSize(0x00E2, 26);
    SetPacketSize(0x00E3, 2);
    SetPacketSize(0x00E4, 6);
    SetPacketSize(0x00E5, 26);
    SetPacketSize(0x00E6, 3);
    SetPacketSize(0x00E7, 3);
    SetPacketSize(0x00E8, 8);
    SetPacketSize(0x00E9, 19);
    SetPacketSize(0x00EA, 5);
    SetPacketSize(0x00EB, 2);
    SetPacketSize(0x00EC, 3);
    SetPacketSize(0x00EE, 2);
    SetPacketSize(0x00EF, 2);
    SetPacketSize(0x00F2, 6);
    SetPacketSize(0x00F3, kVariablePacketSize);
    SetPacketSize(0x00F4, 21);
    SetPacketSize(0x00F5, 8);
    SetPacketSize(0x00F6, 8);
    SetPacketSize(0x00F7, 22);
    SetPacketSize(0x00F8, 2);
    SetPacketSize(0x00F9, 26);
    SetPacketSize(0x00FA, 3);
    SetPacketSize(0x00FB, kVariablePacketSize);
    SetPacketSize(0x00FC, 6);
    SetPacketSize(0x00FD, 27);
    SetPacketSize(0x00FE, 30);
    SetPacketSize(0x00FF, 10);
    SetPacketSize(0x0100, 2);
    SetPacketSize(0x0101, 6);
    SetPacketSize(0x0102, 6);
    SetPacketSize(0x0103, 30);
    SetPacketSize(0x0104, 79);
    SetPacketSize(0x0105, 31);
    SetPacketSize(0x0106, 10);
    SetPacketSize(0x0109, kVariablePacketSize);
    SetPacketSize(0x010F, kVariablePacketSize);
    SetPacketSize(0x0110, 10);
    SetPacketSize(0x0119, 13);
    SetPacketSize(0x011A, 15);
    SetPacketSize(0x011F, 16);
    SetPacketSize(0x0120, 6);
    SetPacketSize(0x012F, kVariablePacketSize);
    SetPacketSize(0x0130, 6);
    SetPacketSize(0x0131, 86);
    SetPacketSize(0x0132, 6);
    SetPacketSize(0x0139, 16);
    SetPacketSize(0x013A, 4);
    SetPacketSize(0x013B, 4);
    SetPacketSize(0x013C, 4);
    SetPacketSize(0x013D, 6);
    SetPacketSize(0x013E, 24);
    SetPacketSize(0x013F, 26);
    SetPacketSize(0x0140, 22);
    SetPacketSize(0x0141, 14);
    SetPacketSize(0x0148, 8);
    SetPacketSize(0x0163, kVariablePacketSize);
    SetPacketSize(0x0164, kVariablePacketSize);
    SetPacketSize(0x0165, 30);
    SetPacketSize(0x0166, kVariablePacketSize);
    SetPacketSize(0x0167, 3);
    SetPacketSize(0x0168, 14);
    SetPacketSize(0x0170, 14);
    SetPacketSize(0x0171, 30);
    SetPacketSize(0x0172, 10);
    SetPacketSize(0x0173, 3);
    SetPacketSize(0x0174, kVariablePacketSize);
    SetPacketSize(0x0175, 6);
    SetPacketSize(0x0193, 2);
    SetPacketSize(0x0194, 30);
    SetPacketSize(0x0195, 102);
    SetPacketSize(0x0192, 24);
    SetPacketSize(0x019B, 10);
    SetPacketSize(0x01A2, 37);
    SetPacketSize(0x01A3, 5);
    SetPacketSize(0x01A4, 11);
    SetPacketSize(0x01A5, 26);
    SetPacketSize(0x01A6, kVariablePacketSize);
    SetPacketSize(0x01A7, 4);
    SetPacketSize(0x01A8, 4);
    SetPacketSize(0x01A9, 6);
    SetPacketSize(0x01AA, 10);
    SetPacketSize(0x01AB, 12);
    SetPacketSize(0x01AC, 6);
    SetPacketSize(0x01AD, kVariablePacketSize);
    SetPacketSize(0x01AE, 4);
    SetPacketSize(0x01AF, 4);
    SetPacketSize(0x01B0, 11);
    SetPacketSize(0x01B1, 7);
    SetPacketSize(0x01B2, kVariablePacketSize);
    SetPacketSize(0x01B3, 67);
    SetPacketSize(0x01B4, 12);
    SetPacketSize(0x01B5, 18);
    SetPacketSize(0x01B6, 114);
    SetPacketSize(0x01B7, 6);
    SetPacketSize(0x01B8, 3);
    SetPacketSize(0x01B9, 6);
    SetPacketSize(0x01BA, 26);
    SetPacketSize(0x01C3, kVariablePacketSize);
    SetPacketSize(0x01C9, 97);
    SetPacketSize(0x01CF, 28);
    SetPacketSize(0x01D0, 8);
    SetPacketSize(0x01D7, 11);
    SetPacketSize(0x01DE, 33);
    SetPacketSize(0x01E1, 8);
    SetPacketSize(0x01EE, kVariablePacketSize);
    SetPacketSize(0x01EF, kVariablePacketSize);
    SetPacketSize(0x01F3, 10);
    SetPacketSize(0x0201, kVariablePacketSize);
    SetPacketSize(0x0209, 36);
    SetPacketSize(0x0220, 10);
    SetPacketSize(0x0229, 15);
    SetPacketSize(0x022A, 58);
    SetPacketSize(0x022B, 57);
    SetPacketSize(0x022C, 65);
    SetPacketSize(0x0283, 6);
    SetPacketSize(0x02EB, 13);
    SetPacketSize(0x02EC, 67);
    SetPacketSize(0x8482, 4);
    SetPacketSize(0x8483, 4);
    SetPacketSize(0x02ED, 59);
    SetPacketSize(0x02EE, 60);
    SetPacketSize(0x02EF, 8);
    SetPacketSize(0x02DD, 32);
    SetPacketSize(0x02D0, kVariablePacketSize);
    SetPacketSize(0x02D1, kVariablePacketSize);
    SetPacketSize(0x02D2, kVariablePacketSize);
    SetPacketSize(0x07F7, kVariablePacketSize);
    SetPacketSize(0x07F8, kVariablePacketSize);
    SetPacketSize(0x07F9, kVariablePacketSize);
    SetPacketSize(0x0856, kVariablePacketSize);
    SetPacketSize(0x0857, kVariablePacketSize);
    SetPacketSize(0x0858, kVariablePacketSize);
    SetPacketSize(0x0814, 86);
    SetPacketSize(0x0816, 6);
    SetPacketSize(0x02C9, 3);
    SetPacketSize(0x02D7, kVariablePacketSize);
    SetPacketSize(0x02DA, 3);
    SetPacketSize(0x02B9, 191);
    SetPacketSize(0x06B3, 14);
    SetPacketSize(0x06B4, 14);
    SetPacketSize(0x0477, 8);
    SetPacketSize(0x0569, 8);
    SetPacketSize(0x02E1, 33);
    SetPacketSize(0x02E8, kVariablePacketSize);
    SetPacketSize(0x0459, 8);
    SetPacketSize(0x045A, 10);
    SetPacketSize(0x045B, kVariablePacketSize);
    SetPacketSize(0x045C, kVariablePacketSize);
    SetPacketSize(0x0463, kVariablePacketSize);
    SetPacketSize(0x045A, 10);
    SetPacketSize(0x045B, kVariablePacketSize);
    SetPacketSize(0x045C, kVariablePacketSize);
    SetPacketSize(0x0461, kVariablePacketSize);
    SetPacketSize(0x0465, 10);
    SetPacketSize(0x0466, 10);
    SetPacketSize(0x0467, 12);
    SetPacketSize(0x0468, 12);
    SetPacketSize(0x0469, 10);
    SetPacketSize(0x046F, 12);
    SetPacketSize(0x0470, 12);
    SetPacketSize(0x0471, 10);
    SetPacketSize(0x0477, 8);
    SetPacketSize(0x05EA, 10);
    SetPacketSize(0x06B3, 14);
    SetPacketSize(0x06B4, 14);
    SetPacketSize(0x0517, 4);
    SetPacketSize(0x0518, 4);
    SetPacketSize(0x0519, 6);
    SetPacketSize(0x06C8, 6);
    SetPacketSize(0x06C9, 7);
    SetPacketSize(0x06CA, 8);
    SetPacketSize(0x06CE, 10);
    SetPacketSize(0x07FA, 8);
    SetPacketSize(0x02DC, kVariablePacketSize);
}

} // namespace

void InitializePacketSize()
{
    std::call_once(g_initOnce, FillPacketSizeTable);
}

s16 GetPacketSize(u16 packetId)
{
    InitializePacketSize();
    return g_packetSize[packetId];
}

bool IsVariableLengthPacket(u16 packetId)
{
    return GetPacketSize(packetId) == kVariablePacketSize;
}

} // namespace ro::net
