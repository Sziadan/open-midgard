    // Synthetic SlotMachine resources isolate the selected taming window from
    // installed game assets. The parent includes UIPetTamingDeceiveWnd.h.
    {
        Require(state!=nullptr,"pet taming Qt state is missing");
        const auto oldCwd=std::filesystem::current_path();
        const auto assets=std::filesystem::path(output.toStdString())/"pet-taming-assets";
        std::filesystem::create_directories(assets/"data"/"sprite");
        std::filesystem::current_path(assets);
        struct RestorePetTamingFixture {std::filesystem::path cwd;int scale;ro::pet::Status pet=g_session.m_petStatus;~RestorePetTamingFixture(){
            if(g_windowMgr.m_petTamingDeceiveWnd)g_windowMgr.DeleteWindow(g_windowMgr.m_petTamingDeceiveWnd);
            SetRuntimeUiScalePercent(scale);std::filesystem::current_path(cwd);g_session.m_petStatus=pet;
        }} restoreTaming{oldCwd,GetConfiguredUiScalePercent()};
        g_session.m_petStatus.actorId=0;
        g_resMgr.RegisterType("spr","data\\sprite\\",new CSprRes());
        g_resMgr.RegisterType("act","data\\sprite\\",new CActRes());
        const auto put16=[](std::vector<u8>& b,u16 v){b.push_back(static_cast<u8>(v));b.push_back(static_cast<u8>(v>>8));};
        const auto put32=[](std::vector<u8>& b,u32 v){for(int i=0;i<4;++i)b.push_back(static_cast<u8>(v>>(8*i)));};
        const auto write=[&](const std::filesystem::path& path,const std::vector<u8>& bytes){
            std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
            Require(f.good(),"synthetic pet taming resource write failed");
        };
        // Full white panel plus a six-pixel RGB mark exercise list0, clip
        // placement, palette transparency and non-bit0 flip flags.
        std::vector<u8> spr{'S','P',1,2};put16(spr,2);put16(spr,0);
        put16(spr,196);put16(spr,256);put16(spr,196*256);spr.insert(spr.end(),196*256,1);
        put16(spr,6);put16(spr,4);std::vector<u8> mark;
        for(int y=0;y<4;++y)for(int x=0;x<6;++x){
            const u8 index=(y==3 && x==2)?0:static_cast<u8>(2+x/2);mark.push_back(index);if(!index)mark.push_back(1);
        }
        put16(spr,static_cast<u16>(mark.size()));spr.insert(spr.end(),mark.begin(),mark.end());
        std::array<u8,1024> palette{};palette[4]=palette[5]=palette[6]=255;
        palette[8]=255;palette[13]=255;palette[18]=255;
        spr.insert(spr.end(),palette.begin(),palette.end());
        write(assets/"data"/"sprite"/"SlotMachine.spr",spr);
        // ACT205 has two 16-byte range rectangles per motion, then clips,
        // event and attachment fields; action1 is required after selection.
        std::vector<u8> act{'A','C',5,2,2,0};act.resize(16,0);
        const auto clip=[&](int x,int y,int index,int flags,int type,int width,int height,u32 zoom){
            put32(act,static_cast<u32>(x));put32(act,static_cast<u32>(y));put32(act,index);put32(act,flags);
            act.insert(act.end(),{0,0,0,0});put32(act,zoom);put32(act,zoom);
            put32(act,123);put32(act,type);put32(act,width);put32(act,height);
        };
        for(int action=0;action<2;++action){put32(act,8);
            for(int frame=0;frame<8;++frame){
                act.resize(act.size()+32,0);put32(act,2);
                // Loader converts ACT centers to top-lefts using declared
                // sizes/zoom; source owner then draws actual SPR dimensions.
                clip(0,0,0,0,0,196,256,0x3f800000);
                clip((frame+2)*2,2,1,action?2:0,1,6,4,0x40000000);
                put32(act,0xffffffff);put32(act,0);
            }
        }
        put32(act,0);put32(act,0x40800000);put32(act,0x40800000);
        write(assets/"data"/"sprite"/"SlotMachine.act",act);
        auto* sourceAct=g_resMgr.GetAs<CActRes>("SlotMachine.act");
        auto* sourceSpr=g_resMgr.GetAs<CSprRes>("SlotMachine.spr");
        Require(sourceAct && sourceAct->GetMotionCount(0)==8 && sourceAct->GetMotionCount(1)==8
            && sourceSpr && sourceSpr->GetSprite(0,1),"synthetic taming ACT/SPR did not load through production resource loaders");
        Require(sourceAct->GetMotion(0,0)->sprClips.size()==2
            && sourceAct->GetMotion(0,0)->sprClips[0].x==-98 && sourceAct->GetMotion(0,0)->sprClips[0].y==-128
            && sourceAct->GetMotion(0,0)->sprClips[1].x==-1 && sourceAct->GetMotion(0,0)->sprClips[1].y==-1,
            "ACT center-to-top-left source fixture oracle differs");
        const auto open=[&](u32 gid){mode.SendMsg(76,10000,gid,0);auto* w=g_windowMgr.m_petTamingDeceiveWnd;
            Require(w && w->GetTargetGid()==gid,"mode76 did not create and initialize taming owner");return w;};
        const auto captureTaming=[&](const QString& stem,UIPetTamingDeceiveWnd* w){
            const int preparedFrame=static_cast<int>(w->GetMotionTick()/w->GetDelay());
            w->PrepareDrawFrame();state->setUiScale(GetConfiguredUiScaleFactor());
            const auto afterPrepare=w->GetMotionTick();
            Require(adapter.syncGameplay(mode,RenderBackendType::LegacyDirect3D7,
                RenderBackendType::LegacyDirect3D7,-1,-1),"pet taming gameplay projection failed");
            view.requestUpdate();for(int i=0;i<6;++i){app.processEvents();QThread::msleep(20);}
            auto* qmlOwner=Find(view.rootObject(),"sourcePetTaming");
            Require(qmlOwner && qmlOwner->isVisible(),"actual Qt pet taming surface missing");
            const QVariantMap data=state->chatWindowUi().value("petTaming").toMap();
            Require(data.value("visible").toBool() && data.value("width").toInt()==w->m_w
                && data.value("height").toInt()==w->m_h && data.value("x").toInt()==w->m_x
                && data.value("y").toInt()==w->m_y && data.value("targetGid").toUInt()==w->GetTargetGid()
                && data.value("selected").toBool()==w->IsSelected() && data.value("action").toInt()==w->GetAction()
                && data.value("delay").toInt()==w->GetDelay() && data.value("destination").toInt()==w->GetDestination()
                && data.value("artwork").toString()==QString::fromStdString(w->GetArtworkUri()),
                "native/Qt pet taming state projection differs");
            const double scale=GetConfiguredUiScaleFactor();const auto bounds=qmlOwner->mapRectToScene(qmlOwner->boundingRect());
            Require(std::abs(bounds.left()-w->m_x*scale)<0.01 && std::abs(bounds.top()-w->m_y*scale)<0.01
                && std::abs(bounds.width()-w->m_w*scale)<0.01 && std::abs(bounds.height()-w->m_h*scale)<0.01,
                "native/Qt pet taming geometry differs");
            ArgbDibSurface dib;Require(dib.EnsureSize(view.width(),view.height()),"pet taming DIB allocation failed");
            std::fill_n(dib.GetPixels(),view.width()*view.height(),0xff181b20u);g_windowMgr.DrawVisibleWindowsToHdc(dib.GetDC(),true);
            auto native=QImage(reinterpret_cast<uchar*>(dib.GetPixels()),view.width(),view.height(),view.width()*4,QImage::Format_RGB32).copy();
            if(GetConfiguredUiScalePercent()!=100)native=native.scaled(qRound(native.width()*scale),qRound(native.height()*scale),
                Qt::IgnoreAspectRatio,Qt::FastTransformation).copy(0,0,view.width(),view.height());
            const auto qml=view.grabWindow().convertToFormat(QImage::Format_RGB32);
            Require(native.save(output+"/pet-taming-"+stem+".native.png") && qml.save(output+"/pet-taming-"+stem+".qml.png"),
                "pet taming native/QML frame save failed");
            const auto point=[&](int x,int y){return QPoint(UiScaleLogicalToRawCoordinate(w->m_x+x),UiScaleLogicalToRawCoordinate(w->m_y+y));};
            const auto same=[&](QPoint p,QColor color){Require(native.rect().contains(p)&&qml.rect().contains(p)
                && native.pixelColor(p)==color && qml.pixelColor(p)==color,"pet taming native/QML source artwork pixel mismatch");};
            same(point(10,10),QColor("#ffffff"));
            same(point(98+preparedFrame,128),w->GetAction()?QColor("#0000ff"):QColor("#ff0000"));
            const int markX=97+preparedFrame;
            Require(w->GetArtwork().pixels[130*196+markX+(w->GetAction()?3:2)]==0xffffffffu,
                "SPR index0 did not preserve underlying clip during flip");
            Require(w->GetMotionTick()==afterPrepare,"passive state projection or native/Qt drawing advanced animation again");
            ++frames;
        };
        auto* taming=open(0x12345678);captureTaming("idle",taming);
        const auto tickBeforeClick=taming->GetMotionTick();
        const int rawX=UiScaleLogicalToRawCoordinate(taming->m_x+98),rawY=UiScaleLogicalToRawCoordinate(taming->m_y+128);
        WindowProc(g_hMainWnd,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(rawX,rawY));
        WindowProc(g_hMainWnd,WM_LBUTTONUP,0,MAKELPARAM(rawX,rawY));adapter.syncChat();app.processEvents();
        Require(taming->IsSelected()&&taming->GetAction()==1&&taming->GetDestination()==7,
            "native manager click did not select actual taming owner");captureTaming("selected",taming);
        Require(taming->GetMotionTick()>=tickBeforeClick,"first click unexpectedly reset the source animation tick");
        taming->SendMsg(nullptr,80,1,3,0);
        Require(taming->GetDestination()==3&&taming->GetDelay()==7,"failed result did not set fail destination");
        captureTaming("failed-result",taming);
        g_windowMgr.DeleteWindow(taming);SetRuntimeUiScalePercent(125);state->setUiScale(GetConfiguredUiScaleFactor());
        taming=open(0x12345679);const int sx=UiScaleLogicalToRawCoordinate(taming->m_x+98),sy=UiScaleLogicalToRawCoordinate(taming->m_y+128);
        WindowProc(g_hMainWnd,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(sx,sy));WindowProc(g_hMainWnd,WM_LBUTTONUP,0,MAKELPARAM(sx,sy));
        Require(taming->IsSelected(),"125% scaled native click missed taming target");captureTaming("125-selected",taming);
        g_windowMgr.DeleteWindow(taming);

        // Explicitly prepare the empty frame at frame==motionCount. It must
        // publish transparent art before wrapping the next tick to zero.
        taming=open(0x12345680);
        for(int i=0;i<64 && taming->GetMotionTick()/taming->GetDelay()<8;++i)taming->PrepareDrawFrame();
        Require(taming->GetMotionTick()/taming->GetDelay()==8,"taming animation skipped empty boundary frame");
        taming->PrepareDrawFrame();
        Require(taming->GetMotionTick()==0 && taming->GetArtwork().IsValid()
            && std::all_of(taming->GetArtwork().pixels.begin(),taming->GetArtwork().pixels.end(),[](u32 p){return p==0;}),
            "empty ACT boundary frame was not transparent before motion wrap");
        g_windowMgr.DeleteWindow(taming);

        // Timer starts only when the integer motion quotient is stopped;
        // changing destination leaves the original start tick untouched.
        taming=open(0x12345681);taming->SendMsg(nullptr,80,1,3,0);
        for(int i=0;i<64 && taming->GetMotionTick()/taming->GetDelay()<3;++i)taming->PrepareDrawFrame();
        Require(taming->GetMotionTick()/taming->GetDelay()==3,"taming animation did not reach result destination");
        taming->ProcessAt(100);Require(taming->GetStartTick()==100,"pet taming stopped-frame timer did not start");
        taming->SendMsg(nullptr,80,1,7,0);taming->ProcessAt(1000);
        Require(taming->GetStartTick()==100 && g_windowMgr.m_petTamingDeceiveWnd==taming,
            "destination change reset stale start tick or closed before reaching destination");
        taming->SendMsg(nullptr,80,1,3,0);taming->ProcessAt(2099);
        Require(g_windowMgr.m_petTamingDeceiveWnd==taming,"pet taming timer closed before 2000ms");
        taming->ProcessAt(2100);Require(!g_windowMgr.m_petTamingDeceiveWnd,"pet taming timer did not close at 2000ms");
        taming=open(0x12345682);taming->SendMsg(nullptr,80,1,0,0);
        taming->ProcessAt(0xffffff00u);taming->ProcessAt(0x6cf);
        Require(g_windowMgr.m_petTamingDeceiveWnd==taming,"taming close timer mishandled unsigned tick wrap before deadline");
        taming->ProcessAt(0x6d0);Require(!g_windowMgr.m_petTamingDeceiveWnd,"taming close timer failed across unsigned tick wrap");
        SetRuntimeUiScalePercent(100);state->setUiScale(1.0);adapter.syncChat();app.processEvents();
    }
