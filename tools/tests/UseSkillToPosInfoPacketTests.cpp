#include "network/MapSendProfile.h"
#include "network/Packet.h"

#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {

void Require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

u16 ReadLE16(const u8* bytes)
{
    return static_cast<u16>(bytes[0]) | (static_cast<u16>(bytes[1]) << 8);
}

} // namespace

int main()
{
    try {
        Require(ro::net::GetActiveMapGameplaySendProfile().id == ro::net::MapGameplaySendProfileId::PacketVer23,
            "0x0190 is defined only by the packet_ver23 send profile");
        std::array<u8, sizeof(PACKET_CZ_USESKILLTOPOSINFO)> buffer{};
        int packetLength = 0;
        constexpr std::string_view contents = "GRAFFITI";
        Require(ro::net::BuildActiveUseSkillToPosInfoPacket(226, 5, 12, 34, contents,
            buffer.data(), static_cast<int>(buffer.size()), &packetLength),
            "0x0190 builder rejected a valid packet_ver23 payload");
        Require(packetLength == 90 && buffer[0] == 0x90 && buffer[1] == 0x01,
            "0x0190 opcode or fixed packet length differs from packet_db ver23");
        Require(ReadLE16(buffer.data() + 2) == 5 && ReadLE16(buffer.data() + 4) == 226
            && ReadLE16(buffer.data() + 6) == 12 && ReadLE16(buffer.data() + 8) == 34,
            "0x0190 skill, level, or target cell offsets differ from Ref");
        Require(std::memcmp(buffer.data() + 10, contents.data(), contents.size()) == 0
            && buffer[10 + contents.size()] == 0,
            "0x0190 text was not copied into the fixed 80-byte field with zero padding");

        const std::string_view tooLong("X", 81);
        Require(!ro::net::BuildActiveUseSkillToPosInfoPacket(226, 5, 12, 34, tooLong,
            buffer.data(), static_cast<int>(buffer.size()), &packetLength),
            "0x0190 builder accepted text larger than its 80-byte wire field");
        Require(!ro::net::BuildActiveUseSkillToPosInfoPacket(226, 5, 12, 34, contents,
            buffer.data(), static_cast<int>(buffer.size() - 1), &packetLength),
            "0x0190 builder accepted an undersized output buffer");
        std::cout << "UseSkillToPosInfo opcode=0x0190 size=90 skill=226 level=5 cell=(12,34) textBytes=8\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
