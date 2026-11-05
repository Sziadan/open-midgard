// Literal expected names derived from primary Ref table initialization and
// GetItemName/GetPrefix/GetPostfix, not from the rewrite's name builder.
void CheckCardNameCases(QGuiApplication& app,QQuickView& view,QtUiState* state,
    CGameMode& mode,UIItemWnd& inventory,UIStorageWnd& storage,QtUiStateAdapter& adapter,const QString& output)
{
    struct Case {
        const char* id;
        std::array<u16,4> cards;
        const char* name;
        bool identified=true;
        u8 refine=0;
    };
    const Case cases[]={
        {"prefix-single",{4001,0,0,0},"Lucky Knife"},
        {"prefix-double",{4001,4001,0,0},"Double Lucky Knife"},
        {"prefix-triple",{4001,4001,4001,0},"Triple Lucky Knife"},
        {"prefix-quadruple",{4001,4001,4001,4001},"Quadruple Lucky Knife"},
        {"distinct-ids-same-label",{4001,4006,0,0},"Lucky Lucky Knife"},
        {"prefix-order",{4002,4001,0,0},"Vital Lucky Knife"},
        {"prefix-reverse-order",{4001,4002,0,0},"Lucky Vital Knife"},
        {"postfix-single",{4014,0,0,0},"Knife of Champion"},
        {"postfix-double",{4014,4014,0,0},"Knife of Champion Double"},
        {"postfix-triple",{4014,4014,4014,0},"Knife of Champion Triple"},
        {"postfix-quadruple",{4014,4014,4014,4014},"Knife of Champion Quadruple"},
        {"mixed-affixes-refined",{4001,4014,4002,4014},"+7 Lucky Vital Knife of Champion Double",true,7},
        {"postfix-first-slot",{4014,4001,4014,4002},"Lucky Vital Knife of Champion Double"},
        {"unidentified-suppression",{4001,4014,4002,4014},"Dagger",false,7},
        // Synthetic source branch probes; no claim of server-reachable card slots.
        {"empty-first-slot-gate",{0,4001,4014,0},"Knife"},
        {"unknown-first-slot-gate",{0xeeee,4001,4014,0},"Knife"},
        {"unknown-later-slot",{4001,0xeeee,0,0},"Lucky Knife"}
    };
    g_session.ClearInventoryItems();ro::commerce::state.cartItems.clear();
    ro::commerce::state.inventoryTab=1;
    inventory.OnLBtnDown(inventory.m_x+5,inventory.m_y+58);inventory.OnLBtnUp(inventory.m_x+5,inventory.m_y+58);
    QJsonArray outcomes;
    for(const auto& test:cases) {
        auto frame=Equipment(0xd0,test.identified,0);
        // Source-extracted Knife1202 has four sockets; all four-card fixtures fit.
        frame[6]=0xb2;frame[15]=test.refine;
        for(size_t slot=0;slot<4;++slot) {
            frame[16+slot*2]=u8(test.cards[slot]);frame[17+slot*2]=u8(test.cards[slot]>>8);
        }
        Dispatch(mode,frame);frame[0]=0xd2;Dispatch(mode,frame);frame[0]=0xd1;Dispatch(mode,frame);
        PopulateCommerceUi(state,mode,false);Frames(app,view);
        const QString expectedHover=QString::fromUtf8(test.name)+": 1 ea";
        const QString expectedList=QString::fromUtf8(test.name)+" [4]";
        // The existing list convention inserts the socket suffix next to the base
        // name, before postfixes; hover composition has no computed socket suffix.
        QString expectedRow=expectedList;
        if(QString::fromUtf8(test.name).contains("Knife of Champion"))
            expectedRow=QString::fromUtf8(test.name).replace("Knife of Champion","Knife [4] of Champion");
        for(const QString& kind:{QString("inventory"),QString("cart")}) {
            const auto rows=state->commerceData().value(kind+"Items").toList();
            if(rows.size()!=1 || rows[0].toMap().value("name").toString()!=expectedRow)
                throw std::runtime_error(std::string("card row name mismatch: ")+test.id+" / "+kind.toStdString());
        }
        Hover(inventory);shopui::ItemHoverInfo nativeHover;
        if(!inventory.GetHoveredItemForQt(&nativeHover) || QString::fromStdString(nativeHover.text)!=expectedHover)
            throw std::runtime_error(std::string("native card hover mismatch: ")+test.id);
        const QRgb colour=test.identified?0xffffffffu:0xffaaaaaau;
        if(QString(test.id)=="distinct-ids-same-label" || QString(test.id)=="postfix-quadruple"
            || QString(test.id)=="unidentified-suppression")
            Require(Native(inventory,colour).save(output+"/card-"+test.id+"-native.png"),"save native card frame failed");
        inventory.OnMouseMove(-100,-100);
        const QPoint storagePoint=Hover(storage);
        Require(storage.GetHoveredItemForQt(&nativeHover) && QString::fromStdString(nativeHover.text)==expectedHover,
            "native storage card hover differs from source");
        if(QString(test.id)=="distinct-ids-same-label" || QString(test.id)=="postfix-quadruple"
            || QString(test.id)=="unidentified-suppression")
            Require(Native(storage,colour).save(output+"/card-"+test.id+"-native-storage.png"),"save native storage card frame failed");
        Require(adapter.syncGameplay(mode,RenderBackendType::LegacyDirect3D7,RenderBackendType::LegacyDirect3D7,
            storagePoint.x(),storagePoint.y()),"card storage gameplay adapter failed");
        Frames(app,view);
        auto* storageLabel=Find(view.rootObject(),"hoverPlayerName");
        Require(storageLabel && storageLabel->isVisible() && storageLabel->property("text").toString()==expectedHover,
            "actual QML storage card label differs from source");
        Require(view.grabWindow().save(output+"/card-"+test.id+"-qt-storage.png"),"save QML storage card frame failed");
        storage.OnMouseMove(-100,-100);state->setAnchors({});
        for(const QString& kind:{QString("inventory"),QString("cart")}) {
            auto* tile=Find(view.rootObject(),"commerceTile_"+kind+"_291");Require(tile,"card item tile missing");
            Mouse(app,view,tile->mapToScene(QPointF(tile->width()/2,tile->height()/2)));
            auto* label=Find(view.rootObject(),"commerceItemTooltipText");
            if(!label || !label->isVisible() || label->property("text").toString()!=expectedHover)
                throw std::runtime_error(std::string("actual QML card hover mismatch: ")+test.id+" / "+kind.toStdString());
            Require(view.grabWindow().save(output+"/card-"+test.id+"-"+kind+".png"),"save QML card frame failed");
            Mouse(app,view,QPointF(Width-1,Height-1));
        }
        outcomes.push_back(QJsonObject{{"case",test.id},{"expected_hover",expectedHover},{"expected_row",expectedRow},{"outcome","PASS"}});
    }
    // Replacement must clear all card-derived names and a currently visible tooltip.
    auto* tile=Find(view.rootObject(),"commerceTile_cart_291");Require(tile,"card removal tile missing");
    Mouse(app,view,tile->mapToScene(QPointF(tile->width()/2,tile->height()/2)));
    Dispatch(mode,{0xd0,2,4,0});Dispatch(mode,{0xd2,2,4,0});Dispatch(mode,{0xd1,2,4,0});
    PopulateCommerceUi(state,mode,false);Frames(app,view);
    Require(!Find(view.rootObject(),"commerceTile_cart_291") && !Find(view.rootObject(),"commerceItemTooltip")->isVisible(),
        "card-derived hover survived row removal");
    QFile file(output+"/card-name-results.json");Require(file.open(QIODevice::WriteOnly),"open card name results failed");
    file.write(QJsonDocument(QJsonObject{{"cases",outcomes},{"outcome","PASS"}}).toJson());
}
