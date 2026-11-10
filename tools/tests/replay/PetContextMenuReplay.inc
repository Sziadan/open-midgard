    {
        CWorld world;CGameActor actor;actor.m_gid=0x112233;actor.m_job=1002;actor.m_headType=100;
        world.RegisterActor(&actor);
        const auto original=std::vector<UIWindow*>(g_windowMgr.m_children.begin(),g_windowMgr.m_children.end());
        struct RestorePetContext {
            CGameMode& mode;CWorld& world;CGameActor& actor;CWorld* previousWorld;
            ro::pet::Status pet=g_session.m_petStatus;SubChatWindowInfo info=g_windowMgr.m_petInfoWindowInfo;
            std::list<ITEM_INFO> inventory=g_session.GetInventoryItems();std::vector<int> ids;
            std::string whisper;u32 target;std::vector<UIWindow*> roots;
            ~RestorePetContext(){
                while(true){auto it=std::find_if(g_windowMgr.m_children.begin(),g_windowMgr.m_children.end(),[&](UIWindow* p){return std::find(roots.begin(),roots.end(),p)==roots.end();});if(it==g_windowMgr.m_children.end())break;g_windowMgr.DeleteWindow(*it);}
                mode.m_world=previousWorld;mode.m_menuIdList=ids;mode.m_menuTargetAID=target;mode.m_lastWhisperMenuCharacterName=whisper;
                world.UnregisterActor(&actor);g_session.m_petStatus=pet;g_windowMgr.m_petInfoWindowInfo=info;
                g_session.ClearInventoryItems();for(const auto& item:inventory)g_session.SetInventoryItem(item);
            }
        } restore{mode,world,actor,mode.m_world,g_session.m_petStatus,g_windowMgr.m_petInfoWindowInfo,g_session.GetInventoryItems(),mode.m_menuIdList,mode.m_lastWhisperMenuCharacterName,mode.m_menuTargetAID,original};
        mode.m_world=&world;auto& pet=g_session.m_petStatus;pet={};pet.actorId=actor.m_gid;pet.job=1002;pet.name="Poring";
        Require(!g_windowMgr.m_petInfoWnd && !g_windowMgr.m_chatRoomMenuWnd,"pet context fixture needs fresh owners");
        const auto open=[&](){Require(mode.OpenPetContextMenu(&actor,123,234),"actual owned pet context producer failed");};
        const auto ExpectFrame=[&](decltype(peer.peer.value),std::initializer_list<u8> expected){Require(peer.ReceiveExact(expected.size())==std::vector<u8>(expected),"pet context request frame differs");};
        const auto select=[&](int row){open();auto* menu=g_windowMgr.m_chatRoomMenuWnd;g_windowMgr.OnLBtnDown(menu->m_x+3,menu->m_y+row*14+2);g_windowMgr.OnLBtnUp(menu->m_x+3,menu->m_y+row*14+2);};
        open();auto* menu=g_windowMgr.m_chatRoomMenuWnd;
        Require(mode.m_menuIdList==std::vector<int>({33,29,30,32,31}) && menu->GetItems().size()==5
            && menu->m_x==124 && menu->m_y==235 && menu->m_h==70 && mode.m_menuTargetAID==actor.m_gid,
            "source pet menu rows/order/position/target differ");
        Require(menu->GetItems()[0].label=="Pet Info" && menu->GetItems()[3].label=="Accessory Off","pet context labels differ");
        menu->OnLBtnUp(menu->m_x-1,menu->m_y);Require(g_windowMgr.m_chatRoomMenuWnd==menu,"outside release closed source menu");
        select(0);ExpectFrame(peer.peer.value,{0xa1,0x01,0});Require(g_windowMgr.m_petInfoWnd && !g_windowMgr.m_chatRoomMenuWnd,"info menu did not request0 before creating owner");
        select(0);ExpectFrame(peer.peer.value,{0xa1,0x01,0});Require(!g_windowMgr.m_petInfoWnd,"info toggle did not retire existing owner");
        select(1);Require(g_windowMgr.m_roomMessageBox && g_windowMgr.m_roomMessageBox->GetMessageId()==601,"actor feed lacks confirmation without PetInfo");
        Require(!mode.OpenPetContextMenu(&actor,0,0),"actor menu bypassed pending modal input ownership");
        g_windowMgr.OnKeyDown(VK_ESCAPE);Require(SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(20))==0,"cancelled actor feed sent a frame");
        select(1);g_windowMgr.OnKeyDown(VK_RETURN);ExpectFrame(peer.peer.value,{0xa1,0x01,1});
        select(2);ExpectFrame(peer.peer.value,{0xa1,0x01,2});select(3);ExpectFrame(peer.peer.value,{0xa1,0x01,4});
        // Producer gates are independent of whether a snapshot is marked valid.
        actor.m_headType=0;Require(!mode.OpenPetContextMenu(&actor,0,0),"unmarked actor opened pet menu");actor.m_headType=100;
        ++pet.actorId;Require(!mode.OpenPetContextMenu(&actor,0,0),"another actor opened owned pet menu");--pet.actorId;
        actor.m_job=1;Require(!mode.OpenPetContextMenu(&actor,0,0),"ineligible pick class/job opened pet menu");
        actor.m_objectType=9;open();g_windowMgr.DeleteWindow(g_windowMgr.m_chatRoomMenuWnd);actor.m_objectType=4;actor.m_job=1002;
        world.UnregisterActor(&actor);Require(!mode.OpenPetContextMenu(&actor,0,0),"unregistered actor opened pet menu");world.RegisterActor(&actor);
        ITEM_INFO egg{};egg.SetItemId(9001);egg.m_itemIndex=1234;egg.m_isDamaged=1;g_session.SetInventoryItem(egg);pet.eggIndex=1234;
        g_windowMgr.MakeWindow(UIWindowMgr::WID_PETINFOWND);select(4);ExpectFrame(peer.peer.value,{0xa1,0x01,3});
        Require(pet.job==-1 && pet.eggIndex==-1 && !g_windowMgr.m_petInfoWnd && !g_session.GetInventoryItemByIndex(1234)->m_isDamaged,
            "actor return egg did not apply exact local state and owner cleanup");
        select(0);ExpectFrame(peer.peer.value,{0xa1,0x01,0});
        Require(!g_windowMgr.m_petInfoWnd,"context Info request0 bypassed actual factory job gate");
        mode.m_menuIdList={30};mode.SendMsg(26,-1,0,0);mode.SendMsg(26,1,0,0);
        Require(SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(20))==0,"out-of-range menu selection sent frame");
    }
