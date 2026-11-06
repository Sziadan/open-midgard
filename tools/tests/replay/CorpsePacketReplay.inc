#include "world/Corpse.h"
#include "world/CorpseState.h"
#include "world/World.h"
#include "gamemode/View.h"

void CorpsePacketReplay()
{
    SetCase("corpse_packet_lifecycle", "PK45.06", "selected-0080-death-and-original-corpse",
        "production framing/router -> queued death -> actor retirement -> independent corpse");
    Require(ro::corpse::DeviceDepth(0.1f)<0&&ro::corpse::DeviceDepth(0.2f)<0
        &&ro::corpse::DeviceDepth(0.0005f)>1,"corpse depth lost original near/far clipping range");
    struct Eligibility { int job, type; bool expected; };
    const Eligibility cases[] = {{28,1,false},{29,1,true},{3999,1,true},{4000,1,false},
        {6000,1,false},{6001,1,true},{6047,1,true},{6048,1,false},
        {28,9,true},{4000,9,true},{6000,9,true},{6048,9,true}};
    for (const auto& c : cases)
        Require(ro::corpse::IsEligible(c.job,c.type)==c.expected,"original corpse eligibility boundary changed");

    // Real original v0x205 ACT/SPR, read directly without touching installed data.
    const auto read=[](const std::filesystem::path& p) {
        std::ifstream file(p,std::ios::binary);
        Require(static_cast<bool>(file),"missing original corpse resource fixture");
        return std::vector<unsigned char>(std::istreambuf_iterator<char>(file),{});
    };
    const auto root=std::filesystem::path(RO_PACKET_SOURCE_DIR)/"Ref/GRF-Content/data/sprite";
    const auto actBytes=read(root/"ork_warrior.act"),sprBytes=read(root/"ork_warrior.spr");
    CActRes act;CSprRes spr;
    Require(act.LoadFromBuffer("data\\sprite\\ork_warrior.act",actBytes.data(),static_cast<int>(actBytes.size())),"original corpse ACT load failed");
    Require(spr.LoadFromBuffer("data\\sprite\\ork_warrior.spr",sprBytes.data(),static_cast<int>(sprBytes.size())),"original corpse SPR load failed");
    act.m_fName.SetString("data\\sprite\\ork_warrior.act");spr.m_fName.SetString("data\\sprite\\ork_warrior.spr");
    Require(act.GetMotionCount(32)==12,"independent original death-action fixture changed");
    const int actLocks=act.m_lockCnt,sprLocks=spr.m_lockCnt;

    CGameMode mode;
    CView view;view.SetInitialCamera(0,-45,300);
    CMode* previousMode=SkillPacketReplayAccess::Current();
    const int previousModeType=SkillPacketReplayAccess::CurrentType();
    CWorld* previousWorld=mode.m_world;CView* previousView=mode.m_view;
    CPlayer* previousPlayer=g_world.m_player;
    auto* previousGround=g_world.m_ground;auto* previousAttr=g_world.m_attr;
    std::list<CGameActor*> savedActors; savedActors.swap(g_world.m_actorList);
    std::list<CGameObject*> savedObjects; savedObjects.swap(g_world.m_gameObjectList);
    std::list<CItem*> savedItems; savedItems.swap(g_world.m_itemList);
    struct Restore {
        CGameMode& mode;CMode* previousMode;int previousModeType;CWorld* previousWorld;CView* previousView;
        CPlayer* previousPlayer;C3dGround* previousGround;C3dAttr* previousAttr;
        std::list<CGameActor*>& savedActors;std::list<CGameObject*>& savedObjects;std::list<CItem*>& savedItems;
        ~Restore() {
            for(auto& entry:mode.m_runtimeActors) {
                g_world.NotifyActorDeleted(entry.second);g_world.UnregisterActor(entry.second);delete entry.second;
            }
            mode.m_runtimeActors.clear();
            for(auto* actor:mode.m_deferredActorDeletes)delete actor;
            mode.m_deferredActorDeletes.clear();
            g_world.ClearFixedObjects();
            g_world.m_actorList.swap(savedActors);g_world.m_gameObjectList.swap(savedObjects);g_world.m_itemList.swap(savedItems);
            g_world.m_player=previousPlayer;g_world.m_ground=previousGround;g_world.m_attr=previousAttr;
            mode.m_world=previousWorld;mode.m_view=previousView;
            SkillPacketReplayAccess::Restore(previousMode,previousModeType);
        }
    } restore{mode,previousMode,previousModeType,previousWorld,previousView,previousPlayer,
        previousGround,previousAttr,savedActors,savedObjects,savedItems};
    mode.m_world=&g_world;mode.m_view=&view;
    g_world.m_player=nullptr;g_world.m_ground=nullptr;g_world.m_attr=nullptr;
    SkillPacketReplayAccess::SetCurrent(&mode);
    auto& router=GetProductionGameModePacketRouter();router.Clear();RegisterDefaultGameModePacketHandlers(router);
    TestConnection connection;
    const std::vector<u8> died={0x80,0,42,0,0,0,1};
    const auto dispatch=[&](const std::vector<u8>& bytes) {
        Require(connection.QueueBytes(bytes),"corpse packet queue failed");
        const auto result=ProcessGameModePacketLoop(mode,connection,router,1);
        Require(result.packetsProcessed==1&&result.packetsDispatched==1,"corpse packet missed production router");
    };
    const auto actorFor=[&](int job,int type,bool pc,u32 gid=42) {
        auto* actor=new CGameActor();actor->m_gid=gid;actor->m_job=job;
        actor->m_objectType=static_cast<u8>(type);actor->m_isPc=pc;
        actor->m_pos={0,0,500};actor->m_actRes=&act;actor->m_sprRes=&spr;
        mode.m_runtimeActors[gid]=actor;g_world.RegisterActor(actor);
        mode.m_actorPosList[gid]={3,4};return actor;
    };
    auto* actor=actorFor(1002,1,false);
    WBA hit{};hit.message=12;hit.time=timeGetTime()+60000;
    actor->m_willBeAttackedList.push_back(hit);
    Require(connection.QueueBytes({died.begin(),died.end()-1}),"partial death queue failed");
    Require(ProcessGameModePacketLoop(mode,connection,router,1).packetsProcessed==0,
        "partial death packet mutated actor state");
    Require(actor->m_stateId!=3,"partial death packet entered death state");
    dispatch({died.back()});
    Require(actor->m_stateId!=3&&actor->m_willBeDead==1&&actor->m_vanishTime==0,
        "queued death failed to defer or armed an invented fade timer");
    actor->m_stateStartTick=timeGetTime()-20000;CleanupPendingActorDespawns(mode);
    Require(mode.m_runtimeActors.count(42)==1&&actor->m_vanishTime==0&&g_world.m_gameObjectList.empty(),
        "corpse timeout removed an actor whose queued hit has not landed");
    actor->m_willBeAttackedList.front().time=timeGetTime()-1;
    actor->ProcessState();
    Require(actor->m_stateId==3&&actor->m_willBeAttackedList.empty(),"due death callback did not enter state3");
    Require(g_world.m_gameObjectList.empty(),"unfinished death animation produced a corpse");
    dispatch(died); // Repeat notification must not create a second corpse.
    actor->m_stateStartTick=timeGetTime()-20000;
    g_world.UpdateActors();
    Require(mode.m_runtimeActors.empty()&&g_world.m_actorList.empty()
        &&mode.m_actorPosList.count(42)==0&&mode.m_deferredActorDeletes.size()==1,
        "completed death retained runtime/cell/world ownership");
    Require(g_world.m_gameObjectList.size()==1,"completed death did not create exactly one corpse");
    dispatch(died);
    Require(g_world.m_gameObjectList.size()==1&&mode.m_deferredActorDeletes.size()==1,
        "duplicate notification recreated an already retired corpse source");
    auto* corpse=dynamic_cast<CCorpse*>(g_world.m_gameObjectList.front());
    Require(corpse&&corpse->m_isLieOnGround&&corpse->m_shadowOn==0
        &&corpse->m_baseAction==32&&corpse->m_curMotion==11,"corpse did not retain the original death frame/defaults");
    Require(corpse->ActName()=="data\\sprite\\ork_warrior.act"&&corpse->SprName()=="data\\sprite\\ork_warrior.spr",
        "corpse did not snapshot exact resource identities");
    Require(act.m_lockCnt==actLocks+1&&spr.m_lockCnt==sprLocks+1,"corpse resources were not retained independently");
    // Same destruction operation as the next OnUpdate's deferred-actor sweep.
    delete mode.m_deferredActorDeletes.front();mode.m_deferredActorDeletes.clear();
    Require(corpse->m_actRes==&act&&corpse->m_sprRes==&spr,"corpse resources depended on the deleted actor");
    const u32 start=corpse->m_stateStartTick;
    Require(corpse->ProcessAt(start)&&corpse->m_u28.m_sprColor.u.s.a==255,"new corpse was not opaque");
    Require(corpse->ProcessAt(start+24)&&corpse->m_u28.m_sprColor.u.s.a==254,"corpse frame conversion changed");
    Require(corpse->ProcessAt(start+3060)&&corpse->m_u28.m_sprColor.u.s.a==127,"corpse half-life alpha changed");
    Require(corpse->ProcessAt(start+6000)&&corpse->m_u28.m_sprColor.u.s.a==5,"float-rounded corpse age changed");
    Require(corpse->ProcessAt(start+6119)&&corpse->m_u28.m_sprColor.u.s.a==0,"corpse expired before255 source frames");
    Require(!corpse->ProcessAt(start+6120),"corpse exceeded255 source frames");
    corpse->m_stateStartTick=0xfffffff0u;
    Require(corpse->ProcessAt(0x10)&&corpse->m_u28.m_sprColor.u.s.a==253,"corpse timer wrap changed");
    corpse->m_stateStartTick=start;
    struct Direction { float rotation;int action; };
    const Direction directions[]={{0,32},{-44.999f,32},{-45,33},{-90,34},{-359.999f,39},{360,32}};
    for(const auto& d:directions) {
        corpse->m_roty=d.rotation;corpse->ProcessAt(start);
        Require(corpse->m_curAction==d.action&&corpse->m_curMotion==11,"corpse generic floor-direction mapping changed");
    }
    corpse->m_roty=0;corpse->ProcessAt(start);
    {
        CActRes empty;
        CCorpse noMotion({},&empty,nullptr,10000,120,0);
        Require(noMotion.ProcessAt(noMotion.m_stateStartTick)&&noMotion.m_curMotion==0,
            "empty/invalid corpse action did not retain source default motion zero");
        Require(!noMotion.ProcessAt(noMotion.m_stateStartTick-1),"future corpse tick did not use modular source age");
    }
    for(const auto& c:cases) {
        auto* candidate=actorFor(c.job,c.type,false);
        dispatch(died);candidate->m_stateStartTick=timeGetTime()-20000;
        // Some boundary jobs are static NPCs without a death animation. Test
        // their eligibility at the completed-motion precondition, separately
        // from the original orc's real animation completion above.
        candidate->m_isMotionFinished=1;
        g_world.UpdateActors();
        Require((mode.m_runtimeActors.count(42)==0)==c.expected,std::string("production state3 eligibility boundary changed job=")+std::to_string(c.job)+" type="+std::to_string(c.type)+" state="+std::to_string(candidate->m_stateId)+" finished="+std::to_string(candidate->m_isMotionFinished));
        if(!c.expected)dispatch({0x80,0,42,0,0,0,0});
        for(auto* old:mode.m_deferredActorDeletes)delete old;
        mode.m_deferredActorDeletes.clear();
    }
    auto* playerStyle=actorFor(1002,1,true);dispatch(died);
    playerStyle->m_stateStartTick=timeGetTime()-20000;g_world.UpdateActors();
    Require(mode.m_runtimeActors.count(42)==1,"player-style death incorrectly converted a disguised PC");
    dispatch({0x80,0,42,0,0,0,0});
    for(auto* old:mode.m_deferredActorDeletes)delete old;
    mode.m_deferredActorDeletes.clear();
    Require(g_world.m_gameObjectList.front()==corpse,"same-ID actor reuse removed an independent corpse");
    const auto beforeModel=g_world.m_gameObjectList.size();
    auto* modelActor=actorFor(1288,1,false);
    Require(!mode.MakeCorpse(*modelActor)&&mode.m_runtimeActors.count(42)==1
        &&g_world.m_gameObjectList.size()==beforeModel,
        "GR2 actor was incorrectly replaced with an empty sprite corpse");
    dispatch({0x80,0,42,0,0,0,0});
    for(auto* old:mode.m_deferredActorDeletes)delete old;
    mode.m_deferredActorDeletes.clear();
    const auto beforeAdjacent=g_world.m_gameObjectList.size();
    auto* first=actorFor(1002,1,false,42);auto* second=actorFor(1002,1,false,43);
    dispatch(died);dispatch({0x80,0,43,0,0,0,1});
    first->m_stateStartTick=second->m_stateStartTick=timeGetTime()-20000;
    g_world.UpdateActors();
    Require(mode.m_runtimeActors.empty()&&g_world.m_actorList.empty()
        &&mode.m_deferredActorDeletes.size()==2&&g_world.m_gameObjectList.size()==beforeAdjacent+2,
        "adjacent actor retirement invalidated the world update cursor");
    for(auto* old:mode.m_deferredActorDeletes)delete old;
    mode.m_deferredActorDeletes.clear();
    const auto liveCount=g_world.m_gameObjectList.size();
    auto* expired=new CCorpse({},&act,&spr,32,11,0);
    expired->m_stateStartTick=timeGetTime()-6200;g_world.m_gameObjectList.push_back(expired);
    g_world.UpdateGameObjects();
    Require(g_world.m_gameObjectList.size()==liveCount,"owning world sweep did not delete an expired corpse");
    g_world.ClearFixedObjects(true);
    Require(g_world.m_gameObjectList.empty()&&act.m_lockCnt==actLocks&&spr.m_lockCnt==sprLocks,
        "map teardown retained a corpse or its resource locks");
    std::cout<<"{\"case_id\":\"corpse_packet_lifecycle\",\"contract_id\":\"PK45.06\",\"status\":\"PASS\",\"source_expiry_ms\":6120,\"eligibility_cases\":12,\"original_assets\":true}\n";
}
