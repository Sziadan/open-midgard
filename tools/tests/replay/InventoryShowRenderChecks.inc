// Ref-derived remembered inventory visibility and map teardown replay checks.
#include <QProcess>
#include <QProcessEnvironment>
#include <QJsonParseError>
#include <QDateTime>
#include <QFileInfo>

namespace {
void InventoryShowRequire(bool ok,const char* message)
{
    if(!ok)throw std::runtime_error(message);
}

void InventoryShowWriteResult(const QString& output,const QString& stage,const QString& outcome)
{
    const auto info=g_windowMgr.GetInventoryWindowInfo();
    const auto* inventory=g_windowMgr.m_itemWnd;
    QFile file(output+"/inventory-show.json");
    InventoryShowRequire(file.open(QIODevice::WriteOnly),"open inventory show child result failed");
    QJsonObject result{{"pid",qint64(QCoreApplication::applicationPid())},{"stage",stage},
        {"remembered_show",g_windowMgr.GetInventoryWindowShowState()},
        {"visible",inventory&&inventory->m_show!=0},{"x",inventory?inventory->m_x:0},
        {"y",inventory?inventory->m_y:0},{"width",inventory?inventory->m_w:0},
        {"height",inventory?inventory->m_h:0},{"record_x",info.x},{"record_y",info.y},
        {"record_width",info.width},{"record_height",info.height},{"record_tab",info.tab},
        {"outcome",outcome}};
    file.write(QJsonDocument(result).toJson(QJsonDocument::Compact));
}

void InventoryShowSync(QGuiApplication& app,QQuickView& view,CGameMode& mode,QtUiStateAdapter& adapter)
{
    InventoryShowRequire(adapter.syncGameplay(mode,RenderBackendType::LegacyDirect3D7,
        RenderBackendType::LegacyDirect3D7,0,0),"inventory show Qt sync failed");
    Frames(app,view);
}

void InventoryShowCheckGeometry(const UIItemWnd& inventory,int x,int y,int width,int height,int tab)
{
    InventoryShowRequire(inventory.m_x==x&&inventory.m_y==y&&inventory.m_w==width
        &&inventory.m_h==height&&inventory.GetCurrentTab()==tab,
        "inventory show lifecycle did not restore cached geometry and tab");
}
}

void CheckInventoryShowChild(QGuiApplication& app,QQuickView& view,QtUiState* state,
    CGameMode& mode,UIItemWnd& fixtureInventory,QtUiStateAdapter& adapter,const QString& output,
    const QString& stage)
{
    InventoryShowRequire(state,"inventory show Qt state missing");
    const QString actual=QDir::cleanPath(QFileInfo(QString::fromStdString(GetOpenMidgardIniPath().string())).absolutePath());
    const QString expected=QDir::cleanPath(QFileInfo(output).dir().absoluteFilePath("settings"));
    InventoryShowRequire(actual==expected,"inventory show child requires private sibling settings");

    if(stage=="seed-warm") {
        SaveSettingsIniInt("UIWindows","ItemWndShow",1);
        InventoryShowWriteResult(output,stage,"SEEDED");
        return;
    }

    // The test harness begins with a stack inventory. Detach it before using
    // manager-owned windows or bulk map teardown, which deletes owned children.
    fixtureInventory.SetShow(0);
    if(g_windowMgr.m_itemWnd==&fixtureInventory)g_windowMgr.m_itemWnd=nullptr;
    g_windowMgr.m_storageWnd=nullptr;
    ro::commerce::state.cart.visible=false;
    InventoryShowRequire(!g_windowMgr.GetInventoryWindowShowState(),
        "inventory startup show state must begin false even when the saved value is true");

    if(stage=="cold"||stage=="warm") {
        g_windowMgr.MakeWindow(UIWindowMgr::WID_BASICINFOWND);
        InventoryShowRequire(!g_windowMgr.m_itemWnd,"basic UI opened inventory from a cold or saved show bit");
        auto* opened=static_cast<UIItemWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_ITEMWND));
        InventoryShowRequire(opened&&opened==g_windowMgr.m_itemWnd&&opened->m_show,
            "inventory MakeWindow did not create a visible managed window");
        InventoryShowRequire(g_windowMgr.GetInventoryWindowShowState(),
            "opening inventory did not remember visible state");
        InventoryShowSync(app,view,mode,adapter);
        if(stage=="cold") InventoryShowCheckGeometry(*opened,0,120,280,102,0);
        else InventoryShowCheckGeometry(*opened,744,514,280,134,1);
        auto* panel=Find(view.rootObject(),"commerce_inventory");
        InventoryShowRequire(panel&&panel->isVisible(),"actual QML inventory panel is missing or hidden");
        InventoryShowWriteResult(output,stage,"PASS");
        opened->SetShow(0);g_windowMgr.RemoveAllWindows();
        return;
    }

    InventoryShowRequire(stage=="lifecycle","unknown inventory show stage");
    InventoryTabStartupFixture(mode);
    auto* owned=static_cast<UIItemWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_ITEMWND));
    InventoryShowRequire(owned&&g_windowMgr.GetInventoryWindowShowState(),"managed inventory open failed");
    owned->Move(100,80);owned->ResizeInventory(280,166);owned->SetCurrentTab(2);
    InventoryShowCheckGeometry(*owned,100,80,280,166,2);

    // Ordinary hide persists the record but leaves the remembered show bit set;
    // MakeWindow must show and reuse the same object.
    owned->SetShow(0);
    InventoryShowRequire(g_windowMgr.GetInventoryWindowShowState()&&!owned->m_show,
        "ordinary inventory hide cleared remembered show state");
    auto* reused=static_cast<UIItemWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_ITEMWND));
    InventoryShowRequire(reused==owned&&reused->m_show,"hidden inventory was not reused and shown");
    InventoryShowCheckGeometry(*reused,100,80,280,166,2);
    reused->SendMsg(nullptr,6,136,0,0);
    InventoryShowRequire(reused->IsMiniMode()&&reused->m_h==17,"inventory did not collapse to mini height");
    InventoryShowSync(app,view,mode,adapter);
    InventoryGeometryCheckQml(view,100,80,280,17);
    InventoryShowRequire(view.grabWindow().save(output+"/inventory-show-mini.qml.png"),"save mini inventory show frame failed");
    reused->SendMsg(nullptr,6,136,0,0);
    InventoryShowRequire(!reused->IsMiniMode()&&reused->m_h==166,"inventory did not restore full height");

    // Map teardown stores and destroys managed children while preserving the
    // remembered bit; MakeWindow(BASICINFO) recreates inventory from that bit.
    reused->SetShow(0);g_windowMgr.m_storageWnd=nullptr;g_windowMgr.RemoveAllWindows();
    InventoryShowRequire(!g_windowMgr.m_itemWnd&&g_windowMgr.GetInventoryWindowShowState(),
        "bulk teardown did not destroy inventory while preserving remembered show state");
    g_windowMgr.MakeWindow(UIWindowMgr::WID_BASICINFOWND);
    auto* restored=g_windowMgr.m_itemWnd;
    InventoryShowRequire(restored&&restored->m_show,"basic-info recreation did not reopen remembered inventory");
    InventoryShowCheckGeometry(*restored,100,80,280,166,2);
    InventoryShowSync(app,view,mode,adapter);
    InventoryGeometryCheckQml(view,100,80,280,166);
    InventoryShowRequire(InventoryTabStartupIdsEqual(InventoryTabStartupRows(state),{4001}),
        "UI teardown lost the selected decoded inventory row or restored the wrong tab");
    auto* cardTile=Find(view.rootObject(),"commerceTile_inventory_3");
    InventoryShowRequire(cardTile&&cardTile->isVisible()&&cardTile->property("item").toMap().value("id").toUInt()==4001,
        "actual QML restored inventory row is absent or has the wrong identity");
    InventoryShowRequire(view.grabWindow().save(output+"/inventory-show-restored.qml.png"),"save restored inventory show frame failed");
    Hover(*restored);
    const auto nativeImage=Native(*restored,0xffffffffu,1280,720);
    InventoryShowRequire(nativeImage.save(output+"/inventory-show-restored.native.png"),"save native restored inventory frame failed");

    // Repeated teardown/recreation is stable, and an explicit close clears the
    // remembered bit so the next basic-info window does not reopen inventory.
    restored->SetShow(0);g_windowMgr.m_storageWnd=nullptr;g_windowMgr.RemoveAllWindows();
    InventoryShowRequire(g_windowMgr.GetInventoryWindowShowState(),"repeat bulk teardown cleared remembered state");
    g_windowMgr.MakeWindow(UIWindowMgr::WID_BASICINFOWND);
    auto* repeated=g_windowMgr.m_itemWnd;
    InventoryShowRequire(repeated&&repeated->m_show,"repeat basic-info recreation failed");
    InventoryShowSync(app,view,mode,adapter);
    auto* closeButton=Find(view.rootObject(),"commerceInventoryClose");
    InventoryShowRequire(closeButton&&closeButton->isVisible(),"actual QML inventory close button is missing");
    Click(app,view,closeButton);
    InventoryShowRequire(!g_windowMgr.m_itemWnd&&!g_windowMgr.GetInventoryWindowShowState(),
        "explicit inventory close did not delete and clear remembered state");
    g_windowMgr.m_storageWnd=nullptr;g_windowMgr.RemoveAllWindows();
    g_windowMgr.MakeWindow(UIWindowMgr::WID_BASICINFOWND);
    InventoryShowRequire(!g_windowMgr.m_itemWnd,"basic-info recreation reopened explicitly closed inventory");

    // The source basic-info button/Alt-E path toggles ownership, including a
    // retained hidden pointer. Do not compare allocator addresses after delete.
    InventoryShowRequire(g_windowMgr.ToggleWindow(UIWindowMgr::WID_ITEMWND)&&g_windowMgr.m_itemWnd,
        "inventory basic-info toggle did not open inventory");
    InventoryShowRequire(!g_windowMgr.ToggleWindow(UIWindowMgr::WID_ITEMWND)&&!g_windowMgr.m_itemWnd,
        "inventory basic-info toggle did not delete owned inventory");
    InventoryShowRequire(g_windowMgr.ToggleWindow(UIWindowMgr::WID_ITEMWND)&&g_windowMgr.m_itemWnd,
        "inventory basic-info toggle did not reopen inventory");
    g_windowMgr.m_itemWnd->SetShow(0);
    InventoryShowRequire(!g_windowMgr.ToggleWindow(UIWindowMgr::WID_ITEMWND)&&!g_windowMgr.m_itemWnd,
        "inventory basic-info toggle did not delete a hidden retained object");
    InventoryShowRequire(g_windowMgr.ToggleWindow(UIWindowMgr::WID_ITEMWND)&&g_windowMgr.m_itemWnd,
        "inventory basic-info toggle did not recreate after hidden deletion");
    InventoryShowSync(app,view,mode,adapter);
    auto* panel=Find(view.rootObject(),"commerce_inventory");
    InventoryShowRequire(panel&&panel->isVisible(),"QML inventory projection was not restored after toggle");
    InventoryShowWriteResult(output,stage,"PASS");
    g_windowMgr.m_itemWnd->SetShow(0);g_windowMgr.m_storageWnd=nullptr;g_windowMgr.RemoveAllWindows();
}

void CheckInventoryShowDriver(const QString& output)
{
    const QString root=QDir::cleanPath(QDir(output).absolutePath()+QString("/inventory-show-%1-%2")
        .arg(QCoreApplication::applicationPid()).arg(QDateTime::currentMSecsSinceEpoch()));
    const QStringList stages={"cold","seed-warm","warm","lifecycle"};
    QJsonArray children;QSet<qint64> pids;
    for(const QString& stage:stages) {
        const QString group=(stage=="warm"||stage=="seed-warm")?"warm":stage;
        const QString groupDir=root+"/"+group;
        const QString childOut=groupDir+"/"+stage;
        InventoryShowRequire(QDir().mkpath(childOut),"create inventory show child output failed");
        InventoryShowRequire(QDir().mkpath(groupDir+"/settings"),"create inventory show private settings failed");
        QProcess process;QProcessEnvironment env=QProcessEnvironment::systemEnvironment();
        env.insert("OPEN_MIDGARD_DATA_DIR",groupDir+"/settings");process.setProcessEnvironment(env);
        process.start(QCoreApplication::applicationFilePath(),{childOut,"inventory-show-child",stage});
        InventoryShowRequire(process.waitForStarted(5000),"inventory show child did not start");
        if(!process.waitForFinished(20000)){process.kill();process.waitForFinished(1000);throw std::runtime_error("inventory show child exceeded 20 second limit");}
        const QByteArray stderrBytes=process.readAllStandardError(),stdoutBytes=process.readAllStandardOutput();
        if(process.exitStatus()!=QProcess::NormalExit||process.exitCode()!=0) {
            const QByteArray clipped=(stderrBytes+" "+stdoutBytes).right(600);
            throw std::runtime_error(("inventory show child "+stage.toStdString()+" failed: "+clipped.toStdString()).c_str());
        }
        QFile resultFile(childOut+"/inventory-show.json");
        InventoryShowRequire(resultFile.open(QIODevice::ReadOnly),"inventory show child result missing");
        QJsonParseError error{};const auto document=QJsonDocument::fromJson(resultFile.readAll(),&error);
        InventoryShowRequire(error.error==QJsonParseError::NoError&&document.isObject(),"inventory show child result invalid JSON");
        QJsonObject result=document.object();
        InventoryShowRequire(result.value("stage").toString()==stage,"inventory show child stage mismatch");
        InventoryShowRequire(result.value("pid").toInteger()>0&&result.value("pid").toInteger()!=QCoreApplication::applicationPid(),
            "inventory show replay did not launch a fresh process");
        pids.insert(result.value("pid").toInteger());
        InventoryShowRequire(result.value("outcome").toString()==(stage=="seed-warm"?"SEEDED":"PASS"),
            "inventory show child reported a failed stage");
        result.insert("output_directory",childOut);children.push_back(result);
    }
    QFile result(output+"/inventory-show-results.json");
    InventoryShowRequire(result.open(QIODevice::WriteOnly),"open inventory show summary failed");
    result.write(QJsonDocument(QJsonObject{{"fresh_processes",stages.size()},{"observed_distinct_pids",pids.size()},
        {"children",children},{"outcome","PASS"}}).toJson(QJsonDocument::Indented));
}
