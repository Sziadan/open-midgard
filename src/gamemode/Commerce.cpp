#include "Commerce.h"
#include "GameMode.h"
#include "GameModePacket.h"
#include "network/Connection.h"
#include "session/Session.h"
#include "ui/UIWindowMgr.h"
#include "ui/UIItemWnd.h"
#include "ui/UIPlayerContextMenuWnd.h"
#include "ui/UINewChatWnd.h"
#include <algorithm>
#include <cstring>
#include <limits>

namespace ro::commerce {
State state;
namespace {
u32 read(const u8* p,int bytes=2){u32 v=0;for(int i=0;i<bytes;++i)v|=u32(p[i])<<(8*i);return v;}
void put(std::vector<u8>& p,int at,u32 v,int bytes=2){for(int i=0;i<bytes;++i)p[at+i]=u8(v>>(8*i));}
std::vector<u8> packet(u16 op,int size){std::vector<u8> p(size);put(p,0,op);return p;}
std::string text(const u8* p,int length){int n=0;while(n<length && p[n])++n;return std::string(reinterpret_cast<const char*>(p),n);}
bool send(const std::vector<u8>& p){if(CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(p.data()),int(p.size())))return true;state.status="Connection lost: request was not sent.";return false;}
bool simple(u16 op){return send(packet(op,2));}
void note(const std::string& message){state.status=message;g_windowMgr.PushChatEvent(message.c_str(),0x0080C0FFu,6);}
bool named(u16 op,const std::string& name,int size=26){if(name.empty()||name.size()>23)return false;auto p=packet(op,size);std::copy(name.begin(),name.end(),p.begin()+2);return send(p);}
void tradeReset(){state.trade.width=380;state.trade.height=170;state.tradeIncoming=state.tradeActive=state.tradeWaiting=state.localLocked=state.remoteLocked=state.lockPending=state.commitPending=state.tradeAddPending=false;state.localOffer.clear();state.remoteOffer.clear();state.localZeny=state.remoteZeny=0;}
bool busy(){return state.tradeActive||state.tradeWaiting||state.tradeIncoming||state.vendingActive||state.vendingPending;}
ITEM_INFO item(const u8* p,bool equip,int size){
    ITEM_INFO i;i.m_itemIndex=read(p);i.SetItemId(read(p+2));i.m_itemType=p[4];i.m_isIdentified=p[5];
    if(equip){i.m_num=1;i.m_location=read(p+6);i.m_isDamaged=p[10];i.m_refiningLevel=p[11];for(int n=0;n<4;++n)i.m_slot[n]=read(p+12+n*2);if(size>=24)i.m_deleteTime=int(read(p+20,4));}
    else{i.m_num=int(read(p+6));i.m_location=read(p+8);if(size>=18)for(int n=0;n<4;++n)i.m_slot[n]=read(p+10+n*2);if(size>=22)i.m_deleteTime=int(read(p+18,4));}
    return i;
}
void remove(std::map<u32,ITEM_INFO>& list,u32 index,u32 amount){auto it=list.find(index);if(it==list.end())return;if(amount>=u32(it->second.m_num))list.erase(it);else it->second.m_num-=int(amount);}
Window* window(const std::string& name){if(name=="cart")return &state.cart;if(name=="vending")return &state.vending;if(name=="trade")return &state.trade;if(name=="shop")return &state.shop;if(name=="mail")return &state.mail;if(name=="equipment")return &state.equipment;return nullptr;}
}
void Reset(){state=State{};state.vending.x=800;state.trade.x=360;state.trade.width=380;state.trade.height=170;state.shop.x=420;state.mail.x=480;state.equipment.x=480;}
void CloseTransactions(){state.cart.visible=state.vending.visible=state.trade.visible=state.shop.visible=state.mail.visible=state.equipment.visible=false;state.promptKind.clear();tradeReset();state.draft.clear();state.vendingActive=state.vendingPending=false;state.shopBoards.clear();state.cartItems.clear();}
void UpdateTradeContext(u32 now,std::map<u32,TradePeer> peers){
    state.clock=now;state.tradePeers=std::move(peers);
    if(!state.tradeWaiting&&!state.tradeIncoming)return;
    if(!state.partnerId)for(const auto& peer:state.tradePeers)if(peer.second.name==state.partner){state.partnerId=peer.first;break;}
    auto peer=state.tradePeers.find(state.partnerId);
    const bool expired=now-state.tradeRequestTick>=30000u;
    if(!state.partnerId&&!expired)return; // Old invitations carry a name without an AID; wait for identity resolution.
    if(peer!=state.tradePeers.end()&&peer->second.distance<=2&&!expired)return;
    if(state.tradeIncoming){auto p=packet(0xe6,3);p[2]=4;send(p);}else simple(0xed);
    const bool tooDistant=peer!=state.tradePeers.end()&&peer->second.distance>2;
    tradeReset();state.trade.visible=false;
    note(tooDistant?"Trade failed: the other player is too far away.":expired?"Trade request expired without a server reply. Please try again.":"Trade canceled: the other player is no longer nearby.");
}
bool HasInput(){return !state.shopBoards.empty()||state.cart.visible||state.vending.visible||state.trade.visible||state.shop.visible||state.mail.visible||state.equipment.visible||!state.promptKind.empty()||(g_windowMgr.m_itemWnd && g_windowMgr.m_itemWnd->m_show)||(g_windowMgr.m_playerContextMenuWnd&&g_windowMgr.m_playerContextMenuWnd->m_show);}
bool HitTest(int x,int y){for(const auto* w:{&state.cart,&state.vending,&state.trade,&state.shop,&state.mail,&state.equipment})if(w->visible && x>=w->x && y>=w->y && x<w->x+w->width && y<w->y+w->height)return true;return !state.promptKind.empty();}

bool Receive(CGameMode&,const PacketView& v){
    if(!v.data||v.packetLength<2)return false;const auto* p=v.data;const int n=v.packetLength;const auto op=v.packetId;
    int entry=0;bool equip=false;
    switch(op){case 0x123:entry=10;break;case 0x1ef:entry=18;break;case 0x2e9:entry=22;break;case 0x122:entry=20;equip=true;break;case 0x297:entry=24;equip=true;break;case 0x2d2:entry=26;equip=true;break;}
    if(entry){if(n<4||(n-4)%entry)return false;auto next=state.cartItems;for(auto it=next.begin();it!=next.end();)if((it->second.m_itemType==4||it->second.m_itemType==5||it->second.m_itemType==7||it->second.m_itemType==8)==equip)it=next.erase(it);else ++it;for(int at=4;at<n;at+=entry){auto i=item(p+at,equip,entry);if(i.m_itemIndex && i.GetItemId() && i.m_num>0)next[i.m_itemIndex]=i;}state.cartItems=std::move(next);state.hasCart=true;return true;}
    switch(op){
    case 0x110:if(n<10)return false;if(read(p+2)==41 && p[8]==0){state.vendingPending=false;state.vendingActive=false;state.status="Vending was rejected by the server. Check the skill, cart and selected items.";}return true;
    case 0x121:if(n<14)return false;state.hasCart=true;state.cartCount=read(p+2);state.cartCapacity=read(p+4);state.cartWeight=read(p+6,4);state.cartMaxWeight=read(p+10,4);return true;
    case 0x12b:state.hasCart=false;state.cartItems.clear();state.cart.visible=state.vending.visible=false;state.draft.clear();state.vendingActive=state.vendingPending=false;return true;
    case 0x124:case 0x1c5:{const bool typed=op==0x1c5;const int shift=typed?1:0;if(n<21+shift)return false;ITEM_INFO i;i.m_itemIndex=read(p+2);i.m_num=int(read(p+4,4));i.SetItemId(read(p+8));i.m_itemType=typed?p[10]:0;i.m_isIdentified=p[10+shift];i.m_isDamaged=p[11+shift];i.m_refiningLevel=p[12+shift];for(int k=0;k<4;++k)i.m_slot[k]=read(p+13+shift+k*2);auto old=state.cartItems.find(i.m_itemIndex);if(old!=state.cartItems.end())i.m_num+=old->second.m_num;state.cartItems[i.m_itemIndex]=i;state.hasCart=true;return true;}
    case 0x125:if(n<8)return false;remove(state.cartItems,read(p+2),read(p+4,4));return true;
    case 0x12c:if(n<3)return false;note(p[2]==0?"Cart transfer failed: cart weight limit.":"Cart transfer failed: cart item capacity limit.");return true;
    case 0x12d:if(n<4)return false;state.vendingSlots=std::min<u32>(12,read(p+2));state.vending.visible=state.cart.visible=true;state.draft.clear();state.vendingActive=state.vendingPending=false;state.status="Drag cart items here, then set quantities and prices.";return true;
    case 0x136:case 0x133:{if(n<8||(n-8)%22)return false;std::vector<Offer> offers;for(int at=8;at<n;at+=22){Offer o;o.price=read(p+at,4);o.amount=read(p+at+(op==0x136?6:4));o.item.m_itemIndex=read(p+at+(op==0x136?4:6));o.item.m_itemType=p[at+8];o.item.SetItemId(read(p+at+9));o.item.m_isIdentified=p[at+11];o.item.m_isDamaged=p[at+12];o.item.m_refiningLevel=p[at+13];o.item.m_num=int(o.amount);for(int k=0;k<4;++k)o.item.m_slot[k]=read(p+at+14+k*2);offers.push_back(o);}if(op==0x136){state.draft=offers;state.vendingActive=true;state.vendingPending=false;state.vending.visible=true;state.status="Shop is open.";}else{state.shopItems=offers;state.sellerId=read(p+4,4);state.shop.visible=true;state.status="Select an item and quantity to purchase.";}return true;}
    case 0x137:if(n<6)return false;for(auto& o:state.draft)if(o.item.m_itemIndex==read(p+2))o.amount=o.amount>read(p+4)?o.amount-read(p+4):0;state.draft.erase(std::remove_if(state.draft.begin(),state.draft.end(),[](const auto& o){return !o.amount;}),state.draft.end());return true;
    case 0x135:if(n<7)return false;if(!p[6])for(auto& o:state.shopItems)if(o.item.m_itemIndex==read(p+2))o.amount=o.amount>read(p+4)?o.amount-read(p+4):0;state.status=p[6]?"Purchase failed: funds, weight, stock or shop changed.":"Purchase confirmed.";return true;
    case 0x138:if(n<3)return false;state.vendingPending=false;if(p[2]){state.vendingActive=false;state.status="The server could not open your shop.";}return true;
    case 0x131:if(n<86)return false;state.shopBoards[read(p+2,4)]=text(p+6,80);return true;
    case 0x132:if(n<6)return false;state.shopBoards.erase(read(p+2,4));if(read(p+2,4)==g_session.m_aid){state.vendingActive=false;state.vendingPending=false;}return true;
    case 0xe5:case 0x1f4:if(n<(op==0xe5?26:32))return false;tradeReset();state.partnerId=0;state.tradeRequestTick=state.clock;state.partner=text(p+2,24);if(op==0x1f4)state.partnerId=read(p+26,4);state.tradeIncoming=true;state.trade.visible=true;state.status="Trade invitation. Review the player name before accepting.";return true;
    case 0xe7:case 0x1f5:if(n<3)return false;if(p[2]==3){tradeReset();state.tradeActive=true;state.trade.visible=true;state.trade.width=660;state.trade.height=420;if(op==0x1f5 && n>=9)state.partnerId=read(p+3,4);state.status="Drag inventory items into your offer. Lock, then confirm after both players lock.";}else{tradeReset();state.trade.visible=false;const char* reasons[]={"Trade failed: the other player is too far away.","Trade failed: the other player is unavailable.","Trade request failed.","", "Trade request declined or canceled.","Trade failed: the other player is busy."};note(p[2]<6?reasons[p[2]]:"Trade request failed.");}return true;
    case 0xe9:{if(n<19||!state.tradeActive)return false;const u32 amount=read(p+2,4),id=read(p+6);if(!id)state.remoteZeny=amount;else{Offer o;o.amount=amount;o.item.SetItemId(id);o.item.m_num=int(amount);o.item.m_isIdentified=p[8];o.item.m_isDamaged=p[9];o.item.m_refiningLevel=p[10];for(int k=0;k<4;++k)o.item.m_slot[k]=read(p+11+2*k);state.remoteOffer.push_back(o);}return true;}
    case 0xea:if(n<5)return false;if(state.tradeAddPending && read(p+2)==state.pendingTrade.item.m_itemIndex){if(p[4]==0){auto old=std::find_if(state.localOffer.begin(),state.localOffer.end(),[&](const auto& o){return o.item.m_itemIndex==state.pendingTrade.item.m_itemIndex;});if(old==state.localOffer.end())state.localOffer.push_back(state.pendingTrade);else old->amount+=state.pendingTrade.amount;}else state.status="Server rejected the offered item or amount.";state.tradeAddPending=false;}return true;
    case 0xec:if(n<3||!state.tradeActive)return false;if(p[2]==0){state.localLocked=true;state.lockPending=false;}else state.remoteLocked=true;return true;
    case 0xee:tradeReset();state.trade.visible=false;note("Trade canceled. No exchange was completed.");return true;
    case 0xf0:if(n<3)return false;tradeReset();state.trade.visible=false;note(p[2]?"Trade failed.":"Trade completed.");return true;
    case 0xf1:state.localOffer.clear();state.localZeny=0;state.localLocked=state.lockPending=false;return true;
    case 0x16a:if(n<30)return false;state.promptKind="guild";state.promptId=read(p+2,4);state.promptName=text(p+6,24);return true;
    case 0x207:if(n<34)return false;state.promptKind="friend";state.promptId=read(p+2,4);state.promptChar=read(p+6,4);state.promptName=text(p+10,24);return true;
    case 0x209:if(n<36)return false;note(read(p+2)?"Friend request was declined or could not be completed.":"Friend added.");return true;
    case 0x169:if(n<3)return false;note(p[2]==2?"Guild invitation accepted.":p[2]==3?"Guild invitation failed: guild is full.":p[2]==0?"Player already belongs to a guild.":"Guild invitation declined.");return true;
    case 0x2d7:{if(n<43||(n-43)%26)return false;std::vector<ITEM_INFO> items;for(int at=43;at<n;at+=26)items.push_back(item(p+at,true,26));state.viewedEquipment=std::move(items);state.equipment.visible=true;state.partner=text(p+4,24);state.status="Equipment shared by this player.";return true;}
    case 0xd1:if(n<4)return false;note(p[3]?"Whisper preference could not be changed.":"Whisper preference updated.");return true;
    case 0x249:if(n<3)return false;state.mailPending=false;note(p[2]?"Mail could not be sent.":"Mail sent.");if(!p[2])state.mail.visible=false;return true;
    default:return false;
    }
}

bool Action(const Command& c){
    state.status.clear();
    if(c.action=="moveWindow"){if(auto* w=window(c.to)){w->x=std::max(0,c.x);w->y=std::max(0,c.y);return true;}if(c.to=="inventory" && g_windowMgr.m_itemWnd){g_windowMgr.m_itemWnd->Move(std::max(0,c.x),std::max(0,c.y));return true;}return false;}
    if(c.action=="inventoryTab"){state.inventoryTab=std::clamp(c.tab,0,2);return true;}
    if(c.action=="cart"){if(state.hasCart){state.cart.visible=!state.cart.visible;return true;}return false;}
    if(c.action=="close"){
        if(c.to=="inventory" && g_windowMgr.m_itemWnd){g_windowMgr.m_itemWnd->SetShow(0);return true;}
        if(c.to=="trade"){bool sent=false;if(state.tradeIncoming){auto p=packet(0xe6,3);p[2]=4;sent=send(p);}else sent=simple(0xed);if(!sent)return false;tradeReset();state.trade.visible=false;note("Trade canceled. No exchange was completed.");return true;}
        if(c.to=="vending"){if(state.vendingActive){if(!simple(0x12e))return false;state.vendingActive=false;}else{auto p=packet(0x1b2,85);put(p,2,85);if(!send(p))return false;}state.draft.clear();state.vendingPending=false;}
        if(auto* w=window(c.to)){w->visible=false;return true;}return false;
    }
    if(c.action=="use"){if(busy())return false;const auto* i=g_session.GetInventoryItemByIndex(c.index);if(!i)return false;return g_modeMgr.SendMsg(i->m_wearLocation?CGameMode::GameMsg_RequestUnequipInventoryItem:i->m_location?CGameMode::GameMsg_RequestEquipInventoryItem:CGameMode::GameMsg_RequestUseInventoryItem,c.index,i->m_location,0)!=0;}
    if(c.action=="transfer"){
        if(!state.hasCart||busy()||!c.amount||c.amount>32767)return false;
        const ITEM_INFO* i=nullptr;if(c.from=="inventory")i=g_session.GetInventoryItemByIndex(c.index);else{auto it=state.cartItems.find(c.index);if(it!=state.cartItems.end())i=&it->second;}
        if(!i||i->m_wearLocation||c.amount>u32(i->m_num))return false;
        if(c.from=="cart")for(const auto& o:state.draft)if(o.item.m_itemIndex==c.index){state.status="Remove this item from the vending list first.";return false;}
        auto p=packet(c.from=="inventory"?0x126:0x127,8);put(p,2,c.index);put(p,4,c.amount,4);return send(p);
    }
    if(c.action=="offerVend"){
        if(!state.vending.visible||state.vendingActive||state.vendingPending)return false;const auto found=state.cartItems.find(c.index);if(found==state.cartItems.end()||found->second.m_deleteTime)return false;
        for(const auto& o:state.draft)if(o.item.m_itemIndex==c.index)return false;
        if(state.draft.size()>=state.vendingSlots)return false;state.draft.push_back({found->second,1,1});return true;
    }
    if(c.action=="removeVend"||c.action=="editVend"){
        if(state.vendingActive||state.vendingPending)return false;
        auto found=std::find_if(state.draft.begin(),state.draft.end(),[&](const auto& o){return o.item.m_itemIndex==c.index;});if(found==state.draft.end())return false;
        if(c.action=="removeVend"){state.draft.erase(found);return true;}const auto item=state.cartItems.find(c.index);
        if(item==state.cartItems.end()||!c.amount||c.amount>u32(item->second.m_num)||c.amount>32767||!c.price||c.price>1000000000)return false;found->amount=c.amount;found->price=c.price;return true;
    }
    if(c.action=="openVend"){
        if(state.vendingActive||state.vendingPending||state.draft.empty()||c.text.empty()||c.text.size()>79)return false;
        auto p=packet(0x1b2,85+int(state.draft.size())*8);put(p,2,u32(p.size()));std::copy(c.text.begin(),c.text.end(),p.begin()+4);p[84]=1;
        for(size_t k=0;k<state.draft.size();++k){const auto& o=state.draft[k];const auto i=state.cartItems.find(o.item.m_itemIndex);if(i==state.cartItems.end()||!o.amount||o.amount>u32(i->second.m_num)||!o.price)return false;put(p,85+int(k)*8,o.item.m_itemIndex);put(p,87+int(k)*8,o.amount);put(p,89+int(k)*8,o.price,4);}
        if(!send(p))return false;state.vendingPending=true;state.shopTitle=c.text;state.status="Opening shop; waiting for server confirmation.";return true;
    }
    if(c.action=="tradeReply"){if(!state.tradeIncoming)return false;if(c.amount){auto peer=state.tradePeers.find(state.partnerId);if(peer==state.tradePeers.end()||peer->second.distance>2){auto decline=packet(0xe6,3);decline[2]=4;send(decline);tradeReset();state.trade.visible=false;note("Trade failed: the other player is too far away or unavailable.");return false;}}auto p=packet(0xe6,3);p[2]=c.amount?3:4;if(!send(p))return false;if(!c.amount){tradeReset();state.trade.visible=false;note("Trade invitation declined.");}else{state.tradeIncoming=false;state.tradeWaiting=true;state.tradeRequestTick=state.clock;state.status="Acceptance sent; waiting for the server.";}return true;}
    if(c.action=="offerTrade"||c.action=="tradeZeny"){
        if(!state.tradeActive||state.localLocked||state.lockPending||state.tradeAddPending||(!c.amount && c.action!="tradeZeny"))return false;Offer o;
        if(c.action=="tradeZeny"){const auto* player=g_session.GetSelectedCharacterInfo();if(!player||c.amount>u32(std::max(0,player->money)))return false;o.amount=c.amount;}
        else{const auto* i=g_session.GetInventoryItemByIndex(c.index);if(!i||i->m_wearLocation||i->m_deleteTime||c.amount>32767)return false;u32 offered=0;for(const auto& old:state.localOffer)if(old.item.m_itemIndex==c.index)offered+=old.amount;if(offered+c.amount>u32(i->m_num)||state.localOffer.size()>=10)return false;o={*i,c.amount,0};}
        auto p=packet(0xe8,8);put(p,2,o.item.m_itemIndex);put(p,4,c.amount,4);if(!send(p))return false;
        // rAthena does not echo a Zeny-add acknowledgement to the sender.
        // This is the editable offer, not the character's actual balance.
        if(c.action=="tradeZeny"){state.localZeny=c.amount;return true;}
        state.pendingTrade=o;state.tradeAddPending=true;return true;
    }
    if(c.action=="lockTrade"){if(!state.tradeActive||state.localLocked||state.lockPending||state.tradeAddPending)return false;if(!simple(0xeb))return false;state.lockPending=true;return true;}
    if(c.action=="commitTrade"){if(!state.tradeActive||!state.localLocked||!state.remoteLocked||state.commitPending)return false;if(!simple(0xef))return false;state.commitPending=true;state.status="Final confirmation sent; waiting for the server.";return true;}
    if(c.action=="buy"){auto found=std::find_if(state.shopItems.begin(),state.shopItems.end(),[&](const auto& o){return o.item.m_itemIndex==c.index;});if(found==state.shopItems.end()||!c.amount||c.amount>found->amount||busy())return false;auto p=packet(0x134,12);put(p,2,12);put(p,4,state.sellerId,4);put(p,8,c.amount);put(p,10,c.index);return send(p);}
    if(c.action=="prompt"){
        std::vector<u8> p;if(state.promptKind=="guild"){p=packet(0x16b,10);put(p,2,state.promptId,4);put(p,6,c.amount?1:0,4);}else if(state.promptKind=="alliance"){p=packet(0x172,10);put(p,2,state.promptId,4);put(p,6,c.amount?1:0,4);}else if(state.promptKind=="friend"){p=packet(0x208,14);put(p,2,state.promptId,4);put(p,6,state.promptChar,4);put(p,10,c.amount?1:0,4);}else return false;
        if(!send(p))return false;state.promptKind.clear();return true;
    }
    if(c.action=="mail"){
        if(state.mailPending||c.text.empty()||c.text.size()>23||c.subject.empty()||c.subject.size()>39||c.body.size()>254)return false;
        auto p=packet(0x248,69+int(c.body.size())+1);put(p,2,u32(p.size()));std::copy(c.text.begin(),c.text.end(),p.begin()+4);std::copy(c.subject.begin(),c.subject.end(),p.begin()+28);p[68]=u8(c.body.size()+1);std::copy(c.body.begin(),c.body.end(),p.begin()+69);
        if(!send(p))return false;state.mailPending=true;state.status="Sending mail; waiting for the server.";return true;
    }
    state.status="That action is unavailable in the current state.";return false;
}

bool PlayerAction(int action,u32 target,const std::string& name){
    state.status.clear();if(!target||target==g_session.m_aid||name.empty())return false;
    switch(action){
    case CGameMode::PlayerContext_RequestDeal:{if(busy())return false;auto peer=state.tradePeers.find(target);if(peer==state.tradePeers.end()||peer->second.distance>2){note(peer==state.tradePeers.end()?"Trade failed: the other player is no longer nearby.":"Trade failed: the other player is too far away.");return false;}state.tradeRequestTick=state.clock;auto p=packet(0xe4,6);put(p,2,target,4);if(!send(p))return false;state.partner=name;state.partnerId=target;state.tradeWaiting=true;state.trade.visible=true;state.trade.width=380;state.trade.height=170;state.status="Waiting for the other player to accept.";return true;}
    case CGameMode::PlayerContext_InviteJoinGuild:{if(!g_session.m_localGuild.guildId||(!g_session.m_localGuild.isMaster && !(g_session.m_localGuild.permissions&1)))return false;auto p=packet(0x168,14);put(p,2,target,4);put(p,6,g_session.m_aid,4);put(p,10,g_session.m_gid,4);return send(p);}
    case CGameMode::PlayerContext_RegisterFriend:return named(0x202,name);
    case CGameMode::PlayerContext_CheckEquipmentInfo:{auto p=packet(0x2d6,6);put(p,2,target,4);return send(p);}
    case CGameMode::PlayerContext_SendWhisper:if(g_windowMgr.m_chatWnd){g_windowMgr.m_chatWnd->SetShow(1);g_windowMgr.m_chatWnd->SetQtInputText(1,name);g_windowMgr.m_chatWnd->FocusQtInput(2);return true;}return false;
    case CGameMode::PlayerContext_RejectWhispering:{if(name.size()>23)return false;auto p=packet(0xcf,27);std::copy(name.begin(),name.end(),p.begin()+2);p[26]=0;return send(p);}
    case CGameMode::PlayerContext_SendMail:{if(busy())return false;state.mailTo=name;state.mail.visible=true;state.mailPending=false;auto p=packet(0x246,4);return send(p);}
    case 8:{auto p=packet(0x130,6);put(p,2,target,4);return send(p);}
    default:return false;
    }
}
}
