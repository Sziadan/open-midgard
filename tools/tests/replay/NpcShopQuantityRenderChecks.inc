void CheckNpcShopQuantityUi(QGuiApplication& app,QQuickView& view,QtUiState* state,CGameMode& mode,
    QtUiStateAdapter& adapter,const QString& output)
{
    SetRuntimeUiScalePercent(100);QJsonArray cases;int frames=0;
    if(g_windowMgr.m_itemWnd)g_windowMgr.m_itemWnd->SetShow(0);
    if(g_windowMgr.m_storageWnd)g_windowMgr.m_storageWnd->SetShow(0);
    ro::commerce::Reset();g_windowMgr.CloseNpcDialogWindows();g_windowMgr.CloseNpcShopWindows();
    if(g_windowMgr.m_itemInfoWnd)g_windowMgr.DeleteWindow(g_windowMgr.m_itemInfoWnd);
    const auto sync=[&] {
        if(g_windowMgr.m_itemShopWnd)g_windowMgr.m_itemShopWnd->Move(20,100);
        if(g_windowMgr.m_itemPurchaseWnd)g_windowMgr.m_itemPurchaseWnd->Move(450,100);
        Require(adapter.syncGameplay(mode,RenderBackendType::LegacyDirect3D7,RenderBackendType::LegacyDirect3D7,Width-1,Height-1),"NPC quantity sync failed");Frames(app,view);
    };
    const auto quote=[&](u8 type) {
        // The type permutations are wire/classification probes, not item legality.
        Dispatch(mode,{0xc4,0,0x44,0x33,0x22,0x11});
        Dispatch(mode,{0xc6,0,15,0,100,0,0,0,77,0,0,0,type,0xf5,1});sync();
    };
    const auto doubleClick=[&] {
        auto* row=Find(view.rootObject(),"npcShopSourceRow_0");Require(row&&row->isVisible(),"NPC quantity source delegate missing");
        const QPointF pt=row->mapToScene(QPointF(row->width()/2,row->height()/2));
        const LPARAM point=MAKELPARAM(WORD(qRound(pt.x())),WORD(qRound(pt.y())));
        WindowProc(nullptr,WM_LBUTTONDBLCLK,MK_LBUTTON,point);WindowProc(nullptr,WM_LBUTTONUP,0,point);sync();
    };
    const auto save=[&](const QString& name) {
        Require(view.grabWindow().save(output+"/"+name+"-qt.png"),"NPC quantity frame save failed");++frames;
    };
    quote(11);doubleClick();
    auto* input=g_windowMgr.m_npcInputWnd;
    Require(input&&input->m_show&&input->IsGameNumberPromptFor(CGameMode::GameMsg_RequestShopAdjustDeal)
        &&state->npcInputVisible()&&g_session.m_shopDealRows.empty(),"stackable delay-consume did not open its count prompt");
    Require(!state->npcInputLabel().isEmpty()&&state->npcInputPlaceholder()=="amount","actual Qt quantity prompt is missing its label/placeholder");
    save("npc-delay-consume-prompt");
    input->GetEditCtrl()->SetText("3");Require(input->HandleKeyDown(VK_RETURN),"native count confirmation was not handled");sync();
    Require(!state->npcInputVisible()&&g_session.m_shopDealRows.size()==1
        &&g_session.m_shopDealRows[0].quantity==3&&g_session.m_shopDealRows[0].itemInfo.m_num==3,"typed count did not reach production deal adjustment");
    save("npc-delay-consume-selected");
    ArgbDibSurface surface;Require(surface.EnsureSize(Width,Height),"NPC quantity native surface failed");
    std::fill_n(surface.GetPixels(),Width*Height,0xff181b20u);g_windowMgr.m_itemPurchaseWnd->DrawNativeToHdc(surface.GetDC());
    const QImage native=QImage(reinterpret_cast<uchar*>(surface.GetPixels()),Width,Height,Width*4,QImage::Format_RGB32).copy();
    Require(native.save(output+"/npc-delay-consume-selected-native.png"),"NPC quantity native frame save failed");++frames;
    cases.push_back(QJsonObject{{"case","delay-consume native double-click, visible Qt count prompt and typed quantity"},{"outcome","PASS"}});

    doubleClick();Require(state->npcInputVisible(),"replacement fixture count prompt did not reopen");
    Dispatch(mode,{0xc6,0,15,0,100,0,0,0,77,0,0,0,0,0xf5,1});sync();
    Require(!state->npcInputVisible()&&!input->IsGameNumberPromptFor(CGameMode::GameMsg_RequestShopAdjustDeal)
        &&!input->HandleKeyDown(VK_RETURN)&&g_session.m_shopDealRows.empty(),"replaced quote retained a stale count callback");
    cases.push_back(QJsonObject{{"case","new C6 list invalidates prior quantity prompt and callback"},{"outcome","PASS"}});

    for(u8 type:{u8(4),u8(5),u8(7),u8(8),u8(12)}) {
        quote(type);doubleClick();
        Require(!state->npcInputVisible()&&g_session.m_shopDealRows.size()==1&&g_session.m_shopDealRows[0].quantity==1,
            "nonstackable shop type opened a count prompt or selected multiple items");
        Require(!g_session.AdjustNpcShopDealBySourceRow(0,10)&&g_session.m_shopDealRows[0].quantity==1,"nonstackable Add exceeded one item");
    }
    for(u8 type:{u8(0),u8(2),u8(3),u8(6),u8(9),u8(10),u8(11),u8(13),u8(14),u8(15),u8(18)}) {
        quote(type);doubleClick();Require(state->npcInputVisible()&&g_session.m_shopDealRows.empty(),"stackable type skipped its quantity prompt");
        Require(input->HandleKeyDown(VK_ESCAPE),"count prompt cancel was not handled");sync();
        Require(!state->npcInputVisible()&&g_session.m_shopDealRows.empty(),"count prompt cancellation changed the deal");
    }
    cases.push_back(QJsonObject{{"case","five nonstackable and eleven stackable type probes share prompt and quantity limits"},{"outcome","PASS"}});
    quote(11);doubleClick();Require(state->npcInputVisible(),"local cancellation fixture prompt missing");
    Require(mode.SendMsg(CGameMode::GameMsg_RequestShopCancel,0,0,0)!=0,"local shop cancel failed without transport");sync();
    Require(!state->npcInputVisible()&&!state->itemShopVisible()&&!state->itemPurchaseVisible()
        &&!input->IsGameNumberPromptFor(CGameMode::GameMsg_RequestShopAdjustDeal)&&g_session.m_shopDealRows.empty(),"shop cancellation retained count prompt or shop panes");
    save("npc-shop-cancelled");
    cases.push_back(QJsonObject{{"case","local shop cancellation closes Qt panes and quantity callback"},{"outcome","PASS"}});
    QFile result(output+"/npc-shop-quantity-results.json");Require(result.open(QIODevice::WriteOnly),"NPC quantity result open failed");
    result.write(QJsonDocument(QJsonObject{{"cases",cases},{"type_probes",16},{"generated_frames",frames},{"outcome","PASS"}}).toJson());
}
