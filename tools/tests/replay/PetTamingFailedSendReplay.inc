    {
        Require(!g_windowMgr.m_petTamingDeceiveWnd,"failed-send taming fixture needs a fresh owner");
        const auto target=g_session.m_petStatus.actorId^0x80000001u;
        mode.SendMsg(76,10000,target,0);
        auto* window=g_windowMgr.m_petTamingDeceiveWnd;
        Require(window!=nullptr,"failed-send taming owner missing");
        window->OnLBtnDown(window->m_x,window->m_y);
        Require(window->IsSelected() && window->GetAction()==1 && window->GetDestination()==7,
            "failed capture send rolled back source first-attempt latch");
        Require(!CRagConnection::instance()->IsWaitingForPetCaptureResult(),
            "failed capture send left an outstanding server result");
        window->OnLBtnDown(window->m_x+1,window->m_y+1);
        Require(g_windowMgr.m_petTamingDeceiveWnd==window && window->IsSelected(),"failed capture click closed/replaced owner");
        g_windowMgr.DeleteWindow(window);

        LoopbackPeer pendingPeer(*connection);
        Require(mode.SendMsg(162,0x12345678,0,0)==1
            && pendingPeer.ReceiveExact(6)==std::vector<u8>({0x9f,1,0x78,0x56,0x34,0x12})
            && connection->IsWaitingForPetCaptureResult(),"could not stage a capture result before disconnect");
        connection->Disconnect();
        Require(!connection->IsWaitingForPetCaptureResult(),"disconnect retained an outstanding capture result");
    }
