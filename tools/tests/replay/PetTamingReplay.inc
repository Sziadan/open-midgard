    {
        Require(!g_windowMgr.m_petTamingDeceiveWnd,"taming replay requires a fresh owner");
        const auto savedPet=g_session.m_petStatus;
        struct RestoreTamingPet {ro::pet::Status pet;~RestoreTamingPet(){
            if(g_windowMgr.m_petTamingDeceiveWnd)g_windowMgr.DeleteWindow(g_windowMgr.m_petTamingDeceiveWnd);
            g_session.m_petStatus=pet;}} restore{savedPet};
        g_session.m_petStatus.actorId=0x12345678;
        CGameModePacketRouter receiver;RegisterDefaultGameModePacketHandlers(receiver);TestConnection incoming;
        const auto result=[&](u8 outcome){
            const std::vector<u8> bytes{0xa0,1,outcome};Require(incoming.QueueBytes(bytes),"taming result queue failed");
            const auto processed=ProcessGameModePacketLoop(mode,incoming,receiver,1);
            Require(processed.packetsProcessed==1 && processed.packetsDispatched==1 && !incoming.BufferedBytes(),"registered taming result framing failed");
        };
        result(1);Require(!g_windowMgr.m_petTamingDeceiveWnd,"orphan capture result created owner");
        mode.SendMsg(76,9999,0x4567,1);mode.SendMsg(76,10000,0x12345678,0);
        Require(!g_windowMgr.m_petTamingDeceiveWnd,"mode76 ignored skill/owned-pet exclusion");
        mode.SendMsg(76,10000,static_cast<msgparam_t>(0x80000001u),0);
        auto* window=g_windowMgr.m_petTamingDeceiveWnd;
        Require(window && window->m_id==91 && window->m_w==196 && window->m_h==256 && window->m_children.empty(),
            "source factory91 geometry/child contract differs");
        const auto client=g_windowMgr.GetChatClientRect();
        Require(window->m_x==client.right/2-98 && window->m_y==client.bottom/2-128
            && window->GetTargetGid()==0x80000001u && window->GetAction()==0 && window->GetMotionTick()==0
            && window->GetDelay()==1 && window->GetMaxDelay()==10000 && window->GetDestination()==-1
            && window->GetStartTick()==0xffffffffu && !window->IsSelected(),"new taming owner defaults differ");
        Require(SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(20))==0,"mode76 emitted request before taming click");
        window->Move(window->m_x+19,window->m_y+23);const int x=window->m_x,y=window->m_y;
        window->OnKeyDown(VK_RETURN);window->OnKeyDown(VK_ESCAPE);
        Require(g_windowMgr.m_petTamingDeceiveWnd==window && !window->IsSelected(),"taming owner invented selector key actions");
        g_windowMgr.OnLBtnDown(x+4,y+4);g_windowMgr.OnLBtnUp(x+4,y+4);
        Require(peer.ReceiveExact(6)==std::vector<u8>({0x9f,1,1,0,0,0x80}),"first taming click sent wrong GID/frame");
        Require(window->IsSelected() && window->GetAction()==1 && window->GetDestination()==7
            && window->GetMotionTick()==0 && window->GetDelay()==1 && window->GetMaxDelay()==10000,
            "first taming click changed animation fields outside source latch/action/destination");
        mode.SendMsg(76,10000,0x76543210,0);
        Require(g_windowMgr.m_petTamingDeceiveWnd==window && window->GetTargetGid()==0x76543210u
            && window->m_x==x && window->m_y==y && window->IsSelected(),"reuse recreated/repositioned/reset selected taming owner");
        g_windowMgr.OnLBtnDown(x+20,y+20);g_windowMgr.OnLBtnUp(x+20,y+20);
        Require(SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(20))==0,"reused selected taming owner retried capture");
        result(0);Require(window->GetDestination()==3 && window->GetDelay()==7 && window->GetMaxDelay()==7,"zero capture result mismatch");
        result(1);Require(window->GetDestination()==7,"nonzero capture result mismatch");
        result(0xff);Require(window->GetDestination()==7,"high-bit capture result was not treated as nonzero");
        const u8 shortFrame[]={0xa0,1};const u8 wrongOpcode[]={0xa1,1,0};const u8 longFrame[]={0xa0,1,0,0};
        for(const auto& packet:std::array<PacketView,4>{{{0x01a0,2,shortFrame},{0x01a0,3,wrongOpcode},
                {0x01a0,4,longFrame},{0x01a0,3,nullptr}}})receiver.Dispatch(mode,packet);
        Require(window->GetDestination()==7 && window->GetDelay()==7,"malformed capture result mutated owner");
        Require(g_session.m_petStatus.actorId==0x12345678u && g_session.m_petStatus.job==savedPet.job
            && g_session.m_petStatus.eggIndex==savedPet.eggIndex,"capture result derived pet presence/egg/job");
        g_windowMgr.m_captureWindow=window;g_windowMgr.DeleteWindow(window);
        Require(!g_windowMgr.m_petTamingDeceiveWnd && !g_windowMgr.m_captureWindow,"deleted taming owner retained manager capture");
        result(0);Require(!g_windowMgr.m_petTamingDeceiveWnd,"late capture result recreated owner");
    }
