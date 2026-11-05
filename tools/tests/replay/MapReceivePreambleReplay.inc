namespace {

std::string TakePendingMapReplayStatus()
{
    std::string status;
    AdoptPendingLoginStatus(status);
    return status;
}

void ResetMapReplayGlobals(int pendingReturnToCharSelect)
{
    CRagConnection::instance()->Disconnect();
    ClearPendingDisconnectAction();
    SetPendingLoginStatus("");
    g_session.m_pendingReturnToCharSelect = pendingReturnToCharSelect;
}

void RequireMapSwitch(CGameMode& mode, TestConnection& connection,
    CGameModePacketRouter& router, const char* expectedStatus,
    PendingDisconnectAction pendingAction, const char* caseId, u32 nowTick = 2,
    int expectedReturnIntent = 0)
{
    const int previousPendingReturn = g_session.m_pendingReturnToCharSelect;
    ResetMapReplayGlobals(1);
    const u64 before = g_modeMgr.GetOperationGeneration();

    ClearPendingDisconnectAction();
    if (pendingAction != PendingDisconnectAction::None) {
        Require(BeginPendingDisconnectAction(pendingAction), "could not seed pending map disconnect action");
        MarkPendingDisconnectActionSent(1);
    }

    const bool canContinue = mode.ProcessNetworkUpdate(connection, router, 64, nowTick);
    const ModeOperationKind operation = g_modeMgr.GetLastOperation();
    const u64 after = g_modeMgr.GetOperationGeneration();
    const PendingDisconnectAction pendingAfter = GetPendingDisconnectAction();
    const int returnIntentAfter = g_session.m_pendingReturnToCharSelect;
    const std::string status = TakePendingMapReplayStatus();

    Require(!canContinue, std::string(caseId) + " did not stop the map frame");
    Require(after == before + 1, std::string(caseId) + " did not request exactly one mode operation");
    Require(operation == ModeOperationKind::Switch, std::string(caseId) + " did not request a Login mode switch");
    Require(g_modeMgr.GetRequestedModeType() == 0, std::string(caseId) + " requested a non-Login mode");
    Require(pendingAfter == PendingDisconnectAction::None, std::string(caseId) + " retained pending disconnect intent");
    Require(returnIntentAfter == expectedReturnIntent, std::string(caseId) + " retained the wrong return-to-character intent");
    Require(status == expectedStatus, std::string(caseId) + " retained the wrong Login status: " + status);

    const u32 worldTick = mode.m_worldProcessTick;
    const float worldCarry = mode.m_worldProcessCarryMs;
    mode.OnUpdate();
    Require(mode.m_worldProcessTick == worldTick && mode.m_worldProcessCarryMs == worldCarry,
        std::string(caseId) + " repeated OnUpdate advanced world processing after terminal receive");
    Require(g_modeMgr.GetOperationGeneration() == after, std::string(caseId) + " repeated update requested another mode operation");
    Require(TakePendingMapReplayStatus().empty(), std::string(caseId) + " repeated update overwrote the one-shot Login status");

    g_session.m_pendingReturnToCharSelect = previousPendingReturn;
    ClearPendingDisconnectAction();
    SetPendingLoginStatus("");
}

} // namespace

void MapReceivePreambleReplay()
{
    SetCase("map_receive_preamble", "PK05.05-map-disconnect", "PK05.05-terminal-map-receive", "CGameMode::ProcessNetworkUpdate -> ProcessGameModePacketLoop -> CRagConnection::RecvPacket");
    const int savedPendingReturn = g_session.m_pendingReturnToCharSelect;
    CRagConnection* const singleton = CRagConnection::instance();
    singleton->Disconnect();
    ClearPendingDisconnectAction();
    SetPendingLoginStatus("");

    CGameModePacketRouter& router = GetProductionGameModePacketRouter();
    router.Clear();
    RegisterDefaultGameModePacketHandlers(router);
    int ordinaryDispatches = 0;
    router.Register(0x006c, [&ordinaryDispatches](CGameMode&, const PacketView&) { ++ordinaryDispatches; });

    // Open/no-data and an ordinary valid packet allow the map frame to continue.
    {
        TestConnection connection;
        LoopbackPeer peer(connection);
        CGameMode mode;
        const u64 before = g_modeMgr.GetOperationGeneration();
        Require(mode.ProcessNetworkUpdate(connection, router, 64, 10), "open/no-data stopped map update");
        Require(connection.IsOpen(), "open/no-data closed the map connection");
        Require(g_modeMgr.GetOperationGeneration() == before, "open/no-data requested a mode operation");

        peer.Send({0x6c, 0x00, 0x00});
        WaitKernelPending(connection, 3);
        Require(mode.ProcessNetworkUpdate(connection, router, 64, 11), "valid 0x006C packet stopped map update");
        Require(ordinaryDispatches == 1, "valid fixed frame did not reach the router exactly once");
        Require(g_modeMgr.GetOperationGeneration() == before, "valid 0x006C packet requested a mode operation");
    }

    // A fragmented fatal header closes framing before a plausible suffix can dispatch.
    {
        TestConnection connection;
        LoopbackPeer peer(connection);
        CGameMode mode;
        const u64 before = g_modeMgr.GetOperationGeneration();
        peer.Send({0xff});
        WaitKernelPending(connection, 1);
        Require(mode.ProcessNetworkUpdate(connection, router, 64, 20), "partial unknown header was treated as terminal");
        peer.Send({0xff, 0x6c, 0x00, 0x00});
        WaitKernelPending(connection, 4);
        RequireMapSwitch(mode, connection, router,
            "Login error: server sent an unknown packet; connection closed.",
            PendingDisconnectAction::None, "fragmented unknown opcode");
        Require(connection.GetLastReceiveError() == CRagConnection::ReceiveError::UnknownOpcode,
            "fragmented unknown header did not preserve UnknownOpcode");
        Require(ordinaryDispatches == 1, "plausible suffix dispatched after a fatal unknown header");
        Require(g_modeMgr.GetOperationGeneration() >= before + 1, "fatal framing did not advance operation generation");
    }

    // Invalid variable length follows the matching durable framing status.
    for (PendingDisconnectAction action : {PendingDisconnectAction::None, PendingDisconnectAction::ReturnToCharSelect, PendingDisconnectAction::ExitToWindows}) {
        TestConnection connection;
        LoopbackPeer peer(connection);
        CGameMode mode;
        peer.Send({0x69, 0x00, 0x03, 0x00});
        WaitKernelPending(connection, 4);
        RequireMapSwitch(mode, connection, router,
            "Login error: server sent a malformed variable-length packet; connection closed.",
            action, "invalid variable length");
        Require(connection.GetLastReceiveError() == CRagConnection::ReceiveError::InvalidVariableLength,
            "invalid variable header did not preserve InvalidVariableLength");
    }

    // EOF is recovery to Login regardless of an unacknowledged return or exit request.
    for (PendingDisconnectAction action : {PendingDisconnectAction::None, PendingDisconnectAction::ReturnToCharSelect, PendingDisconnectAction::ExitToWindows}) {
        TestConnection connection;
        LoopbackPeer peer(connection);
        CGameMode mode;
        CloseSocket(peer.peer.value);
        peer.peer.value = INVALID_SOCKET;
        WaitReadable(connection, Clock::now() + kTimeout);
        RequireMapSwitch(mode, connection, router,
            "Login: connection to map server lost.", action, "ordinary EOF");
        Require(!connection.IsOpen(), "ordinary EOF left the map connection open");
    }

    // A matching return acknowledgement followed by EOF is an accepted handoff; stop on Switch.
    {
        TestConnection connection;
        LoopbackPeer peer(connection);
        CGameMode mode;
        peer.Send({0xb3, 0x00, 0x01});
        WaitKernelPending(connection, 3);
        CloseSocket(peer.peer.value);
        peer.peer.value = INVALID_SOCKET;
        Require(BeginPendingDisconnectAction(PendingDisconnectAction::ReturnToCharSelect), "could not seed 0x00B3 return request");
        const u64 before = g_modeMgr.GetOperationGeneration();
        GameModePacketLoopResult loop{};
        Require(!mode.ProcessNetworkUpdate(connection, router, 64, 30, &loop), "accepted 0x00B3 did not stop map update");
        Require(loop.modeOperationRequested && loop.packetsDispatched == 1, "0x00B3 was not recognized as the mode operation that stopped the batch");
        Require(!loop.receiveDrained, "accepted return probed EOF after its mode operation");
        Require(g_modeMgr.GetOperationGeneration() == before + 1 && g_modeMgr.GetLastOperation() == ModeOperationKind::Switch,
            "accepted 0x00B3 did not preserve the Login handoff before EOF");
        Require(g_session.m_pendingReturnToCharSelect == 1, "accepted 0x00B3 lost its return-to-character intent");
        Require(TakePendingMapReplayStatus().empty(), "accepted 0x00B3 acquired a terminal error status");
        Require(!mode.ProcessNetworkUpdate(connection, router, 64, 30000)
            && g_modeMgr.GetOperationGeneration() == before + 1,
            "accepted return was overwritten by a later update/timeout");
    }

    // A matching Windows-exit acknowledgement followed by EOF retains Quit instead of switching to Login.
    {
        TestConnection connection;
        LoopbackPeer peer(connection);
        CGameMode mode;
        peer.Send({0x8b, 0x01, 0x00, 0x00});
        WaitKernelPending(connection, 4);
        CloseSocket(peer.peer.value);
        peer.peer.value = INVALID_SOCKET;
        Require(BeginPendingDisconnectAction(PendingDisconnectAction::ExitToWindows), "could not seed 0x018B exit request");
        const u64 before = g_modeMgr.GetOperationGeneration();
        GameModePacketLoopResult loop{};
        Require(!mode.ProcessNetworkUpdate(connection, router, 64, 31, &loop), "accepted 0x018B exit did not stop map update");
        Require(loop.modeOperationRequested && loop.packetsDispatched == 1, "0x018B exit was not recognized as a mode operation");
        Require(!loop.receiveDrained, "accepted exit probed EOF after Quit");
        Require(g_modeMgr.GetOperationGeneration() == before + 1 && g_modeMgr.GetLastOperation() == ModeOperationKind::Quit,
            "accepted 0x018B exit was overwritten by EOF recovery");
        Require(GetPendingDisconnectAction() == PendingDisconnectAction::None, "accepted 0x018B exit retained pending state");
        Require(TakePendingMapReplayStatus().empty(), "accepted 0x018B exit acquired an EOF status");
        Require(!mode.ProcessNetworkUpdate(connection, router, 64, 30000)
            && g_modeMgr.GetOperationGeneration() == before + 1,
            "accepted Quit was overwritten by a later update/timeout");
    }

    // A refusal is a valid open-connection response and must not become a terminal map handoff.
    {
        TestConnection connection;
        LoopbackPeer peer(connection);
        CGameMode mode;
        Require(BeginPendingDisconnectAction(PendingDisconnectAction::ExitToWindows), "could not seed refusal request");
        peer.Send({0x8b, 0x01, 0x01, 0x00});
        WaitKernelPending(connection, 4);
        const u64 before = g_modeMgr.GetOperationGeneration();
        Require(mode.ProcessNetworkUpdate(connection, router, 64, 40), "valid refusal stopped map update");
        Require(connection.IsOpen(), "valid refusal closed the map connection");
        Require(g_modeMgr.GetOperationGeneration() == before, "valid refusal requested a mode operation");
        Require(GetPendingDisconnectAction() == PendingDisconnectAction::None, "valid refusal did not clear the matching pending action");
        Require(!g_windowMgr.GetChatEvents().empty()
            && g_windowMgr.GetChatEvents().back().text == "The server refused the disconnect request.",
            "valid refusal did not report the server response");
        ClearPendingDisconnectAction();
        SetPendingLoginStatus("");
    }

    // Expired request fallback applies only after a genuinely drained receive batch.
    {
        TestConnection connection;
        LoopbackPeer peer(connection);
        CGameMode mode;
        Require(BeginPendingDisconnectAction(PendingDisconnectAction::ReturnToCharSelect), "could not seed timeout request");
        MarkPendingDisconnectActionSent(100);
        const u64 before = g_modeMgr.GetOperationGeneration();
        Require(mode.ProcessNetworkUpdate(connection, router, 64, 15099), "unexpired request was timed out");
        Require(g_modeMgr.GetOperationGeneration() == before, "unexpired request requested a mode operation");
        Require(GetPendingDisconnectAction() == PendingDisconnectAction::ReturnToCharSelect, "unexpired request was cleared");
        RequireMapSwitch(mode, connection, router, "Login: disconnect timed out.",
            PendingDisconnectAction::ReturnToCharSelect, "expired drained timeout", 15100, 1);
    }
    {
        TestConnection connection;
        LoopbackPeer peer(connection);
        CGameMode mode;
        CGameModePacketRouter budgetRouter;
        int dispatchCount = 0;
        budgetRouter.Register(0x006c, [&dispatchCount](CGameMode&, const PacketView&) { ++dispatchCount; });
        Require(BeginPendingDisconnectAction(PendingDisconnectAction::ExitToWindows), "could not seed budget timeout request");
        MarkPendingDisconnectActionSent(100);
        peer.Send({0x6c, 0x00, 0x00, 0x6c, 0x00, 0x00});
        WaitKernelPending(connection, 6);
        const u64 before = g_modeMgr.GetOperationGeneration();
        GameModePacketLoopResult loop{};
        Require(mode.ProcessNetworkUpdate(connection, budgetRouter, 1, 15100, &loop), "packet-budget exhaustion stopped map update");
        Require(dispatchCount == 1 && !loop.receiveDrained, "packet-budget fixture did not exhaust on one valid packet");
        Require(g_modeMgr.GetOperationGeneration() == before, "packet-budget exhaustion applied timeout fallback");
        Require(GetPendingDisconnectAction() == PendingDisconnectAction::ExitToWindows, "packet-budget exhaustion cleared the pending exit");
        ClearPendingDisconnectAction();
    }

    // A handler-requested Switch stops the batch before its buffered suffix dispatches.
    {
        TestConnection connection;
        LoopbackPeer peer(connection);
        CGameMode mode;
        CGameModePacketRouter operationRouter;
        int dispatchCount = 0;
        operationRouter.Register(0x006c, [&dispatchCount](CGameMode&, const PacketView&) {
            ++dispatchCount;
            g_modeMgr.Switch(1, "");
        });
        peer.Send({0x6c, 0x00, 0x00, 0x6c, 0x00, 0x00});
        WaitKernelPending(connection, 6);
        const u64 before = g_modeMgr.GetOperationGeneration();
        GameModePacketLoopResult loop{};
        Require(!mode.ProcessNetworkUpdate(connection, operationRouter, 64, 50, &loop), "handler Switch did not stop map update");
        Require(loop.modeOperationRequested && loop.packetsProcessed == 1 && dispatchCount == 1,
            "handler Switch did not stop before suffix dispatch");
        Require(connection.IsOpen() && connection.BufferedBytes() == 3,
            "handler Switch incorrectly required a closed connection or consumed the suffix");
        Require(g_modeMgr.GetOperationGeneration() == before + 1 && g_modeMgr.GetLastOperation() == ModeOperationKind::Switch,
            "handler Switch operation was not observable");
    }

    // A handoff requested outside packet dispatch also wins over apparent EOF.
    {
        TestConnection connection;
        CGameMode mode;
        g_modeMgr.Switch(0, "");
        SetPendingLoginStatus("Preserve earlier terminal operation.");
        const u64 generation = g_modeMgr.GetOperationGeneration();
        Require(!mode.ProcessNetworkUpdate(connection, router, 64, 55), "pre-existing handoff did not stop map update");
        Require(g_modeMgr.GetOperationGeneration() == generation, "pre-existing handoff triggered a second fallback");
        Require(TakePendingMapReplayStatus() == "Preserve earlier terminal operation.", "earlier terminal status was overwritten");
    }

    // Reconnect clears the framing error for a fresh mode, while the old terminal mode stays latched.
    {
        TestConnection connection;
        CGameMode oldMode;
        {
            LoopbackPeer peer(connection);
            peer.Send({0xff, 0xff, 0x6c, 0x00, 0x00});
            WaitKernelPending(connection, 5);
            Require(!oldMode.ProcessNetworkUpdate(connection, router, 64, 60), "unknown opcode did not terminally latch old mode");
            Require(connection.GetLastReceiveError() == CRagConnection::ReceiveError::UnknownOpcode,
                "first connection did not record framing error");
        }
        {
            LoopbackPeer peer(connection);
            Require(connection.GetLastReceiveError() == CRagConnection::ReceiveError::None,
                "reconnect did not clear the framing error");
            CGameMode freshMode;
            peer.Send({0x6c, 0x00, 0x00});
            WaitKernelPending(connection, 3);
            Require(freshMode.ProcessNetworkUpdate(connection, router, 64, 61), "fresh mode did not accept valid traffic after reconnect");
            Require(!oldMode.ProcessNetworkUpdate(connection, router, 64, 62), "old terminal mode lost its per-lifetime latch after reconnect");
        }
    }

    router.Clear();
    RegisterDefaultGameModePacketHandlers(router);
    ResetMapReplayGlobals(savedPendingReturn);
    std::cout << "{\"case_id\":\"map_receive_preamble\",\"contract_id\":\"PK05.05\",\"open_no_data\":true,\"valid_packet_continues\":true,\"fragmented_unknown_suffix_suppressed\":true,\"invalid_length_status\":true,\"ordinary_eof_pending_actions\":true,\"accepted_ack_eof_operations_preserved\":true,\"refusal_open\":true,\"timeout_requires_drained_receive\":true,\"repeated_terminal_update_latched\":true,\"outcome\":\"PASS\"}\n";
}
