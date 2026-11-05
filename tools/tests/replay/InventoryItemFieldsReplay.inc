void InventoryItemFieldsReplay()
{
    SetCase("inventory_item_wire_fields_20080910", "PK09.04",
        "PK09-selected-20080910-inventory-item-list-fields",
        "ProcessGameModePacketLoop -> TryReadPacket -> production router -> inventory handlers");

    struct StateRestore {
        std::vector<ITEM_INFO> inventory{g_session.GetInventoryItems().begin(), g_session.GetInventoryItems().end()};
        ro::commerce::State commerce = ro::commerce::state;
        int weapon=g_session.m_playerWeapon,shield=g_session.m_playerShield;
        int bottom=g_session.m_playerAccessory,top=g_session.m_playerAccessory2,mid=g_session.m_playerAccessory3;
        CHARACTER_INFO* character=g_session.GetMutableSelectedCharacterInfo();
        CHARACTER_INFO characterValue=character?*character:CHARACTER_INFO{};
        ~StateRestore() {
            g_session.ClearInventoryItems();
            for (const ITEM_INFO& item : inventory) g_session.SetInventoryItem(item);
            ro::commerce::state = std::move(commerce);
            g_session.m_playerWeapon=weapon;g_session.m_playerShield=shield;
            g_session.m_playerAccessory=bottom;g_session.m_playerAccessory2=top;g_session.m_playerAccessory3=mid;
            if(character)*character=characterValue;
            GetProductionGameModePacketRouter().Clear();
            RegisterDefaultGameModePacketHandlers(GetProductionGameModePacketRouter());
        }
    } restore;

    // Independently written 20080910 NORMALITEM_INFO[22] row (opcode + length + row).
    const std::vector<u8> normal = {
        0xE8,0x02,0x1A,0x00, 0x31,0x02,0x57,0x13,0x03,0x01,
        0x09,0x00,0x34,0x12, 0xA5,0xA4,0xB7,0xB6,0xC9,0xC8,0xDB,0xDA,
        0xEF,0xCD,0xAB,0x89
    };
    // Independently written 20080910 EQUIPITEM_INFO[26] row.
    const std::vector<u8> equip = {
        0xD0,0x02,0x1E,0x00, 0x32,0x02,0x68,0x24,0x05,0x01,
        0xA0,0x00,0x00,0x20,0x01,0x06, 0x10,0x20,0x30,0x40,0x50,0x60,0x70,0x80,
        0x78,0x56,0x34,0x12,0x02,0x00
    };
    const std::vector<u8> malformedNormal = {
        0xE8,0x02,0x19,0x00, 0x31,0x02,0x57,0x13,0x03,0x01,0x09,0x00,0x34,0x12,
        0xA5,0xA4,0xB7,0xB6,0xC9,0xC8,0xDB,0xDA,0xEF,0xCD,0xAB
    };
    const std::vector<u8> malformedEquip = {
        0xD0,0x02,0x1D,0x00, 0x32,0x02,0x68,0x24,0x05,0x01,
        0xA0,0x00,0x00,0x20,0x01,0x06,0x10,0x20,0x30,0x40,0x50,0x60,0x70,0x80,
        0x78,0x56,0x34,0x12,0x02
    };
    const std::vector<u8> emptyNormal = {0xE8,0x02,0x04,0x00};
    const std::vector<u8> emptyEquip = {0xD0,0x02,0x04,0x00};
    const std::vector<u8> ammo = {
        0xE8,0x02,0x1A,0x00, 0x40,0x02,0xAA,0x05,0x0A,0x01,
        0x01,0x00,0x00,0x80, 0x01,0x00,0x02,0x00,0x03,0x00,0x04,0x00,0x11,0x22,0x33,0x44
    };
    const std::vector<u8> ammoEquip = {0x3C,0x01,0x40,0x02};
    const std::vector<u8> secondNormal = {
        0xE8,0x02,0x1A,0x00, 0x41,0x02,0xAB,0x05,0x0A,0x01,
        0x02,0x00,0x00,0x80, 0x11,0x11,0x22,0x22,0x33,0x33,0x44,0x44,0x55,0x66,0x77,0x88
    };
    const std::vector<u8> clearAmmo = {0xac,0x00,0x41,0x02,0x00,0x80,0x01};

    CGameMode mode;
    TestConnection connection;
    CGameModePacketRouter& router = GetProductionGameModePacketRouter();
    router.Clear(); RegisterDefaultGameModePacketHandlers(router);
    const auto dispatch = [&](const std::vector<u8>& bytes, const char* label) {
        Require(connection.QueueBytes(bytes), std::string("queue failed: ")+label);
        const auto result = ProcessGameModePacketLoop(mode, connection, router, 1);
        Require(result.packetsProcessed==1 && result.packetsDispatched==1 && connection.BufferedBytes()==0,
            std::string("framing/router failed: ")+label);
    };
    const auto has = [](unsigned index) {
        return std::find_if(g_session.GetInventoryItems().begin(),g_session.GetInventoryItems().end(),
            [=](const ITEM_INFO& x){return x.m_itemIndex==index;});
    };

    g_session.ClearInventoryItems(); ro::commerce::Reset();
    dispatch(normal,"02E8 normal literal");
    auto ni=has(0x0231); Require(ni!=g_session.GetInventoryItems().end(),"normal row missing");
    Require(ni->GetItemId()==0x1357 && ni->m_itemType==3 && ni->m_isIdentified==1 && ni->m_num==9
        && ni->m_location==0x1234 && ni->m_wearLocation==0 && ni->m_isDamaged==0 && ni->m_refiningLevel==0
        && ni->m_slot[0]==0xA4A5 && ni->m_slot[1]==0xB6B7 && ni->m_slot[2]==0xC8C9 && ni->m_slot[3]==0xDADB
        && static_cast<u32>(ni->m_deleteTime)==0x89ABCDEFu && ni->m_isYours==0,"normal row fields differ from literal bytes");
    dispatch(equip,"02D0 equip literal");
    auto ei=has(0x0232); Require(ei!=g_session.GetInventoryItems().end(),"equip row missing");
    Require(ei->GetItemId()==0x2468 && ei->m_itemType==5 && ei->m_isIdentified==1 && ei->m_num==1
        && ei->m_location==0x00A0 && ei->m_wearLocation==0x2000 && ei->m_isDamaged==1 && ei->m_refiningLevel==6
        && ei->m_slot[0]==0x2010 && ei->m_slot[1]==0x4030 && ei->m_slot[2]==0x6050 && ei->m_slot[3]==0x8070
        && ei->m_deleteTime==0x12345678 && ei->m_isYours==2,"equip row fields differ from literal bytes");

    dispatch(equip,"equip replacement order A"); dispatch(normal,"normal replacement order A");
    dispatch(malformedNormal,"malformed normal"); dispatch(malformedEquip,"malformed equip");
    ni=has(0x0231); ei=has(0x0232);
    Require(ni!=g_session.GetInventoryItems().end() && ni->m_num==9 && static_cast<u32>(ni->m_deleteTime)==0x89ABCDEFu
        && ei!=g_session.GetInventoryItems().end() && ei->m_wearLocation==0x2000 && ei->m_isYours==2,
        "malformed list partially changed prior inventory snapshot");
    dispatch(emptyEquip,"empty equip");
    Require(has(0x0231)!=g_session.GetInventoryItems().end() && has(0x0232)==g_session.GetInventoryItems().end(),
        "empty equipment list did not preserve normal category");
    dispatch(equip,"equip for empty normal"); dispatch(emptyNormal,"empty normal");
    Require(has(0x0232)!=g_session.GetInventoryItems().end() && has(0x0231)==g_session.GetInventoryItems().end(),
        "empty normal list did not preserve equipment category");

    dispatch(ammo,"normal ammo row");
    auto ai=has(0x0240);Require(ai!=g_session.GetInventoryItems().end() && ai->m_location==0x8000 && ai->m_wearLocation==0,
        "normal snapshot confused allowed ammunition location with actual wear state");
    dispatch(ammoEquip,"013C equip ammo");
    ai=has(0x0240); Require(ai!=g_session.GetInventoryItems().end() && ai->m_itemType==10
        && ai->m_location==0x8000 && ai->m_wearLocation==0x8000,"ammo equip did not set actual wear while retaining slot allowance");
    dispatch(equip,"equipment-list refresh");
    ai=has(0x0240); Require(ai!=g_session.GetInventoryItems().end() && ai->m_wearLocation==0x8000,
        "equipment-list refresh discarded equipped ammo state");
    dispatch(secondNormal,"replace normal with unrelated item");
    Require(has(0x0240)==g_session.GetInventoryItems().end(),"normal replacement retained an omitted old ammo row");
    dispatch({0x3c,0x01,0x41,0x02},"013C equip replacement ammo");
    auto replacementAmmo=has(0x0241);
    Require(replacementAmmo!=g_session.GetInventoryItems().end() && replacementAmmo->m_wearLocation==0x8000,
        "replacement ammunition did not equip");
    dispatch(clearAmmo,"00AC successful ammunition unequip");
    ai=has(0x0240); auto ai2=has(0x0241);
    Require(ai==g_session.GetInventoryItems().end()
        && ai2!=g_session.GetInventoryItems().end() && ai2->m_location==0x8000 && ai2->m_wearLocation==0,
        "successful ammunition unequip did not retain its row while clearing worn state");

    std::cout << "{\"case_id\":\"inventory_item_wire_fields_20080910\",\"contract_id\":\"PK09.04\",\"routes\":[\"02E8\",\"02D0\",\"013C\",\"00AC\"],\"normal_and_equip_fields\":true,\"category_replacement_both_orders\":true,\"malformed_atomicity\":true,\"empty_categories\":true,\"ammo_wear_refresh\":true,\"outcome\":\"PASS\"}" << std::endl;
}
