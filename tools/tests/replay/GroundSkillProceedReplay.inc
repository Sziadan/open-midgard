        {
            const auto savedManner=ro::GetMannerFilter();
            const auto oldFocus=g_windowMgr.m_editWindow;
            const auto oldGroundWindows=g_windowMgr.m_groundSkillTextWindows;
            struct RestoreGroundReplay {std::function<void()> fn;~RestoreGroundReplay(){fn();}} restoreGround{[&]{
                for(std::size_t i=0;i<2;++i)if(g_windowMgr.m_groundSkillTextWindows[i]!=oldGroundWindows[i])
                    g_windowMgr.DeleteWindow(g_windowMgr.m_groundSkillTextWindows[i]);
                ro::GetMannerFilter()=savedManner;g_windowMgr.m_editWindow=oldFocus;player.m_effectState=0;player.SendMsg(nullptr,13,0,0,0);
            }};
            const auto groundFrame=[&](int skill,int level,int x,int y,const char* text=nullptr){
                std::vector<u8> expected(text?102:22,0);expected[0]=text?0x7e:0x13;expected[1]=text?0:1;
                const auto word=[&](int offset,int value){expected[offset]=static_cast<u8>(value);expected[offset+1]=static_cast<u8>(static_cast<unsigned>(value)>>8);};
                word(5,level);word(9,skill);word(12,x);word(20,y);
                if(text)std::memcpy(expected.data()+22,text,std::strlen(text));
                Require(peer.ReceiveExact(expected.size())==expected,"source ground/text frame mismatch");
            };
            PLAYER_SKILL_INFO ground{};ground.SKID=89;ground.level=7;ground.attackRange=99;ground.m_isValid=0;g_session.SetSkillItem(ground);
            for(const int requested:{0,-2,65536,99}){
                const std::array<int,4> values{{89,-1,65536,requested}};
                Require(mode.SendMsg(77,reinterpret_cast<msgparam_t>(&values),0,0)==0,"source77 changed NOSKILL return");
                groundFrame(89,requested?requested:7,-1,0);
            }
            PLAYER_SKILL_INFO companion{};companion.SKID=8001;companion.level=3;g_session.SetSkillItem(companion);
            companion.level=9;companion.m_isValid=1;g_session.SetHomunSkillItem(companion);
            const std::array<int,4> family{{8001,5,6,0}};mode.SendMsg(77,reinterpret_cast<msgparam_t>(&family),0,0);groundFrame(8001,3,5,6);
            const std::array<int,4> absent{{9999,0,0,0}};mode.SendMsg(77,reinterpret_cast<msgparam_t>(&absent),0,0);groundFrame(9999,0,0,0);
            player.m_stateId=0;player.m_effectState=0;player.SendMsg(nullptr,41,target.m_gid,28,3);
            player.m_effectState=2;mode.m_skillUseInfo.attackRange=1;
            player.SendMsg(nullptr,65,592,32,32);player.SendMsg(nullptr,90,-2,0,0);
            Require(player.m_proceedType.type==3 && player.m_skillId==28 && player.m_skillUseLevel==-2,
                "ground65 burrow rejection/90 overwrite differs from actor41 allowlist");
            player.SendMsg(nullptr,65,137,32,32);
            Require(player.m_proceedType.type==4 && player.m_skillId==137 && !player.m_proceedTargetGid,
                "ground65 source burrow exception failed");player.m_effectState=0;
            float wx=0,wz=0;mode.GetClientCoor(32,32,wx,wz);player.m_pos={wx,0,wz};
            for(const int state:{0,1,4,16}){
                player.m_stateId=0;player.SendMsg(nullptr,65,89,32,32);player.SendMsg(nullptr,90,0,0,0);player.m_stateId=state;
                Require(player.m_skillAttackRange==1 && mode.m_groundSkillRange==1 && mode.m_hasGroundSkillChase,
                    "ground65 borrowed learned range or failed PT4 mirroring");
                ReplayPumpSkillChase(mode);groundFrame(89,7,32,32);
                Require(player.m_proceedType.type==0 && !mode.m_hasSkillChase && !mode.m_hasGroundSkillChase,
                    "ground66/77 did not retire through13 after attempt");
            }
            for(const int state:{-1,2,3,6}){
                player.m_stateId=0;player.SendMsg(nullptr,65,89,32,32);player.SendMsg(nullptr,90,-2,0,0);player.m_stateId=state;
                ReplayPumpSkillChase(mode);Require(player.m_proceedType.type==4,"blocked ground state retired intent");
                Require(SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(10))==0,"blocked ground state sent request");
            }
            player.m_stateId=0;int sx=0,sy=0;mode.GetServerCoor(player.m_pos.x,player.m_pos.z,sx,sy);
            player.SendMsg(nullptr,65,89,0,0);player.SendMsg(nullptr,90,-2,0,0);player.m_destCellX=sx;player.m_destCellZ=sy;
            ReplayPumpSkillChase(mode);groundFrame(89,-2,0,0);
            player.SendMsg(nullptr,65,89,32,32);ReplayClearSkillChase(mode);
            Require(player.m_proceedType.type==0 && !mode.m_hasGroundSkillChase,"external chase interruption left canonical PT4 alive");
            // 77 tests the raw DWORD level, whereas144 tests its wire WORD.
            GroundSkillTextRequest direct{{{65536+89,-1,65536,65536}},"fixture"};
            Require(mode.SendMsg(144,reinterpret_cast<msgparam_t>(&direct),0,0)==0,"source144 changed NOSKILL return");groundFrame(89,7,-1,0,"fixture");
            for(const int id:{125,220}){
                auto textRow=ground;textRow.SKID=id;textRow.level=4;g_session.SetSkillItem(textRow);
                player.SendMsg(nullptr,65,id,32,32);player.SendMsg(nullptr,90,0,0,0);ReplayPumpSkillChase(mode);
                auto* window=g_windowMgr.m_groundSkillTextWindows[id==125?0:1];
                Require(window && window->GetValues()==std::array<int,4>{{id,32,32,0}} && player.m_proceedType.type==0,
                    "ground66 did not create/copy text window before13");
                Require(SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(10))==0,"text skill sent before editor submission");
                window->GetEditCtrl()->SetText("fixture");window->SendMsg(nullptr,1,0,0,0);g_windowMgr.OnProcess();
                Require(g_windowMgr.ContainsWindow(window),"source default cancel117 incorrectly became119");
                window->SendMsg(nullptr,0,0,0,0);groundFrame(id,4,32,32,"fixture");
                Require(g_windowMgr.ContainsWindow(window),"text OK closed synchronously instead of posting");
                g_windowMgr.OnProcess();Require(!g_windowMgr.m_groundSkillTextWindows[id==125?0:1],"text OK deferred close not processed");
                player.SendMsg(nullptr,65,id,32,32);player.SendMsg(nullptr,90,-2,0,0);ReplayPumpSkillChase(mode);
                window=g_windowMgr.m_groundSkillTextWindows[id==125?0:1];window->SendMsg(nullptr,6,119,0,0);
                Require(SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(10))==0,"cancel sent text skill");g_windowMgr.OnProcess();Require(!g_windowMgr.m_groundSkillTextWindows[id==125?0:1],"text cancel deferred close not processed");
            }
            ro::GetMannerFilter().AppendEncodedFile("sourcegroundprohibited#\r",1252);
            GroundSkillTextRequest rejected{{{125,32,32,1}},"sourcegroundprohibited"};
            const auto notices=g_windowMgr.GetChatEvents().size();
            Require(!mode.SendMsg(144,reinterpret_cast<msgparam_t>(&rejected),0,0)
                && g_windowMgr.GetChatEvents().size()==notices+1,"text144 filter did not reject and emit source notice");
            Require(SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(10))==0,"ground replay left unexpected frame");
        }
