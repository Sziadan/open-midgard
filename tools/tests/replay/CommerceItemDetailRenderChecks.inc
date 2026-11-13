void CheckCommerceItemDetails(QGuiApplication& app,QQuickView& view,QtUiState* uiState,CGameMode& mode,
    QtUiStateAdapter& adapter,const QString& output)
{
    using namespace ro::commerce;
    struct ScaleRestore { int value=GetConfiguredUiScalePercent();~ScaleRestore(){SetRuntimeUiScalePercent(value);} } scaleRestore;
    SetRuntimeUiScalePercent(100);
    Require(QDir().mkpath(output),"create commerce detail output directory failed");
    if(g_windowMgr.m_itemWnd)g_windowMgr.m_itemWnd->SetShow(1);
    QJsonArray cases;
    int frames=0;
    const auto sync=[&] {
        Require(adapter.syncGameplay(mode,RenderBackendType::LegacyDirect3D7,
            RenderBackendType::LegacyDirect3D7,Width-1,Height-1),"commerce detail gameplay sync failed");
        Frames(app,view);
    };
    const auto hideDetail=[&] {
        if(g_windowMgr.m_itemInfoWnd)g_windowMgr.m_itemInfoWnd->SetShow(0);
        sync();
    };
    const auto write16=[](std::vector<u8>& p,int at,u32 v){p[at]=u8(v);p[at+1]=u8(v>>8);};
    const auto write32=[](std::vector<u8>& p,int at,u32 v){for(int k=0;k<4;++k)p[at+k]=u8(v>>(8*k));};
    const auto offerPacket=[&](u16 opcode,u16 id,u16 index,u32 amount,bool identified) {
        std::vector<u8> p(30);write16(p,0,opcode);write16(p,2,u32(p.size()));write32(p,4,0x12345678);
        write32(p,8,900); // unit price
        if(opcode==0x136){write16(p,12,index);write16(p,14,amount);}
        else {write16(p,12,amount);write16(p,14,index);}
        p[16]=5;write16(p,17,id);p[19]=identified?1:0;p[20]=0;p[21]=3;
        return p;
    };
    const auto rightClick=[&](const QString& objectName,u32 expectedId,bool identified,const char* caseName) {
        hideDetail();sync();
        QQuickItem* target=Find(view.rootObject(),objectName);
        if(!target || !target->isVisible())std::cerr<<"commerce detail target "<<objectName.toStdString()<<'\n';
        Require(target && target->isVisible(),"commerce detail QML target missing or hidden");
        const QVariantMap rendered=target->property("item").toMap();
        Require(rendered.value("id").toUInt()==expectedId,"commerce detail row has unexpected source ID");
        const QPointF p=target->mapToScene(QPointF(target->width()/2,target->height()/2));
        Mouse(app,view,p);
        QMouseEvent press(QEvent::MouseButtonPress,p,view.mapToGlobal(p.toPoint()),Qt::RightButton,Qt::RightButton,Qt::NoModifier);
        QMouseEvent release(QEvent::MouseButtonRelease,p,view.mapToGlobal(p.toPoint()),Qt::RightButton,Qt::NoButton,Qt::NoModifier);
        QCoreApplication::sendEvent(&view,&press);QCoreApplication::sendEvent(&view,&release);Frames(app,view);sync();
        auto* detail=g_windowMgr.m_itemInfoWnd;
        Require(detail && detail->HasItem() && uiState->itemInfoVisible(),"actual QML right-click did not open item detail");
        UIItemInfoWnd::DisplayData data;
        Require(detail->GetDisplayDataForQt(&data),"commerce item detail data missing");
        Require(data.itemId==expectedId && data.identified==identified,"commerce detail ID/identified state differs from clicked row");
        const QString expectedName=rendered.value("name").toString();
        Require(!expectedName.isEmpty() && QString::fromUtf8(data.name.c_str())==expectedName,
            "commerce detail name differs from clicked row");
        detail->Move(450,70);sync();
        auto* label=Find(view.rootObject(),"itemInfoName");
        Require(label && label->isVisible() && label->property("text").toString()==expectedName,
            "actual Qt detail label differs from clicked row");
        Require(qAbs(label->mapToScene(QPointF()).x()-(450+90)*GetConfiguredUiScaleFactor())<2,
            "commerce snapshot popup lost native coordinate scaling");
        const QString suffix=QString::fromLatin1(caseName);
        const auto qt=view.grabWindow().convertToFormat(QImage::Format_RGB32);
        const auto rect=label->mapRectToScene(label->boundingRect()).toAlignedRect().intersected(qt.rect());
        Require(PixelsNear(qt,rect,identified?0xff25313cu:0xff808080u)>5,"actual Qt detail name paint was obscured");
        Require(qt.save(output+"/commerce-detail-"+suffix+"-qt.png"),"save commerce detail Qt frame failed");
        ArgbDibSurface surface;Require(surface.EnsureSize(Width,Height),"commerce detail native surface failed");
        std::fill_n(surface.GetPixels(),Width*Height,0xff181b20u);detail->DrawNativeToHdc(surface.GetDC());
        const QImage native=QImage(reinterpret_cast<uchar*>(surface.GetPixels()),Width,Height,Width*4,QImage::Format_RGB32).copy();
        Require(native.save(output+"/commerce-detail-"+suffix+"-native.png"),"save commerce detail native frame failed");
        frames+=2;cases.push_back(QJsonObject{{"case",caseName},{"id",int(expectedId)},
            {"identified",identified},{"name",expectedName},{"outcome","PASS"}});
    };
    const auto noOpen=[&](const QVariantMap& args,const char* label) {
        hideDetail();
        uiState->commerceAction(QStringLiteral("describe"),args);sync();
        Require(!uiState->itemInfoVisible() ,label);
        cases.push_back(QJsonObject{{"case",label},{"outcome","PASS"}});
    };
    const auto hidePanels=[&] {
        state.cart.visible=state.vending.visible=state.trade.visible=state.shop.visible=state.mail.visible=state.equipment.visible=false;
    };

    Reset();SelectInventoryFixtureTab(1);g_session.ClearInventoryItems();state.cartItems.clear();g_session.OpenStorage(1,600);
    hidePanels();
    // Deliberate same-index collision: inventory is Knife; cart is Dagger.
    Dispatch(mode,Equipment(0xd0,true,0));
    ITEM_INFO cartItem;cartItem.m_itemIndex=3;cartItem.SetItemId(1202);cartItem.m_itemType=5;cartItem.m_isIdentified=0;cartItem.m_num=1;
    state.cartItems[3]=cartItem;state.hasCart=true;state.cart.visible=true;
    sync();
    rightClick("commerceTile_inventory_3",1201,true,"inventory");
    rightClick("commerceTile_cart_3",1202,false,"cart-collision");

    // Literal server snapshots exercise both offer layouts and their itemIndex identity.
    hidePanels();Dispatch(mode,offerPacket(0x136,1202,91,2,true));
    state.vending.visible=true;state.cart.visible=false;sync();
    rightClick("commerceTile_vending_91",1202,true,"vending");
    hidePanels();state.requestedSellerId=0x12345678;Dispatch(mode,offerPacket(0x133,1201,91,4,false));sync();
    rightClick("commerceDetail_shop_91",1201,false,"shop");
    SetRuntimeUiScalePercent(125);rightClick("commerceDetail_shop_91",1201,false,"scaled-shop");
    SetRuntimeUiScalePercent(100);

    // 00E7 opens the real trade panel; 00E9 rows have no item index, so row ordinals select distinct snapshots.
    hidePanels();Dispatch(mode,{0xe7,0,3});Dispatch(mode,{0xe9,0,2,0,0,0,0xb2,4,1,0,2,0,0,0,0,0,0,0,0});
    Dispatch(mode,{0xe9,0,1,0,0,0,0xb1,4,0,0,1,0,0,0,0,0,0,0,0});
    Offer local;local.item.m_itemIndex=291;local.item.SetItemId(1201);local.item.m_itemType=5;local.item.m_isIdentified=1;local.amount=1;
    state.localOffer.push_back(local);state.trade.visible=true;state.tradeActive=true;sync();
    rightClick("commerceDetail_trade-local_291",1201,true,"trade-local");
    rightClick("commerceDetail_trade-remote_0",1202,true,"trade-remote-row0");
    rightClick("commerceDetail_trade-remote_1",1201,false,"trade-remote-row1");

    // Literal remote equipment snapshot; an unsupported route must fail this check.
    hidePanels();
    std::vector<u8> equipment(69);write16(equipment,0,0x2d7);write16(equipment,2,u32(equipment.size()));
    std::memcpy(equipment.data()+4,"DetailPeer",10);write16(equipment,43,291);write16(equipment,45,1202);
    equipment[47]=5;equipment[48]=1;equipment[49]=2;equipment[50]=0;equipment[53]=0;
    Dispatch(mode,equipment);
    state.equipment.visible=true;sync();rightClick("commerceDetail_equipment_291",1202,true,"viewed-equipment");

    // Snapshot details remain the selected item after their source panel/list changes.
    hidePanels();state.requestedSellerId=0x12345678;Dispatch(mode,offerPacket(0x133,1202,91,1,true));sync();
    rightClick("commerceDetail_shop_91",1202,true,"shop-snapshot");
    state.shopItems.clear();state.shop.visible=false;state.cartItems.erase(3);sync();
    Dispatch(mode,{0xaf,0,3,0,1,0});sync();
    UIItemInfoWnd::DisplayData retained;Require(g_windowMgr.m_itemInfoWnd->GetDisplayDataForQt(&retained)
        &&retained.itemId==1202&&uiState->itemInfoVisible(),"closed/removed commerce source invalidated snapshot detail");
    cases.push_back(QJsonObject{{"case","snapshot-after-source-close"},{"outcome","PASS"}});

    // Invalid callbacks are rejected even when a colliding cart row exists.
    state.cartItems[3]=cartItem;state.cart.visible=true;
    state.draft={{cartItem,1,1}};state.vending.visible=true;
    noOpen(QVariantMap{{"from","vending"},{"index",3},{"id",1201}},"stale-expected-id");
    noOpen(QVariantMap{{"from","vending"},{"index",9999},{"id",1202}},"unknown-row");
    noOpen(QVariantMap{{"from","unknown"},{"index",291},{"id",1202}},"unknown-source");
    noOpen(QVariantMap{{"from","cart"},{"index",3}},"missing-expected-id");
    state.draft={{cartItem,0,1}};state.vending.visible=true;
    noOpen(QVariantMap{{"from","vending"},{"index",3},{"id",1202}},"zero-amount");
    state.draft.clear();state.vending.visible=true;
    noOpen(QVariantMap{{"from","vending"},{"index",291},{"id",1202}},"missing-vending-row-with-cart-collision");
    state.shopItems={{cartItem,1,1}};state.shop.visible=false;
    noOpen(QVariantMap{{"from","shop"},{"index",91},{"id",1202}},"hidden-source-panel");
    state.trade.visible=true;state.tradeActive=true;
    noOpen(QVariantMap{{"from","trade-remote"},{"index",0},{"row",8},{"id",1202}},"missing-remote-row");
    noOpen(QVariantMap{{"from","trade-remote"},{"index",0},{"row",-1},{"id",1202}},"negative-remote-row");
    noOpen(QVariantMap{{"from","trade-remote"},{"index",0},{"id",1202}},"omitted-remote-row");

    hidePanels();
    Dispatch(mode,{0xf5,1,3,77,0,0,0,10,0});
    state.partner = "DetailPeer";
    const std::vector<u8> repeatOffer{0xe9,0,2,0,0,0,0xb2,4,1,0,2,0,0,0,0,0,0,0,0};
    for(int i=0;i<11;++i)Dispatch(mode,repeatOffer);
    Dispatch(mode,{0xe9,0,42,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0});
    Dispatch(mode,{0xec,0,1});
    sync();
    const auto item = [&](const QString& name) {
        auto* target = Find(view.rootObject(),name);
        Require(target && target->isVisible(), "expected a visible trade item");
        return target;
    };
    auto* firstOffer = item("commerceTradeItem_remote_0");
    Require(firstOffer->property("text").toString().startsWith(QStringLiteral("2 × "))
        && item("commerceTradeItem_remote_1")->property("text") == firstOffer->property("text")
        && Find(view.rootObject(),"commerceTradeItem_remote_9")
        && !Find(view.rootObject(),"commerceTradeItem_remote_10"), "Qt lost separate remote offer rows");
    Require(item("commerceTradeZeny_remote")->property("text").toString()=="42 Zeny"
        && item("commerceTradeHeading_remote")->property("text").toString().startsWith("DetailPeer")
        && item("commerceTradeHeading_remote")->property("text").toString().endsWith("Locked")
        && !item("commerceTradeHeading_local")->property("text").toString().endsWith("Locked")
        && !item("commerceCommitTrade")->isEnabled(), "remote lock did not update the correct trade controls");
    Dispatch(mode,{0xec,0,0});
    Dispatch(mode,{0xf0,0,0});
    sync();
    Require(item("commerceCommitTrade")->isEnabled(), "unowned result hid a ready trade");
    const auto captureTrade = [&](const QString& suffix) {
        const auto image = view.grabWindow().convertToFormat(QImage::Format_RGB32);
        Require(!image.isNull() && image.save(output+"/trade-"+suffix+".png"), "save trade frame failed");
        ++frames;
        return image;
    };
    const auto lockedFrame = captureTrade("offers-locked");
    for(auto* label : {item("commerceTradeItem_remote_0"), item("commerceTradeZeny_remote"), item("commerceTradeHeading_remote")}) {
        const auto rect = label->mapRectToScene(label->boundingRect()).toAlignedRect().intersected(lockedFrame.rect());
        Require(PixelsNear(lockedFrame,rect,0xff25313cu)>5, "remote trade label did not paint");
    }
    // The production send path is covered by the packet replay. Seed only its pending flag here.
    state.commitPending = true;
    sync();
    Require(!item("commerceCommitTrade")->isEnabled(), "pending trade confirmation remained enabled");
    Dispatch(mode,{0xf0,0,0});
    sync();
    auto* tradePanel = Find(view.rootObject(),"commerce_trade");
    Require(tradePanel && !tradePanel->isVisible() && state.remoteOffer.empty(), "owned result left the trade window visible");
    captureTrade("completed");
    cases.push_back(QJsonObject{{"case","trade-offer-lock-commit"},{"outcome","PASS"}});

    Reset();
    hideDetail();
    constexpr u32 boardSeller = 0x12345678;
    CGameActor vendor;
    vendor.m_gid = boardSeller;
    vendor.m_isVisible = 1;
    vendor.m_pos = {0.0f,0.0f,0.0f};
    mode.m_world->RegisterActor(&vendor);
    mode.m_view->SetInitialCamera(0,-45,500);
    mode.m_view->OnCalcViewInfo();
    g_renderer.m_xoffset=1100;
    g_renderer.m_yoffset=370;
    g_renderer.m_hpc=300.0f;
    g_renderer.m_vpc=300.0f;

    Require(CConnection::Startup(), "vending render socket startup failed");
    struct SocketCleanup {
        SOCKET listener=INVALID_SOCKET;
        SOCKET peer=INVALID_SOCKET;
        ~SocketCleanup(){
            CRagConnection::instance()->Disconnect();
            if(peer!=INVALID_SOCKET)closesocket(peer);
            if(listener!=INVALID_SOCKET)closesocket(listener);
            CConnection::Cleanup();
        }
    } sockets;
    sockets.listener=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    sockaddr_in address{};
    address.sin_family=AF_INET;
    address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    int addressLength=sizeof(address);
    Require(sockets.listener!=INVALID_SOCKET && bind(sockets.listener,reinterpret_cast<sockaddr*>(&address),sizeof(address))==0
        && listen(sockets.listener,1)==0 && getsockname(sockets.listener,reinterpret_cast<sockaddr*>(&address),&addressLength)==0,
        "vending render listener failed");
    auto* connection=CRagConnection::instance();
    Require(connection->Connect("127.0.0.1",ntohs(address.sin_port)),"vending render loopback connect failed");
    sockets.peer=accept(sockets.listener,nullptr,nullptr);
    const DWORD timeout=1000;
    Require(sockets.peer!=INVALID_SOCKET && setsockopt(sockets.peer,SOL_SOCKET,SO_RCVTIMEO,
        reinterpret_cast<const char*>(&timeout),sizeof(timeout))==0,"vending render accept failed");
    const auto expectRequest = [&](const std::vector<u8>& expected) {
        std::vector<u8> actual(expected.size());
        size_t received=0;
        while(received<actual.size()){
            const int count=recv(sockets.peer,reinterpret_cast<char*>(actual.data()+received),
                static_cast<int>(actual.size()-received),0);
            Require(count>0,"vending render did not send request");
            received+=static_cast<size_t>(count);
        }
        Require(actual==expected,"vending render sent an unexpected request");
    };
    std::vector<u8> board(86);
    write16(board,0,0x131);write32(board,2,boardSeller);
    std::memcpy(board.data()+6,"Board vendor",12);
    Dispatch(mode,board);
    sync();
    const QString boardName=QStringLiteral("commerceShopBoard_%1").arg(boardSeller);
    const auto browse = [&] {
        auto* boardItem=item(boardName);
        const auto center=boardItem->mapToScene(QPointF(boardItem->width()/2,boardItem->height()/2));
        Require(QRectF(0,0,view.width(),view.height()).contains(center),"vendor board is outside the fixture viewport");
        Click(app,view,boardItem);
        Require(state.requestedSellerId==boardSeller,"board click did not select the displayed seller");
        expectRequest({0x30,1,0x78,0x56,0x34,0x12});
    };
    browse();
    const auto listing=offerPacket(0x133,1202,91,2,true);
    Dispatch(mode,listing);
    sync();
    Require(item("commerceDetail_shop_91")->property("item").toMap().value("count").toUInt()==2,
        "board stock did not display the listed quantity");
    Require(view.grabWindow().save(output+"/vending-board-stock.png"),"save board stock frame failed");
    ++frames;
    Click(app,view,item("commerceBuy_91"));
    expectRequest({0x34,1,12,0,0x78,0x56,0x34,0x12,1,0,91,0});
    sync();
    auto* shopPanel=Find(view.rootObject(),"commerce_shop");
    Require(shopPanel && !shopPanel->isVisible(),"submitted quote stayed visible");

    browse();
    Dispatch(mode,listing);
    sync();
    item("commerce_shop");
    Dispatch(mode,{0x32,1,0x78,0x56,0x34,0x12});
    Dispatch(mode,listing);
    sync();
    Require(!shopPanel->isVisible() && !Find(view.rootObject(),boardName),"removed board or quote stayed visible");
    Require(view.grabWindow().save(output+"/vending-board-removed.png"),"save board removal frame failed");
    ++frames;
    mode.m_world->UnregisterActor(&vendor);
    cases.push_back(QJsonObject{{"case","vending-board-browse-purchase-remove"},{"outcome","PASS"}});

    QFile results(output+"/commerce-detail-results.json");Require(results.open(QIODevice::WriteOnly),"open commerce detail results failed");
    results.write(QJsonDocument(QJsonObject{{"cases",cases},{"frames",frames},{"wire_shop_snapshot",true},
        {"wire_vending_snapshot",true},{"wire_trade_accept_and_remote_rows",true},{"wire_equipment_routed",true}}).toJson(QJsonDocument::Indented));
}
