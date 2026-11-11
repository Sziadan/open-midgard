namespace {
void WriteAutoSpellU32(std::vector<u8>& packet, size_t offset, u32 value)
{
    packet[offset] = static_cast<u8>(value);
    packet[offset + 1] = static_cast<u8>(value >> 8);
    packet[offset + 2] = static_cast<u8>(value >> 16);
    packet[offset + 3] = static_cast<u8>(value >> 24);
}

void DispatchAutoSpellSnapshot(CGameModePacketRouter& router, CGameMode& mode, const std::vector<u8>& bytes)
{
    PacketView packet{};
    int consumed = 0;
    Require(TryReadPacket(bytes.data(), static_cast<int>(bytes.size()), packet, consumed),
        "production reader rejected the 30-byte 0x01CD snapshot");
    Require(consumed == 30 && packet.packetId == 0x01CD && packet.packetLength == 30,
        "production reader returned the wrong auto-spell boundary");
    Require(router.Dispatch(mode, packet), "production router did not dispatch the auto-spell list");
}

void ClickAutoSpellControl(UISkillSelectWnd& window, const UISkillSelectWnd::DisplayButton& button)
{
    const int x = button.x + button.width / 2;
    const int y = button.y + button.height / 2;
    window.OnLBtnDown(x, y);
    window.OnLBtnUp(x, y);
}

void ClickAutoSpellRow(UISkillSelectWnd& window, const UISkillSelectWnd::DisplayRow& row)
{
    const int x = row.x + row.width / 2;
    const int y = row.y + row.height / 2;
    window.OnLBtnDown(x, y);
    window.OnLBtnUp(x, y);
}
}

void AutoSpellChoiceProductionReplay()
{
    constexpr const char* fixtureId = "PK26-autospell-seven-u32-selected-server-snapshot";
    SetCase("autospell_choice_production_replay", "production_router_replay", fixtureId,
        "TryReadPacket -> production default router -> HandleAutoSpellList -> UISkillSelectWnd -> CGameMode::SendAutoSpellSelection -> CRagConnection::SendPacket");

    CMode* const previousMode = SkillPacketReplayAccess::Current();
    const int previousModeType = SkillPacketReplayAccess::CurrentType();
    CGameMode mode;
    SkillPacketReplayAccess::SetCurrent(&mode);
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();

    CGameModePacketRouter& router = GetProductionGameModePacketRouter();
    router.Clear();
    RegisterDefaultGameModePacketHandlers(router);

    std::vector<u8> snapshot(30, 0);
    snapshot[0] = 0xCD;
    snapshot[1] = 0x01;
    WriteAutoSpellU32(snapshot, 2, 5101);
    WriteAutoSpellU32(snapshot, 6, 0);
    WriteAutoSpellU32(snapshot, 10, 5102);
    WriteAutoSpellU32(snapshot, 14, 0x10000); // server parser truncates to u16; never offer it.
    WriteAutoSpellU32(snapshot, 26, 5107);
    DispatchAutoSpellSnapshot(router, mode, snapshot);

    Require(g_windowMgr.m_skillSelectWnd != nullptr, "nonzero snapshot did not open the selection window");
    UISkillSelectWnd* window = g_windowMgr.m_skillSelectWnd;
    UISkillSelectWnd::DisplayData display{};
    Require(window->GetDisplayDataForQt(&display), "auto-spell selector did not expose its rendered rows");
    Require(display.title == "Auto Spell" && display.rows.size() == 3,
        "selector did not filter the unrepresentable ID or show the sparse choices");
    Require(display.rows[0].itemId == 5101 && display.rows[1].itemId == 5102 && display.rows[2].itemId == 5107,
        "selector did not preserve the nonzero server slot order");

    // A repeated server snapshot replaces the current choices.
    std::fill(snapshot.begin() + 2, snapshot.end(), 0);
    WriteAutoSpellU32(snapshot, 18, 5205);
    DispatchAutoSpellSnapshot(router, mode, snapshot);
    Require(g_windowMgr.m_skillSelectWnd == window, "snapshot replacement changed the live selector instance");
    Require(window->GetDisplayDataForQt(&display) && display.rows.size() == 1 && display.rows[0].itemId == 5205,
        "repeated snapshot appended stale choices instead of replacing them");

    // Reopen a sparse selection for successful/failed sender validation.
    std::fill(snapshot.begin() + 2, snapshot.end(), 0);
    WriteAutoSpellU32(snapshot, 2, 5101);
    WriteAutoSpellU32(snapshot, 10, 5102);
    DispatchAutoSpellSnapshot(router, mode, snapshot);
    Require(window->GetDisplayDataForQt(&display) && display.rows.size() == 2,
        "selector did not replace to the latest two choices");

    // Selecting a listed ID while disconnected must preserve the chooser and selection.
    ClickAutoSpellRow(*window, display.rows[1]);
    Require(window->GetDisplayDataForQt(&display), "selector vanished before failed-send check");
    Require(!mode.SendAutoSpellSelection(7000, {5101, 5102}),
        "production sender accepted a skill ID absent from the offered snapshot");
    Require(!mode.SendAutoSpellSelection(0x10000, {0x10000}),
        "production sender accepted a skill ID that selected server truncates to u16");
    ClickAutoSpellControl(*window, display.okButton);
    Require(g_windowMgr.m_skillSelectWnd == window, "failed 0x01CE send closed the chooser");
    Require(window->GetDisplayDataForQt(&display) && display.rows[1].selected,
        "failed 0x01CE send discarded the user's selected row");

    LoopbackPeer peer(*connection);
    ClickAutoSpellControl(*window, display.okButton);
    const std::vector<u8> captured = peer.ReceiveExact(6);
    const std::vector<u8> expected = {0xCE, 0x01, 0xEE, 0x13, 0x00, 0x00}; // u32 5102
    Require(captured == expected, "production selection sender emitted an incorrect 0x01CE frame");
    Require(g_windowMgr.m_skillSelectWnd == nullptr, "successful 0x01CE send did not close the chooser");

    // Closing Auto Spell is local; the protocol has no cancel value in this profile.
    DispatchAutoSpellSnapshot(router, mode, snapshot);
    window = g_windowMgr.m_skillSelectWnd;
    Require(window != nullptr && window->GetDisplayDataForQt(&display), "cancel fixture did not reopen chooser");
    ClickAutoSpellControl(*window, display.cancelButton);
    Require(g_windowMgr.m_skillSelectWnd == nullptr, "local Auto Spell cancel did not close chooser");
    Require(SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(50)) == 0,
        "local Auto Spell cancel emitted a wire packet");

    // An empty snapshot closes an existing Auto Spell chooser without sending.
    DispatchAutoSpellSnapshot(router, mode, snapshot);
    Require(g_windowMgr.m_skillSelectWnd != nullptr, "empty-snapshot fixture did not reopen chooser");
    std::fill(snapshot.begin() + 2, snapshot.end(), 0);
    DispatchAutoSpellSnapshot(router, mode, snapshot);
    Require(g_windowMgr.m_skillSelectWnd == nullptr, "all-zero snapshot did not close the existing chooser");

    connection->Disconnect();
    SkillPacketReplayAccess::Restore(previousMode, previousModeType);
    router.Clear();
    RegisterDefaultGameModePacketHandlers(router);
    std::cout << "{\"case_id\":\"autospell_choice_production_replay\",\"contract_id\":\"PK26.01CD-01CE\",\"fixture_id\":\""
        << fixtureId << "\",\"production_entry\":\"TryReadPacket -> production default router -> HandleAutoSpellList -> UISkillSelectWnd -> CGameMode::SendAutoSpellSelection -> CRagConnection::SendPacket\",\"profile\":23,\"captured_packet_hex\":\""
        << Hex(captured) << "\",\"failed_send_preserved_selection\":true,\"cancel_sent_packet\":false,\"zero_snapshot_closed_window\":true,\"outcome\":\"PASS\"}\n";
}
