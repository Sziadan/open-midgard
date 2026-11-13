void CartItemWireFieldsReplay()
{
    SetCase("cart_item_wire_fields_20080910", "PK09.04",
        "PK09-selected-20080910-clif-itemlist-fields", "ProcessGameModePacketLoop -> TryReadPacket -> production router -> Commerce::Receive");
    struct CommerceStateRestore {
        ro::commerce::State value = ro::commerce::state;
        ~CommerceStateRestore() { ro::commerce::state = std::move(value); }
    } restore;
    ro::commerce::Reset();

    // Literal rAthena 20080910 layout: 4-byte header + EQUIPITEM_INFO[26].
    // Offsets from packets_struct.hpp/clif.cpp: wear state +8, cards +12..19,
    // HireExpireDate +20..23, bindOnEquipType +24..25.
    const std::vector<u8> equip = {
        0xD2, 0x02, 0x1E, 0x00,
        0x02, 0x00, 0xD2, 0x04, 0x05, 0x01,
        0xA0, 0x00, 0x00, 0x20, 0x01, 0x07,
        0x11, 0x11, 0x22, 0x22, 0x33, 0x33, 0x44, 0x44,
        0x78, 0x56, 0x34, 0x12, 0x02, 0x00,
    };
    // Literal NORMALITEM_INFO[22], including four cards and 32-bit expiry.
    const std::vector<u8> normal = {
        0xE9, 0x02, 0x1A, 0x00,
        0x65, 0x00, 0xD3, 0x04, 0x03, 0x01,
        0x03, 0x00, 0x02, 0x00,
        0xA1, 0xA1, 0xB2, 0xB2, 0xC3, 0xC3, 0xD4, 0xD4,
        0x89, 0x67, 0x45, 0x23,
    };
    const std::vector<u8> malformedEquip = {
        0xD2, 0x02, 0x1D, 0x00,
        0x02, 0x00, 0xD2, 0x04, 0x05, 0x01,
        0xA0, 0x00, 0x00, 0x20, 0x01, 0x07,
        0x11, 0x11, 0x22, 0x22, 0x33, 0x33, 0x44, 0x44,
        0x78, 0x56, 0x34, 0x12, 0x02,
    };
    const std::vector<u8> emptyEquip = { 0xD2, 0x02, 0x04, 0x00 };

    CGameMode mode;
    TestConnection connection;
    CGameModePacketRouter& router = GetProductionGameModePacketRouter();
    router.Clear();
    RegisterDefaultGameModePacketHandlers(router);
    const auto dispatchFrame = [&](const std::vector<u8>& frame, const char* description) {
        Require(connection.QueueBytes(frame), std::string("could not queue ") + description);
        const GameModePacketLoopResult result = ProcessGameModePacketLoop(mode, connection, router, 1);
        Require(result.packetsProcessed == 1 && result.packetsDispatched == 1,
            std::string("production framing/router did not dispatch ") + description);
        Require(connection.BufferedBytes() == 0, std::string(description) + " left receive bytes queued");
    };

    dispatchFrame(equip, "selected 0x02D2 equip cart fixture");
    const auto equipItem = ro::commerce::state.cartItems.find(2);
    Require(equipItem != ro::commerce::state.cartItems.end(), "0x02D2 equip cart item was not stored");
    const ITEM_INFO& e = equipItem->second;
    Require(e.GetItemId() == 0x04D2 && e.m_itemType == 5 && e.m_isIdentified == 1
        && e.m_num == 1 && e.m_location == 0x00A0 && e.m_wearLocation == 0x2000,
        "0x02D2 equip cart fixed fields or WearState differ from literal wire values");
    Require(e.m_isDamaged == 1 && e.m_refiningLevel == 7
        && e.m_slot[0] == 0x1111 && e.m_slot[1] == 0x2222
        && e.m_slot[2] == 0x3333 && e.m_slot[3] == 0x4444
        && e.m_deleteTime == 0x12345678 && e.m_isYours == 2,
        "0x02D2 equip cart cards, expiry or bindOnEquipType differ from literal wire values");

    dispatchFrame(normal, "selected 0x02E9 normal cart fixture");
    const auto normalItem = ro::commerce::state.cartItems.find(101);
    Require(normalItem != ro::commerce::state.cartItems.end(), "0x02E9 normal cart item was not stored");
    const ITEM_INFO& n = normalItem->second;
    Require(n.GetItemId() == 0x04D3 && n.m_itemType == 3 && n.m_isIdentified == 1
        && n.m_num == 3 && n.m_location == 2 && n.m_wearLocation == 0,
        "0x02E9 normal cart fixed fields differ from literal wire values");
    Require(n.m_slot[0] == 0xA1A1 && n.m_slot[1] == 0xB2B2
        && n.m_slot[2] == 0xC3C3 && n.m_slot[3] == 0xD4D4
        && n.m_deleteTime == 0x23456789,
        "0x02E9 normal cart cards or expiry differ from literal wire values");

    dispatchFrame(malformedEquip, "malformed 0x02D2 cart fixture");
    Require(ro::commerce::state.cartItems.count(2) == 1
        && ro::commerce::state.cartItems.count(101) == 1,
        "malformed 0x02D2 cart list changed existing cart contents");
    Require(ro::commerce::state.cartItems.at(2).m_wearLocation == 0x2000
        && ro::commerce::state.cartItems.at(2).m_isYours == 2
        && ro::commerce::state.cartItems.at(101).m_num == 3,
        "malformed cart list partially overwrote prior field values");
    auto invalidFinal = normal;
    invalidFinal[4] = 100;
    invalidFinal.insert(invalidFinal.end(), normal.begin() + 4, normal.end());
    invalidFinal[2] = static_cast<u8>(invalidFinal.size());
    invalidFinal[26] = 100;
    dispatchFrame(invalidFinal, "duplicate final cart index");
    invalidFinal[26] = 102;
    dispatchFrame(invalidFinal, "out-of-range final cart index");
    Require(ro::commerce::state.cartItems.size() == 2 && !ro::commerce::state.cartItems.count(100)
        && ro::commerce::state.cartItems.count(101) && ro::commerce::state.cartItems.count(2),
        "invalid final cart rows replaced an existing snapshot");
    dispatchFrame(emptyEquip, "empty 0x02D2 cart fixture");
    Require(ro::commerce::state.cartItems.count(2) == 0
        && ro::commerce::state.cartItems.count(101) == 1,
        "empty 0x02D2 cart list did not clear only equip entries");

    dispatchFrame(equip, "restore 0x02D2 equipment before normal-list isolation");
    const std::vector<u8> malformedNormal = {
        0xe9,2,25,0, 0x65,0,0xd3,4,3,1,3,0,2,0,
        0xa1,0xa1,0xb2,0xb2,0xc3,0xc3,0xd4,0xd4,0x89,0x67,0x45
    };
    dispatchFrame(malformedNormal, "malformed 0x02E9 normal cart fixture");
    Require(ro::commerce::state.cartItems.at(101).m_num == 3
        && ro::commerce::state.cartItems.at(101).m_deleteTime == 0x23456789
        && ro::commerce::state.cartItems.at(2).m_isYours == 2,
        "malformed normal cart list modified either category");
    dispatchFrame({0xe9,2,4,0}, "empty 0x02E9 normal cart fixture");
    Require(ro::commerce::state.cartItems.count(101) == 0
        && ro::commerce::state.cartItems.at(2).m_wearLocation == 0x2000,
        "empty normal cart list did not preserve equipment category");

    std::cout << "{\"case_id\":\"cart_item_wire_fields_20080910\",\"contract_id\":\"PK09.04\",\"routes\":[\"02D2\",\"02E9\"],\"equip_tail\":[\"WearState\",\"cards\",\"HireExpireDate\",\"bindOnEquipType\"],\"empty_and_malformed_lists_checked\":true,\"outcome\":\"PASS\"}\n";
}

void StorageSnapshotReplay()
{
    CGameMode mode;
    CGameModePacketRouter router;
    RegisterDefaultGameModePacketHandlers(router);
    TestConnection connection;
    const auto dispatch = [&](const std::vector<u8>& bytes) {
        Require(connection.QueueBytes(bytes), "storage snapshot queue failed");
        Require(ProcessGameModePacketLoop(mode, connection, router, 1).packetsDispatched == 1
            && !connection.BufferedBytes(), "storage snapshot did not dispatch");
    };
    const std::vector<u8> normal = {0xea, 2, 26, 0, 1, 0, 0xf5, 1, 0, 1, 5, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    const std::vector<u8> equip = {0xd1, 2, 30, 0, 0x58, 2, 0xb1, 4, 5, 1, 2, 0, 2, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    g_session.CloseStorage();
    dispatch(normal);
    dispatch(equip);
    Require(!g_session.IsStorageOpen() && g_session.GetStorageItemByIndex(600)
        && g_session.GetStorageItemByIndex(600)->m_wearLocation == 0,
        "storage rows must arrive before open and equipment must be unworn");
    dispatch({0xf2, 0, 2, 0, 0x58, 2});
    auto replacement = normal;
    replacement[4] = 2;
    auto bad = replacement;
    bad.insert(bad.end(), normal.begin() + 4, normal.end());
    bad[2] = static_cast<u8>(bad.size());
    bad[32] = 0;
    dispatch(bad);
    Require(g_session.IsStorageOpen() && g_session.GetStorageItems().size() == 2
        && g_session.GetStorageItemByIndex(1) && !g_session.GetStorageItemByIndex(2),
        "invalid final storage quantity replaced the current list");
    dispatch(replacement);
    dispatch({0xf2, 0, 2, 0, 0x58, 2});
    Require(g_session.GetStorageItems().size() == 2 && !g_session.GetStorageItemByIndex(1)
        && g_session.GetStorageItemByIndex(2) && g_session.GetStorageItemByIndex(600),
        "storage replacement or count-only update changed the opposite category");
    dispatch({0xd1, 2, 4, 0});
    Require(g_session.GetStorageItemByIndex(2) && !g_session.GetStorageItemByIndex(600),
        "empty equipment storage list cleared the wrong category");
    dispatch({0xf2, 0, 0, 0, 0x58, 2});
    Require(g_session.IsStorageOpen() && g_session.GetStorageItems().empty(),
        "zero-count refresh without category lists retained stale storage");
    dispatch(normal);
    dispatch({0xf8, 0});
    Require(!g_session.IsStorageOpen() && g_session.GetStorageItems().empty(),
        "storage close retained stale items");
    dispatch({0xf2, 0, 0, 0, 0x58, 2});
    Require(g_session.IsStorageOpen() && g_session.GetStorageItems().empty(),
        "empty reopen inherited closed storage items");
    dispatch({0xf8, 0});
}
