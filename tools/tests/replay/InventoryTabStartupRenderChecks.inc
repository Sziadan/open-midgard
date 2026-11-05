// Fresh-process selected startup policy and in-process tab retention. Included by
// ItemColourRenderChecks.cpp so the child uses the same real QML and native
// inventory fixtures as the other rendering checks.
#include "core/SettingsIni.h"
#include <QProcess>
#include <QProcessEnvironment>
#include <QJsonParseError>
#include <QDateTime>
#include <QSet>
#include <QFileInfo>

struct InventoryTabViewportFixture {
#if RO_PLATFORM_WINDOWS
    HWND previous=g_hMainWnd;
    HWND window=nullptr;
    void Create(const QString& stage) {
        const int width=stage=="read-small"?640:stage=="read-boundary"?1024:1280;
        const int height=stage=="read-small"?480:stage=="read-boundary"?648:720;
        window=CreateWindowExA(0,"STATIC","inventory-tab-fixture",WS_POPUP,0,0,width,height,nullptr,nullptr,GetModuleHandleA(nullptr),nullptr);
        Require(window!=nullptr,"hidden inventory viewport fixture failed");g_hMainWnd=window;
    }
    ~InventoryTabViewportFixture(){if(window){g_hMainWnd=previous;DestroyWindow(window);}}
#else
    void Create(const QString&) {} // Existing logical-client fallback is640x480.
#endif
};

namespace {
void InventoryTabStartupRequire(bool ok,const char* message)
{
    if(!ok)throw std::runtime_error(message);
}

void InventoryTabWriteStageResult(const QString& output,const QString& stage,int tab,
    const QVariantList& rows,const QString& outcome)
{
    QJsonArray ids;
    for(const auto& row:rows)ids.push_back(int(row.toMap().value("id").toUInt()));
    QJsonObject result{{"pid",qint64(QCoreApplication::applicationPid())},
        {"stage",stage},{"tab",tab},{"inventory_ids",ids},{"outcome",outcome}};
    QFile file(output+"/inventory-tab-startup.json");
    InventoryTabStartupRequire(file.open(QIODevice::WriteOnly),"open inventory-tab child result failed");
    file.write(QJsonDocument(result).toJson(QJsonDocument::Compact));
}

QVariantList InventoryTabStartupRows(QtUiState* state)
{
    return state->commerceData().value("inventoryItems").toList();
}

void InventoryTabStartupSetNative(UIItemWnd& inventory,int tab)
{
    UIItemWnd::QtButtonDisplay button{};
    InventoryTabStartupRequire(inventory.GetQtTabDisplayForQt(tab,&button)&&button.visible,
        "native inventory tab button unavailable");
    const int x=button.x+button.width/2,y=button.y+button.height/2;
    inventory.OnLBtnDown(x,y);inventory.OnLBtnUp(x,y);
}

void InventoryTabStartupFixture(CGameMode& mode)
{
    g_session.ClearInventoryItems();
    std::vector<u8> normal(48,0);normal[0]=0xe8;normal[1]=2;normal[2]=48;
    const auto put16=[](std::vector<u8>& bytes,size_t at,u16 value){bytes[at]=u8(value);bytes[at+1]=u8(value>>8);};
    for(int ordinal=0;ordinal<2;++ordinal) {
        const size_t at=4+22*size_t(ordinal);
        put16(normal,at,u16(2+ordinal));put16(normal,at+2,ordinal==0?501:4001);
        normal[at+4]=ordinal==0?0:6;normal[at+5]=1;put16(normal,at+6,1);
    }
    Dispatch(mode,normal);Dispatch(mode,Equipment(0xd0,true,0));
}

bool InventoryTabStartupIdsEqual(const QVariantList& rows,const std::set<unsigned>& expected)
{
    std::set<unsigned> ids;
    for(const auto& row:rows)ids.insert(row.toMap().value("id").toUInt());
    return ids==expected;
}

void InventoryTabWriteSeedSetting(const QString& stage)
{
    QFile file(QString::fromStdString(GetOpenMidgardIniPath().string()));
    InventoryTabStartupRequire(file.open(QIODevice::WriteOnly|QIODevice::Truncate),"open private startup settings failed");
    QByteArray values="[UIWindows]\nItemWndTab=";
    if(stage=="seed-negative")values+="-1\n";
    else if(stage=="seed-three")values+="3\n";
    else if(stage=="seed-large")values+="2147483647\n";
    else if(stage=="seed-text")values+="nope\n";
    else if(stage=="seed-overflow")values+="999999999999999999999999999999999999\n";
    else if(stage=="seed-radio-negative")values+="2\nItemWndRadio1=-1\n";
    else if(stage=="seed-radio1")values+="2\nItemWndRadio1=4\n";
    else if(stage=="seed-radio2")values+="2\nItemWndRadio2=4\n";
    else if(stage=="seed-radio3")values+="2\nItemWndRadio3=5\n";
    else if(stage=="seed-good")values+="2\nItemWndRadio1=3\nItemWndRadio2=3\nItemWndRadio3=4\n";
    else throw std::runtime_error("unknown inventory startup seed");
    file.write(values);
}
}

void CheckInventoryTabStartupChild(QGuiApplication& app,QQuickView& view,QtUiState* uiState,
    CGameMode& mode,UIItemWnd& inventory,QtUiStateAdapter& adapter,const QString& output,const QString& stage)
{
    InventoryTabStartupRequire(uiState,"inventory persistence Qt state missing");
    const QString actualSettings=QDir::cleanPath(QFileInfo(QString::fromStdString(GetOpenMidgardIniPath().string())).absolutePath());
    const QString privateSettings=QDir::cleanPath(QFileInfo(output).dir().absoluteFilePath("settings"));
    InventoryTabStartupRequire(actualSettings==privateSettings,"inventory persistence child requires its private sibling settings directory");
    if(g_windowMgr.m_storageWnd)g_windowMgr.m_storageWnd->SetShow(0);
    UIStorageWnd::DisplayData storage{};
    InventoryTabStartupRequire(g_windowMgr.m_storageWnd
        &&g_windowMgr.m_storageWnd->GetDisplayDataForQt(&storage)&&storage.currentTab==0,
        "fresh storage unexpectedly restored the inventory tab preference");
    ro::commerce::state.cart.visible=false;
    InventoryTabStartupFixture(mode);
    InventoryTabStartupRequire(adapter.syncGameplay(mode,RenderBackendType::LegacyDirect3D7,
        RenderBackendType::LegacyDirect3D7,0,0),"inventory persistence initial Qt sync failed");
    Frames(app,view);
    auto expectedIdsForTab=[](int tab) {
        if(tab==0)return std::set<unsigned>{501};
        if(tab==1)return std::set<unsigned>{1201};
        return std::set<unsigned>{4001};
    };
    auto verify=[&](int tab) {
        UIItemWnd::DisplayData native{};
        InventoryTabStartupRequire(inventory.GetDisplayDataForQt(&native),"native inventory persistence projection failed");
        InventoryTabStartupRequire(inventory.GetCurrentTab()==tab,"native inventory tab differs from persisted value");
        InventoryTabStartupRequire(native.currentTab==tab,"native inventory display tab differs from persisted value");
        InventoryTabStartupRequire(ro::commerce::state.inventoryTab==tab,"commerce inventory tab differs from persisted value");
        InventoryTabStartupRequire(uiState->inventoryTab()==tab,"Qt inventory tab differs from persisted value");
        const auto rows=InventoryTabStartupRows(uiState);
        InventoryTabStartupRequire(InventoryTabStartupIdsEqual(rows,expectedIdsForTab(tab)),
            "Qt filtered inventory rows do not match persisted tab");
        std::set<unsigned> nativeIds;
        for(const auto& slot:native.displaySlots)if(slot.occupied)nativeIds.insert(slot.itemId);
        InventoryTabStartupRequire(nativeIds==expectedIdsForTab(tab),
            "native filtered inventory slots do not match persisted tab");
        auto* qmlTab=Find(view.rootObject(),QString("commerceInventoryTab_%1").arg(tab));
        InventoryTabStartupRequire(qmlTab&&qmlTab->property("primary").toBool(),
            "actual QML inventory tab is not selected");
        InventoryTabStartupRequire(VisibleBagTiles(view.rootObject())==rows.size(),
            "actual QML inventory delegates do not match persisted tab");
        return rows;
    };

    if(stage.startsWith("seed-")) {
        InventoryTabWriteSeedSetting(stage);
        InventoryTabWriteStageResult(output,stage,inventory.GetCurrentTab(),{},"SEEDED");
        return;
    }
    if(stage=="select-native-2") {
        InventoryTabStartupSetNative(inventory,2);
        InventoryTabStartupRequire(inventory.GetCurrentTab()==2,"native tab click did not select tab two");
        InventoryTabStartupRequire(adapter.syncGameplay(mode,RenderBackendType::LegacyDirect3D7,
            RenderBackendType::LegacyDirect3D7,0,0),"native inventory tab sync failed");Frames(app,view);
        verify(2);
        InventoryTabStartupRequire(view.grabWindow().save(output+"/inventory-tab-selected-2.png"),
            "save inventory tab two QML frame failed");
        InventoryTabStartupRequire(QFile::exists(QString::fromStdString(GetOpenMidgardIniPath().string())),
            "inventory tab preference was not written to private settings");
        inventory.SendMsg(nullptr,6,135,0,0);
        int stored=-1;InventoryTabStartupRequire(!inventory.m_show
            &&TryLoadSettingsIniInt("UIWindows","ItemWndTab",&stored)&&stored==2,"native close did not persist the selected tab");
        InventoryTabWriteStageResult(output,stage,2,InventoryTabStartupRows(uiState),"PASS");return;
    }
    if(stage=="select-managed-2") {
        UIItemWnd* original=g_windowMgr.m_itemWnd;g_windowMgr.m_itemWnd=nullptr;
        auto* owned=static_cast<UIItemWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_ITEMWND));
        InventoryTabStartupRequire(owned,"managed inventory construction failed");owned->SetCurrentTab(2);
        g_windowMgr.DeleteWindow(owned);
        int stored=-1;InventoryTabStartupRequire(TryLoadSettingsIniInt("UIWindows","ItemWndTab",&stored)&&stored==2,
            "managed inventory deletion did not store its selected tab");
        SaveSettingsIniInt("UIWindows","ItemWndTab",0);
        auto* recreated=static_cast<UIItemWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_ITEMWND));
        InventoryTabStartupRequire(recreated&&recreated->GetCurrentTab()==2,"same-process recreation reloaded a disk tab instead of manager state");
        g_windowMgr.DeleteWindow(recreated);
        g_windowMgr.m_itemWnd=original;inventory.SetCurrentTab(2);
        InventoryTabWriteStageResult(output,stage,2,{},"PASS");return;
    }
    if(stage=="select-qml-0") {
        auto* qmlTab=Find(view.rootObject(),"commerceInventoryTab_0");
        Click(app,view,qmlTab);
        InventoryTabStartupRequire(adapter.syncGameplay(mode,RenderBackendType::LegacyDirect3D7,
            RenderBackendType::LegacyDirect3D7,0,0),"inventory tab one Qt sync failed");
        Frames(app,view);verify(0);
        InventoryTabStartupRequire(view.grabWindow().save(output+"/inventory-tab-selected-0.png"),
            "save inventory tab one QML frame failed");
        // Exercise the application's real close command; SetShow(0) must own
        // persistence of the native placement and selected tab.
        ro::commerce::Command close;close.action="close";close.to="inventory";
        InventoryTabStartupRequire(ro::commerce::Action(close),"real inventory close action failed");
        InventoryTabStartupRequire(!inventory.m_show,"inventory close action left native window visible");
        InventoryTabWriteStageResult(output,stage,0,InventoryTabStartupRows(uiState),"PASS");return;
    }
    InventoryTabStartupRequire(stage.startsWith("read-"),"unknown inventory startup read stage");
    int expected=0;
#if RO_PLATFORM_WINDOWS
    if(stage!="read-default"&&stage!="read-small"&&!stage.startsWith("read-radio-"))expected=1;
#endif
    const auto rows=verify(expected);
    InventoryTabStartupRequire(view.grabWindow().save(output+QString("/inventory-tab-%1.png").arg(expected)),
        "save fresh-process inventory tab frame failed");
    if(stage=="read-after-native") {
        Hover(inventory);const QImage native=Native(inventory,0xffffffffu);
        InventoryTabStartupRequire(native.save(output+"/inventory-tab-startup-native.png"),"save fresh-process native tab frame failed");
    }
    InventoryTabWriteStageResult(output,stage,expected,rows,"PASS");
}

void CheckInventoryTabStartupDriver(const QString& output)
{
    const QString root=QDir::cleanPath(QDir(output).absolutePath()+QString("/item-tabs-startup-%1-%2")
        .arg(QCoreApplication::applicationPid()).arg(QDateTime::currentMSecsSinceEpoch()));
    const QString settings=root+"/settings";
    InventoryTabStartupRequire(QDir().mkpath(settings),"create private inventory persistence settings directory failed");
    const QStringList stages={"read-default","select-native-2","read-after-native","select-qml-0","read-after-qml","select-managed-2","read-after-managed",
        "seed-negative","read-seed-negative","seed-three","read-seed-three","seed-large","read-seed-large",
        "seed-text","read-seed-text","seed-overflow","read-seed-overflow",
        "seed-radio-negative","read-radio-negative","seed-radio1","read-radio-one","seed-radio2","read-radio-two","seed-radio3","read-radio-three",
        "seed-good","read-good","read-small","read-boundary"};
    QJsonArray children;QSet<qint64> pids;
    for(const QString& stage:stages) {
        const QString childOut=root+"/"+stage;
        InventoryTabStartupRequire(QDir().mkpath(childOut),"create inventory persistence child output failed");
        QProcess process;QProcessEnvironment env=QProcessEnvironment::systemEnvironment();
        env.insert("OPEN_MIDGARD_DATA_DIR",settings);process.setProcessEnvironment(env);
        process.start(QCoreApplication::applicationFilePath(),{childOut,"item-tabs-startup-child",stage});
        InventoryTabStartupRequire(process.waitForStarted(5000),"inventory persistence child did not start");
        if(!process.waitForFinished(20000)) {process.kill();process.waitForFinished(1000);throw std::runtime_error("inventory persistence child exceeded 20 second limit");}
        const QByteArray stderrBytes=process.readAllStandardError();
        const QByteArray stdoutBytes=process.readAllStandardOutput();
        if(process.exitStatus()!=QProcess::NormalExit||process.exitCode()!=0) {
            const QByteArray clipped=(stderrBytes+" "+stdoutBytes).right(600);
            throw std::runtime_error(("inventory persistence child "+stage.toStdString()+" failed: "+clipped.toStdString()).c_str());
        }
        QFile resultFile(childOut+"/inventory-tab-startup.json");
        InventoryTabStartupRequire(resultFile.open(QIODevice::ReadOnly),"inventory persistence child result missing");
        QJsonParseError parse{};const auto doc=QJsonDocument::fromJson(resultFile.readAll(),&parse);
        InventoryTabStartupRequire(parse.error==QJsonParseError::NoError&&doc.isObject(),"inventory persistence child result invalid JSON");
        auto result=doc.object();InventoryTabStartupRequire(result.value("stage").toString()==stage,
            "inventory persistence child stage mismatch");
        InventoryTabStartupRequire(result.value("pid").toInteger()>0
            &&result.value("pid").toInteger()!=QCoreApplication::applicationPid(),"inventory persistence did not launch a child process");
        pids.insert(result.value("pid").toInteger());
        if(stage.startsWith("read-")) {
            InventoryTabStartupRequire(result.value("outcome").toString()=="PASS","fresh process inventory tab verification failed");
        }
        result.insert("output_directory",childOut);children.push_back(result);
    }
    QFile result(output+"/inventory-tab-startup-results.json");
    InventoryTabStartupRequire(result.open(QIODevice::WriteOnly),"open inventory persistence summary failed");
    result.write(QJsonDocument(QJsonObject{{"settings_directory",settings},{"fresh_processes",stages.size()},{"observed_distinct_pids",pids.size()},
        {"children",children},{"outcome","PASS"}}).toJson(QJsonDocument::Indented));
}
