void MannerAndColoredNoticeReplay()
{
    SetCase("manner_and_colored_notice", "PK39.03", "selected-014a-014b-01ab-02c1", "map packet loop -> actor and chat");
    CGameMode mode;
    CWorld world;
    CPc actor;
    actor.m_gid = 0x11223344;
    world.RegisterActor(&actor);
    mode.m_runtimeActors.emplace(actor.m_gid, &actor);
    mode.m_world = &world;
    TestConnection connection;
    CGameModePacketRouter router;
    RegisterDefaultGameModePacketHandlers(router);
    const auto dispatch = [&](const std::vector<u8>& bytes, int count) {
        Require(connection.QueueBytes(bytes), "could not queue manner stream");
        Require(ProcessGameModePacketLoop(mode, connection, router, count).packetsDispatched == count
            && !connection.BufferedBytes(), "manner stream did not frame and dispatch");
    };
    g_windowMgr.ClearChatEvents();
    dispatch({0x4a,1,0,0,0,0, 0x4a,1,3,0,0,0, 0x4a,1,4,0,0,0, 0x4a,1,5,0,0,0,
        0xab,1,0x44,0x33,0x22,0x11,4,0,0xf9,0xff,0xff,0xff,
        0xab,1,0x44,0x33,0x22,0x11,3,0,99,0,0,0}, 6);
    Require(actor.m_honor == -7 && g_windowMgr.GetChatEvents().size() == 3,
        "manner result branches or signed actor state differ");
    std::vector<u8> giver(27, 'X');
    giver[0] = 0x4b; giver[1] = 1; giver[2] = 1;
    dispatch(giver, 1);
    giver[2] = 0;
    dispatch(giver, 1);
    const auto& chat = g_windowMgr.GetChatEvents();
    Require(chat.size() == 4 && chat.back().text.find(std::string(24, 'X')) != std::string::npos,
        "manner notification did not preserve the bounded giver name");
    const std::vector<u8> colored{0xc1,2,15,0,0x44,0x33,0x22,0x11,0x12,0x34,0x56,0,'h','i',0};
    const auto previousExclusions = g_session.m_exAidList;
    g_session.m_exAidList = {actor.m_gid};
    dispatch(colored, 1);
    Require(chat.size() == 4 && actor.GetChatBubbleText().empty(), "ignored colored speaker reached chat or bubble");
    g_session.m_exAidList.clear();
    dispatch(colored, 1);
    g_session.m_exAidList = previousExclusions;
    Require(chat.size() == 5 && chat.back().text == "hi", "colored chat text differs");
    Require(chat.back().color == 0x00123456, "colored chat RGB differs: " + std::to_string(chat.back().color));
    Require(actor.GetChatBubbleText() == "hi", "colored chat actor bubble differs");
    g_windowMgr.ClearChatEvents();
    mode.m_runtimeActors.clear();
    mode.m_world = nullptr;
    world.UnregisterActor(&actor);
}

void BattlegroundStateReplay()
{
    SetCase("battleground_state", "PK41.02", "selected-02dd-02e0", "map packets -> camps, chat, markers and actor HP");
    CGameMode mode;
    CWorld world;
    CPlayer local;
    CPc remote;
    CPc mercenary;
    const auto oldAid = g_session.m_aid;
    const auto oldMercenaryGid = g_session.m_merGID;
    g_session.m_aid = 0x10;
    g_session.m_merGID = mercenary.m_gid = 0x30;
    local.m_gid = 0x11;
    remote.m_gid = 0x20;
    world.m_player = &local;
    world.RegisterActor(&remote);
    world.RegisterActor(&mercenary);
    mode.m_world = &world;
    TestConnection input;
    auto& router = GetProductionGameModePacketRouter();
    const auto dispatch = [&](const std::vector<u8>& bytes, int count = 1) {
        Require(input.QueueBytes(bytes), "could not queue battleground packet");
        Require(ProcessGameModePacketLoop(mode, input, router, count).packetsDispatched == count
            && !input.BufferedBytes(), "battleground packets did not frame and dispatch");
    };
    g_windowMgr.ClearChatEvents();
    dispatch({0xde, 2, 0xfe, 0xff, 0x2c, 1});
    const auto& chat = g_windowMgr.GetChatEvents();
    Require(mode.m_battlegroundCamp == 0 && mode.m_battlegroundScoreA == -2
        && mode.m_battlegroundScoreB == 300 && chat.size() == 1
        && chat.back().text == "[ Camp A = -2 , Camp B = 300 ]" && chat.back().color == 0x00c8c8c8,
        "battleground score state or system notice differs");

    std::vector<u8> camp(32);
    camp[0] = 0xdd; camp[1] = 2; camp[2] = 0x10; camp[30] = 0x42; camp[31] = 1;
    auto campStream = camp;
    camp[2] = 0x20; camp[31] = 2;
    campStream.insert(campStream.end(), camp.begin(), camp.end());
    dispatch(campStream, 2);
    Require(mode.m_battlegroundCamp == 0x142 && local.m_camp == 0x142 && remote.m_camp == 0x242,
        "camp packets lost the upper byte or used character GID for the local AID");

    const int oldWidth = g_renderer.m_width, oldHeight = g_renderer.m_height;
    const float oldHpc = g_renderer.m_hpc, oldVpc = g_renderer.m_vpc;
    const float oldXOffset = g_renderer.m_xoffset, oldYOffset = g_renderer.m_yoffset;
    g_renderer.m_width = 640; g_renderer.m_height = 480;
    g_renderer.m_hpc = g_renderer.m_vpc = 400;
    g_renderer.m_xoffset = 320; g_renderer.m_yoffset = 240;
    g_renderer.DiscardRenderList();
    constexpr char markerKey[] =
        "@mk:\xc0\xaf\xc0\xfa\xc0\xce\xc5\xcd\xc6\xe4\xc0\xcc\xbd\xba\\"
        "\xc0\xfc\xc0\xe5\xc0\xfb\xc7\xa5\xbd\xc3.bmp";
    CTexture fixtureTexture(64, 64);
    fixtureTexture.m_updateWidth = fixtureTexture.m_surfaceUpdateWidth = 48;
    fixtureTexture.m_updateHeight = fixtureTexture.m_surfaceUpdateHeight = 48;
    fixtureTexture.m_upscaleFactor = 2;
    const auto cached = g_texMgr.m_texTable.emplace(markerKey, &fixtureTexture);
    Require(cached.second, "unexpected existing battleground marker texture");
    matrix view{};
    view.m[0][0] = view.m[1][1] = view.m[2][2] = view.m[3][3] = 1;
    view.m[3][2] = 100;
    remote.m_pos = {0, 0, 0};
    world.QueueBattlegroundCampMarkers(view, mode.m_battlegroundCamp);
    const auto& quads = g_renderer.m_rpAlphaNoDepthList;
    Require(quads.size() == 1 && quads.front().second->tex == &fixtureTexture,
        "opposing high-byte camp did not queue the original bitmap overlay");
    const auto* vertices = quads.front().second->verts;
    Require(vertices[0].x == 316.5f && vertices[0].y == 171.5f
        && vertices[3].x - vertices[0].x == 24 && vertices[3].y - vertices[0].y == 24
        && vertices[3].tu == 0.75f && vertices[3].tv == 0.75f,
        "camp marker did not use the source anchor, logical size and texture bounds");
    g_renderer.DiscardRenderList();
    for (const int hidden : {2, 4, 64}) {
        remote.m_effectState = hidden;
        world.QueueBattlegroundCampMarkers(view, mode.m_battlegroundCamp);
        Require(quads.empty(), "hidden opponent still showed a camp marker");
    }
    remote.m_effectState = 0;
    world.QueueBattlegroundCampMarkers(view, remote.m_camp);
    world.QueueBattlegroundCampMarkers(view, 0);
    Require(quads.empty(), "allied or unknown local camp showed a marker");
    world.UnregisterActor(&remote);
    world.QueueBattlegroundCampMarkers(view, mode.m_battlegroundCamp);
    Require(quads.empty(), "actor leaving sight retained a camp marker");
    world.RegisterActor(&remote);
    g_texMgr.m_texTable.erase(cached.first);
    g_renderer.m_width = oldWidth; g_renderer.m_height = oldHeight;
    g_renderer.m_hpc = oldHpc; g_renderer.m_vpc = oldVpc;
    g_renderer.m_xoffset = oldXOffset; g_renderer.m_yoffset = oldYOffset;

    std::vector<u8> position(36);
    position[0] = 0xdf; position[1] = 2; position[2] = 0x40;
    position[32] = 0x2c; position[33] = 1; position[34] = 45;
    dispatch(position);
    Require(mode.m_battlegroundPosList.count(0x40) == 1 && mode.m_battlegroundPosList.at(0x40).x == 300
        && mode.m_battlegroundPosList.at(0x40).y == 45 && mode.m_partyPosList.empty()
        && mode.m_guildPosList.empty(), "teammate marker depended on actor visibility or changed another marker list");
    position[2] = 0x10;
    dispatch(position);
    Require(mode.m_battlegroundPosList.count(0x10) == 0, "self position added a teammate marker");
    position[2] = 0x40; position[32] = 0xff; position[33] = 0xff;
    dispatch(position);
    Require(mode.m_battlegroundPosList.at(0x40).x == 300, "partial removal sentinel changed a marker");
    position[34] = 0xff; position[35] = 0xff;
    dispatch(position);
    Require(mode.m_battlegroundPosList.empty(), "removal sentinel retained the teammate marker");

    std::vector<u8> hp(34);
    hp[0] = 0xe0; hp[1] = 2; hp[2] = 0x20; hp[30] = 74; hp[32] = 100;
    dispatch(hp);
    Require(remote.m_Hp == 74 && remote.m_MaxHp == 100 && remote.m_gage,
        "scaled battleground HP did not reach the actor gauge");
    hp[30] = 0; hp[32] = 0xff; hp[33] = 0x7f;
    dispatch(hp);
    Require(remote.m_Hp == 0 && remote.m_MaxHp == 32767, "zero HP or unscaled maximum was lost");
    hp[30] = 74; hp[32] = 0; hp[33] = 0;
    dispatch(hp);
    Require(remote.m_Hp == 0 && remote.m_MaxHp == 32767, "invalid maximum changed the gauge");
    hp[2] = 0x30; hp[32] = 100;
    dispatch(hp);
    Require(!mercenary.m_gage, "battleground HP opened a mercenary gauge");
    remote.SendMsg(0, 35, 0, 0, 0);
    world.UnregisterActor(&remote);
    world.UnregisterActor(&mercenary);
    world.m_player = nullptr;
    mode.m_world = nullptr;
    g_session.m_aid = oldAid;
    g_session.m_merGID = oldMercenaryGid;
    g_windowMgr.ClearChatEvents();
}

void BattlegroundChatReplay()
{
    SetCase("battleground_chat", "PK41.03", "selected-02dc", "map packet -> notice, bubble and chat log");
    CGameMode mode;
    CWorld world;
    CPc speaker;
    speaker.m_gid = 100001;
    world.RegisterActor(&speaker);
    mode.m_runtimeActors.emplace(speaker.m_gid, &speaker);
    mode.m_world = &world;
    TestConnection input;
    auto& router = GetProductionGameModePacketRouter();
    const auto oldService = g_serviceType;
    const auto oldExclusions = g_session.m_exAidList;
    const auto oldAdmins = s_dwAdminAID;
    const auto oldSaveChat = g_session.m_isSaveChat;
    const int oldNoTalk = g_session.m_isNoTalkMsg, oldNoTalk2 = g_session.m_isNoTalkMsg2;
    g_serviceType = ServiceAmerica;
    g_session.m_exAidList.clear();
    g_session.m_isNoTalkMsg = g_session.m_isNoTalkMsg2 = 0;
    s_dwAdminAID = {g_session.m_aid};
    g_session.m_isSaveChat = false;
    Require(mode.ToggleSourceChatAutoSave(), "could not enable team chat autosave");
    const auto log = std::filesystem::u8path(mode.GetSourceAutoSaveChatPath());
    g_windowMgr.ClearChatEvents();
    const auto incoming = [&](u32 aid, const std::string& message, bool terminated = true) {
        const size_t length = 32 + message.size() + size_t(terminated);
        std::vector<u8> bytes(length, 0);
        bytes[0] = 0xdc; bytes[1] = 2; bytes[2] = u8(length); bytes[3] = u8(length >> 8);
        for (size_t i = 0; i < 4; ++i) bytes[4 + i] = u8(aid >> (8 * i));
        std::fill(bytes.begin() + 8, bytes.begin() + 32, 'X');
        std::copy(message.begin(), message.end(), bytes.begin() + 32);
        Require(input.QueueBytes(bytes)
            && ProcessGameModePacketLoop(mode, input, router, 1).packetsDispatched == 1
            && !input.BufferedBytes(), "team chat frame did not drain");
    };
    const auto& chat = g_windowMgr.GetChatEvents();
    incoming(speaker.m_gid, "Peer : Rally");
    Require(chat.size() == 1 && chat.back().text == "Peer : Rally" && chat.back().color == 0xff009b
        && chat.back().channel == 5 && chat.back().source.route == ro::chat::NoticeRoute::Normal
        && chat.back().source.type == ro::chat::NoticeType::Chat && speaker.GetChatBubbleText() == "Rally",
        "ordinary team chat lost source routing or actor speech");
    g_session.m_exAidList = {speaker.m_gid};
    incoming(speaker.m_gid, "Peer : Ignore");
    Require(chat.size() == 1 && speaker.GetChatBubbleText() == "Rally" && mode.m_sourceAutoSaveChatCount == 1,
        "excluded team sender reached the notice, bubble, or log");
    g_session.m_exAidList.clear();
    incoming(0, "Battle begins");
    Require(chat.size() == 2 && chat.back().text == "Battle begins" && !mode.m_aidList.count(0),
        "team server notice was dropped or treated as an actor");
    g_session.m_isNoTalkMsg = 1;
    incoming(speaker.m_gid, "Peer : Silent one");
    Require(chat.size() == 2 && speaker.GetChatBubbleText() == "Silent one", "first no-talk option changed team speech");
    g_session.m_isNoTalkMsg = 0;
    g_session.m_isNoTalkMsg2 = 1;
    incoming(speaker.m_gid, "Peer : Silent two");
    Require(chat.size() == 2 && speaker.GetChatBubbleText() == "Silent two", "second no-talk option changed team speech");
    g_serviceType = ServiceIndonesia;
    incoming(speaker.m_gid, "Peer : Highlighted");
    Require(chat.size() == 3 && chat.back().color == 0xff0000 && chat.back().source.type == ro::chat::NoticeType::None
        && speaker.GetChatBubbleText() == "Highlighted" && mode.m_sourceAutoSaveChatCount == 5,
        "special speaker lost its no-talk bypass, color, or independent logging");
    incoming(speaker.m_gid, "Peer : Unterminated", false);
    incoming(speaker.m_gid, std::string(256, 'x'));
    Require(chat.size() == 3 && mode.m_sourceAutoSaveChatCount == 5
        && speaker.GetChatBubbleText() == "Highlighted", "invalid text reached a team chat consumer");
    std::ifstream logFile(log, std::ios::binary);
    const std::string saved((std::istreambuf_iterator<char>(logFile)), {});
    logFile.close();
    std::string expected;
    for (const char* line : {"Peer : Rally", "Battle begins", "Peer : Silent one", "Peer : Silent two", "Peer : Highlighted"}) {
        expected += line;
        expected.append("\0\r\n", 3);
    }
    Require(saved == expected, "team chat autosave changed the formatted messages");
    std::filesystem::remove(log);
    mode.m_runtimeActors.clear();
    world.UnregisterActor(&speaker);
    mode.m_world = nullptr;
    g_serviceType = oldService;
    g_session.m_exAidList = oldExclusions;
    s_dwAdminAID = oldAdmins;
    g_session.m_isSaveChat = oldSaveChat;
    g_session.m_isNoTalkMsg = oldNoTalk;
    g_session.m_isNoTalkMsg2 = oldNoTalk2;
    g_windowMgr.ClearChatEvents();
}

void InstanceStatusReplay()
{
    SetCase("instance_status", "PK42.02", "selected-02cb-02ce", "map packets -> instance state and notices");
    using Phase = CSession::InstanceStatus::Phase;
    CGameMode mode;
    TestConnection input;
    auto& router = GetProductionGameModePacketRouter();
    auto& status = g_session.m_instanceStatus;
    const auto previous = status;
    status = {};
    g_windowMgr.ClearChatEvents();
    const auto dispatch = [&](const std::vector<u8>& bytes, int count = 1) {
        Require(input.QueueBytes(bytes) && ProcessGameModePacketLoop(mode, input, router, count).packetsDispatched == count
            && !input.BufferedBytes(), "instance stream lost framing");
    };
    dispatch({0xce, 2, 0, 0, 0, 0, 1, 0, 0, 0});
    Require(status.phase == Phase::None, "unnamed notification created an instance");
    std::vector<u8> reservation(65, 'X');
    reservation[0] = 0xcb; reservation[1] = 2;
    reservation[63] = 9; reservation[64] = 0;
    auto stream = reservation;
    stream.insert(stream.end(), {0xcc, 2, 7, 0});
    dispatch(stream, 2);
    Require(status.phase == Phase::Waiting && status.queuePosition == 7 && status.name == std::string(61, 'X'),
        "reservation or queue position used the obsolete wire layout");
    dispatch({0xcc, 2, 0xff, 0xff});
    Require(status.phase == Phase::None && g_windowMgr.GetChatEvents().size() == 1, "queue cancellation retained the reservation");
    std::vector<u8> active(71, 0);
    active[0] = 0xcd; active[1] = 2;
    std::memcpy(active.data() + 2, "Instance", 8);
    active[63] = 0x34; active[64] = 0x12; active[66] = 0xf1;
    active[67] = 0x78; active[68] = 0x56; active[70] = 0xe2;
    dispatch(active);
    Require(status.phase == Phase::Ready && status.name == "Instance" && status.expiration == 0xf1001234u
        && status.idleDeadline == 0xe2005678u, "instance snapshot changed unsigned absolute deadlines");
    dispatch({0xce, 2, 0, 0, 0, 0, 0, 0, 0, 0});
    Require(status.phase == Phase::Active && !status.idleDeadline && status.expiration == 0xf1001234u,
        "idle timer stop replaced the lifetime deadline");
    dispatch({0xce, 2, 0, 0, 0, 0, 1, 2, 3, 4});
    Require(status.phase == Phase::Ready && status.idleDeadline == 0x04030201, "idle timer update did not restore ready phase");
    dispatch({0xce, 2, 1, 1, 0, 0, 0, 0, 0, 0});
    Require(status.phase == Phase::Ready, "unknown 32-bit notification closed the snapshot");
    for (u8 result = 1; result <= 4; ++result) {
        dispatch(active);
        dispatch({0xce, 2, result, 0, 0, 0, 0, 0, 0, 0});
        Require(status.phase == Phase::None && status.name.empty(), "terminal notification retained instance state");
    }
    Require(g_windowMgr.GetChatEvents().size() == 4, "terminal notifications did not produce three notices and one silent failure");
    status = previous;
    g_windowMgr.ClearChatEvents();
}

void CashShopReplay()
{
    SetCase("npc_cash_shop", "PK40.03", "selected-0287-0289", "NPC shop purchase and balance update");
    CGameMode mode;
    auto* previousMode = SkillPacketReplayAccess::Current();
    const auto previousType = SkillPacketReplayAccess::CurrentType();
    SkillPacketReplayAccess::SetCurrent(&mode);
    TestConnection input;
    CGameModePacketRouter router;
    RegisterDefaultGameModePacketHandlers(router);
    auto* outgoing = CRagConnection::instance();
    outgoing->Disconnect();
    LoopbackPeer server(*outgoing);
    g_session.ClearNpcShopState();
    const auto dispatch = [&](const std::vector<u8>& bytes) {
        Require(input.QueueBytes(bytes), "could not queue cash shop frame");
        Require(ProcessGameModePacketLoop(mode, input, router, 1).packetsDispatched == 1
            && !input.BufferedBytes(), "cash shop frame did not dispatch");
    };
    const std::vector<u8> list{0x87,2,23,0, 120,0,0,0, 150,0,0,0, 110,0,0,0, 100,0,0,0, 0,0xf5,1};
    dispatch(list);
    Require(g_session.m_shopUsesCash && g_session.m_cashShopPoints == 120 && g_session.m_kafraShopPoints == 150
        && g_session.m_shopRows.size() == 1 && g_session.m_shopRows.front().secondaryPrice == 100,
        "cash shop catalog lost balances or discounted price");
    auto malformed = list;
    malformed.push_back(0);
    ++malformed[2];
    dispatch(malformed);
    Require(g_session.m_shopRows.size() == 1 && g_session.AdjustNpcShopDealBySourceRow(0, 2),
        "partial cash catalog replaced the active quote");
    const auto chooseContribution = [&](const char* points) {
        Require(mode.SendMsg(CGameMode::GameMsg_RequestShopBuyList,0,0,0), "cash split did not open");
        auto* input = g_windowMgr.m_npcInputWnd;
        Require(input && input->IsGameNumberPromptFor(CGameMode::GameMsg_ConfirmCashShopContribution),
            "cash split did not use the shared numeric input");
        input->GetEditCtrl()->SetText(points);
        input->HandleKeyDown(VK_RETURN);
        Require(g_windowMgr.m_roomMessageBox && !input->m_show && !g_session.m_cashShopPending,
            "cash split did not wait for explicit confirmation");
    };
    chooseContribution("149");
    g_windowMgr.m_roomMessageBox->Dismiss(false);
    Require(!g_session.m_cashShopPending && g_session.m_shopDealRows.size() == 1,
        "canceling confirmation consumed the cash deal");
    chooseContribution("149");
    g_windowMgr.m_roomMessageBox->Dismiss(true);
    Require(server.ReceiveExact(10) == std::vector<u8>{0x88,2,0xf5,1,2,0,100,0,0,0},
        "cash purchase did not send the selected item, quantity and contribution");
    Require(!mode.SendMsg(CGameMode::GameMsg_RequestShopBuyList,0,0,0), "duplicate cash purchase was allowed");
    dispatch({0x89,2,110,0,0,0, 130,0,0,0, 6,0});
    Require(!g_session.m_cashShopPending && g_session.m_cashShopPoints == 110 && g_session.m_kafraShopPoints == 130,
        "cash shop rejection did not refresh both balances");
    dispatch({0x89,2,0,0,0,0, 0,0,0,0, 0,0});
    Require(g_session.m_cashShopPoints == 110 && g_session.m_kafraShopPoints == 130,
        "duplicate cash result mutated balances");
    Require(g_session.AdjustNpcShopDealBySourceRow(0,1),
        "cash shop did not allow a subsequent purchase");
    chooseContribution("0");
    dispatch(list);
    Require(!g_windowMgr.m_roomMessageBox && !g_session.m_cashShopPending,
        "replacement catalog kept a stale cash confirmation");
    Require(g_session.AdjustNpcShopDealBySourceRow(0,1), "replacement cash catalog did not allow selection");
    chooseContribution("0");
    g_windowMgr.m_roomMessageBox->Dismiss(true);
    Require(server.ReceiveExact(10) == std::vector<u8>{0x88,2,0xf5,1,1,0,0,0,0,0},
        "cash-only purchase encoded an unexpected Kafra contribution");
    dispatch(list);
    dispatch({0x89,2,10,0,0,0, 70,0,0,0, 0,0});
    Require(!g_session.m_cashShopPending && g_session.m_cashShopPoints == 120 && g_session.m_kafraShopPoints == 150,
        "a late cash result changed the replacement quote");
    g_windowMgr.CloseNpcShopWindows();
    g_windowMgr.ClearChatEvents();
    g_session.ClearNpcShopState();
    outgoing->Disconnect();
    SkillPacketReplayAccess::Restore(previousMode, previousType);
}

void AuctionSearchSnapshotReplay()
{
    SetCase("auction_search_snapshot", "PK40.02", "selected-0252-025f-auction-search", "map packet loop -> auction page");
    CGameMode mode;
    auto* oldMode = SkillPacketReplayAccess::Current();
    const auto oldModeType = SkillPacketReplayAccess::CurrentType();
    SkillPacketReplayAccess::SetCurrent(&mode);
    auto* outgoing = CRagConnection::instance();
    outgoing->Disconnect();
    LoopbackPeer server(*outgoing);
    TestConnection connection;
    CGameModePacketRouter router;
    RegisterDefaultGameModePacketHandlers(router);
    const auto dispatch = [&](const std::vector<u8>& bytes, int count = 1) {
        Require(connection.QueueBytes(bytes), "could not queue auction page fixture");
        Require(ProcessGameModePacketLoop(mode, connection, router, count).packetsDispatched == count
            && !connection.BufferedBytes(), "auction page fixture lost packet alignment");
    };
    std::vector<u8> page(95);
    const std::array<u8, 12> header{0x52,2,95,0, 3,0,0,0, 1,0,0,0};
    std::copy(header.begin(), header.end(), page.begin());
    const std::string seller = "ABCDEFGHIJKLMNOPQRSTUVWX";
    std::copy(seller.begin(), seller.end(), page.begin() + 16);
    const std::array<u8, 27> item{0xf5,1, 4,3,2,1, 1,0, 1,1,7,
        1,0,2,0,3,0,4,0, 0x78,0x56,0x34,0x12, 0x21,0x43,0x65,0x87};
    std::copy(item.begin(), item.end(), page.begin() + 40);
    const std::string buyer = "buyer";
    std::copy(buyer.begin(), buyer.end(), page.begin() + 67);
    page[12] = 0x42;
    page[94] = 0x80;

    dispatch(page);
    Require(mode.m_auctionSearch.rows.empty(), "auction results opened a closed search session");
    dispatch({0x5f,2,0,0,0,0});
    Require(mode.m_auctionSearch.open, "server open did not establish auction search ownership");
    const auto clickAuction = [](int x, int y) {
        const auto* window = g_windowMgr.m_auctionWnd;
        Require(window && window->m_show, "server auction command did not open a visible window");
        const int screenX = window->m_x + x;
        const int screenY = window->m_y + y;
        g_windowMgr.OnLBtnDown(screenX, screenY);
        g_windowMgr.OnLBtnUp(screenX, screenY);
    };
    std::vector<u8> browse(34);
    browse[0] = 0x51;
    browse[1] = 2;
    browse[2] = 4;
    browse[32] = 1;
    Require(server.ReceiveExact(browse.size()) == browse, "initial auction search did not request all names");
    Require(!mode.RequestAuctionSearch({}), "auction search allowed two uncorrelated requests in flight");
    dispatch(page);
    const auto& state = mode.m_auctionSearch;
    Require(state.pages == 3 && state.rows.size() == 1, "auction snapshot lost its row count or pages");
    const auto& row = state.rows.front();
    Require(row.auctionId == 0x42 && row.seller == seller && row.buyer == buyer && row.itemId == 501
        && row.itemType == 0x01020304 && row.amount == 1 && row.identified && row.damaged && row.refine == 7
        && row.cards == std::array<u16,4>{1,2,3,4} && row.price == 0x12345678u
        && row.buyNow == 0x87654321u && row.expiresAt == 0x80000000u, "auction row fields or widths differ");

    using Kind = CGameMode::AuctionSearchKind;
    clickAuction(190, 35);
    Require(server.ReceiveExact(4) == std::vector<u8>{0x5c, 2, 1, 0},
        "the auction window's own-bids control did not send the selected request");
    auto malformed = page;
    malformed[8] = 2;
    dispatch(malformed);
    malformed = page;
    malformed.pop_back();
    --malformed[2];
    dispatch(malformed);
    Require(state.pages == 3 && state.rows.size() == 1 && state.rows.front().auctionId == 0x42,
        "malformed auction pages replaced the previous snapshot");
    dispatch({0x52,2,12,0, 1,0,0,0, 0,0,0,0});
    Require(state.pages == 1 && state.rows.empty(), "empty auction page retained old rows");
    Require(mode.RequestAuctionSearch({Kind::Price, 2, 0x12345678, {}}), "could not request a price-filtered auction page");
    auto priceSearch = browse;
    priceSearch[2] = 5;
    priceSearch[4] = 0x78;
    priceSearch[5] = 0x56;
    priceSearch[6] = 0x34;
    priceSearch[7] = 0x12;
    priceSearch[32] = 2;
    Require(server.ReceiveExact(priceSearch.size()) == priceSearch, "auction price or page was encoded incorrectly");
    dispatch({0x5f,2,1,0,0,0});
    dispatch({0x5f,2,0,0,0,0});
    Require(state.open && state.awaitingReply && state.discardReply
        && !mode.RequestAuctionSearch({}), "auction reopen sent a query before the old reply drained");
    dispatch(page);
    Require(state.open && state.awaitingReply && !state.discardReply && state.rows.empty()
        && server.ReceiveExact(browse.size()) == browse, "late auction page replaced the reopened session");
    dispatch(page);
    Require(state.rows.size() == 1, "fresh auction page did not reach the reopened session");
    g_windowMgr.ClearChatEvents();
    dispatch({0x50,2,6});
    Require(state.awaitingReply && server.ReceiveExact(browse.size()) == browse,
        "auction win did not refresh the active search");
    dispatch(page);
    g_windowMgr.ClearChatEvents();
    const auto inventory = g_session.GetInventoryItems();
    g_session.ClearInventoryItems();
    ITEM_INFO itemToStage;
    itemToStage.m_itemIndex = 42;
    itemToStage.SetItemId(1201);
    itemToStage.m_num = 2;
    itemToStage.m_isIdentified = 1;
    g_session.SetInventoryItem(itemToStage);
    const std::vector<u8> stage{0x4c,2,42,0,1,0,0,0};
    const auto requestStage = [&] {
        Require(mode.RequestAuctionStageItem(42), "valid auction stage request was rejected");
        const auto sent = server.ReceiveExact(stage.size());
        Require(sent == stage, "auction stage request bytes differed: " + Hex(sent));
    };
    clickAuction(260, 35);
    clickAuction(120, 110);
    clickAuction(290, 215);
    Require(server.ReceiveExact(stage.size()) == stage, "the registration controls did not stage the selected item");
    dispatch({0x56,2,40,0,0});
    Require(mode.m_auctionStaging.pendingIndex == 42, "raw-index success claimed the pending item");
    dispatch({0x56,2,40,0,1});
    Require(!mode.m_auctionStaging.pendingIndex && !mode.m_auctionStaging.stagedIndex,
        "raw-index rejection left the staging request active");
    requestStage();
    dispatch({0x56,2,42,0,0});
    dispatch({0x56,2,42,0,0});
    Require(mode.m_auctionStaging.stagedIndex == 42 && mode.m_auctionStaging.stagedItemId == 1201
        && g_session.GetInventoryItemByIndex(42)->m_num == 1,
        "staging success did not reserve exactly one item");
    const u16 pickup = ro::net::GetActiveMapReceiveProfile().itemPickupAckExtended;
    std::vector<u8> returnedItem(29);
    returnedItem[0] = static_cast<u8>(pickup);
    returnedItem[1] = static_cast<u8>(pickup >> 8);
    returnedItem[2] = 42;
    returnedItem[4] = 1;
    returnedItem[6] = 0xb1;
    returnedItem[7] = 4;
    returnedItem[8] = 1;
    Require(mode.CancelAuctionStaging() && server.ReceiveExact(4) == std::vector<u8>{0x4b,2,1,0},
        "staging cancel did not send the selected request");
    dispatch(returnedItem);
    Require(g_session.GetInventoryItemByIndex(42)->m_num == 2, "cancel return did not restore exactly one item");
    requestStage();
    Require(mode.RequestAuctionSearch({}) && server.ReceiveExact(browse.size()) == browse,
        "could not start a search while staging a pending item");
    dispatch({0x56,2,42,0,0});
    Require(!mode.m_auctionStaging.pendingIndex && !mode.m_auctionStaging.stagedIndex,
        "a stage acknowledgement revived the selection canceled by search");
    dispatch(returnedItem);
    Require(g_session.GetInventoryItemByIndex(42)->m_num == 2, "search cancellation changed the inventory total");
    dispatch(page);
    using Transaction = CGameMode::AuctionTransaction;
    requestStage();
    dispatch({0x56,2,42,0,0});
    Require(!mode.RequestAuctionRegistration(100, 100, 1) && !mode.RequestAuctionRegistration(100, 200, 49),
        "registration accepted invalid prices or duration");
    Require(mode.RequestAuctionRegistration(100, 200, 48)
        && server.ReceiveExact(12) == std::vector<u8>{0x4d,2,100,0,0,0,200,0,0,0,48,0},
        "registration did not serialize the staged item prices and duration");
    Require(!mode.RequestAuctionRegistration(100, 200, 48), "duplicate registration reached the server");
    dispatch({0x50,2,1});
    Require(!mode.m_auctionStaging.stagedIndex && g_session.GetInventoryItemByIndex(42)->m_num == 1
        && server.ReceiveExact(4) == std::vector<u8>{0x5c,2,0,0},
        "registration success did not consume the reservation or refresh own sales");
    dispatch(page);
    const auto bidQuote = state.rows.front();
    auto staleQuote = bidQuote;
    ++staleQuote.price;
    Require(!mode.RequestAuctionTransaction(Transaction::Bid, staleQuote, 0x12345679)
        && !mode.RequestAuctionTransaction(Transaction::Bid, bidQuote, bidQuote.price),
        "auction accepted a stale quote or an insufficient bid");
    Require(mode.RequestAuctionTransaction(Transaction::Bid, bidQuote, 0x12345679)
        && server.ReceiveExact(10) == std::vector<u8>{0x4f,2,0x42,0,0,0,0x79,0x56,0x34,0x12},
        "auction bid did not serialize the selected quote");
    dispatch({0x50,2,7});
    Require(mode.m_auctionTransaction == Transaction::Bid && !mode.RequestAuctionSearch({}),
        "unsolicited loss released a pending bid");
    dispatch({0x50,2,1});
    Require(mode.m_auctionTransaction == Transaction::None && state.awaitingReply
        && state.query.kind == Kind::OwnBids, "successful bid did not own the server-pushed bid page");
    dispatch(page);
    const std::string oldName = g_session.GetPlayerName();
    std::snprintf(g_session.m_playerName, sizeof(g_session.m_playerName), "%s", seller.c_str());
    Require(mode.RequestAuctionTransaction(Transaction::Stop, state.rows.front())
        && server.ReceiveExact(6) == std::vector<u8>{0x5d,2,0x42,0,0,0},
        "stop request did not preserve the six-byte outbound opcode");
    dispatch({0x5d,2,0,0});
    Require(state.awaitingReply && state.query.kind == Kind::OwnSales,
        "successful stop did not own the server-pushed sales page");
    auto noBidder = page;
    std::fill(noBidder.begin() + 67, noBidder.begin() + 91, 0);
    dispatch(noBidder);
    Require(mode.RequestAuctionTransaction(Transaction::Cancel, state.rows.front())
        && server.ReceiveExact(6) == std::vector<u8>{0x4e,2,0x42,0,0,0}, "cancel did not send the selected sale ID");
    dispatch({0x5d,2,2,0});
    Require(mode.m_auctionTransaction == Transaction::None && !state.awaitingReply,
        "cancel failure through the close reply did not release its transaction");
    std::snprintf(g_session.m_playerName, sizeof(g_session.m_playerName), "%s", oldName.c_str());
    g_session.ClearInventoryItems();
    for (const auto& item : inventory) g_session.SetInventoryItem(item);
    g_windowMgr.ClearChatEvents();
    clickAuction(461, 10);
    dispatch(page);
    Require(!state.open && !state.pages && state.rows.empty() && !g_windowMgr.m_auctionWnd,
        "auction close retained the window or accepted late results");
    outgoing->Disconnect();
    SkillPacketReplayAccess::Restore(oldMode, oldModeType);
}

void AuctionResultNoticeReplay()
{
    SetCase("auction_results", "PK40.02", "selected-0250-025d-auction-notices", "map packet loop -> auction chat");
    CGameMode mode;
    TestConnection connection;
    CGameModePacketRouter router;
    RegisterDefaultGameModePacketHandlers(router);
    g_windowMgr.ClearChatEvents();
    std::vector<u8> frames;
    for (u8 result = 0; result <= 10; ++result) {
        frames.insert(frames.end(), {0x50, 0x02, result});
    }
    for (u8 result = 0; result <= 3; ++result) {
        frames.insert(frames.end(), {0x5d, 0x02, result, 0});
    }
    frames.insert(frames.end(), {0x5d, 0x02, 1, 1, 0x9a, 0, 9, 0, 'n', 'e', 'x', 't', 0});
    Require(connection.QueueBytes(frames), "could not queue auction notices and broadcast");
    Require(ProcessGameModePacketLoop(mode, connection, router, 17).packetsDispatched == 17
        && !connection.BufferedBytes(), "four-byte auction close reply lost stream alignment");
    const auto& events = g_windowMgr.GetChatEvents();
    Require(events.size() == 14 && events.back().text == "next", "auction results lost a notice or accepted an unknown code");
    for (std::size_t i = 0; i < 13; ++i) {
        const u32 color = i == 1 || i == 6 || i == 10 ? 0x00ffffffu : 0x00ff0000u;
        Require(!events[i].text.empty() && events[i].color == color, "auction result lost its reference notice color");
    }
    g_windowMgr.ClearChatEvents();
}

void PvpRankReplay()
{
    SetCase("pvp_rank", "PK39.02", "selected-019a-pvp-rank", "map packet loop -> actor rank and badge");
    CPlayer player;
    CPc remote;
    CWorld world;
    CGameMode mode;
    world.m_player = &player;
    mode.m_world = &world;
    remote.m_gid = 0x55;
    world.RegisterActor(&remote);
    mode.m_runtimeActors.emplace(remote.m_gid, &remote);
    const auto oldAid = g_session.m_aid;
    const auto oldEffects = g_session.m_isEffectOn;
    auto* oldMode = SkillPacketReplayAccess::Current();
    const auto oldModeType = SkillPacketReplayAccess::CurrentType();
    SkillPacketReplayAccess::SetCurrent(&mode);
    player.m_gid = g_session.m_aid = 0x44;
    g_session.m_isEffectOn = true;
    g_windowMgr.ClearChatEvents();

    TestConnection connection;
    CGameModePacketRouter router;
    RegisterDefaultGameModePacketHandlers(router);
    const auto update = [&](u8 aid, u32 rank, u8 total) {
        Require(connection.QueueBytes({0x9a,1,aid,0,0,0, static_cast<u8>(rank), static_cast<u8>(rank >> 8),
            static_cast<u8>(rank >> 16), static_cast<u8>(rank >> 24), total,0,0,0}), "could not queue PvP rank");
        Require(ProcessGameModePacketLoop(mode, connection, router, 1).packetsDispatched == 1
            && !connection.BufferedBytes(), "could not frame and dispatch PvP rank");
    };
    const auto badges = [](const CGameActor& actor) {
        return std::count_if(actor.m_effectList.begin(), actor.m_effectList.end(), [](const CRagEffect* effect) {
            return effect && effect->GetEffectType() == 159;
        });
    };
    update(0x44, 1, 12);
    Require(player.m_pk_rank == 0 && badges(player) == 0, "PvP rank applied outside a PK zone");
    world.m_isPKZone = 1;
    update(0x44, 11, 12);
    update(0x44, 1, 12);
    const auto rankStart = mode.m_pvpRankStartTick;
    Require(player.m_pk_rank == 1 && player.m_pk_total == 12 && rankStart != 0 && badges(player) == 1,
        "rank-one entry did not update local rank, timer and badge");
    update(0x44, 1, 13);
    Require(player.m_pk_total == 13 && mode.m_pvpRankStartTick == rankStart && badges(player) == 1
        && g_windowMgr.GetChatEvents().size() == 2, "rank-one refresh changed its timer or stacked badges or notices");
    update(0x44, 0, 0);
    Require(player.m_pk_rank == 0 && player.m_pk_total == 0 && !mode.m_pvpRankStartTick && badges(player) == 0,
        "PvP rank reset retained local state");

    remote.m_effectState = 0x0004;
    update(0x55, 4, 12);
    update(0x66, 2, 12);
    Require(remote.m_pk_rank == 0 && badges(remote) == 0, "hidden actor received a PvP badge");
    remote.m_effectState = 0;
    update(0x55, 4, 12);
    Require(remote.m_pk_rank == 4 && remote.m_pk_total == 12 && badges(remote) == 1,
        "visible remote rank did not apply its state and badge");
    const u16 stateOpcode = ro::net::GetActiveMapReceiveProfile().actorStateChangeBasic;
    Require(connection.QueueBytes({static_cast<u8>(stateOpcode), static_cast<u8>(stateOpcode >> 8),
        0x55,0,0,0, 0,0, 0,0, 4,0, 0}), "could not queue the ranked actor's hiding transition");
    Require(ProcessGameModePacketLoop(mode, connection, router, 1).packetsDispatched == 1
        && !connection.BufferedBytes() && remote.m_effectState == 4 && badges(remote) == 0,
        "hiding left the remote actor's old rank badge visible");
    remote.m_effectState = 0;
    update(0x55, 4, 12);
    update(0x55, 0xffffffffu, 12);
    Require(remote.m_pk_rank == -1 && badges(remote) == 0, "hidden-rank sentinel left a badge");
    Require(g_windowMgr.GetChatEvents().size() == 2, "remote or reset rank emitted a local notice");
    g_windowMgr.ClearChatEvents();
    mode.m_runtimeActors.clear();
    world.UnregisterActor(&remote);
    world.m_player = nullptr;
    mode.m_world = nullptr;
    SkillPacketReplayAccess::Restore(oldMode, oldModeType);
    g_session.m_aid = oldAid;
    g_session.m_isEffectOn = oldEffects;
}

void PvpPointsReplay()
{
    SetCase("pvp_points", "PK39.02", "selected-0210-pvp-points", "map packet loop -> PvP points chat");
    CGameMode mode;
    TestConnection connection;
    CGameModePacketRouter router;
    RegisterDefaultGameModePacketHandlers(router);
    g_windowMgr.ClearChatEvents();
    Require(connection.QueueBytes({0x10,2,0xef,0xcd,0xab,0x90,0x78,0x56,0x34,0x12,
        23,0,0,0,47,0,0,0,0xf9,0xff,0xff,0xff, 0x9a,0,9,0,'n','e','x','t',0}),
        "could not queue PvP points and broadcast");
    Require(ProcessGameModePacketLoop(mode, connection, router, 2).packetsDispatched == 2
        && !connection.BufferedBytes(), "PvP points reply did not preserve stream alignment");
    const auto& events = g_windowMgr.GetChatEvents();
    Require(events.size() == 2 && events[0].color == 0x0000ffffu && events[1].text == "next",
        "PvP points reply did not reach cyan system chat");
    const auto wins = events[0].text.find("23");
    const auto losses = events[0].text.find("47");
    const auto points = events[0].text.find("-7");
    Require(wins < losses && losses < points && points != std::string::npos,
        "PvP points notice lost wins, losses or signed points");
    g_windowMgr.ClearChatEvents();
}

void LegacyRankingReplay()
{
    SetCase("legacy_ranking", "PK39.02", "selected-legacy-rank-list-and-points", "map packet loop -> ranking chat");
    CGameMode mode;
    TestConnection connection;
    CGameModePacketRouter router;
    RegisterDefaultGameModePacketHandlers(router);
    g_windowMgr.ClearChatEvents();
    const std::string fullName = "ABCDEFGHIJKLMNOPQRSTUVWX";
    const std::array<u8, 4> lists{0x19, 0x1a, 0x26, 0x38};
    const std::array<u8, 4> points{0x1b, 0x1c, 0x24, 0x36};
    for (size_t i = 0; i < lists.size(); ++i) {
        std::vector<u8> list(282, 0);
        list[0] = lists[i]; list[1] = 2;
        std::copy(fullName.begin(), fullName.end(), list.begin() + 218);
        list[278] = 0x34; list[279] = 0x12;
        Require(connection.QueueBytes(list) && connection.QueueBytes({points[i],2,0xf9,0xff,0xff,0xff,19,0,0,0}),
            "could not queue ranking list and points");
        Require(ProcessGameModePacketLoop(mode, connection, router, 2).packetsDispatched == 2
            && !connection.BufferedBytes(), "rank frame consumed bytes from the following point result");
        const auto& events = g_windowMgr.GetChatEvents();
        Require(events.size() == 13 && events[10].text.find(fullName) != std::string::npos
            && events[10].text.find("4660") != std::string::npos && events[12].text.find("-7") != std::string::npos
            && events[12].text.find("19") != std::string::npos, "rank rows or point notification differ");
        g_windowMgr.ClearChatEvents();
    }
}

void MvpRewardReplay()
{
    SetCase("mvp_reward", "PK39.02", "selected-010a-010d-mvp", "map packet loop -> chat and actor message 36");
    struct Winner : CPlayer {
        int effects = 0;
        void SendMsg(CGameObject*, int message, msgparam_t, msgparam_t, msgparam_t) override
        {
            if (message == 36) ++effects;
        }
    } local, remote;
    CWorld world;
    CGameMode mode;
    const auto oldAid = g_session.m_aid;
    g_session.m_aid = local.m_gid = 0x11223344;
    remote.m_gid = 0x55667788;
    world.m_player = &local;
    world.RegisterActor(&remote);
    mode.m_world = &world;
    TestConnection connection;
    CGameModePacketRouter router;
    RegisterDefaultGameModePacketHandlers(router);
    g_windowMgr.ClearChatEvents();
    const auto inventorySize = g_session.GetInventoryItems().size();
    Require(connection.QueueBytes({0x0a,1,0xf5,1, 0x0b,1,0xff,0xff,0xff,0x7f,
        0x0c,1,0x44,0x33,0x22,0x11, 0x0c,1,0x88,0x77,0x66,0x55,
        0x0c,1,1,0,0,0, 0x0d,1, 0x9a,0,9,0,'n','e','x','t',0}), "could not queue MVP frames");
    Require(ProcessGameModePacketLoop(mode, connection, router, 7).packetsDispatched == 7
        && !connection.BufferedBytes(), "MVP packets did not preserve stream alignment");
    const auto& chat = g_windowMgr.GetChatEvents();
    Require(chat.size() == 4 && chat[0].text.find(g_ttemmgr.GetDisplayName(501, false)) != std::string::npos
        && chat[0].color == 0x0000ffff && chat[1].text.find("2147483647") != std::string::npos
        && chat[1].color == 0x0000ffff && chat[2].color == 0x00ff0000 && chat[3].text == "next",
        "MVP reward notices differ");
    Require(local.effects == 1 && remote.effects == 1 && g_session.GetInventoryItems().size() == inventorySize,
        "MVP notices changed inventory or routed the effect incorrectly");
    g_windowMgr.ClearChatEvents();
    mode.m_world = nullptr;
    world.UnregisterActor(&remote);
    world.m_player = nullptr;
    g_session.m_aid = oldAid;
}

void SpecialItemObtainReplay()
{
    SetCase("special_item_obtain", "PK38.01", "selected-07fd-monster-npc",
        "ProcessGameModePacketLoop -> 07FD item obtain broadcast");
    CGameMode mode;
    TestConnection connection;
    CGameModePacketRouter router;
    RegisterDefaultGameModePacketHandlers(router);
    g_windowMgr.ClearChatEvents();
    const std::string character = "Collector";
    const std::string monster = "ABCDEFGHIJKLMNOPQRSTUVW";
    std::vector<u8> npc(32, 0);
    npc[0] = 0xfd; npc[1] = 7; npc[2] = 32; npc[4] = 2;
    npc[5] = 0xf5; npc[6] = 1; npc[7] = 24;
    std::copy(character.begin(), character.end(), npc.begin() + 8);
    auto drop = npc;
    drop.resize(56);
    drop[2] = 56; drop[4] = 1; drop[32] = 24;
    std::copy(monster.begin(), monster.end(), drop.begin() + 33);
    auto bad = drop;
    bad[32] = 25;
    std::vector<u8> tail(drop.begin() + 20, drop.end());
    tail.insert(tail.end(), bad.begin(), bad.end());
    tail.insert(tail.end(), npc.begin(), npc.end());
    const std::vector<u8> broadcast{0x9a, 0, 9, 0, 'n', 'e', 'x', 't', 0};
    tail.insert(tail.end(), broadcast.begin(), broadcast.end());
    Require(connection.QueueBytes({drop.begin(), drop.begin() + 20}), "could not queue partial 07FD");
    Require(ProcessGameModePacketLoop(mode, connection, router, 4).packetsProcessed == 0
        && connection.BufferedBytes() == 20 && g_windowMgr.GetChatEvents().empty(), "partial 07FD dispatched early");
    Require(connection.QueueBytes(tail), "could not queue the rest of the announcement stream");
    Require(ProcessGameModePacketLoop(mode, connection, router, 4).packetsDispatched == 4
        && !connection.BufferedBytes(), "07FD did not preserve following frames");
    const auto& events = g_windowMgr.GetChatEvents();
    Require(events.size() == 3 && events[0].text.find(character) != std::string::npos
        && events[0].text.find(monster) != std::string::npos && events[1].text.find(character) != std::string::npos
        && events[1].text.find(monster) == std::string::npos && events[2].text == "next",
        "07FD presentation, malformed-frame isolation, or subsequent broadcast differs");
    g_windowMgr.ClearChatEvents();
}

void ChatNoticeProducerReplay()
{
    SetCase("chat_notice_producer_source_metadata", "PK09.21",
        "PK09.21-selected-pickup-equip-notices",
        "ProcessGameModePacketLoop -> production router -> HandleItemPickupAck/HandleEquipItemAck -> PushSourceChatEvent");

    CGameMode mode;
    TestConnection connection;
    CGameModePacketRouter& router = GetProductionGameModePacketRouter();
    struct Restore {
        std::vector<ITEM_INFO> inventory{g_session.GetInventoryItems().begin(), g_session.GetInventoryItems().end()};
        std::vector<UIChatEvent> chatEvents{g_windowMgr.GetChatEvents()};
        std::list<u32> pickupQueue;
        std::array<bool, 13> battleOptions = g_session.m_battleMsgOnOffSet;
        CGameMode& mode;
        CMode* currentMode;
        int currentModeType;
        int chatSticky, managerChatShow, managerChatStatus, subChatSTEnabled, subChatBTEnabled;
        u32 aid, gid, pickupTick;
        u32 waitingWear, waitingTakeoff;
        int waitingUse;
        int lastCard;
        explicit Restore(CGameMode& m) :
            pickupQueue(m.m_pickupReqItemNaidList), mode(m),
            currentMode(SkillPacketReplayAccess::Current()), currentModeType(SkillPacketReplayAccess::CurrentType()),
            chatSticky(g_windowMgr.m_chatWndStickOn), managerChatShow(g_windowMgr.m_chatWndShow),
            managerChatStatus(g_windowMgr.m_chatWndStatus), subChatSTEnabled(g_windowMgr.m_onStSubChat),
            subChatBTEnabled(g_windowMgr.m_onBtSubChat), aid(g_session.m_aid), gid(g_session.m_gid),
            pickupTick(m.m_lastPickupRequestTick), waitingWear(m.m_waitingWearEquipAck),
            waitingTakeoff(m.m_waitingTakeoffEquipAck), waitingUse(m.m_waitingUseItemAck),
            lastCard(m.m_lastCardItemIndex)
        {}
        ~Restore() {
            g_windowMgr.RemoveAllWindows();
            g_windowMgr.m_chatWndStickOn = chatSticky;
            g_windowMgr.m_chatWndShow = managerChatShow;
            g_windowMgr.m_chatWndStatus = managerChatStatus;
            g_windowMgr.m_onStSubChat = subChatSTEnabled;
            g_windowMgr.m_onBtSubChat = subChatBTEnabled;
            g_windowMgr.ClearChatEvents();
            for (const UIChatEvent& event : chatEvents)
                g_windowMgr.PushChatEvent(event.text.c_str(), event.color, event.channel, event.tick, event.source);
            g_session.m_battleMsgOnOffSet = battleOptions;
            g_session.m_aid = aid;
            g_session.m_gid = gid;
            g_session.ClearInventoryItems();
            for (const ITEM_INFO& item : inventory) g_session.SetInventoryItem(item);
            mode.m_pickupReqItemNaidList = pickupQueue;
            mode.m_lastPickupRequestTick = pickupTick;
            mode.m_waitingWearEquipAck = waitingWear;
            mode.m_waitingTakeoffEquipAck = waitingTakeoff;
            mode.m_waitingUseItemAck = waitingUse;
            mode.m_lastCardItemIndex = lastCard;
            SkillPacketReplayAccess::Restore(currentMode, currentModeType);
            GetProductionGameModePacketRouter().Clear();
            RegisterDefaultGameModePacketHandlers(GetProductionGameModePacketRouter());
        }
    } restore(mode);

    g_windowMgr.RemoveAllWindows();
    g_windowMgr.ClearChatEvents();
    router.Clear();
    RegisterDefaultGameModePacketHandlers(router);
    g_session.m_battleMsgOnOffSet.fill(true);
    g_session.m_aid = 0x13572468;
    g_session.m_gid = 0x24681357;
    SkillPacketReplayAccess::SetCurrent(&mode);
    g_session.ClearInventoryItems();
    g_windowMgr.m_chatWndStickOn = true;
    g_windowMgr.m_onStSubChat = true;
    g_windowMgr.m_onBtSubChat = true;
    g_windowMgr.EnsureChatWindowVisible();
    UINewChatWnd* const chatWnd = g_windowMgr.m_chatWnd;
    Require(chatWnd != nullptr, "managed test chat window was not created");

    const auto put16 = [](std::vector<u8>& p, size_t o, u16 v) {
        p[o] = static_cast<u8>(v);
        p[o + 1] = static_cast<u8>(v >> 8);
    };
    const auto frame = [&](u16 id, int n) {
        std::vector<u8> p(static_cast<size_t>(n));
        put16(p, 0, id);
        return p;
    };
    const auto dispatch = [&](const std::vector<u8>& p, const char* name) {
        Require(connection.QueueBytes(p), std::string("queue failed: ") + name);
        const auto result = ProcessGameModePacketLoop(mode, connection, router, 1);
        Require(result.packetsProcessed == 1 && result.packetsDispatched == 1 && connection.BufferedBytes() == 0,
            std::string("production route failed: ") + name);
    };
    const auto checkSourceEvent = [](const UIChatEvent& event, ro::chat::NoticeType type, u32 color) {
        Require(event.source.route == ro::chat::NoticeRoute::Normal && event.source.type == type,
            "source notice route/type metadata differs");
        Require(event.source.talker.empty(), "source notice unexpectedly carried a talker");
        Require(event.color == color, "source notice UI color differs");
    };

    chatWnd->SetState(7);
    chatWnd->SetState(0);
    Require(chatWnd->GetExpansionState() == 0, "managed chat could not enter collapsed state");

    auto pickup = frame(0x00A0, 23);
    put16(pickup, 2, 69);
    put16(pickup, 4, 2);
    put16(pickup, 6, 501); // Red Potion has a selected item-name fixture.
    pickup[8] = 1;
    pickup[21] = 3;
    dispatch(pickup, "basic pickup success");
    Require(chatWnd->GetSourceHistory(false).size() == 1
        && chatWnd->GetSourceHistory(true).size() == 1,
        "default type-5 battle filter did not retain pickup notice in ST and BT histories");
    Require(g_windowMgr.m_subChatWndST && g_windowMgr.m_subChatWndBT
        && g_windowMgr.m_subChatWndST->GetRows().size() == 1 && g_windowMgr.m_subChatWndBT->GetRows().size() == 1
        && g_windowMgr.m_subChatWndST->GetRows().back().color == 0x0000FFFFu,
        "actual pickup did not mirror into independent managed detached owners");
    Require(chatWnd->GetSourceHistory(false).back().color == 0x0000FFFFu
        && chatWnd->GetSourceHistory(true).back().color == 0x0000FFFFu,
        "pickup source history did not preserve converted cyan in ST and BT");
    Require(!g_windowMgr.GetChatEvents().empty(), "pickup notice did not reach chat events");
    checkSourceEvent(g_windowMgr.GetChatEvents().back(), ro::chat::NoticeType::ObtainItem, 0x0000FFFFu);
    Require(chatWnd->GetExpansionState() == 0,
        "pickup source notice expanded collapsed chat without an explicit manager show message");

    auto equip = frame(0x00AA, 7);
    put16(equip, 2, 0x2222);
    put16(equip, 4, 0x0040);
    equip[6] = 0;
    dispatch(equip, "equip failure");
    Require(chatWnd->GetSourceHistory(false).size() == 2
        && chatWnd->GetSourceHistory(true).size() == 2,
        "default type-6 battle filter did not retain equip notice in ST and BT histories");
    Require(chatWnd->GetSourceHistory(false).back().color == 0x00FF0000u
        && chatWnd->GetSourceHistory(true).back().color == 0x00FF0000u,
        "equip source history did not preserve red in ST and BT");
    checkSourceEvent(g_windowMgr.GetChatEvents().back(), ro::chat::NoticeType::Equip, 0x00FF0000u);
    Require(chatWnd->GetExpansionState() == 0,
        "equip source notice expanded collapsed chat without an explicit manager show message");

    pickup[22] = 1;
    dispatch(pickup, "known pickup failure and explicit show");
    Require(chatWnd->GetSourceHistory(false).size() == 3
        && chatWnd->GetSourceHistory(true).size() == 2,
        "type-0 pickup failure did not route to ST only");
    checkSourceEvent(g_windowMgr.GetChatEvents().back(), ro::chat::NoticeType::None, 0x00FF0000u);
    Require(chatWnd->m_show != 0 && chatWnd->GetExpansionState() == 1,
        "explicit known-failure manager message did not show and expand collapsed chat");

    {
        struct ResourceRestore {
            CItemMgr itemManager = g_ttemmgr;
            std::filesystem::path cwd = std::filesystem::current_path();
            int folderFirst = g_readFolderFirst;
            ~ResourceRestore() {
                std::filesystem::current_path(cwd);
                g_ttemmgr = itemManager;
                g_readFolderFirst = folderFirst;
            }
        } resourceRestore;
        const auto directory = resourceRestore.cwd / ("chat-notice-empty-resource-" + std::to_string(GetTickCount()));
        std::filesystem::create_directories(directory);
        std::ofstream messages(directory / "msgstringtable.txt", std::ios::binary);
        messages << std::string(54, '#');
        messages.close();
        Require(static_cast<bool>(messages), "write isolated empty positional message table failed");
        std::filesystem::current_path(directory);
        g_readFolderFirst = 1;
        g_ttemmgr = CItemMgr();
        Require(g_ttemmgr.GetPickupFailureNotice(1).empty(), "known-failure empty language resource was not loaded");
        g_windowMgr.ClearChatEvents();
        chatWnd->SetState(0); chatWnd->SetShow(0);
        const auto stBeforeEmpty = chatWnd->GetSourceHistory(false).size();
        const auto btBeforeEmpty = chatWnd->GetSourceHistory(true).size();
        pickup[22] = 1;
        dispatch(pickup, "known pickup failure with empty positional message");
        Require(chatWnd->m_show && chatWnd->GetExpansionState() == 1 && g_windowMgr.GetChatEvents().empty()
            && chatWnd->GetSourceHistory(false).size() == stBeforeEmpty
            && chatWnd->GetSourceHistory(true).size() == btBeforeEmpty,
            "empty known-failure resource suppressed explicit show/expand or fabricated history");
    }
    std::cout << "{\"case_id\":\"chat_notice_producer_source_metadata\",\"contract_id\":\"PK09.21\",\"fixture_id\":\"PK09.21-selected-pickup-equip-notices\",\"production_entry\":\"ProcessGameModePacketLoop -> production router -> HandleItemPickupAck/HandleEquipItemAck -> PushSourceChatEvent\",\"pickup_route\":1,\"pickup_type\":5,\"equip_route\":1,\"equip_type\":6,\"failure_route\":1,\"failure_type\":0,\"failure_expansion_message\":3,\"known_empty_resource_case\":\"PASS isolated known-failure empty resource\",\"outcome\":\"PASS\"}\n";
}
