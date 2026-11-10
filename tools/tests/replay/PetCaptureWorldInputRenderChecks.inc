    // Exercise the real world target picker, not a direct actor-selection
    // helper. The preceding pet fixture installed the synthetic job1002 art.
    {
        const int oldUiScale=GetConfiguredUiScalePercent();SetRuntimeUiScalePercent(100);
        const QByteArray oldBackend=qgetenv("OPEN_MIDGARD_RENDER_BACKEND");
        const bool hadBackend=qEnvironmentVariableIsSet("OPEN_MIDGARD_RENDER_BACKEND");
        struct RestoreCaptureDevice {QByteArray value;bool had;~RestoreCaptureDevice(){
            GetRenderDevice().Shutdown();
            if(had)qputenv("OPEN_MIDGARD_RENDER_BACKEND",value);else qunsetenv("OPEN_MIDGARD_RENDER_BACKEND");
        }} restoreDevice{oldBackend,hadBackend};
        qputenv("OPEN_MIDGARD_RENDER_BACKEND","vulkan");RenderBackendBootstrapResult bootstrap{};
        Require(InitializeRenderBackend(g_hMainWnd,&bootstrap) && bootstrap.backend==RenderBackendType::Vulkan,
            "world capture fixture could not initialize production Vulkan scene device");
        const auto projection=std::make_tuple(g_renderer.m_width,g_renderer.m_height,g_renderer.m_halfWidth,
            g_renderer.m_halfHeight,g_renderer.m_aspectRatio,g_renderer.m_hpc,g_renderer.m_vpc,g_renderer.m_xoffset,g_renderer.m_yoffset);
        struct RestoreCaptureProjection {decltype(projection) saved;~RestoreCaptureProjection(){
            std::tie(g_renderer.m_width,g_renderer.m_height,g_renderer.m_halfWidth,g_renderer.m_halfHeight,
                g_renderer.m_aspectRatio,g_renderer.m_hpc,g_renderer.m_vpc,g_renderer.m_xoffset,g_renderer.m_yoffset)=saved;
        }} restoreProjection{projection};
        g_renderer.SetSize(view.width(),view.height());
        C3dAttr attr;attr.m_width=64;attr.m_height=64;attr.m_zoom=10;
        attr.m_cells.resize(64*64);
        CWorld world;world.m_attr=&attr;world.m_isPKZone=0;world.m_isSiegeMode=0;
        CPlayer player;player.m_gid=0x330001;player.m_isPc=1;player.m_pos={-30.0f,0.0f,0.0f};
        world.m_player=&player;
        CPc monster;monster.m_gid=0x330002;monster.m_job=1002;monster.m_isPc=0;
        monster.m_isVisible=1;monster.m_isVisibleBody=1;monster.m_pos={30.0f,0.0f,0.0f};
        world.RegisterActor(&player);world.RegisterActor(&monster);

        CView camera;camera.SetWorld(&world);camera.SetInitialCamera(0,-45,500);camera.OnCalcViewInfo();
        const auto oldWorld=mode.m_world;const auto oldView=mode.m_view;
        const int oldCanRotate=mode.m_canRotateView;
        const auto oldActors=mode.m_runtimeActors;const auto oldMode=g_modeMgr.GetCurrentMode();
        const auto oldSkillInfo=mode.m_skillUseInfo;
        const int oldHasChase=mode.m_hasSkillChase,oldChaseSkill=mode.m_skillChaseSkillId;
        const int oldChaseLevel=mode.m_skillChaseSkillLevel,oldChaseRange=mode.m_skillChaseRange;
        const u32 oldChaseTarget=mode.m_skillChaseTargetGid,oldChaseTick=mode.m_lastSkillChaseRequestTick;
        const int oldHasGround=mode.m_hasGroundSkillChase,oldGroundX=mode.m_groundSkillCellX,oldGroundY=mode.m_groundSkillCellY;
        const int oldGroundId=mode.m_groundSkillId,oldGroundLevel=mode.m_groundSkillLevel,oldGroundRange=mode.m_groundSkillRange;
        const int oldHasTouch=mode.m_hasTouchSkillChase,oldTouchId=mode.m_touchSkillId;
        const int oldTouchLevel=mode.m_touchSkillLevel,oldTouchRange=mode.m_touchSkillRange;
        const u32 oldTouchTarget=mode.m_touchSkillTargetAid;
        const u32 oldAttackTick=mode.m_lastAttackChaseHintTick,oldAttackTarget=mode.m_attackChaseTargetGid;
        const int oldAttackCellX=mode.m_attackChaseTargetCellX,oldAttackCellY=mode.m_attackChaseTargetCellY;
        const int oldAttackSourceX=mode.m_attackChaseSourceCellX,oldAttackSourceY=mode.m_attackChaseSourceCellY;
        const int oldAttackRange=mode.m_attackChaseRange,oldHasAttack=mode.m_hasAttackChaseHint;
        const auto oldPickupList=mode.m_pickupReqItemNaidList;const u32 oldPickupTick=mode.m_lastPickupRequestTick;
        const int oldCtrlLock=mode.m_isCtrlLock,oldLeftHeld=mode.m_isLeftButtonHeld,oldHasHeld=mode.m_hasHeldMoveTarget;
        const int oldLastMoveX=mode.m_lastMoveRequestCellX,oldLastMoveY=mode.m_lastMoveRequestCellY;
        const u32 oldMoveTick=mode.m_lastMoveRequestTick,oldAttackRequestTick=mode.m_lastAttackRequestTick;
        const int oldNoShift=g_session.m_isNoShift;const u32 oldAid=g_session.m_aid,oldGid=g_session.m_gid;
        struct RestorePetCaptureWorldInput {
            CGameMode& mode;CWorld& world;CPc& monster;CPlayer& player;
            CWorld* oldWorld;CView* oldView;int canRotate;std::map<u32,CGameActor*> actors;CMode* oldMode;
            SKILL_USE_INFO skill;int hasChase,chaseSkill,chaseLevel,chaseRange;u32 chaseTarget,chaseTick;
            int hasGround,groundX,groundY,groundId,groundLevel,groundRange;
            int hasTouch,touchId,touchLevel,touchRange;u32 touchTarget;
            u32 attackTick,attackTarget;int attackX,attackY,attackSourceX,attackSourceY,attackRange,hasAttack;
            std::list<unsigned int> pickupList;u32 pickupTick;int ctrlLock,leftHeld,hasHeld,lastMoveX,lastMoveY;
            u32 moveTick,attackRequestTick;
            int noShift;u32 aid,gid;int uiScale;
            ~RestorePetCaptureWorldInput(){
                world.UnregisterActor(&monster);world.UnregisterActor(&player);
                world.m_player=nullptr;world.m_attr=nullptr;
                mode.m_world=oldWorld;mode.m_view=oldView;mode.m_canRotateView=canRotate;mode.m_runtimeActors=std::move(actors);
                mode.m_skillUseInfo=skill;mode.m_hasSkillChase=hasChase;mode.m_skillChaseSkillId=chaseSkill;
                mode.m_skillChaseSkillLevel=chaseLevel;mode.m_skillChaseRange=chaseRange;
                mode.m_skillChaseTargetGid=chaseTarget;mode.m_lastSkillChaseRequestTick=chaseTick;
                mode.m_hasGroundSkillChase=hasGround;mode.m_groundSkillCellX=groundX;mode.m_groundSkillCellY=groundY;
                mode.m_groundSkillId=groundId;mode.m_groundSkillLevel=groundLevel;mode.m_groundSkillRange=groundRange;
                mode.m_hasTouchSkillChase=hasTouch;mode.m_touchSkillId=touchId;mode.m_touchSkillLevel=touchLevel;
                mode.m_touchSkillRange=touchRange;mode.m_touchSkillTargetAid=touchTarget;
                mode.m_lastAttackChaseHintTick=attackTick;mode.m_attackChaseTargetGid=attackTarget;
                mode.m_attackChaseTargetCellX=attackX;mode.m_attackChaseTargetCellY=attackY;
                mode.m_attackChaseSourceCellX=attackSourceX;mode.m_attackChaseSourceCellY=attackSourceY;
                mode.m_attackChaseRange=attackRange;mode.m_hasAttackChaseHint=hasAttack;
                mode.m_pickupReqItemNaidList=std::move(pickupList);mode.m_lastPickupRequestTick=pickupTick;
                mode.m_isCtrlLock=ctrlLock;mode.m_isLeftButtonHeld=leftHeld;mode.m_hasHeldMoveTarget=hasHeld;
                mode.m_lastMoveRequestCellX=lastMoveX;mode.m_lastMoveRequestCellY=lastMoveY;
                mode.m_lastMoveRequestTick=moveTick;mode.m_lastAttackRequestTick=attackRequestTick;
                SkillPacketReplayAccess::SetCurrent(dynamic_cast<CGameMode*>(oldMode));
                g_session.m_isNoShift=noShift;g_session.m_aid=aid;g_session.m_gid=gid;
                SetRuntimeUiScalePercent(uiScale);
            }
        } restoreWorldInput{mode,world,monster,player,oldWorld,oldView,oldCanRotate,oldActors,oldMode,oldSkillInfo,
            oldHasChase,oldChaseSkill,oldChaseLevel,oldChaseRange,oldChaseTarget,oldChaseTick,
            oldHasGround,oldGroundX,oldGroundY,oldGroundId,oldGroundLevel,oldGroundRange,
            oldHasTouch,oldTouchId,oldTouchLevel,oldTouchRange,oldTouchTarget,
            oldAttackTick,oldAttackTarget,oldAttackCellX,oldAttackCellY,oldAttackSourceX,oldAttackSourceY,oldAttackRange,oldHasAttack,
            oldPickupList,oldPickupTick,oldCtrlLock,oldLeftHeld,oldHasHeld,oldLastMoveX,oldLastMoveY,oldMoveTick,oldAttackRequestTick,
            oldNoShift,oldAid,oldGid,oldUiScale};
        mode.m_world=&world;mode.m_view=&camera;mode.m_canRotateView=0;mode.m_runtimeActors=oldActors;
        mode.m_runtimeActors[player.m_gid]=&player;mode.m_runtimeActors[monster.m_gid]=&monster;
        SkillPacketReplayAccess::SetCurrent(&mode);
        g_session.m_aid=player.m_gid;g_session.m_gid=player.m_gid;g_session.m_isNoShift=1;

        if(!monster.EnsureBillboardTexture(camera.GetCameraLongitude())){
            throw std::runtime_error("synthetic monster billboard failed: resourceJob="+std::to_string(monster.m_cachedNonPcResourceJob)
                +" act="+std::to_string(monster.m_cachedNonPcActRes!=nullptr)+" spr="+std::to_string(monster.m_cachedNonPcSprRes!=nullptr)
                +" action="+std::to_string(monster.m_curAction)+" base="+std::to_string(monster.m_baseAction)
                +" job="+g_session.GetJobName(monster.m_job));
        }
        world.EnsureBillboardFrameCache(camera.GetViewMatrix(),camera.GetCameraLongitude());
        const auto entry=std::find_if(world.m_billboardFrameEntries.begin(),world.m_billboardFrameEntries.end(),
            [&](const CWorld::BillboardScreenEntry& candidate){return candidate.actor==&monster;});
        Require(entry!=world.m_billboardFrameEntries.end(),"synthetic job1002 billboard was not built for world pick");
        const int clickX=static_cast<int>(std::floor((entry->left+entry->right)*0.5f));
        const int clickY=static_cast<int>(std::floor((entry->top+entry->bottom)*0.5f));
        Require(clickX>=0 && clickY>=0 && clickX<view.width() && clickY<view.height(),"world pick sample is outside viewport");
        CGameActor* picked=nullptr;
        Require(world.FindHoveredActorScreen(camera.GetViewMatrix(),camera.GetCameraLongitude(),clickX,clickY,
            &picked,nullptr,nullptr,true)&&picked==&monster,
            "real billboard screen hit did not resolve synthetic target actor");
        Require(!g_windowMgr.HasWindowAtPoint(clickX,clickY),"world target sample is occluded by a native window");

        CGameModePacketRouter router;RegisterDefaultGameModePacketHandlers(router);
        const u8 arm[]={0x9e,0x01};
        Require(router.Dispatch(mode,{0x019e,2,arm}),"registered 019E capture arm did not dispatch");
        Require(mode.m_skillUseInfo.id==10000&&mode.m_skillUseInfo.level==0
            &&mode.m_skillUseInfo.attackRange==5&&mode.m_skillUseInfo.useType==2,
            "019E did not arm canonical capture mode intent");
        const int rawX=clickX,rawY=clickY;
        WindowProc(g_hMainWnd,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(rawX,rawY));
        WindowProc(g_hMainWnd,WM_LBUTTONUP,0,MAKELPARAM(rawX,rawY));
        Require(mode.m_hasSkillChase&&mode.m_skillChaseTargetGid==monster.m_gid
            &&mode.m_skillChaseSkillId==10000&&mode.m_skillChaseSkillLevel==0
            &&mode.m_skillChaseRange==5,"real WindowProc world click did not arm capture chase");
        Require(player.m_proceedType.type==3&&player.m_proceedTargetGid==monster.m_gid
            &&player.m_skillId==10000&&player.m_skillAttackRange==5&&player.m_skillUseLevel==0,
            "real world click did not install canonical player proceed fields");
        Require(mode.m_skillUseInfo.id==0&&mode.m_skillUseInfo.level==0&&mode.m_skillUseInfo.useType==0,
            "world click failed to consume pending capture mode intent");
        QFile worldProof(output+"/pet-capture-world-input-results.json");
        Require(worldProof.open(QIODevice::WriteOnly),"world capture input proof file open failed");
        worldProof.write(QJsonDocument(QJsonObject{{"outcome","PASS"},{"scene_backend","Vulkan"},
            {"real_scene_texture",monster.m_billboardTexture!=nullptr},{"texture_width",monster.m_billboardTextureWidth},
            {"texture_height",monster.m_billboardTextureHeight},{"actor_gid",static_cast<int>(monster.m_gid)},
            {"screen_x",clickX},{"screen_y",clickY},{"real_billboard_picker",true},{"native_windowproc",true},
            {"registered_019e",true},{"capture_skill",mode.m_skillChaseSkillId},{"level",mode.m_skillChaseSkillLevel},
            {"range",mode.m_skillChaseRange},{"source_proceed_type",player.m_proceedType.type},
            {"pending_intent_consumed",true}}).toJson(QJsonDocument::Compact));
        {
            const auto temporary=g_session.GetTemporarySkillItems();
            const int weight=g_session.m_weight,maxWeight=g_session.m_maxWeight,noShift=g_session.m_isNoShift;
            const bool good=g_isGoodSkillHanging;
            struct RestoreSourcePendingInput {std::function<void()> fn;~RestoreSourcePendingInput(){fn();}} restorePending{[&]{
                player.SendMsg(nullptr,13,0,0,0);
                g_session.ClearTemporarySkillItems();for(const auto& row:temporary)g_session.AddTemporarySkillItem(row);
                g_session.m_weight=weight;g_session.m_maxWeight=maxWeight;g_session.m_isNoShift=noShift;g_isGoodSkillHanging=good;
            }};
            g_session.m_weight=0;g_session.m_maxWeight=1000;g_session.m_isNoShift=1;g_session.ClearTemporarySkillItems();
            for(const auto [level,range]:std::array<std::pair<u16,int>,2>{{{0,3},{0xfffe,2}}}){
                player.SendMsg(nullptr,13,0,0,0);
                std::array<u8,39> packet{};packet[0]=0x47;packet[1]=1;packet[2]=30;packet[4]=1;
                packet[8]=static_cast<u8>(level);packet[9]=static_cast<u8>(level>>8);packet[12]=static_cast<u8>(range);
                Require(router.Dispatch(mode,{0x0147,39,packet.data()}),"registered temporary skill did not dispatch to126/79");
                Require(mode.m_skillUseInfo.useType==2 && mode.m_skillUseInfo.attackRange==range,
                    "temporary row selector/range was replaced by learned metadata");
                WindowProc(g_hMainWnd,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(rawX,rawY));
                WindowProc(g_hMainWnd,WM_LBUTTONUP,0,MAKELPARAM(rawX,rawY));
                Require(player.m_proceedType.type==3 && player.m_proceedTargetGid==monster.m_gid && player.m_skillId==30
                    && player.m_skillUseLevel==static_cast<s16>(level) && player.m_skillAttackRange==range
                    && mode.m_hasSkillChase && mode.m_skillChaseSkillId==30 && mode.m_skillChaseRange==range,
                    "real world temporary skill click lost raw level/range/player41/90");
                Require(!mode.m_skillUseInfo.useType && !mode.m_skillUseInfo.id && !mode.m_skillUseInfo.level
                    && mode.m_skillUseInfo.attackRange==range && !g_isGoodSkillHanging,"world click did not consume through source78");
                Require(g_session.GetTemporarySkillItemBySkillId(30)->attackRange==3,
                    "incoming duplicate range incorrectly replaced temporary storage metadata");
            }
            int missX=-1,missY=-1;
            for(int y=20;y<view.height() && missX<0;y+=40)for(int x=20;x<view.width();x+=40){
                CGameActor* hit=nullptr;
                if(!g_windowMgr.HasWindowAtPoint(x,y) && !world.FindHoveredActorScreen(camera.GetViewMatrix(),camera.GetCameraLongitude(),
                    x,y,&hit,nullptr,nullptr,true)){missX=x;missY=y;break;}
            }
            Require(missX>=0,"source pending input fixture has no unoccluded miss point");
            player.SendMsg(nullptr,13,0,0,0);PLAYER_SKILL_INFO row{};row.SKID=30;row.attackRange=4;
            mode.SendMsg(79,2,reinterpret_cast<msgparam_t>(&row),0);
            WindowProc(g_hMainWnd,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(missX,missY));
            WindowProc(g_hMainWnd,WM_LBUTTONUP,0,MAKELPARAM(missX,missY));
            Require(!mode.m_skillUseInfo.useType && !mode.m_hasSkillChase && !mode.m_isLeftButtonHeld
                && player.m_proceedType.type==0,"source character-target miss failed78 or started a world intent");
            QFile pendingProof(output+"/pending-skill-world-input-results.json");
            Require(pendingProof.open(QIODevice::WriteOnly),"pending world input proof open failed");
            pendingProof.write(QJsonDocument(QJsonObject{{"outcome","PASS"},{"registered_0147",true},{"virtual126_79",true},
                {"scene_backend","Vulkan"},{"real_billboard_picker",true},{"native_windowproc",true},{"skill",30},
                {"zero_level_consumed",true},{"negative_level_retained",true},{"incoming_range_snapshot",true},
                {"temporary_duplicate_range_retained",true},{"miss_consumed_without_move",true}}).toJson(QJsonDocument::Compact));
        }
    }
