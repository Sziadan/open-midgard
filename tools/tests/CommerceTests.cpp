#include "gamemode/Commerce.h"
#include <cassert>
#include <algorithm>
#include <cstring>
#include <iostream>
void ITEM_INFO::SetItemId(unsigned int id){m_itemName=std::to_string(id);}
unsigned int ITEM_INFO::GetItemId()const{return m_itemName.empty()?0:unsigned(std::stoul(m_itemName));}
struct PacketView {u16 packetId,packetLength;const u8* data;};
struct CGameMode {
    enum {PlayerContext_CheckEquipmentInfo,PlayerContext_RequestDeal,PlayerContext_AskJoinParty,PlayerContext_InviteJoinGuild,PlayerContext_SendWhisper,PlayerContext_RegisterFriend,PlayerContext_SendMail,PlayerContext_RejectWhispering};
    enum {GameMsg_RequestUseInventoryItem=10,GameMsg_RequestEquipInventoryItem=11,GameMsg_RequestUnequipInventoryItem=12};
};
struct {bool SendMsg(int,unsigned int,int,int){return true;}} g_modeMgr;
struct Window {int m_show=0;void Move(int,int){}void SetShow(int n){m_show=n;}void SetQtInputText(int,const std::string&){}void FocusQtInput(int){}};
struct {Window* m_itemWnd=nullptr;Window* m_chatWnd=nullptr;Window* m_playerContextMenuWnd=nullptr;std::string lastChat;void PushChatEvent(const char* text,u32,int){lastChat=text;}} g_windowMgr;
struct Session {
    struct {u32 guildId=1;bool isMaster=true;int permissions=0;}m_localGuild;
    struct {int money=100000;} character;
    u32 m_aid=101,m_gid=202;std::map<u32,ITEM_INFO> inventory;
    const ITEM_INFO* GetInventoryItemByIndex(u32 id){auto it=inventory.find(id);return it==inventory.end()?nullptr:&it->second;}
    const auto* GetSelectedCharacterInfo(){return &character;}
} g_session;
struct CRagConnection {
    std::vector<std::vector<u8>> sent;bool success=true;
    static CRagConnection* instance(){static CRagConnection s;return &s;}
    bool SendPacket(const char* p,int size){if(!success)return false;sent.emplace_back(p,p+size);return true;}
};
#include "Commerce.inc"
int main(){
    using namespace ro::commerce;Reset();CGameMode mode;
    auto receive=[&](std::vector<u8> p){return Receive(mode,{u16(p[0]|(p[1]<<8)),u16(p.size()),p.data()});};
    auto* socket=CRagConnection::instance();
    UpdateTradeContext(100,{{999,{"Trader",1}}});
    ITEM_INFO potion;potion.SetItemId(501);potion.m_itemIndex=2;potion.m_num=100;potion.m_isIdentified=1;g_session.inventory[2]=potion;
    std::vector<u8> cart={0x23,1,14,0,2,0,0xf5,1,0,1,20,0,0,0};assert(receive(cart)&&state.cartItems.at(2).m_num==20);
    cart.pop_back();assert(!receive(cart)&&state.cartItems.at(2).m_num==20);
    Command c;c.action="transfer";c.from="inventory";c.index=2;c.amount=3;assert(Action(c));
    assert(socket->sent.back()==std::vector<u8>({0x26,1,2,0,3,0,0,0}));assert(g_session.inventory.at(2).m_num==100);
    c.amount=101;assert(!Action(c));c.amount=3;c.from="cart";assert(Action(c));assert(socket->sent.back()[0]==0x27);
    assert(receive({0x2c,1,0})&&state.status.find("failed")!=std::string::npos);
    assert(receive({0x2d,1,4,0}));c.action="offerVend";assert(Action(c));
    c.action="editVend";c.amount=3;c.price=250;assert(Action(c));c.action="openVend";c.text="Test shop";assert(Action(c));
    const auto opening=socket->sent.back();assert(opening.size()==93&&opening[0]==0xb2&&opening[1]==1&&opening[2]==93&&opening[84]==1&&opening[85]==2&&opening[87]==3&&opening[89]==250);
    assert(receive({0x10,1,41,0,0,0,0,0,0,0})&&!state.vendingPending);assert(Action(c));
    c.action="transfer";assert(!Action(c)); // Opening shop locks transfers.
    std::vector<u8> opened(30);opened[0]=0x36;opened[1]=1;opened[2]=30;opened[8]=250;opened[12]=2;opened[14]=3;opened[17]=0xf5;opened[18]=1;opened[19]=1;
    assert(receive(opened)&&state.vendingActive&&!state.vendingPending);c.action="close";c.to="vending";assert(Action(c));assert(socket->sent.back()==std::vector<u8>({0x2e,1}));
    assert(receive({0xe7,0,0})&&!state.trade.visible&&g_windowMgr.lastChat.find("too far")!=std::string::npos);
    assert(receive({0xe7,0,5})&&g_windowMgr.lastChat.find("busy")!=std::string::npos);
    assert(PlayerAction(CGameMode::PlayerContext_RequestDeal,999,"Trader"));assert(socket->sent.back()==std::vector<u8>({0xe4,0,0xe7,3,0,0}));
    assert(receive({0xf5,1,3,0xe7,3,0,0,99,0})&&state.tradeActive);
    c={};c.action="offerTrade";c.index=2;c.amount=5;assert(Action(c));assert(state.localOffer.empty());assert(g_session.inventory.at(2).m_num==100);
    assert(receive({0xea,0,3,0,0})&&state.localOffer.empty()&&state.tradeAddPending);assert(receive({0xea,0,2,0,0})&&state.localOffer.size()==1);
    c.action="tradeZeny";c.amount=500;assert(Action(c)&&state.localZeny==500&&!state.tradeAddPending);assert(g_session.character.money==100000);
    c.action="commitTrade";assert(!Action(c));c.action="lockTrade";assert(Action(c)&&state.lockPending);assert(!Action(c));
    assert(receive({0xec,0,0})&&state.localLocked);c.action="offerTrade";assert(!Action(c));c.action="commitTrade";assert(!Action(c));
    assert(receive({0xec,0,1})&&state.remoteLocked);assert(Action(c)&&socket->sent.back()==std::vector<u8>({0xef,0}));assert(!Action(c));
    assert(receive({0xf0,0,0})&&!state.tradeActive&&!state.trade.visible);assert(!receive({0xec,0,0}));
    assert(PlayerAction(CGameMode::PlayerContext_RegisterFriend,999,"Friend"));assert(socket->sent.back().size()==26&&socket->sent.back()[0]==2&&socket->sent.back()[1]==2);
    assert(PlayerAction(CGameMode::PlayerContext_InviteJoinGuild,999,"Friend"));assert(socket->sent.back().size()==14&&socket->sent.back()[0]==0x68);
    g_session.m_localGuild.isMaster=false;assert(!PlayerAction(CGameMode::PlayerContext_InviteJoinGuild,999,"Friend"));
    assert(receive({0x69,1,1})&&state.status.find("declined")!=std::string::npos);assert(receive({0x69,1,2})&&state.status.find("accepted")!=std::string::npos);
    assert(PlayerAction(CGameMode::PlayerContext_RejectWhispering,999,"Friend"));assert(socket->sent.back().size()==27&&socket->sent.back()[0]==0xcf);
    state.promptKind="guild";state.promptId=77;c={};c.action="prompt";c.amount=1;assert(Action(c)&&socket->sent.back()==std::vector<u8>({0x6b,1,77,0,0,0,1,0,0,0}));
    c={};c.action="mail";c.text="Recipient";c.subject="Test";c.body="Hello";assert(Action(c));assert(socket->sent.back().size()==75&&socket->sent.back()[68]==6);assert(receive({0x49,2,0})&&!state.mailPending);
    c.body=std::string(255,'x');assert(!Action(c));
    UpdateTradeContext(200,{{999,{"Trader",5}}});
    const auto before=socket->sent.size();assert(!PlayerAction(CGameMode::PlayerContext_RequestDeal,999,"Trader")&&!state.tradeWaiting&&socket->sent.size()==before&&g_windowMgr.lastChat.find("too far")!=std::string::npos);
    UpdateTradeContext(300,{{999,{"Trader",1}}});assert(PlayerAction(CGameMode::PlayerContext_RequestDeal,999,"Trader"));
    UpdateTradeContext(400,{{999,{"Trader",5}}});assert(!state.tradeWaiting&&!state.trade.visible&&socket->sent.back()==std::vector<u8>({0xed,0}));
    UpdateTradeContext(500,{{999,{"Trader",1}}});assert(PlayerAction(CGameMode::PlayerContext_RequestDeal,999,"Trader"));
    UpdateTradeContext(30501,{{999,{"Trader",1}}});assert(!state.tradeWaiting&&!state.trade.visible&&g_windowMgr.lastChat.find("expired")!=std::string::npos);
    std::vector<u8> invite(32);invite[0]=0xf4;invite[1]=1;std::memcpy(invite.data()+2,"Trader",6);invite[26]=0xe7;invite[27]=3;
    assert(receive(invite));state.tradePeers[999].distance=8;c={};c.action="tradeReply";c.amount=1;assert(!Action(c)&&!state.tradeIncoming&&!state.trade.visible&&socket->sent.back()==std::vector<u8>({0xe6,0,4}));
    assert(receive(invite));c.amount=0;assert(Action(c)&&!state.tradeIncoming&&!state.trade.visible);
    UpdateTradeContext(30600,{{999,{"Trader",1}}});assert(PlayerAction(CGameMode::PlayerContext_RequestDeal,999,"Trader"));
    c={};c.action="close";c.to="trade";assert(Action(c)&&!state.tradeWaiting&&!state.trade.visible);
    state.promptKind="alliance";state.promptId=77;c={};c.action="prompt";c.amount=1;assert(Action(c)&&socket->sent.back()==std::vector<u8>({0x72,1,77,0,0,0,1,0,0,0}));
    socket->success=false;c={};c.action="transfer";c.from="inventory";c.index=2;c.amount=1;assert(!Action(c));
    std::cout<<"PASS: cart snapshots/transfers, malformed arrays, vending stock/price packets, trade acknowledgements/locks/commit, no optimistic item or Zeny mutation, social permissions, mail limits and connection failures\n";
}
