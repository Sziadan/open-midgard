#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <utility>
#include <vector>

namespace ro::chat {

namespace room_request_detail {

inline void AppendU16(std::vector<std::uint8_t>& packet, std::uint16_t value)
{
    packet.push_back(static_cast<std::uint8_t>(value & 0xffu));
    packet.push_back(static_cast<std::uint8_t>((value >> 8) & 0xffu));
}

inline void AppendU32(std::vector<std::uint8_t>& packet, std::uint32_t value)
{
    packet.push_back(static_cast<std::uint8_t>(value & 0xffu));
    packet.push_back(static_cast<std::uint8_t>((value >> 8) & 0xffu));
    packet.push_back(static_cast<std::uint8_t>((value >> 16) & 0xffu));
    packet.push_back(static_cast<std::uint8_t>((value >> 24) & 0xffu));
}

inline bool BuildTitleRequest(std::uint16_t opcode, std::uint16_t limit, std::uint8_t type,
    const std::array<std::uint8_t, 8>& password, std::string_view title,
    std::vector<std::uint8_t>& output)
{
    constexpr std::size_t headerSize = 15;
    constexpr std::size_t maxTitleSize = static_cast<std::size_t>(std::numeric_limits<std::int16_t>::max()) - headerSize;
    if (title.size() > maxTitleSize) {
        return false;
    }

    std::vector<std::uint8_t> packet;
    packet.reserve(headerSize + title.size());
    AppendU16(packet, opcode);
    AppendU16(packet, static_cast<std::uint16_t>(headerSize + title.size()));
    AppendU16(packet, limit);
    packet.push_back(type);
    packet.insert(packet.end(), password.begin(), password.end());
    for (const char byte : title) {
        packet.push_back(static_cast<std::uint8_t>(static_cast<unsigned char>(byte)));
    }
    output = std::move(packet);
    return true;
}

inline bool AppendFixedName(std::vector<std::uint8_t>& packet, std::string_view name)
{
    constexpr std::size_t nameSize = 24;
    if (name.size() > nameSize) {
        return false;
    }
    for (const char byte : name) {
        packet.push_back(static_cast<std::uint8_t>(static_cast<unsigned char>(byte)));
    }
    packet.resize(packet.size() + nameSize - name.size(), 0);
    return true;
}

} // namespace room_request_detail

// These are wire encoders only. UI validation, room membership and receipt
// handling belong to their callers. A false result leaves output untouched.
// Title bytes are sent verbatim with no terminator; the signed 16-bit total
// length field bounds the variable portion. Password always occupies 8 bytes.
inline bool BuildRoomCreateRequest(std::uint16_t limit, std::uint8_t type,
    const std::array<std::uint8_t, 8>& password, std::string_view title,
    std::vector<std::uint8_t>& output)
{
    return room_request_detail::BuildTitleRequest(0x00d5u, limit, type, password, title, output);
}

inline bool BuildRoomChangeRequest(std::uint16_t limit, std::uint8_t type,
    const std::array<std::uint8_t, 8>& password, std::string_view title,
    std::vector<std::uint8_t>& output)
{
    return room_request_detail::BuildTitleRequest(0x00deu, limit, type, password, title, output);
}

inline void BuildRoomJoinRequest(std::uint32_t roomId,
    const std::array<std::uint8_t, 8>& password, std::vector<std::uint8_t>& output)
{
    std::vector<std::uint8_t> packet;
    packet.reserve(14);
    room_request_detail::AppendU16(packet, 0x00d9u);
    room_request_detail::AppendU32(packet, roomId);
    packet.insert(packet.end(), password.begin(), password.end());
    output = std::move(packet);
}

inline bool BuildRoomRoleRequest(std::uint32_t role, std::string_view name,
    std::vector<std::uint8_t>& output)
{
    std::vector<std::uint8_t> packet;
    packet.reserve(30);
    room_request_detail::AppendU16(packet, 0x00e0u);
    room_request_detail::AppendU32(packet, role);
    if (!room_request_detail::AppendFixedName(packet, name)) {
        return false;
    }
    output = std::move(packet);
    return true;
}

inline bool BuildRoomKickRequest(std::string_view name, std::vector<std::uint8_t>& output)
{
    std::vector<std::uint8_t> packet;
    packet.reserve(26);
    room_request_detail::AppendU16(packet, 0x00e2u);
    if (!room_request_detail::AppendFixedName(packet, name)) {
        return false;
    }
    output = std::move(packet);
    return true;
}

inline void BuildRoomLeaveRequest(std::vector<std::uint8_t>& output)
{
    output = {0xe3, 0x00};
}

} // namespace ro::chat
