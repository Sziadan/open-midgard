namespace {

std::vector<u8> CompassFrame(u32 npcId, u32 type, u32 x, u32 y, u8 id, u32 color)
{
    std::vector<u8> frame(23, 0);
    frame[0] = 0x44;
    frame[1] = 0x01;
    const auto write32 = [&frame](size_t offset, u32 value) {
        frame[offset] = static_cast<u8>(value);
        frame[offset + 1] = static_cast<u8>(value >> 8);
        frame[offset + 2] = static_cast<u8>(value >> 16);
        frame[offset + 3] = static_cast<u8>(value >> 24);
    };
    write32(2, npcId); // Ref client ignores npcId.
    write32(6, type);
    write32(10, x);
    write32(14, y);
    frame[18] = id;
    write32(19, color);
    return frame;
}

void FeedCompassReplayPacket(CGameMode& mode, const std::vector<u8>& frame)
{
    TestConnection connection;
    LoopbackPeer peer(connection);
    peer.Send(frame);
    WaitKernelPending(connection, static_cast<int>(frame.size()));
    auto& router = GetProductionGameModePacketRouter();
    router.Clear();
    RegisterDefaultGameModePacketHandlers(router);
    const GameModePacketLoopResult result = ProcessGameModePacketLoop(mode, connection, router, 1);
    Require(result.packetsProcessed == 1 && result.packetsDispatched == 1
        && result.packetsUnhandled == 0 && connection.BufferedBytes() == 0,
        "PK25 fixture was not consumed and dispatched as one production packet");
    router.Clear();
    RegisterDefaultGameModePacketHandlers(router);
}

} // namespace

void CompassMarkerReplay()
{
    SetCase("compass_marker_0144", "PK25.03", "PK25-03-selected-20080910-0144-23-byte",
        "production packet loop -> selected 20080910 framing -> 0x0144 compass handler");
    CGameMode mode;
    mode.m_compassPosList.clear();

    const auto timed = CompassFrame(0x78563412u, 0, 321, 654, 7, 0x80402010u);
    Require(timed.size() == 23 && timed[0] == 0x44 && timed[1] == 0x01,
        "literal 0x0144 fixture does not match the 23-byte header contract");
    FeedCompassReplayPacket(mode, timed);
    Require(mode.m_compassPosList.size() == 1, "timed 0x0144 did not create one marker");
    const auto& first = mode.m_compassPosList.at(7);
    Require(first.x == 321 && first.y == 654 && first.color == 0xff402010u
        && first.type == 0,
        "0x0144 fields were decoded at incorrect offsets or npcId affected the marker");
    const u32 start = first.startTime;
    Require(!first.IsExpired(start + 15000u) && first.IsExpired(start + 15001u),
        "timed compass expiry did not use the Ref strict-greater-than 15000ms boundary");
    Require(first.IsVisible(start) && !first.IsVisible(start + 500u),
        "compass visibility did not follow the 500ms blink phase");

    FeedCompassReplayPacket(mode, CompassFrame(0x01020304u, 4, 12, 34, 7, 0xffabcdefu));
    Require(mode.m_compassPosList.size() == 1, "duplicate compass id created a second marker");
    const auto& replacement = mode.m_compassPosList.at(7);
    Require(replacement.x == 12 && replacement.y == 34 && replacement.color == 0xffabcdefu
        && replacement.type == 4,
        "duplicate compass id did not replace all marker data");
    Require(!replacement.IsExpired(replacement.startTime + 60000u),
        "Ref persistent compass type was incorrectly expired");

    FeedCompassReplayPacket(mode, CompassFrame(0, 2, 1, 1, 7, 0));
    Require(mode.m_compassPosList.empty(), "type 2 did not remove the compass marker by id");

    FeedCompassReplayPacket(mode, CompassFrame(0, 1, 20, 30, 9, 0x00112233u));
    Require(mode.m_compassPosList.size() == 1,
        "nonzero persistent marker was not retained");
    const PacketView malformed{0x0144, 22, timed.data()};
    const bool malformedDispatched = GetProductionGameModePacketRouter().Dispatch(mode, malformed);
    Require(malformedDispatched && mode.m_compassPosList.size() == 1
        && mode.m_compassPosList.at(9).x == 20,
        "malformed 0x0144 changed compass state");
    mode.m_compassPosList.at(9).type = 0;
    mode.m_compassPosList.at(9).startTime = 1000;
    mode.PruneCompassMarkers(16001u);
    Require(mode.m_compassPosList.empty(), "PruneCompassMarkers retained an expired timed marker");

    mode.m_waitingUseItemAck = 1;
    g_windowMgr.EnsureChatWindowVisible();
    auto* chat = g_windowMgr.m_chatWnd;
    const size_t normalBefore = chat->GetSourceHistory(false).size();
    const size_t commandBefore = chat->GetSourceHistory(true).size();
    const size_t noticesBefore = g_windowMgr.GetChatEvents().size();
    FeedCompassReplayPacket(mode, {0x89, 0x01, 0xff, 0});
    Require(g_windowMgr.GetChatEvents().size() == noticesBefore && mode.m_waitingUseItemAck == 1,
        "unknown 0189 type created a notice or cleared item acknowledgement");
    const std::array<const char*, 4> messages = {
        "Teleport is not allowed in this area.", "Saved point cannot be memorized.",
        "Skills cannot be used in this area.", "Items cannot be used in this area.",
    };
    for (u8 type = 0; type < messages.size(); ++type) {
        FeedCompassReplayPacket(mode, {0x89, 0x01, type, 0});
        const auto& notice = g_windowMgr.GetChatEvents().back();
        const u32 color = type == 0 ? 0x00ff0000u : 0x0000ffffu;
        Require(notice.text == messages[type] && notice.color == color
            && mode.m_waitingUseItemAck == (type == 3 ? 0 : 1),
            "0189 did not preserve the selected type text, color or item acknowledgement");
        const auto& normal = chat->GetSourceHistory(false);
        const auto& command = chat->GetSourceHistory(true);
        Require(normal.size() == normalBefore + type + 1 && command.size() == commandBefore + type + 1
            && normal.back().text == messages[type] && command.back().text == messages[type]
            && normal.back().color == color && command.back().color == color
            && g_windowMgr.GetChatEvents().size() == noticesBefore + type + 1,
            "0189 did not append once to both source histories and once to unified chat");
    }

    std::cout << "{\"case_id\":\"compass_marker_0144\",\"contract_id\":\"PK25.03\",\"fixture_id\":\"PK25-03-selected-20080910-0144-23-byte\",\"input_frame_hex\":\"" << Hex(timed)
        << "\",\"packet_size\":23,\"field_offsets\":\"npcId=2,type=6,x=10,y=14,id=18,color=19\",\"duplicate_id_replaced\":true,\"remove_by_id\":true,\"expiry_strict_gt_15000\":true,\"blink_500ms\":true,\"malformed_size_atomic\":true,\"outcome\":\"PASS\"}" << std::endl;
}
