void RentalItemPacketReplay()
{
    SetCase("rental_item_counter_and_delete", "PK33.05", "selected-20080910-0298-0299-00af",
        "ProcessGameModePacketLoop -> production router -> rental notifications/removal");
#if RO_ENABLE_QT6_UI
    // This receive family reveals chat, whose real wrapping path needs fonts.
    qputenv("QT_QPA_PLATFORM","offscreen");
#ifdef RO_PACKET_QT_PLATFORM_PLUGINS
    qputenv("QT_QPA_PLATFORM_PLUGIN_PATH",RO_PACKET_QT_PLATFORM_PLUGINS);
#endif
    int argc=1;char name[]="rental-replay";char* argv[]={name,nullptr};QGuiApplication app(argc,argv);
#endif
    struct Restore {
        std::vector<ITEM_INFO> inventory{g_session.GetInventoryItems().begin(),g_session.GetInventoryItems().end()};
        std::vector<UIChatEvent> chat=g_windowMgr.GetChatEvents();
        UINewChatWnd* previousChat=g_windowMgr.m_chatWnd;
        ~Restore(){g_session.ClearInventoryItems();for(const auto& item:inventory)g_session.SetInventoryItem(item);
            if(!previousChat && g_windowMgr.m_chatWnd)g_windowMgr.DeleteWindow(g_windowMgr.m_chatWnd);
            g_windowMgr.ClearChatEvents();for(const auto& e:chat)g_windowMgr.PushChatEvent(e.text.c_str(),e.color,e.channel,e.tick);}
    } restore;
    g_session.ClearInventoryItems();g_windowMgr.ClearChatEvents();
    ITEM_INFO item;item.SetItemId(1201);item.m_itemIndex=291;item.m_itemType=5;item.m_num=3;
    item.m_isIdentified=1;item.m_deleteTime=1704067200;g_session.SetInventoryItem(item);
    CGameMode mode;TestConnection connection;
    auto& router=GetProductionGameModePacketRouter();router.Clear();RegisterDefaultGameModePacketHandlers(router);
    const auto dispatch=[&](const std::vector<u8>& bytes) {
        Require(connection.QueueBytes(bytes),"could not queue rental fixture");
        const auto result=ProcessGameModePacketLoop(mode,connection,router,1);
        Require(result.packetsProcessed==1 && result.packetsDispatched==1 && connection.BufferedBytes()==0,
            "rental packet did not frame/dispatch through production loop");
    };
    // Counter is item ID plus unsigned seconds, not an inventory index/expiry epoch.
    for(const u32 seconds:{0u,59u,60u,119u,120u,0xffffffffu}) {
        const auto before=g_windowMgr.GetChatEvents().size();
        dispatch({0x98,2,0xb1,4,u8(seconds),u8(seconds>>8),u8(seconds>>16),u8(seconds>>24)});
        const auto& events=g_windowMgr.GetChatEvents();
        const bool last=seconds/60==1;
        Require(events.size()==before+1+size_t(last),"rental counter notice count differs from primary branches");
        Require(events[before].color==0xffff00 && events[before].channel==6,"rental notice colour/channel differs");
        Require(events[before].text.find(std::to_string(seconds/60?seconds/60:seconds))!=std::string::npos,
            "rental counter did not use floored minutes or seconds");
        Require(events[before].text.find(seconds/60?"minutes":"seconds")!=std::string::npos,"rental units differ from primary");
        if(last)Require(events.back().text.find("1 minute")!=std::string::npos,"last-minute warning missing");
        Require(g_session.GetInventoryItemByIndex(291)->m_num==3
            && g_session.GetInventoryItemByIndex(291)->m_deleteTime==1704067200,"counter changed inventory quantity/absolute expiry");
    }
    dispatch({0x99,2,0x23,1,0xb1,4});
    Require(g_session.GetInventoryItemByIndex(291) && g_session.GetInventoryItemByIndex(291)->m_num==2,
        "0299 did not apply primary one-item decrement");
    Require(g_windowMgr.GetChatEvents().back().text.find("deleted from the Inventory")!=std::string::npos,
        "rental deletion notice missing");
    // Selected pc_inventory_rentals then emits the full pre-deletion count via 00AF.
    dispatch({0xaf,0,0x23,1,3,0});
    Require(!g_session.GetInventoryItemByIndex(291),"rental follow-up full removal left stale inventory");
    dispatch({0x99,2,0x23,1,0xb1,4});dispatch({0xaf,0,0x23,1,3,0});
    Require(g_session.GetInventoryItems().empty(),"repeated removal resurrected an expired item");
    std::cout<<"{\"case_id\":\"rental_item_counter_and_delete\",\"contract_id\":\"PK33.05\",\"routes\":[\"0298\",\"0299\",\"00AF\"],\"outcome\":\"PASS\"}"<<std::endl;
}
