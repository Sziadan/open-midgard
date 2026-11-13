// Source-derived item detail geometry, placement persistence, slots, and
// description viewport checks. Included by ItemColourRenderChecks.cpp.
#include <QProcess>
#include <QProcessEnvironment>
#include <QJsonParseError>
#include <QDateTime>
#include <QFileInfo>
#include <QWheelEvent>
#include "ui/NpcDialogColoredText.h"

namespace {
void ItemDetailGeometryRequire(bool ok,const char* message)
{
    if(!ok)throw std::runtime_error(message);
}

QImage ItemDetailGeometryNative(UIItemInfoWnd& window)
{
    ArgbDibSurface surface;
    ItemDetailGeometryRequire(surface.EnsureSize(1280,720),"native detail surface allocation failed");
    std::fill_n(surface.GetPixels(),1280*720,0xff181b20u);
    window.DrawNativeToHdc(surface.GetDC());
    return QImage(reinterpret_cast<uchar*>(surface.GetPixels()),1280,720,1280*4,QImage::Format_RGB32).copy();
}

void ItemDetailGeometryWriteResult(const QString& output,const QString& stage,
    const UIItemInfoWnd& detail,const QString& outcome)
{
    UIItemInfoWnd::DisplayData data{};
    ItemDetailGeometryRequire(detail.GetDisplayDataForQt(&data),"item detail geometry projection unavailable");
    const auto info=g_windowMgr.GetItemDetailWindowInfo();
    QFile file(output+"/item-detail-geometry.json");
    ItemDetailGeometryRequire(file.open(QIODevice::WriteOnly),"open item detail geometry child result failed");
    file.write(QJsonDocument(QJsonObject{{"pid",qint64(QCoreApplication::applicationPid())},{"stage",stage},
        {"x",detail.m_x},{"y",detail.m_y},{"width",detail.m_w},{"height",detail.m_h},
        {"record_x",info.x},{"record_y",info.y},{"record_width",info.width},{"record_height",info.height},
        {"description",QString::fromUtf8(data.description.c_str())},{"detail_lines",int(data.detailLines.size())},
        {"slot_count",int(data.slotEntries.size())},{"scroll_offset",detail.GetNativeDescriptionScrollOffset()},
        {"outcome",outcome}}).toJson(QJsonDocument::Compact));
}

void ItemDetailGeometryWriteSeed(const QString& settings,const QString& stage)
{
    ItemDetailGeometryRequire(QDir().mkpath(settings),"create item detail settings directory failed");
    QFile file(settings+"/open-midgard.ini");
    ItemDetailGeometryRequire(file.open(QIODevice::WriteOnly|QIODevice::Truncate),"write private item detail seed failed");
    QByteArray text;
    if(stage=="warm-valid")text="[UIWindows]\nItemInfoWndX=15\nItemInfoWndY=55\nItemInfoWndWidth=280\nItemInfoWndHeight=120\n";
    else if(stage=="warm-invalid")text="[UIWindows]\nItemInfoWndX=1001\nItemInfoWndY=55\nItemInfoWndWidth=280\nItemInfoWndHeight=149\n";
    else if(stage=="warm-invalid-x")text="[UIWindows]\nItemInfoWndX=nope\nItemInfoWndY=55\nItemInfoWndWidth=280\nItemInfoWndHeight=120\n";
    else if(stage=="warm-invalid-y-overflow")text="[UIWindows]\nItemInfoWndX=15\nItemInfoWndY=999999999999999999999999\nItemInfoWndWidth=280\nItemInfoWndHeight=120\n";
    else if(stage=="warm-partial")text="[UIWindows]\nItemInfoWndX=15\nItemInfoWndY=55\n";
    file.write(text);
}
}

void CheckItemDetailGeometryChild(QGuiApplication& app,QQuickView& view,QtUiState* uiState,
    CGameMode& mode,UIItemWnd& inventory,QtUiStateAdapter& adapter,const QString& output,const QString& stage)
{
    ItemDetailGeometryRequire(uiState,"item detail geometry Qt state missing");
    const QString actual=QDir::cleanPath(QFileInfo(QString::fromStdString(GetOpenMidgardIniPath().string())).absolutePath());
    const QString expected=QDir::cleanPath(QFileInfo(output).dir().absoluteFilePath("settings"));
    ItemDetailGeometryRequire(actual==expected,"item detail geometry child requires private sibling settings");
    if(g_windowMgr.m_storageWnd)g_windowMgr.m_storageWnd->SetShow(0);
    ro::commerce::state.cart.visible=false;
    InventoryTabStartupFixture(mode);
    ItemDetailGeometryRequire(adapter.syncGameplay(mode,RenderBackendType::LegacyDirect3D7,
        RenderBackendType::LegacyDirect3D7,0,0),"item detail geometry Qt sync failed");
    Frames(app,view);

    const auto open=[&](int index,unsigned int expectedId,bool identified,const std::array<unsigned int,4>& cardWords={}) -> UIItemInfoWnd& {
        const ITEM_INFO* source=g_session.GetInventoryItemByIndex(index);
        ItemDetailGeometryRequire(source&&source->GetItemId()==expectedId,"item detail geometry fixture item missing");
        ITEM_INFO item=*source;item.m_isIdentified=identified?1:0;
        std::copy(cardWords.begin(),cardWords.end(),std::begin(item.m_slot));
        g_windowMgr.ShowItemInfoWindow(item,900,500,ItemInfoSource::Snapshot,ItemInfoAction::ForceUpdate);
        ItemDetailGeometryRequire(adapter.syncGameplay(mode,RenderBackendType::LegacyDirect3D7,
            RenderBackendType::LegacyDirect3D7,0,0),"item detail geometry projection sync failed");Frames(app,view);
        ItemDetailGeometryRequire(g_windowMgr.m_itemInfoWnd&&g_windowMgr.m_itemInfoWnd->HasItem(),"item detail window not created");
        return *g_windowMgr.m_itemInfoWnd;
    };
    if(stage=="cold-default") {
        // Snapshot UI records before later settings writes, like Ref startup.
        SaveSettingsIniInt("UIWindows","ItemInfoWndX",15);
        SaveSettingsIniInt("UIWindows","ItemInfoWndY",55);
        SaveSettingsIniInt("UIWindows","ItemInfoWndWidth",280);
        SaveSettingsIniInt("UIWindows","ItemInfoWndHeight",120);
    }
    auto& detail=(stage=="slots")?open(3,1201,true):open(2,501,true);
    auto verifyWindow=[&](int x,int y,int height,const char* message) {
        UIItemInfoWnd::DisplayData data{};
        ItemDetailGeometryRequire(detail.GetDisplayDataForQt(&data),message);
        ItemDetailGeometryRequire(detail.m_x==x&&detail.m_y==y&&detail.m_w==280&&detail.m_h==height,message);
        auto* qml=Find(view.rootObject(),"itemInfoWindow");
        ItemDetailGeometryRequire(qml&&qml->isVisible()&&qRound(qml->x())==x&&qRound(qml->y())==y
            &&qRound(qml->width())==280&&qRound(qml->height())==height,"actual QML item detail geometry differs from native window");
        ItemDetailGeometryRequire(data.descriptionRect.left==x+91&&data.descriptionRect.top==y+31
            &&data.descriptionRect.right-data.descriptionRect.left==182&&data.descriptionRect.bottom-data.descriptionRect.top==80,
            "item detail description viewport geometry changed with window height");
    };

    if(stage=="cold-default") {
        verifyWindow(280,126,120,"cold item detail placement did not use source default");
        ItemDetailGeometryRequire(view.grabWindow().save(output+"/item-detail-geometry-normal.qml.png"),"save item detail QML frame failed");
    } else if(stage=="warm-valid") {
        verifyWindow(15,55,120,"valid persisted item detail placement was not restored");
        ItemDetailGeometryRequire(view.grabWindow().save(output+"/item-detail-geometry-persisted.qml.png"),"save persisted item detail QML frame failed");
        g_windowMgr.DeleteWindow(g_windowMgr.m_itemInfoWnd);
        g_windowMgr.StoreItemDetailWindowInfo({1000,600,280,120});
        auto& edge=open(3,1201,true);
        ItemDetailGeometryRequire(edge.m_x==1000&&edge.m_y==600&&edge.m_h==149,
            "valid boundary placement was clamped after socket-row expansion");
        auto* edgeQml=Find(view.rootObject(),"itemInfoWindow");
        ItemDetailGeometryRequire(edgeQml&&qRound(edgeQml->x())==1000&&qRound(edgeQml->y())==600,
            "QML applied an independent clamp to expanded boundary detail");
        ItemDetailGeometryRequire(view.grabWindow().save(output+"/item-detail-geometry-expanded-edge.qml.png"),"save expanded boundary detail failed");
    } else if(stage=="warm-invalid"||stage=="warm-partial"||stage.startsWith("warm-invalid-")) {
        verifyWindow(600,252,120,"invalid persisted item detail record did not fall back atomically");
        const ItemDetailWindowInfo badRecords[]={
            {15,55,279,120},{15,55,280,149},{15,602,280,120},{1001,55,280,120}};
        for(const auto& bad:badRecords) {
            g_windowMgr.StoreItemDetailWindowInfo(bad);
            int x=-1,y=-1;g_windowMgr.RestoreItemDetailWindowPlacement(&x,&y);
            const ItemDetailWindowInfo reset=g_windowMgr.GetItemDetailWindowInfo();
            ItemDetailGeometryRequire(x==600&&y==252&&reset.x==280&&reset.y==126
                &&reset.width==280&&reset.height==120,"invalid item detail record was not rejected as a whole");
        }
    } else if(stage=="lifecycle") {
        ArgbDibSurface measureSurface;ItemDetailGeometryRequire(measureSurface.EnsureSize(182,80),"description measurement surface failed");
        const int lineHeight=MeasureNpcSayDialogColoredTextHeight(measureSurface.GetDC(),182,"Header");
        ItemDetailGeometryRequire(lineHeight>0
            &&MeasureNpcSayDialogColoredTextHeight(measureSurface.GetDC(),182,"Header\rTail")==lineHeight*2
            &&MeasureNpcSayDialogColoredTextHeight(measureSurface.GetDC(),182,"Header\r\nTail")==lineHeight*2,
            "shared colored viewer layout stalled or double-counted carriage returns");
        verifyWindow(280,126,120,"initial item detail dimensions differ from source");
        const int oldX=detail.m_x,oldY=detail.m_y;
        ITEM_INFO resized=*g_session.GetInventoryItemByIndex(3);resized.m_slot[0]=4001;
        detail.SetItemInfo(resized,700,400,ItemInfoSource::Snapshot);
        ItemDetailGeometryRequire(detail.m_x==oldX&&detail.m_y==oldY&&detail.m_h==149,
            "identified socket update moved the window or missed source height");
        UIItemInfoWnd::DisplayData resizedData{};ItemDetailGeometryRequire(detail.GetDisplayDataForQt(&resizedData),"resized detail display missing");
        ItemDetailGeometryRequire(resizedData.descriptionRect.left==oldX+91&&resizedData.descriptionRect.top==oldY+31
            &&resizedData.descriptionRect.right-resizedData.descriptionRect.left==182
            &&resizedData.descriptionRect.bottom-resizedData.descriptionRect.top==80,
            "socket resize changed the fixed description viewer rectangle");
        resized.m_slot[0]=255;detail.SetItemInfo(resized,900,500,ItemInfoSource::Snapshot);
        ItemDetailGeometryRequire(detail.m_x==oldX&&detail.m_y==oldY&&detail.m_h==120,
            "slot marker update moved or resized the item detail window");
        UIItemInfoWnd::DisplayData before{};ItemDetailGeometryRequire(detail.GetDisplayDataForQt(&before),"item detail display missing before scroll");
        ItemDetailGeometryRequire(before.description.size()>200,"long item detail description fixture was not loaded");
        const auto generationBefore=before.descriptionGeneration;
        const QRect viewerRect(before.descriptionRect.left,before.descriptionRect.top,182,80);
        auto nativeTop=ItemDetailGeometryNative(detail);
        ItemDetailGeometryRequire(Pixels(nativeTop,viewerRect,0xffff0000u)>3,"native viewer did not paint the red first line");
        ItemDetailGeometryRequire(nativeTop.save(output+"/item-detail-geometry-top.native.png"),"save native viewer top failed");
        ItemDetailGeometryRequire(g_windowMgr.OnWheel(detail.m_x+100,detail.m_y+40,-120),
            "native description viewport did not receive wheel input at its child geometry");
        ItemDetailGeometryRequire(detail.GetNativeDescriptionScrollOffset()>0,"long native description did not scroll");
        detail.ScrollDescriptionWheel(-1200000);
        const auto nativeBottom=ItemDetailGeometryNative(detail);
        ItemDetailGeometryRequire(Pixels(nativeBottom,viewerRect,0xff0000ffu)>3
            &&Pixels(nativeBottom,viewerRect,0xffff0000u)==0,"native clipped viewer cannot reach the blue last line");
        ItemDetailGeometryRequire(nativeBottom.save(output+"/item-detail-geometry-bottom.native.png"),"save native viewer bottom failed");
        detail.ScrollDescriptionWheel(1200000);
        ItemDetailGeometryRequire(detail.GetNativeDescriptionScrollOffset()==0,"native viewer did not clamp back to its start");
        g_windowMgr.OnWheel(detail.m_x+20,detail.m_y+40,-120);
        ItemDetailGeometryRequire(detail.GetNativeDescriptionScrollOffset()==0,"wheel outside the description child scrolled it");
        ItemDetailGeometryRequire(adapter.syncGameplay(mode,RenderBackendType::LegacyDirect3D7,
            RenderBackendType::LegacyDirect3D7,0,0),"item detail scroll sync failed");Frames(app,view);
        UIItemInfoWnd::DisplayData after{};ItemDetailGeometryRequire(detail.GetDisplayDataForQt(&after),"item detail display missing after scroll");
        ItemDetailGeometryRequire(after.descriptionGeneration==generationBefore,"wheel scroll changed item description generation");
        auto* viewport=Find(view.rootObject(),"itemInfoDescriptionViewport");
        ItemDetailGeometryRequire(viewport,"actual Qt description viewport is missing");
        const QPointF qtPoint=viewport->mapToScene(QPointF(viewport->width()/2,viewport->height()/2));
        const QPoint qtGlobal=view.mapToGlobal(qtPoint.toPoint());
        QWheelEvent qtWheel(qtPoint,qtGlobal,QPoint(),QPoint(0,-120),Qt::NoButton,Qt::NoModifier,
            Qt::NoScrollPhase,false);
        QCoreApplication::sendEvent(&view,&qtWheel);Frames(app,view);
        ItemDetailGeometryRequire(viewport->property("contentY").toReal()>0,
            "actual Qt description Flickable did not scroll on wheel input");
        ItemDetailGeometryRequire(view.grabWindow().save(output+"/item-detail-geometry-scroll.qml.png"),"save scrolled item detail QML frame failed");
        const ITEM_INFO* otherSource=g_session.GetInventoryItemByIndex(2);
        ItemDetailGeometryRequire(otherSource&&otherSource->GetItemId()==501,"second item description fixture missing");
        ITEM_INFO other=*otherSource;other.m_isIdentified=1;
        g_windowMgr.ShowItemInfoWindow(other,detail.m_x,detail.m_y,ItemInfoSource::Snapshot,ItemInfoAction::ForceUpdate);
        ItemDetailGeometryRequire(adapter.syncGameplay(mode,RenderBackendType::LegacyDirect3D7,
            RenderBackendType::LegacyDirect3D7,0,0),"second item detail generation sync failed");Frames(app,view);
        UIItemInfoWnd::DisplayData changed{};ItemDetailGeometryRequire(detail.GetDisplayDataForQt(&changed),"second item detail display missing");
        ItemDetailGeometryRequire(changed.descriptionGeneration>generationBefore&&detail.GetNativeDescriptionScrollOffset()==0,
            "item selection did not advance description generation and reset scrolling");
        ItemDetailGeometryRequire(changed.description==before.description,"generation-reset probe requires identical description text");
        ItemDetailGeometryRequire(changed.description.size()>200,"long second item description fixture was not loaded");
        ItemDetailGeometryRequire(viewport->property("contentY").toReal()==0,
            "actual Qt description Flickable did not reset after item selection");
        const int dragX=detail.m_x,dragY=detail.m_y;
        detail.OnLBtnDown(dragX+20,dragY+10);detail.OnMouseMove(dragX+60,dragY+30);detail.OnLBtnUp(dragX+60,dragY+30);
        ItemDetailGeometryRequire(detail.m_x==dragX&&detail.m_y==dragY,"preview area incorrectly started source title dragging");
        g_windowMgr.OnLBtnDown(dragX+100,dragY+10);g_windowMgr.OnMouseMove(dragX+132,dragY+26);g_windowMgr.OnLBtnUp(dragX+132,dragY+26);
        ItemDetailGeometryRequire(detail.m_x==dragX+32&&detail.m_y==dragY+16,"native source title area did not move the detail window");
        // Force an identified item, then confirm its three declared entries
        // share the fixed bottom anchored row at either supported height.
        auto& socketDetail=open(3,1201,true,{4001,4002,4003,4004});
        UIItemInfoWnd::DisplayData slotData{};ItemDetailGeometryRequire(socketDetail.GetDisplayDataForQt(&slotData),"identified slot display missing");
        ItemDetailGeometryRequire(slotData.slotEntries.size()==3,
            "item1201 must project exactly its three source-declared sockets");
        for(size_t i=0;i<slotData.slotEntries.size();++i)ItemDetailGeometryRequire(slotData.slotEntries[i].x==socketDetail.m_x+4+25*int(i)
            &&slotData.slotEntries[i].y==socketDetail.m_y+socketDetail.m_h-26,"item detail slot geometry differs from source");
        auto& noCards=open(3,1201,true,{});UIItemInfoWnd::DisplayData noCardData{};
        ItemDetailGeometryRequire(noCards.GetDisplayDataForQt(&noCardData)
            &&noCardData.slotEntries.size()==slotData.slotEntries.size(),"item detail projected slots beyond its declared slot count");

        // Persist and recreate through the managed-window lifecycle. A valid
        // 120-high record restores its position; a persisted 149-high record
        // is rejected as a whole and the next socket detail uses scaled default.
        const auto& normal=*g_session.GetInventoryItemByIndex(2);
        g_windowMgr.ShowItemInfoWindow(normal,900,500,ItemInfoSource::Snapshot,ItemInfoAction::ForceUpdate);
        detail.Move(15,55);detail.StoreInfo();
        ItemDetailWindowInfo saved=g_windowMgr.GetItemDetailWindowInfo();
        ItemDetailGeometryRequire(saved.x==15&&saved.y==55&&saved.width==280&&saved.height==120,
            "120-high item detail placement was not persisted as a valid record");
        g_windowMgr.OnLBtnDown(detail.m_x+100,detail.m_y+40);
        ItemDetailGeometryRequire(g_windowMgr.m_captureWindow&&g_windowMgr.m_captureWindow->m_parent==&detail,
            "description input child did not own native mouse capture");
        g_windowMgr.DeleteWindow(g_windowMgr.m_itemInfoWnd);
        ItemDetailGeometryRequire(!g_windowMgr.m_captureWindow&&!g_windowMgr.m_lastHitWindow,
            "deleting item detail retained a dangling child input pointer");
        g_windowMgr.ShowItemInfoWindow(normal,900,500,ItemInfoSource::Snapshot,ItemInfoAction::ForceUpdate);
        ItemDetailGeometryRequire(g_windowMgr.m_itemInfoWnd&&g_windowMgr.m_itemInfoWnd->m_x==15
            &&g_windowMgr.m_itemInfoWnd->m_y==55&&g_windowMgr.m_itemInfoWnd->m_h==120,
            "managed item detail recreation did not restore valid 120-high placement");
        ITEM_INFO socketItem=*g_session.GetInventoryItemByIndex(3);socketItem.m_slot[0]=4001;
        g_windowMgr.ShowItemInfoWindow(socketItem,900,500,ItemInfoSource::Snapshot,ItemInfoAction::ForceUpdate);
        ItemDetailGeometryRequire(g_windowMgr.m_itemInfoWnd->m_h==149,"managed socket detail did not expand to149 pixels");
        g_windowMgr.m_itemInfoWnd->Move(40,70);g_windowMgr.m_itemInfoWnd->StoreInfo();
        saved=g_windowMgr.GetItemDetailWindowInfo();
        ItemDetailGeometryRequire(saved.x==40&&saved.y==70&&saved.width==280&&saved.height==149,
            "149-high item detail placement was not persisted in full");
        g_windowMgr.DeleteWindow(g_windowMgr.m_itemInfoWnd);
        g_windowMgr.ShowItemInfoWindow(socketItem,900,500,ItemInfoSource::Snapshot,ItemInfoAction::ForceUpdate);
        ItemDetailGeometryRequire(g_windowMgr.m_itemInfoWnd&&g_windowMgr.m_itemInfoWnd->m_x==600
            &&g_windowMgr.m_itemInfoWnd->m_y==252&&g_windowMgr.m_itemInfoWnd->m_h==149,
            "managed recreation did not reject persisted 149-high record before socket expansion");
    } else if(stage=="slots") {
        auto& slotted=open(3,1201,true,{4001,0,4003,0});UIItemInfoWnd::DisplayData data{};
        ItemDetailGeometryRequire(slotted.GetDisplayDataForQt(&data)&&data.slotEntries.size()==3
            &&data.slotEntries[0].occupied&&!data.slotEntries[1].occupied&&data.slotEntries[2].occupied,"declared socket painting did not retain visible holes");
        ItemDetailGeometryRequire(view.grabWindow().save(output+"/item-detail-geometry-sockets.qml.png"),"save socket item detail frame failed");
        auto& hidden=open(3,1201,false,{4001,4002,0,0});
        ItemDetailGeometryRequire(hidden.m_h==120,"unidentified item expanded for hidden slot metadata");
        auto& sentinel=open(3,1201,true,{255,4002,0,0});
        ItemDetailGeometryRequire(sentinel.m_h==120,"slot sentinel expanded item detail window");
        ItemDetailGeometryRequire(sentinel.GetDisplayDataForQt(&data)&&data.slotEntries.empty(),"manufactured marker became a painted card row");
        ItemDetailGeometryRequire(view.grabWindow().save(output+"/item-detail-geometry-marker.qml.png"),"save marker item detail frame failed");
        auto& created=open(3,1201,true,{254,0,0,0});
        ItemDetailGeometryRequire(created.m_h==149&&created.GetDisplayDataForQt(&data)&&data.slotEntries.empty(),
            "created marker254 was conflated with manufactured255 or painted as a card");
        auto& unknown=open(3,1201,true,{501,0,0,0});
        ItemDetailGeometryRequire(unknown.m_h==149&&unknown.GetDisplayDataForQt(&data)&&data.slotEntries.empty(),
            "non-card first word was painted as a socket row");
        auto& manual=open(3,1201,true,{4001,0,0,0});
        manual.Resize(280,120);
        ItemDetailGeometryRequire(manual.GetDisplayDataForQt(&data)&&data.slotEntries.empty(),"socket painting ignored current outer height");
        manual.Resize(280,149);
        ItemDetailGeometryRequire(manual.GetDisplayDataForQt(&data)&&data.slotEntries.size()==3&&data.slotEntries[0].itemId==4001,
            "geometry-only expansion lost cached source card words");
        auto& manualUnidentified=open(3,1201,false,{4001,0,0,0});
        manualUnidentified.Resize(280,149);
        ItemDetailGeometryRequire(manualUnidentified.GetDisplayDataForQt(&data)&&data.slotEntries.size()==3&&data.slotEntries[0].itemId==4001,
            "painting incorrectly reused the identification-based expansion predicate");
        auto& guard=open(3,1201,true,{});
        guard.Resize(280,144);
        ITEM_INFO guarded=*g_session.GetInventoryItemByIndex(3);
        guard.SetItemInfo(guarded,900,500,ItemInfoSource::Snapshot);
        ItemDetailGeometryRequire(guard.m_h==144,"primary144 height guard was normalized into149");
        open(2,501,true); // Isolate the ordinary149 branch from the synthetic144 guard.
        auto fourPacket=Equipment(0xd0,true,0);fourPacket[6]=0xb2;fourPacket[7]=4;Dispatch(mode,fourPacket);
        auto& four=open(3,1202,true,{4001,4002,4003,4004});
        ItemDetailGeometryRequire(four.GetDisplayDataForQt(&data)&&data.slotEntries.size()==4,
            "item1202 lost its four declared sockets");
        ItemDetailGeometryRequire(view.grabWindow().save(output+"/item-detail-geometry-four-sockets.qml.png"),"save four-socket detail frame failed");
    } else throw std::runtime_error("unknown item detail geometry child stage");
    ItemDetailGeometryRequire(g_windowMgr.m_itemInfoWnd,"item detail geometry result window missing");
    ItemDetailGeometryWriteResult(output,stage,*g_windowMgr.m_itemInfoWnd,"PASS");
}

void CheckItemDetailGeometryDriver(const QString& output)
{
    const QString root=QDir::cleanPath(QDir(output).absolutePath()+QString("/item-detail-geometry-%1-%2")
        .arg(QCoreApplication::applicationPid()).arg(QDateTime::currentMSecsSinceEpoch()));
    const QStringList stages={"cold-default","warm-valid","warm-invalid","warm-partial","warm-invalid-x","warm-invalid-y-overflow","lifecycle","slots"};
    QJsonArray children;QSet<qint64> pids;
    for(const QString& stage:stages) {
        // Every child has its own settings directory so lifecycle writes from
        // one process cannot turn a later cold-start case into a warm start.
        const QString group=stage;
        const QString groupDir=root+"/"+group,childOut=groupDir+"/"+stage;
        ItemDetailGeometryRequire(QDir().mkpath(childOut),"create item detail geometry child output failed");
        ItemDetailGeometryWriteSeed(groupDir+"/settings",stage);
        QProcess process;QProcessEnvironment env=QProcessEnvironment::systemEnvironment();
        env.insert("OPEN_MIDGARD_DATA_DIR",groupDir+"/settings");process.setProcessEnvironment(env);
        process.start(QCoreApplication::applicationFilePath(),{childOut,"item-detail-geometry-child",stage});
        ItemDetailGeometryRequire(process.waitForStarted(5000),"item detail geometry child did not start");
        if(!process.waitForFinished(20000)){process.kill();process.waitForFinished(1000);throw std::runtime_error("item detail geometry child exceeded 20 second limit");}
        const QByteArray stderrBytes=process.readAllStandardError(),stdoutBytes=process.readAllStandardOutput();
        if(process.exitStatus()!=QProcess::NormalExit||process.exitCode()!=0) {
            const QByteArray clipped=(stderrBytes+" "+stdoutBytes).right(600);
            throw std::runtime_error(("item detail geometry child "+stage.toStdString()+" failed: "+clipped.toStdString()).c_str());
        }
        QFile resultFile(childOut+"/item-detail-geometry.json");
        ItemDetailGeometryRequire(resultFile.open(QIODevice::ReadOnly),"item detail geometry child result missing");
        QJsonParseError error{};const auto document=QJsonDocument::fromJson(resultFile.readAll(),&error);
        ItemDetailGeometryRequire(error.error==QJsonParseError::NoError&&document.isObject(),"item detail geometry child result invalid");
        auto result=document.object();ItemDetailGeometryRequire(result.value("stage").toString()==stage,"item detail geometry child stage mismatch");
        const qint64 pid=result.value("pid").toInteger();ItemDetailGeometryRequire(pid>0&&pid!=QCoreApplication::applicationPid(),"item detail geometry stage is not a child process");
        pids.insert(pid);
        result.insert("output_directory",childOut);
        children.push_back(result);
    }
    QFile summary(output+"/item-detail-geometry-results.json");
    ItemDetailGeometryRequire(summary.open(QIODevice::WriteOnly),"open item detail geometry summary failed");
    summary.write(QJsonDocument(QJsonObject{{"children",children},{"case_count",stages.size()},
        {"fresh_processes",stages.size()},{"observed_distinct_pids",pids.size()},{"outcome","PASS"}}).toJson(QJsonDocument::Indented));
}
