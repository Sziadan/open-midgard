        {
            const auto saved=std::make_tuple(g_session.m_weight,g_session.m_maxWeight,g_session.m_isNoShift,g_session.m_isNoCtrl);
            const auto savedPos=player.m_pos;
            struct RestoreSourceActor {std::function<void()> fn;~RestoreSourceActor(){fn();}} restoreSourceActor{[&]{
                std::tie(g_session.m_weight,g_session.m_maxWeight,g_session.m_isNoShift,g_session.m_isNoCtrl)=saved;
                player.m_pos=savedPos;player.SendMsg(nullptr,13,0,0,0);
            }};
            const auto status=[&](u16 type,u32 value,u16 opcode=0x00b0){
                std::vector<u8> packet{static_cast<u8>(opcode),static_cast<u8>(opcode>>8),static_cast<u8>(type),static_cast<u8>(type>>8),
                    static_cast<u8>(value),static_cast<u8>(value>>8),static_cast<u8>(value>>16),static_cast<u8>(value>>24)};
                Require(incoming.QueueBytes(packet),"weight status queue failed");
                Require(ProcessGameModePacketLoop(mode,incoming,receiver,1).packetsDispatched==1,"weight status did not reach registered owner");
            };
            g_session.m_weight=g_session.m_maxWeight=0;g_windowMgr.ClearChatEvents();
            status(24,4999);Require(g_session.m_weight==499 && !g_session.m_maxWeight,"weight producer lost source /10 truncation");
            status(25,10009);Require(g_session.m_maxWeight==1000,"maxWeight producer lost source /10 truncation");
            auto count=g_windowMgr.GetChatEvents().size();
            status(24,5000);Require(g_windowMgr.GetChatEvents().size()==count+1,"weight50 crossing notice missing");
            Require(g_windowMgr.GetChatEvents().back().text=="WEIGHT NO RECOVER"
                && g_windowMgr.GetChatEvents().back().color==0xff0000
                && g_windowMgr.GetChatEvents().back().source.type==ro::chat::NoticeType::Status,"weight crossing lost source message/color/type");
            count=g_windowMgr.GetChatEvents().size();status(24,8999);
            Require(g_session.m_weight==899 && g_windowMgr.GetChatEvents().size()==count,"weight89 introduced a crossing notice");
            status(24,9000);Require(g_windowMgr.GetChatEvents().size()==count+1,"weight90 crossing notice missing");
            count=g_windowMgr.GetChatEvents().size();status(25,5000);
            Require(g_windowMgr.GetChatEvents().size()==count,"source maxWeight decrease invented a warning");
            status(25,20000);Require(g_windowMgr.GetChatEvents().size()==count+2,"source maxWeight increase did not release both thresholds");
            Require(g_windowMgr.GetChatEvents()[count].text=="WEIGHT RECOVER RESTORED"
                && g_windowMgr.GetChatEvents()[count+1].text=="WEIGHT ATTACK RESTORED"
                && g_windowMgr.GetChatEvents()[count+1].color==0x00ffff,"weight release ordering/color differ");
            status(24,18000);count=g_windowMgr.GetChatEvents().size();status(24,9800);
            Require(g_windowMgr.GetChatEvents().size()==count+2,"weight decrease failed source release notices");
            status(24,0xffffffffu);Require(g_session.m_weight==0,"source weight wire-1 did not divide signed toward zero");
            status(24,0x80000000u);Require(g_session.m_weight==-214748364 && ro::stats::SourceWeightPercent(g_session.m_weight,1)==80,
                "selected signed weight decode or wrapped signed percent changed");
            status(25,0x80000000u);Require(g_session.m_maxWeight==-214748364
                && ro::stats::SourceWeightPercent(g_session.m_weight,g_session.m_maxWeight)==80,"negative maxWeight denominator was not clamped");
            status(24,0);status(25,10000);status(24,9999,0x00b1);
            Require(g_session.m_weight==0,"long parameter packet acquired unsupported weight producer behavior");
            g_session.m_isNoShift=0;g_session.m_isNoCtrl=0;player.m_pos.x=95;
            const auto armSource=[&](int id,int level=0,int range=3,int useType=2){
                PLAYER_SKILL_INFO row{};row.m_isValid=1;row.SKID=id;row.attackRange=range;
                mode.SendMsg(79,useType,reinterpret_cast<msgparam_t>(&row),level);
            };
            autorun(30,1,0,7,3,0);
            Require(ReplayTrySourceSkillOnActor(mode,&target,false,false,false) && player.m_proceedType.type==3
                && player.m_skillId==30 && player.m_skillUseLevel==0 && player.m_skillAttackRange==3,
                "temporary-only source selector/range/raw0 did not arm canonical actor proceed");
            ReplayPumpSkillChase(mode);skillFrame(30,0);
            armSource(30,-2);Require(ReplayTrySourceSkillOnActor(mode,&target,false,false,false) && player.m_skillUseLevel==-2,
                "source negative requested level truncated before player90");ReplayPumpSkillChase(mode);skillFrame(30,0xfffe);
            armSource(30,65536);ReplayTrySourceSkillOnActor(mode,&target,false,false,false);
            Require(player.m_skillUseLevel==65536,"source raw32 level truncated while arming");ReplayPumpSkillChase(mode);skillFrame(30,0);
            armSource(30,1);Require(!ReplayTrySourceSkillOnActor(mode,&target,true,false,false),"harmful monster skill ignored Shift");
            g_session.m_isNoShift=1;Require(ReplayTrySourceSkillOnActor(mode,&target,true,false,false),"NoShift failed monster override");
            player.SendMsg(nullptr,13,0,0,0);Require(!ReplayTrySourceSkillOnActor(mode,&target,false,true,false),"Alt allowed ordinary monster skill");
            g_session.m_isNoShift=0;armSource(28,1);
            Require(!ReplayTrySourceSkillOnActor(mode,&target,false,false,false) && ReplayTrySourceSkillOnActor(mode,&target,true,false,false),
                "good monster skill did not invert source Shift policy");player.SendMsg(nullptr,13,0,0,0);
            CPc pc;pc.m_isPc=1;pc.m_gid=0x77000003;g_session.m_aid=pc.m_gid;
            armSource(28,1);Require(!ReplayTrySourceSkillOnActor(mode,&pc,false,false,false),"single-character skill selected canonical self");
            armSource(28,1,3,4);Require(ReplayTrySourceSkillOnActor(mode,&pc,false,false,false),"source ALL selector failed canonical self override");
            player.SendMsg(nullptr,13,0,0,0);g_session.m_aid=0;
            player.m_isPc=1;player.m_gid=0x55000001;
            armSource(28,1);Require(!ReplayTrySourceSkillOnActor(mode,&player,false,false,false),
                "single-character skill selected local player with distinct stored ID");
            armSource(28,1,3,4);Require(ReplayTrySourceSkillOnActor(mode,&player,false,false,false),
                "ALL selector failed local player representation override");player.SendMsg(nullptr,13,0,0,0);
            armSource(30,1);g_session.m_isNoShift=1;
            Require(!ReplayTrySourceSkillOnActor(mode,&pc,false,false,false),"fresh PC click acquired blanket NoShift override");
            pc.m_pkState=1;Require(ReplayTrySourceSkillOnActor(mode,&pc,false,false,false),"PC PK-state override missing");player.SendMsg(nullptr,13,0,0,0);
            CPc companion;companion.m_job=6017;companion.m_gid=0x77000004;g_session.m_merGID=companion.m_gid;
            armSource(28,1);Require(!ReplayTrySourceSkillOnActor(mode,&companion,false,false,false),"single-character skill selected own mercenary");
            armSource(28,1,3,4);Require(ReplayTrySourceSkillOnActor(mode,&companion,false,false,false),"ALL selector failed own mercenary override");
            player.SendMsg(nullptr,13,0,0,0);
            CPc homun;homun.m_job=6001;homun.m_gid=0x77000005;g_session.m_homunGID=homun.m_gid;
            armSource(28,1);Require(!ReplayTrySourceSkillOnActor(mode,&homun,false,false,false),"homunculus escaped PK/siege guard");
            armSource(231,1);Require(ReplayTrySourceSkillOnActor(mode,&homun,false,false,false),"source231 homunculus exception missing");
            player.SendMsg(nullptr,13,0,0,0);g_session.m_isNoShift=0;
            status(24,8999);armSource(30,1);Require(ReplayTrySourceSkillOnActor(mode,&target,false,false,true) && player.m_proceedType.type==3,
                "source weight89 incorrectly blocked actor skill");
            const int retainedSkill=player.m_skillId,retainedLevel=player.m_skillUseLevel;
            status(24,9000);armSource(28,0);Require(ReplayTrySourceSkillOnActor(mode,&target,true,false,true)
                && player.m_skillId==retainedSkill && player.m_skillUseLevel==retainedLevel && mode.m_isCtrlLock,
                "overweight guard replaced old proceed/level or lost source control state");
            armSource(10000,0,5);Require(ReplayTrySourceSkillOnActor(mode,&target,false,false,false) && player.m_skillId==10000,
                "capture lost source weight exemption");player.SendMsg(nullptr,13,0,0,0);
            mode.m_skillUseInfo={30,-2,9,2};g_isGoodSkillHanging=true;
            mode.SendMsg(78,0,0,0);Require(mode.m_skillUseInfo.id==0 && mode.m_skillUseInfo.level==0 && mode.m_skillUseInfo.useType==0
                && mode.m_skillUseInfo.attackRange==9 && !g_isGoodSkillHanging,"source78 clearing/retained range/global differ");
        }
