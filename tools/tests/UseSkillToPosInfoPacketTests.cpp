#include "network/MapSendProfile.h"
#include "network/Packet.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

void Require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

u16 ReadLE16(const u8* bytes)
{
    return static_cast<u16>(bytes[0]) | (static_cast<u16>(bytes[1]) << 8);
}

std::vector<u8> ParseFixtureHex(std::string_view value)
{
    Require(value.size() % 2 == 0, "fixture hex has an odd number of digits");
    std::vector<u8> bytes;
    bytes.reserve(value.size() / 2);
    for (std::size_t i = 0; i < value.size(); i += 2) {
        const auto byte = static_cast<unsigned int>(std::stoul(std::string(value.substr(i, 2)), nullptr, 16));
        bytes.push_back(static_cast<u8>(byte));
    }
    return bytes;
}

} // namespace

int main()
{
    try {
        Require(ro::net::GetActiveMapGameplaySendProfile().id == ro::net::MapGameplaySendProfileId::PacketVer23,
            "independent 0x007E fixture requires the selected packet_ver23 profile");
        std::array<u8, sizeof(PACKET_CZ_USESKILLTOPOSINFO)> buffer{};
        int packetLength = 0;
        constexpr std::string_view contents = "fixture";
        Require(ro::net::BuildActiveUseSkillToPosInfoPacket(0x0123, 3, 100, 200, contents,
            buffer.data(), static_cast<int>(buffer.size()), &packetLength),
            "0x007E builder rejected a valid packet_ver23 payload");
        constexpr std::string_view expectedHex =
            "7e00000000030000002301006400000000000000c8006669787475726500000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000";
        const std::vector<u8> expected = ParseFixtureHex(expectedHex);
        Require(expected.size() == 102 && packetLength == static_cast<int>(expected.size()),
            "selected 0x007E request length differs from independent server fixture");
        Require(std::equal(buffer.begin(), buffer.end(), expected.begin()),
            "production 0x007E serializer differs from independent PK02.03 server-layout fixture");
        Require(buffer[0] == 0x7e && buffer[1] == 0 && ReadLE16(buffer.data() + 5) == 3
            && ReadLE16(buffer.data() + 9) == 0x0123 && ReadLE16(buffer.data() + 12) == 100
            && ReadLE16(buffer.data() + 20) == 200 && std::memcmp(buffer.data() + 22, "fixture", 7) == 0,
            "0x007E field offsets differ from the selected server parser registry");

        const std::string_view tooLong("X", 80);
        Require(!ro::net::BuildActiveUseSkillToPosInfoPacket(0x0123, 3, 100, 200, tooLong,
            buffer.data(), static_cast<int>(buffer.size()), &packetLength),
            "0x007E builder accepted text the server's bounded copy would truncate");
        Require(!ro::net::BuildActiveUseSkillToPosInfoPacket(0x0123, 3, 100, 200, contents,
            buffer.data(), static_cast<int>(buffer.size() - 1), &packetLength),
            "0x007E builder accepted an undersized output buffer");

        std::array<u8, sizeof(PACKET_CZ_USESKILLMAP)> mapBuffer{};
        Require(ro::net::BuildActiveUseSkillMapPacket(27, "prontera.gat",
            mapBuffer.data(), static_cast<int>(mapBuffer.size()), &packetLength),
            "0x011B builder rejected a valid Warp destination");
        Require(packetLength == 20 && mapBuffer[0] == 0x1B && mapBuffer[1] == 0x01
            && ReadLE16(mapBuffer.data() + 2) == 27
            && std::memcmp(mapBuffer.data() + 4, "prontera.gat", 12) == 0
            && mapBuffer[16] == 0,
            "0x011B opcode, length, skill id, or destination offset differs from Ref");
        std::cout << "UseSkillToPosInfo opcode=0x007E size=102 skill=291 level=3 cell=(100,200) textBytes=7\n";
        std::cout << "UseSkillMap opcode=0x011B size=20 skill=27 map=prontera.gat\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
