#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <limits>
using int16 = std::int16_t;
using uint16 = std::uint16_t;
using int32 = std::int32_t;
#define PACKETVER 20080910
#define DEFINE_PACKET_HEADER(name, value) constexpr int16 HEADER_##name = value
#if PACKETVER >= 20141022
struct PACKET_ZC_RECOVERY {
	int16 packetType;
	uint16 type;
	int32 amount;
} __attribute__((packed));
DEFINE_PACKET_HEADER(ZC_RECOVERY, 0xa27);
#else
struct PACKET_ZC_RECOVERY {
	int16 packetType;
	uint16 type;
	int16 amount;
} __attribute__((packed));
DEFINE_PACKET_HEADER(ZC_RECOVERY, 0x13d);
#endif
int main() {
  std::cout << "PACKETVER=" << PACKETVER << " sizeof=" << sizeof(PACKET_ZC_RECOVERY)
            << " offsets=" << offsetof(PACKET_ZC_RECOVERY, packetType) << ","
            << offsetof(PACKET_ZC_RECOVERY, type) << "," << offsetof(PACKET_ZC_RECOVERY, amount)
            << " signed_amount=" << std::numeric_limits<decltype(PACKET_ZC_RECOVERY::amount)>::is_signed << "\n";
  const std::uint32_t vals[] = {0,1,32767,32768,65535,65536,UINT32_MAX};
  for (auto val : vals) {
    PACKET_ZC_RECOVERY packet{};
    packet.packetType = HEADER_ZC_RECOVERY;
    packet.type = 5;
    packet.amount = std::min(static_cast<decltype(packet.amount)>(val), std::numeric_limits<decltype(packet.amount)>::max());
    unsigned char bytes[sizeof(packet)];
    std::memcpy(bytes, &packet, sizeof(packet));
    std::cout << val << " amount=" << packet.amount << " bytes=";
    for (std::size_t i=0;i<sizeof(packet);++i) std::printf("%02X", bytes[i]);
    std::cout << "\n";
  }
}