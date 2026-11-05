void InventoryMapEntryReplay()
{
    SetCase("inventory_map_entry_reset", "PK09.05",
        "PK09-selected-20080910-inventory-cart-literals",
        "CGameMode::OnInit -> inventory/cart/storage lifecycle; framed list packets via TryReadPacket and production router");

    struct StateRestore {
        CGameMode& mode;
        bool initialized = false;
        std::vector<ITEM_INFO> inventory{g_session.GetInventoryItems().begin(), g_session.GetInventoryItems().end()};
        std::vector<ITEM_INFO> storage{g_session.GetStorageItems().begin(), g_session.GetStorageItems().end()};
        const bool storageOpen = g_session.IsStorageOpen();
        const int storageCount = g_session.GetStorageCurrentCount();
        const int storageMax = g_session.GetStorageMaxCount();
        ro::commerce::State commerce = ro::commerce::state;
        CMode* currentMode = SkillPacketReplayAccess::Current();
        int currentModeType = SkillPacketReplayAccess::CurrentType();

        explicit StateRestore(CGameMode& value) : mode(value) {}
        ~StateRestore()
        {
            if (initialized) mode.OnExit();
            // OnInit temporarily installs this test mode into the global world.
            // Leave no pointer to this stack object after OnExit.
            g_world.m_curMode = nullptr;
            SkillPacketReplayAccess::Restore(currentMode, currentModeType);
            g_session.ClearInventoryItems();
            for (const ITEM_INFO& item : inventory) g_session.SetInventoryItem(item);
            g_session.CloseStorage();
            if (storageOpen) {
                g_session.OpenStorage(storageCount, storageMax);
                for (const ITEM_INFO& item : storage) g_session.SetStorageItem(item);
            }
            ro::commerce::state = std::move(commerce);
            GetProductionGameModePacketRouter().Clear();
            RegisterDefaultGameModePacketHandlers(GetProductionGameModePacketRouter());
        }
    };

    CGameMode mode;
    StateRestore restore(mode);
    TestConnection connection;
    CGameModePacketRouter& router = GetProductionGameModePacketRouter();
    router.Clear();
    RegisterDefaultGameModePacketHandlers(router);
    const auto dispatchFrame = [&](const std::vector<u8>& frame, const char* label) {
        Require(connection.QueueBytes(frame), std::string("could not queue ") + label);
        const GameModePacketLoopResult result = ProcessGameModePacketLoop(mode, connection, router, 1);
        Require(result.packetsProcessed == 1 && result.packetsDispatched == 1,
            std::string("production framing/router did not dispatch ") + label);
        Require(connection.BufferedBytes() == 0, std::string(label) + " left receive bytes queued");
    };

    // Literal selected 20080910 NORMALITEM_INFO[22] and EQUIPITEM_INFO[26]
    // rows, copied from CartItemWireFieldsReplay with only the opcode changed.
    const std::vector<u8> normalInventory = {
        0xE8, 0x02, 0x1A, 0x00,
        0x24, 0x01, 0xD3, 0x04, 0x03, 0x01,
        0x03, 0x00, 0x02, 0x00,
        0xA1, 0xA1, 0xB2, 0xB2, 0xC3, 0xC3, 0xD4, 0xD4,
        0x89, 0x67, 0x45, 0x23,
    };
    const std::vector<u8> equipInventory = {
        0xD0, 0x02, 0x1E, 0x00,
        0x23, 0x01, 0xD2, 0x04, 0x05, 0x01,
        0xA0, 0x00, 0x00, 0x20, 0x01, 0x07,
        0x11, 0x11, 0x22, 0x22, 0x33, 0x33, 0x44, 0x44,
        0x78, 0x56, 0x34, 0x12, 0x02, 0x00,
    };
    const std::vector<u8> normalCart = {
        0xE9, 0x02, 0x1A, 0x00,
        0x24, 0x01, 0xD3, 0x04, 0x03, 0x01,
        0x03, 0x00, 0x02, 0x00,
        0xA1, 0xA1, 0xB2, 0xB2, 0xC3, 0xC3, 0xD4, 0xD4,
        0x89, 0x67, 0x45, 0x23,
    };
    const std::vector<u8> equipCart = {
        0xD2, 0x02, 0x1E, 0x00,
        0x23, 0x01, 0xD2, 0x04, 0x05, 0x01,
        0xA0, 0x00, 0x00, 0x20, 0x01, 0x07,
        0x11, 0x11, 0x22, 0x22, 0x33, 0x33, 0x44, 0x44,
        0x78, 0x56, 0x34, 0x12, 0x02, 0x00,
    };

    ro::commerce::Reset();
    g_session.ClearInventoryItems();
    g_session.CloseStorage();
    dispatchFrame(normalInventory, "selected 0x02E8 normal inventory row");
    dispatchFrame(equipInventory, "selected 0x02D0 equipment inventory row");
    dispatchFrame(normalCart, "selected 0x02E9 normal cart row");
    dispatchFrame(equipCart, "selected 0x02D2 equipment cart row");
    Require(g_session.GetInventoryItems().size() == 2 && ro::commerce::state.cartItems.size() == 2,
        "literal inventory/cart seed packets did not populate both item categories");

    ITEM_INFO storageItem{};
    storageItem.m_itemIndex = 0x0456;
    storageItem.SetItemId(0x0789);
    storageItem.m_num = 1;
    g_session.OpenStorage(1, 600);
    g_session.SetStorageItem(storageItem);

    ro::commerce::state.hasCart = true;
    ro::commerce::state.cart.visible = true;
    ro::commerce::state.tradeIncoming = ro::commerce::state.tradeActive = true;
    ro::commerce::state.tradeWaiting = ro::commerce::state.localLocked = true;
    ro::commerce::state.remoteLocked = ro::commerce::state.lockPending = true;
    ro::commerce::state.commitPending = ro::commerce::state.tradeAddPending = true;
    ro::commerce::state.vendingActive = ro::commerce::state.vendingPending = true;
    ro::commerce::state.mailPending = true;
    ro::commerce::state.cart.x = 637;
    ro::commerce::state.cart.y = 241;
    ro::commerce::state.trade.x = 307;
    ro::commerce::state.trade.y = 191;
    ro::commerce::state.inventoryTab = 2;
    CRagConnection::instance()->Disconnect();
    SkillPacketReplayAccess::SetCurrent(&mode);

    const auto requireReset = [&](const char* phase) {
        Require(g_session.GetNpcShopPendingResult()==NpcShopMode::None&&!g_session.m_shopChoicePending
            &&g_session.m_shopRows.empty()&&g_session.m_shopDealRows.empty(),std::string(phase)+" retained NPC shop state");
        Require(g_session.GetInventoryItems().empty(), std::string(phase) + " retained inventory rows");
        Require(ro::commerce::state.cartItems.empty() && !ro::commerce::state.hasCart,
            std::string(phase) + " retained cart rows or hasCart");
        Require(!ro::commerce::state.tradeIncoming && !ro::commerce::state.tradeActive
            && !ro::commerce::state.tradeWaiting && !ro::commerce::state.localLocked
            && !ro::commerce::state.remoteLocked && !ro::commerce::state.lockPending
            && !ro::commerce::state.commitPending && !ro::commerce::state.tradeAddPending
            && !ro::commerce::state.vendingActive && !ro::commerce::state.vendingPending
            && !ro::commerce::state.mailPending,
            std::string(phase) + " retained a trade, vending, or pending flag");
        Require(!g_session.IsStorageOpen() && g_session.GetStorageItems().empty(),
            std::string(phase) + " retained storage state or item rows");
        Require(!ro::commerce::state.cart.visible && !ro::commerce::state.trade.visible,
            std::string(phase) + " retained an open item/transaction panel");
        Require(ro::commerce::state.cart.x == 637 && ro::commerce::state.cart.y == 241
            && ro::commerce::state.trade.x == 307 && ro::commerce::state.trade.y == 191
            && ro::commerce::state.inventoryTab == 2,
            std::string(phase) + " lost local panel positions or inventory tab");
    };

    mode.m_characterNameCache[0x12345678]="Old producer";
    mode.m_characterNameReqTimer[0x12345678]=42;
    g_session.QueueNpcShopResult(NpcShopMode::Buy);g_session.m_shopChoicePending=true;
    g_session.m_shopRows.resize(1);g_session.m_shopDealRows.resize(1);
    mode.OnInit(nullptr);
    restore.initialized = true;
    requireReset("first OnInit(nullptr)");
    Require(mode.m_characterNameCache.empty() && mode.m_characterNameReqTimer.empty(),"actual map entry retained character name state");

    // A server may omit an empty equipment category. Its normal inventory and
    // normal cart snapshots must not resurrect stale equipment from prior entry.
    dispatchFrame(normalInventory, "post-entry 0x02E8 normal inventory row");
    dispatchFrame(normalCart, "post-entry 0x02E9 normal cart row");
    for (const ITEM_INFO& item : g_session.GetInventoryItems()) {
        Require(item.m_itemType != 4 && item.m_itemType != 5 && item.m_itemType != 7 && item.m_itemType != 8,
            "normal inventory snapshot left a prior equipment row");
    }
    for (const auto& entry : ro::commerce::state.cartItems) {
        Require(entry.second.m_itemType != 4 && entry.second.m_itemType != 5
            && entry.second.m_itemType != 7 && entry.second.m_itemType != 8,
            "normal cart snapshot left a prior equipment row");
    }

    mode.OnInit(nullptr);
    requireReset("retry OnInit(nullptr)");
    mode.m_characterNameCache[0x12345678]="Old producer";
    mode.m_characterNameReqTimer[0x12345678]=42;
    g_session.QueueNpcShopResult(NpcShopMode::Sell);g_session.m_shopChoicePending=true;
    g_session.m_shopRows.resize(1);g_session.m_shopDealRows.resize(1);
    mode.OnExit();
    Require(g_session.GetNpcShopPendingResult()==NpcShopMode::None&&!g_session.m_shopChoicePending
        &&g_session.m_shopRows.empty()&&g_session.m_shopDealRows.empty(),"OnExit retained NPC shop state");
    Require(mode.m_characterNameCache.empty() && mode.m_characterNameReqTimer.empty(),"actual mode exit retained character name state");
    restore.initialized = false;
    std::cout << "{\"case_id\":\"inventory_map_entry_reset\",\"contract_id\":\"PK09.05\",\"routes\":[\"02E8\",\"02D0\",\"02E9\",\"02D2\"],\"empty_equipment_category_omitted\":true,\"repeat_map_entry_idempotence\":true,\"positions_and_tab_preserved\":true,\"storage_and_transactions_reset\":true,\"outcome\":\"PASS\"}\n";
}
