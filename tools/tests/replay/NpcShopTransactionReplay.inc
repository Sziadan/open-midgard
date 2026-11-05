void NpcShopTransactionReplay()
{
    SetCase("npc_shop_transaction_replay", "PK37.09", "selected-20080910-shop-c5-c8-c9-ca-cb",
        "CGameMode::SendMsg -> NPC shop production sender; production packet router -> NPC shop handlers");
#if RO_ENABLE_QT6_UI
    // The production chooser/list windows resolve item/chat text through Qt font APIs.
    qputenv("QT_QPA_PLATFORM", "offscreen");
#ifdef RO_PACKET_QT_PLATFORM_PLUGINS
    qputenv("QT_QPA_PLATFORM_PLUGIN_PATH", RO_PACKET_QT_PLATFORM_PLUGINS);
#endif
    int argc = 1; char name[] = "npc-shop-replay"; char* argv[] = { name, nullptr };
    QGuiApplication app(argc, argv);
#endif
    CGameMode mode;
    struct Restore {
        std::vector<ITEM_INFO> inventory{ g_session.GetInventoryItems().begin(), g_session.GetInventoryItems().end() };
        std::vector<UIChatEvent> chat = g_windowMgr.GetChatEvents();
        CMode* previousMode = SkillPacketReplayAccess::Current();
        int previousModeType = SkillPacketReplayAccess::CurrentType();
        bool previousQuest = false;
        u32 previousNaid = 0;
        UINewChatWnd* previousChat = g_windowMgr.m_chatWnd;
        Restore() {
            if (CGameMode* mode = g_modeMgr.GetCurrentGameMode()) {
                previousQuest = mode->m_isOnQuest != 0;
                previousNaid = mode->m_lastNaid;
            }
        }
        ~Restore() {
            CRagConnection::instance()->Disconnect();
            g_windowMgr.CloseNpcDialogWindows();
            g_windowMgr.CloseNpcShopWindows();
            g_session.ClearNpcShopState();
            g_session.ClearInventoryItems();
            for (const ITEM_INFO& item : inventory) g_session.SetInventoryItem(item);
            if (!previousChat && g_windowMgr.m_chatWnd)g_windowMgr.DeleteWindow(g_windowMgr.m_chatWnd);
            g_windowMgr.ClearChatEvents();
            for (const UIChatEvent& event : chat)
                g_windowMgr.PushChatEvent(event.text.c_str(), event.color, event.channel, event.tick);
            if (CGameMode* mode = dynamic_cast<CGameMode*>(previousMode)) {
                mode->m_isOnQuest = previousQuest ? 1 : 0;
                mode->m_lastNaid = previousNaid;
            }
            SkillPacketReplayAccess::Restore(previousMode, previousModeType);
        }
    } restore;

    CGameModePacketRouter& mutableRouter = GetProductionGameModePacketRouter();
    mutableRouter.Clear();
    RegisterDefaultGameModePacketHandlers(mutableRouter);
    mode.m_lastNaid = 0x78563412u;
    mode.m_isOnQuest = 1;
    SkillPacketReplayAccess::SetCurrent(&mode);
    g_session.ClearNpcShopState();
    g_session.ClearInventoryItems();
    g_windowMgr.ClearChatEvents();
    TestConnection incoming;
    LoopbackPeer peer(*CRagConnection::instance());
    const auto dispatch = [&](const std::vector<u8>& bytes) {
        Require(incoming.QueueBytes(bytes), "could not queue NPC shop packet");
        const GameModePacketLoopResult result = ProcessGameModePacketLoop(mode, incoming, mutableRouter, 1);
        Require(result.packetsProcessed == 1 && result.packetsDispatched == 1
            && incoming.BufferedBytes() == 0, "NPC shop packet did not dispatch through production router");
    };
    const auto choose = [&](u32 npcId) {
        dispatch({ 0xc4, 0x00, u8(npcId), u8(npcId >> 8), u8(npcId >> 16), u8(npcId >> 24) });
        Require(g_session.m_shopNpcId == npcId && !g_session.m_shopChoicePending,
            "C4 did not establish the expected NPC chooser state");
    };
    const auto sendMsg = [&](int message, msgparam_t first = 0) {
        return mode.SendMsg(message, first, 0, 0) != 0;
    };
    const auto noOutbound = [&]() {
        Require(SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(40)) == 0,
            "rejected or local-only NPC shop action emitted outbound bytes");
    };
    const auto addBuyQuote = [&]() {
        // One C6 row: price=100, discounted price=77, type=0, item=501.
        dispatch({ 0xc6, 0x00, 0x0f, 0x00, 100, 0, 0, 0, 77, 0, 0, 0, 0, 0xf5, 0x01 });
        Require(g_session.m_shopMode == NpcShopMode::Buy && g_session.m_shopRows.size() == 1
            && g_session.m_shopRows[0].itemInfo.GetItemId() == 501,
            "C6 did not establish the quoted buy row");
    };
    const auto addSellQuote = [&]() {
        // Selected normal inventory: index2, item501, type0, identified, count5.
        dispatch({0xe8,2,26,0,2,0,0xf5,1,0,1,5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0});
        // One C7 row: index=2, base price=50, overcharge price=40.
        dispatch({ 0xc7, 0x00, 0x0e, 0x00, 2, 0, 50, 0, 0, 0, 40, 0, 0, 0 });
        Require(g_session.m_shopMode == NpcShopMode::Sell && g_session.m_shopRows.size() == 1
            && g_session.m_shopRows[0].sourceItemIndex == 2,
            "C7 did not establish the quoted sell row");
    };

    choose(0x11223344u);
    mode.m_isPlayerDead=1;Require(!sendMsg(CGameMode::GameMsg_RequestShopDealType,0),"dead player selected a shop type");mode.m_isPlayerDead=0;
    Require(!sendMsg(CGameMode::GameMsg_RequestShopDealType,2)&&!sendMsg(CGameMode::GameMsg_RequestShopDealType,255),"invalid choice type was accepted");
    Require(!sendMsg(CGameMode::GameMsg_RequestShopDealType, 0x100),
        "wide deal type was truncated and accepted as buy");
    noOutbound();
    Require(sendMsg(CGameMode::GameMsg_RequestShopDealType, 1), "valid sell choice was rejected");
    const std::vector<u8> c5 = peer.ReceiveExact(7);
    Require(c5 == std::vector<u8>({ 0xc5, 0x00, 0x44, 0x33, 0x22, 0x11, 1 }),
        "production C5 choice bytes differ");
    Require(!g_windowMgr.m_chooseSellBuyWnd || !g_windowMgr.m_chooseSellBuyWnd->m_show,
        "successful C5 left chooser visible");
    Require(g_session.m_shopChoicePending && g_session.GetNpcShopPendingResult() == NpcShopMode::None,
        "successful C5 established incorrect pending result state");
    Require(!sendMsg(CGameMode::GameMsg_RequestShopDealType,1),"pending C5 was sent twice");noOutbound();
    dispatch({ 0xc7, 0x00, 0x04, 0x00 });
    Require(!g_session.m_shopChoicePending, "C7 did not clear the pending choice");
    std::cout << "{\"case_id\":\"npc_shop_choice_boundary\",\"contract_id\":\"PK37.09\","
        << ConfiguredSourceProvenanceJson()
        << ",\"production_entry\":\"CGameMode::SendMsg -> RequestNpcShopDealType; ProcessGameModePacketLoop -> C4/C7\""
        << ",\"wide_wparam_rejected_without_bytes\":true,\"valid_c5_hex\":\"" << Hex(c5)
        << "\",\"c7_cleared_choice_pending\":true,\"outcome\":\"PASS\"}\n";
    // Start over with a fresh chooser; C4 invalidates a prior unresolved choice/result.
    choose(0x11223344u);
    Require(sendMsg(CGameMode::GameMsg_RequestShopDealType, 0), "valid buy choice was rejected");
    const std::vector<u8> c5Buy=peer.ReceiveExact(7);
    Require(c5Buy==std::vector<u8>({0xc5,0,0x44,0x33,0x22,0x11,0}),"buy C5 type bytes differ");
    addBuyQuote();
    Require(g_session.AdjustNpcShopDealBySourceRow(0, 2), "could not add a quoted buy row");

    const std::vector<NPC_SHOP_DEAL_ROW> goodBuyDeal = g_session.m_shopDealRows;
    const unsigned int goodBuyId = goodBuyDeal[0].itemInfo.GetItemId();
    g_session.m_shopDealRows[0].quantity = 0;
    Require(!sendMsg(CGameMode::GameMsg_RequestShopBuyList), "zero quantity buy row was accepted");
    g_session.m_shopDealRows[0] = goodBuyDeal[0];
    g_session.m_shopDealRows[0].quantity = 30001;
    Require(!sendMsg(CGameMode::GameMsg_RequestShopBuyList), "over-limit buy quantity was accepted");
    g_session.m_shopDealRows[0] = goodBuyDeal[0];
    g_session.m_shopDealRows[0].itemInfo.SetItemId(0x10000u);
    Require(!sendMsg(CGameMode::GameMsg_RequestShopBuyList), "wide buy item id was truncated");
    g_session.m_shopDealRows[0] = goodBuyDeal[0];
    NPC_SHOP_DEAL_ROW duplicateBuy = goodBuyDeal[0];
    g_session.m_shopDealRows.push_back(duplicateBuy);
    Require(!sendMsg(CGameMode::GameMsg_RequestShopBuyList), "duplicate buy source row was accepted");
    g_session.m_shopDealRows = goodBuyDeal;
    mode.m_isPlayerDead=1;Require(!sendMsg(CGameMode::GameMsg_RequestShopBuyList),"dead player submitted a buy");mode.m_isPlayerDead=0;
    g_session.m_shopDealRows[0].unitPrice++;
    Require(!sendMsg(CGameMode::GameMsg_RequestShopBuyList),"stale buy quote price was accepted");g_session.m_shopDealRows=goodBuyDeal;
    Require(g_session.m_shopDealRows[0].itemInfo.GetItemId() == goodBuyId, "buy validation fixture was not restored");
    noOutbound();
    Require(sendMsg(CGameMode::GameMsg_RequestShopBuyList), "valid buy transaction was rejected");
    const std::vector<u8> c8 = peer.ReceiveExact(8);
    const std::vector<u8> expectedC8 = { 0xc8, 0x00, 0x08, 0x00, 2, 0, 0xf5, 0x01 };
    Require(c8 == expectedC8, "production C8 amount/item-ID bytes differ");
    Require(!sendMsg(CGameMode::GameMsg_RequestShopBuyList), "repeated buy submit resent after success");
    Require(g_session.GetNpcShopPendingResult() == NpcShopMode::Buy && !mode.m_isOnQuest
        && g_session.m_shopDealRows.empty(), "successful C8 did not finish editable deal and release quest");
    const size_t chatBeforeWrongFamily = g_windowMgr.GetChatEvents().size();
    dispatch({ 0xcb, 0x00, 0 });
    Require(g_session.GetNpcShopPendingResult() == NpcShopMode::Buy
        && g_windowMgr.GetChatEvents().size() == chatBeforeWrongFamily,
        "wrong result family consumed buy pending state or posted chat");
    dispatch({ 0xca, 0x00, 0 });
    Require(g_session.GetNpcShopPendingResult() == NpcShopMode::None
        && g_windowMgr.GetChatEvents().size() == chatBeforeWrongFamily + 1,
        "matching CA result did not consume pending state and post result chat");
    const size_t chatAfterResult = g_windowMgr.GetChatEvents().size();
    dispatch({ 0xca, 0x00, 0 });
    Require(g_windowMgr.GetChatEvents().size() == chatAfterResult, "unsolicited CA posted a duplicate result message");
    std::cout << "{\"case_id\":\"npc_shop_choice_buy_result\",\"contract_id\":\"PK37.09\","
        << ConfiguredSourceProvenanceJson()
        << ",\"production_entry\":\"CGameMode::SendMsg -> RequestNpcShopDealType/RequestNpcShopPurchaseList -> CRagConnection::SendPacket; ProcessGameModePacketLoop -> C6/CA/CB\""
        << ",\"c5_hex\":\"" << Hex(c5Buy) << "\",\"c8_hex\":\"" << Hex(c8)
        << "\",\"invalid_buy_rows_rejected\":5,\"dead_player_rejected\":true,\"wrong_family_ignored\":true,\"matching_result_consumed_once\":true,\"outcome\":\"PASS\"}\n";

    // Sell must be validated against the unchanged C7 quote and current inventory snapshot.
    choose(0x55667788u);
    Require(sendMsg(CGameMode::GameMsg_RequestShopDealType, 1), "valid sell choice was rejected");
    (void)peer.ReceiveExact(7);
    addSellQuote();
    Require(g_session.AdjustNpcShopDealBySourceRow(0, 2), "could not add a quoted sell row");
    const NPC_SHOP_DEAL_ROW goodSellDeal = g_session.m_shopDealRows[0];
    g_session.m_shopDealRows[0].sourceItemIndex = 0x10000u;
    Require(!sendMsg(CGameMode::GameMsg_RequestShopSellList), "wide sell index was truncated");
    g_session.m_shopDealRows[0] = goodSellDeal;
    g_session.m_shopDealRows[0].quantity = 30001;
    Require(!sendMsg(CGameMode::GameMsg_RequestShopSellList), "over-limit sell quantity was accepted");
    g_session.m_shopDealRows[0] = goodSellDeal;
    g_session.m_shopDealRows[0].sourceItemIndex = 502;
    Require(!sendMsg(CGameMode::GameMsg_RequestShopSellList), "stale sell source index was accepted");
    g_session.m_shopDealRows[0] = goodSellDeal;
    ITEM_INFO changed = *g_session.GetInventoryItemByIndex(2); changed.m_refiningLevel++;
    g_session.SetInventoryItem(changed);
    Require(!sendMsg(CGameMode::GameMsg_RequestShopSellList), "changed sell metadata passed the C7 quote guard");
    changed.m_refiningLevel--; g_session.SetInventoryItem(changed);
    changed.m_slot[3]=1;g_session.SetInventoryItem(changed);
    Require(!sendMsg(CGameMode::GameMsg_RequestShopSellList),"replaced card metadata was sold from an old quote");
    changed.m_slot[3]=0;changed.m_num=1;g_session.SetInventoryItem(changed);
    Require(!sendMsg(CGameMode::GameMsg_RequestShopSellList),"sell quantity exceeded current stock");
    changed.m_num=5;changed.m_wearLocation=1;g_session.SetInventoryItem(changed);
    Require(!sendMsg(CGameMode::GameMsg_RequestShopSellList),"worn inventory item was sold");
    changed.m_wearLocation=0;g_session.SetInventoryItem(changed);
    g_session.m_shopDealRows.push_back(goodSellDeal);
    Require(!sendMsg(CGameMode::GameMsg_RequestShopSellList),"duplicate sell index was accepted");g_session.m_shopDealRows.resize(1);
    mode.m_isPlayerDead=1;Require(!sendMsg(CGameMode::GameMsg_RequestShopSellList),"dead player submitted a sell");mode.m_isPlayerDead=0;
    g_session.m_shopDealRows[0] = goodSellDeal;
    noOutbound();
    Require(sendMsg(CGameMode::GameMsg_RequestShopSellList), "valid sell transaction was rejected");
    const std::vector<u8> c9 = peer.ReceiveExact(8);
    const std::vector<u8> expectedC9 = { 0xc9, 0x00, 0x08, 0x00, 2, 0, 2, 0 };
    Require(c9 == expectedC9, "production C9 index/amount bytes differ");
    Require(g_session.GetNpcShopPendingResult() == NpcShopMode::Sell && !mode.m_isOnQuest,
        "successful C9 did not track pending sell result and release quest");
    const int inventoryCountBeforeResult = g_session.GetInventoryItemByIndex(2)->m_num;
    dispatch({ 0xcb, 0x00, 1 });
    Require(g_session.GetNpcShopPendingResult() == NpcShopMode::None,
        "matching CB failure did not consume pending sell result");
    Require(g_session.GetInventoryItemByIndex(2)->m_num == inventoryCountBeforeResult,
        "shop result optimistically changed inventory count");
    std::cout << "{\"case_id\":\"npc_shop_sell_result\",\"contract_id\":\"PK37.09\","
        << ConfiguredSourceProvenanceJson()
        << ",\"production_entry\":\"CGameMode::SendMsg -> RequestNpcShopSellList -> CRagConnection::SendPacket; ProcessGameModePacketLoop -> C7/CB\""
        << ",\"c7_quote_rows\":1,\"invalid_sell_rows_rejected\":8,\"dead_player_rejected\":true,\"c9_hex\":\"" << Hex(c9)
        << "\",\"inventory_unchanged_until_authoritative_update\":true,\"outcome\":\"PASS\"}\n";

    // Each selected normal-shop result consumes one successful submission.
    const char* buyMessages[]={"The deal has successfully completed.","You do not have enough zeny.",
        "You cannot carry more items because you are overweight.","You cannot carry any more items."};
    for(u8 result:{u8(0),u8(1),u8(2),u8(3),u8(255)}) {
        choose(0x11223344u);addBuyQuote();Require(g_session.AdjustNpcShopDealBySourceRow(0,1),"result buy setup failed");
        Require(sendMsg(CGameMode::GameMsg_RequestShopBuyList),"result buy send failed");(void)peer.ReceiveExact(8);
        const size_t before=g_windowMgr.GetChatEvents().size();dispatch({0xca,0,result});
        Require(g_session.GetNpcShopPendingResult()==NpcShopMode::None,"CA did not finish its pending transaction");
        Require(g_windowMgr.GetChatEvents().size()==before+(result<4?1u:0u),"CA result message count differs");
        if(result<4) {
            const auto& event=g_windowMgr.GetChatEvents().back();
            Require(event.text==buyMessages[result]&&event.channel==6&&event.color==(result==0?0x00ffff00u:0x000000ffu),
                "CA result text/color/channel differs");
        }
    }
    for(u8 result:{u8(0),u8(1),u8(255)}) {
        choose(0x55667788u);addSellQuote();Require(g_session.AdjustNpcShopDealBySourceRow(0,1),"result sell setup failed");
        Require(sendMsg(CGameMode::GameMsg_RequestShopSellList),"result sell send failed");(void)peer.ReceiveExact(8);
        const size_t before=g_windowMgr.GetChatEvents().size();dispatch({0xcb,0,result});
        Require(g_session.GetNpcShopPendingResult()==NpcShopMode::None,"CB did not finish its pending transaction");
        Require(g_windowMgr.GetChatEvents().size()==before+(result<2?1u:0u),"CB result message count differs");
        if(result<2) {
            const auto& event=g_windowMgr.GetChatEvents().back();
            Require(event.text==(result==0?buyMessages[0]:"The deal has failed.")&&event.channel==6
                &&event.color==(result==0?0x00ffff00u:0x000000ffu),"CB result text/color/channel differs");
        }
        Require(g_session.GetInventoryItemByIndex(2)->m_num==5,"CB optimistically changed inventory");
    }
    choose(0x11223344u);addBuyQuote();Require(g_session.AdjustNpcShopDealBySourceRow(0,1),"stale CA setup failed");
    Require(sendMsg(CGameMode::GameMsg_RequestShopBuyList),"stale CA submit failed");(void)peer.ReceiveExact(8);
    choose(0x01020304u);const size_t beforeStale=g_windowMgr.GetChatEvents().size();dispatch({0xca,0,0});
    Require(g_session.m_shopNpcId==0x01020304u&&mode.m_isOnQuest&&g_windowMgr.m_chooseSellBuyWnd->m_show
        &&g_windowMgr.GetChatEvents().size()==beforeStale+1,"older CA closed or unlocked a newer chooser");
    // A later same-family transaction must not replace the old expected reply.
    for(int request=0;request<2;++request) {
        choose(0x11223344u);addBuyQuote();Require(g_session.AdjustNpcShopDealBySourceRow(0,1),"FIFO setup failed");
        Require(sendMsg(CGameMode::GameMsg_RequestShopBuyList),"FIFO submission failed");(void)peer.ReceiveExact(8);
    }
    choose(0x01020304u);dispatch({0xca,0,0});
    Require(g_session.GetNpcShopPendingResult()==NpcShopMode::Buy,"older CA consumed the later expected reply");
    dispatch({0xca,0,1});
    Require(g_session.GetNpcShopPendingResult()==NpcShopMode::None&&g_session.m_shopNpcId==0x01020304u
        &&mode.m_isOnQuest&&g_windowMgr.m_chooseSellBuyWnd->m_show,"FIFO results damaged the newer chooser");
    std::cout<<"{\"case_id\":\"npc_shop_result_ownership\",\"contract_id\":\"PK37.09\","
        <<ConfiguredSourceProvenanceJson()<<",\"buy_codes\":[0,1,2,3,255],\"sell_codes\":[0,1,255],"
        <<"\"older_ca_preserves_new_chooser\":true,\"same_family_fifo_verified\":true,\"outcome\":\"PASS\"}\n";

    // Synthetic source state isolates the signed request-length boundary. It
    // does not claim a server can emit this many C6 rows or legal shop items.
    std::vector<NPC_SHOP_ROW> lengthQuotes;std::vector<NPC_SHOP_DEAL_ROW> lengthDeals;
    for(unsigned int id=1;id<=8191;++id) {
        NPC_SHOP_ROW row{};row.itemInfo.SetItemId(id);row.itemInfo.m_itemType=0;row.price=row.secondaryPrice=1;
        lengthQuotes.push_back(row);NPC_SHOP_DEAL_ROW deal{};deal.itemInfo=row.itemInfo;
        deal.itemInfo.m_num=deal.quantity=1;deal.unitPrice=1;lengthDeals.push_back(deal);
    }
    g_session.SetNpcShopRows(0x11223344,NpcShopMode::Buy,lengthQuotes);g_session.m_shopDealRows=lengthDeals;
    Require(!sendMsg(CGameMode::GameMsg_RequestShopBuyList)&&g_session.m_shopDealRows.size()==8191,
        "signed-negative length frame was emitted or changed the deal");noOutbound();
    g_session.m_shopRows.resize(8190);g_session.m_shopDealRows.resize(8190);
    Require(sendMsg(CGameMode::GameMsg_RequestShopBuyList),"largest aligned positive length frame was rejected");
    const auto maximum=peer.ReceiveExact(32764);
    Require(std::vector<u8>(maximum.begin(),maximum.begin()+4)==std::vector<u8>({0xc8,0,0xfc,0x7f})
        &&std::vector<u8>(maximum.end()-4,maximum.end())==std::vector<u8>({1,0,0xfe,0x1f}),"maximum positive frame bounds/last row differ");
    dispatch({0xca,0,0});Require(g_session.GetNpcShopPendingResult()==NpcShopMode::None,"maximum frame receipt was not consumed");
    std::cout<<"{\"case_id\":\"npc_shop_positive_length_boundary\",\"contract_id\":\"PK37.09\","
        <<ConfiguredSourceProvenanceJson()<<",\"synthetic_quote_source\":true,\"rejected_rows\":8191,"
        <<"\"captured_bytes\":32764,\"header_hex\":\"c800fc7f\",\"last_row_hex\":\"0100fe1f\",\"outcome\":\"PASS\"}\n";

    // Local cancellation is intentionally packetless in all three NPC-shop contexts.
    choose(0x01020304u);
    Require(sendMsg(CGameMode::GameMsg_RequestShopCancel), "chooser cancellation failed");
    noOutbound();
    Require(g_session.m_shopNpcId == 0 && !g_session.m_shopChoicePending && !mode.m_isOnQuest,
        "chooser cancellation did not clear local NPC state");
    choose(0x01020304u);
    Require(sendMsg(CGameMode::GameMsg_RequestShopDealType, 0), "buy choice for cancel case failed");
    (void)peer.ReceiveExact(7);
    addBuyQuote();
    Require(g_session.AdjustNpcShopDealBySourceRow(0, 1), "buy cancel fixture deal setup failed");
    Require(sendMsg(CGameMode::GameMsg_RequestShopCancel), "buy transaction cancellation failed");
    noOutbound();
    choose(0x01020304u);
    Require(sendMsg(CGameMode::GameMsg_RequestShopDealType, 1), "sell choice for cancel case failed");
    (void)peer.ReceiveExact(7);
    addSellQuote();
    Require(g_session.AdjustNpcShopDealBySourceRow(0, 1), "sell cancel fixture deal setup failed");
    Require(sendMsg(CGameMode::GameMsg_RequestShopCancel), "sell transaction cancellation failed");
    noOutbound();
    g_session.SetNpcShopChoice(0x01020304u);
    NPC_SHOP_ROW disconnectedQuote{};
    disconnectedQuote.itemInfo.SetItemId(501);
    disconnectedQuote.itemInfo.m_itemType = 0;
    disconnectedQuote.price = 100;
    disconnectedQuote.secondaryPrice = 77;
    g_session.SetNpcShopRows(0x01020304u, NpcShopMode::Buy, { disconnectedQuote });
    Require(g_session.AdjustNpcShopDealBySourceRow(0, 1), "offline submit fixture deal setup failed");
    mode.m_isOnQuest = 1;
    g_windowMgr.MakeWindow(UIWindowMgr::WID_ITEMSHOPWND);
    g_windowMgr.MakeWindow(UIWindowMgr::WID_ITEMPURCHASEWND);
    CRagConnection::instance()->Disconnect();
    Require(!sendMsg(CGameMode::GameMsg_RequestShopBuyList), "disconnected C8 submission unexpectedly succeeded");
    Require(g_session.m_shopMode == NpcShopMode::Buy && g_session.m_shopDealRows.size() == 1
        && mode.m_isOnQuest && g_windowMgr.m_itemShopWnd && g_windowMgr.m_itemShopWnd->m_show
        && g_windowMgr.m_itemPurchaseWnd && g_windowMgr.m_itemPurchaseWnd->m_show,
        "disconnected C8 failure mutated deal, panes, or quest state");
    Require(sendMsg(CGameMode::GameMsg_RequestShopCancel), "offline local NPC-shop cancellation failed");
    Require(g_session.m_shopNpcId == 0 && !g_session.m_shopChoicePending,
        "offline cancellation retained NPC chooser state");
    choose(0x01020304u);
    Require(!sendMsg(CGameMode::GameMsg_RequestShopDealType,0)&&!g_session.m_shopChoicePending
        &&g_windowMgr.m_chooseSellBuyWnd->m_show&&mode.m_isOnQuest,"offline C5 destroyed chooser state");
    addSellQuote();Require(g_session.AdjustNpcShopDealBySourceRow(0,2),"offline C9 setup failed");
    Require(!sendMsg(CGameMode::GameMsg_RequestShopSellList)&&g_session.m_shopDealRows.size()==1
        &&g_session.m_shopDealRows[0].quantity==2&&g_windowMgr.m_itemSellWnd->m_show&&mode.m_isOnQuest,
        "offline C9 destroyed editable deal state");
    Require(sendMsg(CGameMode::GameMsg_RequestShopCancel),"offline sell cancel failed");
    std::cout << "{\"case_id\":\"npc_shop_local_cancel\",\"contract_id\":\"PK37.09\","
        << ConfiguredSourceProvenanceJson()
        << ",\"production_entry\":\"CGameMode::SendMsg -> RequestNpcShopCancel -> local NPC dialog/shop cleanup\""
        << ",\"choice_buy_sell_cancel_outbound_bytes\":0,\"offline_submit_preserved_deal_ui_quest\":true"
        << ",\"offline_cancel_succeeded\":true,\"outcome\":\"PASS\"}\n";
}
