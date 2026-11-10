{
    Require(g_windowMgr.m_petEggListWnd==nullptr,"pet egg render fixture needs an unused selector");
    const std::vector<UIWindow*> existingRoots(g_windowMgr.m_children.begin(),g_windowMgr.m_children.end());
    struct RestorePetEggRender {
        std::vector<UIWindow*> roots;
        std::list<ITEM_INFO> inventory=g_session.GetInventoryItems();
        ro::pet::Status pet=g_session.m_petStatus;
        int scale=GetConfiguredUiScalePercent();
        ~RestorePetEggRender(){
            while(true){auto it=std::find_if(g_windowMgr.m_children.begin(),g_windowMgr.m_children.end(),
                [&](UIWindow* p){return std::find(roots.begin(),roots.end(),p)==roots.end();});
                if(it==g_windowMgr.m_children.end())break;g_windowMgr.DeleteWindow(*it);}
            g_session.ClearInventoryItems();for(const auto& item:inventory)g_session.SetInventoryItem(item);
            g_session.m_petStatus=std::move(pet);SetRuntimeUiScalePercent(scale);
        }
    } restore{existingRoots};
    SetRuntimeUiScalePercent(100);
    g_session.m_petStatus.job=1002;g_session.m_petStatus.eggIndex=-1;
    g_session.ClearInventoryItems();
    for(unsigned int i=20;i<=25;++i){
        ITEM_INFO item{};item.SetItemId(9000+i);item.m_itemIndex=i;item.m_num=static_cast<int>(i-19);
        item.m_isIdentified=1;item.m_isDamaged=0;item.m_slot[0]=4000+i;
        g_session.SetInventoryItem(item);
    }
    CGameModePacketRouter router;RegisterDefaultGameModePacketHandlers(router);
    const auto makePacket=[](std::initializer_list<u16> indices){
        std::vector<u8> bytes(4+indices.size()*2);bytes[0]=0xa6;bytes[1]=1;
        bytes[2]=static_cast<u8>(bytes.size());bytes[3]=static_cast<u8>(bytes.size()>>8);
        std::size_t at=4;for(const auto index:indices){bytes[at++]=static_cast<u8>(index);bytes[at++]=static_cast<u8>(index>>8);}return bytes;
    };
    const auto dispatch=[&](const std::vector<u8>& bytes){
        Require(router.Dispatch(mode,{0x01a6,static_cast<u16>(bytes.size()),bytes.data()}),
            "production pet egg list packet did not dispatch");
    };
    dispatch(makePacket({20,21,22,23,24,25}));
    auto* egg=g_windowMgr.m_petEggListWnd;
    const auto client=g_windowMgr.GetChatClientRect();
    Require(egg && egg->m_w==200 && egg->m_h==200 && egg->m_x==200+(client.right-640)/2 && egg->m_y==200*client.bottom/480
        && egg->m_children.size()==3 && egg->GetItems().size()==6 && egg->GetVisibleRows().size()==4,
        "source pet egg factory geometry, children or initial visible rows differ");
    const auto initialSnapshot=egg->GetItems()[0];
    Require(egg->GetTitle()=="Pet Egg List" && egg->GetFramePieces().size()==27,"egg source title or six frame assets missing");
    auto* sourceList=Find(view.rootObject(),"sourcePetEggList");
    const auto captureEgg=[&](const QString& stem){
        state->setUiScale(GetConfiguredUiScaleFactor());
        Require(adapter.syncGameplay(mode,RenderBackendType::LegacyDirect3D7,
            RenderBackendType::LegacyDirect3D7,-1,-1),"pet egg gameplay projection failed");
        Frames(app,view);sourceList=Find(view.rootObject(),"sourcePetEggList");
        Require(sourceList && sourceList->isVisible(),"actual Qt pet egg selector missing");
        const QVariantMap data=state->chatWindowUi().value("petEgg").toMap();
        const auto rows=data.value("rows").toList();
        Require(data.value("visible").toBool() && data.value("width").toInt()==egg->m_w
            && data.value("height").toInt()==egg->m_h && data.value("current").toInt()==egg->GetCurrentItem()
            && data.value("offset").toInt()==egg->GetOffset() && rows.size()==static_cast<int>(egg->GetVisibleRows().size()),
            "native/passive Qt pet egg state or visible row count differs");
        const double scale=GetConfiguredUiScaleFactor();
        const auto bounds=sourceList->mapRectToScene(sourceList->boundingRect());
        Require(std::abs(bounds.left()-egg->m_x*scale)<0.01 && std::abs(bounds.top()-egg->m_y*scale)<0.01
            && std::abs(bounds.width()-egg->m_w*scale)<0.01 && std::abs(bounds.height()-egg->m_h*scale)<0.01,
            "native/passive Qt pet egg geometry differs");
        const auto nativeRows=egg->GetVisibleRows();
        for(int i=0;i<rows.size();++i){
            const auto& nativeRow=nativeRows[static_cast<std::size_t>(i)];
            const auto projected=rows[i].toMap();
            Require(projected.value("index").toInt()==nativeRow.index
                && projected.value("itemIndex").toUInt()==nativeRow.itemIndex
                && projected.value("selected").toBool()==nativeRow.selected
                && projected.value("text").toString()==QString::fromUtf8(nativeRow.text.c_str()),
                "native/passive Qt pet egg row data differs");
        }
        ArgbDibSurface dib;Require(dib.EnsureSize(view.width(),view.height()),"pet egg native DIB allocation failed");
        std::fill_n(dib.GetPixels(),view.width()*view.height(),0xff181b20u);
        g_windowMgr.DrawVisibleWindowsToHdc(dib.GetDC(),true);
        auto native=QImage(reinterpret_cast<uchar*>(dib.GetPixels()),view.width(),view.height(),
            view.width()*4,QImage::Format_RGB32).copy();
        if(GetConfiguredUiScalePercent()!=100)native=native.scaled(qRound(native.width()*scale),
            qRound(native.height()*scale),Qt::IgnoreAspectRatio,Qt::FastTransformation).copy(0,0,view.width(),view.height());
        const auto qml=view.grabWindow().convertToFormat(QImage::Format_RGB32);
        Require(native.save(output+"/pet-egg-"+stem+".native.png")
            && qml.save(output+"/pet-egg-"+stem+".qml.png"),"pet egg paired frame save failed");
        const auto point=[&](int x,int y){return QPoint(UiScaleLogicalToRawCoordinate(x),UiScaleLogicalToRawCoordinate(y));};
        const auto same=[&](QPoint p,const QColor& color,const char* name){
            Require(native.rect().contains(p)&&qml.rect().contains(p),"pet egg pixel sample outside viewport");
            if(native.pixelColor(p)!=color||qml.pixelColor(p)!=color)
                throw std::runtime_error((std::string("pet egg ")+name+" native/QML pixel mismatch").c_str());
        };
        same(point(egg->m_x+1,egg->m_y+1),QColor("#d24240"),"source title-left art");
        same(point(egg->m_x+100,egg->m_y+25),QColor("#f5f2ea"),"body fill");
        if(egg->GetCurrentItem()>=egg->GetOffset()&&egg->GetCurrentItem()<egg->GetOffset()+4){
            const int local=egg->GetCurrentItem()-egg->GetOffset();
            same(point(egg->m_x+170,egg->m_y+34+32*local+20),QColor("#d7dff0"),"selected-row fill");
        }
        ++frames;
    };

    captureEgg("initial");
    Require(egg->GetCurItem(4,34)==0 && egg->GetCurItem(186,161)==3
        && egg->GetCurItem(3,34)==-1 && egg->GetCurItem(187,34)==-1
        && egg->GetCurItem(4,33)==-1 && egg->GetCurItem(4,162)==-1,
        "source pet egg row hit rectangle boundaries differ");
    click(egg->m_x+12,egg->m_y+34+32+5);
    Require(egg->GetCurrentItem()==1,"native manager click did not select pet egg row");
    captureEgg("selected");
    const auto* okButton=egg->GetButtons()[0];
    pointer(WM_LBUTTONDOWN,okButton->m_x+4,okButton->m_y+4);
    pointer(WM_CANCELMODE,0,0);
    pointer(WM_LBUTTONUP,okButton->m_x+4,okButton->m_y+4);
    Require(g_windowMgr.m_petEggListWnd==egg && !g_windowMgr.m_captureWindow && !GetCapture(),
        "cancelled native egg OK press activated or retained capture");
    const int oldX=egg->m_x,oldY=egg->m_y;
    pointer(WM_LBUTTONDOWN,oldX+8,oldY+5);
    pointer(WM_MOUSEMOVE,oldX+38,oldY+25);
    pointer(WM_LBUTTONUP,oldX+38,oldY+25);
    Require(egg->m_x==oldX+30 && egg->m_y==oldY+20
        && egg->GetButtons()[0]->m_x==egg->m_x+108 && egg->GetScrollControl()->m_x==egg->m_x+185,
        "native egg header drag lost source child positions");

    click(egg->m_x+191,egg->m_y+34+6);
    click(egg->m_x+191,egg->m_y+34+138);
    Require(egg->GetOffset()==1 && egg->GetCurrentItem()==1,"actual scrollbar arrow click did not scroll source list");
    captureEgg("scrolled");

    auto replacement=*g_session.GetInventoryItemByIndex(20);replacement.SetItemId(9999);replacement.m_num=88;
    g_session.SetInventoryItem(replacement);
    dispatch(makePacket({20,25}));
    Require(g_windowMgr.m_petEggListWnd==egg && egg->GetItems().size()==8
        && egg->GetCurrentItem()==1 && egg->GetOffset()==1
        && egg->GetItems()[0].GetItemId()==initialSnapshot.GetItemId()
        && egg->GetItems()[6].GetItemId()==9999 && egg->GetItems()[6].m_num==88,
        "source repeated egg packet cleared state or failed to append independent item snapshots");
    replacement.m_num=99;g_session.SetInventoryItem(replacement);
    Require(egg->GetItems()[6].m_num==88,"later inventory mutation changed stored egg snapshot");
    captureEgg("appended");SetRuntimeUiScalePercent(125);captureEgg("125");SetRuntimeUiScalePercent(100);
    state->setUiScale(1.0);adapter.syncChat();Frames(app,view);

    // Cancel must remain passive through an outside release, then close only
    // when the actual source button is clicked.
    const auto cancel=egg->GetButtons()[1];
    pointer(WM_LBUTTONDOWN,cancel->m_x+cancel->m_w/2,cancel->m_y+cancel->m_h/2);
    pointer(WM_LBUTTONUP,-20,-20);
    Require(g_windowMgr.m_petEggListWnd==egg,"outside release activated the pet egg Cancel button");
    click(cancel->m_x+cancel->m_w/2,cancel->m_y+cancel->m_h/2);
    Require(!g_windowMgr.m_petEggListWnd,"native source Cancel button did not close pet egg selector");

    SetRuntimeUiScalePercent(125);state->setUiScale(GetConfiguredUiScaleFactor());
    dispatch(makePacket({20,21,22,23}));egg=g_windowMgr.m_petEggListWnd;
    adapter.syncChat();Frames(app,view);
    click(egg->m_x+12,egg->m_y+34+5);
    Require(egg->GetCurrentItem()==0,"native manager click did not select egg for OK");
    click(egg->GetButtons()[0]->m_x+egg->GetButtons()[0]->m_w/2,
        egg->GetButtons()[0]->m_y+egg->GetButtons()[0]->m_h/2);
    const auto* selectedItem=g_session.GetInventoryItemByIndex(20);
    Require(!g_windowMgr.m_petEggListWnd && g_session.m_petStatus.eggIndex==20
        && selectedItem && selectedItem->m_isDamaged==1,
        "native source OK did not select, mark and close on the chosen snapshot");
    Require(!g_windowMgr.m_captureWindow && !GetCapture(),"egg OK retained logical/native capture after owner deletion");
    SetRuntimeUiScalePercent(100);state->setUiScale(1.0);
    adapter.syncGameplay(mode,RenderBackendType::LegacyDirect3D7,RenderBackendType::LegacyDirect3D7,-1,-1);Frames(app,view);
    Require(!Find(view.rootObject(),"sourcePetEggList")->isVisible(),"closed pet egg selector remains in Qt view");
}
