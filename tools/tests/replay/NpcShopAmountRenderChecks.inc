void CheckNpcShopAmounts(QGuiApplication& app,QQuickView& view,QtUiState* uiState,CGameMode& mode,
    QtUiStateAdapter& adapter,const QString& output)
{
    Require(uiState,"NPC amount state missing");
    struct ScaleRestore { int value=GetConfiguredUiScalePercent();~ScaleRestore(){SetRuntimeUiScalePercent(value);} } scaleRestore;
    SetRuntimeUiScalePercent(100);Require(QDir().mkpath(output),"NPC amount output directory failed");
    QJsonArray cases;int frameCount=0;
    const auto sync=[&] {
        if(g_windowMgr.m_itemShopWnd)g_windowMgr.m_itemShopWnd->Move(20,100);
        if(g_windowMgr.m_itemPurchaseWnd)g_windowMgr.m_itemPurchaseWnd->Move(450,100);
        if(g_windowMgr.m_itemSellWnd)g_windowMgr.m_itemSellWnd->Move(450,100);
        Require(adapter.syncGameplay(mode,RenderBackendType::LegacyDirect3D7,RenderBackendType::LegacyDirect3D7,Width-1,Height-1),"NPC amount gameplay sync failed");Frames(app,view);
    };
    const auto write16=[](std::vector<u8>& b,size_t o,u16 v){b[o]=u8(v);b[o+1]=u8(v>>8);};
    const auto write32=[](std::vector<u8>& b,size_t o,u32 v){for(int i=0;i<4;++i)b[o+size_t(i)]=u8(v>>(8*i));};
    const auto buyList=[&](const std::vector<std::pair<u8,u16>>& rows,u32 price,u32 secondary=0) {
        std::vector<u8> b(4+11*rows.size(),0);write16(b,0,0xc6);write16(b,2,u16(b.size()));
        for(size_t i=0;i<rows.size();++i){size_t at=4+i*11;write32(b,at,price);write32(b,at+4,secondary);b[at+8]=rows[i].first;write16(b,at+9,rows[i].second);}return b;
    };
    const auto sellList=[&](u16 index,u32 price,u32 secondary) {std::vector<u8>b(14,0);write16(b,0,0xc7);write16(b,2,14);write16(b,4,index);write32(b,6,price);write32(b,10,secondary);return b;};
    const auto open=[&](u32 id){Dispatch(mode,{0xc4,0,u8(id),u8(id>>8),u8(id>>16),u8(id>>24)});};
    const auto stock=[&](u16 quantity) {
        std::vector<u8> b(26,0);write16(b,0,0x02e8);write16(b,2,26);
        write16(b,4,3);write16(b,6,501);b[9]=1;write16(b,10,quantity);Dispatch(mode,b);
    };
    const auto find=[&](const QString& n){return Find(view.rootObject(),n);};
    const auto label=[&](const QString& name,const QString& expected) {
        auto* item=find(name);Require(item&&item->isVisible(),"NPC amount text object missing or hidden");
        Require(item->property("text").toString()==expected,"NPC amount rendered text differs from exact decimal value");
        Require(item->property("contentWidth").toReal()<=item->width(),"exact NPC amount does not fit its text column");
    };
    const auto capture=[&](const QString& stem,auto* nativeWindow) {
        sync();QImage qt=view.grabWindow().convertToFormat(QImage::Format_RGB32);Require(qt.save(output+"/"+stem+"-qt.png"),"save NPC amount Qt frame failed");++frameCount;
        ArgbDibSurface surface;Require(surface.EnsureSize(Width,Height),"NPC amount native surface allocation failed");std::fill_n(surface.GetPixels(),Width*Height,0xff181b20u);
        nativeWindow->DrawNativeToHdc(surface.GetDC());QImage native=QImage(reinterpret_cast<uchar*>(surface.GetPixels()),Width,Height,Width*4,QImage::Format_RGB32).copy();
        const QRect body(nativeWindow->m_x,nativeWindow->m_y,nativeWindow->m_w,nativeWindow->m_h);
        Require(Pixels(native,body,0xff181b20u)<body.width()*body.height()/2,"NPC native pane was not painted");
        Require(native.save(output+"/"+stem+"-native.png"),"save NPC amount native frame failed");++frameCount;
    };
    const auto mark=[&](const char* name){cases.push_back(QJsonObject{{"case",name},{"outcome","PASS"}});};

    g_windowMgr.RemoveAllWindows();g_session.ClearNpcShopState();g_session.ClearInventoryItems();ro::commerce::Reset();
    // Literal C6 carries 32-bit price words. Exercise both high-bit and maximum values.
    open(0x12345a01);Dispatch(mode,buyList({{0,501}},0x80000000u,0xffffffffu));sync();
    Require(g_session.m_shopRows.size()==1,"literal C6 source row missing");
    Require(g_session.m_shopRows[0].price==0x80000000u&&g_session.GetNpcShopUnitPrice(g_session.m_shopRows[0])==0xffffffffu,"C6 source price words lost unsigned precision");
    label("npcShopSourcePrice_0",QStringLiteral("4294967295"));
    Require(g_session.AdjustNpcShopDealBySourceRow(0,1),"C6 max-price deal insertion failed");sync();
    Require(g_session.m_shopDealRows[0].unitPrice==0xffffffffu&&g_session.m_shopDealTotal==0xffffffffu,"C6 max-price total differs");
    label("npcShopBuyAmount_0",QStringLiteral("4294967295"));label("npcShopBuyTotal",QStringLiteral("4294967295"));
    capture("npc-shop-amount-unsigned-buy",g_windowMgr.m_itemPurchaseWnd);mark("literal C6 high-bit and exact UINT32_MAX price text and native/QML render");

    open(0x12345a06);Dispatch(mode,buyList({{0,501}},90,0));sync();
    Require(g_session.AdjustNpcShopDealBySourceRow(0,3),"free C6 source insertion failed");sync();
    Require(g_session.m_shopDealTotal==0&&g_session.m_shopDealRows[0].unitPrice==0,"free C6 source fell back to base price");
    label("npcShopSourcePrice_0",QStringLiteral("0"));label("npcShopBuyAmount_0",QStringLiteral("0"));label("npcShopBuyTotal",QStringLiteral("0"));
    mark("literal C6 zero secondary is authoritative free price");

    // A literal C7 inventory fixture verifies that a zero secondary price stays zero.
    g_session.ClearNpcShopState();g_session.ClearInventoryItems();Dispatch(mode,Equipment(0xd0,true,0));
    open(0x12345a02);Dispatch(mode,sellList(3,0xffffffffu,0));sync();
    Require(!g_session.m_shopRows.empty()&&g_session.m_shopRows[0].price==0xffffffffu&&g_session.GetNpcShopUnitPrice(g_session.m_shopRows[0])==0,"literal C7 UINT32_MAX/zero-secondary source price differs");
    label("npcShopSourcePrice_0",QStringLiteral("0"));
    Require(g_session.AdjustNpcShopDealBySourceRow(0,1),"C7 zero-price deal insertion failed");sync();
    Require(g_session.m_shopDealRows[0].unitPrice==0&&g_session.m_shopDealTotal==0,"zero secondary price changed to another price");
    label("npcShopSellAmount_0",QStringLiteral("0"));label("npcShopSellTotal",QStringLiteral("0"));
    capture("npc-shop-amount-zero-sell",g_windowMgr.m_itemSellWnd);
    // A zero-count source must reject insertion rather than invent unlimited stock.
    g_session.m_shopRows[0].availableCount=0;g_session.m_shopDealRows.clear();g_session.m_shopDealTotal=0;
    Require(!g_session.AdjustNpcShopDealBySourceRow(0,1)&&g_session.m_shopDealRows.empty(),"zero-count C7 source invented sell quantity");
    g_session.m_shopRows[0].availableCount=-1;
    Require(!g_session.AdjustNpcShopDealBySourceRow(0,1)&&g_session.m_shopDealRows.empty(),"negative sell stock invented quantity");
    mark("literal C7 zero secondary with synthetic zero/negative availability guards");

    // Rebuild as a C7 source with a 1.5B unit amount; production adjustment
    // multiplies and accumulates in the session, including values above int.
    g_session.ClearNpcShopState();g_session.ClearInventoryItems();stock(3);
    open(0x12345a03);Dispatch(mode,sellList(3,0,1500000000u));sync();
    Require(g_session.AdjustNpcShopDealBySourceRow(0,3),"1.5B source adjustment failed");sync();
    Require(g_session.m_shopDealTotal==4500000000ull,"1.5B times three did not total exactly 4500000000");
    label("npcShopSellAmount_0",QStringLiteral("4500000000"));label("npcShopSellTotal",QStringLiteral("4500000000"));
    Require(!g_session.AdjustNpcShopDealBySourceRow(0,2147483647)
        &&g_session.m_shopDealRows[0].quantity==3,"sell quantity exceeded available stock or overflowed");
    capture("npc-shop-amount-wide-sell",g_windowMgr.m_itemSellWnd);mark("literal C7 production adjustment computes 4500000000");

    // Synthetic C6 source rows stress aggregate precision beyond JavaScript's
    // exact integer range. Wire fixture prices are uint32; each quantity is capped at 30000.
    g_session.ClearNpcShopState();open(0x12345a04);std::vector<std::pair<u8,u16>> many;many.reserve(100);
    for(int i=0;i<100;++i)many.push_back({0,u16(600+i)});
    Dispatch(mode,buyList(many,0xffffffffu,0xffffffffu));sync();
    Require(g_session.m_shopRows.size()==100,"multirow C6 source fixture row count differs");
    for(size_t i=0;i<g_session.m_shopRows.size();++i)Require(g_session.AdjustNpcShopDealBySourceRow(i,i==0?29999:30000),"multirow C6 production adjustment failed");
    // An odd value above 2^53 cannot be represented exactly by a JS Number.
    sync();const QString exact=QStringLiteral("12884897590032705");
    Require(g_session.m_shopDealTotal==12884897590032705ull,"multirow amount total lost exact integer precision");
    label("npcShopBuyAmount_0",QStringLiteral("128844723882705"));
    label("npcShopBuyTotal",exact);
    auto* total=find("npcShopBuyTotal");Require(total,"QML total label unavailable");
    const QImage image=view.grabWindow().convertToFormat(QImage::Format_RGB32);const QRect ink=total->mapRectToScene(total->boundingRect()).toAlignedRect().intersected(image.rect());
    const QRgb totalColor=total->property("color").value<QColor>().rgb();
    Require(!ink.isEmpty()&&Pixels(image,ink,totalColor)>2,"large exact QML total has no rendered ink");
    capture("npc-shop-amount-js-safe-boundary",g_windowMgr.m_itemPurchaseWnd);mark("synthetic multirow C6 aggregate above JS 2^53 preserves decimal QML text");

    // Extreme delta inputs must clamp without signed overflow; removal below
    // zero must clear the deal safely. This row is synthetic source state.
    g_session.ClearNpcShopState();open(0x12345a05);Dispatch(mode,buyList({{0,501}},1,1));sync();
    Require(g_session.AdjustNpcShopDealBySourceRow(0,2147483647),"INT_MAX source delta failed");
    Require(g_session.m_shopDealRows[0].quantity==30000,"INT_MAX source delta did not clamp to stock limit");
    Require(!g_session.AdjustNpcShopDealBySourceRow(0,2147483647)
        &&g_session.m_shopDealRows.size()==1&&g_session.m_shopDealRows[0].quantity==30000,
        "positive delta overflow removed a saturated deal");
    Require(g_session.AdjustNpcShopDealBySourceRow(0,-2147483647-1),"INT_MIN source delta failed to remove clamped deal");
    Require(g_session.m_shopDealRows.empty()&&g_session.m_shopDealTotal==0,"INT_MIN delta did not clamp safely to zero");
    Require(g_session.AdjustNpcShopDealBySourceRow(0,1),"underflow fixture deal insertion failed");
    Require(g_session.AdjustNpcShopDealBySourceRow(0,-30001),"deal underflow removal failed");
    Require(g_session.m_shopDealRows.empty()&&g_session.m_shopDealTotal==0,"deal underflow left stale amount");
    mark("synthetic INT_MAX and INT_MIN quantity deltas clamp safely and underflow removes row");
    g_session.m_shopMode=NpcShopMode::None;
    Require(!g_session.AdjustNpcShopDealBySourceRow(0,1)&&g_session.m_shopDealRows.empty(),"inactive shop accepted a quantity change");
    mark("inactive shop mode rejects deal edits");

    QFile results(output+"/npc-shop-amount-results.json");Require(results.open(QIODevice::WriteOnly),"open NPC amount results failed");
    results.write(QJsonDocument(QJsonObject{{"cases",cases},{"saved_frames",frameCount},{"literal_packet_families",QJsonArray{"00C6 buy","00C7 sell"}},
        {"synthetic_multirow_aggregate",true},{"outcome","PASS"}}).toJson(QJsonDocument::Indented));
}
