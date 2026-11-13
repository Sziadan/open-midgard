void InventoryMutationAckReplay()
{
    SetCase("inventory_mutation_ack_lifecycle", "PK09.17",
        "PK09.17-literal-inventory-mutation-acks",
        "ProcessGameModePacketLoop -> TryReadPacket -> production router -> inventory mutation acknowledgement handlers");

    CGameMode mode;
    CGameMode otherMode;
    TestConnection connection;
    CGameModePacketRouter& router = GetProductionGameModePacketRouter();
    struct Restore {
        std::vector<ITEM_INFO> inventory{g_session.GetInventoryItems().begin(), g_session.GetInventoryItems().end()};
        std::vector<UIChatEvent> chat{g_windowMgr.GetChatEvents()};
        int waitingThrow = 0, waitingUse = 0, lastCard = 0;
        u32 waitingWear = 0, waitingTakeoff = 0, pickupTick = 0;
        u32 aid = 0, gid = 0;
        std::list<u32> pickupQueue;
        CGameMode& mode;
        CMode* currentMode;
        int currentModeType;
        UINewChatWnd* chatWnd = nullptr;
        int chatShow = 0, managerChatShow = 0;
        explicit Restore(CGameMode& m) :
            waitingThrow(m.m_waitingItemThrowAck), waitingUse(m.m_waitingUseItemAck), lastCard(m.m_lastCardItemIndex),
            waitingWear(m.m_waitingWearEquipAck), waitingTakeoff(m.m_waitingTakeoffEquipAck),
            pickupTick(m.m_lastPickupRequestTick), aid(g_session.m_aid), gid(g_session.m_gid),
            pickupQueue(m.m_pickupReqItemNaidList), mode(m), currentMode(SkillPacketReplayAccess::Current()),
            currentModeType(SkillPacketReplayAccess::CurrentType()), chatWnd(g_windowMgr.m_chatWnd),
            chatShow(chatWnd ? chatWnd->m_show : 0), managerChatShow(g_windowMgr.m_chatWndShow) {}
        ~Restore() {
            g_session.ClearInventoryItems();
            for (const ITEM_INFO& item : inventory) g_session.SetInventoryItem(item);
            mode.m_waitingItemThrowAck = waitingThrow; mode.m_waitingUseItemAck = waitingUse;
            mode.m_lastCardItemIndex = lastCard; mode.m_waitingWearEquipAck = waitingWear;
            mode.m_waitingTakeoffEquipAck = waitingTakeoff; mode.m_lastPickupRequestTick = pickupTick;
            mode.m_pickupReqItemNaidList = pickupQueue;
            g_session.m_aid = aid; g_session.m_gid = gid;
            if (!chatWnd && g_windowMgr.m_chatWnd) g_windowMgr.DeleteWindow(g_windowMgr.m_chatWnd);
            else if (chatWnd && g_windowMgr.m_chatWnd == chatWnd) chatWnd->SetShow(chatShow);
            g_windowMgr.m_chatWndShow=managerChatShow;
            g_windowMgr.ClearChatEvents();
            for (const UIChatEvent& event : chat) g_windowMgr.PushChatEvent(event.text.c_str(),event.color,event.channel,event.tick);
            SkillPacketReplayAccess::Restore(currentMode,currentModeType);
            GetProductionGameModePacketRouter().Clear();
            RegisterDefaultGameModePacketHandlers(GetProductionGameModePacketRouter());
        }
    } restore(mode);

    router.Clear(); RegisterDefaultGameModePacketHandlers(router);
    g_session.m_aid=0x13572468; g_session.m_gid=0x24681357;
    SkillPacketReplayAccess::SetCurrent(&otherMode);
    otherMode.m_waitingUseItemAck=77;
    g_windowMgr.ClearChatEvents();
    g_session.ClearInventoryItems();
    const auto put16 = [](std::vector<u8>& p, size_t o, u16 v) { p[o]=static_cast<u8>(v); p[o+1]=static_cast<u8>(v>>8); };
    const auto put32 = [](std::vector<u8>& p, size_t o, u32 v) { for (int i=0;i<4;++i) p[o+i]=static_cast<u8>(v>>(8*i)); };
    const auto frame = [&](u16 id, int n) { std::vector<u8> p(n); put16(p,0,id); return p; };
    const auto dispatch = [&](const std::vector<u8>& p, const char* what) {
        Require(connection.QueueBytes(p), std::string("queue failed: ")+what);
        const auto r=ProcessGameModePacketLoop(mode,connection,router,1);
        Require(r.packetsProcessed==1 && r.packetsDispatched==1 && connection.BufferedBytes()==0,
            std::string("production route failed: ")+what);
    };
    const auto find = [](u16 index) { return g_session.GetInventoryItemByIndex(index); };
    const auto seed = [&]() {
        ITEM_INFO i{}; i.m_itemIndex=0x1234; i.SetItemId(0x4567); i.m_itemType=3; i.m_num=5;
        i.m_isIdentified=1; i.m_isDamaged=1; i.m_refiningLevel=4; i.m_slot[0]=0x1122;
        i.m_location=0x2345; i.m_wearLocation=0x0040; i.m_deleteTime=0x10203040; i.m_isYours=1;
        g_session.SetInventoryItem(i);
    };
    const auto preserved = [](const ITEM_INFO* i, int amount) {
        return i && i->m_num==amount && i->GetItemId()==0x4567 && i->m_itemType==3
            && i->m_isIdentified==1 && i->m_isDamaged==1 && i->m_refiningLevel==4
            && i->m_slot[0]==0x1122 && i->m_location==0x2345 && i->m_wearLocation==0x0040
            && i->m_deleteTime==0x10203040 && i->m_isYours==1;
    };

    // Additive pickup stack preserves the existing item's raw metadata; valid failures clean pending state.
    seed(); mode.m_pickupReqItemNaidList={0x11112222,0x33334444}; mode.m_lastPickupRequestTick=99;
    auto pick=frame(0x02D4,29); put16(pick,2,0x1234); put16(pick,4,2); put16(pick,6,0x9999);
    pick[8]=0; pick[9]=0; pick[10]=0; put16(pick,11,0); put16(pick,13,0); put16(pick,15,0); put16(pick,17,0);
    put16(pick,19,0x7777); pick[21]=3; pick[22]=0; put32(pick,23,0xAABBCCDD); put16(pick,27,2);
    dispatch(pick,"extended pickup success");
    Require(preserved(find(0x1234),7),"same-index pickup failed to add count while retaining item metadata");
    Require(mode.m_pickupReqItemNaidList.empty() && mode.m_lastPickupRequestTick==0,"pickup success did not clear pending queue and tick");
    put16(pick,2,0x5678); put16(pick,4,4); put16(pick,6,0x1357); pick[8]=1; pick[9]=1; pick[10]=3;
    put16(pick,11,0x2211); put16(pick,19,0x3456); pick[21]=10; put32(pick,23,0xAABBCCDD); put16(pick,27,2);
    mode.m_pickupReqItemNaidList={0x11112222}; mode.m_lastPickupRequestTick=98;
    dispatch(pick,"extended pickup literal fields");
    const ITEM_INFO* added=find(0x5678);
    Require(added && added->m_num==4 && added->GetItemId()==0x1357 && added->m_isIdentified==1
        && added->m_isDamaged==1 && added->m_refiningLevel==3 && added->m_slot[0]==0x2211
        && added->m_location==0x8000 && added->m_deleteTime==static_cast<int>(0xAABBCCDDu)
        && added->m_isYours==2 && mode.m_pickupReqItemNaidList.empty() && mode.m_lastPickupRequestTick==0,
        "extended pickup fields or pending cleanup differ from literal bytes");

    seed(); mode.m_pickupReqItemNaidList={0x11112222}; mode.m_lastPickupRequestTick=100;
    pick=frame(0x02D4,29); pick[22]=3; dispatch(pick,"extended pickup server failure");
    Require(preserved(find(0x1234),5),"pickup failure replaced inventory metadata/count");
    Require(mode.m_pickupReqItemNaidList.empty() && mode.m_lastPickupRequestTick==0,"valid pickup failure did not clear pending queue and tick");
    seed(); mode.m_pickupReqItemNaidList={0x11112222}; mode.m_lastPickupRequestTick=101;
    pick=frame(0x00A0,23); pick[22]=1; dispatch(pick,"basic pickup failure");
    Require(preserved(find(0x1234),5) && mode.m_pickupReqItemNaidList.empty() && mode.m_lastPickupRequestTick==0,
        "basic pickup failure changed inventory or retained pending state");
    seed();
    pick=frame(0x00A0,23); put16(pick,2,0x5679); put16(pick,4,3); put16(pick,6,0x2468);
    pick[8]=1; pick[9]=0; pick[10]=2; put16(pick,11,0x3344); put16(pick,19,0x1357); pick[21]=3;
    mode.m_pickupReqItemNaidList={0x11112222}; mode.m_lastPickupRequestTick=103;
    dispatch(pick,"basic pickup success literal fields");
    const ITEM_INFO* legacy=find(0x5679);
    Require(legacy && legacy->GetItemId()==0x2468 && legacy->m_num==3 && legacy->m_isIdentified==1
        && legacy->m_isDamaged==0 && legacy->m_refiningLevel==2 && legacy->m_slot[0]==0x3344
        && legacy->m_location==0x1357 && legacy->m_deleteTime==0 && legacy->m_isYours==0
        && mode.m_pickupReqItemNaidList.empty() && mode.m_lastPickupRequestTick==0,
        "basic pickup fields or pending cleanup differ from literal bytes");
    mode.m_pickupReqItemNaidList={0x11112222}; mode.m_lastPickupRequestTick=102;
    pick=frame(0x02D4,29); pick[22]=7; dispatch(pick,"unknown pickup failure");
    Require(preserved(find(0x1234),5) && mode.m_pickupReqItemNaidList.empty() && mode.m_lastPickupRequestTick==0,
        "unknown pickup failure changed inventory or retained pending state");

    const auto pickupFailure = [&](u8 code, const char* expected, bool quiet) {
        seed(); mode.m_pickupReqItemNaidList={0x11112222}; mode.m_lastPickupRequestTick=104;
        g_windowMgr.ClearChatEvents(); pick=frame(0x02D4,29); pick[22]=code;
        dispatch(pick,"pickup failure notice");
        Require(preserved(find(0x1234),5) && mode.m_pickupReqItemNaidList.empty() && mode.m_lastPickupRequestTick==0,
            "pickup failure notice changed inventory or retained pending state");
        const auto& events=g_windowMgr.GetChatEvents();
        Require(quiet ? events.empty() : (events.size()==1 && events.back().text==expected
            && events.back().color==0x00ff0000u && events.back().channel==0),"pickup failure notice text/channel/color differs from reference");
        if (!quiet) Require(g_windowMgr.m_chatWnd && g_windowMgr.m_chatWnd->m_show,
            "known pickup failure notice did not make chat visible");
    };
    pickupFailure(1,"You cannot get the item.",false);
    pickupFailure(2,"You cannot carry more items because you are overweight.",false);
    pickupFailure(4,"You can't have this item because you will exceed the weight limit.",false);
    pickupFailure(5,"You cannot carry more than 30,000 of one kind of item.",false);
    pickupFailure(6,"You cannot get the item.",false);
    pickupFailure(3,"",true);
    pickupFailure(7,"",true);

    // Basic zero-count drop rejection is a correction only; positive removal is authoritative.
    seed(); mode.m_waitingItemThrowAck=0x1234;
    auto drop=frame(0x00AF,6); put16(drop,2,0x1234); put16(drop,4,0); dispatch(drop,"zero-count drop reject");
    Require(preserved(find(0x1234),5) && mode.m_waitingItemThrowAck==0,"drop rejection changed item or retained matching pending state");
    mode.m_waitingItemThrowAck=0x7777; put16(drop,4,2); dispatch(drop,"partial drop");
    Require(preserved(find(0x1234),3) && mode.m_waitingItemThrowAck==0x7777,"partial drop count/pending correlation differs");
    put16(drop,4,3); dispatch(drop,"full drop"); Require(find(0x1234)==nullptr,"full drop did not remove row");

    // Basic and extended use acknowledgements carry the authoritative remainder; failures and unknown rows are harmless.
    constexpr u16 useIndex = 65;
    const auto seedUseItem = [&]() {
        seed();
        ITEM_INFO item = *find(0x1234);
        g_session.RemoveInventoryItem(0x1234, 0);
        item.m_itemIndex = useIndex;
        g_session.SetInventoryItem(item);
    };
    seedUseItem(); mode.m_waitingUseItemAck=useIndex;
    auto use=frame(0x00A8,7); put16(use,2,useIndex); put16(use,4,8); use[6]=1; dispatch(use,"basic use success count increase");
    Require(find(useIndex) && find(useIndex)->m_num==8 && mode.m_waitingUseItemAck==0,"basic success did not apply authoritative remainder/clear pending");
    mode.m_waitingUseItemAck=useIndex; put16(use,4,2); use[6]=0; dispatch(use,"basic use failure");
    Require(preserved(find(useIndex),8) && mode.m_waitingUseItemAck==0,"basic failure changed inventory or retained pending state");
    auto use2=frame(0x01C8,13); put16(use2,2,useIndex); put16(use2,4,0x4567); put32(use2,6,0xDEADBEEF); put16(use2,10,3); use2[12]=1;
    mode.m_waitingUseItemAck=useIndex; const u32 oldGid=g_session.m_gid; g_session.m_gid=0x10101010;
    dispatch(use2,"extended remote use success");
    Require(preserved(find(useIndex),8) && mode.m_waitingUseItemAck==useIndex && otherMode.m_waitingUseItemAck==77,
        "remote extended use ack mutated local item or either pending state");
    g_session.m_gid=oldGid;
    put16(use2,10,3); put32(use2,6,g_session.m_gid); mode.m_waitingUseItemAck=useIndex;
    dispatch(use2,"extended local use decrease");
    Require(find(useIndex) && find(useIndex)->m_num==3 && mode.m_waitingUseItemAck==0
        && otherMode.m_waitingUseItemAck==77,"extended local success did not apply smaller remainder to passed mode");
    put16(use2,10,9); mode.m_waitingUseItemAck=useIndex;
    dispatch(use2,"extended local use increase");
    Require(find(useIndex) && find(useIndex)->m_num==9 && mode.m_waitingUseItemAck==0,"extended local success did not apply larger remainder");
    put16(use2,10,0); mode.m_waitingUseItemAck=useIndex;
    dispatch(use2,"extended local use zero remainder");
    Require(find(useIndex)==nullptr && mode.m_waitingUseItemAck==0,"extended local success did not remove zero-remainder stack");
    seedUseItem(); put16(use2,10,1); use2[12]=0; mode.m_waitingUseItemAck=useIndex;
    dispatch(use2,"extended local use failure");
    Require(preserved(find(useIndex),5) && mode.m_waitingUseItemAck==0,"extended local failure changed inventory or retained passed-mode pending state");
    put16(use2,2,0x9999); use2[12]=1; mode.m_waitingUseItemAck=useIndex;
    dispatch(use2,"extended local unknown item");
    Require(preserved(find(useIndex),5) && mode.m_waitingUseItemAck==useIndex,"unknown extended use item mutated row or cleared unrelated pending state");
    put16(use,2,0x9999); put16(use,4,0); use[6]=1; mode.m_waitingUseItemAck=useIndex;
    dispatch(use,"basic unknown use item");
    Require(preserved(find(useIndex),5) && mode.m_waitingUseItemAck==useIndex,"unknown basic use item changed inventory or cleared unrelated pending state");
    put16(use,2,useIndex); use[6]=1; mode.m_waitingUseItemAck=useIndex;
    dispatch(use,"basic zero remainder");
    Require(find(useIndex)==nullptr && mode.m_waitingUseItemAck==0,"basic zero remainder did not remove stack/clear pending");

    // Equip result 1 succeeds; result 0/2 fail and result 3 is quiet. Correlation is index-specific.
    seed(); ITEM_INFO other{}; other.m_itemIndex=0x2222; other.m_num=1; other.m_wearLocation=0x0040; g_session.SetInventoryItem(other);
    g_windowMgr.ClearChatEvents();
    mode.m_waitingWearEquipAck=0x1234;
    auto equip=frame(0x00AA,7); put16(equip,2,0x1234); put16(equip,4,0x0040); equip[6]=1;
    dispatch(equip,"equip success");
    Require(find(0x1234)->m_wearLocation==0x0040 && find(0x2222)->m_wearLocation==0 && mode.m_waitingWearEquipAck==0,
        "equip success did not move mask/clear matching pending");
    g_session.SetInventoryItemWearLocation(0x2222,0x0040);
    mode.m_waitingWearEquipAck=0x2222; put16(equip,2,0x1234); equip[6]=0; dispatch(equip,"equip failure result zero");
    Require(find(0x1234)->m_wearLocation==0x0040 && find(0x2222)->m_wearLocation==0x0040 && mode.m_waitingWearEquipAck==0x2222,
        "equip failure mutated masks or cleared unrelated pending");
    const auto& equipEvents0=g_windowMgr.GetChatEvents();
    Require(equipEvents0.size()==1 && equipEvents0.back().text=="You can't put this item on."
        && equipEvents0.back().color==0x00ff0000u && equipEvents0.back().channel==6,"equip result-zero notice differs from reference");
    g_windowMgr.ClearChatEvents(); mode.m_waitingWearEquipAck=0x1234; put16(equip,2,0x1234); equip[6]=2;
    dispatch(equip,"equip failure result two");
    Require(find(0x1234)->m_wearLocation==0x0040 && find(0x2222)->m_wearLocation==0x0040 && mode.m_waitingWearEquipAck==0,
        "known result-two equip failure mutated masks or retained matching pending");
    const auto& equipEvents2=g_windowMgr.GetChatEvents();
    Require(equipEvents2.size()==1 && equipEvents2.back().text=="You can't put this item on."
        && equipEvents2.back().color==0x00ff0000u && equipEvents2.back().channel==6,"equip result-two notice differs from reference");
    mode.m_waitingWearEquipAck=0x2222; put16(equip,2,0); equip[6]=2; dispatch(equip,"zero-index equip failure");
    Require(mode.m_waitingWearEquipAck==0x2222,"zero-index equip failure cleared unrelated pending");
    put16(equip,2,0x9999); put16(equip,4,0x0040); equip[6]=1; dispatch(equip,"equip unknown success");
    Require(find(0x2222)->m_wearLocation==0x0040,"unknown equip success cleared an occupant mask");
    g_windowMgr.ClearChatEvents(); put16(equip,2,0x1234); equip[6]=3; dispatch(equip,"equip unknown result");
    Require(find(0x1234)->m_wearLocation==0x0040 && mode.m_waitingWearEquipAck==0x2222
        && g_windowMgr.GetChatEvents().empty(),"unknown equip result mutated state, emitted notice, or cleared unrelated pending");

    // Ammo ack selects or clears the ammo mask and clears pending; unequip failure/success are correlated.
    auto ammo=frame(0x013C,4); put16(ammo,2,0x1234); mode.m_waitingWearEquipAck=0x2222;
    dispatch(ammo,"ammo equip");
    Require(find(0x1234)->m_wearLocation==0x8000 && find(0x2222)->m_wearLocation==0x0040 && mode.m_waitingWearEquipAck==0,
        "ammo equip did not move ammo mask/clear pending");
    put16(ammo,2,0); mode.m_waitingWearEquipAck=0x2222; dispatch(ammo,"ammo clear");
    Require(find(0x1234)->m_wearLocation==0 && find(0x2222)->m_wearLocation==0x0040 && mode.m_waitingWearEquipAck==0,
        "zero ammo ack did not clear mask/pending while preserving other slots");

    g_session.SetInventoryItemWearLocation(0x1234,0x0040);
    mode.m_waitingTakeoffEquipAck=0x1234;
    auto off=frame(0x00AC,7); put16(off,2,0x1234); put16(off,4,0x0040); off[6]=0; dispatch(off,"unequip failure");
    Require(find(0x1234)->m_wearLocation==0x0040 && mode.m_waitingTakeoffEquipAck==0,"unequip failure changed row or retained matching pending");
    mode.m_waitingTakeoffEquipAck=0x1234; off[6]=1; dispatch(off,"unequip success");
    Require(find(0x1234)->m_wearLocation==0 && mode.m_waitingTakeoffEquipAck==0,"unequip success did not clear wear state/pending");

    std::cout << "{\"case_id\":\"inventory_mutation_ack_lifecycle\",\"contract_id\":\"PK09.17\",\"fixture_id\":\"PK09.17-literal-inventory-mutation-acks\",\"outcome\":\"PASS\",\"assertions\":\"pickup additive metadata and cleanup; drop rejection/partial/full; basic/extended use counts, failure, unknown and remote isolation; equip/ammo/unequip state and pending correlation\"}" << std::endl;
}
