namespace {
void PutSkillChoiceU16(std::vector<u8>& bytes, size_t offset, u16 value)
{
    bytes[offset] = static_cast<u8>(value);
    bytes[offset + 1] = static_cast<u8>(value >> 8);
}

void DispatchSkillChoiceFrame(CGameModePacketRouter& router, CGameMode& mode, const std::vector<u8>& bytes, u16 id)
{
    PacketView packet{};
    int consumed = 0;
    Require(TryReadPacket(bytes.data(), static_cast<int>(bytes.size()), packet, consumed), "skill choice frame was rejected by production reader");
    Require(consumed == static_cast<int>(bytes.size()) && packet.packetId == id, "skill choice frame boundary or opcode mismatch");
    Require(router.Dispatch(mode, packet), "production router did not dispatch skill choice offer");
}

void SkillChoiceClick(UISkillSelectWnd& window, const UISkillSelectWnd::DisplayButton& button)
{
    const int x = button.x + button.width / 2;
    const int y = button.y + button.height / 2;
    window.OnLBtnDown(x, y);
    window.OnLBtnUp(x, y);
}

void SkillChoiceClick(UISkillSelectWnd& window, const UISkillSelectWnd::DisplayRow& row)
{
    const int x = row.x + row.width / 2;
    const int y = row.y + row.height / 2;
    window.OnLBtnDown(x, y);
    window.OnLBtnUp(x, y);
}
}

void SkillChoiceLifecycleProductionReplay()
{
    constexpr const char* fixtureId = "PK26-warp-and-making-arrow-choice-lifecycle";
    SetCase("skill_choice_lifecycle_production_replay", "production_router_replay", fixtureId,
        "TryReadPacket -> production default router -> Warp/Arrow list handlers -> UISkillSelectWnd -> production selection senders");

    CMode* const previousMode = SkillPacketReplayAccess::Current();
    const int previousModeType = SkillPacketReplayAccess::CurrentType();
    CGameMode mode;
    SkillPacketReplayAccess::SetCurrent(&mode);
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    CGameModePacketRouter& router = GetProductionGameModePacketRouter();
    router.Clear();
    RegisterDefaultGameModePacketHandlers(router);

    std::vector<u8> warp(68, 0);
    warp[0] = 0x1C; warp[1] = 0x01;
    PutSkillChoiceU16(warp, 2, 27);
    std::memcpy(warp.data() + 4, "prontera", 8);
    std::memcpy(warp.data() + 20, "geffen", 6);
    DispatchSkillChoiceFrame(router, mode, warp, 0x011C);
    Require(g_windowMgr.m_skillSelectWnd != nullptr, "warp offer did not open selection window");
    UISkillSelectWnd* window = g_windowMgr.m_skillSelectWnd;
    UISkillSelectWnd::DisplayData display{};
    Require(window->GetDisplayDataForQt(&display) && display.title == "Warp Portal" && display.rows.size() == 2,
        "warp offer did not present both destinations");
    Require(!display.rows[0].label.empty() && !display.rows[1].label.empty(),
        "warp destinations were not labeled");
    Require(!mode.SendWarpMapSelection("payon"), "warp sender accepted a map absent from offer");
    SkillChoiceClick(*window, display.rows[1]);
    SkillChoiceClick(*window, display.okButton);
    Require(g_windowMgr.m_skillSelectWnd == window,
        "disconnected warp send did not preserve chooser");
    Require(window->GetDisplayDataForQt(&display) && display.rows[1].selected,
        "failed warp send discarded selected destination");
    LoopbackPeer peer(*connection);
    SkillChoiceClick(*window, display.okButton);
    const std::vector<u8> warpCaptured = peer.ReceiveExact(20);
    std::vector<u8> warpExpected(20, 0);
    warpExpected[0] = 0x1B; warpExpected[1] = 0x01; warpExpected[2] = 27;
    std::memcpy(warpExpected.data() + 4, "geffen", 6);
    Require(warpCaptured == warpExpected, "warp selection emitted incorrect 0x011B frame");
    Require(g_windowMgr.m_skillSelectWnd == nullptr, "successful warp selection did not close chooser");

    // Warp cancel is a protocol choice and must send the literal "cancel" map name.
    DispatchSkillChoiceFrame(router, mode, warp, 0x011C);
    window = g_windowMgr.m_skillSelectWnd;
    Require(window && window->GetDisplayDataForQt(&display), "warp cancel offer did not reopen");
    SkillChoiceClick(*window, display.cancelButton);
    const std::vector<u8> warpCancel = peer.ReceiveExact(20);
    std::vector<u8> expectedWarpCancel(20, 0);
    expectedWarpCancel[0] = 0x1B; expectedWarpCancel[1] = 0x01; expectedWarpCancel[2] = 27;
    std::memcpy(expectedWarpCancel.data() + 4, "cancel", 6);
    Require(warpCancel == expectedWarpCancel && g_windowMgr.m_skillSelectWnd == nullptr,
        "warp cancel did not send exact 0x011B cancel frame and close chooser");

    std::vector<u8> randomWarp(68, 0);
    randomWarp[0] = 0x1C; randomWarp[1] = 0x01; randomWarp[2] = 26;
    std::memcpy(randomWarp.data() + 4, "Random", 6);
    DispatchSkillChoiceFrame(router, mode, randomWarp, 0x011C);
    std::vector<u8> expectedRandom(20, 0);
    expectedRandom[0] = 0x1B; expectedRandom[1] = 0x01; expectedRandom[2] = 26;
    std::memcpy(expectedRandom.data() + 4, "Random", 6);
    Require(peer.ReceiveExact(20) == expectedRandom && g_windowMgr.m_skillSelectWnd == nullptr,
        "random-only teleport did not send automatically");

    std::vector<u8> arrows = {0xAD, 0x01, 0x0A, 0x00, 0x34, 0x12, 0, 0, 0x78, 0x56};
    DispatchSkillChoiceFrame(router, mode, warp, 0x011C);
    DispatchSkillChoiceFrame(router, mode, arrows, 0x01AD);
    Require(g_windowMgr.m_skillSelectWnd != nullptr, "making-arrow offer did not open selection window");
    window = g_windowMgr.m_skillSelectWnd;
    Require(window->GetDisplayDataForQt(&display) && display.title == "Create Arrow" && display.rows.size() == 2,
        "making-arrow offer did not filter zero ID or present the materials");
    Require(display.rows[0].itemId == 0x1234 && display.rows[1].itemId == 0x5678,
        "making-arrow items did not retain wire order");
    Require(!mode.SendWarpMapSelection("prontera") && mode.m_warpMapDestinations.empty(),
        "making-arrow offer left a stale warp selection");
    std::vector<u8> emptyWarp(68, 0);
    emptyWarp[0] = 0x1C; emptyWarp[1] = 0x01; emptyWarp[2] = 27;
    DispatchSkillChoiceFrame(router, mode, emptyWarp, 0x011C);
    Require(g_windowMgr.m_skillSelectWnd == window, "empty warp offer closed the making-arrow chooser");
    SkillChoiceClick(*window, display.rows[1]);
    SkillChoiceClick(*window, display.okButton);
    const std::vector<u8> arrowCaptured = peer.ReceiveExact(4);
    const std::vector<u8> expectedArrow = {0xAE, 0x01, 0x78, 0x56};
    Require(arrowCaptured == expectedArrow && g_windowMgr.m_skillSelectWnd == nullptr,
        "making-arrow selection did not send exact 0x01AE frame and close chooser");

    DispatchSkillChoiceFrame(router, mode, arrows, 0x01AD);
    window = g_windowMgr.m_skillSelectWnd;
    Require(window && window->GetDisplayDataForQt(&display), "making-arrow cancel offer did not reopen");
    SkillChoiceClick(*window, display.rows[1]);
    connection->Disconnect();
    SkillChoiceClick(*window, display.okButton);
    SkillChoiceClick(*window, display.cancelButton);
    Require(g_windowMgr.m_skillSelectWnd == window && window->GetDisplayDataForQt(&display) && display.rows[1].selected,
        "failed making-arrow submit or cancel did not preserve chooser selection");
    LoopbackPeer arrowPeer(*connection);
    SkillChoiceClick(*window, display.cancelButton);
    const std::vector<u8> arrowCancel = arrowPeer.ReceiveExact(4);
    const std::vector<u8> expectedArrowCancel = {0xAE, 0x01, 0, 0};
    Require(arrowCancel == expectedArrowCancel && g_windowMgr.m_skillSelectWnd == nullptr,
        "making-arrow cancel did not send exact zero-ID 0x01AE frame");

    connection->Disconnect();
    DispatchSkillChoiceFrame(router, mode, randomWarp, 0x011C);
    window = g_windowMgr.m_skillSelectWnd;
    Require(window && window->IsWarpSelection() && window->GetDisplayDataForQt(&display) && display.rows.size() == 1,
        "failed automatic teleport did not expose the destination for retry");
    DispatchSkillChoiceFrame(router, mode, emptyWarp, 0x011C);
    Require(g_windowMgr.m_skillSelectWnd == nullptr && mode.m_warpMapDestinations.empty(),
        "empty warp offer did not clear its own chooser and pending state");
    SkillPacketReplayAccess::Restore(previousMode, previousModeType);
    router.Clear();
    RegisterDefaultGameModePacketHandlers(router);
    std::cout << "{\"case_id\":\"skill_choice_lifecycle_production_replay\",\"contract_id\":\"PK26.011C-01AD\",\"fixture_id\":\""
        << fixtureId << "\",\"profile\":23,\"warp_selected_hex\":\"" << Hex(warpCaptured)
        << "\",\"warp_cancel_hex\":\"" << Hex(warpCancel) << "\",\"arrow_selected_hex\":\"" << Hex(arrowCaptured)
        << "\",\"arrow_cancel_hex\":\"" << Hex(arrowCancel)
        << "\",\"invalid_and_disconnected_sends_rejected\":true,\"failed_send_preserved_selection\":true,\"outcome\":\"PASS\"}\n";
}
