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
