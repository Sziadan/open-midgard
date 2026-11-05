void CheckNpcShopDetails(QGuiApplication& app,QQuickView& view,QtUiState* uiState,CGameMode& mode,
    QtUiStateAdapter& adapter,const QString& output)
{
    Require(uiState,"NPC shop detail state missing");
    struct ScaleRestore { int value=GetConfiguredUiScalePercent();~ScaleRestore(){SetRuntimeUiScalePercent(value);} } scaleRestore;
    SetRuntimeUiScalePercent(100);Require(QDir().mkpath(output),"NPC shop detail output directory failed");
    QJsonArray cases;int frameCount=0;
    const auto sync=[&] {
        Require(adapter.syncGameplay(mode,RenderBackendType::LegacyDirect3D7,RenderBackendType::LegacyDirect3D7,Width-1,Height-1),
            "NPC shop detail gameplay sync failed");Frames(app,view);
    };
    const auto write16=[](std::vector<u8>& bytes,size_t offset,u16 value) {bytes[offset]=u8(value);bytes[offset+1]=u8(value>>8);};
    const auto write32=[](std::vector<u8>& bytes,size_t offset,u32 value) {for(int b=0;b<4;++b)bytes[offset+size_t(b)]=u8(value>>(8*b));};
    const auto buyList=[&](const std::vector<std::pair<u8,u16>>& rows) {
        std::vector<u8> bytes(4+11*rows.size(),0);write16(bytes,0,0x00c6);write16(bytes,2,static_cast<u16>(bytes.size()));
        for(size_t i=0;i<rows.size();++i) {const size_t at=4+i*11;write32(bytes,at,90+u32(i));write32(bytes,at+4,80+u32(i));bytes[at+8]=rows[i].first;write16(bytes,at+9,rows[i].second);}
        return bytes;
    };
    const auto sellList=[&](u16 inventoryIndex,u32 price,u32 secondary) {
        std::vector<u8> bytes(14,0);write16(bytes,0,0x00c7);write16(bytes,2,14);write16(bytes,4,inventoryIndex);write32(bytes,6,price);write32(bytes,10,secondary);return bytes;
    };
    const auto openShop=[&](u32 npcId) {Dispatch(mode,{0xc4,0,u8(npcId),u8(npcId>>8),u8(npcId>>16),u8(npcId>>24)});};
    const auto sendRightClick=[&](QQuickItem* target,const char* label) {
        Require(target&&target->isVisible(),"NPC shop QML row is not visible");
        const auto rendered=target->property("modelData").toMap();const u32 renderedId=rendered.value("itemId").toUInt();
        const QPointF scene=target->mapToScene(QPointF(target->width()/2,target->height()/2));
        const int x=qRound(scene.x()),y=qRound(scene.y());const LPARAM point=MAKELPARAM(static_cast<WORD>(x),static_cast<WORD>(y));
        const int dragBefore=mode.m_dragType;
        // Go through the same WindowProc route as QtPlatformWindow's live input.
        WindowProc(nullptr,WM_RBUTTONDOWN,MK_RBUTTON,point);WindowProc(nullptr,WM_RBUTTONUP,0,point);sync();
        Require(!g_windowMgr.m_npcInputWnd && mode.m_dragType==dragBefore,"NPC right-click armed drag or opened a quantity prompt");
        if(g_windowMgr.m_itemInfoWnd) {
            UIItemInfoWnd::DisplayData data;Require(g_windowMgr.m_itemInfoWnd->GetDisplayDataForQt(&data),"NPC clicked detail data missing");
            Require(data.itemId==renderedId,"native detail differs from actual rendered NPC row");
        }
        (void)label;
    };
    const auto rowTarget=[&](const QString& name) {return Find(view.rootObject(),name);};
    const auto expectDetail=[&](u32 itemId,bool identified,const char* label) {
        UIItemInfoWnd* detail=g_windowMgr.m_itemInfoWnd;Require(detail&&detail->HasItem()&&uiState->itemInfoVisible(),label);
        UIItemInfoWnd::DisplayData data{};Require(detail->GetDisplayDataForQt(&data),"NPC item detail data unavailable");
        Require(data.itemId==itemId&&data.identified==identified,"NPC item detail ID/identification differs from source row");
        auto* text=Find(view.rootObject(),"itemInfoName");
        Require(text&&text->isVisible()&&text->property("text").toString()==QString::fromUtf8(data.name.c_str()),
            "NPC QML item detail label differs from native source snapshot");
        const QImage image=view.grabWindow().convertToFormat(QImage::Format_RGB32);
        const QRect ink=text->mapRectToScene(text->boundingRect()).toAlignedRect().intersected(image.rect());
        Require(!ink.isEmpty()&&PixelsNear(image,ink,identified?0xff25313cu:0xff808080u)>2,"NPC QML item detail label has no rendered ink");
        return data;
    };
    const auto capture=[&](const QString& stem,u32 expectedId,bool identified) {
        sync();expectDetail(expectedId,identified,"NPC detail not visible for capture");
        Require(view.grabWindow().save(output+"/"+stem+"-qt.png"),"save NPC detail Qt frame failed");
        UIItemInfoWnd* detail=g_windowMgr.m_itemInfoWnd;Require(detail,"NPC item detail disappeared before native capture");
        ArgbDibSurface surface;Require(surface.EnsureSize(Width,Height),"NPC detail native surface allocation failed");
        std::fill_n(surface.GetPixels(),Width*Height,0xff181b20u);detail->DrawNativeToHdc(surface.GetDC());
        const QImage native=QImage(reinterpret_cast<uchar*>(surface.GetPixels()),Width,Height,Width*4,QImage::Format_RGB32).copy();
        Require(native.save(output+"/"+stem+"-native.png"),"save NPC detail native frame failed");frameCount+=2;
    };
    const auto clearDetail=[&] {
        if(g_windowMgr.m_itemInfoWnd)g_windowMgr.DeleteWindow(g_windowMgr.m_itemInfoWnd);
        sync();Require(!uiState->itemInfoVisible(),"NPC detail remained visible after owned-window removal");
    };
    const auto clickSource=[&](int localIndex,u32 expectedId,bool identified=true) {
        const QString name=QString("npcShopSourceRow_%1").arg(localIndex);QQuickItem* target=rowTarget(name);
        sendRightClick(target,"source row right-click");if(g_windowMgr.m_itemInfoWnd)g_windowMgr.m_itemInfoWnd->Move(GetConfiguredUiScalePercent()>100?500:850,GetConfiguredUiScalePercent()>100?320:400);sync();expectDetail(expectedId,identified,"NPC source row did not open detail");
    };
    const auto clickBuyDeal=[&](int localIndex,u32 expectedId) {
        const QString name=QString("npcShopBuyDealRow_%1").arg(localIndex);QQuickItem* target=rowTarget(name);
        sendRightClick(target,"buy deal row right-click");if(g_windowMgr.m_itemInfoWnd)g_windowMgr.m_itemInfoWnd->Move(GetConfiguredUiScalePercent()>100?500:850,GetConfiguredUiScalePercent()>100?320:400);sync();expectDetail(expectedId,true,"NPC buy deal row did not open detail");
    };
    const auto clickSellDeal=[&](int localIndex,u32 expectedId,bool identified=true) {
        const QString name=QString("npcShopSellDealRow_%1").arg(localIndex);QQuickItem* target=rowTarget(name);
        sendRightClick(target,"sell deal row right-click");if(g_windowMgr.m_itemInfoWnd)g_windowMgr.m_itemInfoWnd->Move(GetConfiguredUiScalePercent()>100?500:850,GetConfiguredUiScalePercent()>100?320:400);sync();expectDetail(expectedId,identified,"NPC sell deal row did not open detail");
    };
    const auto createDeal=[](u16 itemId,u8 type,u16 sourceIndex,int qty,int unitPrice) {
        NPC_SHOP_DEAL_ROW row{};row.itemInfo.SetItemId(itemId);row.itemInfo.m_itemType=type;row.itemInfo.m_isIdentified=1;
        row.itemInfo.m_num=qty;row.sourceItemIndex=sourceIndex;row.quantity=qty;row.unitPrice=unitPrice;return row;
    };

    clearDetail();g_session.ClearNpcShopState();g_session.ClearInventoryItems();g_session.OpenStorage(0,600);
    ro::commerce::Reset();
    if(g_windowMgr.m_itemWnd)g_windowMgr.m_itemWnd->SetShow(0);
    if(g_windowMgr.m_storageWnd)g_windowMgr.m_storageWnd->SetShow(0);
    // Collision fixtures: NPC catalog/deal snapshots share an index with a
    // different local inventory/cart ID. This does not claim 00C6 carries an index.
    Dispatch(mode,Equipment(0xd0,true,0));
    ITEM_INFO bagItem=*g_session.GetInventoryItemByIndex(291);ITEM_INFO cartItem=bagItem;cartItem.SetItemId(1202);
    ro::commerce::state.cartItems[291]=cartItem;ro::commerce::state.hasCart=true;ro::commerce::state.cart.visible=false;
    openShop(0x12345678);Dispatch(mode,buyList({{0,501}}));
    Require(g_windowMgr.m_itemShopWnd&&g_windowMgr.m_itemPurchaseWnd,"production 00C6 did not open NPC buy shop windows");
    g_windowMgr.m_itemShopWnd->Move(20,100);g_windowMgr.m_itemPurchaseWnd->Move(450,100);
    // Synthetic source-index collision for the catalog row only; 00C6 wire rows
    // contain prices/type/item ID and no inventory index.
    g_session.m_shopRows[0].sourceItemIndex=291;sync();
    QQuickItem* catalog=rowTarget("npcShopSourceRow_0");Require(catalog&&catalog->isVisible(),"NPC catalog row missing from Qt projection");
    const int selectedSourceBefore=g_session.m_shopSelectedSourceRow;const size_t buyDealsBefore=g_session.m_shopDealRows.size();
    const int totalBefore=g_session.m_shopDealTotal;UIItemPurchaseWnd* purchaseBefore=g_windowMgr.m_itemPurchaseWnd;
    clickSource(0,501);capture("npc-buy-catalog-snapshot",501,true);
    Require(g_session.m_shopSelectedSourceRow==selectedSourceBefore&&g_session.m_shopDealRows.size()==buyDealsBefore
        &&g_session.m_shopDealTotal==totalBefore&&g_windowMgr.m_itemPurchaseWnd==purchaseBefore,
        "NPC catalog right-click mutated selection, deals, total, or prompt/window state");
    UIItemInfoWnd* catalogDetail=g_windowMgr.m_itemInfoWnd;
    sendRightClick(rowTarget("npcShopSourceRow_0"),"repeat catalog");
    Require(!g_windowMgr.m_itemInfoWnd&&!uiState->itemInfoVisible(),"repeated NPC catalog right-click did not toggle detail closed");
    cases.push_back(QJsonObject{{"case","buy catalog row right-click snapshot and repeat toggle"},{"item_id",501},{"outcome","PASS"}});

    clickSource(0,501);catalogDetail=g_windowMgr.m_itemInfoWnd;
    // Replacing the catalog closes source windows but must preserve the selected snapshot.
    openShop(0x12345679);Dispatch(mode,buyList({{5,1201}}));g_session.m_shopRows[0].sourceItemIndex=291;
    g_windowMgr.m_itemShopWnd->Move(20,100);g_windowMgr.m_itemPurchaseWnd->Move(450,100);sync();
    Require(g_windowMgr.m_itemInfoWnd==catalogDetail,"NPC buy catalog replacement deleted the item detail window");
    expectDetail(501,true,"buy catalog replacement changed retained snapshot");
    cases.push_back(QJsonObject{{"case","buy snapshot survives source catalog replacement"},{"outcome","PASS"}});
    clearDetail();

    // Exercise the production quantity/deal projection from the selected C6
    // source row. This is a local action after the literal packet, not a claim
    // that C6 itself carries a deal row.
    Require(g_session.AdjustNpcShopDealBySourceRow(0,1),"NPC buy source did not create a deal row");sync();
    Require(g_session.m_shopDealRows.size()==1&&g_session.m_shopDealRows[0].quantity==1
        &&g_session.m_shopDealRows[0].itemInfo.m_num==1,"NPC buy source deal quantity projection differs");
    clickBuyDeal(0,1201);capture("npc-buy-deal-from-source",1201,true);
    Require(g_session.m_shopDealRows[0].quantity==1&&g_session.m_shopDealTotal==g_session.m_shopDealRows[0].unitPrice,
        "NPC buy deal right-click changed production deal quantity or total");
    cases.push_back(QJsonObject{{"case","buy deal projected from C6 source row"},{"item_id",1201},{"outcome","PASS"}});clearDetail();

    // Named synthetic deal contexts check same-index/different-owner collisions;
    // they are not represented as 00C6/00C7 source rows.
    g_session.m_shopDealRows={createDeal(1202,5,291,2,75)};g_session.m_shopSelectedDealRow=0;g_session.m_shopDealTotal=150;
    sync();const int buySelectedBefore=g_session.m_shopSelectedDealRow;const int buyQuantityBefore=g_session.m_shopDealRows[0].quantity;
    clickBuyDeal(0,1202);capture("npc-buy-deal-snapshot",1202,true);
    Require(g_session.m_shopSelectedDealRow==buySelectedBefore&&g_session.m_shopDealRows[0].quantity==buyQuantityBefore
        &&g_session.m_shopDealTotal==150,"NPC buy deal right-click mutated selected deal/quantity/total");
    UIItemInfoWnd* buyDealDetail=g_windowMgr.m_itemInfoWnd;sendRightClick(rowTarget("npcShopBuyDealRow_0"),"repeat buy deal");
    Require(!g_windowMgr.m_itemInfoWnd&&!uiState->itemInfoVisible(),"repeated NPC buy deal right-click did not toggle detail closed");
    clickBuyDeal(0,1202);buyDealDetail=g_windowMgr.m_itemInfoWnd;
    g_windowMgr.CloseNpcShopWindows();g_session.ClearNpcShopState();sync();Require(g_windowMgr.m_itemInfoWnd==buyDealDetail,"closing buy deal source deleted its detail snapshot");
    expectDetail(1202,true,"buy deal snapshot changed after source close");
    cases.push_back(QJsonObject{{"case","buy deal snapshot, repeat toggle, source close"},{"outcome","PASS"}});clearDetail();

    // Sell catalog rows come from literal 00C7 and resolve the 00C7 index against
    // inventory once. Subsequent detail actions are detached snapshots.
    auto unidentified=Equipment(0xd0,false,0);unidentified[15]=7;write16(unidentified,16,4001);
    Dispatch(mode,unidentified);openShop(0x12345680);Dispatch(mode,sellList(291,44,55));
    Require(g_windowMgr.m_itemShopWnd&&g_windowMgr.m_itemSellWnd,"production 00C7 did not open NPC sell windows");
    g_windowMgr.m_itemShopWnd->Move(20,100);g_windowMgr.m_itemSellWnd->Move(450,100);sync();
    const int sellSourceBefore=g_session.m_shopSelectedSourceRow;const size_t sellDealsBefore=g_session.m_shopDealRows.size();
    clickSource(0,1201,false);capture("npc-sell-catalog-snapshot",1201,false);
    Require(g_session.m_shopSelectedSourceRow==sellSourceBefore&&g_session.m_shopDealRows.size()==sellDealsBefore,
        "NPC sell catalog right-click mutated selection or sell deals");
    g_windowMgr.ShowItemInfoWindow(g_session.m_shopRows[0].itemInfo,0,0,ItemInfoSource::Snapshot,ItemInfoAction::ForceUpdate);
    g_windowMgr.m_itemInfoWnd->Move(850,400);sync();
    sendRightClick(rowTarget("npcShopSourceRow_0"),"rich sell key");
    Require(!g_windowMgr.m_itemInfoWnd,"NPC sell snapshot lost raw refinement/card key fields");
    cases.push_back(QJsonObject{{"case","unidentified sell catalog preserves inventory refinement/card key"},{"outcome","PASS"}});clearDetail();

    Require(g_session.AdjustNpcShopDealBySourceRow(0,1),"NPC sell source did not create a deal row");sync();
    Require(g_session.m_shopDealRows.size()==1&&g_session.m_shopDealRows[0].quantity==1
        &&g_session.m_shopDealRows[0].itemInfo.m_num==1,"NPC sell source deal quantity projection differs");
    clickSellDeal(0,1201,false);capture("npc-sell-deal-from-source",1201,false);
    Require(g_session.m_shopDealRows[0].quantity==1,"NPC sell deal right-click changed production deal quantity");
    cases.push_back(QJsonObject{{"case","sell deal projected from C7 source row"},{"item_id",1201},{"outcome","PASS"}});clearDetail();

    g_session.m_shopDealRows={createDeal(501,0,291,3,12)};g_session.m_shopSelectedDealRow=0;g_session.m_shopDealTotal=36;
    sync();const int sellSelectedBefore=g_session.m_shopSelectedDealRow;const int sellQuantityBefore=g_session.m_shopDealRows[0].quantity;
    clickSellDeal(0,501);capture("npc-sell-deal-snapshot",501,true);
    Require(g_session.m_shopSelectedDealRow==sellSelectedBefore&&g_session.m_shopDealRows[0].quantity==sellQuantityBefore
        &&g_session.m_shopDealTotal==36,"NPC sell deal right-click mutated selection/quantity/total");
    UIItemInfoWnd* sellDealDetail=g_windowMgr.m_itemInfoWnd;g_session.ClearInventoryItems();sync();
    Require(g_windowMgr.m_itemInfoWnd==sellDealDetail,"removing sell source inventory deleted the detail snapshot");
    expectDetail(501,true,"sell snapshot changed after inventory source removal");
    cases.push_back(QJsonObject{{"case","sell deal snapshot isolated from removed inventory row"},{"outcome","PASS"}});clearDetail();

    // The first row QML target is the local visible row. Header and blank list
    // space must not be treated as an item, including after native scrolling.
    g_session.ClearInventoryItems();g_session.ClearNpcShopState();openShop(0x12345681);
    std::vector<std::pair<u8,u16>> longRows;for(int i=0;i<18;++i)longRows.push_back({0,u16(6000+i)});
    Dispatch(mode,buyList(longRows));Require(g_windowMgr.m_itemShopWnd,"scroll catalog window missing");
    auto* shopWnd=g_windowMgr.m_itemShopWnd;shopWnd->Move(20,100);shopWnd->Resize(shopWnd->m_w,360);sync();
    const int nativeVisible=shopWnd->GetVisibleRowCountForQt();Require(nativeVisible>0&&g_session.m_shopRows.size()>size_t(nativeVisible),"scroll catalog does not exceed visible rows");
    for(size_t i=0;i<g_session.m_shopRows.size();++i)shopWnd->OnWheel(-120);
    sync();
    const int localLast=nativeVisible-1;QQuickItem* last=rowTarget(QString("npcShopSourceRow_%1").arg(localLast));
    Require(last&&last->isVisible(),"scrolled final NPC source row is not rendered");
    const u32 finalId=g_session.m_shopRows.back().itemInfo.GetItemId();
    const QPointF lastPoint=last->mapToScene(QPointF(last->width()/2,last->height()/2));
    const int lastX=qRound(lastPoint.x()),lastY=qRound(lastPoint.y());const LPARAM lastLp=MAKELPARAM(WORD(lastX),WORD(lastY));
    WindowProc(nullptr,WM_RBUTTONDOWN,MK_RBUTTON,lastLp);WindowProc(nullptr,WM_RBUTTONUP,0,lastLp);sync();
    expectDetail(finalId,true,"scrolled final NPC source row did not open its item");
    cases.push_back(QJsonObject{{"case","scrolled last catalog row opens correct item"},{"item_id",int(finalId)},{"outcome","PASS"}});clearDetail();
    auto* firstRow=rowTarget("npcShopSourceRow_0");Require(firstRow,"scrolled first row missing");
    const QPointF firstPoint=firstRow->mapToScene(QPointF(firstRow->width()/2,firstRow->height()/2));
    const LPARAM headerLp=MAKELPARAM(WORD(qRound(firstPoint.x())),WORD(qRound(firstPoint.y()-18)));
    WindowProc(nullptr,WM_RBUTTONDOWN,MK_RBUTTON,headerLp);WindowProc(nullptr,WM_RBUTTONUP,0,headerLp);sync();
    Require(!g_windowMgr.m_itemInfoWnd,"scrolled NPC list header opened item detail");
    cases.push_back(QJsonObject{{"case","scrolled header is not an item row"},{"outcome","PASS"}});

    openShop(0x12345683);Dispatch(mode,buyList(longRows));
    shopWnd=g_windowMgr.m_itemShopWnd;shopWnd->Move(20,100);g_windowMgr.m_itemPurchaseWnd->Move(450,100);sync();
    Require(shopWnd->GetViewOffset()==0,"reopened NPC dialog retained another dialog's scroll offset");
    cases.push_back(QJsonObject{{"case","new NPC dialog resets retained pane viewport"},{"outcome","PASS"}});

    // Buy and sell deal panes have a header plus data rows. Their last row
    // must remain reachable; a scrolled header must not select the previous row.
    const auto checkDealScroll=[&](auto* pane,const QString& prefix,const char* kind) {
        g_session.m_shopDealRows.clear();g_session.m_shopDealTotal=0;
        for(int i=0;i<18;++i) {
            auto entry=createDeal(u16(5000+i),0,u16(100+i),i+1,i+10);
            g_session.m_shopDealTotal+=entry.quantity*entry.unitPrice;g_session.m_shopDealRows.push_back(entry);
        }
        g_session.m_shopSelectedDealRow=0;pane->Move(450,100);sync();
        for(int i=0;i<24;++i)pane->OnWheel(-120);
        sync();const int visible=pane->GetVisibleRowCountForQt();
        Require(pane->GetViewOffset()==int(g_session.m_shopDealRows.size())-visible,"final deal row is unreachable by scrolling");
        auto* lastDeal=rowTarget(prefix+QString::number(visible-1));Require(lastDeal,"last scrolled deal row missing");
        sendRightClick(lastDeal,kind);if(g_windowMgr.m_itemInfoWnd)g_windowMgr.m_itemInfoWnd->Move(850,400);sync();
        expectDetail(5017,true,"last deal row did not resolve its own record");clearDetail();
        auto* firstDeal=rowTarget(prefix+"0");Require(firstDeal,"first scrolled deal row missing");
        const QPointF pt=firstDeal->mapToScene(QPointF(firstDeal->width()/2,firstDeal->height()/2));
        const LPARAM hp=MAKELPARAM(WORD(qRound(pt.x())),WORD(qRound(pt.y()-18)));
        const int selected=g_session.m_shopSelectedDealRow,total=g_session.m_shopDealTotal;
        WindowProc(nullptr,WM_RBUTTONDOWN,MK_RBUTTON,hp);WindowProc(nullptr,WM_RBUTTONUP,0,hp);sync();
        Require(!g_windowMgr.m_itemInfoWnd,"scrolled deal header described an item");
        WindowProc(nullptr,WM_LBUTTONDOWN,MK_LBUTTON,hp);WindowProc(nullptr,WM_LBUTTONUP,0,hp);sync();
        Require(g_session.m_shopSelectedDealRow==selected&&g_session.m_shopDealTotal==total,"scrolled deal header selected a previous item");
        // Keyboard scrolling and shrinking models share the same effective offset.
        for(int i=0;i<18;++i)pane->HandleKeyDown(VK_DOWN);
        Require(g_session.m_shopSelectedDealRow==17&&pane->GetViewOffset()==18-visible,"keyboard selection is not visible at the list end");
        g_session.m_shopDealRows.resize(1);sync();
        Require(pane->GetViewOffset()==0&&rowTarget(prefix+"0"),"shrinking deal model left its only row off screen");
        sendRightClick(rowTarget(prefix+"0"),kind);if(g_windowMgr.m_itemInfoWnd)g_windowMgr.m_itemInfoWnd->Move(850,400);sync();
        expectDetail(5000,true,"shrunk deal model described a stale offset");clearDetail();
        cases.push_back(QJsonObject{{"case",QString::fromLatin1(kind)},{"outcome","PASS"}});
    };
    checkDealScroll(g_windowMgr.m_itemPurchaseWnd,"npcShopBuyDealRow_","buy deal scroll/header/shrink");
    // Open a genuine sell phase, then inject explicitly synthetic deal records.
    Dispatch(mode,Equipment(0xd0,true,0));openShop(0x12345690);Dispatch(mode,sellList(291,44,55));
    g_windowMgr.m_itemShopWnd->Move(20,100);g_windowMgr.m_itemSellWnd->Move(450,100);sync();
    checkDealScroll(g_windowMgr.m_itemSellWnd,"npcShopSellDealRow_","sell deal scroll/header/shrink");

    // Return to a short list in a tall window so there is clear blank space
    // below the one visible catalog row.
    openShop(0x12345682);Dispatch(mode,buyList({{0,501}}));shopWnd=g_windowMgr.m_itemShopWnd;
    shopWnd->Move(20,100);shopWnd->Resize(shopWnd->m_w,380);sync();
    QQuickItem* shortRow=rowTarget("npcShopSourceRow_0");Require(shortRow&&shortRow->isVisible(),"short NPC source row missing");
    const QPointF rowPoint=shortRow->mapToScene(QPointF(shortRow->width()/2,shortRow->height()/2));
    const int blankX=qRound(rowPoint.x()),blankY=qRound(rowPoint.y()+27);const LPARAM blankLp=MAKELPARAM(WORD(blankX),WORD(blankY));
    WindowProc(nullptr,WM_RBUTTONDOWN,MK_RBUTTON,blankLp);WindowProc(nullptr,WM_RBUTTONUP,0,blankLp);sync();
    Require(!g_windowMgr.m_itemInfoWnd,"blank NPC list space opened item detail");
    cases.push_back(QJsonObject{{"case","blank NPC list space is not an item row"},{"outcome","PASS"}});

    // An inventory-shaped ID/index collision must not affect the catalog snapshot.
    g_session.ClearInventoryItems();Dispatch(mode,Equipment(0xd0,true,0));
    g_session.m_shopRows[0].itemInfo.SetItemId(501);g_session.m_shopRows[0].sourceItemIndex=291;sync();
    QQuickItem* finalRow=rowTarget("npcShopSourceRow_0");sendRightClick(finalRow,"final source snapshot");expectDetail(501,true,"collision source did not open detail");
    UIItemInfoWnd* finalDetail=g_windowMgr.m_itemInfoWnd;g_session.ClearInventoryItems();
    ro::commerce::state.cartItems.erase(291);sync();Require(g_windowMgr.m_itemInfoWnd==finalDetail,"inventory/cart ID collision invalidated NPC snapshot");
    expectDetail(501,true,"NPC snapshot resolved through colliding inventory/cart owner");
    Require(view.grabWindow().save(output+"/npc-shop-retained-snapshot-qt.png"),"save NPC retained snapshot QML failed");
    UIItemInfoWnd::DisplayData finalData{};Require(finalDetail->GetDisplayDataForQt(&finalData),"NPC retained detail native data missing");
    ArgbDibSurface finalSurface;Require(finalSurface.EnsureSize(Width,Height),"NPC retained native surface failed");
    std::fill_n(finalSurface.GetPixels(),Width*Height,0xff181b20u);finalDetail->DrawNativeToHdc(finalSurface.GetDC());
    const QImage finalNative=QImage(reinterpret_cast<uchar*>(finalSurface.GetPixels()),Width,Height,Width*4,QImage::Format_RGB32).copy();
    Require(finalNative.save(output+"/npc-shop-retained-snapshot-native.png"),"save NPC retained snapshot native failed");frameCount+=2;
    cases.push_back(QJsonObject{{"case","NPC snapshot survives inventory/cart removal"},{"outcome","PASS"}});

    clearDetail();SetRuntimeUiScalePercent(125);sync();
    clickSource(0,501);
    Require(view.grabWindow().save(output+"/npc-shop-scaled-input-qt.png"),"save scaled NPC input frame failed");++frameCount;
    cases.push_back(QJsonObject{{"case","125 percent passive Qt row through WindowProc coordinate conversion"},{"outcome","PASS"}});
    QFile results(output+"/npc-shop-detail-results.json");Require(results.open(QIODevice::WriteOnly),"open NPC shop detail results failed");
    results.write(QJsonDocument(QJsonObject{{"cases",cases},{"saved_frames",frameCount},{"catalog_packet_families",QJsonArray{"00C6 buy","00C7 sell"}},
        {"synthetic_deal_contexts",true},{"outcome","PASS"}}).toJson(QJsonDocument::Indented));
}
