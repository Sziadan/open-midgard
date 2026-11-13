// Slotted-card item detail routing, presentation, and independent placement.
// Included by ItemColourRenderChecks.cpp.
#include <QProcess>
#include <QProcessEnvironment>
#include <QJsonParseError>
#include <QDateTime>
#include <QFileInfo>
#include <QMouseEvent>

namespace {
void ItemCardDetailRequire(bool ok,const char* message)
{
    if(!ok)throw std::runtime_error(message);
}

void ItemCardDetailWriteResult(const QString& output,const QString& stage,int count,const QString& outcome)
{
    const auto main=g_windowMgr.GetItemDetailWindowInfo(ItemDetailKind::Main);
    const auto card=g_windowMgr.GetItemDetailWindowInfo(ItemDetailKind::SlottedCard);
    QFile file(output+"/item-card-detail.json");
    ItemCardDetailRequire(file.open(QIODevice::WriteOnly),"open card detail child result failed");
    file.write(QJsonDocument(QJsonObject{{"pid",qint64(QCoreApplication::applicationPid())},{"stage",stage},
        {"case_count",count},{"main_x",main.x},{"main_y",main.y},{"main_width",main.width},{"main_height",main.height},
        {"card_x",card.x},{"card_y",card.y},{"card_width",card.width},{"card_height",card.height},
        {"outcome",outcome}}).toJson(QJsonDocument::Compact));
}

void ItemCardDetailWriteSeed(const QString& settings,const QString& stage)
{
    ItemCardDetailRequire(QDir().mkpath(settings),"create card detail settings directory failed");
    QFile file(settings+"/open-midgard.ini");
    ItemCardDetailRequire(file.open(QIODevice::WriteOnly|QIODevice::Truncate),"write private card detail seed failed");
    QByteArray text;
    if(stage=="warm-valid")text="[UIWindows]\nCardInfoWndX=15\nCardInfoWndY=55\nCardInfoWndWidth=280\nCardInfoWndHeight=120\n";
    else if(stage=="warm-invalid")text="[UIWindows]\nCardInfoWndX=1001\nCardInfoWndY=55\nCardInfoWndWidth=280\nCardInfoWndHeight=149\n";
    else if(stage=="warm-partial")text="[UIWindows]\nCardInfoWndX=15\nCardInfoWndY=55\n";
    file.write(text);
}

ITEM_INFO ItemCardDetailParentItem(CGameMode& mode,bool identified,const std::array<unsigned int,4>& cards,
    unsigned int itemId=1202)
{
    auto packet=Equipment(0xd0,identified,0);
    packet[6]=u8(itemId);packet[7]=u8(itemId>>8);
    for(size_t i=0;i<cards.size();++i) {
        packet[16+i*2]=u8(cards[i]);packet[17+i*2]=u8(cards[i]>>8);
    }
    Dispatch(mode,packet);
    const ITEM_INFO* row=g_session.GetInventoryItemByIndex(3);
    ItemCardDetailRequire(row&&row->GetItemId()==itemId,"slotted card parent fixture was not decoded");
    ITEM_INFO item=*row;item.m_isIdentified=identified?1:0;
    std::copy(cards.begin(),cards.end(),std::begin(item.m_slot));
    return item;
}

UIItemInfoWnd& ItemCardDetailOpenParent(QGuiApplication& app,QQuickView& view,QtUiStateAdapter& adapter,
    CGameMode& mode,const ITEM_INFO& item)
{
    g_windowMgr.ShowItemInfoWindow(item,1050,640,ItemInfoSource::Snapshot,ItemInfoAction::ForceUpdate);
    ItemCardDetailRequire(g_windowMgr.m_itemInfoWnd&&g_windowMgr.m_itemInfoWnd->GetKind()==ItemDetailKind::Main,
        "main item detail parent did not open");
    ItemCardDetailRequire(adapter.syncGameplay(mode,RenderBackendType::LegacyDirect3D7,
        RenderBackendType::LegacyDirect3D7,0,0),"main item detail parent Qt sync failed");Frames(app,view);
    return *g_windowMgr.m_itemInfoWnd;
}

void ItemCardDetailNativeRightClick(int x,int y)
{
    g_windowMgr.OnRBtnDown(x,y);g_windowMgr.OnRBtnUp(x,y);
}

void ItemCardDetailQtRightClick(QGuiApplication& app,QQuickView& view,QPointF point)
{
    Mouse(app,view,point);
    const QPoint pixel=point.toPoint();const QPoint global=view.mapToGlobal(pixel);
    QMouseEvent press(QEvent::MouseButtonPress,point,global,Qt::RightButton,Qt::RightButton,Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease,point,global,Qt::RightButton,Qt::NoButton,Qt::NoModifier);
    QCoreApplication::sendEvent(&view,&press);QCoreApplication::sendEvent(&view,&release);Frames(app,view);
}

void ItemCardDetailCheckProjection(QQuickView& view,QtUiState* uiState,const UIItemInfoWnd& card,
    unsigned int expectedId)
{
    ItemCardDetailRequire(card.GetKind()==ItemDetailKind::SlottedCard,"card detail window has the wrong detail kind");
    UIItemInfoWnd::DisplayData data{};
    ItemCardDetailRequire(card.GetDisplayDataForQt(&data)&&data.itemId==expectedId,"card detail native snapshot has the wrong item ID");
    ItemCardDetailRequire(!data.name.empty(),"card detail native snapshot has no display name");
    auto* frame=Find(view.rootObject(),"cardInfoWindow");
    auto* name=Find(view.rootObject(),"cardInfoName");
    ItemCardDetailRequire(frame&&frame->isVisible()&&qRound(frame->x())==card.m_x&&qRound(frame->y())==card.m_y
        &&qRound(frame->width())==card.m_w&&qRound(frame->height())==card.m_h,
        "actual QML card detail frame differs from the native card window");
    ItemCardDetailRequire(name&&name->isVisible()&&name->property("text").toString()==QString::fromUtf8(data.name.c_str()),
        "actual QML card detail name differs from native snapshot");
    const QVariantMap projected=uiState->cardInfoData();
    ItemCardDetailRequire(projected.value("visible").toBool()&&projected.value("itemId").toUInt()==expectedId
        &&projected.value("x").toInt()==card.m_x&&projected.value("y").toInt()==card.m_y
        &&projected.value("width").toInt()==card.m_w&&projected.value("height").toInt()==card.m_h,
        "Qt card detail projection differs from manager-owned card window");
}

void ItemCardDetailSync(QGuiApplication& app,QQuickView& view,QtUiStateAdapter& adapter,CGameMode& mode)
{
    ItemCardDetailRequire(adapter.syncGameplay(mode,RenderBackendType::LegacyDirect3D7,
        RenderBackendType::LegacyDirect3D7,0,0),"card detail Qt sync failed");Frames(app,view);
}

void ItemCardDetailClickCell(int index,int parentHeight)
{
    const int localX=15+25*index,localY=parentHeight-13;
    ItemCardDetailNativeRightClick(g_windowMgr.m_itemInfoWnd->m_x+localX,g_windowMgr.m_itemInfoWnd->m_y+localY);
}

QString ItemCardDetailTooltipText(QQuickItem* item)
{
    if(!item)return {};
    if(item->metaObject()->indexOfProperty("text")>=0) {
        const QString text=item->property("text").toString();
        if(!text.isEmpty())return text;
    }
    for(auto* child:item->childItems()) {
        const QString text=ItemCardDetailTooltipText(child);
        if(!text.isEmpty())return text;
    }
    return {};
}

QString ItemCardDetailCheckHover(QGuiApplication& app,QQuickView& view,QtUiState* uiState,
    QtUiStateAdapter& adapter,CGameMode& mode,UIItemInfoWnd& parent,int index,bool expected,const char* message)
{
    const int x=parent.m_x+15+25*index,y=parent.m_y+parent.m_h-13;
    parent.OnMouseMove(x,y);
    ItemCardDetailSync(app,view,adapter,mode);
    UIItemInfoWnd::DisplayData native{};
    ItemCardDetailRequire(parent.GetDisplayDataForQt(&native),"main detail hover projection unavailable");
    const QString expectedText=QString::fromUtf8(native.slotTooltip.c_str());
    const QVariantMap itemData=uiState->itemInfoData();
    ItemCardDetailRequire(expected ? !expectedText.isEmpty() : expectedText.isEmpty(),message);
    ItemCardDetailRequire(itemData.value("slotTooltip").toString()==expectedText,
        "Qt main item detail tooltip role differs from native hover projection");
    ItemCardDetailRequire(itemData.value("slotTooltipX").toInt()==native.slotTooltipAnchor.left
        &&itemData.value("slotTooltipY").toInt()==native.slotTooltipAnchor.top,
        "Qt main item detail tooltip anchor differs from native source anchor");
    auto* tooltip=Find(view.rootObject(),"itemInfoSlotTooltip");
    ItemCardDetailRequire(tooltip&&tooltip->isVisible()==expected,"actual QML main card tooltip visibility differs from native hover");
    if(expected) {
        ItemCardDetailRequire(ItemCardDetailTooltipText(tooltip)==expectedText,
            "actual QML main card tooltip text differs from native source text");
        const QRectF rect=tooltip->mapRectToScene(tooltip->boundingRect());
        ItemCardDetailRequire(rect.left()>=-4&&rect.top()>=-4&&rect.right()<=view.width()+4&&rect.bottom()<=view.height()+4,
            "actual QML card tooltip escaped the viewport clamp");
    }
    return expectedText;
}
}

void CheckItemCardDetailChild(QGuiApplication& app,QQuickView& view,QtUiState* uiState,CGameMode& mode,
    UIItemWnd& inventory,QtUiStateAdapter& adapter,const QString& output,const QString& stage)
{
    ItemCardDetailRequire(uiState,"card detail Qt state missing");
    const QString actual=QDir::cleanPath(QFileInfo(QString::fromStdString(GetOpenMidgardIniPath().string())).absolutePath());
    const QString expected=QDir::cleanPath(QFileInfo(output).dir().absoluteFilePath("settings"));
    ItemCardDetailRequire(actual==expected,"card detail child requires private sibling settings");
    if(g_windowMgr.m_storageWnd)g_windowMgr.m_storageWnd->SetShow(0);
    if(g_windowMgr.m_itemInfoWnd)g_windowMgr.DeleteWindow(g_windowMgr.m_itemInfoWnd);
    if(g_windowMgr.m_cardInfoWnd)g_windowMgr.DeleteWindow(g_windowMgr.m_cardInfoWnd);
    inventory.SetShow(0);ro::commerce::state.cart.visible=false;
    std::array<unsigned int,4> cards={4001,4002,4003,4004};
    ITEM_INFO parentItem=ItemCardDetailParentItem(mode,true,cards);
    auto& parent=ItemCardDetailOpenParent(app,view,adapter,mode,parentItem);
    int count=0;
    const auto expectCard=[&](unsigned int id) {
        ItemCardDetailRequire(g_windowMgr.m_cardInfoWnd,"right-clicked card did not create its managed detail window");
        ItemCardDetailSync(app,view,adapter,mode);
        ItemCardDetailCheckProjection(view,uiState,*g_windowMgr.m_cardInfoWnd,id);++count;
        UIItemInfoWnd::DisplayData mainData{};
        ItemCardDetailRequire(g_windowMgr.m_itemInfoWnd==&parent&&parent.GetDisplayDataForQt(&mainData)
            &&mainData.itemId==1202,"opening a card changed or replaced its main parent snapshot");
        const ITEM_INFO* live=g_session.GetInventoryItemByIndex(3);
        ItemCardDetailRequire(live&&live->GetItemId()==1202,"card selection changed the live inventory row");
    };
    const auto noCard=[&](const char* message) {
        ItemCardDetailRequire(g_windowMgr.m_cardInfoWnd==nullptr,message);++count;
    };
    const auto resetParent=[&](bool identified,const std::array<unsigned int,4>& values) {
        parentItem=ItemCardDetailParentItem(mode,identified,values);
        parent.SetItemInfo(parentItem,1000,600,ItemInfoSource::Snapshot);
        ItemCardDetailSync(app,view,adapter,mode);
        ItemCardDetailRequire(g_windowMgr.m_cardInfoWnd==nullptr,"test parent reset left a previous card window open");
    };

    if(stage=="cold-default"||stage=="warm-valid"||stage=="warm-invalid"||stage=="warm-partial") {
        ItemCardDetailClickCell(0,parent.m_h);
        expectCard(4001);
        const auto info=g_windowMgr.GetItemDetailWindowInfo(ItemDetailKind::SlottedCard);
        if(stage=="cold-default")ItemCardDetailRequire(g_windowMgr.m_cardInfoWnd->m_x==280&&g_windowMgr.m_cardInfoWnd->m_y==278
            &&info.x==280&&info.y==278&&info.width==280&&info.height==120,"cold card detail placement did not use independent defaults");
        else if(stage=="warm-valid")ItemCardDetailRequire(g_windowMgr.m_cardInfoWnd->m_x==15&&g_windowMgr.m_cardInfoWnd->m_y==55,
            "valid card-specific placement was not restored");
        else ItemCardDetailRequire(g_windowMgr.m_cardInfoWnd->m_x==600&&g_windowMgr.m_cardInfoWnd->m_y==556,
            "invalid or partial card placement did not use scaled card defaults");
        const int expectedMain=(stage=="cold-default")?280:600;
        const int expectedMainY=(stage=="cold-default")?126:252;
        ItemCardDetailRequire(g_windowMgr.m_itemInfoWnd==&parent&&parent.m_x==expectedMain&&parent.m_y==expectedMainY,
            "opening card detail changed the independent main detail window");
        if(stage=="cold-default")ItemCardDetailRequire(view.grabWindow().save(output+"/item-card-detail-cold.qml.png"),"save cold card detail frame failed");
    } else if(stage=="source-hits") {
        // Prefix scanning stops at the first empty slot; strict rectangle edges
        // and the row's strict top edge do not select a card.
        resetParent(true,{4001,0,4002,4003});
        ItemCardDetailNativeRightClick(parent.m_x+65,parent.m_y+136);noCard("card after first empty slot remained hittable");
        ItemCardDetailNativeRightClick(parent.m_x+15,parent.m_y+136);expectCard(4001);
        ItemCardDetailNativeRightClick(parent.m_x+15,parent.m_y+136);noCard("repeat selection did not toggle only the card detail closed");
        ItemCardDetailNativeRightClick(parent.m_x+15,parent.m_y+136);expectCard(4001);
        g_windowMgr.DeleteWindow(g_windowMgr.m_cardInfoWnd);
        resetParent(true,{4001,4002,4003,4004});
        ItemCardDetailNativeRightClick(parent.m_x+40,parent.m_y+136);expectCard(4002);
        ItemCardDetailRequire(g_windowMgr.m_itemInfoWnd==&parent&&parent.HasItem(),"card selection changed the main detail parent");
        g_windowMgr.DeleteWindow(g_windowMgr.m_cardInfoWnd);
        ItemCardDetailNativeRightClick(parent.m_x+3,parent.m_y+136);noCard("left card row edge was not strict");
        ItemCardDetailNativeRightClick(parent.m_x+28,parent.m_y+136);noCard("shared card cell edge was not strict");
        ItemCardDetailNativeRightClick(parent.m_x+15,parent.m_y+123);noCard("top card row edge was not strict");
        ItemCardDetailNativeRightClick(parent.m_x+15,parent.m_y+124);expectCard(4001);
    } else if(stage=="metadata-gates") {
        resetParent(false,{4001,4002,0,0});
        ItemCardDetailRequire(parent.m_h==120,"unidentified card parent expanded unexpectedly");
        ItemCardDetailClickCell(0,parent.m_h);expectCard(4001);
        g_windowMgr.DeleteWindow(g_windowMgr.m_cardInfoWnd);
        resetParent(false,{255,4002,0,0});
        ItemCardDetailClickCell(0,parent.m_h);expectCard(255);
        ITEM_INFO changedParent=parentItem;changedParent.m_slot[0]=254;
        parent.SetItemInfo(changedParent,0,0,ItemInfoSource::Snapshot);ItemCardDetailSync(app,view,adapter,mode);
        UIItemInfoWnd::DisplayData retainedCard{};
        ItemCardDetailRequire(g_windowMgr.m_cardInfoWnd->GetDisplayDataForQt(&retainedCard)&&retainedCard.itemId==255,
            "card detail snapshot changed when its parent item changed");++count;
        g_windowMgr.DeleteWindow(g_windowMgr.m_cardInfoWnd);
        resetParent(true,{254,4002,0,0});
        ItemCardDetailRequire(parent.m_h==149,"254 marker parent did not retain the source socket-row height");
        ItemCardDetailClickCell(0,parent.m_h);expectCard(254);
        g_windowMgr.DeleteWindow(g_windowMgr.m_cardInfoWnd);
        resetParent(true,{501,4002,0,0});
        UIItemInfoWnd::DisplayData hidden{};
        ItemCardDetailRequire(parent.GetDisplayDataForQt(&hidden)&&hidden.slotEntries.empty(),
            "noncard prefix unexpectedly painted a socket row");
        ItemCardDetailClickCell(0,parent.m_h);expectCard(501);
        g_windowMgr.DeleteWindow(g_windowMgr.m_cardInfoWnd);
        resetParent(true,{0xeeee,4002,0,0});
        ItemCardDetailClickCell(0,parent.m_h);expectCard(0xeeee);
        g_windowMgr.DeleteWindow(g_windowMgr.m_cardInfoWnd);
        const auto zeroSocket=ItemCardDetailParentItem(mode,true,{4001,4002,0,0},501);
        parent.SetItemInfo(zeroSocket,0,0,ItemInfoSource::Snapshot);
        ItemCardDetailClickCell(0,parent.m_h);noCard("zero-socket descriptor exposed a card hit");
    } else if(stage=="routing") {
        // The lower-right point is geometrically in slot four's old hit band,
        // but belongs to the description child and must not open a card.
        resetParent(false,{4001,4002,4003,4004});
        ItemCardDetailRequire(parent.m_h==120,"routing parent is not the 120-pixel base detail");
        ItemCardDetailNativeRightClick(parent.m_x+100,parent.m_y+100);noCard("description child right-click leaked to the underlying fourth card");
        auto* parentInput=Find(view.rootObject(),"itemInfoCardInput");
        ItemCardDetailRequire(parentInput&&parentInput->isVisible(),"main item detail card input surface is missing");
        const QPointF qtPoint(parent.m_x+15,parent.m_y+parent.m_h-13);
        ItemCardDetailQtRightClick(app,view,qtPoint);expectCard(4001);
        ItemCardDetailRequire(view.grabWindow().save(output+"/item-card-detail-qt-right-click.qml.png"),"save QML right-click card detail frame failed");
    } else if(stage=="hover") {
        resetParent(true,{4001,0,4002,4004});
        ItemCardDetailClickCell(0,parent.m_h);expectCard(4001);
        const QString first=ItemCardDetailCheckHover(app,view,uiState,adapter,mode,parent,0,true,"native prefix card hover did not create tooltip text");++count;
        ItemCardDetailCheckHover(app,view,uiState,adapter,mode,parent,1,false,"native hover crossed the first empty card slot");++count;
        ItemCardDetailRequire(ItemCardDetailCheckHover(app,view,uiState,adapter,mode,parent,2,false,
            "native hover exposed a card after the prefix hole").isEmpty(),"prefix hole retained tooltip text");++count;
        // QML hover drives the same source manager route; moving out or over a
        // prefix hole clears both independent detail tooltip projections.
        const QPointF qtFirst(parent.m_x+15,parent.m_y+parent.m_h-13);
        Mouse(app,view,qtFirst);ItemCardDetailSync(app,view,adapter,mode);
        auto* mainTooltip=Find(view.rootObject(),"itemInfoSlotTooltip");
        auto* cardTooltip=Find(view.rootObject(),"cardInfoSlotTooltip");
        ItemCardDetailRequire(mainTooltip&&mainTooltip->isVisible()&&cardTooltip&&!cardTooltip->isVisible(),
            "QML hover did not show only the parent card tooltip while both windows coexist");
        const QPointF qtHole(parent.m_x+40,parent.m_y+parent.m_h-13);
        Mouse(app,view,qtHole);ItemCardDetailSync(app,view,adapter,mode);
        ItemCardDetailRequire(!mainTooltip->isVisible()&&!cardTooltip->isVisible(),
            "QML hover over a prefix hole did not clear both tooltip layers");++count;
        Mouse(app,view,QPointF(Width-5,Height-5));ItemCardDetailSync(app,view,adapter,mode);
        ItemCardDetailRequire(uiState->itemInfoData().value("slotTooltip").toString().isEmpty()
            &&uiState->cardInfoData().value("slotTooltip").toString().isEmpty(),
            "QML pointer exit did not clear native hover from both detail windows");++count;

        // All four metadata-backed prefixes remain independently hoverable.
        g_windowMgr.DeleteWindow(g_windowMgr.m_cardInfoWnd);
        resetParent(true,{4001,4002,4003,4004});
        ItemCardDetailClickCell(0,parent.m_h);expectCard(4001);
        const QString lastText=ItemCardDetailCheckHover(app,view,uiState,adapter,mode,parent,3,true,
            "last declared card slot did not produce tooltip text");++count;
        ItemCardDetailRequire(!lastText.isEmpty(),"last-slot tooltip fixture is empty");
        const QVariantMap mainData=uiState->itemInfoData();
        const int anchorX=parent.m_x+4+25*3,anchorY=parent.m_y+parent.m_h-50;
        ItemCardDetailRequire(mainData.value("slotTooltipX").toInt()==anchorX&&mainData.value("slotTooltipY").toInt()==anchorY,
            "last-slot tooltip did not preserve the global native anchor");
        auto* lastTooltip=Find(view.rootObject(),"itemInfoSlotTooltip");
        auto* cardFrame=Find(view.rootObject(),"cardInfoWindow");
        ItemCardDetailRequire(lastTooltip&&lastTooltip->isVisible()&&cardFrame&&cardFrame->isVisible()
            &&lastTooltip->z()>cardFrame->z(),"root-level parent tooltip was not layered above a coexisting card frame");
        const QRectF lastRect=lastTooltip->mapRectToScene(lastTooltip->boundingRect());
        ItemCardDetailRequire(lastText==QString(35,'W')+"\n"+QString(35,'W')
            &&lastRect.right()>parent.m_x+parent.m_w,
            "long card tooltip did not wrap at35 characters and extend beyond its owner");
        ItemCardDetailRequire(lastRect.left()>=-4&&lastRect.top()>=-4&&lastRect.right()<=view.width()+4&&lastRect.bottom()<=view.height()+4,
            "card tooltip exceeded the viewport clamp");
        ItemCardDetailRequire(view.grabWindow().save(output+"/item-card-detail-hover-layers.qml.png"),"save card tooltip layering frame failed");
        ArgbDibSurface surface;
        ItemCardDetailRequire(surface.EnsureSize(1280,720),"card detail native canvas failed");
        std::fill_n(surface.GetPixels(),1280*720,0xff181b20u);
        auto* nativeCard=g_windowMgr.m_cardInfoWnd;
        const int cardX=nativeCard->m_x,cardY=nativeCard->m_y;
        nativeCard->Move(anchorX,anchorY);
        g_windowMgr.OnDrawExcludingRoMapToHdc(surface.GetDC());
        const QImage nativeImage=QImage(reinterpret_cast<uchar*>(surface.GetPixels()),1280,720,1280*4,QImage::Format_RGB32).copy();
        ItemCardDetailRequire((nativeImage.pixel(anchorX+3,anchorY+3)&0xffffffu)==0x303030u,
            "native tooltip was covered by the coexisting card window");
        ItemCardDetailRequire(PixelsNear(nativeImage,QRect(anchorX+6,anchorY+18,380,13),0xffffffffu)>10,
            "native card tooltip flattened or clipped its second wrapped line");
        ItemCardDetailRequire(nativeImage.save(output+"/item-card-detail-hover.native.png"),"save native card detail frame failed");
        nativeCard->Move(cardX,cardY);

        // Native and Qt hover paths must both publish a tooltip; the QML path
        // checks the actual delegated text and root-level viewport clamp.
        const QPointF qtLast(parent.m_x+15+25*3,parent.m_y+parent.m_h-13);
        Mouse(app,view,qtLast);ItemCardDetailSync(app,view,adapter,mode);
        ItemCardDetailRequire(lastTooltip->isVisible()&&ItemCardDetailTooltipText(lastTooltip)==lastText,
            "actual QML last-slot hover text differs from the native tooltip source");++count;
        Mouse(app,view,QPointF(Width-5,Height-5));ItemCardDetailSync(app,view,adapter,mode);
        ItemCardDetailRequire(!lastTooltip->isVisible(),"actual QML tooltip did not clear on pointer exit");++count;
        ItemCardDetailRequire(first.size()>0,"first card tooltip was empty");
    } else if(stage=="lifecycle") {
        ItemCardDetailClickCell(0,parent.m_h);expectCard(4001);
        UIItemInfoWnd* card=g_windowMgr.m_cardInfoWnd;
        card->Move(15,55);card->StoreInfo();ItemCardDetailSync(app,view,adapter,mode);
        auto saved=g_windowMgr.GetItemDetailWindowInfo(ItemDetailKind::SlottedCard);
        const auto mainSaved=g_windowMgr.GetItemDetailWindowInfo(ItemDetailKind::Main);
        ItemCardDetailRequire(saved.x==15&&saved.y==55&&saved.width==280&&saved.height==120,
            "120-pixel card placement was not persisted independently");
        ItemCardDetailRequire(mainSaved.x==280&&mainSaved.y==126,"card placement overwrote the main detail record");
        auto* close=Find(view.rootObject(),"cardInfoClose");Click(app,view,close);
        ItemCardDetailRequire(g_windowMgr.m_cardInfoWnd==nullptr,"actual Qt card close button did not delete managed window");
        ItemCardDetailClickCell(0,parent.m_h);expectCard(4001);
        ItemCardDetailRequire(g_windowMgr.m_cardInfoWnd->m_x==15&&g_windowMgr.m_cardInfoWnd->m_y==55,
            "managed card recreation did not restore valid placement");
        g_windowMgr.DeleteWindow(g_windowMgr.m_cardInfoWnd);
        g_windowMgr.StoreItemDetailWindowInfo({40,70,280,149},ItemDetailKind::SlottedCard);
        ItemCardDetailClickCell(0,parent.m_h);expectCard(4001);
        ItemCardDetailRequire(g_windowMgr.m_cardInfoWnd->m_x==600&&g_windowMgr.m_cardInfoWnd->m_y==556,
            "149-pixel card record was not rejected before managed recreation");
        g_windowMgr.DeleteWindow(g_windowMgr.m_itemInfoWnd);
        ItemCardDetailSync(app,view,adapter,mode);
        ItemCardDetailRequire(g_windowMgr.m_itemInfoWnd==nullptr&&g_windowMgr.m_cardInfoWnd
            &&uiState->cardInfoData().value("visible").toBool(),
            "closing the main detail parent also closed its independent card detail child");++count;
    } else throw std::runtime_error("unknown card detail child stage");
    ItemCardDetailWriteResult(output,stage,count,"PASS");
}

void CheckItemCardDetailDriver(const QString& output)
{
    const QString root=QDir::cleanPath(QDir(output).absolutePath()+QString("/item-card-detail-%1-%2")
        .arg(QCoreApplication::applicationPid()).arg(QDateTime::currentMSecsSinceEpoch()));
    const QStringList stages={"cold-default","warm-valid","warm-invalid","warm-partial","source-hits","metadata-gates","routing","hover","lifecycle"};
    QJsonArray children;
    for(const QString& stage:stages) {
        const QString group=stage,groupDir=root+"/"+group,childOut=groupDir+"/"+stage;
        ItemCardDetailRequire(QDir().mkpath(childOut),"create card detail child output failed");
        ItemCardDetailWriteSeed(groupDir+"/settings",stage);
        QProcess process;QProcessEnvironment env=QProcessEnvironment::systemEnvironment();
        env.insert("OPEN_MIDGARD_DATA_DIR",groupDir+"/settings");process.setProcessEnvironment(env);
        process.start(QCoreApplication::applicationFilePath(),{childOut,"item-card-detail-child",stage});
        ItemCardDetailRequire(process.waitForStarted(5000),"card detail child did not start");
        if(!process.waitForFinished(20000)){process.kill();process.waitForFinished(1000);throw std::runtime_error("card detail child exceeded 20 second limit");}
        const QByteArray stderrBytes=process.readAllStandardError(),stdoutBytes=process.readAllStandardOutput();
        if(process.exitStatus()!=QProcess::NormalExit||process.exitCode()!=0) {
            const QByteArray clipped=(stderrBytes+" "+stdoutBytes).right(600);
            throw std::runtime_error(("card detail child "+stage.toStdString()+" failed: "+clipped.toStdString()).c_str());
        }
        QFile resultFile(childOut+"/item-card-detail.json");
        ItemCardDetailRequire(resultFile.open(QIODevice::ReadOnly),"card detail child result missing");
        QJsonParseError error{};const auto document=QJsonDocument::fromJson(resultFile.readAll(),&error);
        ItemCardDetailRequire(error.error==QJsonParseError::NoError&&document.isObject(),"card detail child result invalid");
        const auto result=document.object();ItemCardDetailRequire(result.value("stage").toString()==stage,"card detail child stage mismatch");
        ItemCardDetailRequire(result.value("outcome").toString()=="PASS","card detail child did not report PASS");
        const qint64 pid=result.value("pid").toInteger();
        ItemCardDetailRequire(pid>0&&pid!=QCoreApplication::applicationPid(),
            "card detail stage did not run in a child process");
        children.push_back(QJsonObject{{"stage",stage},{"pid",pid},{"output_directory",childOut},{"case_count",result.value("case_count")},{"outcome",result.value("outcome")}});
    }
    QFile summary(output+"/item-card-detail-results.json");
    ItemCardDetailRequire(summary.open(QIODevice::WriteOnly),"open card detail summary failed");
    summary.write(QJsonDocument(QJsonObject{{"children",children},{"child_count",stages.size()},{"qml_errors",0},{"outcome","PASS"}}).toJson(QJsonDocument::Indented));
}
