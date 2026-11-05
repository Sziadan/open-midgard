void CartStackDeltaReplay()
{
    SetCase("cart_stack_delta_lifecycle_20080910", "PK37.05",
        "PK37-selected-20080910-cart-status-list-add-delete-failure", "ProcessGameModePacketLoop -> production router -> Commerce::Receive");
    struct Restore {
        ro::commerce::State commerce = ro::commerce::state;
        bool cartActive = IsKnownLocalCartActive();
        std::vector<UIChatEvent> chat = g_windowMgr.GetChatEvents();
        ~Restore() {
            PrimeLocalCartStateFromEffectState(cartActive ? 8 : 0);
            ro::commerce::state = std::move(commerce);
            g_windowMgr.ClearChatEvents();
            for (const UIChatEvent& event : chat)
                g_windowMgr.PushChatEvent(event.text.c_str(), event.color, event.channel, event.tick);
        }
    } restore;
    ro::commerce::Reset();

    CGameMode mode;
    TestConnection connection;
    CGameModePacketRouter& router = GetProductionGameModePacketRouter();
    router.Clear();
    RegisterDefaultGameModePacketHandlers(router);
    const auto dispatch = [&](const std::vector<u8>& frame, const char* label) {
        Require(connection.QueueBytes(frame), std::string("could not queue ") + label);
        const auto result = ProcessGameModePacketLoop(mode, connection, router, 1);
        Require(result.packetsProcessed == 1 && result.packetsDispatched == 1
            && connection.BufferedBytes() == 0, std::string("production dispatch failed for ") + label);
    };

    // Literal 0x0121 (14 bytes), then 0x02E9 with one 22-byte normal row.
    dispatch({0x21,0x01, 1,0, 100,0, 0x20,0x03,0,0, 0x10,0x27,0,0}, "cart counters");
    const std::vector<u8> normal = {0xE9,0x02,26,0,0x24,1,0xD6,6,10,1,9,0,0,0x80,
        0,0,0,0,0,0,0,0,0,0,0,0};
    dispatch(normal, "normal cart snapshot");
    Require(ro::commerce::state.hasCart && ro::commerce::state.cartCount == 1
        && ro::commerce::state.cartCapacity == 100 && ro::commerce::state.cartWeight == 800
        && ro::commerce::state.cartMaxWeight == 10000, "0121 counters were not decoded literally");
    const auto item = ro::commerce::state.cartItems.find(0x0124);
    Require(item != ro::commerce::state.cartItems.end() && item->second.GetItemId() == 1750
        && item->second.m_itemType == 10 && item->second.m_num == 9
        && item->second.m_location == 0x8000, "02E9 snapshot row fields differ from fixture");

    // 01C5 carries amount plus ordinary item attributes, but has no location field.
    // A same-index stack delta must retain location from the existing snapshot.
    dispatch({0xC5,0x01, 0x24,0x01, 3,0,0,0, 0xD6,0x06, 10,1,0,0,
        0,0,0,0,0,0,0,0}, "typed cart stack delta");
    Require(ro::commerce::state.cartItems.at(0x0124).m_num == 12
        && ro::commerce::state.cartItems.at(0x0124).m_location == 0x8000,
        "01C5 stack delta did not accumulate while preserving uncarried location");
    Require(ro::commerce::state.cartCount == 1 && ro::commerce::state.cartWeight == 800,
        "cart delta changed 0121 counters before a new status packet");

    dispatch({0x25,0x01, 0x24,0x01, 5,0,0,0}, "partial cart removal");
    Require(ro::commerce::state.cartItems.at(0x0124).m_num == 7
        && ro::commerce::state.cartCount == 1 && ro::commerce::state.cartWeight == 800,
        "0125 partial removal or counter retention is wrong");
    dispatch({0x25,0x01, 0x24,0x01, 7,0,0,0}, "full cart removal");
    Require(ro::commerce::state.cartItems.count(0x0124) == 0
        && ro::commerce::state.cartCount == 1 && ro::commerce::state.cartWeight == 800,
        "0125 full removal did not remove row or incorrectly changed status counters");

    dispatch({0x21,0x01, 0,0, 100,0, 0,0,0,0, 0x10,0x27,0,0}, "updated empty cart counters");
    Require(ro::commerce::state.hasCart && ro::commerce::state.cartCount == 0 && ro::commerce::state.cartWeight == 0,
        "subsequent 0121 did not update cart counters");
    dispatch(normal,"restore normal row for transfer failures");
    dispatch({0x21,0x01,1,0,100,0,0x20,3,0,0,0x10,0x27,0,0},"restored counters");
    dispatch({0x2C,0x01, 0}, "cart weight failure");
    Require(ro::commerce::state.status == "Cart transfer failed: cart weight limit."
        && ro::commerce::state.cartCount == 1 && ro::commerce::state.cartWeight == 800
        && ro::commerce::state.cartItems.at(0x0124).m_num==9,
        "012C result 0 text or state preservation is wrong");
    dispatch({0x2C,0x01, 1}, "cart capacity failure");
    Require(ro::commerce::state.status == "Cart transfer failed: cart item capacity limit."
        && ro::commerce::state.cartCount == 1 && ro::commerce::state.cartWeight == 800
        && ro::commerce::state.cartItems.at(0x0124).m_num==9,
        "012C result 1 text or state preservation is wrong");
    const auto noticeCount=g_windowMgr.GetChatEvents().size();const auto lastStatus=ro::commerce::state.status;
    dispatch({0x2c,1,2},"unknown cart failure result");
    Require(g_windowMgr.GetChatEvents().size()==noticeCount && ro::commerce::state.status==lastStatus,
        "unknown cart result invented a failure notice");
    // Re-add the cart leg of a failed withdrawal after a partial removal.
    dispatch({0x25,1,0x24,1,2,0,0,0},"withdrawal decrement");
    dispatch({0xc5,1,0x24,1,2,0,0,0,0xd6,6,10,1,0,0,0,0,0,0,0,0,0,0},"withdrawal rollback add");
    Require(ro::commerce::state.cartItems.at(0x0124).m_num==9
        && ro::commerce::state.cartItems.at(0x0124).m_location==0x8000,"cart rollback lost quantity or uncarried metadata");
    // A new index copies every carried field; missing fields remain unspecified/default.
    dispatch({0xc5,1,0x25,1,1,0,0,0,0xb1,4,4,1,1,3,0x11,0x11,0x22,0x22,0x33,0x33,0x44,0x44},"new typed cart item");
    const auto& added=ro::commerce::state.cartItems.at(0x125);
    Require(added.GetItemId()==1201 && added.m_itemType==4 && added.m_num==1 && added.m_isIdentified==1
        && added.m_isDamaged==1 && added.m_refiningLevel==3 && added.m_slot[0]==0x1111
        && added.m_slot[1]==0x2222 && added.m_slot[2]==0x3333 && added.m_slot[3]==0x4444,
        "new typed cart delta lost carried fields");
    std::cout << "{\"case_id\":\"cart_stack_delta_lifecycle_20080910\",\"contract_id\":\"PK37.05\",\"routes\":[\"0121\",\"02E9\",\"01C5\",\"0125\",\"012C\"],\"outcome\":\"PASS\"}" << std::endl;
}

void CartClearContentsReplay()
{
    SetCase("cart_clear_contents_lifecycle_20080910", "PK37.05",
        "PK37-selected-20080910-cart-off-retains-status", "ProcessGameModePacketLoop -> production router -> Commerce::Receive");
    struct Restore {
        ro::commerce::State commerce = ro::commerce::state;
        bool cartActive = IsKnownLocalCartActive();
        std::vector<UIChatEvent> chat = g_windowMgr.GetChatEvents();
        ~Restore() {
            PrimeLocalCartStateFromEffectState(cartActive ? 8 : 0);
            ro::commerce::state = std::move(commerce);
            g_windowMgr.ClearChatEvents();
            for (const UIChatEvent& event : chat)
                g_windowMgr.PushChatEvent(event.text.c_str(), event.color, event.channel, event.tick);
        }
    } restore;
    ro::commerce::Reset();
    CGameMode mode;
    TestConnection connection;
    CGameModePacketRouter& router = GetProductionGameModePacketRouter();
    router.Clear();
    RegisterDefaultGameModePacketHandlers(router);
    const auto dispatch = [&](const std::vector<u8>& frame, const char* label) {
        Require(connection.QueueBytes(frame), std::string("could not queue ") + label);
        const auto result = ProcessGameModePacketLoop(mode, connection, router, 1);
        Require(result.packetsProcessed == 1 && result.packetsDispatched == 1
            && connection.BufferedBytes() == 0, std::string("production dispatch failed for ") + label);
    };

    dispatch({0x21,0x01, 12,0, 100,0, 0x40,0x06,0,0, 0x10,0x27,0,0}, "cart counters");
    dispatch({0xE9,0x02,26,0,0x24,1,0xD6,6,10,1,9,0,0,0x80,
        0,0,0,0,0,0,0,0,0,0,0,0}, "normal cart snapshot");
    auto& state = ro::commerce::state;
    state.cart.visible = true;
    state.vending.visible = true;
    state.vendingPending = true;
    state.vendingActive = true;
    state.draft.emplace_back();
    dispatch({0x2B,0x01}, "cart-off clear contents");
    Require(state.cartItems.empty() && !state.cart.visible && !state.vending.visible
        && !state.vendingPending && !state.vendingActive && state.draft.empty(),
        "012B did not clear cart rows and close pending vending/cart UI state");
    Require(state.hasCart && state.cartCount == 12 && state.cartCapacity == 100
        && state.cartWeight == 1600 && state.cartMaxWeight == 10000,
        "012B incorrectly removed cart presence or cleared 0121 status counters");
    Require(IsKnownLocalCartActive(),"012B erased the actor's known cart state");
    ro::commerce::Command open;open.action="cart";
    Require(ro::commerce::Action(open) && state.cart.visible,"cleared cart cannot reopen despite retained entitlement");
    std::cout << "{\"case_id\":\"cart_clear_contents_lifecycle_20080910\",\"contract_id\":\"PK37.05\",\"routes\":[\"0121\",\"02E9\",\"012B\"],\"outcome\":\"PASS\"}" << std::endl;
}

void CartActorPresenceReplay()
{
    SetCase("cart_local_option_presence_20080910", "PK37.05",
        "PK37-local-option-before-clear", "ProcessGameModePacketLoop -> HandleActorStateChange / HandleCartOff");
    CWorld world;
    CPlayer player;
    CGameActor peer;
    CGameMode mode;
    struct Restore {
        CWorld& world; CGameMode& mode;
        u32 aid=g_session.m_aid, gid=g_session.m_gid;
        CMode* previous=SkillPacketReplayAccess::Current();
        int previousType=SkillPacketReplayAccess::CurrentType();
        bool active=IsKnownLocalCartActive();
        ro::commerce::State commerce=ro::commerce::state;
        std::vector<UIChatEvent> chat=g_windowMgr.GetChatEvents();
        ~Restore() {
            PrimeLocalCartStateFromEffectState(0);
            ApplyPendingLocalCartState(mode);
            mode.m_runtimeActors.clear();
            world.m_player=nullptr;mode.m_world=nullptr;
            g_session.m_aid=aid;g_session.m_gid=gid;
            SkillPacketReplayAccess::Restore(previous,previousType);
            PrimeLocalCartStateFromEffectState(active?8:0);
            ro::commerce::state=std::move(commerce);
            g_windowMgr.ClearChatEvents();
            for (const UIChatEvent& event : chat)
                g_windowMgr.PushChatEvent(event.text.c_str(),event.color,event.channel,event.tick);
        }
    } restore{world,mode};
    ro::commerce::Reset();
    g_session.m_aid=1;g_session.m_gid=1;
    player.m_gid=1;peer.m_gid=2;
    world.m_player=&player;mode.m_world=&world;
    mode.m_runtimeActors[2]=&peer;
    SkillPacketReplayAccess::SetCurrent(&mode);
    TestConnection connection;
    auto& router=GetProductionGameModePacketRouter();
    router.Clear();RegisterDefaultGameModePacketHandlers(router);
    const auto dispatch=[&](const std::vector<u8>& frame) {
        Require(connection.QueueBytes(frame),"could not queue cart option fixture");
        const auto result=ProcessGameModePacketLoop(mode,connection,router,1);
        Require(result.packetsProcessed==1 && result.packetsDispatched==1 && connection.BufferedBytes()==0,
            "cart option fixture missed production dispatch");
    };
    constexpr int cartMask=0x0008|0x0080|0x0100|0x0200|0x0400;
    for(const int mask:{0x0008,0x0080,0x0100,0x0200,0x0400}) {
        // Selected 0119: GID@2, body@6, health@8, effect(word)@10, PK@12.
        dispatch({0x19,1,1,0,0,0,0,0,0,0,u8(mask),u8(mask>>8),0});
        Require(ro::commerce::state.hasCart && IsKnownLocalCartActive()
            && (player.m_effectState&cartMask)==mask,"local option did not establish cart presence");
        dispatch({0x2b,1});
        Require(ro::commerce::state.hasCart && IsKnownLocalCartActive()
            && (player.m_effectState&cartMask)==mask,"contents clear removed actor cart state");
    }
    dispatch({0x19,1,2,0,0,0,0,0,0,0,0,0,0});
    Require(ro::commerce::state.hasCart && IsKnownLocalCartActive()
        && (player.m_effectState&cartMask)==0x400,"remote option changed local cart presence");
    // pc_setoption emits the local option change before clif_clearcart.
    dispatch({0x19,1,1,0,0,0,0,0,0,0,0,0,0});
    dispatch({0x2b,1});
    Require(!ro::commerce::state.hasCart && !IsKnownLocalCartActive()
        && !(player.m_effectState&cartMask),"local removal did not clear UI and actor cart presence");
    ro::commerce::Command open;open.action="cart";
    Require(!ro::commerce::Action(open),"removed cart could still reopen");
    std::cout << "{\"case_id\":\"cart_local_option_presence_20080910\",\"contract_id\":\"PK37.05\",\"routes\":[\"0119\",\"012B\"],\"outcome\":\"PASS\"}" << std::endl;
}
