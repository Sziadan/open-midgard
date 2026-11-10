    {
        Require(!g_windowMgr.m_petTamingDeceiveWnd,"failed-send taming fixture needs a fresh owner");
        const auto target=g_session.m_petStatus.actorId^0x80000001u;
        mode.SendMsg(76,10000,target,0);
        auto* window=g_windowMgr.m_petTamingDeceiveWnd;
        Require(window!=nullptr,"failed-send taming owner missing");
        window->OnLBtnDown(window->m_x,window->m_y);
        Require(window->IsSelected() && window->GetAction()==1 && window->GetDestination()==7,
            "failed capture send rolled back source first-attempt latch");
        window->OnLBtnDown(window->m_x+1,window->m_y+1);
        Require(g_windowMgr.m_petTamingDeceiveWnd==window && window->IsSelected(),"failed capture click closed/replaced owner");
        g_windowMgr.DeleteWindow(window);
    }
