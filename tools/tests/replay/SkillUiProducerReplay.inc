        {
            const auto oldSlot=*g_session.GetShortcutSlotByAbsoluteIndex(26);
            const int oldLevel=g_session.GetSkillUseLevel(30),oldHomun=g_session.GetHomunSkillUseLevel(8001),oldMerc=g_session.GetMerSkillUseLevel(8201);
            struct RestoreSkillProducers {std::function<void()> fn;~RestoreSkillProducers(){fn();}} restoreProducers{[&]{
                g_session.SetShortcutSlotByAbsoluteIndex(26,oldSlot.isSkill,oldSlot.id,oldSlot.count);
                g_session.SetSkillUseLevel(30,oldLevel);g_session.SetHomunSkillUseLevel(8001,oldHomun);g_session.SetMerSkillUseLevel(8201,oldMerc);
                SkillPacketReplayAccess::SetCurrent(&mode);
            }};
            PLAYER_SKILL_INFO row{};row.m_isValid=1;row.SKID=30;row.level=7;row.spcost=15;row.type=1;row.attackRange=12;
            g_session.SetSkillItem(row);player.SendMsg(nullptr,13,0,0,0);player.m_stateId=0;
            player.SendMsg(nullptr,41,target.m_gid,28,3);player.SendMsg(nullptr,90,0,0,0);
            for(const int level:{0,-2,65536}){
                g_session.SetSkillUseLevel(30,level);
                Require(ArmPendingSkillUseFromSkillList(30),"list activation rejected source saved level");
                Require(mode.m_skillUseInfo.useType==2 && mode.m_skillUseInfo.id==30 && mode.m_skillUseInfo.level==level
                    && mode.m_skillUseInfo.attackRange==12 && player.m_proceedType.type==3 && player.m_skillId==28,
                    "list activation lost raw saved level/copied row or changed old proceed");
            }
            const std::vector<u8> update{0x0e,1,30,0,9,0,17,0,13,0,1};
            Require(incoming.QueueBytes(update) && ProcessGameModePacketLoop(mode,incoming,receiver,1).packetsDispatched==1,"primary010E update not dispatched");
            Require(g_session.GetSkillUseLevel(30)==65536,"primary skill update reset saved use level using secondary-client behavior");
            for(int guard=0;guard<3;++guard){
                auto invalid=row;if(guard==0)invalid.m_isValid=0;else if(guard==1)invalid.level=0;else invalid.spcost=0;
                g_session.SetSkillItem(invalid);mode.SendMsg(78,0,0,0);
                Require(!ArmPendingSkillUseFromSkillList(30) && !ReplayHasPendingSkillUse(mode),"list guard accepted invalid/zero learned/zero SP row");
            }
            row.level=-1;row.spcost=-1;g_session.SetSkillItem(row);
            Require(ArmPendingSkillUseFromSkillList(30),"source nonzero list guards became positive guards");
            row.level=7;row.spcost=0;g_session.SetSkillItem(row);
            for(const unsigned short count:{static_cast<unsigned short>(0),static_cast<unsigned short>(65535),static_cast<unsigned short>(99)}){
                g_session.SetShortcutSlotByAbsoluteIndex(26,1,30,count);
                Require(ReplayRequestShortcutSlotUse(26),"shortcut incorrectly requires SP or clamps use level");
                Require(mode.m_skillUseInfo.level==(count==65535?-1:count) && mode.m_skillUseInfo.useType==2,
                    "shortcut lost source signed slot word/126 mapping");
            }
            row.spcost=15;row.type=0;g_session.SetSkillItem(row);mode.SendMsg(78,0,0,0);
            Require(ArmPendingSkillUseFromSkillList(30) && !ReplayHasPendingSkillUse(mode),"passive list row armed a cursor");
            row.type=1;g_session.SetSkillItem(row);
            g_session.SetSkillUseLevel(30,3);Require(ArmPendingSkillUseFromSkillList(30),"active list row not restored");
            mode.SendMsg(79,0,0,0);
            Require(mode.m_skillUseInfo.id==30 && mode.m_skillUseInfo.level==3 && !ReplayHasPendingSkillUse(mode),
                "null79 retained fields incorrectly own cursor");
            Require(!ArmPendingSkillUseFromSkillList(30,3),"unknown skill-list family accepted");
            for(const auto pair:std::array<std::pair<int,int>,2>{{{8001,1},{8201,2}}}){
                auto companion=row;companion.SKID=pair.first;companion.level=6;
                if(pair.second==1){g_session.SetHomunSkillItem(companion);g_session.SetHomunSkillUseLevel(pair.first,-4);}
                else{g_session.SetMercSkillItem(companion);g_session.SetMerSkillUseLevel(pair.first,-4);}
                Require(g_skillMgr.IsLevelUseSkill(pair.first)==(pair.second==1),"companion source asset level-use classification mismatch");
                Require(ArmPendingSkillUseFromSkillList(pair.first,pair.second) && mode.m_skillUseInfo.id==pair.first && mode.m_skillUseInfo.level==(pair.second==1?-4:6),
                    "list family ignored companion learned/saved storage");
                Require(!ArmPendingSkillUseFromSkillList(pair.first),"player list borrowed companion family row");
                g_session.SetShortcutSlotByAbsoluteIndex(26,1,pair.first,65535);
                Require(ReplayRequestShortcutSlotUse(26) && mode.m_skillUseInfo.id==pair.first && mode.m_skillUseInfo.level==(pair.second==1?-1:6),
                    "shortcut family lookup or signed companion level mismatch");
            }
            // Self activation sends immediately to canonical AID, then null79
            // retains the old snapshot while making it idle.
            row.type=4;g_session.SetSkillItem(row);g_session.SetSkillUseLevel(30,65536);g_session.m_aid=0x13572468;
            Require(ArmPendingSkillUseFromSkillList(30),"self list activation rejected");skillFrame(30,0,g_session.m_aid);
            Require(!ReplayHasPendingSkillUse(mode),"self activation retained active cursor");
            SkillPacketReplayAccess::SetCurrent(nullptr);
            Require(!ArmPendingSkillUseFromSkillList(30) && !ReplayRequestShortcutSlotUse(26),"activation sent through out-of-mode fallback");
            SkillPacketReplayAccess::SetCurrent(&mode);
            Require(SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(20))==0,"UI producer emitted unexpected extra frame");
            player.SendMsg(nullptr,13,0,0,0);mode.SendMsg(78,0,0,0);
        }
