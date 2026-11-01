#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <list>
#include <vector>
using u8=std::uint8_t; using u16=std::uint16_t; using u32=std::uint32_t;
using msgparam_t=std::intptr_t;
void DbgLog(const char*,...) {}
struct ITEM_INFO {
    unsigned m_itemIndex=0,id=0;
    int m_itemType=0,m_isIdentified=0,m_num=0,m_location=0,m_wearLocation=0,m_deleteTime=0;
    int m_slot[4]{};
    void SetItemId(unsigned value){id=value;}
    unsigned GetItemId()const{return id;}
};
struct SHORTCUT_SLOT { unsigned id=1750; int isSkill=0,count=1; } shortcut;
struct PLAYER_SKILL_INFO {int level=10;};
struct Session {
    std::vector<ITEM_INFO> items;
    int m_playerJob=11;u32 m_aid=2000000;
    const ITEM_INFO* GetInventoryItemByIndex(unsigned i)const{
        for(const auto& item:items)if(item.m_itemIndex==i)return &item;return nullptr;
    }
    const ITEM_INFO* GetInventoryItemByItemId(unsigned id)const{
        for(const auto& item:items)if(item.id==id)return &item;return nullptr;
    }
    bool SetInventoryItemWearLocation(unsigned i,int mask){
        for(auto& item:items)if(item.m_itemIndex==i){item.m_wearLocation=mask;return true;}return false;
    }
    void ClearInventoryWearLocationMask(int mask,unsigned except){
        for(auto& item:items)if(item.m_itemIndex!=except)item.m_wearLocation&=~mask;
    }
    const SHORTCUT_SLOT* GetShortcutSlotByAbsoluteIndex(int i){return i>=0&&i<27?&shortcut:nullptr;}
    const PLAYER_SKILL_INFO* GetSkillItemBySkillId(int){return nullptr;}
    const std::list<ITEM_INFO>& GetInventoryItems()const{static std::list<ITEM_INFO> result;result.assign(items.begin(),items.end());return result;}
    const char* GetJobName(int){return "";}
    void GetMonsterSkillInfo(const char*,u16,int&,int&){}
} g_session;
struct vector3d {float x=0,y=0,z=0;};
struct CRagEffect {void SendMsg(CRagEffect*,int,msgparam_t,msgparam_t,msgparam_t){}};
struct CGameActor {
    u32 m_effectState=0;
    int m_job=11,m_objectType=0,m_isMotionFreezed=0,stateCalls=0,state=0;
    u32 m_freezeEndTick=0;float m_motionSpeed=1;vector3d m_pos;
    void SetState(int value){state=value;++stateCalls;}
    void SendMsg(CGameActor*,int,msgparam_t,msgparam_t,msgparam_t){}
    void SetAction(int,int,int){}
    template<class... Args>void SetForceAnimation(Args...){}
    CRagEffect* LaunchEffect(int,vector3d,float){return nullptr;}
};
struct CPlayer:CGameActor {u32 m_targetGid=0,m_proceedTargetGid=0;} player;
struct CGroundScope {void SendMsg(void*,int,msgparam_t,msgparam_t,msgparam_t){}};
struct World {CPlayer* m_player=&player;int m_isPKZone=0,m_isSiegeMode=0;std::vector<CGroundScope*> m_gameObjectList;} world;
struct CGameMode {
    World* m_world=&world;
    int m_waitingWearEquipAck=0,m_waitingTakeoffEquipAck=0,m_waitingUseItemAck=0,m_lastCardItemIndex=0;
    u32 m_lastMonGid=0,m_lastLockOnMonGid=0,m_attackChaseTargetGid=0;
    int m_hasAttackChaseHint=0,m_hasSkillChase=0,m_hasGroundSkillChase=0;
    struct {int id=0,level=0;} m_skillUseInfo;
    bool m_hasCursorSnap=false;
    int m_cursorSnapRawX=0,m_cursorSnapRawY=0,m_cursorSnapDrawX=0,m_cursorSnapDrawY=0;
    void GetClientCoor(int,int,float&,float&){}
} mode;
struct {CGameMode* GetCurrentGameMode(){return &mode;}} g_modeMgr;
bool knownCartActive=false;
bool IsKnownLocalCartActive(){return knownCartActive;}
#include "EquipmentOptionLabelBodies.inc"
struct {bool IsCardItem(unsigned){return false;}} g_ttemmgr;
struct {
    std::string last; int color=0,count=0;
    void SendMsg(int,msgparam_t text,int value){last=reinterpret_cast<const char*>(text);color=value;++count;}
} g_windowMgr;
constexpr int kUiChatEventMsg=1;
struct PacketView {const u8* data;int packetLength;u16 packetId=0;};
u16 ReadLE16(const u8* p){return p[0]|(p[1]<<8);}
u32 ReadLE32(const u8* p){return ReadLE16(p)|(u32(ReadLE16(p+2))<<16);}
#pragma pack(push,1)
struct PACKET_CZ_REQ_WEAR_EQUIP {u16 PacketType=0,ItemIndex=0,WearLocation=0;};
struct PACKET_CZ_REQ_TAKEOFF_EQUIP {u16 PacketType=0,ItemIndex=0;};
struct PACKET_CZ_ITEM_COMPOSITION_LIST {u16 PacketType=0,CardItemIndex=0;};
struct PACKET_CZ_USEITEM_PACKETVER22 {u8 bytes[20];};
#pragma pack(pop)
namespace ro::net {
struct MapGameplaySendProfile {u16 equipItem=0xA9,unequipItem=0xAB,itemCompositionList=0x17A;};
const MapGameplaySendProfile& GetActiveMapGameplaySendProfile(){static MapGameplaySendProfile profile;return profile;}
int GetActiveStackableItemListEntrySize(u16){return 22;}
bool BuildActiveUseItemPacket(u16,u32,u8* out,int,int* length){out[0]=0x9f;out[1]=0;*length=2;return true;}
}
struct CRagConnection {
    std::vector<u8> packet;int count=0;
    static CRagConnection* instance(){static CRagConnection connection;return &connection;}
    bool SendPacket(const char* p,int length){packet.assign(p,p+length);++count;return true;}
};
enum class ShortcutSkillUseMode {CartSelect,ImmediateSelf,GroundTarget,ActorTarget,SkillTarget};
ShortcutSkillUseMode ResolveShortcutSkillUseMode(u16){return ShortcutSkillUseMode::ActorTarget;}
bool HandleChangeCartSkillUiRequest(CGameMode&){return false;}
void ClearSkillChase(CGameMode&){}
void ClearAttackChaseHint(CGameMode&){}
bool SendUseSkillToIdPacket(u16,u16,u32){return false;}
u32 ResolveShortcutSkillTargetId(CGameMode*){return 0;}
u32 ResolveSelfSkillTargetId(){return 0;}
#include "AmmoPacketBodies.inc"
#include "AmmoRequestBodies.inc"
u32 timeGetTime(){return 1000;}
CGameActor* ResolveSkillPacketActor(CGameMode&,u32){return &player;}
void GetSkillActionInfo2(int,int& effect,int& motion,int,int){effect=16;motion=8;}
bool IsMonsterSkillActor(CGameActor*){return false;}
void FaceActorTowardPosition(CGameActor*,vector3d){}
struct SKILL_INFO {static bool IsGoodSkillForActor(u16){return false;}};
#include "InstantCastBody.inc"
struct EquipSlotDefLocal {int wearMask,iconX,iconY;};
struct UIEquipWnd {std::vector<const ITEM_INFO*> BuildSlotAssignments()const;};
#include "AmmoSlotBodies.inc"
namespace inventory_refresh {
struct CSession {
    std::list<ITEM_INFO> m_inventoryItems;
    void ClearEquipmentInventoryItems();
};
#include "AmmoRefreshBody.inc"
}
namespace inventory_view {
struct UIItemWnd {
    int m_currentTab=2;
    int GetInventoryItemCount()const;
    std::vector<const ITEM_INFO*> GetFilteredItems()const;
};
#include "AmmoInventoryViewBodies.inc"
}
struct POINT {int x,y;};
POINT raw{0,0};
bool GetClientCursorPosInternal(POINT* out){if(!out)return false;*out=raw;return true;}
#include "CursorFrameBody.inc"
int main(){
    assert(!GetLocalPlayerRemoveOptionLabel());
    player.m_effectState=0x10;assert(std::strcmp(GetLocalPlayerRemoveOptionLabel(),"Release Falcon")==0);
    player.m_effectState=0x20;assert(std::strcmp(GetLocalPlayerRemoveOptionLabel(),"Unmount")==0);
    for(u32 cart:{0x8u,0x80u,0x100u,0x200u,0x400u}){
        player.m_effectState=cart;assert(std::strcmp(GetLocalPlayerRemoveOptionLabel(),"Remove cart")==0);
    }
    player.m_effectState=0;knownCartActive=true;
    assert(std::strcmp(GetLocalPlayerRemoveOptionLabel(),"Remove cart")==0);
    player.m_effectState=0x20;assert(std::strcmp(GetLocalPlayerRemoveOptionLabel(),"Unmount")==0);
    player.m_effectState=0;knownCartActive=false;
    // Stackable list: usable equipment location is independent of current wear state.
    std::array<u8,22> entry{};
    entry[0]=7;entry[2]=0xd6;entry[3]=6;entry[4]=10;entry[5]=1;entry[6]=100;
    entry[9]=0x80; // Server lists EQP_AMMO even for unequipped arrow stacks.
    ITEM_INFO ammo;
    assert(BuildNormalInventoryItem({entry.data(),22,0x1ee},entry.data(),ammo));
    assert(ammo.m_location==0x8000&&ammo.m_wearLocation==0);
    // A Wizard carrying Fire Arrows gets no ammunition-equipped acknowledgement.
    ITEM_INFO wizardAmmo=ammo;wizardAmmo.SetItemId(1752);
    g_session.m_playerJob=9;g_session.items={wizardAmmo};
    UIEquipWnd wizardEquipment;
    assert(wizardEquipment.BuildSlotAssignments()[10]==nullptr);
    inventory_view::UIItemWnd wizardInventory;
    const auto carriedArrows=wizardInventory.GetFilteredItems();
    assert(carriedArrows.size()==1&&carriedArrows[0]->GetItemId()==1752);
    g_session.m_playerJob=11;
    g_session.items={ammo};
    auto* connection=CRagConnection::instance();
    assert(RequestUseInventoryItem(7));
    assert(connection->packet.size()==6&&ReadLE16(connection->packet.data())==0xa9);
    assert(ReadLE16(connection->packet.data()+2)==7&&ReadLE16(connection->packet.data()+4)==0x8000);
    assert(RequestShortcutSlotUse(18)); // third row hotkey equips, never consumes
    assert(ReadLE16(connection->packet.data())==0xa9);
    // Dedicated server ack marks only the selected stack equipped.
    ITEM_INFO old=ammo;old.m_itemIndex=8;old.m_wearLocation=0x8000;g_session.items.push_back(old);
    u8 ack[]={0x3c,1,7,0};
    HandleEquipAmmo(mode,{ack,4,0x13c});
    assert(g_session.items[0].m_wearLocation==0x8000&&g_session.items[1].m_wearLocation==0);
    const int before=connection->count;
    assert(RequestShortcutSlotUse(18)&&connection->count==before); // already equipped is harmless
    assert(RequestUnequipInventoryItem(7));
    assert(ReadLE16(connection->packet.data())==0xab&&ReadLE16(connection->packet.data()+2)==7);
    assert(g_session.items[0].m_wearLocation==0x8000); // wait for server ack
    UIEquipWnd equipment;
    const auto slots=equipment.BuildSlotAssignments();
    assert(slots.size()==11&&slots[10]&&slots[10]->m_itemIndex==7);
    int ammoSlots=0;for(const auto* item:slots)if(item&&item->m_itemType==10)++ammoSlots;
    assert(ammoSlots==1);
    // Login order: normal-item list, ammunition equip ack, then equipment list.
    inventory_refresh::CSession refreshed;
    ITEM_INFO staleWeapon;staleWeapon.m_itemIndex=3;staleWeapon.m_itemType=4;staleWeapon.m_location=2;
    refreshed.m_inventoryItems={g_session.items[0],staleWeapon};
    refreshed.ClearEquipmentInventoryItems();
    assert(refreshed.m_inventoryItems.size()==1);
    const auto& restoredAmmo=refreshed.m_inventoryItems.front();
    assert(restoredAmmo.m_itemIndex==7&&restoredAmmo.m_num==100&&restoredAmmo.m_wearLocation==0x8000);
    g_session.items.assign(refreshed.m_inventoryItems.begin(),refreshed.m_inventoryItems.end());
    const auto restoredSlots=equipment.BuildSlotAssignments();
    assert(restoredSlots[10]&&restoredSlots[10]->m_itemIndex==7);
    inventory_view::UIItemWnd inventory;
    assert(inventory.GetInventoryItemCount()==1);
    const auto visibleAmmo=inventory.GetFilteredItems();
    assert(visibleAmmo.size()==1&&visibleAmmo[0]->m_itemIndex==7);
    mode.m_lastMonGid=mode.m_lastLockOnMonGid=mode.m_attackChaseTargetGid=100;
    mode.m_hasAttackChaseHint=mode.m_hasSkillChase=mode.m_hasGroundSkillChase=1;
    player.m_targetGid=player.m_proceedTargetGid=100;
    u8 failure[]={0x3b,1,0,0};
    HandleActionFailure(mode,{failure,4,0x13b});
    assert(g_windowMgr.last=="Please equip the proper ammunition first."&&g_windowMgr.color==255);
    assert(!mode.m_lastLockOnMonGid&&!mode.m_hasAttackChaseHint&&!mode.m_hasSkillChase&&!player.m_targetGid);
    const int notices=g_windowMgr.count;
    HandleActionFailure(mode,{failure,3,0x13b});
    failure[2]=99;HandleActionFailure(mode,{failure,4,0x13b});
    assert(g_windowMgr.count==notices);
    // A cast announcement with no cast time isn't successful use confirmation.
    std::array<u8,24> cast{};cast[14]=46;
    HandleSkillCastAck(mode,{cast.data(),24,0x13e});
    assert(player.stateCalls==0);
    cast[20]=200;HandleSkillCastAck(mode,{cast.data(),24,0x13e});
    assert(player.stateCalls==1&&player.state==8); // real wind-up still starts
    for(auto* scope:world.m_gameObjectList)delete scope;
    // New raw input between pick and draw cannot break the frame's snap.
    mode.m_hasCursorSnap=true;mode.m_cursorSnapRawX=210;mode.m_cursorSnapRawY=220;
    mode.m_cursorSnapDrawX=200;mode.m_cursorSnapDrawY=205;
    for(int i=0;i<100;++i){raw={210+i,220-i};POINT drawn{};assert(GetModeCursorClientPos(&drawn));assert(drawn.x==200&&drawn.y==205);}
    mode.m_hasCursorSnap=false;POINT drawn{};assert(GetModeCursorClientPos(&drawn));assert(drawn.x==raw.x&&drawn.y==raw.y);
    std::cout<<"Production-body ammunition equip/shortcut/ack/failure, relogin visibility in both windows, and cursor frame regressions passed\n";
}
