namespace {

constexpr u32 kDialogueNpcId = 0x11223344u;

std::vector<u8> NpcIdFrame(u16 opcode, size_t idOffset, u32 npcId, size_t length)
{
    std::vector<u8> frame(length, 0);
    frame[0] = static_cast<u8>(opcode);
    frame[1] = static_cast<u8>(opcode >> 8);
    for (size_t i = 0; i < 4; ++i) {
        frame[idOffset + i] = static_cast<u8>(npcId >> (i * 8));
    }
    return frame;
}

void FeedNpcDialoguePacket(CGameMode& mode, LoopbackPeer& peer, const std::vector<u8>& frame)
{
    peer.Send(frame);
    const auto deadline = Clock::now() + kTimeout;
    while (Clock::now() < deadline) {
        int pending = 0;
#if RO_PLATFORM_WINDOWS
        Require(ioctlsocket(PacketStreamReplayAccess::Native(*CRagConnection::instance()), FIONREAD,
            reinterpret_cast<u_long*>(&pending)) == 0, "NPC dialogue FIONREAD failed");
#else
        Require(ioctl(PacketStreamReplayAccess::Native(*CRagConnection::instance()), FIONREAD, &pending) == 0,
            "NPC dialogue FIONREAD failed");
#endif
        if (pending >= static_cast<int>(frame.size())) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    Require(Clock::now() < deadline, "NPC dialogue bytes did not reach the receive socket");
    CGameModePacketRouter& router = GetProductionGameModePacketRouter();
    const GameModePacketLoopResult result = ProcessGameModePacketLoop(mode, *CRagConnection::instance(), router, 1);
    Require(result.packetsProcessed == 1 && result.packetsDispatched == 1
        && result.packetsUnhandled == 0 && PacketStreamReplayAccess::BufferedBytes(*CRagConnection::instance()) == 0,
        "NPC dialogue fixture did not traverse the production receive/router path");
}

void RequireNpcRetryState(const CGameMode& mode, u32 npcId, const char* action)
{
    Require(mode.m_isOnQuest != 0 && mode.m_lastNaid == npcId,
        std::string(action) + " refusal cleared active NPC quest/AID state");
}

} // namespace

void NpcDialogueActionsReplay()
{
    SetCase("npc_dialogue_actions_retry", "PK25.03", "PK25-03-selected-20080910-dialogue-actions",
        "production receive/router -> NPC windows -> CModeMgr::SendMsg -> production packet send");
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    g_windowMgr.CloseNpcDialogWindows();
    CMode* const oldMode = SkillPacketReplayAccess::Current();
    const int oldModeType = SkillPacketReplayAccess::CurrentType();
    CGameMode mode;
    SkillPacketReplayAccess::SetCurrent(&mode);
    CGameModePacketRouter& router = GetProductionGameModePacketRouter();
    router.Clear();
    RegisterDefaultGameModePacketHandlers(router);

    std::vector<u8> menuFrame(8 + 11, 0);
    menuFrame[0] = 0xb7;
    menuFrame[4] = static_cast<u8>(kDialogueNpcId);
    menuFrame[5] = static_cast<u8>(kDialogueNpcId >> 8);
    menuFrame[6] = static_cast<u8>(kDialogueNpcId >> 16);
    menuFrame[7] = static_cast<u8>(kDialogueNpcId >> 24);
    std::memcpy(menuFrame.data() + 8, "Alpha:Beta", 11);
    menuFrame[2] = static_cast<u8>(menuFrame.size());
    menuFrame[3] = static_cast<u8>(menuFrame.size() >> 8);

    {
        LoopbackPeer peer(*connection);
        FeedNpcDialoguePacket(mode, peer, NpcIdFrame(0x00b5, 2, kDialogueNpcId, 6));
        UISayDialogWnd* const say = g_windowMgr.m_sayDialogWnd;
        Require(say && say->m_show && say->HasActionButton() && say->IsNextAction()
            && say->GetNpcId() == kDialogueNpcId, "production next packet did not activate the real say window");
        RequireNpcRetryState(mode, kDialogueNpcId, "initial next action");

        connection->Disconnect();
        Require(say->HandleKeyDown(VK_RETURN), "offline next Enter key was not consumed");
        Require(say->m_show && say->HasActionButton() && say->IsNextAction(),
            "failed next action hid the real dialogue window or discarded its action");
        RequireNpcRetryState(mode, kDialogueNpcId, "next");
    }
    {
        LoopbackPeer peer(*connection);
        UISayDialogWnd* const say = g_windowMgr.m_sayDialogWnd;
        Require(say && say->HandleKeyDown(VK_RETURN), "next action retry failed with an open connection");
        const std::vector<u8> captured = peer.ReceiveExact(6);
        Require(captured == std::vector<u8>({0xb9,0x00,0x44,0x33,0x22,0x11}),
            "next action retry emitted incorrect 0x00b9 bytes");
        Require(!say->HasActionButton() && say->m_show,
            "successful next did not retain the dialogue window while clearing its one-shot action");
        RequireNpcRetryState(mode, kDialogueNpcId, "successful next");

        FeedNpcDialoguePacket(mode, peer, NpcIdFrame(0x00b6, 2, kDialogueNpcId, 6));
        Require(say->m_show && say->HasActionButton() && !say->IsNextAction(),
            "production close packet did not activate the real close action");
        connection->Disconnect();
        Require(say->HandleKeyDown(VK_RETURN), "offline close Enter key was not consumed");
        Require(say->m_show && say->HasActionButton(),
            "failed close action hid the real dialogue window or discarded its action");
        RequireNpcRetryState(mode, kDialogueNpcId, "close");
    }
    {
        LoopbackPeer peer(*connection);
        UISayDialogWnd* const say = g_windowMgr.m_sayDialogWnd;
        Require(say && say->HandleKeyDown(VK_RETURN), "close action retry failed with an open connection");
        const std::vector<u8> captured = peer.ReceiveExact(6);
        Require(captured == std::vector<u8>({0x46,0x01,0x44,0x33,0x22,0x11}),
            "close action retry emitted incorrect 0x0146 bytes");
        Require(!say->m_show && mode.m_isOnQuest == 0 && mode.m_lastNaid == 0,
            "successful close did not finish and release the NPC interaction");

        FeedNpcDialoguePacket(mode, peer, menuFrame);
        UINpcMenuWnd* const menu = g_windowMgr.m_npcMenuWnd;
        Require(menu && menu->m_show && menu->GetNpcId() == kDialogueNpcId
            && menu->GetOptions().size() == 2, "production menu packet did not activate the real menu window");
        RequireNpcRetryState(mode, kDialogueNpcId, "initial menu");
        connection->Disconnect();
        Require(menu->HandleKeyDown(VK_ESCAPE), "menu cancel key was not handled");
        Require(menu->m_show && menu->GetNpcId() == kDialogueNpcId,
            "failed menu cancel hid the real menu or discarded its NPC context");
        RequireNpcRetryState(mode, kDialogueNpcId, "menu cancel");
    }
    {
        LoopbackPeer peer(*connection);
        UINpcMenuWnd* const menu = g_windowMgr.m_npcMenuWnd;
        Require(menu && menu->HandleKeyDown(VK_ESCAPE), "menu cancel retry key was not handled");
        const std::vector<u8> captured = peer.ReceiveExact(7);
        Require(captured == std::vector<u8>({0xb8,0x00,0x44,0x33,0x22,0x11,0xff}),
            "successful menu cancel did not send choice 255 on 0x00b8");
        Require(!menu->m_show && mode.m_isOnQuest == 0 && mode.m_lastNaid == 0,
            "successful menu cancel did not close the menu and release NPC state");

        std::vector<u8> sparseMenuFrame(8 + 12, 0);
        sparseMenuFrame[0] = 0xb7;
        sparseMenuFrame[2] = static_cast<u8>(sparseMenuFrame.size());
        sparseMenuFrame[3] = static_cast<u8>(sparseMenuFrame.size() >> 8);
        sparseMenuFrame[4] = static_cast<u8>(kDialogueNpcId);
        sparseMenuFrame[5] = static_cast<u8>(kDialogueNpcId >> 8);
        sparseMenuFrame[6] = static_cast<u8>(kDialogueNpcId >> 16);
        sparseMenuFrame[7] = static_cast<u8>(kDialogueNpcId >> 24);
        std::memcpy(sparseMenuFrame.data() + 8, "Alpha::Beta", 12);
        FeedNpcDialoguePacket(mode, peer, sparseMenuFrame);
        UINpcMenuWnd* const sparseMenu = g_windowMgr.m_npcMenuWnd;
        Require(sparseMenu && sparseMenu->GetOptions().size() == 2,
            "sparse menu did not preserve two visible choices across an empty source slot");
        sparseMenu->HandleKeyDown(VK_DOWN);
        sparseMenu->HandleKeyDown(VK_RETURN);
        const std::vector<u8> sparseChoice = peer.ReceiveExact(7);
        Require(sparseChoice == std::vector<u8>({0xb8,0x00,0x44,0x33,0x22,0x11,0x03}),
            "visible second sparse-menu choice did not preserve source ordinal 3 on the wire");
        Require(!sparseMenu->m_show && mode.m_isOnQuest != 0 && mode.m_lastNaid == kDialogueNpcId,
            "successful sparse-menu choice did not close only the menu while retaining the interaction");

        FeedNpcDialoguePacket(mode, peer, NpcIdFrame(0x0142, 2, kDialogueNpcId, 6));
        UINpcInputWnd* const numericInput = g_windowMgr.m_npcInputWnd;
        Require(numericInput && numericInput->m_show && numericInput->GetNpcId() == kDialogueNpcId
            && numericInput->GetInputMode() == UINpcInputWnd::InputMode::Number,
            "production 0x0142 packet did not activate the real numeric input window");
        numericInput->GetEditCtrl()->SetText("0");
        connection->Disconnect();
        Require(numericInput->HandleKeyDown(VK_RETURN), "offline numeric-input Enter key was not consumed");
        Require(numericInput->m_show && numericInput->GetNpcId() == kDialogueNpcId
            && std::string(numericInput->GetInputText()) == "0",
            "failed numeric input did not retain the typed zero and NPC prompt");
        RequireNpcRetryState(mode, kDialogueNpcId, "numeric input");
    }
    {
        LoopbackPeer peer(*connection);
        UINpcInputWnd* const numericInput = g_windowMgr.m_npcInputWnd;
        Require(numericInput && numericInput->HandleKeyDown(VK_RETURN),
            "numeric zero retry key was not handled");
        const std::vector<u8> captured = peer.ReceiveExact(10);
        Require(captured == std::vector<u8>({0x43,0x01,0x44,0x33,0x22,0x11,0,0,0,0}),
            "numeric zero retry did not emit the exact 0x0143 frame");
        Require(!numericInput->m_show && mode.m_isOnQuest != 0 && mode.m_lastNaid == kDialogueNpcId,
            "successful numeric input did not hide the prompt while retaining the NPC interaction");

        FeedNpcDialoguePacket(mode, peer, NpcIdFrame(0x01d4, 2, kDialogueNpcId, 6));
        UINpcInputWnd* const input = g_windowMgr.m_npcInputWnd;
        Require(input && input->m_show && input->GetNpcId() == kDialogueNpcId
            && input->GetInputMode() == UINpcInputWnd::InputMode::String,
            "production string-input packet did not activate the real input window");
        input->GetEditCtrl()->SetText("Hi");
        RequireNpcRetryState(mode, kDialogueNpcId, "initial string input");
        connection->Disconnect();
        Require(input->HandleKeyDown(VK_RETURN), "string-input return key was not handled");
        Require(input->m_show && input->GetNpcId() == kDialogueNpcId
            && std::string(input->GetInputText()) == "Hi",
            "failed string-input send hid the input or discarded its text/NPC context");
        RequireNpcRetryState(mode, kDialogueNpcId, "string input");
    }
    {
        LoopbackPeer peer(*connection);
        UINpcInputWnd* const input = g_windowMgr.m_npcInputWnd;
        Require(input && input->HandleKeyDown(VK_RETURN), "string-input retry key was not handled");
        const std::vector<u8> captured = peer.ReceiveExact(11);
        Require(captured == std::vector<u8>({0xd5,0x01,0x0b,0x00,0x44,0x33,0x22,0x11,0x48,0x69,0x00}),
            "successful NPC string retry emitted incorrect 0x01d5 bytes");
        Require(!input->m_show && mode.m_isOnQuest != 0 && mode.m_lastNaid == kDialogueNpcId,
            "successful NPC string submission incorrectly released the ongoing interaction");

        FeedNpcDialoguePacket(mode, peer, NpcIdFrame(0x01d4, 2, kDialogueNpcId, 6));
        input->GetEditCtrl()->SetText(std::string(71, 'A').c_str());
        Require(input->HandleKeyDown(VK_RETURN), "oversized string-input return key was not handled");
        Require(input->m_show && input->GetNpcId() == kDialogueNpcId
            && std::string(input->GetInputText()) == std::string(71, 'A'),
            "71-byte NPC string did not preserve the active prompt and input");
        Require(SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(50)) == 0,
            "rejected 71-byte NPC string emitted a packet");
        input->GetEditCtrl()->SetText(std::string(70, 'A').c_str());
        Require(input->HandleKeyDown(VK_RETURN), "70-byte string submission key was not handled");
        std::vector<u8> expectedString{0xd5,0x01,0x4f,0x00,0x44,0x33,0x22,0x11};
        expectedString.insert(expectedString.end(), 70, static_cast<u8>('A'));
        expectedString.push_back(0);
        const std::vector<u8> sizeBoundaryCaptured = peer.ReceiveExact(expectedString.size());
        Require(sizeBoundaryCaptured == expectedString,
            "70-byte NPC string did not emit the exact selected variable frame");
        Require(!input->m_show && mode.m_isOnQuest != 0 && mode.m_lastNaid == kDialogueNpcId,
            "accepted 70-byte NPC string closed the interaction incorrectly");

        std::vector<u8> textFrame(8 + 12, 0);
        textFrame[0] = 0xb4;
        textFrame[2] = static_cast<u8>(textFrame.size());
        textFrame[3] = static_cast<u8>(textFrame.size() >> 8);
        textFrame[4] = static_cast<u8>(kDialogueNpcId);
        textFrame[5] = static_cast<u8>(kDialogueNpcId >> 8);
        textFrame[6] = static_cast<u8>(kDialogueNpcId >> 16);
        textFrame[7] = static_cast<u8>(kDialogueNpcId >> 24);
        std::memcpy(textFrame.data() + 8, "Keep this", 10);
        FeedNpcDialoguePacket(mode, peer, textFrame);
        UISayDialogWnd* const say = g_windowMgr.m_sayDialogWnd;
        Require(say && say->m_show && say->GetDisplayText().find("Keep this") != std::string::npos,
            "NPC say text fixture did not reach the real say window");

        auto* oldShop = static_cast<UIItemShopWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_ITEMSHOPWND));
        Require(oldShop && oldShop->m_show, "could not stage the prior NPC shop control");
        FeedNpcDialoguePacket(mode, peer, NpcIdFrame(0x00c4, 2, kDialogueNpcId, 6));
        UIChooseSellBuyWnd* const shopChooser = g_windowMgr.m_chooseSellBuyWnd;
        Require(shopChooser && shopChooser->m_show && oldShop->m_show == 0,
            "inbound 0x00c4 did not replace old shop controls with the deal chooser");
        Require(say->m_show && say->GetDisplayText().find("Keep this") != std::string::npos,
            "opening the deal chooser discarded the active say dialog/text");
        shopChooser->HandleKeyDown(VK_ESCAPE);
        Require(shopChooser->m_show == 0 && oldShop->m_show == 0 && say->m_show == 0,
            "local shop cancel did not close both shop and NPC dialogue windows");
        Require(SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(50)) == 0,
            "local shop cancel emitted a script-menu cancel or close packet");
        g_session.ClearNpcShopState();
    }

    connection->Disconnect();
    g_windowMgr.CloseNpcDialogWindows();
    router.Clear();
    RegisterDefaultGameModePacketHandlers(router);
    SkillPacketReplayAccess::Restore(oldMode, oldModeType);
    std::cout << "{\"case_id\":\"npc_dialogue_actions_retry\",\"contract_id\":\"PK25.03\",\"fixture_id\":\"PK25-03-selected-20080910-dialogue-actions\",\"receive_path\":\"production packet loop/router; actual say/menu/input/shop windows\",\"failed_actions_retain_interaction\":true,\"next_wire_hex\":\"b90044332211\",\"close_wire_hex\":\"460144332211\",\"menu_cancel_wire_hex\":\"b80044332211ff\",\"sparse_menu_choice3_wire_hex\":\"b8004433221103\",\"numeric_zero_wire_hex\":\"43014433221100000000\",\"string_wire_hex\":\"d5010b0044332211486900\",\"string_max_70_accept_71_reject\":true,\"shop_cancel_local_only_say_text_retained_on_handoff\":true,\"outcome\":\"PASS\"}" << std::endl;
}
