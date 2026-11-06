#include "world/GameActor.h"
#include "world/World.h"
#include "world/RagEffect.h"
#include "gamemode/GameMode.h"
#include "gamemode/GameModePacket.h"
#include "session/Session.h"
#include "ui/UIWindowMgr.h"
#include "ui/UIBasicInfoWnd.h"

void RecoveryDeltaReplay()
{
    SetCase("recovery_delta_packet_replay", "PK09.20",
        "selected-20080910-recovery-delta-and-status-order", "ProcessGameModePacketLoop -> production router -> recovery/status handlers");

    CWorld world;
    class RecordingRecoveryPlayer final : public CPlayer {
    public:
        struct Message { int id; msgparam_t a, b, c; };
        std::vector<Message> messages;
        void SendMsg(CGameObject* src, int id, msgparam_t a, msgparam_t b, msgparam_t c) override {
            if (id == 88) messages.push_back({id, a, b, c});
            CPlayer::SendMsg(src, id, a, b, c);
        }
    } player;
    CGameMode mode;
    TestConnection connection;
    CGameModePacketRouter& router = GetProductionGameModePacketRouter();
    CMode* const savedMode = SkillPacketReplayAccess::Current();
    const int savedModeType = SkillPacketReplayAccess::CurrentType();
    CWorld* const savedModeWorld = mode.m_world;
    const CharacterVitals savedVitals = g_session.GetCharacterVitals();
    const u32 savedAid = g_session.m_aid, savedGid = g_session.m_gid;
    const int savedEffects = g_session.m_isEffectOn;
    auto* savedCharacterPointer=g_session.GetMutableSelectedCharacterInfo();
    const CHARACTER_INFO savedCharacter=savedCharacterPointer?*savedCharacterPointer:CHARACTER_INFO{};
    const int savedBasicDirty = g_windowMgr.m_basicInfoWnd ? g_windowMgr.m_basicInfoWnd->m_isDirty : 0;
    struct Restore {
        CWorld& world; RecordingRecoveryPlayer& player; CGameMode& mode; CGameModePacketRouter& router;
        CMode* savedMode; int savedModeType; CWorld* savedModeWorld; CharacterVitals vitals;
        u32 aid, gid; int effects; int basicDirty;
        CHARACTER_INFO* characterPointer;CHARACTER_INFO character;
        ~Restore() {
            while(!player.m_msgEffectList.empty()) {
                auto* effect=player.m_msgEffectList.front();
                g_world.m_gameObjectList.remove(effect);delete effect;
            }
            world.ClearFixedObjects();
            SkillPacketReplayAccess::Restore(savedMode, savedModeType);
            mode.m_world = savedModeWorld;
            world.UnregisterActor(&player); world.m_player = nullptr;
            g_session.UpdateCharacterVital(5, vitals.hp); g_session.UpdateCharacterVital(6, vitals.maxHp);
            g_session.UpdateCharacterVital(7, vitals.sp); g_session.UpdateCharacterVital(8, vitals.maxSp);
            if(characterPointer)*characterPointer=character;
            g_session.m_aid = aid; g_session.m_gid = gid; g_session.m_isEffectOn = effects;
            if (g_windowMgr.m_basicInfoWnd) g_windowMgr.m_basicInfoWnd->m_isDirty = basicDirty;
            router.Clear(); RegisterDefaultGameModePacketHandlers(router);
        }
    } restore{world, player, mode, router, savedMode, savedModeType, savedModeWorld,
        savedVitals, savedAid, savedGid, savedEffects, savedBasicDirty,savedCharacterPointer,savedCharacter};

    const auto set16=[](std::vector<u8>& p,size_t o,u16 v){p[o]=static_cast<u8>(v);p[o+1]=static_cast<u8>(v>>8);};
    const auto set32=[](std::vector<u8>& p,size_t o,u32 v){for(int i=0;i<4;++i)p[o+i]=static_cast<u8>(v>>(8*i));};
    const auto recovery=[](u16 status,u16 amount){std::vector<u8> p(6);p[0]=0x3d;p[1]=0x01;p[2]=static_cast<u8>(status);p[3]=static_cast<u8>(status>>8);p[4]=static_cast<u8>(amount);p[5]=static_cast<u8>(amount>>8);return p;};
    const auto status=[&](u16 type,u32 value){std::vector<u8> p(8);set16(p,0,0x00b0);set16(p,2,type);set32(p,4,value);return p;};
    const auto dispatch=[&](const std::vector<u8>& bytes,const char* label){
        Require(connection.QueueBytes(bytes),std::string("queue failed: ")+label);
        const auto result=ProcessGameModePacketLoop(mode,connection,router,1);
        Require(result.packetsProcessed==1&&result.packetsDispatched==1&&connection.BufferedBytes()==0,
            std::string("production packet did not dispatch: ")+label);
    };
    const auto setVitals=[&](u32 hp,u32 maxHp,u32 sp,u32 maxSp){
        g_session.UpdateCharacterVital(5,hp);g_session.UpdateCharacterVital(6,maxHp);
        g_session.UpdateCharacterVital(7,sp);g_session.UpdateCharacterVital(8,maxSp);
    };
    const auto effectCount=[](const CGameActor& actor,int id){return std::count_if(actor.m_effectList.begin(),actor.m_effectList.end(),[&](const CRagEffect* e){return e&&e->GetEffectType()==id;});};

    world.m_player=&player; world.RegisterActor(&player); mode.m_world=&world;
    SkillPacketReplayAccess::SetCurrent(&mode); g_session.m_aid=0x13572468u;g_session.m_gid=0x13572468u;g_session.m_isEffectOn=true;
    router.Clear(); RegisterDefaultGameModePacketHandlers(router);

    struct DeltaCase { u16 raw; u32 current, want; int message; };
    const std::array<DeltaCase,5> cases{{
        {0x7fff,100,32867,32767},
        {0x8000,100,0xffff8064u,-32768},
        {0xffff,10,9,-1},
        {1,0xffffffffu,0u,1},
        {1,0x7fffffffu,0x80000000u,1},
    }};
    for (const auto& c : cases) {
        setVitals(c.current,50,c.current,40); player.m_Hp=c.current+77;player.m_MaxHp=1;player.m_Sp=c.current+88;player.m_MaxSp=1;
        if (g_windowMgr.m_basicInfoWnd) g_windowMgr.m_basicInfoWnd->m_isDirty=0;
        const size_t messages=player.messages.size();
        dispatch(recovery(5,c.raw),"013D literal signed-delta HP");
        Require(g_session.GetCharacterVitals().hp==c.want&&player.m_Hp==c.want,
            "HP recovery did not use signed16 delta and uint32 modulo from session vitals");
        Require(player.messages.size()==messages+1&&player.messages.back().id==88
            &&static_cast<u32>(player.messages.back().a)==static_cast<u32>(c.message)
            &&player.messages.back().c==114&&static_cast<u32>(player.messages.back().b)==0xff00ff00u,
            "HP recovery number feedback differs from signed wire amount");
        Require(effectCount(player,331)==1,"HP recovery did not emit effect 331");
        Require(player.m_lastDamageNumberEffect&&player.m_lastDamageNumberEffect->m_msgEffectType==114
            &&player.m_lastDamageNumberEffect->m_numberValue==(c.message<0?0:c.message)
            &&player.m_lastDamageNumberEffect->m_colorArgb==0xff00ff00u,"actual HP recovery number type/value/color differs");
        Require(!g_windowMgr.m_basicInfoWnd || g_windowMgr.m_basicInfoWnd->m_isDirty!=0,
            "changed HP recovery did not invalidate basic info");
        while(effectCount(player,331)>0)player.SendMsg(&player,109,331,0,0);
    }
    for (const auto& c : cases) {
        setVitals(20,30,c.current,40); player.m_Sp=c.current+99;player.m_MaxSp=1;
        const size_t messages=player.messages.size();
        dispatch(recovery(7,c.raw),"013D literal signed-delta SP");
        Require(g_session.GetCharacterVitals().sp==c.want&&player.m_Sp==c.want,
            "SP recovery did not use signed16 delta and uint32 modulo from session vitals");
        Require(player.messages.size()==messages+1&&player.messages.back().id==88
            &&static_cast<u32>(player.messages.back().a)==static_cast<u32>(c.message)
            &&player.messages.back().c==114&&static_cast<u32>(player.messages.back().b)==0xff0000ffu,
            "SP recovery number feedback differs from signed wire amount");
        Require(effectCount(player,332)==1,"SP recovery did not emit effect 332");
        Require(player.m_lastDamageNumberEffect&&player.m_lastDamageNumberEffect->m_msgEffectType==114
            &&player.m_lastDamageNumberEffect->m_numberValue==(c.message<0?0:c.message)
            &&player.m_lastDamageNumberEffect->m_colorArgb==0xff0000ffu,"actual SP recovery number type/value/color differs");
        while(effectCount(player,332)>0)player.SendMsg(&player,109,332,0,0);
    }

    setVitals(4,0,8,0); player.m_Hp=999;player.m_MaxHp=1;
    if (g_windowMgr.m_basicInfoWnd) g_windowMgr.m_basicInfoWnd->m_isDirty=0;
    size_t messages=player.messages.size(); dispatch(recovery(5,0),"zero HP feedback");
    Require(g_session.GetCharacterVitals().hp==4&&player.messages.size()==messages+1&&player.messages.back().a==0
        &&player.messages.back().c==114&&static_cast<u32>(player.messages.back().b)==0xff00ff00u&&effectCount(player,331)==1,
        "zero recovery must retain number/effect feedback without changing HP");
    Require(!g_windowMgr.m_basicInfoWnd || g_windowMgr.m_basicInfoWnd->m_isDirty==0,
        "unchanged zero recovery invalidated basic info");
    while(effectCount(player,331)>0)player.SendMsg(&player,109,331,0,0);
    const size_t beforeUnknown=player.messages.size();
    dispatch(recovery(4,9),"unknown recovery status");
    Require(player.messages.size()==beforeUnknown&&effectCount(player,331)==0&&effectCount(player,332)==0
        &&g_session.GetCharacterVitals().hp==4&&g_session.GetCharacterVitals().sp==8,
        "unsupported recovery status was not quiet");

    // Without a world player, the session remains the state authority and no visual feedback is emitted.
    world.UnregisterActor(&player);world.m_player=nullptr;setVitals(7,0,9,0);
    messages=player.messages.size();dispatch(recovery(5,3),"session-only recovery");
    Require(g_session.GetCharacterVitals().hp==10&&player.messages.size()==messages,
        "session-only recovery did not update vitals quietly");

    // Status parameters are absolute authoritative values and replace a prior delta.
    world.RegisterActor(&player);world.m_player=&player;setVitals(20,100,30,80);player.m_Hp=20;player.m_MaxHp=100;
    dispatch(recovery(5,5),"recovery before authoritative status");
    dispatch(status(5,33),"authoritative 00B0 HP after recovery");
    Require(g_session.GetCharacterVitals().hp==33&&player.m_Hp==33,"00B0 did not supersede recovery delta");
    dispatch(status(5,34),"authoritative status before repeat delta");
    dispatch(recovery(5,2),"repeated recovery delta after status");
    Require(g_session.GetCharacterVitals().hp==36&&player.m_Hp==36,"repeated delta did not use latest authoritative status");

    g_session.m_isEffectOn=false;setVitals(40,50,60,70);player.m_Hp=40;player.m_MaxHp=50;
    const size_t beforeDisabled=player.m_effectList.size();messages=player.messages.size();
    dispatch(recovery(5,2),"effect-disabled recovery");
    Require(g_session.GetCharacterVitals().hp==42&&player.m_Hp==42&&player.messages.size()==messages+1
        &&player.m_effectList.size()<=beforeDisabled,"effects-disabled recovery suppressed number or changed state incorrectly");

    std::cout<<"{\"case_id\":\"recovery_delta_packet_replay\",\"contract_id\":\"PK09.20\",\"routes\":[\"013D\",\"00B0\"],\"signed16_literals\":[32767,-32768,-1],\"modulo_wrap\":true,\"session_authority\":true,\"zero_feedback\":true,\"unknown_quiet\":true,\"status_order\":true,\"effects_disabled_number\":true,\"outcome\":\"PASS\"}"<<std::endl;
}
