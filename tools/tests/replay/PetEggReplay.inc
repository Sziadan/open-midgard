    {
        Require(!g_windowMgr.m_petEggListWnd,"egg replay needs a fresh selector owner");
        const std::vector<UIWindow*> roots(g_windowMgr.m_children.begin(),g_windowMgr.m_children.end());
        struct RestorePetEgg {
            std::vector<UIWindow*> roots;
            ro::pet::Status pet=g_session.m_petStatus;
            SubChatWindowInfo info=g_windowMgr.m_petInfoWindowInfo;
            std::list<ITEM_INFO> inventory=g_session.GetInventoryItems();
            ~RestorePetEgg(){
                while(true){auto it=std::find_if(g_windowMgr.m_children.begin(),g_windowMgr.m_children.end(),[&](UIWindow* p){return std::find(roots.begin(),roots.end(),p)==roots.end();});if(it==g_windowMgr.m_children.end())break;g_windowMgr.DeleteWindow(*it);}
                g_session.m_petStatus=pet;g_windowMgr.m_petInfoWindowInfo=info;
                g_session.ClearInventoryItems();for(const auto& item:inventory)g_session.SetInventoryItem(item);
            }
        } restore{roots};
        const auto originalEggIndex=g_session.m_petStatus.eggIndex;
        for(const auto index:std::array<msgparam_t,4>{{0,0x1234,-1,0x12345678}}){
            Require(mode.SendMsg(159,index,0,0)==1,"selected-egg source sender failed");
            Require(peer.ReceiveExact(4)==std::vector<u8>({0xa7,1,static_cast<u8>(index),static_cast<u8>(static_cast<u16>(index)>>8)}),"selected-egg low-word frame differs");
            Require(g_session.m_petStatus.eggIndex==originalEggIndex,"mode159 sender mutated local egg ownership");
        }
        g_session.ClearInventoryItems();g_session.m_petStatus.job=1002;g_session.m_petStatus.eggIndex=-1;
        ITEM_INFO first{};first.SetItemId(9001);first.m_itemIndex=7;first.m_num=1;first.m_isIdentified=1;first.m_slot[0]=4001;
        ITEM_INFO second=first;second.SetItemId(9002);second.m_itemIndex=8;second.m_num=0;second.m_isIdentified=0;
        ITEM_INFO ordinary=first;ordinary.SetItemId(501);ordinary.m_itemIndex=9;
        ITEM_INFO negative=first;negative.SetItemId(9003);negative.m_itemIndex=0xffff8001u;
        for(const auto& item:{first,second,ordinary,negative})g_session.SetInventoryItem(item);
        CGameModePacketRouter receiver;RegisterDefaultGameModePacketHandlers(receiver);TestConnection incoming;
        const auto frame=[](std::initializer_list<u16> indices){
            std::vector<u8> bytes(4+2*indices.size());bytes[0]=0xa6;bytes[1]=1;
            bytes[2]=static_cast<u8>(bytes.size());bytes[3]=static_cast<u8>(bytes.size()>>8);std::size_t at=4;
            for(const auto index:indices){bytes[at++]=static_cast<u8>(index);bytes[at++]=static_cast<u8>(index>>8);}return bytes;
        };
        const auto receive=[&](const std::vector<u8>& bytes){
            Require(incoming.QueueBytes(bytes),"egg frame queue failed");
            const auto result=ProcessGameModePacketLoop(mode,incoming,receiver,1);
            Require(result.packetsProcessed==1 && result.packetsDispatched==1 && !incoming.BufferedBytes(),"egg frame did not dispatch exactly once");
        };
        receive(frame({7,7,8,9,0,123,0x8001}));
        auto* window=g_windowMgr.m_petEggListWnd;
        const int defaultX=window?window->m_x:0,defaultY=window?window->m_y:0;
        Require(window && window->m_w==200 && window->m_h==200 && window->m_children.size()==3
            && window->GetItems().size()==5 && window->GetCurrentItem()==-1,"source egg factory/list filters/default selection differ");
        Require(window->GetItems()[0].m_itemIndex==7 && window->GetItems()[1].m_itemIndex==7
            && window->GetItems()[2].m_num==0 && !window->GetItems()[2].m_isIdentified
            && window->GetItems()[3].GetItemId()==501 && window->GetItems()[4].m_itemIndex==0xffff8001u,
            "egg list deduplicated, imposed item/identify/count filters or lost signed index lookup");
        window->SendMsg(nullptr,6,118,0,0);Require(g_windowMgr.m_petEggListWnd==window,"unselected OK closed source selector");
        Require(window->GetCurItem(4,34)==0 && window->GetCurItem(186,161)==3
            && window->GetCurItem(3,34)==-1 && window->GetCurItem(187,34)==-1 && window->GetCurItem(4,33)==-1 && window->GetCurItem(4,162)==-1,
            "source egg hit rectangle boundaries differ");
        window->OnLBtnDown(window->m_x+5,window->m_y+67);window->OnWheel(-1200);
        Require(window->GetCurrentItem()==1 && window->GetOffset()==1,"source wheel sign/row selection differs");
        window->OnKeyDown(VK_RETURN);window->OnKeyDown(VK_ESCAPE);
        window->SendMsg(nullptr,0,0,0,0);window->SendMsg(nullptr,1,0,0,0);
        Require(g_windowMgr.m_petEggListWnd==window && SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(20))==0,
            "invented default key/message submission or cancellation");
        window->Move(40,50);
        first.SetItemId(9010);first.m_num=6;g_session.SetInventoryItem(first);
        auto* info=g_windowMgr.MakeWindow(UIWindowMgr::WID_PETINFOWND);
        receive(frame({7}));receive(frame({}));auto odd=frame({});odd.push_back(0xee);odd[2]=5;receive(odd);
        Require(g_windowMgr.m_petEggListWnd==window && window->GetItems().size()==6 && window->GetCurrentItem()==1
            && window->GetOffset()==1 && window->GetItems().back().GetItemId()==9010 && g_windowMgr.m_children.back()==info,
            "message75 cleared/reset/raised source egg selector or odd tail changed row count");
        Require(window->m_x==40 && window->m_y==50,"reuse reset existing selector placement");
        auto bad=frame({7});bad[2]=8;receiver.Dispatch(mode,{0x01a6,static_cast<u16>(bad.size()),bad.data()});
        bad=frame({7});bad[1]=0;receiver.Dispatch(mode,{0x01a6,static_cast<u16>(bad.size()),bad.data()});
        bad.assign(3,0);receiver.Dispatch(mode,{0x01a6,3,bad.data()});
        bad.assign(0x8000,0);bad[0]=0xa6;bad[1]=1;bad[3]=0x80;receiver.Dispatch(mode,{0x01a6,0x8000,bad.data()});
        Require(window->GetItems().size()==6 && window->GetCurrentItem()==1,"invalid/signed-unsafe egg frame mutated existing owner");
        g_session.ClearInventoryItems();window->SendMsg(nullptr,6,118,0,0);
        Require(peer.ReceiveExact(4)==std::vector<u8>({0xa7,1,7,0}),"actual selector sent wrong selected index");
        const auto* restored=g_session.GetInventoryItemByIndex(7);
        Require(!g_windowMgr.m_petEggListWnd && restored && restored->GetItemId()==9001 && restored->m_num==1
            && restored->m_slot[0]==4001 && restored->m_isIdentified && restored->m_isDamaged==1
            && g_session.m_petStatus.eggIndex==7 && g_session.m_petStatus.job==1002 && g_windowMgr.m_petInfoWnd==info,
            "selection did not restore its copied snapshot, mark exact egg, preserve pet job or retire only selector");
        receive(frame({7}));window=g_windowMgr.m_petEggListWnd;
        Require(window->m_x==defaultX && window->m_y==defaultY && window->GetItems().size()==1 && window->GetOffset()==0 && window->GetCurrentItem()==-1,
            "new selector restored old placement or old selection/list state");
        window->SendMsg(nullptr,6,119,0,0);
        Require(!g_windowMgr.m_petEggListWnd && g_session.m_petStatus.eggIndex==7
            && SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(20))==0,"cancel sent a request or reset remembered egg");
        receive(frame({}));window=g_windowMgr.m_petEggListWnd;
        Require(window && window->GetItems().empty() && window->GetCurrentItem()==-1,"fresh empty list did not create empty source selector");
        window->SendMsg(nullptr,6,119,0,0);
        g_session.SetInventoryItem(ordinary);receive(frame({7,9}));window=g_windowMgr.m_petEggListWnd;
        window->OnLBtnDblClk(window->m_x+5,window->m_y+35);
        Require(g_windowMgr.m_petEggListWnd==window,"double-click invented a selection when none existed");
        window->OnLBtnDown(window->m_x+5,window->m_y+67);
        window->OnLBtnDblClk(window->m_x+5,window->m_y+35);
        Require(peer.ReceiveExact(4)==std::vector<u8>({0xa7,1,9,0}) && !g_windowMgr.m_petEggListWnd
            && g_session.m_petStatus.eggIndex==9,"double-click reselected from its coordinates instead of using existing source row");
    }
