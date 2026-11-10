    {
        Require(!g_windowMgr.m_petInfoWnd,"pet window fixture requires fresh owner");
        const auto savedPet=g_session.m_petStatus;
        const auto savedInfo=g_windowMgr.m_petInfoWindowInfo;
        const auto savedItems=g_session.GetInventoryItems();
        const std::vector<UIWindow*> original(g_windowMgr.m_children.begin(),g_windowMgr.m_children.end());
        struct RestorePetWindow {
            ro::pet::Status pet;SubChatWindowInfo info;std::list<ITEM_INFO> items;std::vector<UIWindow*> roots;
            ~RestorePetWindow(){
                while(true){auto it=std::find_if(g_windowMgr.m_children.begin(),g_windowMgr.m_children.end(),[&](UIWindow* p){return std::find(roots.begin(),roots.end(),p)==roots.end();});if(it==g_windowMgr.m_children.end())break;g_windowMgr.DeleteWindow(*it);}
                g_session.m_petStatus=pet;g_windowMgr.m_petInfoWindowInfo=info;
                g_session.ClearInventoryItems();for(const auto& item:items)g_session.SetInventoryItem(item);
            }
        } restorePet{savedPet,savedInfo,savedItems,original};
        g_session.m_petStatus={};g_session.m_petStatus.valid=true;
        Require(!g_windowMgr.MakeWindow(UIWindowMgr::WID_PETINFOWND),"stale valid status created pet window without job");
        if(!g_windowMgr.m_basicInfoWnd)g_windowMgr.MakeWindow(UIWindowMgr::WID_BASICINFOWND);
        Require(g_windowMgr.ProcessSourceCommandWindowKey(74) && !g_windowMgr.m_petInfoWnd,"job gate differs on source Alt+J route");
        auto& pet=g_session.m_petStatus;pet.job=1002;pet.name="Poring";pet.level=12;pet.fullness=25;pet.relationship=750;
        Require(g_windowMgr.ProcessSourceCommandWindowKey(74),"source pet key was not handled");
        auto* window=g_windowMgr.m_petInfoWnd;Require(window && window->m_w==280 && window->m_h==160 && window->m_children.size()==5,"actual pet factory/controller/child order missing");
        g_windowMgr.m_petInfoWindowInfo={-1,-1,0,0};window->SendMsg(nullptr,34,reinterpret_cast<msgparam_t>(&g_windowMgr.m_petInfoWindowInfo),0,0);
        Require(window->m_x==150 && window->m_y==100 && g_windowMgr.m_petInfoWindowInfo.height==200 && window->m_h==160,"pet default rect incorrectly resized/clamped the actual window");
        Require(g_windowMgr.MakeWindow(UIWindowMgr::WID_PETINFOWND)==window,"pet factory did not reuse its actual owner");
        Require(SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(20))==0,"source hotkey sent menu status request0");
        window->OnKeyDown(VK_RETURN);window->OnKeyDown(VK_ESCAPE);Require(g_windowMgr.m_petInfoWnd==window,"source default117 closed pet window");
        const auto click=[&](UIWindow* control){const int x=control->m_x+1,y=control->m_y+1;g_windowMgr.OnLBtnDown(x,y);g_windowMgr.OnLBtnUp(x,y);};
        auto* edit=window->GetNameEdit();Require(edit->m_w==80 && edit->m_h==16 && edit->m_maxchar==50,"pet editor source dimensions/limit differ");
        edit->SetText("");click(edit);g_windowMgr.OnChar('A');g_windowMgr.OnChar('B');g_windowMgr.OnKeyDown(VK_BACK);g_windowMgr.OnChar('\b');
        Require(std::string(edit->GetText())=="A","pet native key/char pair deleted more than one character");
        edit->SetText(std::string(24,'Z').c_str());click(window->GetButtons()[1]);
        Require(g_windowMgr.m_roomMessageBox && g_windowMgr.m_roomMessageBox->GetMessageId()==682 && !g_windowMgr.m_roomMessageBox->HasCancel(),"24-byte name skipped source error dialog");
        g_windowMgr.OnKeyDown(VK_RETURN);Require(!g_windowMgr.m_modalWindow && !pet.modified,"name error mutated state");
        const std::string proposed(23,'N');edit->SetText(proposed.c_str());click(window->GetButtons()[1]);
        Require(g_windowMgr.m_roomMessageBox && g_windowMgr.m_roomMessageBox->GetMessageId()==683 && g_windowMgr.m_roomMessageBox->HasCancel(),"rename did not await actual confirmation");
        auto* modal=g_windowMgr.m_roomMessageBox;
        Require(g_windowMgr.MakeWindow(UIWindowMgr::WID_PETINFOWND)==window && g_windowMgr.m_children.back()==modal,"reused pet factory raised owner above its modal");
        Require(SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(20))==0,"rename was sent before confirmation");
        g_windowMgr.OnKeyDown(VK_ESCAPE);Require(!g_windowMgr.m_modalWindow && !pet.modified && g_windowMgr.m_editWindow==edit,"rename cancellation lost state/focus");
        click(window->GetButtons()[1]);const std::string accepted(23,'R');edit->SetText(accepted.c_str());g_windowMgr.OnKeyDown(VK_RETURN);
        const auto renamed=peer.ReceiveExact(26);Require(renamed[0]==0xa5 && renamed[1]==1 && std::equal(accepted.begin(),accepted.end(),renamed.begin()+2) && renamed[25]==0,"actual pet rename emitted wrong source frame or did not reread after confirmation");
        Require(pet.modified==1 && pet.name==accepted && edit->m_x==window->m_x-100 && window->GetButtons()[1]->m_x==window->m_x-100,"source optimistic rename did not hide actual controls");
        auto received=std::vector<u8>(35);received[0]=0xa2;received[1]=1;std::memcpy(received.data()+2,"ServerPet",9);received[27]=12;
        CGameModePacketRouter receiver;RegisterDefaultGameModePacketHandlers(receiver);
        Require(receiver.Dispatch(mode,{0x01a2,35,received.data()}),"production pet property receiver was not registered");
        Require(std::string(edit->GetText())=="ServerPet" && pet.name=="ServerPet" && edit->m_x==window->m_x-100,"property receipt did not update source editor or invented a visibility reset");
        window->ShowCommandMenu();auto* menu=window->GetCommandMenu();Require(menu && menu->m_w==110 && menu->m_h==64,"actual four-row pet menu missing");
        Require(menu->GetLabels()[2]=="Accessory Off" && menu->GetLabels()[3]=="Return Egg","source pet menu insertion order differs");
        const auto select=[&](int index){window->ShowCommandMenu();auto* item=window->GetCommandMenu();g_windowMgr.OnLBtnDown(item->m_x+3,item->m_y+index*16+3);g_windowMgr.OnLBtnUp(item->m_x+3,item->m_y+index*16+3);};
        select(0);Require(!window->GetCommandMenu() && g_windowMgr.m_roomMessageBox && g_windowMgr.m_roomMessageBox->GetMessageId()==601,"feed skipped source confirmation");
        g_windowMgr.OnKeyDown(VK_ESCAPE);Require(SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(20))==0,"cancelled feed emitted a request");
        select(0);g_windowMgr.OnKeyDown(VK_RETURN);Require(peer.ReceiveExact(3)==std::vector<u8>({0xa1,1,1}),"confirmed feed frame differs");
        select(1);Require(peer.ReceiveExact(3)==std::vector<u8>({0xa1,1,2}),"performance menu mapping differs");
        select(2);Require(peer.ReceiveExact(3)==std::vector<u8>({0xa1,1,4}),"accessory-off menu mapping differs");
        // Actual owner deletion while its callback is pending must retire the
        // modal before its captured owner pointer can be used.
        select(0);g_windowMgr.DeleteWindow(window);Require(!g_windowMgr.m_petInfoWnd && !g_windowMgr.m_modalWindow && !g_windowMgr.m_roomMessageBox,"pet deletion retained modal/callback");
        g_windowMgr.OnKeyDown(VK_RETURN);Require(SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(20))==0,"deleted pet callback sent feed later");
        window=static_cast<UIPetInfoWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_PETINFOWND));
        const int edge=g_windowMgr.GetChatClientRect().bottom-160;window->Move(150,edge);g_windowMgr.DeleteWindow(window);
        g_windowMgr.m_petInfoWindowInfo={150,100,280,200};window=static_cast<UIPetInfoWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_PETINFOWND));
        Require(window->m_y==edge && g_windowMgr.m_petInfoWindowInfo.height==160,"saved pet height was lost and invalidated a valid bottom-edge position");
        window->Move(150,100);
        ITEM_INFO egg{};egg.m_itemIndex=4321;egg.SetItemId(9001);egg.m_isDamaged=1;egg.m_num=1;g_session.SetInventoryItem(egg);pet.eggIndex=4321;
        window->ShowCommandMenu();auto* returnMenu=window->GetCommandMenu();g_windowMgr.OnLBtnDown(returnMenu->m_x+3,returnMenu->m_y+51);g_windowMgr.OnLBtnUp(returnMenu->m_x+3,returnMenu->m_y+51);
        Require(peer.ReceiveExact(3)==std::vector<u8>({0xa1,1,3}) && pet.job==-1 && pet.eggIndex==-1 && !g_windowMgr.m_petInfoWnd,"return egg did not send and retire actual owner");
        Require(g_session.GetInventoryItemByIndex(4321) && !g_session.GetInventoryItemByIndex(4321)->m_isDamaged,"return egg did not update exact inventory record");
    }
