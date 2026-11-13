#include "ui/UINotifyItemObtainWnd.h"
#include <QFont>
#include <QFontMetrics>
#include <functional>

void CheckItemObtainUi(QGuiApplication& app,QQuickView& view,QtUiState* state,CGameMode& mode,
    UIItemWnd& inventory,QtUiStateAdapter& adapter,const QString& output)
{
    Require(state!=nullptr,"item-obtain Qt state missing");
    struct ScaleRestore { int value=GetConfiguredUiScalePercent();~ScaleRestore(){SetRuntimeUiScalePercent(value);} } scaleRestore;
    SetRuntimeUiScalePercent(100);
    const auto sync=[&] {
        Require(adapter.syncGameplay(mode,RenderBackendType::LegacyDirect3D7,
            RenderBackendType::LegacyDirect3D7,0,0),"item-obtain gameplay sync failed");
        Frames(app,view);
    };
    const auto put16=[](std::vector<u8>& b,size_t at,u16 v){b[at]=u8(v);b[at+1]=u8(v>>8);};
    const auto put32=[](std::vector<u8>& b,size_t at,u32 v){for(int i=0;i<4;++i)b[at+i]=u8(v>>(8*i));};
    const auto pickup=[&](u16 index,u16 amount,u16 itemId,u8 identified,u8 refine,
        std::array<u16,4> cardWords,u8 result=0) {
        std::vector<u8> p(29,0);p[0]=0xd4;p[1]=2;put16(p,2,index);put16(p,4,amount);
        put16(p,6,itemId);p[8]=identified;p[9]=0;p[10]=refine;
        for(int i=0;i<4;++i)put16(p,11+i*2,cardWords[i]);
        put16(p,19,0x7777);p[21]=5;p[22]=result;put32(p,23,0xaabbccdd);put16(p,27,2);
        return p;
    };
    QJsonArray cases;int frames=0;
    mode.m_characterNameCache[0x12345678]="Smith";
    g_session.ClearInventoryItems();
    SelectInventoryFixtureTab(1);inventory.SetCurrentTab(1);inventory.Move(10,10);sync();

    // Successful 02D4 snapshots the raw server quantity and descriptor fields before inventory ownership changes.
    auto award=pickup(77,3,1202,1,7,{255,0x0f03,0x5678,0x1234});
    Dispatch(mode,award);sync();
    auto* notice=g_windowMgr.m_itemObtainWnd;
    Require(notice,"successful pickup did not create the independent item-obtain window");
    ITEM_INFO snap=notice->GetItemSnapshot();
    Require(snap.m_num==3&&snap.GetItemId()==1202&&snap.m_isIdentified==1&&snap.m_refiningLevel==7
        &&snap.m_slot[0]==255&&snap.m_slot[1]==0x0f03&&snap.m_slot[2]==0x5678&&snap.m_slot[3]==0x1234,
        "item-obtain popup did not preserve raw pickup snapshot fields");
    Require(g_session.GetInventoryItemByIndex(77)&&g_session.GetInventoryItemByIndex(77)->m_num==3,
        "pickup acknowledgement failed to update inventory alongside independent popup");
    g_session.RemoveInventoryItem(77,3);
    Require(notice->GetItemSnapshot().m_num==3,"popup quantity followed later inventory mutation instead of snapshot");
    g_session.AddInventoryItem(snap);

    const auto checkData=[&](const char* label,int amount=3) {
        const QVariantMap data=state->itemObtainData();
        Require(data.value("visible").toBool(),"item-obtain state not visible");
        Require(data.value("itemId").toUInt()==1202,"item-obtain state item id differs from snapshot");
        Require(data.value("text").toString().contains(QString(" - %1 obtained.").arg(amount)),"full item-obtain message lacks acquired quantity");
        Require(data.value("sizingText").toString().startsWith("You got ")
            &&data.value("sizingText").toString().endsWith(QString(" (%1).").arg(amount)),"width sizing text differs from source message template");
        const QVariantList runs=data.value("runs").toList();Require(!runs.empty(),"item-obtain colored runs missing");
        QString producer;for(const auto& v:runs) {const auto r=v.toMap();
            if(r.value("colorARGB").toUInt()==0xff4040e0u)producer+=r.value("text").toString();}
        Require(producer.contains("Smith"),"item-obtain producer name absent from colored text runs");
        const int expectedX=540,expectedY=51;
        Require(data.value("x").toInt()==expectedX&&data.value("y").toInt()==expectedY,
            "item-obtain popup origin differs from native logical geometry");
        auto* qml=Find(view.rootObject(),"itemObtainWindow");Require(qml&&qml->isVisible(),"actual Qt item-obtain window missing");
        QString expectedRuns;for(const auto& v:runs)expectedRuns+=v.toMap().value("text").toString();
        QString qtText;int qtRuns=0;std::function<void(QQuickItem*)> collectRuns=[&](QQuickItem* item) {
            if(item->objectName()=="itemObtainTextRun"&&item->isVisible()) {qtText+=item->property("text").toString();++qtRuns;}
            for(auto* child:item->childItems())collectRuns(child);
        };collectRuns(qml);
        if(qtRuns==0||qtText!=expectedRuns)std::cerr<<"RUNS expected="<<expectedRuns.toStdString()<<" actual="<<qtText.toStdString()<<" count="<<qtRuns<<"\n";
        Require(qtRuns>0&&qtText==expectedRuns,"Qt item-obtain colored runs differ from native display data");
        auto* icon=Find(qml,"itemObtainIcon");Require(icon&&icon->isVisible(),"actual Qt item-obtain icon missing");
        Require(qRound(icon->x())==13&&qRound(icon->y())==5&&qRound(icon->width())==24&&qRound(icon->height())==24,
            "item-obtain icon rectangle differs from source local geometry");
        Require(qRound(qml->x())==expectedX&&qRound(qml->y())==expectedY,
            "Qt item-obtain placement differs from native logical placement");
        Require(qRound(data.value("width").toReal())>=34&&qRound(qml->width())==data.value("width").toInt(),
            "item-obtain width did not follow bounded source-text sizing");
        const QImage qt=view.grabWindow().convertToFormat(QImage::Format_RGB32);
        Require(qt.save(output+QString("/item-obtain-%1-qt.png").arg(label)),"save item-obtain Qt frame failed");++frames;
        ArgbDibSurface surface;Require(surface.EnsureSize(view.width(),view.height()),"item-obtain native DIB failed");
        std::fill_n(surface.GetPixels(),view.width()*view.height(),0xff181b20u);notice->DrawNativeToHdc(surface.GetDC());
        const QImage native=QImage(reinterpret_cast<uchar*>(surface.GetPixels()),view.width(),view.height(),view.width()*4,QImage::Format_RGB32).copy();
        Require(native.save(output+QString("/item-obtain-%1-native.png").arg(label)),"save item-obtain native frame failed");++frames;
        Require(qt.pixelColor(expectedX+25,expectedY+17)==QColor("#dc781e")
            &&native.pixelColor(expectedX+25,expectedY+17)==QColor("#dc781e"),"native/Qt identified icon absent or wrong resource selected");
        const QRect popup(expectedX,expectedY,data.value("width").toInt(),32);
        Require(qt.rect().contains(popup)&&native.rect().contains(popup),"popup frame lies outside actual captured viewport");
        int qtInk=0,nativeInk=0,qtBlue=0,nativeBlue=0;
        for(int y=popup.top()+3;y<popup.bottom()-1;++y)for(int x=popup.left()+41;x<popup.right()-3;++x) {
            const auto q=qt.pixelColor(x,y),n=native.pixelColor(x,y);
            if(q.red()<80&&q.green()<80&&q.blue()<80)++qtInk;
            if(n.red()<80&&n.green()<80&&n.blue()<80)++nativeInk;
            if(q.blue()>q.red()+70&&q.blue()>q.green()+70)++qtBlue;
            if(n.blue()>n.red()+70&&n.blue()>n.green()+70)++nativeBlue;
        }
        Require(qtInk>20&&nativeInk>20&&qtBlue>5&&nativeBlue>5,"actual native/Qt popup lacks visible text or blue producer pixels");
        UINotifyItemObtainWnd::DisplayData nativeData;Require(notice->GetDisplayDataForQt(&nativeData),"native popup display missing");
        Require(nativeData.runs.size()==size_t(runs.size()),"native and projected run counts differ");
        for(size_t i=0;i<nativeData.runs.size();++i) {
            const auto r=runs[int(i)].toMap();const auto& n=nativeData.runs[i];
            Require(r.value("text").toString()==QString::fromUtf8(n.text.c_str())&&r.value("x").toInt()==n.x
                &&r.value("y").toInt()==n.y&&r.value("colorARGB").toUInt()==n.color,"native/Qt text run snapshot differs");
        }

    };
    g_windowMgr.OnLBtnDown(notice->m_x+2,notice->m_y+2);
    Require(!notice->m_isDragging,"notification captured manager hit testing");
    g_windowMgr.OnLBtnUp(notice->m_x+2,notice->m_y+2);
    checkData("initial");cases.push_back(QJsonObject{{"case","02D4 success snapshots item data and renders independent native/Qt notification"},{"outcome","PASS"}});

    // Producer name arrival changes the name/color run without restarting the timer.
    notice->m_isDirty=0;
    const u32 producerTick=notice->GetMadeTick();mode.m_characterNameCache.erase(0x12345678);
    notice->ProcessAtTick(producerTick+1);sync();
    QVariantMap data=state->itemObtainData();QString redProducer;
    for(const auto& v:data.value("runs").toList()) {const auto r=v.toMap();
        if(r.value("colorARGB").toUInt()==0xffe04040u)redProducer+=r.value("text").toString();}
    Require(redProducer.contains("Nameless")&&notice->GetMadeTick()==producerTick,"unknown producer fallback/color or timer differs from native behavior");
    mode.m_characterNameCache[0x12345678]="Smith";notice->ProcessAtTick(producerTick+2);sync();
    data=state->itemObtainData();QString blueProducer;
    for(const auto& v:data.value("runs").toList()) {const auto r=v.toMap();
        if(r.value("colorARGB").toUInt()==0xff4040e0u)blueProducer+=r.value("text").toString();}
    Require(blueProducer.contains("Smith")&&notice->GetMadeTick()==producerTick,"producer reply failed to refresh blue run without timer reset");
    cases.push_back(QJsonObject{{"case","missing producer fallback refreshes to named producer without extending lifetime"},{"outcome","PASS"}});

    // Badge is an independent overlay and should coexist with the source inventory and item detail.
    g_windowMgr.ShowItemInfoWindow(*g_session.GetInventoryItemByIndex(77),0,0);
    auto* detail=g_windowMgr.m_itemInfoWnd;Require(detail,"owned detail creation failed");
    detail->Move(400,120);sync();checkData("with-detail");
    Require(state->itemInfoData().value("layer").toInt()>state->itemObtainData().value("layer").toInt(),
        "newly opened detail lost manager stacking order");
    Require(state->itemInfoVisible()&&Find(view.rootObject(),"itemInfoWindow")->isVisible(),
        "item-obtain notification hid the item detail window");
    cases.push_back(QJsonObject{{"case","notification coexists with regular item detail"},{"outcome","PASS"}});

    notice->SetItem(snap,123);const u32 firstTick=notice->GetMadeTick();
    award=pickup(77,2,1202,1,7,{255,0x0f03,0x5678,0x1234});Dispatch(mode,award);sync();
    notice=g_windowMgr.m_itemObtainWnd;Require(notice&&notice->GetMadeTick()!=firstTick,
        "same-item award did not replace popup and reset its lifetime");
    Require(notice->GetItemSnapshot().m_num==2&&g_session.GetInventoryItemByIndex(77)->m_num==5,
        "same-item award accumulated quantity instead of displaying raw delta");
    checkData("same-item-replaced",2);cases.push_back(QJsonObject{{"case","same-item repeated award replaces delta and resets popup lifetime"},{"outcome","PASS"}});

    // A failed pickup must leave the existing popup and timestamp untouched.
    const u32 beforeFailure=notice->GetMadeTick();const auto beforeItem=notice->GetItemSnapshot();
    Dispatch(mode,pickup(0x4321,9,501,1,0,{0,0,0,0},3));sync();
    Require(g_windowMgr.m_itemObtainWnd==notice&&notice->GetMadeTick()==beforeFailure
        &&notice->GetItemSnapshot().m_num==beforeItem.m_num&&notice->GetItemSnapshot().GetItemId()==beforeItem.GetItemId(),
        "failed pickup changed existing item-obtain notification");
    cases.push_back(QJsonObject{{"case","failed pickup preserves current notification and timer"},{"outcome","PASS"}});

    // Deterministic boundary check: the popup survives at exactly 5000ms and expires only after it.
    notice->SetItem(beforeItem,1000);notice->ProcessAtTick(6000);
    Require(g_windowMgr.m_itemObtainWnd==notice,"item-obtain popup expired at inclusive 5000ms boundary");
    notice->ProcessAtTick(6001);notice=g_windowMgr.m_itemObtainWnd;
    Require(!notice,"item-obtain popup did not expire after 5000ms");
    cases.push_back(QJsonObject{{"case","popup survives 5000ms and expires after 5000ms"},{"outcome","PASS"}});

    // Layout is based on a font measurement at 12px with kerning disabled, then rounded in 28px steps.
    QFont font(QStringLiteral("MS Sans Serif"));font.setPixelSize(12);font.setKerning(false);
    const QString sizing=QStringLiteral("You got Knife (3).");
    const int measured=QFontMetrics(font).horizontalAdvance(sizing)+54;
    const int expectedWidth=std::max(34,((measured+27)/28)*28);
    notice=static_cast<UINotifyItemObtainWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_NOTIFYITEMOBTAINWND));
    ITEM_INFO sizingItem{};sizingItem.SetItemId(1202);sizingItem.m_itemType=5;sizingItem.m_isIdentified=1;sizingItem.m_num=3;
    notice->SetItem(sizingItem,7000);sync();
    Require(state->itemObtainData().value("width").toInt()==expectedWidth,
        "item-obtain width differs from independent 12px non-kerning font oracle");
    cases.push_back(QJsonObject{{"case","width follows independent MS Sans Serif 12px measurement and 28px rounding"},{"outcome","PASS"}});

    // 100% and 125% use the same logical origin; Qt maps logical dimensions through scale.
    g_windowMgr.DeleteWindow(notice);SetRuntimeUiScalePercent(125);
    notice=static_cast<UINotifyItemObtainWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_NOTIFYITEMOBTAINWND));
    notice->SetItem(sizingItem,8000);sync();
    Require(state->itemObtainData().value("x").toInt()==412
        &&state->itemObtainData().value("y").toInt()==51,"scaled popup changed its logical origin");
    auto* scaled=Find(view.rootObject(),"itemObtainWindow");Require(scaled,"scaled Qt notification missing");
    Require(qAbs(scaled->mapToScene(QPointF(0,0)).x()-412*1.25)<1&&qAbs(scaled->mapToScene(QPointF(0,0)).y()-51*1.25)<1,
        "125% Qt item-obtain geometry did not scale native logical origin");
    Require(notice->m_x==412&&notice->m_y==51&&!notice->ShouldDoHitTest(),"scaled native geometry/hit-test contract differs");
    const QImage scaledFrame=view.grabWindow();
    Require(scaledFrame.pixelColor(546,85)==QColor("#dc781e"),"125% icon pixels absent at source-scaled scene origin");
    Require(scaledFrame.save(output+"/item-obtain-125-qt.png"),"save scaled popup failed");++frames;
    cases.push_back(QJsonObject{{"case","100/125 percent popup origin respects logical UI scaling"},{"outcome","PASS"}});

    SetRuntimeUiScalePercent(100);
    g_windowMgr.DeleteWindow(notice);
    notice=static_cast<UINotifyItemObtainWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_NOTIFYITEMOBTAINWND));
    notice->SetItem(sizingItem,timeGetTime());sync();
    Require(state->itemObtainData().value("text").toString()=="Knife - 3 obtained.","plain visible text acquired socket suffix");
    Require(g_ttemmgr.GetItemObtainChatText(sizingItem)=="You got Knife [4] (3).","chat failed to retain plain equipment slot suffix");
    Require(g_ttemmgr.GetItemObtainSizingText(sizingItem)=="You got Knife (3).","dummy sizing acquired slot suffix");
    const auto fixedWidth=notice->m_w;const auto fixedTick=notice->GetMadeTick();
    notice->SetItem(snap,fixedTick);const auto forgedWidth=notice->m_w;
    mode.m_characterNameCache[0x12345678]="A much longer producer";notice->RefreshContent();
    Require(notice->m_w==forgedWidth&&notice->GetMadeTick()==fixedTick,"name arrival resized or extended notification");
    Require(fixedWidth==forgedWidth,"sizing used decorated name instead of bare dummy name");
    sizingItem.m_isIdentified=0;sizingItem.m_isYours=2;sizingItem.m_refiningLevel=9;
    notice->SetItem(sizingItem,timeGetTime());sync();
    for(const auto& r:state->itemObtainData().value("runs").toList())
        Require(r.toMap().value("colorARGB").toUInt()==0xff000000u,"unidentified/owned notice acquired tooltip colors");
    Require(!notice->ShouldDoHitTest(),"notification intercepted native hit testing");
    const QImage unknown=view.grabWindow();Require(unknown.pixelColor(notice->m_x+25,notice->m_y+17)==QColor("#a050c0"),
        "actual Qt unidentified icon did not select alternate resource");
    Require(view.grabWindow().save(output+"/item-obtain-unidentified-qt.png"),"save unidentified frame failed");++frames;
    g_windowMgr.RemoveAllWindows();sync();
    Require(!g_windowMgr.m_itemObtainWnd&&state->itemObtainData().isEmpty()
        &&!Find(view.rootObject(),"itemObtainWindow")->isVisible(),"map teardown retained native or projected notification");
    cases.push_back(QJsonObject{{"case","context names, stable sizing, passive hit testing and map teardown"},{"outcome","PASS"}});

    QFile result(output+"/item-obtain-render-results.json");Require(result.open(QIODevice::WriteOnly),"open item-obtain render result failed");
    result.write(QJsonDocument(QJsonObject{{"cases",cases},{"frames",frames},{"outcome","PASS"}}).toJson());
}
