#include "item/ItemUseFeedback.h"
#include "world/GameActor.h"
#include "world/RagEffect.h"
#include "world/World.h"
#include <array>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <utility>

void ItemUseFeedbackReplay()
{
    SetCase("item_use_feedback_source_oracle", "PK09.19",
        "source-transcribed-item-use-feedback-tsv", "GetUseFeedback pure source selection");

    const std::string fixturePath = std::string(RO_PACKET_SOURCE_DIR)
        + "/tools/tests/fixtures/packet-item-data/item-use-feedback.tsv";
    std::ifstream input(fixturePath);
    Require(input.good(), "could not open item-use-feedback source fixture");
    struct Expected { int effect; std::string sound; unsigned notice; int extra; };
    std::unordered_map<unsigned, Expected> expected;
    std::string line;
    size_t lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        if (line.empty() || line[0] == '#') continue;
        std::istringstream fields(line);
        unsigned itemId = 0, notice = 0;
        int effect = -1, extra = -1;
        std::string sound, trailing;
        Require(static_cast<bool>(fields >> itemId >> effect >> sound >> notice >> extra)
            && !(fields >> trailing), "malformed item-use-feedback TSV row " + std::to_string(lineNumber));
        Require(itemId <= 0xffffu, "item-use-feedback fixture id exceeds packet domain");
        Expected row{effect, sound == "-" ? std::string{} : sound, notice, extra};
        Require(expected.emplace(itemId, std::move(row)).second,
            "duplicate item-use-feedback TSV row " + std::to_string(itemId));
    }
    Require(input.eof() && !expected.empty(), "item-use-feedback fixture read failed or was empty");
    for (unsigned itemId = 0; itemId <= 0xffffu; ++itemId) {
        const auto found = expected.find(itemId);
        const auto actual = ro::item::GetUseFeedback(itemId);
        const Expected absent{-1, {}, 0, -1};
        const Expected& want = found == expected.end() ? absent : found->second;
        Require(actual.effectId == want.effect && std::string(actual.sound ? actual.sound : "") == want.sound
            && actual.noticeMessageId == want.notice && actual.extraEffectId == want.extra,
            "item-use-feedback selection differs from source fixture at item " + std::to_string(itemId));
    }

    SetCase("item_use_feedback_packet_replay", "PK09.19",
        "legacy00a8-selected01c8-use-feedback", "ProcessGameModePacketLoop -> production router -> use-item acknowledgement");

    CWorld world;
    CPlayer player;
    CGameActor remote;
    CGameMode mode;
    TestConnection connection;
    CGameModePacketRouter& router = GetProductionGameModePacketRouter();
    const u32 localAid = 0x13572468u, remoteAid = 0x24681357u;
    const std::vector<ITEM_INFO> savedInventory(g_session.GetInventoryItems().begin(), g_session.GetInventoryItems().end());
    const std::vector<UIChatEvent> savedChat = g_windowMgr.GetChatEvents();
    const u32 savedAid = g_session.m_aid, savedGid = g_session.m_gid;
    const int savedEffects = g_session.m_isEffectOn;
    CMode* const savedMode = SkillPacketReplayAccess::Current();
    const int savedModeType = SkillPacketReplayAccess::CurrentType();
    CWorld* const savedModeWorld = mode.m_world;
    struct Restore {
        CWorld& world; CPlayer& player; CGameActor& remote; CGameMode& mode;
        CGameModePacketRouter& router; CMode* savedMode; int savedModeType; CWorld* savedModeWorld;
        std::vector<ITEM_INFO> inventory; std::vector<UIChatEvent> chat;
        u32 aid, gid; int effects; int waitingUse, lastCard;
        ~Restore() {
            world.ClearFixedObjects();
            SkillPacketReplayAccess::Restore(savedMode, savedModeType);
            mode.m_world = savedModeWorld;
            world.UnregisterActor(&player); world.UnregisterActor(&remote);
            world.m_player = nullptr;
            g_session.ClearInventoryItems(); for (const auto& item : inventory) g_session.SetInventoryItem(item);
            g_session.m_aid=aid; g_session.m_gid=gid; g_session.m_isEffectOn=effects;
            mode.m_waitingUseItemAck=waitingUse; mode.m_lastCardItemIndex=lastCard;
            g_windowMgr.ClearChatEvents();
            for (const auto& event : chat) g_windowMgr.PushChatEvent(event.text.c_str(),event.color,event.channel,event.tick);
            router.Clear(); RegisterDefaultGameModePacketHandlers(router);
        }
    } restore{world, player, remote, mode, router, savedMode, savedModeType, savedModeWorld,
        savedInventory, savedChat, savedAid, savedGid, savedEffects, mode.m_waitingUseItemAck, mode.m_lastCardItemIndex};

    world.m_player = &player;
    player.m_gid = localAid; remote.m_gid = remoteAid;
    world.RegisterActor(&player); world.RegisterActor(&remote);
    mode.m_world = &world;
    SkillPacketReplayAccess::SetCurrent(&mode);
    g_session.m_aid=localAid; g_session.m_gid=localAid; g_session.m_isEffectOn=true;
    g_windowMgr.ClearChatEvents();
    g_session.ClearInventoryItems();
    router.Clear(); RegisterDefaultGameModePacketHandlers(router);

    const auto set16=[](std::vector<u8>& p,size_t o,u16 v){p[o]=static_cast<u8>(v);p[o+1]=static_cast<u8>(v>>8);};
    const auto set32=[](std::vector<u8>& p,size_t o,u32 v){for(int i=0;i<4;++i)p[o+i]=static_cast<u8>(v>>(8*i));};
    const auto putItem=[&](unsigned index,unsigned itemId,int count){ITEM_INFO item;item.SetItemId(itemId);item.m_itemIndex=index;item.m_num=count;g_session.SetInventoryItem(item);};
    const auto itemAt=[](unsigned index)->const ITEM_INFO*{return g_session.GetInventoryItemByIndex(index);};
    const auto effectCount=[](const CGameActor& actor,int id){return std::count_if(actor.m_effectList.begin(),actor.m_effectList.end(),[&](const CRagEffect* e){return e&&e->GetEffectType()==id;});};
    const auto dispatch=[&](const std::vector<u8>& packet,const char* label){
        Require(connection.QueueBytes(packet),std::string("queue failed: ")+label);
        const auto result=ProcessGameModePacketLoop(mode,connection,router,1);
        Require(result.packetsProcessed==1&&result.packetsDispatched==1&&connection.BufferedBytes()==0,
            std::string("production packet did not dispatch: ")+label);
    };
    const auto extended=[&](unsigned itemIndex,unsigned itemId,u32 actorId,bool ok,int remaining){
        std::vector<u8> p(13);set16(p,0,0x01c8);set16(p,2,static_cast<u16>(itemIndex));set16(p,4,static_cast<u16>(itemId));
        set32(p,6,actorId);set16(p,10,static_cast<u16>(remaining));p[12]=ok?1:0;return p;
    };
    const auto basic=[&](unsigned itemIndex,bool ok,int remaining){
        std::vector<u8> p(7);set16(p,0,0x00a8);set16(p,2,static_cast<u16>(itemIndex));set16(p,4,static_cast<u16>(remaining));p[6]=ok?1:0;return p;
    };

    putItem(31,607,4); mode.m_waitingUseItemAck=9;
    dispatch(extended(31,607,localAid,true,3),"local Yggdrasilberry");
    Require(effectCount(player,211)==1&&itemAt(31)&&itemAt(31)->m_num==3&&mode.m_waitingUseItemAck==0,
        "local item 607 feedback/count/pending state differs");

    // Item ids 663 and 11502 were absent from the earlier live switch and are explicit source rows.
    putItem(32,663,2); dispatch(extended(32,663,localAid,true,1),"local item 663");
    Require(effectCount(player,7)==1&&itemAt(32)&&itemAt(32)->m_num==1,"item 663 did not select effect 7");
    putItem(33,11502,2); dispatch(extended(33,11502,localAid,true,1),"local item 11502");
    Require(effectCount(player,208)==1,"item 11502 did not select effect 208");
    putItem(34,12118,2); dispatch(extended(34,12118,localAid,true,1),"local item 12118");
    Require(effectCount(player,603)==1,"item 12118 did not select effect 603");

    for (const auto [itemId,effectId] : std::array<std::pair<unsigned,int>,6>{{
        {12041,593},{12046,594},{12051,595},{12056,596},{12061,597},{12066,598}}}) {
        dispatch(extended(80,itemId,localAid,true,1),"expanded effect range sample");
        Require(effectCount(player,effectId)==1,"expanded item range selected the wrong feedback effect");
    }
    const size_t beforeUnknown=player.m_effectList.size();
    dispatch(extended(81,62000,localAid,true,1),"unknown item feedback");
    Require(player.m_effectList.size()==beforeUnknown,"unknown item created a potion effect");

    // Duplicate feedback effects are replaced, while unrelated effects survive.
    const int unrelatedBeforeDuplicate = effectCount(player,7);
    player.LaunchEffect(211); player.LaunchEffect(211); player.LaunchEffect(7);
    dispatch(extended(31,607,localAid,true,2),"duplicate Yggdrasilberry effects");
    Require(effectCount(player,211)==1&&effectCount(player,7)==unrelatedBeforeDuplicate+1,
        "duplicate replacement removed unrelated effect or retained duplicate");

    putItem(40,607,5);
    dispatch(extended(40,607,remoteAid,true,0),"foreign actor success");
    Require(itemAt(40)&&itemAt(40)->m_num==5&&effectCount(remote,211)==1,
        "foreign success must target remote actor without mutating local inventory");
    const size_t playerBeforeFailure=player.m_effectList.size(), remoteBeforeFailure=remote.m_effectList.size();
    dispatch(extended(40,607,localAid,false,0),"local item use failure");
    Require(itemAt(40)&&itemAt(40)->m_num==5&&player.m_effectList.size()==playerBeforeFailure,
        "local item use failure changed count or emitted feedback");
    dispatch(extended(40,607,remoteAid,false,0),"remote item use failure");
    Require(itemAt(40)&&itemAt(40)->m_num==5&&remote.m_effectList.size()==remoteBeforeFailure,
        "remote item use failure changed count or emitted feedback");

    const int unrelatedBeforeAux = effectCount(player,7);
    dispatch(extended(41,684,localAid,true,0),"item 684 extra feedback");
    Require(effectCount(player,220)==1&&effectCount(player,261)==1&&effectCount(player,7)==unrelatedBeforeAux,
        "item 684 did not emit main/aux effects while retaining unrelated effect");

    dispatch(extended(41,684,localAid,true,0),"repeated auxiliary feedback");
    Require(effectCount(player,220)==1&&effectCount(player,261)==2,"684 auxiliary path incorrectly replaced old261 or retained main duplicates");

    g_windowMgr.ClearChatEvents();
    dispatch(extended(42,12208,localAid,true,0),"local fallback EXP notice");
    const auto localNotice=g_ttemmgr.GetItemUseNotice(12208);
    const auto& events=g_windowMgr.GetChatEvents();
    Require(!localNotice.empty()&&!events.empty()&&events.size()==1&&events.back().text==localNotice
        &&events.back().color==0x0000ffffu&&events.back().channel==6,"local item-use notice text/color/channel differs");
    const size_t localNoticeCount=events.size();
    dispatch(extended(43,12208,remoteAid,true,0),"remote fallback EXP notice");
    Require(g_windowMgr.GetChatEvents().size()==localNoticeCount,"remote actor emitted a local EXP notice");

    // Basic 00A8 identifies the item only through an existing local inventory row.
    g_session.ClearInventoryItems();
    while(effectCount(player,7)>0)player.SendMsg(&player,109,7,0,0);
    putItem(50,501,2); dispatch(basic(50,true,1),"basic recovery potion 501");
    Require(effectCount(player,7)==1&&itemAt(50)&&itemAt(50)->m_num==1,"basic 00A8 did not give recovery-item effect 7");
    putItem(54,599,2); dispatch(basic(54,true,1),"basic recovery potion 599");
    Require(effectCount(player,7)==2&&itemAt(54)&&itemAt(54)->m_num==1,"basic 00A8 upper recovery boundary 599 differs");
    putItem(51,500,2); dispatch(basic(51,true,1),"basic boundary item 500");
    putItem(52,600,2); dispatch(basic(52,true,1),"basic boundary item 600");
    Require(effectCount(player,7)==2,"basic 00A8 boundaries 500/600 emitted potion feedback");
    putItem(55,599,1);dispatch(basic(55,true,0),"basic final recovery use");
    Require(!itemAt(55)&&effectCount(player,7)==3,"final basic recovery use lost feedback after row deletion");
    const int beforeMissing=static_cast<int>(player.m_effectList.size());
    dispatch(basic(53,true,1),"basic acknowledgement without inventory row");
    Require(static_cast<int>(player.m_effectList.size())==beforeMissing,"basic acknowledgement without row emitted feedback");

    const size_t beforeZeroIndex=player.m_effectList.size();
    putItem(0,501,1);dispatch(basic(0,true,0),"zero-index compatibility recovery row");
    Require(player.m_effectList.size()==beforeZeroIndex,"basic zero-index row emitted recovery feedback");
    putItem(56,501,2);mode.m_waitingUseItemAck=56;dispatch(basic(56,false,0),"failed basic recovery use");
    Require(player.m_effectList.size()==beforeZeroIndex&&itemAt(56)&&itemAt(56)->m_num==2&&mode.m_waitingUseItemAck==0,
        "failed basic recovery use emitted feedback or changed ownership/count/pending");
    player.LaunchEffect(211);g_session.m_isEffectOn=false;
    dispatch(extended(56,607,localAid,true,1),"effect-disabled replacement cleanup");
    Require(effectCount(player,211)==0&&effectCount(player,7)==3,"effects-disabled acknowledgement retained old copies or launched new visual");
    g_session.m_isEffectOn=true;

    std::cout<<"{\"case_id\":\"item_use_feedback_source_oracle\",\"contract_id\":\"PK09.19\",\"fixture_rows\":"<<expected.size()<<",\"domain\":65536,\"outcome\":\"PASS\"}\n"
        <<"{\"case_id\":\"item_use_feedback_packet_replay\",\"contract_id\":\"PK09.19\",\"routes\":[\"00A8\",\"01C8\"],\"outcome\":\"PASS\"}"<<std::endl;
}
