#include "../../src/core/ChatRoomPackets.h"
#include "../../src/core/ChatRoomRequests.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

int main()
{
    const auto require = [](bool ok) {
        if (!ok) {
            std::cerr << "chat room packet literal mismatch\n";
            std::exit(1);
        }
    };
    using namespace ro::chat;

    const std::uint8_t listingPacket[] = {
        0xd7, 0x00, 0x15, 0x00,
        0x98, 0xba, 0xdc, 0xfe,
        0xef, 0xcd, 0xab, 0x89,
        0xff, 0xff, 0x00, 0x80,
        0x03, 'R', 'o', 'o', 'm',
    };
    RoomListing listing;
    require(ParseRoomListing(listingPacket, sizeof(listingPacket), listing));
    require(listing.owner == 0xfedcba98u);
    require(listing.info.roomId == 0x89abcdefu);
    require(listing.info.maxNumPeople == -1 && listing.info.numPeople == -32768);
    require(listing.info.roomType == 3 && listing.info.title == "Room" && listing.info.pass.empty());

    const std::uint8_t emptyChangePacket[] = {
        0xdf, 0x00, 0x11, 0x00,
        0x01, 0x00, 0x00, 0x80,
        0x02, 0x00, 0x00, 0x80,
        0x00, 0x80, 0xff, 0xff,
        0x00,
    };
    require(ParseRoomListing(emptyChangePacket, sizeof(emptyChangePacket), listing));
    require(listing.owner == 0x80000001u && listing.info.roomId == 0x80000002u);
    require(listing.info.maxNumPeople == -32768 && listing.info.numPeople == -1);
    require(listing.info.title.empty());

    std::vector<std::uint8_t> longTitlePacket = {
        0xd7, 0x00, 0x51, 0x00,
        0x04, 0x03, 0x02, 0x01,
        0x08, 0x07, 0x06, 0x05,
        0x0a, 0x00, 0x09, 0x00,
        0x02,
    };
    longTitlePacket.insert(longTitlePacket.end(), 64, static_cast<std::uint8_t>('T'));
    require(ParseRoomListing(longTitlePacket.data(), longTitlePacket.size(), listing));
    require(listing.info.title == std::string(60, 'T'));

    const std::uint8_t emptyEntryPacket[] = {
        0xdb, 0x00, 0x08, 0x00,
        0x44, 0x33, 0x22, 0x11,
    };
    RoomEntry entry;
    require(ParseRoomEntry(emptyEntryPacket, sizeof(emptyEntryPacket), entry));
    require(entry.roomId == 0x11223344u && entry.members.empty());

    const std::uint8_t fullNameEntryPacket[] = {
        0xdb, 0x00, 0x24, 0x00,
        0xef, 0xcd, 0xab, 0x89,
        0x98, 0xba, 0xdc, 0xfe,
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
        'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
        'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    };
    require(ParseRoomEntry(fullNameEntryPacket, sizeof(fullNameEntryPacket), entry));
    require(entry.roomId == 0x89abcdefu && entry.members.size() == 1);
    require(entry.members[0].role == 0xfedcba98u);
    require(entry.members[0].name == "ABCDEFGHIJKLMNOPQRSTUVWX");

    const std::uint8_t mismatchedListingLength[] = {
        0xd7, 0x00, 0x12, 0x00,
        0x01, 0x00, 0x00, 0x00,
        0x02, 0x00, 0x00, 0x00,
        0x03, 0x00, 0x04, 0x00,
        0x01,
    };
    const RoomListing priorListing = listing;
    require(!ParseRoomListing(mismatchedListingLength, sizeof(mismatchedListingLength), listing));
    require(listing.owner == priorListing.owner && listing.info.roomId == priorListing.info.roomId
        && listing.info.title == priorListing.info.title);

    const std::uint8_t negativeListingLength[] = {
        0xdf, 0x00, 0xff, 0xff,
        0x01, 0x00, 0x00, 0x00,
        0x02, 0x00, 0x00, 0x00,
        0x03, 0x00, 0x04, 0x00,
        0x01,
    };
    require(!ParseRoomListing(negativeListingLength, sizeof(negativeListingLength), listing));

    const std::uint8_t mismatchedEntryLength[] = {
        0xdb, 0x00, 0x09, 0x00,
        0x44, 0x33, 0x22, 0x11,
    };
    const RoomEntry priorEntry = entry;
    require(!ParseRoomEntry(mismatchedEntryLength, sizeof(mismatchedEntryLength), entry));
    require(entry.roomId == priorEntry.roomId && entry.members.size() == priorEntry.members.size()
        && entry.members[0].name == priorEntry.members[0].name);

    const std::uint8_t misalignedEntry[] = {
        0xdb, 0x00, 0x09, 0x00,
        0x44, 0x33, 0x22, 0x11, 0xaa,
    };
    require(!ParseRoomEntry(misalignedEntry, sizeof(misalignedEntry), entry));
    require(entry.roomId == priorEntry.roomId && entry.members.size() == priorEntry.members.size());

    const std::uint8_t wrongListingOpcode[] = {
        0xd8, 0x00, 0x11, 0x00,
        0x01, 0x00, 0x00, 0x00,
        0x02, 0x00, 0x00, 0x00,
        0x03, 0x00, 0x04, 0x00,
        0x01,
    };
    require(!ParseRoomListing(wrongListingOpcode, sizeof(wrongListingOpcode), listing));

    const std::array<std::uint8_t, 8> roomPassword = { 'p', 'a', 's', 's', 0, 0xfe, 0, 0xff };
    std::vector<std::uint8_t> request;
    const std::vector<std::uint8_t> expectedCreate = {
        0xd5, 0x00, 0x15, 0x00, 0x14, 0x00, 0x01,
        'p', 'a', 's', 's', 0x00, 0xfe, 0x00, 0xff,
        'R', 'o', 'o', 'm', ' ', 'A',
    };
    require(BuildRoomCreateRequest(20, 1, roomPassword, "Room A", request));
    require(request == expectedCreate);

    const std::vector<std::uint8_t> expectedChange = {
        0xde, 0x00, 0x11, 0x00, 0x02, 0x00, 0x00,
        'p', 'a', 's', 's', 0x00, 0xfe, 0x00, 0xff,
        'X', 'Y',
    };
    require(BuildRoomChangeRequest(2, 0, roomPassword, "XY", request));
    require(request == expectedChange);

    const std::array<std::uint8_t, 8> joinPassword = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 };
    const std::vector<std::uint8_t> expectedJoin = {
        0xd9, 0x00, 0xef, 0xcd, 0xab, 0x89,
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    };
    BuildRoomJoinRequest(0x89abcdefu, joinPassword, request);
    require(request == expectedJoin);

    const std::vector<std::uint8_t> expectedRole = {
        0xe0, 0x00, 0x00, 0x00, 0x00, 0x00,
        'B', 'o', 'b', 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    };
    require(BuildRoomRoleRequest(0, "Bob", request));
    require(request == expectedRole);

    const std::vector<std::uint8_t> expectedKick = {
        0xe2, 0x00,
        'E', 'v', 'e', 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
    };
    require(BuildRoomKickRequest("Eve", request));
    require(request == expectedKick);

    BuildRoomLeaveRequest(request);
    require(request == std::vector<std::uint8_t>({ 0xe3, 0x00 }));

    const std::vector<std::uint8_t> priorRequest = request;
    require(!BuildRoomRoleRequest(1, std::string(25, 'N'), request));
    require(request == priorRequest);
    require(!BuildRoomKickRequest(std::string(25, 'N'), request));
    require(request == priorRequest);
    require(!BuildRoomCreateRequest(20, 1, roomPassword,
        std::string(static_cast<std::size_t>(0x7fff - 14), 'T'), request));
    require(request == priorRequest);
}
