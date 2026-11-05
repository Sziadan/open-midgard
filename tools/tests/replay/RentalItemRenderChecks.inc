void SetRentalTestZone(const char* zone)
{
#if RO_PLATFORM_WINDOWS
    _putenv_s("TZ",zone?zone:"");_tzset();
#else
    if(zone)setenv("TZ",zone,1);else unsetenv("TZ");tzset();
#endif
}

void CheckRentalItemUi(QGuiApplication& app,QQuickView& view,QtUiState* state,CGameMode& mode,
    UIItemWnd& inventory,UIStorageWnd& storage,QtUiStateAdapter& adapter,const QString& output)
{
    struct ZoneRestore {
        bool existed=std::getenv("TZ")!=nullptr;
        std::string value=existed?std::getenv("TZ"):"";
        ~ZoneRestore(){SetRentalTestZone(existed?value.c_str():nullptr);}
    } zoneRestore;
    struct ScaleRestore { int value=GetConfiguredUiScalePercent();~ScaleRestore(){SetRuntimeUiScalePercent(value);} } scaleRestore;
    SetRuntimeUiScalePercent(100);
    SetRentalTestZone("UTC0");
    const auto sync=[&] {
        Require(adapter.syncGameplay(mode,RenderBackendType::LegacyDirect3D7,RenderBackendType::LegacyDirect3D7,Width-1,Height-1),"rental gameplay sync failed");
        Frames(app,view);
    };
    const auto equip=[&](u8 opcode,u32 expiry,u16 id=1201,bool identified=true) {
        auto bytes=Equipment(opcode,identified,0);bytes[6]=u8(id);bytes[7]=u8(id>>8);
        for(int k=0;k<4;++k)bytes[24+k]=u8(expiry>>(8*k));return bytes;
    };
    const auto openQt=[&](const QString& kind) {
        if(g_windowMgr.m_itemInfoWnd)g_windowMgr.m_itemInfoWnd->SetShow(0);
        sync();auto* tile=Find(view.rootObject(),"commerceTile_"+kind+"_291");Require(tile,"rental tile missing");
        const auto p=tile->mapToScene(QPointF(tile->width()/2,tile->height()/2));Mouse(app,view,p);
        QMouseEvent press(QEvent::MouseButtonPress,p,view.mapToGlobal(p.toPoint()),Qt::RightButton,Qt::RightButton,Qt::NoModifier);
        QMouseEvent release(QEvent::MouseButtonRelease,p,view.mapToGlobal(p.toPoint()),Qt::RightButton,Qt::NoButton,Qt::NoModifier);
        QCoreApplication::sendEvent(&view,&press);QCoreApplication::sendEvent(&view,&release);Frames(app,view);
        Require(g_windowMgr.m_itemInfoWnd && g_windowMgr.m_itemInfoWnd->HasItem(),"actual Qt describe did not open detail");
        g_windowMgr.m_itemInfoWnd->Move(450,70);Mouse(app,view,QPointF(Width-1,Height-1));sync();
    };
    int frames=0;QJsonArray cases;
    const auto capture=[&](const char* name,const char* date) {
        sync();auto* detail=g_windowMgr.m_itemInfoWnd;Require(detail && state->itemInfoVisible(),"bound rental detail missing");
        UIItemInfoWnd::DisplayData data;Require(detail->GetDisplayDataForQt(&data),"native rental detail data missing");
        const bool expiry=date && *date;
        Require((data.description.find("Expiration Date:")!=std::string::npos)==expiry,"expiry row presence differs from wire date");
        if(expiry)Require(data.description.find(date)!=std::string::npos,"native expiry date/time differs from source format");
        auto* label=Find(view.rootObject(),"itemInfoDescription");Require(label && label->isVisible(),"actual Qt item description missing");
        Require(label->property("text").toString().contains("Expiration Date:")==expiry,"Qt expiry row stale");
        if(expiry)Require(label->property("text").toString().contains(QString::fromUtf8(date)),"Qt expiry date differs from native");
        const auto rect=label->mapRectToScene(label->boundingRect()).toAlignedRect().intersected(QRect(0,0,Width,Height));
        const QImage qt=view.grabWindow().convertToFormat(QImage::Format_RGB32);
        Require(qt.save(output+"/"+name+"-qt.png"),"save rental Qt frame failed");
        if(rect.isEmpty() || (Pixels(qt,rect,0xffff0000)>8)!=expiry)
            std::cerr<<"rental paint "<<name<<" rect="<<rect.x()<<','<<rect.y()<<','<<rect.width()<<','<<rect.height()
                <<" red="<<Pixels(qt,rect,0xffff0000)<<'\n';
        Require(!rect.isEmpty() && (Pixels(qt,rect,0xffff0000)>8)==expiry,"actual Qt expiry red pixels missing/stale");
        ArgbDibSurface surface;Require(surface.EnsureSize(Width,Height),"rental native surface failed");
        std::fill_n(surface.GetPixels(),Width*Height,0xff181b20u);detail->DrawNativeToHdc(surface.GetDC());
        const QImage native=QImage(reinterpret_cast<uchar*>(surface.GetPixels()),Width,Height,Width*4,QImage::Format_RGB32).copy();
        Require((Pixels(native,native.rect(),0xffff0000)>8)==expiry,"native expiry red pixels missing/stale");
        Require(native.save(output+"/"+name+"-native.png"),"save rental native frame failed");frames+=2;
        cases.push_back(QJsonObject{{"case",name},{"expiry",expiry},{"date",date?date:""},{"outcome","PASS"}});
    };
    g_session.ClearInventoryItems();ro::commerce::state.cartItems.clear();g_session.OpenStorage(1,600);
    Dispatch(mode,equip(0xd0,1704067200));Dispatch(mode,equip(0xd2,1704240000));Dispatch(mode,equip(0xd1,1704240000));
    PopulateCommerceUi(state,mode,false);openQt("inventory");capture("inventory-date","01/01 00:00");
    auto* description=Find(view.rootObject(),"itemInfoDescription");auto* viewport=description->parentItem()->parentItem();
    Require(viewport->clip() && viewport->property("contentHeight").toReal()>viewport->height(),
        "long rental description is not bounded and scrollable");
    viewport->setProperty("contentY",viewport->property("contentHeight").toReal()-viewport->height());Frames(app,view);
    Require(description->mapToScene(QPointF()).y()<viewport->mapToScene(QPointF()).y(),"description scroll did not move content");
    Require(view.grabWindow().save(output+"/description-scrolled-qt.png"),"save rental scroll frame failed");++frames;
    viewport->setProperty("contentY",0);Frames(app,view);
    SetRuntimeUiScalePercent(125);g_windowMgr.m_itemInfoWnd->Move(300,40);capture("scaled-inventory-date","01/01 00:00");
    auto* scaledLabel=Find(view.rootObject(),"itemInfoDescription");
    Require(qAbs(scaledLabel->mapToScene(QPointF()).x()-(300+112)*1.25)<1,
        "raised detail popup lost native coordinate scaling");
    SetRuntimeUiScalePercent(100);g_windowMgr.m_itemInfoWnd->Move(450,70);
    Dispatch(mode,equip(0xd2,1704326400));capture("inventory-cart-collision","01/01 00:00");
    Dispatch(mode,equip(0xd0,1704153600));capture("inventory-date-updated","01/02 00:00");
    Dispatch(mode,equip(0xd0,0));capture("inventory-permanent",nullptr);
    Dispatch(mode,equip(0xd0,1704067200,1201,false));capture("unidentified-rental","01/01 00:00");
    Dispatch(mode,equip(0xd0,0x80000000));capture("unsigned-2038-date","01/19 03:14");
    Dispatch(mode,equip(0xd0,0xffffffff));capture("unsigned-2106-date","02/07 06:28");
    Require(g_session.GetInventoryItemByIndex(291),"local date display predicted item removal");
    SetRentalTestZone("UTC-2");
    Require(g_ttemmgr.GetExpiryDisplayText(1704067200).find("01/01 02:00")!=std::string::npos,"expiry formatter ignored local timezone");
    SetRentalTestZone("UTC0");

    // Same numeric index in another owner must not follow inventory deletion.
    Dispatch(mode,equip(0xd0,1704067200));PopulateCommerceUi(state,mode,false);openQt("cart");capture("cart-date","01/04 00:00");
    Dispatch(mode,{0x99,2,0x23,1,0xb1,4});capture("cart-survives-inventory-expiry","01/04 00:00");
    Dispatch(mode,{0xaf,0,0x23,1,1,0});sync();Require(state->itemInfoVisible(),"inventory follow-up closed cart detail");
    Dispatch(mode,{0xd2,2,4,0});sync();Require(!state->itemInfoVisible() && !g_windowMgr.m_itemInfoWnd->HasItem(),"cart snapshot removal left stale detail");

    // Open the storage owner through its real native right-click entry.
    storage.OnLBtnDown(storage.m_x+5,storage.m_y+58);storage.OnLBtnUp(storage.m_x+5,storage.m_y+58);
    UIStorageWnd::DisplayData storageData;Require(storage.GetDisplayDataForQt(&storageData),"storage detail fixture missing");
    auto slot=std::find_if(storageData.displaySlots.begin(),storageData.displaySlots.end(),[](const auto& s){return s.occupied;});
    Require(slot!=storageData.displaySlots.end(),"native rental storage slot missing");
    storage.OnRBtnDown(slot->x+slot->width/2,slot->y+slot->height/2);
    g_windowMgr.m_itemInfoWnd->Move(450,70);capture("storage-date","01/03 00:00");
    Dispatch(mode,{0xf8,0});sync();Require(!state->itemInfoVisible(),"closed storage retained bound expiry detail");

    // Selected NORMALITEM_INFO also carries HireExpireDate at row+18.
    std::vector<u8> normal={0xe8,2,26,0,0x23,1,0xf5,1,0,1,2,0,0,0,0,0,0,0,0,0,0,0,0x80,0,0x92,0x65};
    Dispatch(mode,normal);ro::commerce::state.inventoryTab=0;PopulateCommerceUi(state,mode,false);
    openQt("inventory");capture("normal-stack-rental","01/01 00:00");
    Require(g_session.GetInventoryItemByIndex(291)->m_num==2,"normal rental amount lost");
    Dispatch(mode,{0x99,2,0x23,1,0xf5,1});sync();
    Require(state->itemInfoVisible() && g_session.GetInventoryItemByIndex(291)->m_num==1,
        "partial rental decrement closed surviving stack detail");
    Dispatch(mode,{0xaf,0,0x23,1,2,0});sync();Require(!state->itemInfoVisible(),"full normal rental removal retained detail");
    ro::commerce::state.inventoryTab=1;
    Dispatch(mode,equip(0xd0,1704067200));PopulateCommerceUi(state,mode,false);openQt("inventory");
    Dispatch(mode,equip(0xd0,0,1202));sync();Require(!state->itemInfoVisible(),"reused inventory index retargeted an old detail");
    Dispatch(mode,equip(0xd0,1704067200));PopulateCommerceUi(state,mode,false);openQt("inventory");
    auto* tile=Find(view.rootObject(),"commerceTile_inventory_291");Require(tile,"expiring tile missing");
    Mouse(app,view,tile->mapToScene(QPointF(tile->width()/2,tile->height()/2)));
    Dispatch(mode,{0x99,2,0x23,1,0xb1,4});sync();
    Require(!g_session.GetInventoryItemByIndex(291) && !state->itemInfoVisible()
        && !Find(view.rootObject(),"commerceTile_inventory_291") && !Find(view.rootObject(),"commerceItemTooltip")->isVisible(),
        "actual expiry left inventory/delegate/tooltip/detail state");
    Require(g_windowMgr.m_chatWnd && g_windowMgr.m_chatWnd->m_show,"rental notification did not reveal chat");
    const auto& notice=g_windowMgr.GetChatEvents().back();
    Require(notice.color==0xffff00 && notice.text.find("deleted from the Inventory")!=std::string::npos,"expiry notice missing/wrong colour");
    Dispatch(mode,{0xaf,0,0x23,1,1,0});sync();Require(g_session.GetInventoryItems().empty(),"expiry follow-up resurrected row");
    Require(view.grabWindow().save(output+"/expiry-removed-qt.png"),"save expiry removal frame failed");++frames;

    // Positional message probes distinguish real loaded values from fallbacks.
    QFile messages(output+"/data/msgstringtable.txt");Require(messages.open(QIODevice::ReadOnly),"open rental messages failed");
    const auto original=messages.readAll();messages.close();
    struct RestoreMessages {QString path;QByteArray bytes;~RestoreMessages(){QFile f(path);if(f.open(QIODevice::WriteOnly))f.write(bytes);}} restore{messages.fileName(),original};
    const auto writeEntries=[&](QList<QByteArray> entries){QByteArray bytes;for(int k=0;k<entries.size();++k){if(k)bytes+='#';bytes+=entries[k];}
        Require(messages.open(QIODevice::WriteOnly) && messages.write(bytes)==bytes.size(),"write rental message probe failed");messages.close();};
    auto entries=original.split('#');Require(entries.size()==1272,"rental message fixture count changed");entries[1260]="\n%Y";writeEntries(entries);
    CItemMgr yearLocale;Require(yearLocale.GetExpiryDisplayText(1704067200)==" ^ff0000Expiration Date: 2024^000000","loaded local date format was ignored");
    entries[1255]="\n";writeEntries(entries);CItemMgr emptyLocale;
    Require(emptyLocale.GetExpiryDisplayText(1704067200).empty(),"configured empty expiry message was replaced by fallback");
    QFile results(output+"/rental-results.json");Require(results.open(QIODevice::WriteOnly),"open rental results failed");
    results.write(QJsonDocument(QJsonObject{{"cases",cases},{"frames",frames},{"local_zone",true},{"unsigned_wire_epoch",true},
        {"source_namespace_isolation",true},{"live_update_and_removal",true},{"loaded_date_format",true},{"configured_empty_message",true},{"outcome","PASS"}}).toJson());
}
