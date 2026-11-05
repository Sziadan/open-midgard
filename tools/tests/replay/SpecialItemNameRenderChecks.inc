void CheckSpecialItemNames(QGuiApplication& app,QQuickView& view,QtUiState* state,CGameMode& mode,
    UIItemWnd& inventory,UIStorageWnd& storage,QtUiStateAdapter& adapter,const QString& output)
{
    struct Case { const char* id;u16 itemId;u8 type;std::array<u16,4> cards;const char* name;bool identified=true;u8 refine=0; };
    const Case cases[]={
        {"forged-normal",1202,5,{255,0,0x5678,0x1234},"Smith's Knife",true,7},
        {"forged-ice",1202,5,{255,1,0x5678,0x1234},"Smith's Ice Knife"},
        {"forged-earth",1202,5,{255,2,0x5678,0x1234},"Smith's Earth Knife"},
        {"forged-fire",1202,5,{255,3,0x5678,0x1234},"Smith's Fire Knife"},
        {"forged-wind",1202,5,{255,4,0x5678,0x1234},"Smith's Wind Knife"},
        {"one-star",1202,5,{255,0x0500,0x5678,0x1234},"Very Strong Smith's Knife"},
        {"two-stars",1202,5,{255,0x0a01,0x5678,0x1234},"Very Very Strong Smith's Ice Knife"},
        {"three-stars",1202,5,{255,0x0f03,0x5678,0x1234},"Very Very Very Strong Smith's Fire Knife",true,7},
        {"unidentified-forge",1202,5,{255,0x0f03,0x5678,0x1234},"Dagger",false,7},
        {"created-potion",501,0,{254,0,0x5678,0x1234},"Smith's Red Potion"},
        {"named-pet",9001,4,{0,0,0,1},"BelovedPoring Egg"},
        {"named-pet-intimacy",9001,4,{0,0,300,1},"BelovedPoring Egg"},
        {"unnamed-pet",9001,4,{0,0,300,0},"Poring Egg"},
        {"unidentified-pet",9001,4,{0,0,300,1},"Poring Egg",false},
        // Raw type seven is primary/historical compatibility, not selected emission.
        {"legacy-type7-named-pet",9001,7,{0,0,0,1},"BelovedPoring Egg"},
        {"legacy-type7-unnamed-pet",9001,7,{0,0,300,0},"Poring Egg"},
        // Synthetic source-branch probes; not all combinations are server reachable.
        {"create-nonzero-word-guard",501,0,{254,1,0x5678,0x1234},"Red Potion"},
        {"forge-type-guard",501,0,{255,0x0f03,0x5678,0x1234},"Red Potion"},
        {"unknown-element",1202,5,{255,5,0x5678,0x1234},"SmithKnife"},
        {"unknown-star",1202,5,{255,0x0600,0x5678,0x1234},"Smith's Knife"}
    };
    mode.m_characterNameCache[0x12345678]="Smith";
    g_session.OpenStorage(1,600);
    const auto frameFor=[&](const Case& test,u8 lowOpcode) {
        const bool normal=test.type==0;
        std::vector<u8> frame=normal
            ? std::vector<u8>{lowOpcode,2,26,0,0x23,1,u8(test.itemId),u8(test.itemId>>8),test.type,u8(test.identified),3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
            : Equipment(lowOpcode,test.identified,0);
        frame[6]=u8(test.itemId);frame[7]=u8(test.itemId>>8);frame[8]=test.type;
        if(!normal) { frame[15]=test.refine;if(test.itemId==9001)frame[10]=frame[11]=0; }
        const int start=normal?14:16;
        for(int k=0;k<4;++k){frame[start+k*2]=u8(test.cards[k]);frame[start+k*2+1]=u8(test.cards[k]>>8);}
        return frame;
    };
    const auto seed=[&](const Case& test) {
        g_session.ClearInventoryItems();ro::commerce::state.cartItems.clear();
        const int tab=test.type==0?0:(test.type==7?2:1);const int y=tab==0?28:(tab==1?58:88);
        ro::commerce::state.inventoryTab=tab;
        inventory.OnLBtnDown(inventory.m_x+5,inventory.m_y+y);inventory.OnLBtnUp(inventory.m_x+5,inventory.m_y+y);
        storage.OnLBtnDown(storage.m_x+5,storage.m_y+y);storage.OnLBtnUp(storage.m_x+5,storage.m_y+y);
        Dispatch(mode,{0xea,2,4,0});Dispatch(mode,{0xd1,2,4,0});
        const bool normal=test.type==0;
        Dispatch(mode,frameFor(test,normal?0xe8:0xd0));
        Dispatch(mode,frameFor(test,normal?0xe9:0xd2));
        Dispatch(mode,frameFor(test,normal?0xea:0xd1));
        PopulateCommerceUi(state,mode,false);Frames(app,view);
    };
    const auto check=[&](const Case& test,const QString& name,const QString& prefix) {
        const QString fullName=(test.identified && test.refine && (test.type==4 || test.type==5)?QString("+%1 ").arg(test.refine):QString())+name;
        const QString expectedHover=fullName+(test.type==0?": 3 ea":": 1 ea");
        QString expectedRow=fullName;
        if(test.itemId==1202)expectedRow.replace("Knife","Knife [4]").replace("Dagger","Dagger [4]");
        UIItemInfoWnd detail;
        detail.SetItemInfo(*g_session.GetInventoryItemByIndex(291),0,0);detail.SetShow(1);
        UIItemInfoWnd::DisplayData detailData;
        Require(detail.GetDisplayDataForQt(&detailData) && QString::fromStdString(detailData.name)==expectedRow,
            "detailed item name differs from shared special composition");
        for(const QString& kind:{QString("inventory"),QString("cart")}) {
            const auto rows=state->commerceData().value(kind+"Items").toList();
            if(rows.size()!=1 || rows[0].toMap().value("name").toString()!=expectedRow)
                throw std::runtime_error(std::string("special row mismatch: ")+test.id+" / "+kind.toStdString());
        }
        Hover(inventory);shopui::ItemHoverInfo native;
        if(!inventory.GetHoveredItemForQt(&native) || QString::fromStdString(native.text)!=expectedHover)
            throw std::runtime_error(std::string("special native hover mismatch: ")+test.id);
        const bool representative=QString(test.id)=="three-stars" || QString(test.id)=="created-potion" || QString(test.id)=="named-pet-intimacy";
        if(representative)Require(Native(inventory,0xffffffffu).save(output+"/"+prefix+"-native-inventory.png"),"save special native frame failed");
        inventory.OnMouseMove(-100,-100);
        const auto storagePoint=Hover(storage);
        Require(storage.GetHoveredItemForQt(&native) && QString::fromStdString(native.text)==expectedHover,"special native storage name mismatch");
        if(representative)Require(Native(storage,0xffffffffu).save(output+"/"+prefix+"-native-storage.png"),"save special native storage frame failed");
        Require(adapter.syncGameplay(mode,RenderBackendType::LegacyDirect3D7,RenderBackendType::LegacyDirect3D7,storagePoint.x(),storagePoint.y()),"special storage adapter failed");
        Frames(app,view);auto* label=Find(view.rootObject(),"hoverPlayerName");
        Require(label && label->isVisible() && label->property("text").toString()==expectedHover,"actual Qt special storage name mismatch");
        Require(view.grabWindow().save(output+"/"+prefix+"-qt-storage.png"),"save special Qt storage frame failed");
        storage.OnMouseMove(-100,-100);state->setAnchors({});
        for(const QString& kind:{QString("inventory"),QString("cart")}) {
            auto* tile=Find(view.rootObject(),"commerceTile_"+kind+"_291");Require(tile,"special item tile missing");
            Mouse(app,view,tile->mapToScene(QPointF(tile->width()/2,tile->height()/2)));
            label=Find(view.rootObject(),"commerceItemTooltipText");
            Require(label && label->isVisible() && label->property("text").toString()==expectedHover,"actual Qt special hover name mismatch");
            Require(view.grabWindow().save(output+"/"+prefix+"-qt-"+kind+".png"),"save special Qt frame failed");
            Mouse(app,view,QPointF(Width-1,Height-1));
        }
    };
    QJsonArray outcomes;
    for(const auto& test:cases) {
        seed(test);check(test,QString::fromUtf8(test.name),test.id);
        outcomes.push_back(QJsonObject{{"case",test.id},{"name",test.name},{"outcome","PASS"}});
    }
    Case delayed={"delayed-producer",1202,5,{255,0,0x5678,0x1234},"'s Knife"};
    mode.m_characterNameCache.erase(0x12345678);seed(delayed);check(delayed,"'s Knife","producer-pending");
    UIItemInfoWnd pendingDetail;
    pendingDetail.SetItemInfo(*g_session.GetInventoryItemByIndex(291),0,0);pendingDetail.SetShow(0);
    struct DetachDetail { ~DetachDetail(){g_windowMgr.m_itemInfoWnd=nullptr;} } detachDetail;
    g_windowMgr.m_itemInfoWnd=&pendingDetail;pendingDetail.m_isDirty=0;
    Hover(inventory);
    Require(Native(inventory,0xffffffffu).save(output+"/producer-pending-native-inventory.png"),"save pending producer native frame failed");
    auto* pendingTile=Find(view.rootObject(),"commerceTile_cart_291");Require(pendingTile,"pending producer tile missing");
    Mouse(app,view,pendingTile->mapToScene(QPointF(pendingTile->width()/2,pendingTile->height()/2)));
    inventory.m_isDirty=0;storage.m_isDirty=0;
    std::vector<u8> reply(30,0);
    const std::array<u8,6> header={0x94,1,0x78,0x56,0x34,0x12};
    std::copy(header.begin(),header.end(),reply.begin());std::copy_n("Maker",5,reply.begin()+6);
    Dispatch(mode,reply);
    Require(inventory.m_isDirty && storage.m_isDirty && pendingDetail.m_isDirty,"producer reply did not invalidate native item names");
    pendingDetail.SetShow(1);UIItemInfoWnd::DisplayData updatedDetail;
    Require(pendingDetail.GetDisplayDataForQt(&updatedDetail) && updatedDetail.name=="Maker's Knife [4]",
        "existing detailed item name did not update without item resend");pendingDetail.SetShow(0);
    shopui::ItemHoverInfo arrived;
    Require(inventory.GetHoveredItemForQt(&arrived) && arrived.text=="Maker's Knife: 1 ea",
        "native stationary hover did not update on producer reply");
    Require(Native(inventory,0xffffffffu).save(output+"/producer-arrived-native-inventory.png"),"save arrived producer native frame failed");
    inventory.OnMouseMove(-100,-100);
    PopulateCommerceUi(state,mode,false);Frames(app,view);
    auto* currentName=Find(view.rootObject(),"commerceItemTooltipText");
    Require(currentName && currentName->isVisible() && currentName->property("text").toString()=="Maker's Knife: 1 ea",
        "Qt stationary hover did not update on producer reply");
    check(delayed,"Maker's Knife","producer-arrived");
    // Replacing only the cache result must recompute names without another item packet.
    std::fill(reply.begin()+6,reply.end(),0);std::copy_n("New",3,reply.begin()+6);Dispatch(mode,reply);
    PopulateCommerceUi(state,mode,false);Frames(app,view);check(delayed,"New's Knife","producer-renamed");
    Dispatch(mode,{0xd0,2,4,0});Dispatch(mode,{0xd2,2,4,0});Dispatch(mode,{0xd1,2,4,0});
    PopulateCommerceUi(state,mode,false);Frames(app,view);
    Require(!Find(view.rootObject(),"commerceTile_cart_291"),"special name survived item removal");
    // A configured empty marker differs from the English fallback. This proves
    // the positional table is used and an intentional empty message is retained.
    QFile messages(output+"/data/msgstringtable.txt");Require(messages.open(QIODevice::ReadOnly),"open message override probe failed");
    const QByteArray originalMessages=messages.readAll();messages.close();
    struct RestoreMessages {
        QString path;QByteArray bytes;
        ~RestoreMessages(){QFile file(path);if(file.open(QIODevice::WriteOnly))file.write(bytes);}
    } restoreMessages{messages.fileName(),originalMessages};
    auto entries=originalMessages.split('#');Require(entries.size()==758,"message fixture index count changed");
    entries[756]="\n";QByteArray emptyMarker;
    for(int k=0;k<entries.size();++k){if(k)emptyMarker+='#';emptyMarker+=entries[k];}
    Require(messages.open(QIODevice::WriteOnly) && messages.write(emptyMarker)==emptyMarker.size(),"write message override probe failed");messages.close();
    CItemMgr configuredMessages;ITEM_INFO egg;egg.SetItemId(9001);egg.m_itemType=7;egg.m_isIdentified=1;egg.m_slot[3]=1;
    Require(configuredMessages.GetItemName(egg,false)=="Poring Egg","configured empty message was replaced by fallback text");
    QFile file(output+"/special-name-results.json");Require(file.open(QIODevice::WriteOnly),"open special name results failed");
    file.write(QJsonDocument(QJsonObject{{"cases",outcomes},{"delayed_name_without_item_resend",true},{"name_replacement",true},{"row_removal",true},{"configured_empty_message_preserved",true},{"outcome","PASS"}}).toJson());
}
