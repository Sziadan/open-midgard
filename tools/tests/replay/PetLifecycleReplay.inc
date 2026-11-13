#include "core/File.h"
#include "res/ActRes.h"
#include "res/Res.h"
#include "res/Sprite.h"
#include "world/World.h"
#include "ui/UIWindow.h"
#include "ui/UIItemInfoWnd.h"
#include "core/SourcePetSummaryText.generated.inc"

void PetLifecycleReplay()
{
    SetCase("pet_live_state_receive","PK27.PET-CONTROL","selected-pet-state-and-egg-index","actual framing/router/session/actor/resource paths");
    CWorld world;
    class PetActor final:public CPc {
    public:
        int requestedState=-1,requestedPk=0;
        void SetState(int state)override{requestedState=state;requestedPk=m_pkState;CPc::SetState(state);}
    } actor;
    CGameMode mode;
    TestConnection connection;
    auto& router=GetProductionGameModePacketRouter();
    UIWindow invalidationProbe;invalidationProbe.m_id=88;
    Require(!g_windowMgr.m_itemInfoWnd,"pet state replay requires a fresh shared detail owner");
    struct Restore {
        CWorld& world;CGameMode& mode;PetActor& actor;UIWindow& probe;CGameModePacketRouter& router;
        ro::pet::Status pet=g_session.m_petStatus;
        std::list<ITEM_INFO> inventory=g_session.GetInventoryItems();
        decltype(g_windowMgr.m_chatEvents) events=g_windowMgr.m_chatEvents;
        std::array<int,6> appearance{{g_session.m_playerJob,g_session.m_playerWeapon,g_session.m_playerShield,
            g_session.m_playerAccessory,g_session.m_playerAccessory2,g_session.m_playerAccessory3}};
        CHARACTER_INFO* character=g_session.GetMutableSelectedCharacterInfo();
        CHARACTER_INFO characterValue=character?*character:CHARACTER_INFO{};
        CMode* current=SkillPacketReplayAccess::Current();int modeType=SkillPacketReplayAccess::CurrentType();
        int folder=g_readFolderFirst;std::filesystem::path cwd=std::filesystem::current_path(),temporary;
        ~Restore(){
            if(g_windowMgr.m_itemInfoWnd)g_windowMgr.DeleteWindow(g_windowMgr.m_itemInfoWnd);
            g_windowMgr.m_children.remove(&probe);world.UnregisterActor(&actor);mode.m_world=nullptr;
            SkillPacketReplayAccess::Restore(current,modeType);
            g_session.m_petStatus=pet;g_session.ClearInventoryItems();for(const auto& item:inventory)g_session.SetInventoryItem(item);g_windowMgr.m_chatEvents=events;
            g_session.m_playerJob=appearance[0];g_session.m_playerWeapon=appearance[1];g_session.m_playerShield=appearance[2];
            g_session.m_playerAccessory=appearance[3];g_session.m_playerAccessory2=appearance[4];g_session.m_playerAccessory3=appearance[5];
            if(character)*character=characterValue;
            g_readFolderFirst=folder;std::filesystem::current_path(cwd);
            // This unique fixture directory contains only our generated inputs.
            std::error_code ec;
            if(!temporary.empty() && temporary.parent_path()==std::filesystem::temp_directory_path()
                && temporary.filename().string().find("ro-pet-state-")==0)std::filesystem::remove_all(temporary,ec);
            router.Clear();RegisterDefaultGameModePacketHandlers(router);
        }
    } restore{world,mode,actor,invalidationProbe,router};
    restore.temporary=std::filesystem::temp_directory_path()/("ro-pet-state-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount()));
    Require(std::filesystem::create_directory(restore.temporary),"pet replay resource directory already exists");
    std::filesystem::current_path(restore.temporary);g_readFolderFirst=1;
    {std::ofstream file("msgstringtable.txt");for(int i=0;i<=1188;++i)file<<(i==1188?"Pet starving":"")<<"#\r\n";}
    g_session.m_petStatus={};g_session.ClearInventoryItems();g_windowMgr.ClearChatEvents();
    mode.m_world=&world;SkillPacketReplayAccess::SetCurrent(&mode);
    actor.m_gid=0x12345678;actor.m_job=1002;actor.m_isPc=0;world.RegisterActor(&actor);
    g_windowMgr.m_children.push_back(&invalidationProbe);
    router.Clear();RegisterDefaultGameModePacketHandlers(router);
    const auto set16=[](std::vector<u8>& p,std::size_t o,u16 v){p[o]=static_cast<u8>(v);p[o+1]=static_cast<u8>(v>>8);};
    const auto set32=[](std::vector<u8>& p,std::size_t o,u32 v){for(int i=0;i<4;++i)p[o+i]=static_cast<u8>(v>>(8*i));};
    const auto change=[&](u8 type,u32 gid,u32 value){std::vector<u8> p(11);set16(p,0,0x01a4);p[2]=type;set32(p,3,gid);set32(p,7,value);return p;};
    const auto dispatch=[&](const std::vector<u8>& bytes){
        Require(connection.QueueBytes(bytes),"pet input queue failed");
        const auto result=ProcessGameModePacketLoop(mode,connection,router,1);
        Require(result.packetsProcessed==1 && result.packetsDispatched==1 && connection.BufferedBytes()==0,"pet frame did not dispatch exactly once");
    };
    auto& pet=g_session.m_petStatus;
    dispatch(change(0,0x777,0));Require(pet.actorId==0x777 && pet.job==-1 && !pet.valid,"pet presence was invented without actor lookup");
    dispatch(change(0,actor.m_gid,0));Require(pet.actorId==actor.m_gid && pet.job==1002 && !pet.valid,"pet actor/job association requires a property snapshot");
    actor.m_job=1011;invalidationProbe.m_isDirty=0;
    dispatch(change(1,actor.m_gid,0x80000000u));Require(pet.relationship==(-2147483647-1) && pet.job==1011 && invalidationProbe.m_isDirty,"pet intimacy lost DWORD sign/job/invalidation");
    pet.fullness=70000;invalidationProbe.m_isDirty=0;
    dispatch(change(2,0x888,0xffffffffu));Require(pet.oldFullness==70000 && pet.fullness==-1 && pet.job==1011 && invalidationProbe.m_isDirty,"missing actor blocked source hunger update/history");
    g_session.m_playerJob=4050;pet.fullness=29;
    dispatch(change(1,0x888,99));Require(!g_windowMgr.GetChatEvents().empty() && g_windowMgr.GetChatEvents().back().text=="Pet starving","source pet starvation notice missing");
    const auto notices=g_windowMgr.GetChatEvents().size();
    dispatch(change(1,0x888,100));g_session.m_playerJob=4049;dispatch(change(1,0x888,99));
    g_session.m_playerJob=4052;pet.fullness=30;dispatch(change(1,0x888,99));
    Require(g_windowMgr.GetChatEvents().size()==notices,"pet warning ignored job/intimacy/fullness boundaries");
    pet.accessoryItemId=77;dispatch(change(3,0x888,10001));Require(pet.accessoryItemId==77,"missing actor changed accessory state");
    // Supply actual source-key ACT bytes; the loader must select ACT while
    // retaining the existing SPR, rather than substituting an item sprite.
    g_resMgr.RegisterType("act","data\\sprite\\",new CActRes());
    const std::string path=g_session.GetPetAccActName(10001);
    const std::string file="data\\sprite\\"+path;
    // Match the real CFile CreateFileA byte-key path. std::filesystem's
    // narrow conversion may instead require UTF-8 under the Qt locale.
    CreateDirectoryA("data",nullptr);CreateDirectoryA("data\\sprite",nullptr);
    CreateDirectoryA(file.substr(0,file.find_last_of('\\')).c_str(),nullptr);
    {const HANDLE handle=CreateFileA(file.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        Require(handle!=INVALID_HANDLE_VALUE,"source-key pet ACT fixture open failed");
        const u8 header[16]={0x41,0x43,0,1};DWORD written=0;
        const bool saved=WriteFile(handle,header,16,&written,nullptr)!=0;CloseHandle(handle);
        Require(saved && written==16,"source-key pet ACT fixture write failed");}
    CSprRes priorSpr;actor.m_sprRes=&priorSpr;actor.m_cachedNonPcSprRes=&priorSpr;
    const u32 ownedPetActorId=pet.actorId;pet.actorId=0x777;
    invalidationProbe.m_isDirty=0;dispatch(change(3,actor.m_gid,10001));
    Require(pet.accessoryItemId==77 && actor.m_petAccessoryActName==path && actor.m_actRes
        && actor.m_sprRes==&priorSpr && actor.m_cachedNonPcSprRes==&priorSpr && !invalidationProbe.m_isDirty,
        "remote pet accessory appearance changed owned pet state or skipped actor appearance");
    pet.actorId=ownedPetActorId;
    invalidationProbe.m_isDirty=0;dispatch(change(3,actor.m_gid,10001));
    Require(pet.accessoryItemId==10001 && actor.m_petAccessoryActName==path && actor.m_actRes
        && actor.m_sprRes==&priorSpr && actor.m_cachedNonPcSprRes==&priorSpr && invalidationProbe.m_isDirty,"pet accessory did not replace ACT alone");
    auto* accessoryAct=actor.m_actRes;const int locks=accessoryAct->m_lockCnt;
    Require(locks>0,"pet accessory ACT is not protected from resource eviction");
    dispatch(change(3,actor.m_gid,10001));Require(actor.m_actRes==accessoryAct && accessoryAct->m_lockCnt==locks,"repeated pet accessory change leaked a resource lock");
    Require(std::string(g_session.GetPetAccActName(10002))==g_session.GetPetAccActName(-1)
        && path!=g_session.GetPetAccActName(10002),"source accessory default mapping differs");
    dispatch(change(3,actor.m_gid,0));Require(pet.accessoryItemId==0 && actor.m_petAccessoryActName.empty()
        && !actor.m_petAccessoryActRes && accessoryAct->m_lockCnt==locks-1,"accessory removal retained forced ACT/lock");
    dispatch(change(4,actor.m_gid,2));Require(actor.requestedState==0 && actor.requestedPk==2,"pet performance did not enter real state0 with source PK action");
    dispatch(change(5,actor.m_gid,0));Require(actor.m_headType==100,"source pet head marker missing");
    const auto saved=pet;dispatch(change(255,actor.m_gid,0));Require(pet.relationship==saved.relationship && pet.job==saved.job && pet.actorId==saved.actorId,"unknown pet subtype changed session");
    auto malformed=change(1,actor.m_gid,0);malformed.pop_back();
    router.Dispatch(mode,{0x01a4,static_cast<u16>(malformed.size()),malformed.data()});
    Require(pet.relationship==saved.relationship,"short direct pet dispatch partially changed state");
    std::vector<u8> property(35);set16(property,0,0x01a2);property[2]='P';property[26]=255;
    set16(property,27,10);set16(property,29,0xffff);set16(property,31,0x8000);set16(property,33,0xffff);
    pet.fullness=80000;pet.eggIndex=4321;dispatch(property);
    Require(pet.valid && pet.name=="P" && pet.modified==255 && pet.fullness==-1 && pet.oldFullness==80000
        && pet.relationship==-32768 && pet.accessoryItemId==65535 && pet.actorId==actor.m_gid && pet.job==1011 && pet.eggIndex==4321,"property extension/history/identity preservation differs");
    set16(property,27,10000);property[2]='X';dispatch(property);
    Require(pet.name=="P" && pet.level==10,"alternate pet summary overwrote normal pet state");
    auto* detail=g_windowMgr.m_itemInfoWnd;
    Require(detail && detail->HasPetSummary() && detail->HasContent() && !detail->HasItem()
        && detail->m_w==280 && detail->m_h==120,"alternate summary did not create actual shared source detail owner");
    UIItemInfoWnd::DisplayData summary;
    Require(detail->GetDisplayDataForQt(&summary),"fresh alternate summary has no passive view");
    using namespace ro::pet::summary_text;
    const auto utf8=[](const std::string& raw){
        const int n=MultiByteToWideChar(949,0,raw.data(),static_cast<int>(raw.size()),nullptr,0);
        std::wstring wide(n,L'\0');MultiByteToWideChar(949,0,raw.data(),static_cast<int>(raw.size()),wide.data(),n);
        const int size=WideCharToMultiByte(CP_UTF8,0,wide.data(),n,nullptr,0,nullptr,nullptr);
        std::string out(size,'\0');WideCharToMultiByte(CP_UTF8,0,wide.data(),n,out.data(),size,nullptr,nullptr);return out;
    };
    const std::string expected=std::string("X\n")+utf8(std::string(modified)+'\n'+levelPrefix+"0\n"
        +fullnessPrefix+hungerUnknown+'\n'+relationshipPrefix+friendlyUnknown+'\n'+equipped+'\n');
    Require(summary.description==expected && summary.detailLines.empty() && summary.itemId==0,
        "alternate summary rows/order/signed fields/unsigned flag-accessory differ");
    Require(!g_windowMgr.m_itemCollectionWnd,"alternate summary opened illustration preview");
    const std::pair<int,const char*> hungerCases[]={{-1,hungerUnknown},{0,hunger0},{10,hunger0},{11,hunger1},
        {25,hunger1},{26,hunger2},{75,hunger2},{76,hunger3},{90,hunger3},{91,hunger4},{100,hunger4},{101,hungerUnknown}};
    for(const auto& test:hungerCases){
        set16(property,29,static_cast<u16>(test.first));dispatch(property);
        Require(g_windowMgr.m_itemInfoWnd==detail && detail->GetDisplayDataForQt(&summary)
            && summary.description.find(utf8(std::string(fullnessPrefix)+test.second+'\n'))!=std::string::npos,
            "alternate summary hunger boundary or singleton reuse differs");
    }
    const std::pair<int,const char*> friendCases[]={{-1,friendlyUnknown},{0,friendly0},{100,friendly0},{101,friendly1},
        {250,friendly1},{251,friendly2},{750,friendly2},{751,friendly3},{900,friendly3},{901,friendly4},{1000,friendly4},{1001,friendlyUnknown}};
    for(const auto& test:friendCases){
        set16(property,31,static_cast<u16>(test.first));dispatch(property);
        Require(detail->GetDisplayDataForQt(&summary)
            && summary.description.find(utf8(std::string(relationshipPrefix)+test.second+'\n'))!=std::string::npos,
            "alternate summary relationship boundary differs");
    }
    ITEM_INFO previous{};previous.SetItemId(501);previous.m_itemIndex=64321;previous.m_num=2;previous.m_isIdentified=1;
    g_session.SetInventoryItem(previous);
    detail->SetItemInfo(previous,0,0,ItemInfoSource::Inventory);Require(!detail->HasPetSummary(),"ordinary item replacement retained summary sentinel");
    UIItemInfoWnd::DisplayData itemDisplay;Require(detail->GetDisplayDataForQt(&itemDisplay),"ordinary detail fixture missing");
    property[26]=0;set16(property,27,32767);set16(property,33,0);
    std::fill(property.begin()+2,property.begin()+26,'N');dispatch(property);
    Require(detail->GetDisplayDataForQt(&summary) && summary.itemId==501 && summary.name==itemDisplay.name
        && summary.description.find(std::string(24,'N')+'\n'+utf8(std::string(unmodified)+'\n'+levelPrefix+"22767\n"))==0
        && summary.description.find(utf8(std::string(unequipped)+'\n'))!=std::string::npos
        && (itemDisplay.description.empty() || summary.description.find(itemDisplay.description)!=std::string::npos),
        "alternate summary replaced item, lost appended description, or mishandled max wire name/level");
    Require(g_windowMgr.m_children.back()==detail && pet.name=="P" && pet.level==10 && pet.job==1011
        && pet.fullness==-1 && pet.relationship==-32768 && pet.modified==255 && pet.accessoryItemId==65535,
        "summary did not raise shared detail or mutated normal pet state");
    g_session.ClearInventoryItems();detail->RefreshBoundItem();
    Require(detail->HasPetSummary() && detail->HasItem() && detail->GetDisplayDataForQt(&summary) && summary.itemId==501,
        "alternate summary remained tied to the old live inventory binding");
    for(int i=2;i<26;i+=2){property[i]=0xb0;property[i+1]=0xa1;}
    const int savedCodePage=g_codePage;g_codePage=949;dispatch(property);
    Require(detail->GetDisplayDataForQt(&summary) && summary.description.find(u8"가가가가가가가가가가가가\n")==0,
        "DBCS wire summary name was not decoded to passive/native UTF8 text");
    g_codePage=savedCodePage;
    const auto normalBeforeBoundary=pet;
    const std::string summaryBeforeBoundary=summary.description;
    set16(property,27,9999);dispatch(property);
    Require(pet.level==9999 && detail->GetDisplayDataForQt(&summary) && summary.description==summaryBeforeBoundary,
        "level9999 incorrectly refreshed alternate summary");
    set16(property,27,0x8000);dispatch(property);
    Require(pet.level==-32768 && detail->GetDisplayDataForQt(&summary) && summary.description==summaryBeforeBoundary,
        "negative signed property level was treated as alternate summary");
    pet=normalBeforeBoundary;
    detail->SetItemInfo(previous,0,0);Require(!detail->HasPetSummary(),"returning to ordinary item left pet text active");
    const auto inventory=[&](u16 opcode){
        const int size=ro::net::GetActiveEquipmentItemListEntrySize(opcode);Require(size==20 || size==26,"unexpected selected equipment size");
        std::vector<u8> p(4+size*4);set16(p,0,opcode);set16(p,2,static_cast<u16>(p.size()));
        const u16 ids[]={9001,9049,9050,9002};
        for(int i=0;i<4;++i){const auto at=4+size*i;set16(p,at,98+i);set16(p,at+2,ids[i]);p[at+4]=7;p[at+5]=0;p[at+10]=i==3?0:1;set16(p,at+8,i==1?1:0);}
        return p;
    };
    for(u16 opcode:{u16{0x00a4},u16{0x02d0}}){
        dispatch(inventory(opcode));Require(pet.eggIndex==99,"pet egg linkage did not select last eligible wire row before wear split");
        std::vector<u8> empty(4);set16(empty,0,opcode);set16(empty,2,4);dispatch(empty);
        Require(pet.eggIndex==99,"empty equipment list cleared source remembered egg");
    }
    g_session.ClearInventoryItems();Require(pet.eggIndex==99,"inventory clear invented pet egg reset");
    std::cout<<"{\"case_id\":\"pet_live_state_receive\",\"contract_id\":\"PK27.PET-CONTROL\",\"source01a4\":true,\"actual_actor_act_resource\":true,\"property_widths\":true,\"egg_wire_order\":true,\"alternate_shared_detail_owner\":true,\"outcome\":\"PASS\"}\n";
}
