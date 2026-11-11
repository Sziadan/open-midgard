    {
        Require(!g_windowMgr.m_petTamingDeceiveWnd,"capture target fixture needs a fresh taming owner");
        const auto previousWorld=mode.m_world;const auto previousActors=mode.m_runtimeActors;
        const auto previousPending=mode.m_skillUseInfo;const auto previousPet=g_session.m_petStatus;
        const auto saved=std::make_tuple(mode.m_skillChaseTargetGid,mode.m_skillChaseSkillId,mode.m_skillChaseSkillLevel,
            mode.m_skillChaseRange,mode.m_lastSkillChaseRequestTick,mode.m_hasSkillChase,mode.m_hasGroundSkillChase,
            mode.m_hasTouchSkillChase,mode.m_lastMonGid,mode.m_lastLockOnMonGid,mode.m_isCtrlLock,
            mode.m_hasAttackChaseHint,mode.m_isLeftButtonHeld,mode.m_hasHeldMoveTarget,
            g_session.m_isNoShift,g_session.m_isNoCtrl,g_session.m_homunGID,g_session.m_merGID);
        CWorld world;CPlayer player;CPc monster,pc,companion,npc;C3dAttr attr;
        attr.m_width=attr.m_height=64;attr.m_zoom=10;attr.m_cells.resize(64*64);
        for(auto& cell:attr.m_cells)cell={0,0,0,0,0};
        world.m_player=&player;world.m_attr=&attr;player.m_pos={0,0,0};player.m_isPc=1;
        monster.m_gid=0x66000001;monster.m_job=1002;monster.m_isPc=0;monster.m_pos={100,0,0};monster.m_headType=0;
        world.RegisterActor(&monster);mode.m_world=&world;mode.m_runtimeActors.clear();mode.m_runtimeActors[monster.m_gid]=&monster;
        struct RestoreCaptureTarget {std::function<void()> fn;~RestoreCaptureTarget(){fn();}} restore{[&]{
            if(g_windowMgr.m_petTamingDeceiveWnd)g_windowMgr.DeleteWindow(g_windowMgr.m_petTamingDeceiveWnd);
            world.UnregisterActor(&monster);world.m_player=nullptr;world.m_attr=nullptr;
            mode.m_world=previousWorld;mode.m_runtimeActors=previousActors;mode.m_skillUseInfo=previousPending;g_session.m_petStatus=previousPet;
            std::tie(mode.m_skillChaseTargetGid,mode.m_skillChaseSkillId,mode.m_skillChaseSkillLevel,
                mode.m_skillChaseRange,mode.m_lastSkillChaseRequestTick,mode.m_hasSkillChase,mode.m_hasGroundSkillChase,
                mode.m_hasTouchSkillChase,mode.m_lastMonGid,mode.m_lastLockOnMonGid,mode.m_isCtrlLock,
                mode.m_hasAttackChaseHint,mode.m_isLeftButtonHeld,mode.m_hasHeldMoveTarget,
                g_session.m_isNoShift,g_session.m_isNoCtrl,g_session.m_homunGID,g_session.m_merGID)=saved;
        }};
        g_session.m_petStatus.actorId=0;g_session.m_isNoShift=g_session.m_isNoCtrl=0;
        CGameModePacketRouter receiver;RegisterDefaultGameModePacketHandlers(receiver);TestConnection incoming;
        const auto receive=[&](const std::vector<u8>& bytes){
            Require(incoming.QueueBytes(bytes),"capture target queue failed");
            const auto processed=ProcessGameModePacketLoop(mode,incoming,receiver,1);
            Require(processed.packetsProcessed==1 && processed.packetsDispatched==1,"capture arm frame did not dispatch exactly once");
        };
        const auto arm=[&]{receive({0x9e,1});Require(mode.m_skillUseInfo.id==10000 && mode.m_skillUseInfo.level==0
            && mode.m_skillUseInfo.attackRange==5 && mode.m_skillUseInfo.useType==2,"019E did not arm exact source state");};
        mode.m_hasSkillChase=1;mode.m_skillChaseTargetGid=123;mode.m_skillChaseSkillId=28;mode.m_skillChaseSkillLevel=3;
        player.m_proceedType.type=3;player.m_proceedTargetGid=123;player.m_skillId=28;player.m_skillUseLevel=3;
        arm();arm();Require(mode.m_hasSkillChase && mode.m_skillChaseTargetGid==123 && mode.m_skillChaseSkillId==28
            && player.m_proceedTargetGid==123 && player.m_skillUseLevel==3,"arming/rearming cleared an existing proceed");
        const auto armState=mode.m_skillUseInfo;const u8 shortArm[]={0x9e};const u8 wrongArm[]={0x9f,1};const u8 longArm[]={0x9e,1,0};
        for(const auto& view:std::array<PacketView,4>{{{0x019e,1,shortArm},{0x019e,2,wrongArm},{0x019e,3,longArm},{0x019e,2,nullptr}}})receiver.Dispatch(mode,view);
        Require(mode.m_skillUseInfo.id==armState.id && mode.m_skillUseInfo.level==0 && mode.m_skillUseInfo.attackRange==5
            && mode.m_skillUseInfo.useType==2,"malformed capture arm changed intent");
        mode.SendMsg(CGameMode::GameMsg_LButtonDown,-10000,-10000,0);
        Require(mode.m_skillUseInfo.id==0 && mode.m_skillUseInfo.level==0 && mode.m_skillUseInfo.useType==0
            && mode.m_skillUseInfo.attackRange==5 && mode.m_hasSkillChase && mode.m_skillChaseTargetGid==123,
            "world miss did not consume only pending capture intent");
        player.SendMsg(nullptr,13,0,0,0);
        arm();Require(!ReplayTryPetCaptureOnActor(mode,&monster,true,false,false) && !mode.m_hasSkillChase,"Shift should block hostile monster capture");
        g_session.m_isNoShift=1;Require(ReplayTryPetCaptureOnActor(mode,&monster,true,false,true)
            && mode.m_hasSkillChase && mode.m_isCtrlLock==1,"noShift/control capture branch failed");
        Require(player.m_proceedType.type==3 && player.m_proceedTargetGid==monster.m_gid && player.m_skillId==10000
            && player.m_skillAttackRange==5 && player.m_skillUseLevel==0,"player41/90 capture fields differ");
        player.SendMsg(nullptr,13,0,0,0);g_session.m_isNoShift=0;
        Require(!ReplayTryPetCaptureOnActor(mode,&monster,false,true,false),"Alt monster click armed capture");
        monster.m_headType=100;Require(!ReplayTryPetCaptureOnActor(mode,&monster,false,false,false),"marked source class3 actor became capture target");
        monster.m_headType=0;
        player.m_stateId=6;player.m_skillUseLevel=9;player.m_proceedTargetGid=123;
        Require(ReplayTryPetCaptureOnActor(mode,&monster,false,false,false) && !mode.m_hasSkillChase
            && player.m_proceedTargetGid==123 && player.m_skillUseLevel==0,"state6 player41 guard or following90 mismatch");
        player.m_stateId=0;player.m_effectState=2;player.m_skillUseLevel=9;
        Require(ReplayTryPetCaptureOnActor(mode,&monster,false,false,false) && !mode.m_hasSkillChase
            && player.m_skillUseLevel==0,"burrow capture guard mismatch");player.m_effectState=0;
        player.m_proceedType.type=3;player.m_proceedTargetGid=0x66000055;player.m_skillId=28;
        player.m_skillAttackRange=4;player.m_skillUseLevel=6;
        mode.SynchronizePlayerSkillProceed(&player);
        player.m_stateId=6;
        Require(ReplayTryPetCaptureOnActor(mode,&monster,false,false,false)
            && player.m_proceedType.type==3 && player.m_proceedTargetGid==0x66000055
            && player.m_skillId==28 && player.m_skillAttackRange==4 && player.m_skillUseLevel==0
            && mode.m_hasSkillChase && !mode.m_hasGroundSkillChase && !mode.m_hasTouchSkillChase
            && mode.m_skillChaseTargetGid==0x66000055 && mode.m_skillChaseSkillId==28
            && mode.m_skillChaseRange==4 && mode.m_skillChaseSkillLevel==0
            && mode.m_skillUseInfo.id==10000 && mode.m_skillUseInfo.level==0,
            "rejected capture41 overwrote old chase or failed to sync unconditional level90");
        player.SendMsg(nullptr,13,0,0,0);player.m_stateId=0;
        Require(!mode.m_hasSkillChase && mode.m_skillUseInfo.id==10000,
            "rejected capture old-chase fixture did not retire cleanly");
        pc.m_isPc=1;pc.m_job=1;pc.m_gid=g_session.m_gid==g_session.m_aid?0x66000002:g_session.m_gid;
        Require(!ReplayTryPetCaptureOnActor(mode,&pc,false,false,false),"unshifted non-PK PC captured");
        Require(ReplayTryPetCaptureOnActor(mode,&pc,true,false,false) && mode.m_skillChaseTargetGid==pc.m_gid,"Shift PC target or char-ID collision handling mismatch");
        player.SendMsg(nullptr,13,0,0,0);pc.m_pkState=1;
        Require(ReplayTryPetCaptureOnActor(mode,&pc,false,false,false),"PK PC capture target was rejected");
        player.SendMsg(nullptr,13,0,0,0);Require(!ReplayTryPetCaptureOnActor(mode,&player,true,false,false),"character-target capture allowed self");
        companion.m_gid=0x66000003;companion.m_job=6001;companion.m_objectType=9;g_session.m_homunGID=companion.m_gid;
        world.m_isPKZone=1;Require(!ReplayTryPetCaptureOnActor(mode,&companion,true,false,false),"owned homunculus capture exclusion lost");
        g_session.m_homunGID=0;Require(ReplayTryPetCaptureOnActor(mode,&companion,false,false,false),"foreign PK homunculus target rejected");
        player.SendMsg(nullptr,13,0,0,0);companion.m_job=1002;companion.m_headType=100;world.m_isPKZone=0;
        g_session.m_merGID=companion.m_gid;Require(!ReplayTryPetCaptureOnActor(mode,&companion,true,false,false),"owned companion exclusion lost");
        g_session.m_merGID=0;Require(ReplayTryPetCaptureOnActor(mode,&companion,true,false,false),"class4 override of class3 was ignored");
        player.SendMsg(nullptr,13,0,0,0);npc.m_gid=0x66000004;npc.m_isPc=0;npc.m_job=111;npc.m_pos={100,0,50};
        player.m_roty=0;player.m_headDir=0;player.m_stateId=0;
        Require(ReplayTryPetCaptureOnActor(mode,&npc,false,false,false) && !mode.m_hasSkillChase,"capture targeting suppressed source NPC contact");
        const auto npcFrames=peer.ReceiveExact(18);
        Require(npcFrames[0]==0x85 && npcFrames[1]==0 && npcFrames[7]==2 && npcFrames[8]==0 && npcFrames[10]==5
            && std::vector<u8>(npcFrames.begin()+11,npcFrames.end())==std::vector<u8>({0x90,0,4,0,0,0x66,1})
            && player.m_headDir==2 && std::abs(player.m_roty-72.645999f)<0.001f && player.m_curMotion==2,
            "NPC capture click did not preserve continuous source turn then contact ordering");
        player.m_stateId=2;Require(!ReplayTryPetCaptureOnActor(mode,&npc,false,false,false),"moving player incorrectly contacted NPC");
        npc.m_job=45;player.m_stateId=0;Require(!ReplayTryPetCaptureOnActor(mode,&npc,false,false,false),"portal became capture/NPC request");
        Require(SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(20))==0,"ineligible NPC/portal emitted a request");
        player.SendMsg(nullptr,13,0,0,0);arm();Require(ReplayTryPetCaptureOnActor(mode,&monster,false,false,false),"ordinary capture target rejected");
        mode.m_skillUseInfo.id=0;mode.m_skillUseInfo.level=0;mode.m_skillUseInfo.useType=0;
        player.m_stateId=2;ReplayPumpSkillChase(mode);
        Require(mode.m_hasSkillChase && !g_windowMgr.m_petTamingDeceiveWnd,"approach state2 gate opened capture window");
        player.m_stateId=0;ReplayPumpSkillChase(mode);
        const auto move=peer.ReceiveExact(8);
        Require(move[0]==0xa7 && move[1]==0 && move[5]==9 && move[6]==0xc2 && move[7]==0
            && player.m_destCellX==39 && player.m_destCellZ==32 && !g_windowMgr.m_petTamingDeceiveWnd,
            "capture approach did not use source range5 projected cell");
        ReplayPumpSkillChase(mode);Require(SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(20))==0,"duplicate approach sent another packet");
        player.m_pos.x=75;ReplayPumpSkillChase(mode);
        auto* window=g_windowMgr.m_petTamingDeceiveWnd;
        Require(window && window->GetTargetGid()==monster.m_gid && !mode.m_hasSkillChase
            && player.m_proceedType.type==0 && !player.m_isAttackRequest,"in-range capture did not hand off76/clear13");
        Require(SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(20))==0,"approach/handoff sent ordinary skill or premature capture request");
        window->OnLBtnDown(window->m_x+1,window->m_y+1);
        Require(peer.ReceiveExact(6)==std::vector<u8>({0x9f,1,1,0,0,0x66}),"019E target/approach/window click chain sent wrong request");
        receive({0xa0,1,1});Require(window->GetDestination()==7 && window->GetDelay()==7,"capture chain result did not reach owner");
        g_windowMgr.DeleteWindow(window);
        player.m_pos.x=0;arm();ReplayTryPetCaptureOnActor(mode,&monster,false,false,false);
        mode.m_runtimeActors.erase(monster.m_gid);ReplayPumpSkillChase(mode);
        Require(!mode.m_hasSkillChase && player.m_proceedType.type==0 && !g_windowMgr.m_petTamingDeceiveWnd,
            "missing target did not retire capture proceed without owner");

        auto vanishTargetOwner=std::make_unique<CPc>();auto* vanishTarget=vanishTargetOwner.get();
        vanishTarget->m_gid=0x66000005;vanishTarget->m_job=1002;vanishTarget->m_isPc=0;vanishTarget->m_pos={100,0,0};
        world.RegisterActor(vanishTarget);mode.m_runtimeActors[vanishTarget->m_gid]=vanishTarget;
        struct RestoreVanishTarget {CGameMode& mode;CWorld& world;CPc* actor;~RestoreVanishTarget(){
            auto it=mode.m_runtimeActors.find(actor->m_gid);if(it!=mode.m_runtimeActors.end()&&it->second==actor)mode.m_runtimeActors.erase(it);
            world.UnregisterActor(actor);
            mode.m_deferredActorDeletes.erase(std::remove(mode.m_deferredActorDeletes.begin(),mode.m_deferredActorDeletes.end(),actor),mode.m_deferredActorDeletes.end());
        }} restoreVanishTarget{mode,world,vanishTarget};
        arm();Require(ReplayTryPetCaptureOnActor(mode,vanishTarget,false,false,false),"vanish fixture capture target was rejected");
        arm(); // Keep a manually armed capture intent pending while the queued target disappears.
        Require(player.m_proceedTargetGid==vanishTarget->m_gid&&mode.m_hasSkillChase
            &&mode.m_skillUseInfo.id==10000,"vanish fixture did not retain both queued target and pending manual intent");
        mode.SendMsg(76,10000,vanishTarget->m_gid,0);
        Require(g_windowMgr.m_petTamingDeceiveWnd&&g_windowMgr.m_petTamingDeceiveWnd->GetTargetGid()==vanishTarget->m_gid,
            "vanish fixture did not create an unsubmitted target window");
        receive({0x80,0x00,0x05,0x00,0x00,0x66,0x00});
        Require(mode.m_runtimeActors.count(vanishTarget->m_gid)==0&&!mode.m_hasSkillChase
            &&player.m_proceedType.type==0&&player.m_proceedTargetGid==0,
            "production 0080 removal did not retire the queued capture proceed");
        Require(mode.m_skillUseInfo.id==10000&&mode.m_skillUseInfo.level==0&&mode.m_skillUseInfo.useType==2,
            "target removal consumed an already-pending manual capture intent");
        Require(!g_windowMgr.m_petTamingDeceiveWnd,"target removal retained an unsubmitted taming window");
        ReplayPumpSkillChase(mode);
        Require(SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(20))==0,
            "retired capture target generated another replay packet");
    }
