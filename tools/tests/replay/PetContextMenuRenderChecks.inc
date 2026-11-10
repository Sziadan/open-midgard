// Included by CheckPetWindowUi after its pet-summary captures. This exercises
// the shared source room-menu popup through the normal game-mode producer.
{
    Require(g_windowMgr.m_chatRoomMenuWnd==nullptr && g_windowMgr.m_roomMessageBox==nullptr,
        "pet context render fixture requires an unused room popup and modal");
    CWorld world;
    CGameActor actor;
    actor.m_gid=0x112233;actor.m_job=1002;actor.m_headType=100;
    world.RegisterActor(&actor);
    struct RestorePetContextRender {
        CGameMode& mode;CWorld& world;CGameActor& actor;CWorld* oldWorld;
        ro::pet::Status pet=g_session.m_petStatus;
        std::vector<int> menuIds=mode.m_menuIdList;
        u32 target=mode.m_menuTargetAID;
        UIStorageWnd* storage=g_windowMgr.m_storageWnd;
        ~RestorePetContextRender(){
            if(g_windowMgr.m_roomMessageBox)g_windowMgr.DeleteWindow(g_windowMgr.m_roomMessageBox);
            if(g_windowMgr.m_chatRoomMenuWnd)g_windowMgr.DeleteWindow(g_windowMgr.m_chatRoomMenuWnd);
            mode.m_world=oldWorld;mode.m_menuIdList=std::move(menuIds);mode.m_menuTargetAID=target;
            g_windowMgr.m_storageWnd=storage;
            world.UnregisterActor(&actor);g_session.m_petStatus=std::move(pet);
        }
    } restore{mode,world,actor,mode.m_world};
    mode.m_world=&world;
    auto& pet=g_session.m_petStatus;pet={};pet.actorId=actor.m_gid;pet.job=1002;pet.name="Poring";
    Require(g_windowMgr.m_storageWnd && !mode.OpenPetContextMenu(&actor,123,234),"source storage-instance menu guard differs");
    g_windowMgr.m_storageWnd=nullptr; // The harness's unrelated stack storage pane is now closed for this scenario.

    const auto captureContext=[&](const QString& stem){
        state->setUiScale(GetConfiguredUiScaleFactor());
        Require(adapter.syncGameplay(mode,RenderBackendType::LegacyDirect3D7,
            RenderBackendType::LegacyDirect3D7,-1,-1),"pet context gameplay projection failed");
        Frames(app,view);
        const auto* menu=g_windowMgr.m_chatRoomMenuWnd;
        Require(menu,"actual source pet context popup missing");
        auto* qmlMenu=Find(view.rootObject(),"sourceRoomMenu");
        Require(qmlMenu && qmlMenu->isVisible(),"actual Qt room-menu popup missing");
        const QVariantMap data=state->chatWindowUi().value("roomMenu").toMap();
        const auto rows=data.value("rows").toList();
        Require(data.value("visible").toBool() && data.value("width").toInt()==menu->m_w
            && data.value("height").toInt()==70 && rows.size()==5,
            "native/passive Qt room-menu dimensions or rows differ");
        const auto bounds=qmlMenu->mapRectToScene(qmlMenu->boundingRect());
        const double scale=GetConfiguredUiScaleFactor();
        Require(std::abs(bounds.left()-menu->m_x*scale)<0.01
            && std::abs(bounds.top()-menu->m_y*scale)<0.01
            && std::abs(bounds.width()-menu->m_w*scale)<0.01
            && std::abs(bounds.height()-menu->m_h*scale)<0.01
            && qmlMenu->z()>0,"actual Qt room-menu geometry or layer differs");
        for(int i=0;i<5;++i){
            Require(rows[i].toMap().value("action").toInt()==mode.m_menuIdList[i]
                && rows[i].toMap().value("text").toString()==QString::fromUtf8(menu->GetItems()[i].label.c_str()),
                "native/passive Qt room-menu row data differs");
        }
        ArgbDibSurface dib;Require(dib.EnsureSize(view.width(),view.height()),"pet context DIB allocation failed");
        std::fill_n(dib.GetPixels(),view.width()*view.height(),0xff181b20u);
        g_windowMgr.DrawVisibleWindowsToHdc(dib.GetDC(),true);
        auto native=QImage(reinterpret_cast<uchar*>(dib.GetPixels()),view.width(),view.height(),
            view.width()*4,QImage::Format_RGB32).copy();
        if(GetConfiguredUiScalePercent()!=100)native=native.scaled(qRound(native.width()*scale),
            qRound(native.height()*scale),Qt::IgnoreAspectRatio,Qt::FastTransformation).copy(0,0,view.width(),view.height());
        const auto qml=view.grabWindow().convertToFormat(QImage::Format_RGB32);
        const QPoint visiblePixel(UiScaleLogicalToRawCoordinate(menu->m_x+1),UiScaleLogicalToRawCoordinate(menu->m_y+1));
        Require(native.pixelColor(visiblePixel)==QColor("#f0f0f0") && qml.pixelColor(visiblePixel)==QColor("#f0f0f0"),
            "source pet menu is obscured by an unrelated pane despite visible geometry");
        Require(native.save(output+"/pet-context-"+stem+".native.png")
            && qml.save(output+"/pet-context-"+stem+".qml.png"),"pet context paired frame save failed");
        ++frames;
    };

    Require(mode.OpenPetContextMenu(&actor,123,234),"actual owned pet context producer failed");
    auto* menu=g_windowMgr.m_chatRoomMenuWnd;
    Require(menu && menu->m_x==124 && menu->m_y==235 && menu->m_h==70
        && mode.m_menuIdList==std::vector<int>({33,29,30,32,31})
        && mode.m_menuTargetAID==actor.m_gid,"source pet context order, placement or target differs");
    Require(menu->GetItems()[0].label=="Pet Info","source pet context info label differs (message 596)");
    captureContext("100");
    SetRuntimeUiScalePercent(125);captureContext("125");SetRuntimeUiScalePercent(100);
    pointer(WM_LBUTTONDOWN,menu->m_x+4,menu->m_y+4);
    pointer(WM_LBUTTONUP,menu->m_x-2,menu->m_y-2);
    Require(g_windowMgr.m_chatRoomMenuWnd==menu && !g_windowMgr.m_captureWindow && !GetCapture(),
        "outside native release retired source popup or retained capture");

    // Reopen at the stable 100% fixture position, then select Feed by sending
    // the same WindowProc mouse events used by the rest of this render check.
    Require(mode.OpenPetContextMenu(&actor,123,234),"pet context popup could not be reopened");
    menu=g_windowMgr.m_chatRoomMenuWnd;
    click(menu->m_x+4,menu->m_y+14+3);
    auto* confirmation=g_windowMgr.m_roomMessageBox;
    Require(confirmation && confirmation->GetMessageId()==601 && !confirmation->GetSourceOwner(),
        "native pet context Feed click did not open ownerless feed confirmation");
    adapter.syncChat();Frames(app,view);
    auto* qmlModal=Find(view.rootObject(),"sourceRoomValidation");
    Require(qmlModal && qmlModal->isVisible(),"ownerless pet feed confirmation missing in Qt projection");
    ArgbDibSurface modalDib;Require(modalDib.EnsureSize(view.width(),view.height()),"pet context modal DIB failed");
    std::fill_n(modalDib.GetPixels(),view.width()*view.height(),0xff181b20u);
    g_windowMgr.DrawVisibleWindowsToHdc(modalDib.GetDC(),true);
    const auto modalNative=QImage(reinterpret_cast<uchar*>(modalDib.GetPixels()),view.width(),view.height(),view.width()*4,QImage::Format_RGB32).copy();
    Require(modalNative.save(output+"/pet-context-feed-confirmation.native.png")
        && view.grabWindow().save(output+"/pet-context-feed-confirmation.qml.png"),"pet context modal paired save failed");
    ++frames;
    pointer(WM_KEYDOWN,VK_ESCAPE,0);
    Require(!g_windowMgr.m_roomMessageBox,"Escape did not cancel ownerless pet feed confirmation");
    adapter.syncChat();Frames(app,view);
    Require(!Find(view.rootObject(),"sourceRoomValidation")->isVisible(),
        "cancelled ownerless feed confirmation remains visible in Qt");
}
