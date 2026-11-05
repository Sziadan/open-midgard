void CheckItemDetailToggleUi(QGuiApplication& app,QQuickView& view,QtUiState* uiState,CGameMode& mode,
    UIItemWnd& inventory,UIStorageWnd& storage,QtUiStateAdapter& adapter,const QString& output)
{
    Require(uiState,"item detail toggle state missing");
    struct ScaleRestore { int value=GetConfiguredUiScalePercent();~ScaleRestore(){SetRuntimeUiScalePercent(value);} } scaleRestore;
    SetRuntimeUiScalePercent(100);Require(QDir().mkpath(output),"item detail output directory failed");
    QJsonArray outcomes;int renderedFrames=0;
    const auto sync=[&] {
        Require(adapter.syncGameplay(mode,RenderBackendType::LegacyDirect3D7,RenderBackendType::LegacyDirect3D7,Width-1,Height-1),
            "item detail gameplay sync failed");Frames(app,view);
    };
    const auto hideDetail=[&] {
        if(g_windowMgr.m_itemInfoWnd)g_windowMgr.DeleteWindow(g_windowMgr.m_itemInfoWnd);
        sync();Require(!uiState->itemInfoVisible(),"deleted item detail remained visible in Qt state");
    };
    const auto inspect=[&](unsigned int id,bool identified,const QString& label={}) {
        UIItemInfoWnd* detail=g_windowMgr.m_itemInfoWnd;
        Require(detail&&detail->HasItem()&&uiState->itemInfoVisible(),"item detail is not owned and visible");
        UIItemInfoWnd::DisplayData data;Require(detail->GetDisplayDataForQt(&data),"item detail display data unavailable");
        Require(data.itemId==id&&data.identified==identified,"item detail ID/identified fields differ from selected item");
        auto* name=Find(view.rootObject(),"itemInfoName");
        Require(name&&name->isVisible()&&name->property("text").toString()==QString::fromUtf8(data.name.c_str()),
            "actual Qt item detail name differs from native display data");
        if(!label.isEmpty())Require(name->property("text").toString()==label,"actual Qt item detail label differs from expected name");
        return data;
    };
    const auto capture=[&](const QString& stem,bool identified=true) {
        inventory.OnMouseMove(Width-1,Height-1);storage.OnMouseMove(Width-1,Height-1);
        Mouse(app,view,QPointF(Width-1,Height-1));
        sync();inspect(1201,identified);
        const auto qt=view.grabWindow().convertToFormat(QImage::Format_RGB32);
        auto* name=Find(view.rootObject(),"itemInfoName");
        const auto rect=name->mapRectToScene(name->boundingRect()).toAlignedRect().intersected(qt.rect());
        Require(Pixels(qt,rect,0xff25313c)>5,"actual Qt detail name paint is obscured");
        Require(qt.save(output+"/"+stem+"-qt.png"),"save Qt item detail frame failed");
        UIItemInfoWnd* detail=g_windowMgr.m_itemInfoWnd;Require(detail,"item detail disappeared before native capture");
        ArgbDibSurface surface;Require(surface.EnsureSize(Width,Height),"item detail native surface failed");
        std::fill_n(surface.GetPixels(),Width*Height,0xff181b20u);detail->DrawNativeToHdc(surface.GetDC());
        const QImage image=QImage(reinterpret_cast<uchar*>(surface.GetPixels()),Width,Height,Width*4,QImage::Format_RGB32).copy();
        Require(image.save(output+"/"+stem+"-native.png"),"save native item detail frame failed");renderedFrames+=2;
    };
    const auto rightClick=[&](const QString& objectName,unsigned int expectedId,const char* label) {
        QQuickItem* target=Find(view.rootObject(),objectName);Require(target&&target->isVisible(),"item detail QML target missing or hidden");
        const QVariantMap row=target->property("item").toMap();Require(row.value("id").toUInt()==expectedId,"item detail QML target has unexpected item ID");
        const QPointF point=target->mapToScene(QPointF(target->width()/2,target->height()/2));Mouse(app,view,point);
        QMouseEvent press(QEvent::MouseButtonPress,point,view.mapToGlobal(point.toPoint()),Qt::RightButton,Qt::RightButton,Qt::NoModifier);
        QMouseEvent release(QEvent::MouseButtonRelease,point,view.mapToGlobal(point.toPoint()),Qt::RightButton,Qt::NoButton,Qt::NoModifier);
        QCoreApplication::sendEvent(&view,&press);QCoreApplication::sendEvent(&view,&release);sync();
        (void)label;
    };
    const auto assertClosed=[&](const char* label) {
        Require(g_windowMgr.m_itemInfoWnd==nullptr&&!uiState->itemInfoVisible(),label);
        Require(!Find(view.rootObject(),"itemInfoName")->isVisible(),"closed detail label remained visible in actual QML");
        outcomes.push_back(QJsonObject{{"case",label},{"outcome","PASS"}});
    };
    const auto forceUpdate=[&](const ITEM_INFO& item,ItemInfoSource source) {
        g_windowMgr.ShowItemInfoWindow(item,320,90,source,ItemInfoAction::ForceUpdate);sync();
    };
    const auto nativeRightClick=[&](auto& wnd,const char* label) {
        using Window=std::decay_t<decltype(wnd)>;typename Window::DisplayData data{};
        Require(wnd.GetDisplayDataForQt(&data),"native item window display unavailable");
        const auto slot=std::find_if(data.displaySlots.begin(),data.displaySlots.end(),[](const auto& s){return s.occupied&&s.itemId==1201;});
        Require(slot!=data.displaySlots.end(),"native item window does not contain the detail fixture");
        wnd.OnRBtnDown(slot->x+slot->width/2,slot->y+slot->height/2);sync();
        (void)label;
    };
    const auto chooseNativeTab=[](auto& wnd,int tab) {
        using Window=std::decay_t<decltype(wnd)>;typename Window::QtButtonDisplay data{};
        Require(wnd.GetQtTabDisplayForQt(tab,&data)&&data.visible,"native detail fixture tab is unavailable");
        const int x=data.x+data.width/2,y=data.y+data.height/2;wnd.OnLBtnDown(x,y);wnd.OnLBtnUp(x,y);
    };

    if(g_windowMgr.m_itemInfoWnd)g_windowMgr.DeleteWindow(g_windowMgr.m_itemInfoWnd);
    ro::commerce::Reset();SelectInventoryFixtureTab(1);
    g_session.ClearInventoryItems();g_session.ClearStorageItems();g_session.OpenStorage(1,600);
    if(g_windowMgr.m_itemWnd)g_windowMgr.m_itemWnd->SetShow(1);inventory.SetShow(1);storage.SetShow(1);
    Dispatch(mode,Equipment(0xd0,true,0));
    const ITEM_INFO* inventoryItem=g_session.GetInventoryItemByIndex(291);Require(inventoryItem&&inventoryItem->GetItemId()==1201,"literal inventory context row missing");
    ITEM_INFO storageItem=*inventoryItem;storageItem.m_itemIndex=777;g_session.SetStorageItem(storageItem);
    chooseNativeTab(inventory,1);chooseNativeTab(storage,1);
    ITEM_INFO cartItem=*inventoryItem;cartItem.m_itemIndex=292;cartItem.m_itemType=9;cartItem.m_isIdentified=0;
    cartItem.m_isDamaged=1;cartItem.m_isYours=2;cartItem.m_location=0x8000;cartItem.m_wearLocation=2;
    cartItem.m_price=99;cartItem.m_realPrice=77;
    cartItem.m_num=7;cartItem.m_deleteTime=123456;ro::commerce::state.cartItems[292]=cartItem;
    ro::commerce::state.hasCart=true;ro::commerce::state.cart.visible=true;
    auto& state=ro::commerce::state;
    PopulateCommerceUi(uiState,mode,false);sync();

    // Actual QML inventory right-click opens, and repeating the same source key
    // closes and deletes the owned window.
    rightClick("commerceTile_inventory_291",1201,"inventory-open");
    inspect(1201,true);capture("item-detail-inventory-selected");
    g_windowMgr.m_itemInfoWnd->Move(100,70);sync();
    rightClick("commerceTile_inventory_291",1201,"inventory-repeat");assertClosed("same QML inventory key toggles closed");
    Require(view.grabWindow().save(output+"/item-detail-toggle-closed-qt.png"),"save closed detail frame failed");++renderedFrames;

    // A retained hidden window is reopened by Select instead of being toggled.
    rightClick("commerceTile_inventory_291",1201,"inventory-hidden-reopen");
    UIItemInfoWnd* retained=g_windowMgr.m_itemInfoWnd;Require(retained,"hidden reopen setup failed");
    retained->SetShow(0);sync();Require(!uiState->itemInfoVisible(),"hidden retained item detail remained visible");
    rightClick("commerceTile_inventory_291",1201,"inventory-hidden-reopen-again");
    Require(g_windowMgr.m_itemInfoWnd==retained&&uiState->itemInfoVisible(),"same-key selection failed to reopen hidden retained detail");
    inspect(1201,true);outcomes.push_back(QJsonObject{{"case","hidden retained same-key selection reopens"},{"outcome","PASS"}});

    // Owner, index, type, amount, expiry and identification are not the source
    // key. A cross-owner same-key QML selection therefore toggles the window.
    rightClick("commerceTile_cart_292",1201,"cart-owner-index-identification");assertClosed("cross-owner/index/type/identified same key toggles closed");
    rightClick("commerceTile_cart_292",1201,"cart-reopen");inspect(1201,false);
    outcomes.push_back(QJsonObject{{"case","cart context opens after same-key toggle"},{"outcome","PASS"}});

    // Textual item-name matching uses the complete raw name; its decimal parse
    // may still resolve to the same numeric item ID.
    forceUpdate(*inventoryItem,ItemInfoSource::Snapshot);
    UIItemInfoWnd* detail=g_windowMgr.m_itemInfoWnd;cartItem.m_itemName="01201";state.cartItems[292]=cartItem;PopulateCommerceUi(uiState,mode,false);
    rightClick("commerceTile_cart_292",1201,"raw-name-leading-zero");
    Require(g_windowMgr.m_itemInfoWnd==detail&&uiState->itemInfoVisible(),"raw item-name difference incorrectly toggled detail closed");
    inspect(1201,false);outcomes.push_back(QJsonObject{{"case","raw item name participates in source key"},{"outcome","PASS"}});

    // Refine and every card word independently change the selection key.
    ITEM_INFO keyBaseline=*inventoryItem;forceUpdate(keyBaseline,ItemInfoSource::Snapshot);
    cartItem=keyBaseline;cartItem.m_itemIndex=292;cartItem.m_itemType=9;cartItem.m_isIdentified=0;cartItem.m_num=7;cartItem.m_deleteTime=123456;
    cartItem.m_refiningLevel=3;state.cartItems[292]=cartItem;PopulateCommerceUi(uiState,mode,false);
    rightClick("commerceTile_cart_292",1201,"refine-key");
    Require(g_windowMgr.m_itemInfoWnd==detail&&uiState->itemInfoVisible(),"refine change incorrectly toggled detail closed");
    inspect(1201,false);outcomes.push_back(QJsonObject{{"case","refine participates in source key"},{"outcome","PASS"}});
    for(int word=0;word<4;++word) {
        forceUpdate(keyBaseline,ItemInfoSource::Snapshot);
        ITEM_INFO changed=keyBaseline;changed.m_itemIndex=292;changed.m_itemType=9;changed.m_isIdentified=0;
        changed.m_num=7;changed.m_deleteTime=123456;changed.m_slot[word]=4001+word;
        state.cartItems[292]=changed;PopulateCommerceUi(uiState,mode,false);
        rightClick("commerceTile_cart_292",1201,"card-key");
        Require(g_windowMgr.m_itemInfoWnd==detail&&uiState->itemInfoVisible(),"card word change incorrectly toggled detail closed");
        inspect(1201,false);
        outcomes.push_back(QJsonObject{{"case",QString("card word %1 participates in source key").arg(word)},{"outcome","PASS"}});
    }

    // ForceUpdate with the same key changes the visible item without toggling.
    ITEM_INFO force=cartItem;force.m_refiningLevel=0;force.m_slot[0]=force.m_slot[1]=force.m_slot[2]=force.m_slot[3]=0;
    force.m_isIdentified=1;force.m_itemType=2;force.m_num=99;force.m_deleteTime=8888;
    forceUpdate(keyBaseline,ItemInfoSource::Snapshot);
    forceUpdate(force,ItemInfoSource::Snapshot);Require(g_windowMgr.m_itemInfoWnd==detail&&uiState->itemInfoVisible(),"ForceUpdate toggled a same-key detail closed");
    auto forcedData=inspect(1201,true);outcomes.push_back(QJsonObject{{"case","ForceUpdate refreshes and remains visible"},{"display_name",QString::fromUtf8(forcedData.name.c_str())},{"outcome","PASS"}});

    capture("item-detail-forced-update");

    // A snapshot is isolated from same-index inventory/cart mutations.
    const QString snapshotName=QString::fromUtf8(forcedData.name.c_str());
    ITEM_INFO changedInventory=*g_session.GetInventoryItemByIndex(291);changedInventory.m_isIdentified=0;changedInventory.m_refiningLevel=7;
    g_session.SetInventoryItem(changedInventory);ITEM_INFO changedCart=state.cartItems.at(292);changedCart.m_isIdentified=0;changedCart.m_refiningLevel=6;state.cartItems[292]=changedCart;
    sync();auto isolated=inspect(1201,true,snapshotName);
    Require(QString::fromUtf8(isolated.name.c_str())==snapshotName,"snapshot item detail bound to a same-index owner row");
    outcomes.push_back(QJsonObject{{"case","snapshot binding isolation"},{"outcome","PASS"}});

    // Bound owner sources refresh from their current rows instead of a colliding
    // row belonging to another source.
    ITEM_INFO cartBound=state.cartItems.at(292);forceUpdate(cartBound,ItemInfoSource::Cart);
    Require(g_windowMgr.m_itemInfoWnd==detail&&inspect(1201,false).identified==false,"cart ForceUpdate failed to rebind source");
    cartBound.m_isIdentified=1;cartBound.m_refiningLevel=4;state.cartItems[292]=cartBound;sync();
    auto cartRefreshed=inspect(1201,true);Require(QString::fromUtf8(cartRefreshed.name.c_str()).contains("+4"),"cart-bound detail did not refresh source refinement");
    outcomes.push_back(QJsonObject{{"case","cart source binding refresh"},{"outcome","PASS"}});
    ITEM_INFO inventoryBound=*g_session.GetInventoryItemByIndex(291);forceUpdate(inventoryBound,ItemInfoSource::Inventory);
    inventoryBound.m_isIdentified=0;inventoryBound.m_refiningLevel=5;g_session.SetInventoryItem(inventoryBound);sync();
    auto inventoryRefreshed=inspect(1201,false);Require(!QString::fromUtf8(inventoryRefreshed.name.c_str()).contains("+5"),
        "unidentified inventory detail leaked refinement after owner refresh");
    outcomes.push_back(QJsonObject{{"case","inventory source binding refresh"},{"outcome","PASS"}});

    // Native inventory and storage right-clicks use the same Select toggle.
    forceUpdate(*g_session.GetInventoryItemByIndex(291),ItemInfoSource::Inventory);
    nativeRightClick(inventory,"native-inventory-toggle");assertClosed("native inventory repeat selection toggles closed");
    nativeRightClick(storage,"native-storage-open");inspect(1201,true);capture("item-detail-native-storage");
    nativeRightClick(storage,"native-storage-repeat");
    assertClosed("native storage repeat selection toggles closed");
    outcomes.push_back(QJsonObject{{"case","native inventory and storage selection"},{"outcome","PASS"}});

    // A real shop detail icon uses its owner lookup and toggles a matching key.
    state.shopItems.clear();ro::commerce::Offer offer;offer.item=storageItem;
    offer.item.m_itemIndex=333;offer.item.m_itemType=6;offer.item.m_isIdentified=0;offer.amount=1;offer.price=100;state.shopItems.push_back(offer);state.shop.visible=true;
    forceUpdate(storageItem,ItemInfoSource::Snapshot);g_windowMgr.m_itemInfoWnd->Move(100,300);
    PopulateCommerceUi(uiState,mode,false);sync();rightClick("commerceDetail_shop_333",1201,"shop-source");assertClosed("cross-owner shop same key toggles closed");
    rightClick("commerceDetail_shop_333",1201,"shop-reopen");inspect(1201,false);
    outcomes.push_back(QJsonObject{{"case","actual QML shop detail source"},{"outcome","PASS"}});

    // ForceUpdate can set arbitrary current context without the Select toggle.
    ITEM_INFO forceShop=offer.item;forceShop.m_isIdentified=1;forceShop.m_refiningLevel=2;
    forceUpdate(forceShop,ItemInfoSource::Snapshot);Require(g_windowMgr.m_itemInfoWnd&&uiState->itemInfoVisible(),"ForceUpdate closed the shop context");
    inspect(1201,true);
    outcomes.push_back(QJsonObject{{"case","ForceUpdate bypasses toggle"},{"outcome","PASS"}});

    // Closing through the actual native close button deletes the owned window.
    g_windowMgr.m_itemInfoWnd->Move(100,70);sync();
    UIItemInfoWnd::DisplayData closeData;Require(g_windowMgr.m_itemInfoWnd->GetDisplayDataForQt(&closeData),"close button display data missing");
    const int closeX=closeData.closeButton.x+closeData.closeButton.width/2,closeY=closeData.closeButton.y+closeData.closeButton.height/2;
    g_windowMgr.OnLBtnDown(closeX,closeY);g_windowMgr.OnLBtnUp(closeX,closeY);sync();
    assertClosed("native close button deletes owned item detail");
    rightClick("commerceDetail_shop_333",1201,"shop-reopen-saved-placement");
    UIItemInfoWnd* reopened=g_windowMgr.m_itemInfoWnd;Require(reopened,"deleted item detail did not reopen");
    UIItemInfoWnd::DisplayData reopenedData;Require(reopened->GetDisplayDataForQt(&reopenedData),"reopened item detail data missing");
    Require(reopenedData.itemId==1201&&reopened->m_x==100&&reopened->m_y==70,"reopened selection did not restore saved placement and identity");
    outcomes.push_back(QJsonObject{{"case","deleted item detail can be recreated"},{"outcome","PASS"}});

    inventory.OnMouseMove(Width-1,Height-1);storage.OnMouseMove(Width-1,Height-1);
    Mouse(app,view,QPointF(Width-1,Height-1));
    SetRuntimeUiScalePercent(125);forceUpdate(forceShop,ItemInfoSource::Snapshot);sync();
    inspect(1201,true);Require(view.grabWindow().save(output+"/item-detail-scaled-retained-qt.png"),"save scaled retained detail failed");
    outcomes.push_back(QJsonObject{{"case","ForceUpdate at 125 percent scale"},{"outcome","PASS"}});++renderedFrames;
    QFile results(output+"/item-detail-toggle-results.json");Require(results.open(QIODevice::WriteOnly),"open item detail toggle results failed");
    results.write(QJsonDocument(QJsonObject{{"cases",outcomes},{"rendered_frames",renderedFrames},{"outcome","PASS"}}).toJson(QJsonDocument::Indented));
}
