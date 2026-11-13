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
