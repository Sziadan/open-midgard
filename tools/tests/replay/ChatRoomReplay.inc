#include "ui/UIChatRoomWnd.h"
#include <cstddef>
#include <iterator>

namespace {

void RoomPut16(std::vector<u8>& packet, size_t offset, u16 value)
{
    packet[offset] = static_cast<u8>(value);
    packet[offset + 1] = static_cast<u8>(value >> 8);
}

void RoomPut32(std::vector<u8>& packet, size_t offset, u32 value)
{
    for (int i = 0; i < 4; ++i) {
        packet[offset + static_cast<size_t>(i)] = static_cast<u8>(value >> (8 * i));
    }
}

std::vector<u8> RoomListingFrame(u16 opcode, u32 owner, u32 roomId,
    int maxPeople, int people, u8 type, const std::string& title)
{
    std::vector<u8> packet(17 + title.size());
    RoomPut16(packet, 0, opcode);
    RoomPut16(packet, 2, static_cast<u16>(packet.size()));
    RoomPut32(packet, 4, owner);
    RoomPut32(packet, 8, roomId);
    RoomPut16(packet, 12, static_cast<u16>(maxPeople));
    RoomPut16(packet, 14, static_cast<u16>(people));
    packet[16] = type;
    std::copy(title.begin(), title.end(), packet.begin() + 17);
    return packet;
}

struct RoomMemberSample {
    u32 role;
    std::string name;
};

std::vector<u8> RoomEntryFrame(u32 roomId, const std::vector<RoomMemberSample>& members)
{
    std::vector<u8> packet(8 + 28 * members.size());
    RoomPut16(packet, 0, 0x00db);
    RoomPut16(packet, 2, static_cast<u16>(packet.size()));
    RoomPut32(packet, 4, roomId);
    for (size_t i = 0; i < members.size(); ++i) {
        const size_t offset = 8 + 28 * i;
        RoomPut32(packet, offset, members[i].role);
        const size_t copyLength = (std::min)(members[i].name.size(), size_t{24});
        std::copy_n(members[i].name.begin(), copyLength,
            packet.begin() + static_cast<std::ptrdiff_t>(offset + 4));
    }
    return packet;
}

std::vector<u8> RoomDestroyFrame(u32 roomId)
{
    std::vector<u8> packet(6);
    RoomPut16(packet, 0, 0x00d8);
    RoomPut32(packet, 2, roomId);
    return packet;
}

std::vector<u8> RoomMemberChangeFrame(u16 opcode, int people,
    const std::string& name, u8 exitReason = 0)
{
    const size_t size = opcode == 0x00dd ? 29 : 28;
    std::vector<u8> packet(size);
    RoomPut16(packet, 0, opcode);
    RoomPut16(packet, 2, static_cast<u16>(people));
    const size_t copyLength = (std::min)(name.size(), size_t{24});
    std::copy_n(name.begin(), copyLength, packet.begin() + 4);
    if (opcode == 0x00dd) {
        packet[28] = exitReason;
    }
    return packet;
}

std::vector<u8> RoomRoleChangeFrame(u32 role, const std::string& name)
{
    std::vector<u8> packet(30);
    RoomPut16(packet, 0, 0x00e1);
    RoomPut32(packet, 2, role);
    const size_t copyLength = (std::min)(name.size(), size_t{24});
    std::copy_n(name.begin(), copyLength, packet.begin() + 6);
    return packet;
}

std::vector<u8> RoomChatFrame(u16 opcode, const std::string& text, u32 speaker = 0)
{
    const size_t offset = opcode == 0x008d ? 8 : 4;
    std::vector<u8> packet(offset + text.size() + 1, 0);
    RoomPut16(packet, 0, opcode);
    RoomPut16(packet, 2, static_cast<u16>(packet.size()));
    if (opcode == 0x008d) {
        RoomPut32(packet, 4, speaker);
    }
    std::copy(text.begin(), text.end(), packet.begin() + static_cast<std::ptrdiff_t>(offset));
    return packet;
}

bool SameSubChatRows(const std::vector<SubChatRow>& left, const std::vector<SubChatRow>& right)
{
    if (left.size() != right.size()) {
        return false;
    }
    for (size_t i = 0; i < left.size(); ++i) {
        if (left[i].text != right[i].text || left[i].color != right[i].color
            || left[i].talker != right[i].talker) {
            return false;
        }
    }
    return true;
}

} // namespace

void ChatRoomReplay()
{
    SetCase("chat_room_packet_lifecycle", "PK09.21",
        "selected-20080910-d7-db-d8-dc-dd-df-e1",
        "ProcessGameModePacketLoop -> production router -> room/chat handlers -> real world actors and room UI");

    constexpr u32 ownerAid = 0xfedcba98u;
    constexpr u32 ownerRoomId = 0x89abcdefu;
    constexpr u32 mismatchRoomId = 0x76543210u;
    constexpr u32 noOwnerRoomId = 0x80000011u;

    CWorld world;
    CPlayer player;
    CGameActor owner;
    CGameMode mode;
    TestConnection connection;
    CGameModePacketRouter& router = GetProductionGameModePacketRouter();

    struct Restore {
        CWorld& world;
        CPlayer& player;
        CGameActor& owner;
        CGameMode& mode;
        CMode* currentMode;
        int currentModeType;
        u32 sessionAid, sessionGid;
        std::array<char, 25> playerName{};
        std::list<u32> excludedAids;
        ServiceType serviceType;
        UIChatRoomWnd* oldRoom;
        std::list<UIWindow*> oldChildren;
        UIWindow* oldEdit;
        UIWindow* oldCapture;
        UIWindow* oldModal;
        UIWindow* oldLastHit;
        SubChatWindowInfo oldRoomPlacement;
        std::vector<UIChatEvent> oldEvents;
        int oldRecordCount, oldRepeatCount;
        std::string oldLastChat;
        std::array<std::string, 11> oldRecordChat;
        std::array<u32, 11> oldRecordTimes{};

        explicit Restore(CWorld& w, CPlayer& p, CGameActor& a, CGameMode& m)
            : world(w), player(p), owner(a), mode(m),
              currentMode(SkillPacketReplayAccess::Current()),
              currentModeType(SkillPacketReplayAccess::CurrentType()),
              sessionAid(g_session.m_aid), sessionGid(g_session.m_gid),
              excludedAids(g_session.m_exAidList), serviceType(g_serviceType),
              oldRoom(g_windowMgr.m_chatRoomWnd), oldChildren(g_windowMgr.m_children),
              oldEdit(g_windowMgr.m_editWindow), oldCapture(g_windowMgr.m_captureWindow),
              oldModal(g_windowMgr.m_modalWindow), oldLastHit(g_windowMgr.m_lastHitWindow),
              oldRoomPlacement(g_windowMgr.m_chatRoomWindowInfo),
              oldEvents(g_windowMgr.GetChatEvents()),
              oldRecordCount(m.m_recordChatNum), oldRepeatCount(m.m_sameChatRepeatCnt),
              oldLastChat(m.m_lastChat)
        {
            std::copy(std::begin(g_session.m_playerName), std::end(g_session.m_playerName), playerName.begin());
            for (size_t i = 0; i < oldRecordChat.size(); ++i) {
                oldRecordChat[i] = m.m_recordChat[i];
                oldRecordTimes[i] = m.m_recordChatTime[i];
            }
            for (UIWindow* window : oldChildren) {
                if (window == oldRoom) {
                    g_windowMgr.m_children.remove(oldRoom);
                    break;
                }
            }
            if (oldRoom) {
                g_windowMgr.m_chatRoomWnd = nullptr;
                if (g_windowMgr.m_editWindow == oldRoom) {
                    g_windowMgr.m_editWindow = nullptr;
                }
            }
            g_windowMgr.m_chatRoomWindowInfo = {500, 180, 280, 120};
            g_windowMgr.ClearChatEvents();
        }

        ~Restore()
        {
            owner.SendMsg(nullptr, 33, 0, 0, 0);
            world.UnregisterActor(&owner);
            world.UnregisterActor(&player);
            world.m_player = nullptr;
            mode.m_runtimeActors.clear();
            mode.m_world = nullptr;
            if (g_windowMgr.m_chatRoomWnd && g_windowMgr.m_chatRoomWnd != oldRoom) {
                g_windowMgr.DeleteWindow(g_windowMgr.m_chatRoomWnd);
            }
            g_windowMgr.m_children = oldChildren;
            g_windowMgr.m_chatRoomWnd = oldRoom;
            g_windowMgr.m_editWindow = oldEdit;
            g_windowMgr.m_captureWindow = oldCapture;
            g_windowMgr.m_modalWindow = oldModal;
            g_windowMgr.m_lastHitWindow = oldLastHit;
            g_windowMgr.m_chatRoomWindowInfo = oldRoomPlacement;
            g_windowMgr.ClearChatEvents();
            for (const UIChatEvent& event : oldEvents) {
                g_windowMgr.PushChatEvent(event.text.c_str(), event.color, event.channel,
                    event.tick, event.source);
            }
            g_session.m_aid = sessionAid;
            g_session.m_gid = sessionGid;
            std::copy(playerName.begin(), playerName.end(), std::begin(g_session.m_playerName));
            g_session.m_exAidList = excludedAids;
            g_serviceType = serviceType;
            mode.m_recordChatNum = oldRecordCount;
            mode.m_sameChatRepeatCnt = oldRepeatCount;
            mode.m_lastChat = oldLastChat;
            for (size_t i = 0; i < oldRecordChat.size(); ++i) {
                mode.m_recordChat[i] = oldRecordChat[i];
                mode.m_recordChatTime[i] = oldRecordTimes[i];
            }
            SkillPacketReplayAccess::Restore(currentMode, currentModeType);
            GetProductionGameModePacketRouter().Clear();
            RegisterDefaultGameModePacketHandlers(GetProductionGameModePacketRouter());
        }
    } restore(world, player, owner, mode);

    world.m_player = &player;
    player.m_gid = g_session.m_gid = 0x11223344u;
    g_session.m_aid = 0x55667788u;
    std::memset(g_session.m_playerName, 0, sizeof(g_session.m_playerName));
    std::memcpy(g_session.m_playerName, "Local", 6);
    g_session.m_exAidList.clear();
    owner.m_gid = ownerAid;
    owner.m_isPc = 1;
    owner.SetChatBubbleText("owner bubble before room", GetTickCount() + 60000u);
    player.SetChatBubbleText("player bubble before room", GetTickCount() + 60000u);
    world.RegisterActor(&player);
    world.RegisterActor(&owner);
    mode.m_world = &world;
    mode.m_runtimeActors[ownerAid] = &owner;
    mode.m_lastChatroomInfo.title = "prior room metadata";
    mode.m_lastChatroomInfo.roomId = 0x10203040u;
    SkillPacketReplayAccess::SetCurrent(&mode);
    router.Clear();
    RegisterDefaultGameModePacketHandlers(router);

    const auto dispatch = [&](const std::vector<u8>& packet, const char* name) {
        Require(connection.QueueBytes(packet), std::string("room packet queue failed: ") + name);
        const auto result = ProcessGameModePacketLoop(mode, connection, router, 1);
        Require(result.packetsProcessed == 1 && result.packetsDispatched == 1
            && result.packetsUnhandled == 0 && connection.BufferedBytes() == 0,
            std::string("room packet missed production route: ") + name);
    };
    const auto entryWithNames = [](u32 roomId) {
        return RoomEntryFrame(roomId, {
            {0, "Zed"}, {1, "alice"}, {1, "Mira"}, {1, "amy"},
        });
    };

    // A valid DB packet with no owner metadata consumes the pending request,
    // remembers roomId -> 0, and cannot invent a title or open a room.
    mode.m_isWaitingEnterRoom = 1;
    dispatch(RoomEntryFrame(noOwnerRoomId, {}), "DB entry without owner metadata");
    Require(mode.m_isWaitingEnterRoom == 0, "DB did not clear the pending room wait");
    Require(mode.m_chatRoomOwners.count(noOwnerRoomId) == 1
        && mode.m_chatRoomOwners.at(noOwnerRoomId) == 0,
        "ownerless DB did not retain its zero room-owner mapping");
    Require(g_windowMgr.m_chatRoomWnd == nullptr
        && mode.m_lastChatroomInfo.title == "prior room metadata"
        && mode.m_lastChatroomInfo.roomId == 0x10203040u,
        "ownerless DB inferred room metadata or opened the room window");

    const auto listing = RoomListingFrame(0x00d7, ownerAid, ownerRoomId, 8, 1, 1, "Public sample room");
    dispatch(listing, "D7 room listing");
    Require(mode.m_chatRoomOwners.at(ownerRoomId) == ownerAid,
        "D7 did not record roomId to owner AID");
    Require(owner.m_chatTitle != nullptr && owner.m_chatTitle->m_roomInfo.title == "Public sample room"
        && owner.m_chatTitle->m_roomInfo.roomId == ownerRoomId
        && owner.m_chatTitle->m_roomInfo.maxNumPeople == 8
        && owner.m_chatTitle->m_roomInfo.numPeople == 1
        && owner.m_chatTitle->m_roomInfo.pass.empty(),
        "D7 did not attach the listing metadata to its owner actor");
    Require(g_windowMgr.m_chatRoomWnd == nullptr,
        "D7 incorrectly opened the local active room window");

    // A DB room ID mapped to the same owner but different from its title removes
    // the stale owner title and does not infer metadata from the packet roster.
    mode.m_chatRoomOwners[mismatchRoomId] = ownerAid;
    mode.m_isWaitingEnterRoom = 1;
    dispatch(RoomEntryFrame(mismatchRoomId, {}), "DB room ID mismatches owner title");
    Require(mode.m_isWaitingEnterRoom == 0 && mode.m_chatRoomOwners.at(mismatchRoomId) == ownerAid,
        "mismatched DB did not clear wait while retaining the pre-existing owner map");
    Require(owner.m_chatTitle == nullptr && g_windowMgr.m_chatRoomWnd == nullptr,
        "mismatched DB did not remove the owner title or opened a room window");
    Require(mode.m_lastChatroomInfo.title == "prior room metadata"
        && mode.m_lastChatroomInfo.roomId == 0x10203040u,
        "mismatched DB overwrote the last matched room metadata");

    // DB reuses D7's actor-owned title data, then consumes that bubble and uses
    // the roster records in packet order while updating the authoritative count.
    dispatch(listing, "D7 owner listing before DB match");
    dispatch(entryWithNames(ownerRoomId), "matching DB entry");
    UIChatRoomWnd* room = g_windowMgr.m_chatRoomWnd;
    Require(room != nullptr, "matching DB did not open the room window");
    Require(owner.m_chatTitle == nullptr, "matching DB left the owner's listing title attached");
    Require(room->GetInfo().title == "Public sample room" && room->GetInfo().roomId == ownerRoomId
        && room->GetInfo().maxNumPeople == 8 && room->GetInfo().numPeople == 4,
        "matching DB did not copy actor metadata and replace count with roster size");
    Require(room->GetInfo().pass.empty(), "D7 listing invented a room password");
    const auto& arrivalRows = room->GetMemberRows();
    Require(arrivalRows.size() == 4 && arrivalRows[0].name == "Zed" && arrivalRows[0].role == 0
        && arrivalRows[1].name == "alice" && arrivalRows[1].role == 1
        && arrivalRows[2].name == "Mira" && arrivalRows[2].role == 1
        && arrivalRows[3].name == "amy" && arrivalRows[3].role == 1,
        "DB roster did not preserve member arrival order, roles, or case");
    Require(room->GetSelectedMember() == -1,
        "DB roster clear did not reset selected member state");
    Require(mode.m_lastChatroomInfo.title == "Public sample room"
        && mode.m_lastChatroomInfo.roomId == ownerRoomId
        && mode.m_lastChatroomInfo.numPeople == 4
        && mode.m_lastChatroomInfo.maxNumPeople == 8,
        "matching DB did not retain the room metadata snapshot");

    const std::string wrongCaseRosterName = "Alice";
    room->SendMsg(nullptr, 45, reinterpret_cast<msgparam_t>(wrongCaseRosterName.c_str()), 0, 0);
    Require(room->GetMembers().count("alice") == 1 && room->GetMembers().count("Alice") == 0
        && room->GetMemberRows()[0].name == "Zed" && room->GetMemberRows()[1].name == "alice"
        && room->GetMemberRows()[2].name == "Mira" && room->GetMemberRows()[3].name == "amy",
        "Remove45 matched a roster name without preserving case");

    // Room chat stays in the room history: it does not reach the main event
    // buffer/history and it does not replace either actor's speech bubble.
    const size_t eventCountBeforeRoomSpeech = g_windowMgr.GetChatEvents().size();
    const int mainRecordCountBeforeRoomSpeech = mode.m_recordChatNum;
    const std::string ownerBubble = owner.GetChatBubbleText();
    const std::string playerBubble = player.GetChatBubbleText();
    const std::vector<SubChatRow> mainHistoryBefore = g_windowMgr.m_chatWnd
        ? g_windowMgr.m_chatWnd->GetSourceHistory(false) : std::vector<SubChatRow>{};
    const std::vector<SubChatRow> commandHistoryBefore = g_windowMgr.m_chatWnd
        ? g_windowMgr.m_chatWnd->GetSourceHistory(true) : std::vector<SubChatRow>{};
    const std::string peerText = "Owner : room message";
    const std::string playerText = "Local room message";
    dispatch(RoomChatFrame(0x008d, peerText, ownerAid), "008D chat while in room");
    Require(room->GetRows().size() == 1 && room->GetRows().back().text == peerText
        && room->GetRows().back().color == 0x00222222u,
        "008D did not route the peer message to the room with source color");
    dispatch(RoomChatFrame(0x008e, playerText), "008E player chat while in room");
    Require(room->GetRows().size() == 2 && room->GetRows().back().text == playerText
        && room->GetRows().back().color == ro::chat::ColorRefToUiRgb(30720u),
        "008E did not route player chat to the room with source color");
    Require(owner.GetChatBubbleText() == ownerBubble && player.GetChatBubbleText() == playerBubble,
        "room chat changed an actor speech bubble");
    Require(g_windowMgr.GetChatEvents().size() == eventCountBeforeRoomSpeech
        && mode.m_recordChatNum == mainRecordCountBeforeRoomSpeech,
        "room chat entered the main chat event or record buffer");
    if (g_windowMgr.m_chatWnd) {
        Require(SameSubChatRows(g_windowMgr.m_chatWnd->GetSourceHistory(false), mainHistoryBefore)
            && SameSubChatRows(g_windowMgr.m_chatWnd->GetSourceHistory(true), commandHistoryBefore),
            "room chat changed main chat history");
    }

    const size_t roomRowsBeforeExcluded = room->GetRows().size();
    const std::string lastRoomChatBeforeExcluded = mode.m_lastChat;
    g_session.m_exAidList.push_back(ownerAid);
    dispatch(RoomChatFrame(0x008d, "Owner : excluded", ownerAid), "excluded 008D chat");
    Require(room->GetRows().size() == roomRowsBeforeExcluded
        && mode.m_lastChat == lastRoomChatBeforeExcluded
        && owner.GetChatBubbleText() == ownerBubble,
        "excluded AID room chat changed room text, last chat, or actor bubble");
    Require(g_windowMgr.GetChatEvents().size() == eventCountBeforeRoomSpeech
        && mode.m_recordChatNum == mainRecordCountBeforeRoomSpeech,
        "excluded AID room chat entered the main chat buffers");
    g_session.m_exAidList.clear();

    // Basic room UI paging and source-size quantization boundaries.
    for (int i = 0; i < 20; ++i) {
        room->Append("history row", 0);
    }
    Require(room->GetRows().back().color == 0,
        "black room-history rows were changed to a default color");
    const int bottomFirst = room->GetFirst();
    room->SendMsg(nullptr, 3, 0, 0, 0);
    Require(room->GetFirst() < bottomFirst, "room history page-up message did not page upward");
    room->SendMsg(nullptr, 4, 0, 0, 0);
    Require(room->GetFirst() == bottomFirst, "room history page-down message did not return to the bottom");
    room->ResizeSource(319, 159);
    Require(room->m_w == 312 && room->m_h == 152,
        "room resize did not quantize dimensions in source increments");
    room->ResizeSource(5, 5);
    Require(room->m_w == 280 && room->m_h == 120,
        "room resize fell below source minimum dimensions");
    room->ResizeSource(900, 900);
    Require(room->m_w == 504 && room->m_h == 376,
        "room resize exceeded source maximum dimensions");

    // Room's generic Resize slot is inherited UIWindow::Resize: it changes
    // the frame only. Child layout and scroll page belong to source message14.
    room->ResizeSource(280,120);
    room->Resize(400,200);
    Require(room->m_w==400 && room->m_h==200 && room->GetLayout().history.right==222
        && room->GetLayout().history.bottom==99 && room->GetLayout().edit.top==102
        && room->GetScrollModel().page==5 && room->GetScrollModel().axis==82,
        "generic parent resize reflowed source room children");
    room->Resize(312,152);room->ResizeSource(312,152);
    Require(room->GetLayout().history.bottom==99 && room->GetScrollModel().page==5,
        "unchanged source resize unexpectedly repaired retained child geometry");
    const auto retainedRoster=room->GetMemberRows();room->ClearMembers();
    Require(room->GetScrollModel(true).page==5,
        "member clear used generic parent height instead of its retained child height");
    for(const auto& member:retainedRoster)room->AddMember(member.name,member.role);
    room->ResizeSource(344,152);
    Require(room->GetLayout().history.bottom==131 && room->GetScrollModel().page==8,
        "changed source resize did not update child geometry/page");

    // Fixed member packets carry authoritative counts; name matching is
    // case-sensitive, and Remove45 rebuilds sorted rows only for an existing key.
    dispatch(RoomMemberChangeFrame(0x00dc, 12, "Aaron"), "DC member join");
    Require(room->GetInfo().numPeople == 12 && room->GetMembers().at("Aaron") == 1
        && room->GetMemberRows().back().name == "Aaron",
        "DC did not use its count or append the new normal member");
    dispatch(RoomMemberChangeFrame(0x00dd, 11, "alice", 0), "DD peer leave");
    Require(room->GetInfo().numPeople == 11 && room->GetMembers().count("alice") == 0,
        "DD did not use its count or remove the matching member");
    Require(room->GetMembers().at("Zed") == 0 && room->GetMembers().at("Mira") == 1,
        "DB owner/normal roles were not retained in the member model");

    dispatch(RoomMemberChangeFrame(0x00dc, 25, "Bob"), "DC member join before Remove45 cases");
    Require(room->GetInfo().numPeople == 25 && room->GetMemberRows().back().name == "Bob",
        "DC did not append the member used to expose Remove45 rebuild behavior");
    const std::vector<ro::chat::RoomMember> rowsBeforeMissingRemove = room->GetMemberRows();
    const std::string wrongCaseName = "Nobody";
    room->SendMsg(nullptr, 45, reinterpret_cast<msgparam_t>(wrongCaseName.c_str()), 0, 0);
    Require(room->GetMembers().count(wrongCaseName) == 0
        && room->GetMemberRows().size() == rowsBeforeMissingRemove.size()
        && room->GetMemberRows().back().name == rowsBeforeMissingRemove.back().name,
        "Remove45 treated a missing or differently-cased name as a member");

    const std::string removedName = "Zed";
    room->SendMsg(nullptr, 45, reinterpret_cast<msgparam_t>(removedName.c_str()), 0, 0);
    const auto& sortedRows = room->GetMemberRows();
    Require(room->GetMembers().count(removedName) == 0 && sortedRows.size() == 4
        && sortedRows[0].name == "Aaron" && sortedRows[1].name == "Bob"
        && sortedRows[2].name == "Mira" && sortedRows[3].name == "amy",
        "Remove45 did not rebuild remaining member rows in case-sensitive map order");

    dispatch(RoomRoleChangeFrame(0, "Mira"), "E1 owner role change");
    Require(room->GetMembers().at("Mira") == 0 && room->GetMemberRows().size() == 4
        && room->GetMemberRows()[0].name == "Aaron"
        && room->GetMemberRows()[1].name == "Bob"
        && room->GetMemberRows()[2].name == "Mira"
        && room->GetMemberRows()[3].name == "amy",
        "E1 did not update an existing member role and rebuild sorted rows");

    dispatch(RoomMemberChangeFrame(0x00dc, 20, "Zoe"), "DC member join before missing E1");
    Require(room->GetInfo().numPeople == 20 && room->GetMemberRows().back().name == "Zoe",
        "DC did not append a member before the missing-role rebuild case");
    dispatch(RoomRoleChangeFrame(0, "Nobody"), "E1 role change for missing member");
    Require(room->GetMembers().count("Nobody") == 0 && room->GetMemberRows().size() == 5
        && room->GetMemberRows()[0].name == "Aaron"
        && room->GetMemberRows()[1].name == "Bob"
        && room->GetMemberRows()[2].name == "Mira"
        && room->GetMemberRows()[3].name == "Zoe"
        && room->GetMemberRows()[4].name == "amy",
        "E1 created a missing member or skipped its sorted roster rebuild");

    const ro::chat::RoomInfo lastMatchedInfo = mode.m_lastChatroomInfo;
    const u32 retainedOwner = mode.m_chatRoomOwners.at(ownerRoomId);
    dispatch(RoomMemberChangeFrame(0x00dd, 0, g_session.GetPlayerName(), 0), "DD local member exit");
    Require(g_windowMgr.m_chatRoomWnd == nullptr
        && mode.m_chatRoomOwners.at(ownerRoomId) == retainedOwner
        && mode.m_lastChatroomInfo.roomId == lastMatchedInfo.roomId
        && mode.m_lastChatroomInfo.title == lastMatchedInfo.title,
        "DD local exit failed to close the active room or cleared retained metadata");

    // D8 removes a matching title and closes the matching active room, while
    // retaining room-owner and last-room snapshots for the mode lifetime.
    dispatch(listing, "D7 owner listing before D8");
    dispatch(entryWithNames(ownerRoomId), "DB reopen before D8");
    Require(g_windowMgr.m_chatRoomWnd != nullptr && owner.m_chatTitle == nullptr,
        "DB setup before D8 did not open room and consume owner title");
    owner.SendMsg(nullptr, 32, reinterpret_cast<msgparam_t>(&lastMatchedInfo), 0, 0);
    Require(owner.m_chatTitle != nullptr, "could not restore owner title for D8 removal case");
    dispatch(RoomDestroyFrame(ownerRoomId), "D8 destroy matching active room");
    Require(g_windowMgr.m_chatRoomWnd == nullptr && owner.m_chatTitle == nullptr,
        "D8 did not remove owner title and close matching active room");
    Require(mode.m_chatRoomOwners.at(ownerRoomId) == retainedOwner
        && mode.m_lastChatroomInfo.roomId == lastMatchedInfo.roomId
        && mode.m_lastChatroomInfo.title == lastMatchedInfo.title,
        "D8 erased retained room-owner or last-room metadata");

    // A DF status update opens the room window from packet metadata even when
    // its owner does not resolve to a local actor.
    const auto changeWithoutActor = RoomListingFrame(0x00df, 0x76543210u,
        0x55667788u, 15, 7, 1, "DF without actor");
    dispatch(changeWithoutActor, "DF listing without local owner actor");
    Require(g_windowMgr.m_chatRoomWnd != nullptr
        && g_windowMgr.m_chatRoomWnd->GetInfo().title == "DF without actor"
        && g_windowMgr.m_chatRoomWnd->GetInfo().roomId == 0x55667788u
        && mode.m_chatRoomOwners.at(0x55667788u) == 0x76543210u,
        "DF did not open a room from listing metadata without a local owner actor");

    std::cout << "{\"case_id\":\"chat_room_packet_lifecycle\",\"contract_id\":\"PK09.21\",\"outcome\":\"PASS\"}" << std::endl;
}
