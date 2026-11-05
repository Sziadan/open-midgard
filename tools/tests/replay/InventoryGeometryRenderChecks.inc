// Source-derived inventory window geometry, mini mode, and complete-record
// persistence checks. Included by ItemColourRenderChecks.cpp.
#include <QProcess>
#include <QProcessEnvironment>
#include <QJsonParseError>
#include <QDateTime>
#include <QFileInfo>

namespace {
void InventoryGeometryRequire(bool ok,const char* message)
{
    if(!ok)throw std::runtime_error(message);
}

void InventoryGeometryWriteResult(const QString& output,const QString& stage,
    const UIItemWnd& inventory,const QString& outcome)
{
    const auto info=g_windowMgr.GetInventoryWindowInfo();
    QFile file(output+"/inventory-geometry.json");
    InventoryGeometryRequire(file.open(QIODevice::WriteOnly),"open inventory geometry child result failed");
    const QJsonObject result{{"pid",qint64(QCoreApplication::applicationPid())},{"stage",stage},
        {"x",inventory.m_x},{"y",inventory.m_y},{"width",inventory.m_w},{"height",inventory.m_h},
        {"original_height",info.originalHeight},{"restore_height",inventory.GetRestoreHeight()},{"tab",inventory.GetCurrentTab()},
        {"record_x",info.x},{"record_y",info.y},{"record_width",info.width},{"record_height",info.height},
        {"record_tab",info.tab},{"mini",inventory.IsMiniMode()},{"outcome",outcome}};
    file.write(QJsonDocument(result).toJson(QJsonDocument::Compact));
}

void InventoryGeometryAssert(const UIItemWnd& inventory,int x,int y,int width,int height,
    int originalHeight,int restoreHeight,int tab,const char* message)
{
    const auto info=g_windowMgr.GetInventoryWindowInfo();
    InventoryGeometryRequire(inventory.m_x==x&&inventory.m_y==y&&inventory.m_w==width
        &&inventory.m_h==height&&info.originalHeight==originalHeight
        &&inventory.GetRestoreHeight()==restoreHeight
        &&inventory.GetCurrentTab()==tab&&info.tab==tab,message);
}

void InventoryGeometryCheckQml(QQuickView& view,int x,int y,int width,int height)
{
    auto* panel=Find(view.rootObject(),"commerce_inventory");
    InventoryGeometryRequire(panel&&panel->isVisible(),"actual QML inventory panel is missing or hidden");
    InventoryGeometryRequire(qRound(panel->x())==x&&qRound(panel->y())==y
        &&qRound(panel->width())==width&&qRound(panel->height())==height,
        "actual QML inventory panel geometry differs from native window");
}

void InventoryGeometrySeed(InventoryWindowInfo info)
{
    g_windowMgr.StoreInventoryWindowInfo(info);
}
}

void CheckInventoryGeometryChild(QGuiApplication& app,QQuickView& view,QtUiState* uiState,
    CGameMode& mode,UIItemWnd& inventory,QtUiStateAdapter& adapter,const QString& output,
    const QString& stage)
{
    InventoryGeometryRequire(uiState,"inventory geometry Qt state missing");
    const QString actual=QDir::cleanPath(QFileInfo(QString::fromStdString(GetOpenMidgardIniPath().string())).absolutePath());
    const QString expected=QDir::cleanPath(QFileInfo(output).dir().absoluteFilePath("settings"));
    InventoryGeometryRequire(actual==expected,"inventory geometry child requires private sibling settings");
    if(g_windowMgr.m_storageWnd)g_windowMgr.m_storageWnd->SetShow(0);
    ro::commerce::state.cart.visible=false;
    InventoryTabStartupFixture(mode);
    InventoryGeometryRequire(adapter.syncGameplay(mode,RenderBackendType::LegacyDirect3D7,
        RenderBackendType::LegacyDirect3D7,0,0),"inventory geometry Qt sync failed");
    Frames(app,view);
    UIItemWnd* active=&inventory;

    if(stage=="seed-warm") {
        InventoryWindowInfo info{};
        info.x=744;info.y=514;info.width=280;info.height=134;info.originalHeight=134;
        info.tab=1;info.radio1=3;info.radio2=3;info.radio3=4;
        InventoryGeometrySeed(info);
        InventoryGeometryWriteResult(output,stage,inventory,"SEEDED");return;
    }
    if(stage=="defaults") {
        InventoryGeometryAssert(inventory,0,120,280,102,120,102,0,
            "cold inventory geometry did not use normalized source defaults");
        UIItemWnd::DisplayData display{};
        InventoryGeometryRequire(inventory.GetDisplayDataForQt(&display),"cold inventory display projection failed");
        InventoryGeometryRequire(display.currentTab==0,"cold inventory native projection has wrong tab");
        InventoryGeometryCheckQml(view,0,120,280,102);
        InventoryGeometryRequire(view.grabWindow().save(output+"/inventory-geometry-default.qml.png"),"save default inventory QML frame failed");
        Hover(inventory);const QImage native=Native(inventory,0xffffffffu);
        InventoryGeometryRequire(native.save(output+"/inventory-geometry-default.native.png"),"save default native inventory frame failed");
    } else if(stage=="warm") {
        InventoryGeometryAssert(inventory,744,514,280,134,134,134,1,
            "warm inventory geometry record did not restore as a whole");
        UIItemWnd::DisplayData display{};
        InventoryGeometryRequire(inventory.GetDisplayDataForQt(&display)&&display.currentTab==1,
            "warm inventory native projection differs from restored record");
        InventoryGeometryCheckQml(view,744,514,280,134);
        InventoryGeometryRequire(view.grabWindow().save(output+"/inventory-geometry-warm.qml.png"),"save warm inventory QML frame failed");
        Hover(inventory);const QImage native=Native(inventory,0xffffffffu,1280,720);
        InventoryGeometryRequire(native.save(output+"/inventory-geometry-warm.native.png"),"save warm native inventory frame failed");
    } else if(stage=="small") {
        InventoryGeometryAssert(inventory,0,120,280,102,120,102,0,
            "small viewport cold inventory geometry differs from source defaults");
    } else if(stage=="invalid") {
        UIItemWnd* original=g_windowMgr.m_itemWnd;g_windowMgr.m_itemWnd=nullptr;
        const InventoryWindowInfo invalidCases[]={
            {-1,200,280,134,134,2,3,3,4},{100,-1,280,134,134,2,3,3,4},
            {1001,200,280,134,134,2,3,3,4},{100,600,280,134,134,2,3,3,4},
            {100,200,280,134,134,3,3,3,4},{100,200,280,134,134,-1,3,3,4},
            {100,200,280,134,134,2,4,3,4},{100,200,280,134,134,2,3,-1,4},
            {100,200,280,134,134,2,3,3,5}};
        for(const auto& bad:invalidCases) {
            InventoryGeometrySeed(bad);
            auto* owned=static_cast<UIItemWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_ITEMWND));
            InventoryGeometryRequire(owned!=nullptr,"invalid-record managed inventory construction failed");
            InventoryGeometryAssert(*owned,0,120,280,102,120,102,0,
                "invalid inventory record was not reset atomically to defaults");
            g_windowMgr.DeleteWindow(owned);g_windowMgr.m_itemWnd=nullptr;
        }
        InventoryWindowInfo boundary{1000,586,280,134,134,2,3,3,4};
        InventoryGeometrySeed(boundary);
        auto* exact=static_cast<UIItemWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_ITEMWND));
        InventoryGeometryRequire(exact!=nullptr,"exact-boundary managed inventory construction failed");
        InventoryGeometryAssert(*exact,1000,586,280,134,134,134,2,
            "exact viewport boundary record should remain valid");
        g_windowMgr.DeleteWindow(exact);g_windowMgr.m_itemWnd=nullptr;
        InventoryWindowInfo degenerate{0,0,-1,-1,-1,2,3,3,4};
        InventoryGeometrySeed(degenerate);
        auto* synthetic=static_cast<UIItemWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_ITEMWND));
        InventoryGeometryRequire(synthetic!=nullptr,"synthetic managed inventory construction failed");
        InventoryGeometryAssert(*synthetic,0,0,248,102,-1,102,2,
            "source-valid negative-dimension record no longer follows resize normalization");
        InventoryGeometryWriteResult(output,stage,*synthetic,"PASS");
        g_windowMgr.DeleteWindow(synthetic);g_windowMgr.m_itemWnd=original;
        return;
    } else if(stage=="mini") {
        InventoryWindowInfo mini{};
        mini.x=100;mini.y=600;mini.width=280;mini.height=17;mini.originalHeight=134;
        mini.tab=2;mini.radio1=3;mini.radio2=3;mini.radio3=4;
        InventoryGeometrySeed(mini);
        UIItemWnd* original=g_windowMgr.m_itemWnd;g_windowMgr.m_itemWnd=nullptr;
        auto* owned=static_cast<UIItemWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_ITEMWND));
        InventoryGeometryRequire(owned!=nullptr,"mini-record managed inventory construction failed");
        active=owned;
        InventoryGeometryRequire(adapter.syncGameplay(mode,RenderBackendType::LegacyDirect3D7,
            RenderBackendType::LegacyDirect3D7,0,0),"persisted mini Qt sync failed");Frames(app,view);
        InventoryGeometryAssert(*active,100,600,280,17,134,134,2,
            "valid persisted mini inventory record did not restore");
        InventoryGeometryCheckQml(view,100,600,280,17);
        auto* miniButton=Find(view.rootObject(),"commerceInventoryMini");
        InventoryGeometryRequire(miniButton&&miniButton->isVisible(),"QML inventory mini button is missing");
        Click(app,view,miniButton);
        InventoryGeometryRequire(adapter.syncGameplay(mode,RenderBackendType::LegacyDirect3D7,
            RenderBackendType::LegacyDirect3D7,0,0),"QML inventory mini expansion sync failed");Frames(app,view);
        InventoryGeometryRequire(!active->IsMiniMode()&&active->m_h==134,
            "QML inventory mini button did not expand the native inventory");
        InventoryGeometryCheckQml(view,100,600,280,134);
        miniButton=Find(view.rootObject(),"commerceInventoryMini");
        Click(app,view,miniButton);
        InventoryGeometryRequire(adapter.syncGameplay(mode,RenderBackendType::LegacyDirect3D7,
            RenderBackendType::LegacyDirect3D7,0,0),"QML inventory mini collapse sync failed");Frames(app,view);
        InventoryGeometryRequire(active->IsMiniMode()&&active->m_h==17,
            "QML inventory mini button did not collapse the native inventory");
        UIItemWnd::DisplayData display{};
        InventoryGeometryRequire(active->GetDisplayDataForQt(&display)&&display.displaySlots.empty(),"mini inventory still projects native item slots");
        InventoryGeometryRequire(VisibleBagTiles(view.rootObject())==0,"collapsed QML inventory still exposes item delegates");
        active->SendMsg(nullptr,6,136,0,0);
        InventoryGeometryAssert(*active,100,600,280,134,134,134,2,
            "mini button did not restore original inventory height");
        active->SendMsg(nullptr,6,136,0,0);
        InventoryGeometryAssert(*active,100,600,280,17,134,134,2,
            "mini button did not recapture full inventory height");
        InventoryGeometryRequire(view.grabWindow().save(output+"/inventory-geometry-mini.qml.png"),"save mini inventory QML frame failed");
        InventoryGeometryWriteResult(output,stage,*active,"PASS");
        g_windowMgr.DeleteWindow(owned);g_windowMgr.m_itemWnd=original;
        return;
    } else if(stage=="lifecycle") {
        // Source resize message 14 uses clamped input and signed truncation
        // toward zero before snapping the requested dimensions.
        struct Case { int requestWidth,requestHeight,width,height; };
        const Case cases[]={{231,102,248,102},{232,240,248,230},{279,151,280,134},
            {280,240,280,230},{300,151,280,134},{320,240,312,230},{321,241,312,230}};
        for(const Case& test:cases) {
            inventory.SendMsg(nullptr,14,7,test.requestWidth,test.requestHeight);
            InventoryGeometryRequire(inventory.m_w==test.width&&inventory.m_h==test.height,
                "inventory resize clamp/quantization table diverged from source");
        }
        inventory.SendMsg(nullptr,14,7,300,151);
        InventoryGeometryRequire(inventory.m_w==280&&inventory.m_h==134,
            "inventory resize message did not apply source clamp and snap");
        InventoryGeometryRequire(adapter.syncGameplay(mode,RenderBackendType::LegacyDirect3D7,
            RenderBackendType::LegacyDirect3D7,0,0),"resized inventory Qt sync failed");Frames(app,view);
        InventoryGeometryCheckQml(view,inventory.m_x,inventory.m_y,280,134);
        auto* resizeHandle=Find(view.rootObject(),"commerceInventoryResize");
        InventoryGeometryRequire(resizeHandle&&resizeHandle->isVisible(),"QML inventory resize handle is missing");
        const QPointF qmlStart=resizeHandle->mapToScene(QPointF(resizeHandle->width()/2,resizeHandle->height()/2));
        const QPointF qmlFinish=qmlStart+QPointF(32,32);
        const QPoint qmlStartPixel=qmlStart.toPoint(),qmlFinishPixel=qmlFinish.toPoint();
        QMouseEvent qmlPress(QEvent::MouseButtonPress,qmlStart,view.mapToGlobal(qmlStartPixel),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        QMouseEvent qmlMove(QEvent::MouseMove,qmlFinish,view.mapToGlobal(qmlFinishPixel),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        QMouseEvent qmlRelease(QEvent::MouseButtonRelease,qmlFinish,view.mapToGlobal(qmlFinishPixel),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
        QCoreApplication::sendEvent(&view,&qmlPress);QCoreApplication::sendEvent(&view,&qmlMove);
        QCoreApplication::sendEvent(&view,&qmlRelease);Frames(app,view);
        InventoryGeometryRequire(adapter.syncGameplay(mode,RenderBackendType::LegacyDirect3D7,
            RenderBackendType::LegacyDirect3D7,0,0),"QML resize gesture sync failed");Frames(app,view);
        InventoryGeometryRequire(inventory.m_w==312&&inventory.m_h==166,
            "real QML resize handle gesture did not resize native inventory");
        InventoryGeometryCheckQml(view,inventory.m_x,inventory.m_y,312,166);
        int savedX=-1,savedY=-1,savedWidth=-1,savedHeight=-1,savedOrg=-1,savedTab=-1;
        int savedRadio1=-1,savedRadio2=-1,savedRadio3=-1;
        InventoryGeometryRequire(TryLoadSettingsIniInt("UIWindows","ItemWndX",&savedX)
            &&TryLoadSettingsIniInt("UIWindows","ItemWndY",&savedY)
            &&TryLoadSettingsIniInt("UIWindows","ItemWndWidth",&savedWidth)
            &&TryLoadSettingsIniInt("UIWindows","ItemWndHeight",&savedHeight)
            &&TryLoadSettingsIniInt("UIWindows","ItemWndOrgHeight",&savedOrg)
            &&TryLoadSettingsIniInt("UIWindows","ItemWndTab",&savedTab)
            &&TryLoadSettingsIniInt("UIWindows","ItemWndRadio1",&savedRadio1)
            &&TryLoadSettingsIniInt("UIWindows","ItemWndRadio2",&savedRadio2)
            &&TryLoadSettingsIniInt("UIWindows","ItemWndRadio3",&savedRadio3)
            &&savedX==inventory.m_x&&savedY==inventory.m_y&&savedWidth==312&&savedHeight==166
            &&savedOrg==166&&savedTab==inventory.GetCurrentTab()
            &&savedRadio1==3&&savedRadio2==3&&savedRadio3==4,
            "QML resize release did not persist complete inventory record");
        const int nativeStartX=inventory.m_x+inventory.m_w-8,nativeStartY=inventory.m_y+inventory.m_h-8;
        inventory.OnLBtnDown(nativeStartX,nativeStartY);
        inventory.OnMouseMove(nativeStartX+32,nativeStartY+32);
        inventory.OnLBtnUp(nativeStartX+32,nativeStartY+32);
        InventoryGeometryRequire(inventory.m_w==312&&inventory.m_h==198,
            "native resize handle gesture did not resize inventory");
        inventory.SendMsg(nullptr,6,136,0,0);
        InventoryGeometryRequire(inventory.IsMiniMode()&&inventory.m_h==17,
            "inventory mini button did not collapse to 17 pixels");
        inventory.SendMsg(nullptr,6,136,0,0);
        InventoryGeometryRequire(inventory.m_h==198&&inventory.m_w==312,"inventory mini button did not preserve resized width and full height");
        InventoryGeometryRequire(adapter.syncGameplay(mode,RenderBackendType::LegacyDirect3D7,
            RenderBackendType::LegacyDirect3D7,0,0),"native resize gesture sync failed");Frames(app,view);
        InventoryGeometryCheckQml(view,inventory.m_x,inventory.m_y,312,198);
        InventoryGeometryRequire(view.grabWindow().save(output+"/inventory-geometry-resized.qml.png"),"save resized inventory QML frame failed");
        Hover(inventory);
        InventoryGeometryRequire(Native(inventory,0xffffffffu,1280,720).save(output+"/inventory-geometry-resized.native.png"),"save resized inventory native frame failed");

        UIItemWnd* original=g_windowMgr.m_itemWnd;g_windowMgr.m_itemWnd=nullptr;
        auto* owned=static_cast<UIItemWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_ITEMWND));
        InventoryGeometryRequire(owned!=nullptr,"managed inventory construction failed");
        owned->SendMsg(nullptr,14,7,300,151);owned->Move(20,30);
        owned->SetCurrentTab(2);owned->SendMsg(nullptr,6,136,0,0);
        owned->SendMsg(nullptr,6,135,0,0);
        InventoryGeometryRequire(g_windowMgr.m_itemWnd==nullptr,"native close did not release managed inventory");
        auto* recreated=static_cast<UIItemWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_ITEMWND));
        InventoryGeometryRequire(recreated!=nullptr,"managed inventory recreation failed");
        InventoryGeometryRequire(recreated->m_x==20&&recreated->m_y==30&&recreated->m_w==280
            &&recreated->m_h==17&&recreated->GetRestoreHeight()==134&&recreated->GetCurrentTab()==2,
            "managed inventory recreation did not restore stored geometry, mini state, and tab");
        InventoryGeometryRequire(adapter.syncGameplay(mode,RenderBackendType::LegacyDirect3D7,
            RenderBackendType::LegacyDirect3D7,0,0),"managed mini recreation sync failed");Frames(app,view);
        InventoryGeometryRequire(view.grabWindow().save(output+"/inventory-geometry-lifecycle.qml.png"),"save lifecycle inventory QML frame failed");
        InventoryGeometryWriteResult(output,stage,*recreated,"PASS");
        Click(app,view,Find(view.rootObject(),"commerceInventoryClose"));
        InventoryGeometryRequire(g_windowMgr.m_itemWnd==nullptr,"actual QML close did not release managed inventory");
        g_windowMgr.m_itemWnd=original;
        return;
    } else {
        throw std::runtime_error("unknown inventory geometry child stage");
    }
    InventoryGeometryWriteResult(output,stage,*active,"PASS");
}

void CheckInventoryGeometryDriver(const QString& output)
{
    const QString root=QDir::cleanPath(QDir(output).absolutePath()+QString("/item-geometry-%1-%2")
        .arg(QCoreApplication::applicationPid()).arg(QDateTime::currentMSecsSinceEpoch()));
    const QStringList stages={"defaults","seed-warm","warm","small","invalid","mini","lifecycle"};
    QJsonArray children;QSet<qint64> pids;
    for(const QString& stage:stages) {
        const QString group=(stage=="seed-warm"||stage=="warm"||stage=="small")?"warm":stage;
        const QString groupDir=root+"/"+group;
        const QString childOut=groupDir+"/"+stage;
        InventoryGeometryRequire(QDir().mkpath(childOut),"create inventory geometry child output failed");
        InventoryGeometryRequire(QDir().mkpath(groupDir+"/settings"),"create inventory geometry private settings failed");
        QProcess process;QProcessEnvironment env=QProcessEnvironment::systemEnvironment();
        env.insert("OPEN_MIDGARD_DATA_DIR",groupDir+"/settings");process.setProcessEnvironment(env);
        process.start(QCoreApplication::applicationFilePath(),{childOut,"item-geometry-child",stage});
        InventoryGeometryRequire(process.waitForStarted(5000),"inventory geometry child did not start");
        if(!process.waitForFinished(20000)){process.kill();process.waitForFinished(1000);throw std::runtime_error("inventory geometry child exceeded 20 second limit");}
        const QByteArray stderrBytes=process.readAllStandardError(),stdoutBytes=process.readAllStandardOutput();
        if(process.exitStatus()!=QProcess::NormalExit||process.exitCode()!=0) {
            const QByteArray clipped=(stderrBytes+" "+stdoutBytes).right(600);
            throw std::runtime_error(("inventory geometry child "+stage.toStdString()+" failed: "+clipped.toStdString()).c_str());
        }
        QFile resultFile(childOut+"/inventory-geometry.json");
        InventoryGeometryRequire(resultFile.open(QIODevice::ReadOnly),"inventory geometry child result missing");
        QJsonParseError error{};const auto document=QJsonDocument::fromJson(resultFile.readAll(),&error);
        InventoryGeometryRequire(error.error==QJsonParseError::NoError&&document.isObject(),"inventory geometry child result invalid");
        QJsonObject result=document.object();
        InventoryGeometryRequire(result.value("stage").toString()==stage,"inventory geometry child stage mismatch");
        InventoryGeometryRequire(result.value("pid").toInteger()>0&&result.value("pid").toInteger()!=QCoreApplication::applicationPid(),
            "inventory geometry persistence did not launch a fresh process");
        pids.insert(result.value("pid").toInteger());
        InventoryGeometryRequire(result.value("outcome").toString()==(stage.startsWith("seed-")?"SEEDED":"PASS"),
            "inventory geometry child reported a failed stage");
        result.insert("output_directory",childOut);children.push_back(result);
    }
    QFile result(output+"/inventory-geometry-results.json");
    InventoryGeometryRequire(result.open(QIODevice::WriteOnly),"open inventory geometry summary failed");
    result.write(QJsonDocument(QJsonObject{{"fresh_processes",stages.size()},{"observed_distinct_pids",pids.size()},
        {"children",children},{"outcome","PASS"}}).toJson(QJsonDocument::Indented));
}
