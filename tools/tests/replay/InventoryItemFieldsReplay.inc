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
        int eggIndex=g_session.m_petStatus.eggIndex;
        CHARACTER_INFO* character=g_session.GetMutableSelectedCharacterInfo();
        CHARACTER_INFO characterValue=character?*character:CHARACTER_INFO{};
        ~StateRestore() {
            g_session.ClearInventoryItems();
            for (const ITEM_INFO& item : inventory) g_session.SetInventoryItem(item);
            ro::commerce::state = std::move(commerce);
            g_session.m_playerWeapon=weapon;g_session.m_playerShield=shield;
            g_session.m_playerAccessory=bottom;g_session.m_playerAccessory2=top;g_session.m_playerAccessory3=mid;
            g_session.m_petStatus.eggIndex=eggIndex;
            if(character)*character=characterValue;
            GetProductionGameModePacketRouter().Clear();
            RegisterDefaultGameModePacketHandlers(GetProductionGameModePacketRouter());
        }
    } restore;

    // Independently written 20080910 NORMALITEM_INFO[22] row (opcode + length + row).
    const std::vector<u8> normal = {
        0xE8,0x02,0x1A,0x00, 0x31,0x00,0x57,0x13,0x03,0x01,
        0x09,0x00,0x34,0x12, 0xA5,0xA4,0xB7,0xB6,0xC9,0xC8,0xDB,0xDA,
        0xEF,0xCD,0xAB,0x89
    };
    // Independently written 20080910 EQUIPITEM_INFO[26] row.
    const std::vector<u8> equip = {
        0xD0,0x02,0x1E,0x00, 0x32,0x00,0x68,0x24,0x05,0x01,
        0xA0,0x00,0x00,0x20,0x01,0x06, 0x10,0x20,0x30,0x40,0x50,0x60,0x70,0x80,
        0x78,0x56,0x34,0x12,0x02,0x00
    };
    const std::vector<u8> malformedNormal = {
        0xE8,0x02,0x19,0x00, 0x31,0x00,0x57,0x13,0x03,0x01,0x09,0x00,0x34,0x12,
        0xA5,0xA4,0xB7,0xB6,0xC9,0xC8,0xDB,0xDA,0xEF,0xCD,0xAB
    };
    const std::vector<u8> malformedEquip = {
        0xD0,0x02,0x1D,0x00, 0x32,0x00,0x68,0x24,0x05,0x01,
        0xA0,0x00,0x00,0x20,0x01,0x06,0x10,0x20,0x30,0x40,0x50,0x60,0x70,0x80,
        0x78,0x56,0x34,0x12,0x02
    };
    const std::vector<u8> emptyNormal = {0xE8,0x02,0x04,0x00};
    const std::vector<u8> emptyEquip = {0xD0,0x02,0x04,0x00};
    const std::vector<u8> ammo = {
        0xE8,0x02,0x1A,0x00, 0x40,0x00,0xAA,0x05,0x0A,0x01,
        0x01,0x00,0x00,0x80, 0x01,0x00,0x02,0x00,0x03,0x00,0x04,0x00,0x11,0x22,0x33,0x44
    };
    const std::vector<u8> ammoEquip = {0x3C,0x01,0x40,0x00};
    const std::vector<u8> secondNormal = {
        0xE8,0x02,0x1A,0x00, 0x41,0x00,0xAB,0x05,0x0A,0x01,
        0x02,0x00,0x00,0x80, 0x11,0x11,0x22,0x22,0x33,0x33,0x44,0x44,0x55,0x66,0x77,0x88
    };
    const std::vector<u8> clearAmmo = {0xac,0x00,0x41,0x00,0x00,0x80,0x01};

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
    auto ni=has(0x0031); Require(ni!=g_session.GetInventoryItems().end(),"normal row missing");
    Require(ni->GetItemId()==0x1357 && ni->m_itemType==3 && ni->m_isIdentified==1 && ni->m_num==9
        && ni->m_location==0x1234 && ni->m_wearLocation==0 && ni->m_isDamaged==0 && ni->m_refiningLevel==0
        && ni->m_slot[0]==0xA4A5 && ni->m_slot[1]==0xB6B7 && ni->m_slot[2]==0xC8C9 && ni->m_slot[3]==0xDADB
        && static_cast<u32>(ni->m_deleteTime)==0x89ABCDEFu && ni->m_isYours==0,"normal row fields differ from literal bytes");
    dispatch(equip,"02D0 equip literal");
    auto ei=has(0x0032); Require(ei!=g_session.GetInventoryItems().end(),"equip row missing");
    Require(ei->GetItemId()==0x2468 && ei->m_itemType==5 && ei->m_isIdentified==1 && ei->m_num==1
        && ei->m_location==0x00A0 && ei->m_wearLocation==0x2000 && ei->m_isDamaged==1 && ei->m_refiningLevel==6
        && ei->m_slot[0]==0x2010 && ei->m_slot[1]==0x4030 && ei->m_slot[2]==0x6050 && ei->m_slot[3]==0x8070
        && ei->m_deleteTime==0x12345678 && ei->m_isYours==2,"equip row fields differ from literal bytes");

    dispatch(equip,"equip replacement order A"); dispatch(normal,"normal replacement order A");
    dispatch(malformedNormal,"malformed normal"); dispatch(malformedEquip,"malformed equip");
    ni=has(0x0031); ei=has(0x0032);
    Require(ni!=g_session.GetInventoryItems().end() && ni->m_num==9 && static_cast<u32>(ni->m_deleteTime)==0x89ABCDEFu
        && ei!=g_session.GetInventoryItems().end() && ei->m_wearLocation==0x2000 && ei->m_isYours==2,
        "malformed list partially changed prior inventory snapshot");
    const auto pair = [](std::vector<u8> first, const std::vector<u8>& second) {
        first.insert(first.end(), second.begin() + 4, second.end());
        first[2] = static_cast<u8>(first.size());
        return first;
    };
    auto replacement = normal;
    replacement[4] = 101;
    for (const auto badIndex : {1, 101, 102}) {
        auto bad = normal;
        bad[4] = static_cast<u8>(badIndex);
        dispatch(pair(replacement, bad), "invalid or duplicate final inventory index");
    }
    auto badQuantity = normal;
    badQuantity[10] = 0xff;
    badQuantity[11] = 0xff;
    dispatch(pair(replacement, badQuantity), "negative final inventory quantity");
    auto badEquipment = equip;
    badEquipment[6] = badEquipment[7] = 0;
    auto egg = equip;
    egg[4] = 100;
    egg[6] = 0x29;
    egg[7] = 0x23;
    const int priorEggIndex = g_session.m_petStatus.eggIndex;
    dispatch(pair(egg, badEquipment), "invalid final equipment ID");
    Require(has(101)==g_session.GetInventoryItems().end() && has(100)==g_session.GetInventoryItems().end()
        && has(0x31)!=g_session.GetInventoryItems().end() && has(0x32)!=g_session.GetInventoryItems().end()
        && g_session.m_petStatus.eggIndex==priorEggIndex,
        "invalid final inventory row changed items or pet egg state");
    dispatch(replacement, "valid inventory index at upper bound");
    Require(has(101)!=g_session.GetInventoryItems().end() && has(0x31)==g_session.GetInventoryItems().end()
        && has(0x32)!=g_session.GetInventoryItems().end(), "valid replacement after malformed inventory failed");
    dispatch(normal,"restore normal fixture");
    dispatch(emptyEquip,"empty equip");
    Require(has(0x0031)!=g_session.GetInventoryItems().end() && has(0x0032)==g_session.GetInventoryItems().end(),
        "empty equipment list did not preserve normal category");
    dispatch(equip,"equip for empty normal"); dispatch(emptyNormal,"empty normal");
    Require(has(0x0032)!=g_session.GetInventoryItems().end() && has(0x0031)==g_session.GetInventoryItems().end(),
        "empty normal list did not preserve equipment category");

    dispatch(ammo,"normal ammo row");
    auto ai=has(0x0040);Require(ai!=g_session.GetInventoryItems().end() && ai->m_location==0x8000 && ai->m_wearLocation==0,
        "normal snapshot confused allowed ammunition location with actual wear state");
    dispatch(ammoEquip,"013C equip ammo");
    ai=has(0x0040); Require(ai!=g_session.GetInventoryItems().end() && ai->m_itemType==10
        && ai->m_location==0x8000 && ai->m_wearLocation==0x8000,"ammo equip did not set actual wear while retaining slot allowance");
    dispatch(equip,"equipment-list refresh");
    ai=has(0x0040); Require(ai!=g_session.GetInventoryItems().end() && ai->m_wearLocation==0x8000,
        "equipment-list refresh discarded equipped ammo state");
    dispatch(secondNormal,"replace normal with unrelated item");
    Require(has(0x0040)==g_session.GetInventoryItems().end(),"normal replacement retained an omitted old ammo row");
    dispatch({0x3c,0x01,0x41,0x00},"013C equip replacement ammo");
    auto replacementAmmo=has(0x0041);
    Require(replacementAmmo!=g_session.GetInventoryItems().end() && replacementAmmo->m_wearLocation==0x8000,
        "replacement ammunition did not equip");
    dispatch(clearAmmo,"00AC successful ammunition unequip");
    ai=has(0x0040); auto ai2=has(0x0041);
    Require(ai==g_session.GetInventoryItems().end()
        && ai2!=g_session.GetInventoryItems().end() && ai2->m_location==0x8000 && ai2->m_wearLocation==0,
        "successful ammunition unequip did not retain its row while clearing worn state");

    auto reusedIndex = normal;
    reusedIndex[4] = 0x32;
    dispatch(reusedIndex, "normal snapshot reuses a prior equipment slot");
    Require(g_session.GetInventoryItems().size()==1 && has(0x32)!=g_session.GetInventoryItems().end()
        && has(0x32)->GetItemId()==0x1357, "authoritative slot reuse kept stale equipment");

    std::cout << "{\"case_id\":\"inventory_item_wire_fields_20080910\",\"contract_id\":\"PK09.04\",\"routes\":[\"02E8\",\"02D0\",\"013C\",\"00AC\"],\"normal_and_equip_fields\":true,\"category_replacement_both_orders\":true,\"malformed_atomicity\":true,\"empty_categories\":true,\"ammo_wear_refresh\":true,\"outcome\":\"PASS\"}" << std::endl;
}

void ItemCompositionResultReplay()
{
    CGameMode mode;
    CGameModePacketRouter router;
    RegisterDefaultGameModePacketHandlers(router);
    TestConnection input;
    const auto dispatch = [&](const std::vector<u8>& bytes) {
        Require(input.QueueBytes(bytes) && ProcessGameModePacketLoop(mode,input,router,1).packetsDispatched==1,
            "composition result did not dispatch");
    };
    ITEM_INFO card{};
    card.m_itemIndex=100;
    card.SetItemId(4001);
    card.m_num=2;
    ITEM_INFO equipment{};
    equipment.m_itemIndex=101;
    equipment.SetItemId(1201);
    equipment.m_num=1;
    g_session.SetInventoryItem(card);
    g_session.SetInventoryItem(equipment);
    const std::vector<u8> success{0x7d,1,101,0,100,0,0};
    dispatch(success);
    Require(g_session.GetInventoryItemByIndex(100)->m_num==2, "unowned result consumed a card");
    mode.m_pendingItemComposition={100,101,4001,1201,{}};
    dispatch({0x7d,1,99,0,100,0,0});
    dispatch({0x7d,1,101,0,100,0,2});
    Require(mode.m_pendingItemComposition.cardIndex==100, "unrelated result consumed the pending request");
    dispatch(success);
    dispatch(success);
    Require(!mode.m_pendingItemComposition.cardIndex && g_session.GetInventoryItemByIndex(100)->m_num==1
        && g_session.GetInventoryItemByIndex(101)->m_slot[0]==4001, "successful result was not applied exactly once");
    mode.m_pendingItemComposition={100,101,4001,1201,{4001,0,0,0}};
    equipment.SetItemId(1202);
    g_session.SetInventoryItem(equipment);
    dispatch(success);
    Require(g_session.GetInventoryItemByIndex(100)->m_num==1 && g_session.GetInventoryItemByIndex(101)->m_slot[0]==0,
        "composition result changed a replacement equipment row");
    g_session.RemoveInventoryItem(100,1);
    g_session.RemoveInventoryItem(101,1);
    g_windowMgr.ClearChatEvents();
}

void ItemRefineResultReplay()
{
    CGameMode mode;
    CGameModePacketRouter router;
    RegisterDefaultGameModePacketHandlers(router);
    TestConnection connection;
    const auto dispatch = [&](const std::vector<u8>& bytes, int count = 1) {
        Require(connection.QueueBytes(bytes), "refine result fixture could not queue frames");
        Require(ProcessGameModePacketLoop(mode, connection, router, count).packetsDispatched == count
            && !connection.BufferedBytes(), "refine result did not frame and dispatch");
    };
    ITEM_INFO seed{};
    seed.m_itemIndex = 87;
    seed.SetItemId(1201);
    seed.m_itemType = 5;
    seed.m_num = 1;
    seed.m_refiningLevel = 4;
    g_session.SetInventoryItem(seed);
    g_windowMgr.ClearChatEvents();
    const auto findItem = [] { return g_session.GetInventoryItemByIndex(87); };
    dispatch({0x88, 1, 0, 0, 87, 0, 5, 0});
    Require(findItem() && findItem()->m_refiningLevel == 5 && findItem()->m_num == 1,
        "refine success did not update the existing item");
    dispatch({0x88, 1, 2, 0, 87, 0, 3, 0});
    dispatch({0x88, 1, 3, 0, 87, 0, 7, 0});
    dispatch({0x88, 1, 0, 0, 87, 0, 0, 1});
    Require(findItem() && findItem()->m_refiningLevel == 3,
        "downgrade or invalid refine result changed the wrong value");
    dispatch({0x88, 1, 1, 0, 87, 0, 0, 0});
    Require(findItem() && findItem()->m_refiningLevel == 0 && g_windowMgr.GetChatEvents().size() == 2,
        "refine failure removed an item or result notices differed");
    dispatch({0xaf, 0, 87, 0, 1, 0, 0x88, 1, 0, 0, 87, 0, 6, 0}, 2);
    Require(!findItem(), "refine result resurrected an absent item after removal");
    dispatch({0xd4, 2, 87, 0, 1, 0, 0xb1, 4, 1, 0, 6, 0, 0, 0, 0,
        0, 0, 0, 0, 2, 0, 5, 0, 0, 0, 0, 0, 0, 0});
    Require(findItem() && findItem()->m_refiningLevel == 6 && findItem()->GetItemId() == 1201,
        "authoritative add did not restore the refined item after removal");
    g_session.RemoveInventoryItem(87, 1);
    g_windowMgr.ClearChatEvents();
}

void ProductionResultReplay()
{
    CGameMode mode;
    CWorld world;
    CPlayer player;
    mode.m_world = &world;
    world.m_player = &player;
    auto* oldMode = SkillPacketReplayAccess::Current();
    const auto oldModeType = SkillPacketReplayAccess::CurrentType();
    const auto oldEffects = g_session.m_isEffectOn;
    SkillPacketReplayAccess::SetCurrent(&mode);
    g_session.m_isEffectOn = true;
    CGameModePacketRouter router;
    RegisterDefaultGameModePacketHandlers(router);
    TestConnection connection;
    const auto receive = [&](u8 result) {
        Require(connection.QueueBytes({0x8f, 1, result, 0, 0xb1, 4})
            && ProcessGameModePacketLoop(mode, connection, router, 1).packetsDispatched == 1
            && !connection.BufferedBytes(), "production result did not frame and dispatch");
    };
    const auto effectCount = [&](int id) {
        return std::count_if(player.m_effectList.begin(), player.m_effectList.end(), [=](const CRagEffect* effect) {
            return effect && effect->GetEffectType() == id;
        });
    };
    const auto inventorySize = g_session.GetInventoryItems().size();
    const auto noticeCount = g_windowMgr.GetChatEvents().size();
    for (u8 result = 0; result < 8; ++result) receive(result);
    receive(2);
    receive(8);
    Require(player.m_effectList.size() == 4 && effectCount(154) == 1 && effectCount(155) == 1
        && effectCount(305) == 1 && effectCount(306) == 1,
        "production result effects did not match reference selection and replacement");
    g_session.m_isEffectOn = false;
    receive(0);
    receive(2);
    Require(player.m_effectList.size() == 4 && effectCount(305) == 1
        && g_session.GetInventoryItems().size() == inventorySize && g_windowMgr.GetChatEvents().size() == noticeCount,
        "production result altered effects while disabled, inventory, or chat");
    g_session.m_isEffectOn = oldEffects;
    SkillPacketReplayAccess::Restore(oldMode, oldModeType);
    mode.m_world = nullptr;
}

void ProductionSelectionReplay()
{
    CGameMode mode;
    auto* oldMode = SkillPacketReplayAccess::Current();
    const auto oldModeType = SkillPacketReplayAccess::CurrentType();
    SkillPacketReplayAccess::SetCurrent(&mode);
    CGameModePacketRouter router;
    RegisterDefaultGameModePacketHandlers(router);
    TestConnection input;
    auto* connection = CRagConnection::instance();
    connection->Disconnect();
    LoopbackPeer peer(*connection);
    const std::vector<u8> list{0x8d, 1, 12, 0, 0xb1, 4, 0, 0, 0, 0, 0, 0};
    const auto receive = [&](const std::vector<u8>& bytes) {
        Require(input.QueueBytes(bytes) && ProcessGameModePacketLoop(mode, input, router, 1).packetsDispatched == 1,
            "crafting list did not dispatch");
    };
    receive(list);
    auto* window = g_windowMgr.m_skillSelectWnd;
    Require(window && window->IsProductionSelection(), "crafting list did not open its chooser");
    receive({0x8d, 1, 5, 0, 0});
    Require(g_windowMgr.m_skillSelectWnd == window, "malformed crafting list replaced the chooser");
    window->HandleKeyDown(VK_DOWN);
    window->HandleKeyDown(VK_RETURN);
    Require(g_windowMgr.m_skillSelectWnd == window
        && SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(30)) == 0,
        "choosing a recipe sent a request before confirmation");
    window->HandleKeyDown(VK_RETURN);
    Require(!g_windowMgr.m_skillSelectWnd
        && peer.ReceiveExact(10) == std::vector<u8>{0x8e, 1, 0xb1, 4, 0, 0, 0, 0, 0, 0},
        "crafting confirmation did not send the selected recipe");
    ITEM_INFO material{};
    material.m_itemIndex = 99;
    material.SetItemId(1000);
    material.m_num = 2;
    g_session.SetInventoryItem(material);
    material.m_itemIndex = 98;
    material.SetItemId(994);
    material.m_num = 1;
    g_session.SetInventoryItem(material);
    material.m_itemIndex = 97;
    material.SetItemId(995);
    g_session.SetInventoryItem(material);
    receive(list);
    window = g_windowMgr.m_skillSelectWnd;
    window->HandleKeyDown(VK_DOWN);
    window->HandleKeyDown(VK_RETURN);
    UISkillSelectWnd::DisplayData display;
    Require(window->GetDisplayDataForQt(&display) && display.materialSlots.size() == 3,
        "weapon production confirmation did not show three material slots");
    const auto drop = [&](unsigned int index, unsigned int id) {
        mode.m_dragType = static_cast<int>(DragType::ShortcutItem);
        mode.m_dragInfo = {};
        mode.m_dragInfo.type = mode.m_dragType;
        mode.m_dragInfo.source = static_cast<int>(DragSource::InventoryWindow);
        mode.m_dragInfo.itemIndex = index;
        mode.m_dragInfo.itemId = id;
        g_windowMgr.OnLBtnUp(display.materialSlots[0].x + 8, display.materialSlots[0].y + 8);
    };
    drop(98, 994);
    drop(97, 995);
    drop(99, 994);
    drop(99, 1000);
    drop(99, 1000);
    Require(window->GetDisplayDataForQt(&display) && display.materialSlots[0].itemId == 994
        && display.materialSlots[1].itemId == 1000 && display.materialSlots[2].itemId == 1000,
        "optional slots did not reject the second element and stale item while accepting duplicate crumbs");
    g_session.RemoveInventoryItem(99, 1);
    window->HandleKeyDown(VK_RETURN);
    Require(g_windowMgr.m_skillSelectWnd == window && window->GetDisplayDataForQt(&display)
        && display.materialSlots[1].itemId == 1000 && display.materialSlots[2].itemId == 0
        && SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(30)) == 0,
        "confirmation did not clear an unavailable material and wait for a fresh confirmation");
    g_windowMgr.OnLBtnDown(display.materialSlots[0].x + 8, display.materialSlots[0].y + 8);
    g_windowMgr.OnLBtnUp(window->m_x + window->m_w + 1, window->m_y);
    Require(window->GetDisplayDataForQt(&display) && display.materialSlots[0].itemId == 0
        && display.materialSlots[1].itemId == 1000 && g_session.GetInventoryItemByIndex(98)->m_num == 1,
        "dragging out a material did not clear only its slot");
    window->HandleKeyDown(VK_RETURN);
    Require(!g_windowMgr.m_skillSelectWnd && g_session.GetInventoryItemByIndex(99)->m_num == 1
        && peer.ReceiveExact(10) == std::vector<u8>{0x8e, 1, 0xb1, 4, 0, 0, 0xe8, 3, 0, 0},
        "optional crafting materials did not preserve slot order and inventory counts");
    g_session.RemoveInventoryItem(99, 1);
    g_session.RemoveInventoryItem(98, 1);
    g_session.RemoveInventoryItem(97, 1);
    receive({0x8d, 1, 12, 0, 0xe6, 3, 0, 0, 0, 0, 0, 0});
    window = g_windowMgr.m_skillSelectWnd;
    window->HandleKeyDown(VK_DOWN);
    window->HandleKeyDown(VK_RETURN);
    Require(window->GetDisplayDataForQt(&display) && display.productionConfirmation && display.materialSlots.empty(),
        "ore processing displayed optional weapon materials");
    window->HandleKeyDown(VK_ESCAPE);
    Require(peer.ReceiveExact(10) == std::vector<u8>{0x8e, 1, 0, 0, 0, 0, 0, 0, 0, 0},
        "production confirmation cancellation did not send a zero recipe");
    receive(list);
    g_windowMgr.m_skillSelectWnd->HandleKeyDown(VK_ESCAPE);
    Require(!g_windowMgr.m_skillSelectWnd
        && peer.ReceiveExact(10) == std::vector<u8>{0x8e, 1, 0, 0, 0, 0, 0, 0, 0, 0},
        "crafting cancellation did not send a zero recipe");
    receive({0x8d, 1, 4, 0});
    Require(g_windowMgr.m_roomMessageBox && g_windowMgr.m_roomMessageBox->GetMessageId() == 424,
        "empty crafting list did not show the reference notice");
    g_windowMgr.m_roomMessageBox->Close();
    connection->Disconnect();
    SkillPacketReplayAccess::Restore(oldMode, oldModeType);
}

void WeaponRefineSelectionReplay()
{
    CGameMode mode;
    auto* oldMode = SkillPacketReplayAccess::Current();
    const auto oldModeType = SkillPacketReplayAccess::CurrentType();
    SkillPacketReplayAccess::SetCurrent(&mode);
    CGameModePacketRouter router;
    RegisterDefaultGameModePacketHandlers(router);
    TestConnection input;
    auto* connection = CRagConnection::instance();
    connection->Disconnect();
    LoopbackPeer peer(*connection);
    const auto receive = [&](const std::vector<u8>& bytes) {
        Require(input.QueueBytes(bytes) && ProcessGameModePacketLoop(mode, input, router, 1).packetsDispatched == 1,
            "weapon refine frame did not dispatch");
    };
    ITEM_INFO weapon{};
    weapon.SetItemId(1201);
    weapon.m_itemIndex = 101;
    weapon.m_refiningLevel = 4;
    weapon.m_num = 1;
    g_session.SetInventoryItem(weapon);
    const std::vector<u8> list{0x21, 2, 17, 0, 101, 0, 0xb1, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0};
    receive(list);
    auto* menu = g_windowMgr.m_skillSelectWnd;
    Require(menu && menu->IsCraftingSelection(), "weapon refine list did not open a chooser");
    auto malformed = list;
    malformed[4] = 102;
    receive(malformed);
    Require(g_windowMgr.m_skillSelectWnd == menu, "invalid refine index replaced the chooser");
    menu->HandleKeyDown(VK_DOWN);
    menu->HandleKeyDown(VK_RETURN);
    Require(!g_windowMgr.m_skillSelectWnd
        && peer.ReceiveExact(6) == std::vector<u8>{0x22, 2, 101, 0, 0, 0},
        "weapon refine selection did not send the full inventory index");
    receive(list);
    g_windowMgr.m_skillSelectWnd->HandleKeyDown(VK_ESCAPE);
    Require(!g_windowMgr.m_skillSelectWnd
        && peer.ReceiveExact(6) == std::vector<u8>{0x22, 2, 255, 255, 255, 255},
        "weapon refine cancellation did not send index -1");
    g_windowMgr.ClearChatEvents();
    for (u8 result : {0, 1, 2, 3, 4}) receive({0x23, 2, result, 0, 0, 0, 0xb1, 4});
    const auto& notices = g_windowMgr.GetChatEvents();
    Require(notices.size() == 4 && notices[0].color == 0x00ffff00
        && notices[1].color == 0x00cdcd00 && notices[2].color == 0x00c8c8ff
        && notices[2].text != notices[3].text && g_session.GetInventoryItemByIndex(101)->m_refiningLevel == 4,
        "weapon refine notices differed or changed the item without an inventory packet");
    receive({0x21, 2, 4, 0});
    Require(g_windowMgr.m_roomMessageBox && g_windowMgr.m_roomMessageBox->GetMessageId() == 424,
        "empty weapon refine list did not show the reference notice");
    mode.ClearCraftingSelection();
    g_windowMgr.ClearChatEvents();
    g_session.RemoveInventoryItem(101, 1);
    connection->Disconnect();
    SkillPacketReplayAccess::Restore(oldMode, oldModeType);
}

void ItemRepairSelectionReplay()
{
    CGameMode mode;
    auto* oldMode = SkillPacketReplayAccess::Current();
    const auto oldModeType = SkillPacketReplayAccess::CurrentType();
    SkillPacketReplayAccess::SetCurrent(&mode);
    CGameModePacketRouter router;
    RegisterDefaultGameModePacketHandlers(router);
    TestConnection input;
    auto* connection = CRagConnection::instance();
    connection->Disconnect();
    LoopbackPeer peer(*connection);
    const auto receive = [&](const std::vector<u8>& bytes) {
        Require(input.QueueBytes(bytes) && ProcessGameModePacketLoop(mode, input, router, 1).packetsDispatched == 1,
            "item repair frame did not dispatch");
    };
    const std::vector<u8> list{0xfc, 1, 17, 0, 0, 0, 0xb1, 4, 4, 0x11, 0x22, 0x33, 0x44, 0, 0, 0, 0};
    receive(list);
    auto* menu = g_windowMgr.m_skillSelectWnd;
    Require(menu && menu->IsCraftingSelection(), "repair list rejected raw slot zero");
    auto malformed = list;
    malformed[4] = 100;
    receive(malformed);
    Require(g_windowMgr.m_skillSelectWnd == menu, "bad repair slot replaced the active chooser");
    menu->HandleKeyDown(VK_DOWN);
    menu->HandleKeyDown(VK_RETURN);
    Require(!g_windowMgr.m_skillSelectWnd && peer.ReceiveExact(15) == std::vector<u8>{
        0xfd, 1, 0, 0, 0xb1, 4, 4, 0x11, 0x22, 0x33, 0x44, 0, 0, 0, 0},
        "repair selection did not echo the full offered target row");
    receive(list);
    g_windowMgr.m_skillSelectWnd->HandleKeyDown(VK_ESCAPE);
    Require(!g_windowMgr.m_skillSelectWnd && peer.ReceiveExact(15) == std::vector<u8>{
        0xfd, 1, 0xff, 0xff, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        "repair cancel did not send the selected sentinel");

    g_session.ClearInventoryItems();
    const std::vector<u8> equipment{0xd0, 2, 30, 0, 2, 0, 0xb1, 4, 5, 1, 2, 0, 0, 0,
        1, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    receive(equipment);
    g_windowMgr.ClearChatEvents();
    receive({0xfe, 1, 2, 0, 0});
    receive({0xfe, 1, 2, 0, 1});
    receive({0xfe, 1, 2, 0, 2});
    const auto& notices = g_windowMgr.GetChatEvents();
    Require(notices.size() == 2 && notices[0].color == 0x00ffff00 && notices[1].color == 0xff
        && g_session.GetInventoryItemByIndex(2) && g_session.GetInventoryItemByIndex(2)->m_isDamaged,
        "repair result changed a potentially unrelated local item or produced incorrect notices");
    auto repaired = equipment;
    repaired[14] = 0;
    receive(repaired);
    Require(g_session.GetInventoryItemByIndex(2) && !g_session.GetInventoryItemByIndex(2)->m_isDamaged,
        "authoritative equipment refresh did not clear the broken flag");
    g_session.RemoveInventoryItem(2, 1);
    g_windowMgr.ClearChatEvents();
    connection->Disconnect();
    SkillPacketReplayAccess::Restore(oldMode, oldModeType);
}

void InactiveStoragePasswordReplay()
{
    CGameMode mode;
    CGameModePacketRouter router;
    RegisterDefaultGameModePacketHandlers(router);
    TestConnection input;
    auto* connection = CRagConnection::instance();
    connection->Disconnect();
    LoopbackPeer peer(*connection);
    g_session.CloseStorage();
    g_windowMgr.ClearChatEvents();

    const std::vector<u8> frames{
        0x3a, 2, 2, 0,       // Unknown challenge.
        0x3a, 2, 1, 0,       // Protected challenge.
        0x3c, 2, 6, 0, 0, 0, // Unsolicited successful check.
        0x3c, 2, 8, 0, 2, 0, // Penalty result.
    };
    Require(input.QueueBytes(frames) && ProcessGameModePacketLoop(mode, input, router, 4).packetsDispatched == 4
        && !input.BufferedBytes(), "inactive storage password frames did not drain in order");
    Require(g_windowMgr.GetChatEvents().size() == 1 && !g_session.IsStorageOpen()
        && SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(30)) == 0,
        "inactive storage password frames changed storage state or emitted a reply");

    g_windowMgr.ClearChatEvents();
    connection->Disconnect();
}

void CookingSelectionReplay()
{
    CGameMode mode;
    auto* oldMode = SkillPacketReplayAccess::Current();
    const auto oldModeType = SkillPacketReplayAccess::CurrentType();
    SkillPacketReplayAccess::SetCurrent(&mode);
    CGameModePacketRouter router;
    RegisterDefaultGameModePacketHandlers(router);
    TestConnection input;
    auto* connection = CRagConnection::instance();
    connection->Disconnect();
    LoopbackPeer peer(*connection);
    const auto receive = [&](const std::vector<u8>& bytes) {
        Require(input.QueueBytes(bytes) && ProcessGameModePacketLoop(mode, input, router, 1).packetsDispatched == 1
            && !input.BufferedBytes(), "cooking list did not frame and dispatch");
    };
    const std::vector<u8> list{0x5a, 2, 10, 0, 1, 0, 0xad, 0x2e, 0xae, 0x2e};
    receive(list);
    auto* menu = g_windowMgr.m_skillSelectWnd;
    Require(menu && menu->IsCraftingSelection(), "cooking list did not open its chooser");
    receive({0x5a, 2, 7, 0, 1, 0, 0xad});
    receive({0x5a, 2, 8, 0, 4, 0, 0xad, 0x2e});
    Require(g_windowMgr.m_skillSelectWnd == menu, "partial row or unsupported cooking type replaced the chooser");
    menu->HandleKeyDown(VK_DOWN);
    menu->HandleKeyDown(VK_DOWN);
    menu->HandleKeyDown(VK_RETURN);
    Require(!g_windowMgr.m_skillSelectWnd && peer.ReceiveExact(6) == std::vector<u8>{0x5b, 2, 1, 0, 0xae, 0x2e},
        "cooking selection did not send the selected product and make type");
    receive({0x5a, 2, 6, 0, 1, 0});
    menu = g_windowMgr.m_skillSelectWnd;
    Require(menu, "empty cooking list did not open the reference chooser");
    menu->HandleKeyDown(VK_RETURN);
    Require(g_windowMgr.m_skillSelectWnd == menu
        && SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(30)) == 0,
        "empty cooking list sent a product on confirmation");
    menu->HandleKeyDown(VK_ESCAPE);
    Require(!g_windowMgr.m_skillSelectWnd && peer.ReceiveExact(6) == std::vector<u8>{0x5b, 2, 1, 0, 0xff, 0xff},
        "cooking cancel did not send the reference sentinel");
    for (const u8 type : {5, 6, 7, 8}) {
        receive({0x5a, 2, 8, 0, 1, 0, 0xad, 0x2e});
        receive({0x5a, 2, 8, 0, type, 0, 0xae, 0x2e});
        menu = g_windowMgr.m_skillSelectWnd;
        Require(menu && menu->IsCraftingSelection(), "server cooking variant did not open its chooser");
        menu->HandleKeyDown(VK_DOWN);
        menu->HandleKeyDown(VK_RETURN);
        Require(!g_windowMgr.m_skillSelectWnd && peer.ReceiveExact(6) == std::vector<u8>{0x5b, 2, type, 0, 0xae, 0x2e},
            "cooking variant selection did not echo the latest type and product");
        receive({0x5a, 2, 6, 0, type, 0});
        menu = g_windowMgr.m_skillSelectWnd;
        Require(menu, "empty server cooking variant did not open its chooser");
        menu->HandleKeyDown(VK_ESCAPE);
        Require(!g_windowMgr.m_skillSelectWnd && peer.ReceiveExact(6) == std::vector<u8>{0x5b, 2, type, 0, 0xff, 0xff},
            "cooking variant cancel did not preserve its make type");
    }
    connection->Disconnect();
    SkillPacketReplayAccess::Restore(oldMode, oldModeType);
}
