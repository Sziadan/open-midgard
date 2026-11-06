// Production inventory acknowledgement effects rendered through the existing
// Qt commerce projection, bound item-detail window, and native inventory.
// Included by ItemColourRenderChecks.cpp.
#include <functional>
#include "ui/UIEquipWnd.h"
void CheckInventoryMutationUi(QGuiApplication& app,QQuickView& view,QtUiState* uiState,
    CGameMode& mode,UIItemWnd& inventory,QtUiStateAdapter& adapter,const QString& output)
{
    Require(uiState!=nullptr,"inventory mutation Qt state missing");
    SetRuntimeUiScalePercent(100);
    if(g_windowMgr.m_storageWnd)g_windowMgr.m_storageWnd->SetShow(0);
    ro::commerce::state.cart.visible=false;
    if(g_windowMgr.m_itemInfoWnd)g_windowMgr.DeleteWindow(g_windowMgr.m_itemInfoWnd);
    g_session.ClearInventoryItems();
    struct ActorRestore { u32 aid=g_session.m_aid,gid=g_session.m_gid;~ActorRestore(){g_session.m_aid=aid;g_session.m_gid=gid;} } actorRestore;
    g_session.m_aid=0x10101010u;g_session.m_gid=0x20202020u;
    const auto put16=[](std::vector<u8>& b,size_t at,u16 v){b[at]=u8(v);b[at+1]=u8(v>>8);};
    const auto put32=[](std::vector<u8>& b,size_t at,u32 v){for(int i=0;i<4;++i)b[at+i]=u8(v>>(8*i));};
    const auto sync=[&] {
        Require(adapter.syncGameplay(mode,RenderBackendType::LegacyDirect3D7,
            RenderBackendType::LegacyDirect3D7,0,0),"inventory mutation gameplay sync failed");
        Frames(app,view);
    };
    QJsonArray cases;int frames=0;
    // Selected 02E8 record: index 2, item 501, usable type 0, identified,
    // count 5. The row itself is explicitly 22 bytes after the four-byte header.
    std::vector<u8> list(26,0);list[0]=0xe8;list[1]=2;list[2]=26;
    put16(list,4,2);put16(list,6,501);list[8]=0;list[9]=1;put16(list,10,5);
    Dispatch(mode,list);Dispatch(mode,Equipment(0xd0,true,0));
    SelectInventoryFixtureTab(0);inventory.SetCurrentTab(0);inventory.Move(20,200);inventory.ResizeInventory(280,166);
    sync();
    const ITEM_INFO* potion=g_session.GetInventoryItemByIndex(2);
    Require(potion&&potion->GetItemId()==501&&potion->m_num==5,"selected 02E8 potion fixture missing");
    g_windowMgr.ShowItemInfoWindow(*potion,500,250,ItemInfoSource::Inventory,ItemInfoAction::ForceUpdate);
    sync();
    auto* detail=g_windowMgr.m_itemInfoWnd;UIItemInfoWnd::DisplayData detailData{};
    Require(detail&&detail->GetDisplayDataForQt(&detailData),"bound potion detail did not open");
    detail->Move(500,250);sync();
    const auto hasQuantity=[](const std::vector<std::string>& lines,int n) {
        const std::string expected="Quantity: "+std::to_string(n);
        return std::find(lines.begin(),lines.end(),expected)!=lines.end();
    };
    const auto checkDetail=[&](int n) {
        detail->RefreshBoundItem();sync();
        UIItemInfoWnd::DisplayData d{};Require(detail->GetDisplayDataForQt(&d)&&hasQuantity(d.detailLines,n),
            "bound native item detail quantity did not refresh");
        auto* viewport=Find(view.rootObject(),"itemInfoDescriptionViewport");bool found=false;
        std::function<void(QQuickItem*)> visit=[&](QQuickItem* item){
            if(item->property("text").toString()==QString("Quantity: %1").arg(n))found=true;
            for(auto* child:item->childItems())visit(child);
        };
        if(viewport)visit(viewport);
        Require(viewport&&found,"actual QML item detail Text did not render the updated quantity");
    };
    const auto rowCount=[&](unsigned index,int expected) {
        const auto rows=uiState->commerceData().value("inventoryItems").toList();
        for(const auto& value:rows){const auto row=value.toMap();if(row.value("index").toUInt()==index)
            return row.value("count").toInt()==expected;}
        return false;
    };
    const auto tileCount=[&](unsigned index,int expected) {
        auto* tile=Find(view.rootObject(),QString("commerceTile_inventory_%1").arg(index));
        if(!tile||!tile->isVisible()||tile->property("item").toMap().value("count").toInt()!=expected)return false;
        bool textFound=false;std::function<void(QQuickItem*)> visit=[&](QQuickItem* item){
            if(item->property("text").toString()==QString::number(expected))textFound=true;
            for(auto* child:item->childItems())visit(child);
        };visit(tile);return textFound;
    };
    const auto capture=[&](const QString& stem) {
        Require(view.grabWindow().save(output+"/inventory-mutation-"+stem+".qml.png"),"save inventory mutation QML frame failed");++frames;
        Hover(inventory);const QImage native=Native(inventory,0xffffffffu,view.width(),view.height());
        Require(native.save(output+"/inventory-mutation-"+stem+".native.png"),"save inventory mutation native frame failed");++frames;
    };
    const auto checkNotice=[&](const QString& expected,ro::chat::NoticeType type,bool command) {
        const auto& events=g_windowMgr.GetChatEvents();bool sourceFound=false;
        for(const auto& event:events)if(QString::fromUtf8(event.text.c_str()).contains(expected)
            &&event.source.route==ro::chat::NoticeRoute::Normal&&event.source.type==type
            &&(event.color&0x00ffffffu)==0x00ff0000u)sourceFound=true;
        Require(sourceFound,"inventory acknowledgement failure notice missing or not red in chat source");
        Require(g_windowMgr.m_chatWnd!=nullptr,"inventory acknowledgement chat window was not created");
        auto* chat=g_windowMgr.m_chatWnd;
        chat->SetState(7);chat->Resize(600,200);chat->Move(320,510);chat->SetWindowFocused(true);
        const int tabIndex=command?1:0;
        ChatTabDisplay tab{};Require(chat->GetTabDisplay(tabIndex,&tab)
            &&tab.id==tabIndex&&QString::fromUtf8(tab.label)==(command?"BT":"ST"),
            "actual ST/BT source tab metadata differs");
        const auto layout=chat->GetLayoutDisplay();
        Require(layout.tabs[tabIndex].right>layout.tabs[tabIndex].left,
            "native source tab geometry is empty");
        chat->SelectSourceTab(tabIndex);
        Require(chat->GetSourceTab()==tabIndex&&chat->GetTabDisplay(tabIndex,&tab)&&tab.active!=0,
            "native source tab selection failed");
        sync();
        const auto& sourceRows=chat->GetSourceHistory(command);
        bool nativeRow=false;
        for(const auto& row:sourceRows)if(QString::fromUtf8(row.text.c_str()).contains(expected)
            &&row.color==0x00ff0000u)nativeRow=true;
        Require(nativeRow,"selected native source history omitted red inventory failure notice");
        const auto projected=uiState->chatWindowLines();bool qmlRow=false;
        for(int i=0;i<projected.size();++i) {
            const auto item=projected[i].toMap();
            if(item.value("text").toString().contains(expected)) {
                auto* row=Find(view.rootObject(),QString("sourceChatRow%1").arg(i));
                qmlRow=row&&row->isVisible()&&row->property("text").toString().contains(expected);
                if(qmlRow) {
                    const auto pos=row->mapToScene(QPointF(0,0));
                    qmlRow=qRound(pos.y())==chat->m_y+layout.history.top+3+i*14;
                }
            }
        }
        Require(qmlRow,"actual sourceChatRow Text did not render inventory failure notice");
        auto* pane=Find(view.rootObject(),"chatHistoryPane");Require(pane,"actual chat viewport missing");
        const QRect expectedPane(chat->m_x+layout.history.left,chat->m_y+layout.history.top,
            layout.history.right-layout.history.left,layout.history.bottom-layout.history.top);
        Require(pane->isVisible()&&pane->mapRectToScene(pane->boundingRect()).toAlignedRect()==expectedPane,
            "actual source-history viewport geometry diverged from native layout");
        const QRect visible=pane->mapRectToScene(pane->boundingRect()).toAlignedRect()
            .intersected(QRect(0,0,view.width(),view.height()));
        const auto image=view.grabWindow().convertToFormat(QImage::Format_RGB32);
        int red=0;
        for(int y=visible.top();y<=visible.bottom();++y)for(int x=visible.left();x<=visible.right();++x) {
            const auto c=image.pixelColor(x,y);
            if(c.red()>150&&c.green()<90&&c.blue()<90)++red;
        }
        Require(red>20,"actual Qt notice did not paint red inside the visible chat viewport");
        Require(image.save(output+QString("/inventory-mutation-notice-%1.qml.png").arg(tabIndex)),
            "save visible acknowledgement notice failed");++frames;
    };
    Require(hasQuantity(detailData.detailLines,5),"initial bound detail quantity is not five");
    auto* inventoryPanel=Find(view.rootObject(),"commerce_inventory");
    Require(inventoryPanel&&qRound(inventoryPanel->x())==20
        &&qRound(inventoryPanel->y())==200,"QML inventory geometry did not follow native placement");
    auto* detailWindow=Find(view.rootObject(),"itemInfoWindow");
    Require(detailWindow&&detailWindow->isVisible()&&!QRectF(20,200,280,166).intersects(
        QRectF(detailWindow->x(),detailWindow->y(),detailWindow->width(),detailWindow->height())),
        "item detail overlaps the inventory");

    auto pickupFailure=std::vector<u8>(29,0);pickupFailure[0]=0xd4;pickupFailure[1]=2;pickupFailure[22]=2;
    mode.m_pickupReqItemNaidList={0x12345678};mode.m_lastPickupRequestTick=77;
    g_windowMgr.EnsureChatWindowVisible();
    Require(g_windowMgr.m_chatWnd!=nullptr,"pickup acknowledgement main chat window was not created");
    g_windowMgr.m_chatWnd->SetState(7);
    Dispatch(mode,pickupFailure);sync();checkNotice(QStringLiteral("You cannot carry more items because you are overweight."),ro::chat::NoticeType::None,false);
    cases.push_back(QJsonObject{{"case","02D4 failure renders the red overweight notice in actual Qt chat"},{"outcome","PASS"}});

    // Same-index expanded pickup adds two while retaining the original row metadata.
    auto pickup=std::vector<u8>(29,0);pickup[0]=0xd4;pickup[1]=2;
    put16(pickup,2,2);put16(pickup,4,2);put16(pickup,6,501);pickup[8]=1;pickup[21]=0;put32(pickup,23,0xAABBCCDD);put16(pickup,27,2);
    Dispatch(mode,pickup);sync();potion=g_session.GetInventoryItemByIndex(2);
    Require(potion&&potion->m_num==7&&potion->GetItemId()==501&&potion->m_itemType==0&&potion->m_isIdentified==1,
        "same-index pickup count or existing metadata changed incorrectly");
    Require(rowCount(2,7)&&tileCount(2,7),"pickup quantity did not reach Qt model and actual tile text");
    checkDetail(7);capture("pickup-seven");
    cases.push_back(QJsonObject{{"case","02D4 adds two to selected 02E8 stack and refreshes actual Qt/native quantity"},{"outcome","PASS"}});

    // Basic use remainder is authoritative, including an increase.
    std::vector<u8> use={0xa8,0,2,0,9,0,1};Dispatch(mode,use);sync();
    Require(g_session.GetInventoryItemByIndex(2)&&g_session.GetInventoryItemByIndex(2)->m_num==9
        &&rowCount(2,9)&&tileCount(2,9),"00A8 authoritative count nine failed to update Qt inventory");
    checkDetail(9);capture("basic-use-nine");
    cases.push_back(QJsonObject{{"case","00A8 authoritative remaining count updates Qt tile and bound detail"},{"outcome","PASS"}});

    // Extended local ack applies its remaining count; remote actor does not.
    auto extended=std::vector<u8>(13,0);extended[0]=0xc8;extended[1]=1;put16(extended,2,2);put16(extended,4,501);
    put32(extended,6,g_session.m_aid);put16(extended,10,3);extended[12]=1;Dispatch(mode,extended);sync();
    Require(g_session.GetInventoryItemByIndex(2)->m_num==3&&rowCount(2,3)&&tileCount(2,3),"local 01C8 count three failed");
    checkDetail(3);capture("extended-use-three");
    put16(extended,10,0);put32(extended,6,g_session.m_aid^0x01010101u);Dispatch(mode,extended);sync();
    Require(g_session.GetInventoryItemByIndex(2)->m_num==3&&rowCount(2,3),"remote 01C8 changed local inventory count");
    cases.push_back(QJsonObject{{"case","01C8 local count three applies and remote actor count zero is isolated"},{"outcome","PASS"}});

    use[4]=1;use[5]=0;use[6]=0;Dispatch(mode,use);sync();
    Require(g_session.GetInventoryItemByIndex(2)->m_num==3&&rowCount(2,3),"failed 00A8 changed authoritative inventory count");
    checkDetail(3);cases.push_back(QJsonObject{{"case","failed 00A8 leaves quantity and detail unchanged"},{"outcome","PASS"}});
    auto drop=std::vector<u8>{0xaf,0,2,0,0,0};Dispatch(mode,drop);sync();
    Require(g_session.GetInventoryItemByIndex(2)&&g_session.GetInventoryItemByIndex(2)->m_num==3&&rowCount(2,3),"00AF zero acknowledgement deleted stack");
    put16(drop,4,1);Dispatch(mode,drop);sync();
    Require(g_session.GetInventoryItemByIndex(2)&&g_session.GetInventoryItemByIndex(2)->m_num==2,"positive 00AF did not decrement stack");
    put16(drop,4,2);Dispatch(mode,drop);sync();
    Require(!g_session.GetInventoryItemByIndex(2)&&!Find(view.rootObject(),"commerceTile_inventory_2"),"full 00AF removal left inventory tile");
    detail->RefreshBoundItem();sync();
    Require(!detail->m_show&&!Find(view.rootObject(),"itemInfoWindow")->isVisible(),"full removal did not hide bound native and Qt item detail");
    Require(view.grabWindow().save(output+"/inventory-mutation-stack-removed.qml.png"),"save removed-stack QML frame failed");++frames;
    const int frameWidth=view.width(),frameHeight=view.height();
    ArgbDibSurface emptySurface;Require(emptySurface.EnsureSize(frameWidth,frameHeight),"removed-stack native surface failed");
    std::fill_n(emptySurface.GetPixels(),frameWidth*frameHeight,0xff181b20u);inventory.DrawNativeToHdc(emptySurface.GetDC());
    const QImage emptyNative=QImage(reinterpret_cast<uchar*>(emptySurface.GetPixels()),frameWidth,frameHeight,frameWidth*4,QImage::Format_RGB32).copy();
    Require(emptyNative.save(output+"/inventory-mutation-stack-removed.native.png"),"save removed-stack native frame failed");++frames;
    cases.push_back(QJsonObject{{"case","00AF zero preserves, positive count decrements and full removal clears bound detail"},{"outcome","PASS"}});

    // Equipment acknowledgement projection keeps the worn item in equipment rows,
    // while source inventory continues to contain the row and wear flag.
    SelectInventoryFixtureTab(1);inventory.SetCurrentTab(1);sync();
    auto* equipment=static_cast<UIEquipWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_EQUIPWND));
    Require(equipment,"local equipment window did not open");equipment->Move(640,100);sync();
    auto equipFail=std::vector<u8>{0xaa,0,0x23,1,2,0,2};Dispatch(mode,equipFail);sync();
    auto* knife=g_session.GetInventoryItemByIndex(291);Require(knife&&knife->m_wearLocation==0,"failed equip ack changed wear state");
    g_windowMgr.EnsureChatWindowVisible();sync();checkNotice(QStringLiteral("You can't put this item on."),ro::chat::NoticeType::Equip,true);
    auto equipOk=equipFail;equipOk[6]=1;Dispatch(mode,equipOk);sync();
    knife=g_session.GetInventoryItemByIndex(291);Require(knife&&knife->m_wearLocation==2,"successful equip ack did not set wear mask");
    const auto wearRow=[&]()->QVariantMap {for(const auto& v:uiState->commerceData().value("inventoryItems").toList())if(v.toMap().value("index").toUInt()==291)return v.toMap();return {};};
    Require(wearRow().isEmpty()&&!Find(view.rootObject(),"commerceTile_inventory_291"),
        "worn equipment remained in the actual Qt bag");
    UIItemWnd::DisplayData nativeBag{};
    Require(inventory.GetDisplayDataForQt(&nativeBag),"native bag projection missing after equip");
    for(const auto& slot:nativeBag.displaySlots)Require(!slot.occupied||slot.itemId!=1201,
        "worn equipment remained in the native bag");
    bool equippedName=false;const auto equipSlots=uiState->equipData().value("slots").toList();
    for(int i=0;i<equipSlots.size();++i) {
        const auto slot=equipSlots[i].toMap();
        if(slot.value("occupied").toBool()&&slot.value("itemId").toUInt()==1201) {
            auto* name=Find(view.rootObject(),QString("equipmentName%1").arg(i));
            equippedName=name&&name->isVisible()&&name->property("text").toString().contains("Knife");
        }
    }
    Require(equippedName,"actual equipment pane did not show the acknowledged Knife");
    Require(view.grabWindow().save(output+"/inventory-mutation-equipped.qml.png"),"save equipped frame failed");++frames;
    auto off=std::vector<u8>{0xac,0,0x23,1,2,0,0};Dispatch(mode,off);sync();
    Require(g_session.GetInventoryItemByIndex(291)->m_wearLocation==2,"failed unequip ack cleared wear state");
    off[6]=1;Dispatch(mode,off);sync();Require(g_session.GetInventoryItemByIndex(291)->m_wearLocation==0,"successful unequip ack retained wear state");
    Require(wearRow().value("equipped").toBool()==false,"unequipped Qt row did not clear equipped state");
    Require(Find(view.rootObject(),"commerceTile_inventory_291")&&wearRow().value("id").toUInt()==1201,
        "unequipped item did not return to the actual Qt bag");
    for(const auto& v:uiState->equipData().value("slots").toList())
        Require(!v.toMap().value("occupied").toBool()||v.toMap().value("itemId").toUInt()!=1201,
            "unequipped Knife remained in equipment slots");
    Require(view.grabWindow().save(output+"/inventory-mutation-equipment.qml.png"),"save equipment mutation frame failed");++frames;
    cases.push_back(QJsonObject{{"case","00AA/00AC acknowledgements keep native inventory and Qt equipped projection synchronized"},{"outcome","PASS"}});
    QFile result(output+"/inventory-mutation-render-results.json");Require(result.open(QIODevice::WriteOnly),"open inventory mutation render result failed");
    result.write(QJsonDocument(QJsonObject{{"cases",cases},{"generated_frames",frames},{"qml_checks","PASS"},{"errors",0},{"outcome","PASS"}}).toJson(QJsonDocument::Indented));
}
