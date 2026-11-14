    {
        Require(g_modeMgr.GetCurrentGameMode()==&mode,"player proceed fixture requires current game mode");
        const auto normal=g_session.GetSkillItems(),temporary=g_session.GetTemporarySkillItems();
        const auto homun=g_session.GetHomunSkillItems(),merc=g_session.GetMercSkillItems();
        const auto oldWorld=mode.m_world;const auto oldActors=mode.m_runtimeActors;
        const auto oldCommerce=ro::commerce::state;auto* oldRoom=g_windowMgr.m_chatRoomWnd;auto* oldCart=g_windowMgr.m_selectCartWnd;
        const u32 oldSp=g_session.GetCharacterVitals().sp;
        const auto oldPending=mode.m_skillUseInfo;const bool oldGood=g_isGoodSkillHanging;
        const auto oldIdentity=std::make_tuple(g_session.m_aid,g_session.m_gid,g_session.m_homunGID,g_session.m_merGID);
        const auto oldChase=std::make_tuple(mode.m_skillChaseTargetGid,mode.m_skillChaseSkillId,mode.m_skillChaseSkillLevel,
            mode.m_skillChaseRange,mode.m_hasSkillChase,mode.m_hasGroundSkillChase,mode.m_hasTouchSkillChase);
        CWorld world;CPlayer player;CPc target;C3dAttr attr;CSkill unit;
        attr.m_width=attr.m_height=64;attr.m_zoom=10;attr.m_cells.resize(64*64);
        world.m_attr=&attr;world.m_player=&player;world.m_isPKZone=0;world.m_isSiegeMode=0;player.m_pos={75,0,0};player.m_stateId=0;
        target.m_gid=0x77000001;target.m_job=1002;target.m_isPc=0;target.m_headType=0;target.m_pos={100,0,0};world.RegisterActor(&target);
        mode.m_world=&world;mode.m_runtimeActors.clear();mode.m_runtimeActors[target.m_gid]=&target;
        struct RestorePlayerProceed {std::function<void()> fn;~RestorePlayerProceed(){fn();}} restore{[&]{
            if(g_windowMgr.m_selectCartWnd && g_windowMgr.m_selectCartWnd!=oldCart)g_windowMgr.DeleteWindow(g_windowMgr.m_selectCartWnd);
            g_windowMgr.m_chatRoomWnd=oldRoom;ro::commerce::state=oldCommerce;
            world.UnregisterActor(&target);world.m_player=nullptr;world.m_attr=nullptr;
            world.m_skillList.clear();
            mode.m_world=oldWorld;mode.m_runtimeActors=oldActors;
            std::tie(mode.m_skillChaseTargetGid,mode.m_skillChaseSkillId,mode.m_skillChaseSkillLevel,
                mode.m_skillChaseRange,mode.m_hasSkillChase,mode.m_hasGroundSkillChase,mode.m_hasTouchSkillChase)=oldChase;
            g_session.ClearSkillItems();for(const auto& row:normal)g_session.SetSkillItem(row);
            g_session.ClearTemporarySkillItems();for(const auto& row:temporary)g_session.AddTemporarySkillItem(row);
            g_session.ClearHomunSkillItems();for(const auto& row:homun)g_session.SetHomunSkillItem(row);
            g_session.ClearMercSkillItems();for(const auto& row:merc)g_session.SetMercSkillItem(row);
            g_session.UpdateCharacterVital(7,oldSp);
            mode.m_skillUseInfo=oldPending;g_isGoodSkillHanging=oldGood;
            std::tie(g_session.m_aid,g_session.m_gid,g_session.m_homunGID,g_session.m_merGID)=oldIdentity;
        }};
        g_windowMgr.m_chatRoomWnd=nullptr;ro::commerce::state.vendingActive=false;
        g_session.ClearSkillItems();g_session.ClearTemporarySkillItems();g_session.ClearHomunSkillItems();g_session.ClearMercSkillItems();
        PLAYER_SKILL_INFO learned{};learned.m_isValid=1;learned.SKID=28;learned.level=7;g_session.SetSkillItem(learned);
        const auto skillFrame=[&](u16 skill,u16 level,u32 gid=0x77000001){
            Require(peer.ReceiveExact(10)==std::vector<u8>({0x38,4,static_cast<u8>(level),static_cast<u8>(level>>8),
                static_cast<u8>(skill),static_cast<u8>(skill>>8),static_cast<u8>(gid),static_cast<u8>(gid>>8),
                static_cast<u8>(gid>>16),static_cast<u8>(gid>>24)}),"source player skill dispatch frame mismatch");
        };
        const auto begin=[&](int id,int level){player.SendMsg(nullptr,41,target.m_gid,id,5);player.SendMsg(nullptr,90,level,0,0);};
        begin(28,3);Require(player.m_proceedType.type==3 && mode.m_hasSkillChase && mode.m_skillChaseSkillLevel==3,
            "source player41/90 did not synchronize canonical proceed and mode transport");
        player.m_stateId=6;player.SendMsg(nullptr,41,0x77000002,10000,9);player.SendMsg(nullptr,90,0,0,0);
        Require(player.m_skillId==28 && player.m_proceedTargetGid==target.m_gid && player.m_skillAttackRange==5
            && player.m_skillUseLevel==0 && mode.m_hasSkillChase && mode.m_skillChaseSkillId==28
            && mode.m_skillChaseTargetGid==target.m_gid && mode.m_skillChaseSkillLevel==0,
            "rejected capture41 lost old proceed or following90 failed to synchronize zero");
        ReplayPumpSkillChase(mode);Require(mode.m_hasSkillChase && SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(20))==0,
            "state6 pump dispatched or retired an old proceed");
        player.m_stateId=0;ReplayPumpSkillChase(mode);skillFrame(28,7);
        Require(!mode.m_hasSkillChase && player.m_proceedType.type==0 && player.m_skillId==28
            && player.m_proceedTargetGid==target.m_gid && player.m_skillUseLevel==0,"source42/13 did not dispatch learned fallback then retain stored fields");
        begin(28,2);player.m_effectState=2;player.SendMsg(nullptr,41,999,10000,9);player.SendMsg(nullptr,90,0,0,0);
        Require(player.m_proceedType.type==3 && player.m_skillId==28 && mode.m_skillChaseSkillLevel==0,
            "burrow rejection cancelled/replaced old proceed");ReplayPumpSkillChase(mode);skillFrame(28,7);player.m_effectState=0;
        begin(28,3);player.SendMsg(nullptr,15,0,0,0);Require(player.m_proceedType.type==3 && mode.m_hasSkillChase,
            "mouse release message15 cancelled PT_SKILL");
        player.m_isAttackRequest=1;player.SendMsg(nullptr,13,0,0,0);
        Require(!mode.m_hasSkillChase && !player.m_isAttackRequest && player.m_proceedType.type==0
            && player.m_skillId==28 && player.m_skillUseLevel==3,"explicit source13 clear/retained fields differ");
        begin(28,0x10000);ReplayPumpSkillChase(mode);skillFrame(28,0);
        begin(28,-1);ReplayPumpSkillChase(mode);skillFrame(28,0xffff);
        begin(0,0);ReplayPumpSkillChase(mode);skillFrame(0,0);
        player.m_effectState=2;for(int allowed:{137,212,529,530,592}){
            begin(allowed,4);Require(player.m_skillId==allowed && mode.m_hasSkillChase,"source burrow exception rejected");
        }player.m_effectState=0;player.SendMsg(nullptr,13,0,0,0);
        CGameModePacketRouter receiver;RegisterDefaultGameModePacketHandlers(receiver);TestConnection incoming;
        const auto autorun=[&](u16 id,u32 type,u16 level,u16 sp,u16 range,u8 flag){
            std::vector<u8> p(39);p[0]=0x47;p[1]=1;p[2]=static_cast<u8>(id);p[3]=static_cast<u8>(id>>8);
            for(int i=0;i<4;++i)p[4+i]=static_cast<u8>(type>>(8*i));
            for(const auto pair:std::array<std::pair<int,u16>,3>{{{8,level},{10,sp},{12,range}}}){
                p[pair.first]=static_cast<u8>(pair.second);p[pair.first+1]=static_cast<u8>(pair.second>>8);
            }p[38]=flag;Require(incoming.QueueBytes(p),"temporary skill frame queue failed");
            const auto result=ProcessGameModePacketLoop(mode,incoming,receiver,1);
            Require(result.packetsDispatched==1,"0147 temporary producer did not dispatch");
        };
        autorun(30,2,0xfffe,0xfffc,0xfffd,0x80);
        const auto first=*g_session.GetTemporarySkillItemBySkillId(30);
        Require(first.m_isValid && first.level==-2 && first.spcost==-4 && first.attackRange==-3 && first.upgradable==0x80
            && !g_session.GetSkillItemBySkillId(30),"temporary skill source widths/separate ownership differ");
        autorun(30,0xdeadbeef,9,7,99,1);const auto* updated=g_session.GetTemporarySkillItemBySkillId(30);
        Require(updated->level==9 && updated->spcost==7 && updated->upgradable==1 && updated->type==2
            && updated->attackRange==-3 && updated->skillName==first.skillName,"temporary duplicate replaced retained metadata");
        begin(30,0);ReplayPumpSkillChase(mode);skillFrame(30,9);
        learned.m_isValid=0;learned.level=15;g_session.SetSkillItem(learned);
        PLAYER_SKILL_INFO temp=learned;temp.m_isValid=1;temp.level=4;g_session.AddTemporarySkillItem(temp);
        begin(28,0);ReplayPumpSkillChase(mode);skillFrame(28,4);
        learned.m_isValid=1;learned.level=7;g_session.SetSkillItem(learned);
        begin(28,0);ReplayPumpSkillChase(mode);skillFrame(28,7);
        for(const auto pair:std::array<std::pair<int,int>,2>{{{8000,6},{8200,9}}}){
            PLAYER_SKILL_INFO row{};row.m_isValid=1;row.SKID=pair.first;row.level=1;g_session.SetSkillItem(row);
            row.level=pair.second;if(pair.first==8000)g_session.SetHomunSkillItem(row);else g_session.SetMercSkillItem(row);
            player.m_stateId=6;mode.SendMsg(76,row.SKID,target.m_gid,0);skillFrame(static_cast<u16>(row.SKID),static_cast<u16>(row.level));
        }player.m_stateId=0;
        ro::commerce::state.vendingActive=true;mode.SendMsg(76,28,target.m_gid,3);
        Require(SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(20))==0,"own vending did not block ordinary source skill");
        ro::commerce::state.vendingActive=false;
        {UIChatRoomWnd room;g_windowMgr.m_chatRoomWnd=&room;mode.SendMsg(76,28,target.m_gid,3);
            Require(SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(20))==0,"chat-room owner did not block ordinary skill");g_windowMgr.m_chatRoomWnd=nullptr;}
        mode.SendMsg(76,20000,target.m_gid,0);
        Require(SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(20))==0,
            "dormant couple skill sent a request with no selected parser");
        mode.SendMsg(76,20001,target.m_gid,0);Require(peer.ReceiveExact(6)==std::vector<u8>({0xf9,1,1,0,0,0x77}),"source special20001 frame mismatch");
        g_session.UpdateCharacterVital(7,39);mode.SendMsg(76,154,target.m_gid,1);
        Require(SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(20))==0,"cart source76 ignored SP40 guard");
        g_session.UpdateCharacterVital(7,0x80000028u);player.m_effectState=8;mode.SendMsg(76,154,target.m_gid,1);
        Require(SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(20))==0,"cart source76 SP comparison lost signed semantics");
        g_session.UpdateCharacterVital(7,40);player.m_effectState=0;mode.SendMsg(76,154,target.m_gid,1);
        Require(SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(20))==0,"cart source76 ignored cart-effect guard");
        player.m_effectState=8;mode.SendMsg(76,154,target.m_gid,1);skillFrame(154,1);
        Require(g_windowMgr.m_selectCartWnd,"source cart recipient missing before request fallthrough");player.m_effectState=0;
        CPlayer other;other.SendMsg(nullptr,41,999,137,2);other.SendMsg(nullptr,90,7,0,0);
        Require(!mode.m_hasSkillChase && other.m_proceedType.type==3,"unrelated player overwrote current mode proceed");
        // Actual0147 -> virtual126 ->79/76; incoming stack row, not the
        // potentially older metadata retained in temporary storage, drives126.
        g_skillMgr.EnsureLoaded();
        const auto* decreaseAgi=g_skillMgr.GetSkillMetadata(30);
        Require(decreaseAgi && decreaseAgi->levelSpCosts==std::vector<int>({15,17,19,21,23,25,27,29,31,33}),
            "source numeric hash tokens were not retained as SP amounts");
        Require(g_skillMgr.IsLevelUseSkill(30),"level-use30 lacks source SP table");
        Require(!g_skillMgr.IsLevelUseSkill(4),"passive4 incorrectly has a level-use source SP table");
        Require(g_skillMgr.IsLevelUseSkill(0),"descriptor0 lacks source initialized SP");
        Require(!g_skillMgr.IsLevelUseSkill(7000),"missing7000 incorrectly inherited descriptor0 SP");
        Require(g_skillMgr.IsLevelUseSkill(0) && !g_skillMgr.IsLevelUseSkill(8000) && !g_skillMgr.IsLevelUseSkill(8200),"source zero/base descriptor classification mismatch");
        Require(!g_skillMgr.IsLevelUseSkill(-1) && !g_skillMgr.IsLevelUseSkill(8238),"level-use out-of-table safety guard differs");
        begin(28,3);
        for(const auto [type,use]:std::array<std::pair<u32,int>,5>{{{1,2},{2,1},{8,3},{16,4},{32,5}}}){
            autorun(30,type,0xfffe,7,11,1);
            Require(mode.m_skillUseInfo.id==30 && mode.m_skillUseInfo.level==-2 && mode.m_skillUseInfo.attackRange==11
                && mode.m_skillUseInfo.useType==use && !g_isGoodSkillHanging
                && player.m_proceedType.type==3 && player.m_skillId==28 && mode.m_skillChaseSkillId==28,
                "source126 mapping/79 raw level/range changed old proceed");
        }
        autorun(30,3,7,7,99,0);
        Require(mode.m_skillUseInfo.useType==5 && mode.m_skillUseInfo.level==-2 && mode.m_skillUseInfo.attackRange==11,
            "source126 treated unknown/combined type as a bitmask");
        PLAYER_SKILL_INFO passive{};passive.m_isValid=1;passive.SKID=4;passive.level=7;g_session.SetSkillItem(passive);
        autorun(4,1,99,7,13,0);
        Require(mode.m_skillUseInfo.id==4 && mode.m_skillUseInfo.level==7 && mode.m_skillUseInfo.attackRange==13 && g_isGoodSkillHanging,
            "non-level-use79 did not resolve learned level");
        passive.m_isValid=0;g_session.SetSkillItem(passive);passive.m_isValid=1;passive.level=9;g_session.AddTemporarySkillItem(passive);
        autorun(4,1,99,7,14,0);
        Require(mode.m_skillUseInfo.level==0,"source79 invented temporary fallback for non-level-use skill");
        const auto retainedPending=mode.m_skillUseInfo;const bool retainedGood=g_isGoodSkillHanging;
        mode.SendMsg(79,0,0,0);
        Require(mode.m_skillUseInfo.useType==0 && mode.m_skillUseInfo.id==retainedPending.id
            && mode.m_skillUseInfo.level==retainedPending.level && mode.m_skillUseInfo.attackRange==retainedPending.attackRange
            && g_isGoodSkillHanging==retainedGood,"source79 null row reset retained fields/global");
        g_session.m_aid=0x11000001;g_session.m_gid=0x22000002;g_session.m_homunGID=0x33000003;g_session.m_merGID=0x44000004;
        autorun(30,4,3,7,5,0);skillFrame(30,3,g_session.m_aid);
        Require(g_session.GetTemporarySkillItemBySkillId(30)->type==2 && mode.m_skillUseInfo.useType==0,
            "source126 dispatched retained temporary type instead of incoming type");
        for(const auto [id,gid]:std::array<std::pair<u16,u32>,3>{{{40,g_session.m_aid},{8000,g_session.m_homunGID},{8200,g_session.m_merGID}}}){
            autorun(id,4,3,7,5,0);skillFrame(id,3,gid);
            Require(mode.m_skillUseInfo.useType==0 && player.m_proceedType.type==3,"self126 did not reset only pending use type");
        }
        autorun(40,4,0xfffe,7,5,0);skillFrame(40,0xfffe,g_session.m_aid);
        ro::commerce::state.vendingActive=true;mode.m_skillUseInfo.useType=2;autorun(40,4,3,7,5,0);
        Require(mode.m_skillUseInfo.useType==2 && g_session.GetTemporarySkillItemBySkillId(40)->level==3
            && SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(20))==0,"vending guard did not preserve arm after temporary receipt");
        ro::commerce::state.vendingActive=false;player.m_stateId=6;autorun(40,4,3,7,5,0);
        Require(mode.m_skillUseInfo.useType==0 && SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(20))==0,
            "source126 failed to reset pending type after sitting dispatch rejection");
        autorun(8000,4,3,7,5,0);skillFrame(8000,3,g_session.m_homunGID);player.m_stateId=0;
        g_session.m_aid=0;autorun(40,4,3,7,5,0);skillFrame(40,3,0);
        player.SendMsg(nullptr,13,0,0,0);
        unit.m_aid=0x77000002;unit.m_job=141;unit.m_pos={100,0,0};world.m_skillList.push_back(&unit);
        const auto touch=[&](int id,int level,int range=5){
            const SKILL_USE_INFO pending{id,level,range,5};
            player.SendMsg(nullptr,103,unit.m_aid,reinterpret_cast<msgparam_t>(&pending),0);
        };
        const auto actionFrame=[&](u32 gid,u8 action){
            const bool padded=ro::net::GetActiveMapGameplaySendProfile().id==ro::net::MapGameplaySendProfileId::PacketVer22;
            const auto p=peer.ReceiveExact(padded?19:7);const std::size_t offset=padded?5:2;
            Require(p[0]==(padded?0x90:0x37) && p[1]==(padded?1:4)
                && p[offset]==static_cast<u8>(gid) && p[offset+1]==static_cast<u8>(gid>>8)
                && p[offset+2]==static_cast<u8>(gid>>16) && p[offset+3]==static_cast<u8>(gid>>24) && p.back()==action,
                "source virtual150 action frame mismatch");
        };
        touch(0,0);Require(player.m_proceedType.type==6 && mode.m_hasTouchSkillChase && mode.m_touchSkillTargetAid==unit.m_aid,
            "103 failed to synchronize canonical touch proceed");
        ReplayPumpSkillChase(mode);actionFrame(unit.m_aid,0);
        Require(player.m_proceedType.type==6 && mode.m_hasTouchSkillChase,"special zero touch retired after29");
        ReplayPumpSkillChase(mode);actionFrame(unit.m_aid,0);
        player.SendMsg(nullptr,29,0x55000001,141,0);actionFrame(0x55000001,0);
        player.SendMsg(nullptr,29,0x55000001,999,0);actionFrame(unit.m_aid,0);
        player.m_stateId=6;touch(28,9,99);
        Require(player.m_proceedType.type==6 && player.m_skillId==0 && player.m_skillUseLevel==0 && player.m_skillAttackRange==5,
            "rejected103 replaced old touch snapshot");
        player.m_stateId=0;player.m_effectState=2;touch(28,9,99);
        Require(mode.m_touchSkillId==0 && mode.m_touchSkillLevel==0 && mode.m_touchSkillRange==5,
            "burrow rejected103 changed mode transport");player.m_effectState=0;
        touch(28,0);ReplayPumpSkillChase(mode);skillFrame(28,7,unit.m_aid);
        Require(player.m_proceedType.type==0 && !mode.m_hasTouchSkillChase && player.m_skillId==28,
            "ordinary touch did not dispatch42/76 then retire13");
        touch(28,65536);ReplayPumpSkillChase(mode);skillFrame(28,0,unit.m_aid);
        touch(65536,2);ReplayPumpSkillChase(mode);skillFrame(0,2,unit.m_aid);
        touch(0,0);player.m_stateId=-1;ReplayPumpSkillChase(mode);
        Require(SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(20))==0 && player.m_proceedType.type==6,
            "touch source unsigned lower-state guard accepted negative state");
        player.m_stateId=16;ReplayPumpSkillChase(mode);actionFrame(unit.m_aid,0);player.m_stateId=0;
        mode.SendMsg(150,511,0,0);actionFrame(0,255);
        world.m_skillList.clear();ReplayPumpSkillChase(mode);
        Require(player.m_proceedType.type==0 && !mode.m_hasTouchSkillChase,"missing touch target did not retire13");
#include "SourcePendingActorReplay.inc"
#include "SkillUiProducerReplay.inc"
#include "SkillUseLevelPersistenceReplay.inc"
#include "GroundSkillProceedReplay.inc"
    }
