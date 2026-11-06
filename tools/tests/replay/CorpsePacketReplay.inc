#include "world/Corpse.h"
#include "world/GrannyCorpse.h"
#include "world/CorpseState.h"
#include "world/World.h"
#include "gamemode/View.h"
#include "render/Renderer.h"
#include "render3d/RenderDevice.h"

void CorpsePacketReplay(bool modelNative=false)
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
    if(modelNative) {
        const int nativeJob=std::getenv("RO_GR2_CORPSE_JOB")?std::atoi(std::getenv("RO_GR2_CORPSE_JOB")):1286;
        Require(Gr2MonsterModel(nativeJob)!=nullptr,"unsupported native model job");
        auto* guardian=actorFor(nativeJob,1,false);
        guardian->m_pos={0,0,0};guardian->m_roty=35;
        matrix modelView{};modelView.m[0][0]=1;modelView.m[1][1]=.70710678f;modelView.m[2][1]=-.70710678f;
        modelView.m[1][2]=.70710678f;modelView.m[2][2]=.70710678f;modelView.m[3][2]=500;modelView.m[3][3]=1;
        g_renderer.m_hpc=g_renderer.m_vpc=1750;g_renderer.m_xoffset=320;g_renderer.m_yoffset=240;
        const Gr2MonsterState renderState{42,nativeJob,0,guardian->m_stateStartTick,guardian->m_pos,
            guardian->m_roty,1,false,0xFFFFFFFFu,{0,-1,0},{1,1,1},{.5f,.5f,.5f}};
        RenderGr2Monster(renderState,modelView);WaitForGr2Monsters();
        Require(RenderGr2Monster(renderState,modelView),"native original model did not render");
        const float modelRotation=Gr2MonsterCurrentRotation(42,guardian->m_roty);
        WBA pendingHit{};pendingHit.message=12;pendingHit.time=timeGetTime()+60000;
        guardian->m_willBeAttackedList.push_back(pendingHit);
        dispatch(died);
        Require(guardian->m_stateId!=3&&guardian->m_willBeDead==1,"native model death ignored queued hit");
        guardian->m_stateStartTick=timeGetTime()-20000;CleanupPendingActorDespawns(mode);
        Require(guardian->m_vanishTime==0&&mode.m_runtimeActors.count(42)==1,"native model hit lost to timeout");
        const auto countBefore=g_world.m_gameObjectList.size();
        guardian->m_willBeAttackedList.front().time=timeGetTime()-1;
        // The model virtual ProcessMotion marks state3 complete immediately;
        // the newly independent corpse, not the old actor, plays death from0.
        g_world.UpdateActors();
        Require(mode.m_runtimeActors.empty()&&g_world.m_actorList.empty()
            &&g_world.m_gameObjectList.size()==countBefore+1,"native due-hit state3 failed immediate model retirement");
        auto* retained=dynamic_cast<CGrannyCorpse3d*>(g_world.m_gameObjectList.back());
        Require(retained&&retained->Rotation()==modelRotation,"native corpse lost interpolated model rotation");
        for(auto* old:mode.m_deferredActorDeletes)delete old;mode.m_deferredActorDeletes.clear();
        // Destroy the actor/model lookup caches before the corpse's first draw.
        // Its pinned asset and private mesh/animation instance must survive.
        ClearGr2Monsters();
        // The live-actor submissions establish cache state, not capture pixels.
        // Clear their queue before the corpse-only frame starts its render pass.
        g_renderer.DiscardRenderList();
        const int requested=std::getenv("RO_GR2_CORPSE_COUNT")?std::atoi(std::getenv("RO_GR2_CORPSE_COUNT")):1;
        Require(requested>=1&&requested<=602,"native model counter outside fixture bounds");
        bool alive=true;while(retained->StateCount()<requested)alive=retained->OnProcess()!=0;
        const int alpha=retained->Alpha();
        const u32 age=std::getenv("RO_GR2_CORPSE_AGE")?static_cast<u32>(std::strtoul(std::getenv("RO_GR2_CORPSE_AGE"),nullptr,10)):5000;
        Require(age<=60000,"native model age outside fixture bounds");
        g_renderer.m_nClearColor=0xFF202020;g_renderer.ClearBackground();g_renderer.Clear(0);
        if(alive)Require(retained->RenderAt(modelView,retained->StartTick()+age),"retained model resources did not render after cache teardown");
        Require(!alive||std::fabs(retained->AnimationClock()-age*.002f)<.0001f,"corpse animation clock lost2x elapsed-time rate");
        Require(g_renderer.m_rpAlphaList.empty(),"model corpse changed source flag0 to translucent sprite sorting");
        std::cout<<"{\"case_id\":\"gr2_corpse_native\",\"job\":"<<nativeJob<<",\"count\":"<<requested<<",\"alpha\":"<<alpha
            <<",\"alive\":"<<alive<<",\"age_ms\":"<<age<<",\"clock\":"<<retained->AnimationClock()
            <<",\"rotation\":"<<retained->Rotation()<<",\"faces\":"<<g_renderer.m_rpFaceList.size()<<"}\n";
        Require(g_renderer.DrawScene(),"native model corpse frame failed");
        Require(GetRenderDevice().Present(false)==0,"native model corpse presentation failed");
        // Source lifetime sweep; then later factory tests can reuse the same ID.
        while(retained->StateCount()<=601)retained->OnProcess();
        g_world.UpdateGameObjects();
        Require(g_world.m_gameObjectList.size()==countBefore,"native model corpse did not sweep");
    }
    const auto beforeModel=g_world.m_gameObjectList.size();
    auto* modelActor=actorFor(1288,1,false);
    modelActor->m_pos={3,4,5};modelActor->m_roty=47;modelActor->m_zoom=3;
    dispatch(died);modelActor->m_stateStartTick=timeGetTime()-20000;
    CleanupPendingActorDespawns(mode);
    Require(mode.m_runtimeActors.count(42)==1&&modelActor->m_vanishTime==0,
        "GR2 death still armed an invented timed actor fade");
    // Source model ProcessMotion completes state3 immediately, independently
    // of any rendered actor animation. The corpse begins its own death bank.
    g_world.UpdateActors();
    Require(mode.m_runtimeActors.empty()
        &&g_world.m_gameObjectList.size()==beforeModel+1,
        "GR2 factory did not independently retire its actor");
    auto* modelCorpse=dynamic_cast<CGrannyCorpse3d*>(g_world.m_gameObjectList.back());
    Require(modelCorpse&&modelCorpse->StateCount()==1&&modelCorpse->Alpha()==255
        &&modelCorpse->Position().x==3&&modelCorpse->Position().y==4
        &&modelCorpse->Position().z==5&&modelCorpse->Rotation()==133,
        "GR2 corpse snapshot/initial owning-sweep count changed");
    dispatch(died);
    Require(g_world.m_gameObjectList.size()==beforeModel+1,
        "duplicate death created another GR2 corpse");
    for(auto* old:mode.m_deferredActorDeletes)delete old;
    mode.m_deferredActorDeletes.clear();
    // Literal PE counter/byte-alpha goldens, deliberately independent of time.
    struct ModelCount {int previous;int alpha;bool alive;};
    const ModelCount modelCounts[]={{355,255,true},{356,244,true},{512,88,true},
        {599,1,true},{600,0,true},{601,255,false}};
    int next=1;
    for(const auto& test:modelCounts) {
        while(next<test.previous){Require(modelCorpse->OnProcess()!=0,"GR2 expired early");++next;}
        Require((modelCorpse->OnProcess()!=0)==test.alive&&modelCorpse->Alpha()==test.alpha
            &&modelCorpse->StateCount()==test.previous+1,"GR2 source counter/byte-alpha boundary changed");
        ++next;
    }
    auto* replacement=actorFor(1288,1,false);
    Require(mode.MakeCorpse(*replacement)&&g_world.m_gameObjectList.size()==beforeModel+2,
        "same-ID model actor reuse did not get an independent corpse");
    auto* replacementCorpse=dynamic_cast<CGrannyCorpse3d*>(g_world.m_gameObjectList.back());
    Require(replacementCorpse!=modelCorpse&&replacementCorpse->StateCount()==1
        &&replacementCorpse->Alpha()==255,"same-ID model reuse shared corpse animation/lifetime");
    // The expired object remains on the owning list until its next sweep.
    g_world.UpdateGameObjects();
    Require(g_world.m_gameObjectList.size()==beforeModel+1,
        "world sweep did not retire the expired model corpse exactly once");
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

void CorpseNativePacketReplay()
{
    Require(GetRequestedRenderBackend()==RenderBackendType::Vulkan,"native corpse proof requires explicit Vulkan backend");
    HWND window=CreateWindowExA(0,"STATIC","Packet corpse fixture",WS_POPUP,0,0,640,480,nullptr,nullptr,GetModuleHandleA(nullptr),nullptr);
    Require(window!=nullptr,"hidden native corpse window creation failed");
    struct Cleanup {
        HWND window;
        ~Cleanup(){ClearGr2Monsters();GetRenderDevice().Shutdown();DestroyWindow(window);}
    } cleanup{window};
    RenderBackendBootstrapResult result{};
    Require(GetRenderDevice().Initialize(window,&result)&&result.backend==RenderBackendType::Vulkan,
        "native corpse Vulkan initialization failed");
    g_renderer.Init();g_renderer.SetSize(640,480);
    CorpsePacketReplay(true);
}
