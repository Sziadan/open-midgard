// Source fragment: selected rAthena src/map/packets.hpp PACKET_ZC_NOTIFY_VANISH.
// The declaration/header below are unconditional in the selected20080910 source.
// This probes layout only; it is not a full server build or deployment proof.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
using int16=std::int16_t;using uint32=std::uint32_t;using uint8=std::uint8_t;
struct PACKET_ZC_NOTIFY_VANISH {
    int16 packetType;
    uint32 gid;
    uint8 type;
} __attribute__((packed));
constexpr int HEADER_ZC_NOTIFY_VANISH=0x80;
static_assert(sizeof(PACKET_ZC_NOTIFY_VANISH)==7);
static_assert(offsetof(PACKET_ZC_NOTIFY_VANISH,gid)==2);
static_assert(offsetof(PACKET_ZC_NOTIFY_VANISH,type)==6);
int main() {
    const unsigned char literal[]={0x80,0,42,0,0,0,1};
    PACKET_ZC_NOTIFY_VANISH value{};std::memcpy(&value,literal,sizeof(value));
    if(value.packetType!=HEADER_ZC_NOTIFY_VANISH||value.gid!=42||value.type!=1)return 1;
    std::cout<<"profile=20080910 header=0x0080 size="<<sizeof(value)
        <<" gid="<<offsetof(PACKET_ZC_NOTIFY_VANISH,gid)<<" type="<<offsetof(PACKET_ZC_NOTIFY_VANISH,type)
        <<" literal_gid="<<value.gid<<" literal_type="<<unsigned(value.type)<<"\n";
}
