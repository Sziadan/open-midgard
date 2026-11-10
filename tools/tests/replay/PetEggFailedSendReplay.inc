    {
        const auto savedPet=g_session.m_petStatus;
        const auto savedItems=g_session.GetInventoryItems();
        Require(!g_windowMgr.m_petEggListWnd && !mode.SendMsg(159,32,0,0),"disconnected selected-egg sender reported success");
        ITEM_INFO item{};item.SetItemId(9001);item.m_itemIndex=32;item.m_num=1;item.m_isIdentified=1;
        g_session.SetInventoryItem(item);g_session.m_petStatus.eggIndex=-1;
        const int jobBefore=g_session.m_petStatus.job;
        auto* window=static_cast<UIPetEggListWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_SELECTPETEGGWND));
        window->SendMsg(nullptr,31,0,32,0);
        window->OnLBtnDown(window->m_x+5,window->m_y+35);window->SendMsg(nullptr,6,118,0,0);
        Require(!g_windowMgr.m_petEggListWnd && g_session.m_petStatus.eggIndex==32
            && g_session.m_petStatus.job==jobBefore && g_session.GetInventoryItemByIndex(32)->m_isDamaged==1,
            "failed selected-egg send rolled back local item/index/owner transition or derived pet job");
        g_session.m_petStatus=savedPet;g_session.ClearInventoryItems();for(const auto& row:savedItems)g_session.SetInventoryItem(row);
    }
