void MemoPacketReplay()
{
    SetCase("memo_result_011e", "production_memo_result", "PK26.02.011E",
        "ProcessGameModePacketLoop -> RecvPacket -> TryReadPacket -> production router -> HandleMemoResult");
    Require(ro::net::GetPacketSize(0x011E) == 3, "011E fixed framing length is not the selected three-byte layout");

    ro::UiMessageStrings messages;
    messages.Load();
    const struct {
        u8 result;
        unsigned int messageId;
        const char* fallback;
        u32 color;
    } cases[] = {
        {0, 217, "Saved location as a Memo Point for Warp Skill.", 0x00FFFFu},
        {1, 214, "Skill Level is not high enough.", 0xFF0000u},
        {2, 216, "You haven't learned Warp.", 0xFF0000u},
    };

    CGameMode mode;
    auto& router = GetProductionGameModePacketRouter();
    RegisterDefaultGameModePacketHandlers(router);
    TestConnection connection;
    g_windowMgr.ClearChatEvents();
    for (const auto& test : cases) {
        const std::vector<u8> frame = {0x1E, 0x01, test.result};
        Require(connection.QueueBytes(std::vector<u8>(frame.begin(), frame.begin() + 2)),
            "could not queue partial 011E header");
        const auto partial = ProcessGameModePacketLoop(mode, connection, router, 1);
        Require(partial.packetsProcessed == 0 && g_windowMgr.GetChatEvents().empty(),
            "partial 011E frame dispatched before its result byte arrived");
        Require(connection.QueueBytes(std::vector<u8>(frame.begin() + 2, frame.end())),
            "could not queue final 011E result byte");
        const auto complete = ProcessGameModePacketLoop(mode, connection, router, 1);
        Require(complete.packetsProcessed == 1 && complete.packetsDispatched == 1
            && complete.packetsUnhandled == 0, "complete 011E frame missed the production handler");

        const auto& events = g_windowMgr.GetChatEvents();
        const std::string expected = messages.Get(test.messageId, test.fallback);
        Require(events.size() == 1 && events.back().text == expected && events.back().color == test.color
            && events.back().channel == 6, "011E result did not preserve Ref message, color, and system chat route");
        std::cout << "{\"case_id\":\"memo_result_011e_" << static_cast<unsigned int>(test.result)
            << "\",\"expected_message\":\"" << JsonEscape(expected)
            << "\",\"actual_message\":\"" << JsonEscape(events.back().text)
            << "\",\"expected_color\":" << test.color << ",\"actual_color\":" << events.back().color
            << ",\"packets_processed\":" << complete.packetsProcessed
            << ",\"packets_dispatched\":" << complete.packetsDispatched << ",\"outcome\":\"PASS\"}\n";
        g_windowMgr.ClearChatEvents();
    }
}
