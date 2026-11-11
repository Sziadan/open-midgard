    {
        const auto savedPet=g_session.m_petStatus;const auto savedInfo=g_windowMgr.m_petInfoWindowInfo;
        g_session.m_petStatus={};g_session.m_petStatus.job=1002;g_session.m_petStatus.name="Pet";
        g_session.m_petStatus.valid=true;g_session.m_petStatus.actorId=0x66550003;
        auto* window=static_cast<UIPetInfoWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_PETINFOWND));
        window->GetNameEdit()->SetText("FailedSend");window->RequestRename();Require(g_windowMgr.m_roomMessageBox,"failed-send rename confirmation missing");
        g_windowMgr.OnKeyDown(VK_RETURN);
        Require(g_session.m_petStatus.modified && g_session.m_petStatus.name=="FailedSend","failed source send rolled back optimistic pet rename");
        window->SendMsg(nullptr,39,3,0,0);Require(!g_windowMgr.m_petInfoWnd && g_session.m_petStatus.job==-1 && g_session.m_petStatus.eggIndex==-1,"failed source return send retained pet/window state");
        Require(!g_session.m_petStatus.valid && g_session.m_petStatus.actorId==0,
            "failed return-to-egg retained the retired pet actor identity");
        const auto savedMenuIds=mode.m_menuIdList;mode.m_menuIdList={33,31};
        g_session.m_petStatus.job=1002;mode.SendMsg(26,0,0,0);
        Require(g_windowMgr.m_petInfoWnd,"failed context info request suppressed local owner creation");
        mode.SendMsg(26,0,0,0);Require(!g_windowMgr.m_petInfoWnd,"failed context info request suppressed toggle deletion");
        mode.SendMsg(26,0,0,0);mode.SendMsg(26,1,0,0);
        Require(!g_windowMgr.m_petInfoWnd && g_session.m_petStatus.job==-1 && g_session.m_petStatus.eggIndex==-1,
            "failed context return request rolled back local pet/owner cleanup");
        mode.m_menuIdList=savedMenuIds;g_session.m_petStatus=savedPet;g_windowMgr.m_petInfoWindowInfo=savedInfo;
    }
