#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace ro::chat {

struct RoomInfo {
    std::string title;
    std::string pass;
    int roomType = 0;
    int numPeople = 0;
    int maxNumPeople = 0;
    std::uint32_t roomId = 0;
};

struct RoomListing {
    std::uint32_t owner = 0;
    RoomInfo info;
};

struct RoomMember {
    std::uint32_t role = 0;
    std::string name;
};

struct RoomEntry {
    std::uint32_t roomId = 0;
    std::vector<RoomMember> members;
};

namespace detail {
inline std::uint16_t ReadRoomU16(const std::uint8_t* data)
{
    return static_cast<std::uint16_t>(data[0])
        | static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[1]) << 8);
}

inline std::uint32_t ReadRoomU32(const std::uint8_t* data)
{
    return static_cast<std::uint32_t>(data[0])
        | (static_cast<std::uint32_t>(data[1]) << 8)
        | (static_cast<std::uint32_t>(data[2]) << 16)
        | (static_cast<std::uint32_t>(data[3]) << 24);
}

inline int ReadRoomI16(const std::uint8_t* data)
{
    const std::uint16_t value = ReadRoomU16(data);
    return value >= 0x8000u ? static_cast<int>(value) - 0x10000 : static_cast<int>(value);
}

inline bool HasValidRoomPacketLength(const std::uint8_t* data, std::size_t size)
{
    if (!data || size < 4 || size > 0x7fffu) {
        return false;
    }
    const int declaredLength = ReadRoomI16(data + 2);
    return declaredLength > 0 && static_cast<std::size_t>(declaredLength) == size;
}

inline std::string ReadBoundedRoomName(const std::uint8_t* data, std::size_t capacity)
{
    std::size_t length = 0;
    while (length < capacity && data[length] != 0) {
        ++length;
    }
    return std::string(reinterpret_cast<const char*>(data), length);
}
} // namespace detail

// Reads ZC_ROOM_NEWENTRY (0x00D7) or ZC_CHANGE_CHATROOM (0x00DF).
// The wire title is clipped at 60 source bytes, matching the legacy client;
// multibyte UTF-8 sequences can therefore be cut at the boundary.
inline bool ParseRoomListing(const std::uint8_t* data, std::size_t size, RoomListing& output)
{
    if (!data || size < 17 || !detail::HasValidRoomPacketLength(data, size)) {
        return false;
    }
    const std::uint16_t packetId = detail::ReadRoomU16(data);
    if (packetId != 0x00d7u && packetId != 0x00dfu) {
        return false;
    }

    RoomListing parsed;
    parsed.owner = detail::ReadRoomU32(data + 4);
    parsed.info.roomId = detail::ReadRoomU32(data + 8);
    parsed.info.maxNumPeople = detail::ReadRoomI16(data + 12);
    parsed.info.numPeople = detail::ReadRoomI16(data + 14);
    parsed.info.roomType = data[16];
    parsed.info.pass.clear();
    const std::size_t titleLength = (size - 17 < 60) ? size - 17 : 60;
    parsed.info.title.assign(reinterpret_cast<const char*>(data + 17), titleLength);
    output = std::move(parsed);
    return true;
}

// Reads ZC_ENTER_ROOM (0x00DB), whose payload is a room ID followed by
// fixed 28-byte {role, name[24]} member records.
inline bool ParseRoomEntry(const std::uint8_t* data, std::size_t size, RoomEntry& output)
{
    constexpr std::size_t headerSize = 8;
    constexpr std::size_t memberSize = 28;
    if (!data || size < headerSize || !detail::HasValidRoomPacketLength(data, size)
        || detail::ReadRoomU16(data) != 0x00dbu
        || (size - headerSize) % memberSize != 0) {
        return false;
    }

    RoomEntry parsed;
    parsed.roomId = detail::ReadRoomU32(data + 4);
    parsed.members.reserve((size - headerSize) / memberSize);
    for (std::size_t offset = headerSize; offset < size; offset += memberSize) {
        RoomMember member;
        member.role = detail::ReadRoomU32(data + offset);
        member.name = detail::ReadBoundedRoomName(data + offset + 4, 24);
        parsed.members.push_back(std::move(member));
    }
    output = std::move(parsed);
    return true;
}

} // namespace ro::chat
