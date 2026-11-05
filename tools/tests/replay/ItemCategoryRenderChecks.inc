void CheckItemCategoryUi(QGuiApplication& app,QQuickView& view,QtUiState* uiState,CGameMode& mode,
    UIItemWnd& inventory,UIStorageWnd& storage,QtUiStateAdapter& adapter,const QString& output)
{
    Require(uiState,"item category state missing");
    struct Fixture { int id=0; QString aegis; QString name; int wireType=0; int index=0; bool historical=false; };
    const QString sourcePath=QString::fromUtf8(RO_PACKET_SOURCE_DIR)+"/tools/tests/fixtures/packet-item-data/item-type-source-cases.json";
    QFile source(sourcePath);Require(source.open(QIODevice::ReadOnly),"item type source cases missing");
    QJsonParseError parseError{};const QJsonDocument sourceDoc=QJsonDocument::fromJson(source.readAll(),&parseError);
    Require(parseError.error==QJsonParseError::NoError && sourceDoc.isObject(),"item type source cases are invalid JSON");
    std::vector<Fixture> selectedFixtures,fixtures;int ordinal=0;
    for(const auto value:sourceDoc.object().value("fixtures").toArray()) {
        const auto row=value.toObject();Fixture f;f.id=row.value("id").toInt();f.aegis=row.value("aegis_name").toString();
        f.name=row.value("name").toString();f.wireType=row.value("emitted_type").toInt(-1);f.index=200+ordinal++;
        Require(f.id>0 && f.wireType>=0 && !f.aegis.isEmpty(),"source item fixture is missing its emitted type or identity");
        selectedFixtures.push_back(f);fixtures.push_back(f);
    }
    const auto historical=sourceDoc.object().value("historical_only_tab_branch_probes").toArray();
    for(const auto value:historical) {
        const int type=value.toObject().value("type_id").toInt(-1);Require(type>=0,"historical item type probe is invalid");
        Fixture f;f.id=1201;f.name=QStringLiteral("Knife");f.aegis=QStringLiteral("Knife");f.wireType=type;f.index=200+ordinal++;f.historical=true;
        fixtures.push_back(f);
    }
    // Historical client branch with raw Petegg type in the Other tab, plus
    // out-of-map values. These are synthetic client branch probes, not DB rows.
    fixtures.push_back({7007,QStringLiteral("synthetic_petegg_wire"),QStringLiteral("Pet egg wire probe"),7,200+ordinal++,true});
    fixtures.push_back({7019,QStringLiteral("synthetic_unknown_19"),QStringLiteral("Unknown 19"),19,200+ordinal++,true});
    fixtures.push_back({7255,QStringLiteral("synthetic_unknown_255"),QStringLiteral("Unknown 255"),255,200+ordinal++,true});

    // Only source-derived selected fixtures use this wire grouping. Pet eggs
    // and shadowgear have already normalized to4/5; historical aliases below
    // are direct state probes, not inferred equipment packet categories.
    auto isEquipmentWire=[](int type) {return type==4||type==5||type==8;};
    auto expectedTab=[](int type) {
        switch(type) {
        case 0:case 1:case 2:case 18:return 0;
        case 4:case 5:case 8:case 9:case 11:case 12:case 13:case 14:case 15:return 1;
        case 3:case 6:case 7:case 10:case 16:case 17:return 2;
        default:return -1;
        }
    };
    auto write16=[](std::vector<u8>& bytes,size_t at,u16 value) {bytes[at]=u8(value&0xff);bytes[at+1]=u8(value>>8);};
    auto makeList=[&](u16 opcode,const std::vector<Fixture>& rows,bool equipment) {
        const size_t rowSize=equipment?26u:22u;std::vector<u8> bytes(4+rowSize*rows.size(),0);
        bytes[0]=u8(opcode&0xff);bytes[1]=u8(opcode>>8);write16(bytes,2,static_cast<u16>(bytes.size()));
        for(size_t i=0;i<rows.size();++i) {
            const size_t at=4+i*rowSize;const auto& f=rows[i];write16(bytes,at,static_cast<u16>(f.index));write16(bytes,at+2,static_cast<u16>(f.id));
            bytes[at+4]=u8(f.wireType);bytes[at+5]=1;
            if(equipment) {write16(bytes,at+6,0x0080);write16(bytes,at+8,0);bytes[at+10]=0;bytes[at+11]=0;}
            else {write16(bytes,at+6,1);write16(bytes,at+8,0);}
        }
        return bytes;
    };
    std::vector<Fixture> normalRows,equipRows;
    for(const auto& f:selectedFixtures)(isEquipmentWire(f.wireType)?equipRows:normalRows).push_back(f);
    auto dispatchSnapshot=[&](u16 normalOpcode,u16 equipOpcode,const std::vector<Fixture>& ns,const std::vector<Fixture>& es) {
        Dispatch(mode,makeList(normalOpcode,ns,false));Dispatch(mode,makeList(equipOpcode,es,true));
    };
    g_session.ClearInventoryItems();g_session.ClearStorageItems();ro::commerce::Reset();
    SelectInventoryFixtureTab(0);g_session.OpenStorage(0,600);
    dispatchSnapshot(0x02e8,0x02d0,normalRows,equipRows);
    dispatchSnapshot(0x02ea,0x02d1,normalRows,equipRows);
    // Historical selectors and non-emitted branch probes are projected from
    // session state directly; do not pretend these types came over a packet.
    for(size_t i=selectedFixtures.size();i<fixtures.size();++i) {
        const auto& f=fixtures[i];ITEM_INFO item{};item.m_itemIndex=static_cast<unsigned int>(f.index);
        item.SetItemId(static_cast<unsigned int>(f.id));item.m_itemType=f.wireType;item.m_isIdentified=1;item.m_num=1;
        g_session.SetInventoryItem(item);g_session.SetStorageItem(item);
    }
    PopulateCommerceUi(uiState,mode,false);
    Require(adapter.syncGameplay(mode,RenderBackendType::LegacyDirect3D7,RenderBackendType::LegacyDirect3D7,0,0),
        "initial item category gameplay sync failed");
    Frames(app,view);

    auto chooseNativeTab=[](auto& wnd,int tab) {
        typename std::decay_t<decltype(wnd)>::QtButtonDisplay display{};
        Require(wnd.GetQtTabDisplayForQt(tab,&display),"native item tab display is unavailable");
        Require(display.visible,"native item tab is hidden");
        const int x=display.x+display.width/2,y=display.y+display.height/2;
        wnd.OnLBtnDown(x,y);wnd.OnLBtnUp(x,y);
    };
    auto tabIdSet=[](const QVariantList& rows) {std::multiset<unsigned int> ids;for(const auto& value:rows)ids.insert(value.toMap().value("id").toUInt());return ids;};
    auto indexSet=[](const QVariantList& rows) {std::multiset<unsigned int> ids;for(const auto& value:rows)ids.insert(value.toMap().value("index").toUInt());return ids;};
    auto nativeIdSet=[](const auto& display) {std::multiset<unsigned int> ids;for(const auto& slot:display.displaySlots)if(slot.occupied)ids.insert(slot.itemId);return ids;};
    auto expectedIds=[&](int tab,bool all=false) {std::multiset<unsigned int> ids;for(const auto& f:fixtures)if(all||expectedTab(f.wireType)==tab)ids.insert(static_cast<unsigned int>(f.id));return ids;};
    auto expectedIndices=[&](int tab) {std::multiset<unsigned int> ids;for(const auto& f:fixtures)if(expectedTab(f.wireType)==tab)ids.insert(static_cast<unsigned int>(f.index));return ids;};
    auto renderNative=[&](auto& wnd,const QString& path) {
        ArgbDibSurface surface;Require(surface.EnsureSize(Width,Height),"item category native DIB failed");
        std::fill_n(surface.GetPixels(),Width*Height,0xff181b20u);wnd.DrawNativeToHdc(surface.GetDC());
        const QImage image=QImage(reinterpret_cast<uchar*>(surface.GetPixels()),Width,Height,Width*4,QImage::Format_RGB32).copy();
        Require(image.save(path),"save native item category frame failed");
    };
    QJsonArray outcomes;
    for(int tab=0;tab<3;++tab) {
        chooseNativeTab(inventory,tab);chooseNativeTab(storage,tab);
        auto* qmlTab=Find(view.rootObject(),QString("commerceInventoryTab_%1").arg(tab));Click(app,view,qmlTab);
        PopulateCommerceUi(uiState,mode,false);
        Require(adapter.syncGameplay(mode,RenderBackendType::LegacyDirect3D7,RenderBackendType::LegacyDirect3D7,0,0),
            "item category storage gameplay sync failed");
        PopulateCommerceUi(uiState,mode,false);
        Require(ro::commerce::state.inventoryTab==tab,"QML inventory tab action did not update selection");
        UIItemWnd::DisplayData invDisplay{};UIStorageWnd::DisplayData storageDisplay{};
        Require(inventory.GetDisplayDataForQt(&invDisplay)&&storage.GetDisplayDataForQt(&storageDisplay),"native item category projection failed");
        Require(invDisplay.currentTab==tab&&storageDisplay.currentTab==tab,"native inventory/storage tab action diverged");
        const auto invRows=uiState->commerceData().value("inventoryItems").toList();
        const auto actualIndices=indexSet(invRows);
        for(const auto& f:fixtures)if((expectedTab(f.wireType)==tab)!=(actualIndices.count(unsigned(f.index))!=0))
            std::cerr<<"category mismatch tab="<<tab<<" type="<<f.wireType<<" id="<<f.id<<" index="<<f.index<<'\n';
        Require(tabIdSet(invRows)==expectedIds(tab)&&indexSet(invRows)==expectedIndices(tab),"Qt commerce inventory IDs/indices differ from source tab categories");
        Require(nativeIdSet(invDisplay)==expectedIds(tab),"native inventory IDs differ from source tab categories");
        Require(nativeIdSet(storageDisplay)==expectedIds(tab),"native storage IDs differ from source tab categories");
        Frames(app,view);
        Require(VisibleBagTiles(view.rootObject())==int(expectedIndices(tab).size()),"actual Qt category delegates differ from source sets");
        for(const auto index:expectedIndices(tab)) {
            auto* tile=Find(view.rootObject(),QString("commerceTile_inventory_%1").arg(index));
            Require(tile && tile->isVisible(),"expected item has no actual Qt category tile");
        }
        if(tab==0) {
            auto cash=std::find_if(selectedFixtures.begin(),selectedFixtures.end(),[](const Fixture& f){return f.wireType==18;});
            Require(cash!=selectedFixtures.end(),"selected Cash fixture missing");
            auto* tile=Find(view.rootObject(),QString("commerceTile_inventory_%1").arg(cash->index));
            Mouse(app,view,tile->mapToScene(QPointF(tile->width()/2,tile->height()/2)));
            Require(Find(view.rootObject(),"commerceItemTooltip")->isVisible(),"actual Usable Cash hover did not render");
        }
        std::multiset<unsigned int> qtStorageIds;
        for(const auto& value:uiState->storageData().value("slots").toList()) {
            const auto slot=value.toMap();if(slot.value("occupied").toBool())qtStorageIds.insert(slot.value("itemId").toUInt());
        }
        Require(qtStorageIds==expectedIds(tab),"Qt storage category slots differ from source sets");
        Require(view.grabWindow().save(output+QString("/item-category-qt-tab-%1.png").arg(tab)),"save Qt item category frame failed");
        renderNative(inventory,output+QString("/item-category-native-inventory-tab-%1.png").arg(tab));
        renderNative(storage,output+QString("/item-category-native-storage-tab-%1.png").arg(tab));
        QJsonArray indices;for(const auto index:expectedIndices(tab))indices.push_back(int(index));
        outcomes.push_back(QJsonObject{{"tab",tab},{"inventory_count",invRows.size()},{"expected_indices",indices},{"qml_action","PASS"},{"native_inventory","PASS"},{"native_storage","PASS"}});
    }
    for(const auto& f:fixtures) {
        const auto* inv=g_session.GetInventoryItemByIndex(unsigned(f.index));
        const auto* stored=g_session.GetStorageItemByIndex(unsigned(f.index));
        Require(inv && stored && inv->m_itemType==f.wireType && stored->m_itemType==f.wireType,
            "category projection rewrote or dropped a raw item type");
    }

    // Unworn-bag filtering excludes worn equipment but preserves worn ammo;
    // projections must leave each packet type untouched while refreshing.
    const Fixture *wornArmor=nullptr,*wornAmmo=nullptr;
    for(const auto& f:fixtures){if(f.wireType==4&&!wornArmor)wornArmor=&f;if(f.wireType==10&&!wornAmmo)wornAmmo=&f;}
    Require(wornArmor&&wornAmmo,"category wear fixtures missing");
    auto setWear=[&](const Fixture& f,u16 wear) {const ITEM_INFO* existing=g_session.GetInventoryItemByIndex(static_cast<unsigned>(f.index));Require(existing,"wear fixture missing from inventory");ITEM_INFO updated=*existing;updated.m_wearLocation=wear;g_session.SetInventoryItem(updated);};
    setWear(*wornArmor,0x0002);setWear(*wornAmmo,0x8000);
    chooseNativeTab(inventory,1);SelectInventoryFixtureTab(1);PopulateCommerceUi(uiState,mode,false);
    auto wearRows=uiState->commerceData().value("inventoryItems").toList();
    auto expectedWornEquip=expectedIds(1);expectedWornEquip.erase(static_cast<unsigned int>(wornArmor->id));
    UIItemWnd::DisplayData wornInventoryDisplay{};Require(inventory.GetDisplayDataForQt(&wornInventoryDisplay),"worn inventory native display failed");
    Require(tabIdSet(wearRows)==expectedWornEquip&&nativeIdSet(wornInventoryDisplay)==expectedWornEquip,
        "worn equipment remained in the inventory bag projection");
    chooseNativeTab(inventory,2);SelectInventoryFixtureTab(2);PopulateCommerceUi(uiState,mode,false);
    wearRows=uiState->commerceData().value("inventoryItems").toList();
    UIItemWnd::DisplayData wornAmmoDisplay{};Require(inventory.GetDisplayDataForQt(&wornAmmoDisplay),"worn ammo native display failed");
    Require(tabIdSet(wearRows)==expectedIds(2)&&nativeIdSet(wornAmmoDisplay)==expectedIds(2),
        "worn ammunition exception was lost from the Etc tab");
    Require(g_session.GetInventoryItemByIndex(static_cast<unsigned>(wornArmor->index))->m_itemType==wornArmor->wireType
        &&g_session.GetInventoryItemByIndex(static_cast<unsigned>(wornAmmo->index))->m_itemType==wornAmmo->wireType,
        "Qt/native item tab filtering mutated packet item types");
    outcomes.push_back(QJsonObject{{"case","worn equipment excluded; ammo retained"},{"outcome","PASS"}});

    // A well-framed but partial list is rejected atomically by the production handler.
    const size_t beforeMalformed=g_session.GetInventoryItems().size();
    const Fixture* guard=nullptr;for(const auto& f:fixtures)if(expectedTab(f.wireType)==0){guard=&f;break;}
    Require(guard,"inventory replacement guard fixture missing");const ITEM_INFO before=*g_session.GetInventoryItemByIndex(static_cast<unsigned>(guard->index));
    Dispatch(mode,{0xe8,0x02,0x05,0x00,0xff});
    Require(g_session.GetInventoryItems().size()==beforeMalformed&&g_session.GetInventoryItemByIndex(static_cast<unsigned>(guard->index))
        &&g_session.GetInventoryItemByIndex(static_cast<unsigned>(guard->index))->m_itemType==before.m_itemType,
        "malformed normal inventory refresh partially replaced item category data");
    outcomes.push_back(QJsonObject{{"case","malformed snapshot atomicity"},{"outcome","PASS"}});

    // Replacing a normal item at the same index with an unknown type then Cash
    // exercises a category move on production refresh, without changing the raw ID.
    Fixture changed{19019,QStringLiteral("refresh_type_change"),QStringLiteral("Refresh type change"),19,1200,true};
    Dispatch(mode,makeList(0x02e8,{changed},false));
    for(int tab=0;tab<3;++tab) {
        SelectInventoryFixtureTab(tab);PopulateCommerceUi(uiState,mode,false);
        for(const auto& value:uiState->commerceData().value("inventoryItems").toList())
            Require(value.toMap().value("index").toUInt()!=1200,"unknown item type appeared in a Qt inventory tab");
    }
    Require(g_session.GetInventoryItemByIndex(1200) && g_session.GetInventoryItemByIndex(1200)->m_itemType==19,
        "filtering removed or rewrote authoritative unknown item state");
    changed.wireType=18;Dispatch(mode,makeList(0x02e8,{changed},false));SelectInventoryFixtureTab(0);PopulateCommerceUi(uiState,mode,false);
    auto changedRows=uiState->commerceData().value("inventoryItems").toList();
    Require(changedRows.size()==1&&changedRows[0].toMap().value("index").toUInt()==1200
        &&changedRows[0].toMap().value("id").toUInt()==19019,"raw-type replacement did not move item into usable tab on refresh");
    outcomes.push_back(QJsonObject{{"case","raw type changes across normal snapshot refresh"},{"outcome","PASS"}});
    Require(adapter.syncGameplay(mode,RenderBackendType::LegacyDirect3D7,RenderBackendType::LegacyDirect3D7,0,0),"category refresh sync failed");
    Frames(app,view);
    auto* changedTile=Find(view.rootObject(),"commerceTile_inventory_1200");Require(changedTile,"refreshed Cash tile missing");
    Mouse(app,view,changedTile->mapToScene(QPointF(changedTile->width()/2,changedTile->height()/2)));
    Require(Find(view.rootObject(),"commerceItemTooltip")->isVisible(),"refreshed Cash hover missing");
    changed.wireType=3;Dispatch(mode,makeList(0x02e8,{changed},false));
    Require(adapter.syncGameplay(mode,RenderBackendType::LegacyDirect3D7,RenderBackendType::LegacyDirect3D7,0,0),"category move sync failed");
    Frames(app,view);
    Require(!Find(view.rootObject(),"commerceTile_inventory_1200") && !Find(view.rootObject(),"commerceItemTooltip")->isVisible(),
        "stationary hover or delegate survived moving out of the selected category");
    Click(app,view,Find(view.rootObject(),"commerceInventoryTab_2"));
    Require(adapter.syncGameplay(mode,RenderBackendType::LegacyDirect3D7,RenderBackendType::LegacyDirect3D7,0,0),"moved item tab sync failed");
    Frames(app,view);Require(Find(view.rootObject(),"commerceTile_inventory_1200"),"authoritative changed type did not appear in Other");
    Require(view.grabWindow().save(output+"/item-category-moved-other-qt.png"),"save category move frame failed");
    outcomes.push_back(QJsonObject{{"case","stationary hover cleanup on type move"},{"outcome","PASS"}});

    g_session.ClearInventoryItems();
    Fixture lifeNormal{19018,QStringLiteral("lifecycle_normal"),QStringLiteral("Lifecycle normal"),18,1300,true};
    Fixture lifeEquip{19004,QStringLiteral("lifecycle_equip"),QStringLiteral("Lifecycle equipment"),4,1301,true};
    Dispatch(mode,makeList(0x02e8,{lifeNormal},false));Dispatch(mode,makeList(0x02d0,{lifeEquip},true));
    Dispatch(mode,{0xe8,0x02,0x04,0x00});
    Require(!g_session.GetInventoryItemByIndex(1300)&&g_session.GetInventoryItemByIndex(1301),
        "empty normal snapshot did not clear only normal inventory rows");
    Dispatch(mode,makeList(0x02e8,{lifeNormal},false));
    Require(g_session.GetInventoryItemByIndex(1300)&&g_session.GetInventoryItemByIndex(1301),
        "replacement normal snapshot did not restore the normal row");
    Dispatch(mode,{0xd0,0x02,0x04,0x00});
    Require(g_session.GetInventoryItemByIndex(1300)&&!g_session.GetInventoryItemByIndex(1301),
        "empty equipment snapshot did not clear only equipment rows");
    outcomes.push_back(QJsonObject{{"case","empty and replacement snapshots"},{"outcome","PASS"}});

    // The commerce reset intentionally preserves the selected bag tab.
    SelectInventoryFixtureTab(2);ro::commerce::ResetForMapEntry();
    Require(ro::commerce::state.inventoryTab==2,"commerce reset lost the selected inventory tab");
    outcomes.push_back(QJsonObject{{"case","inventory tab preference survives commerce reset"},{"outcome","PASS"}});
    QFile result(output+"/item-category-results.json");Require(result.open(QIODevice::WriteOnly),"open item category result JSON failed");
    result.write(QJsonDocument(QJsonObject{{"source_fixture_rows",int(sourceDoc.object().value("fixtures").toArray().size())},
        {"historical_synthetic_probes",int(historical.size())},{"extra_synthetic_probes",3},{"frames",10},{"tab_cases",outcomes},{"outcome","PASS"}}).toJson());
}
