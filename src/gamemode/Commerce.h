#pragma once
#include "item/Item.h"
#include <map>
#include <vector>
class CGameMode;
struct PacketView;
namespace ro::commerce {
struct Window {bool visible=false;int x=380,y=130,width=400,height=420;};
struct Offer {ITEM_INFO item;u32 amount=0,price=0;};
struct TradePeer {std::string name;int distance=0;};
struct Command {
    std::string action,from,to,text,subject,body;
    u32 index=0,amount=0,price=0,target=0;
    int x=0,y=0,tab=0;
};
struct State {
    Window cart,vending,trade,shop,mail,equipment;
    bool hasCart=false,vendingActive=false,vendingPending=false,tradeIncoming=false,tradeActive=false,tradeWaiting=false;
    bool localLocked=false,remoteLocked=false,lockPending=false,commitPending=false,mailPending=false;
    u32 cartCount=0,cartCapacity=100,cartWeight=0,cartMaxWeight=0,vendingSlots=0,partnerId=0,sellerId=0;
    u32 localZeny=0,remoteZeny=0;
    u32 clock=0,tradeRequestTick=0;
    std::map<u32,TradePeer> tradePeers;
    int inventoryTab=0;
    std::string partner,shopTitle,mailTo,status;
    std::map<u32,ITEM_INFO> cartItems;
    std::vector<Offer> draft,localOffer,remoteOffer,shopItems;
    std::map<u32,std::string> shopBoards;
    Offer pendingTrade;bool tradeAddPending=false;
    std::string promptKind,promptName;u32 promptId=0,promptChar=0;
    std::vector<ITEM_INFO> viewedEquipment;
};
extern State state;
void Reset();
void CloseTransactions();
bool Receive(CGameMode&,const PacketView&);
bool Action(const Command&);
bool PlayerAction(int action,u32 target,const std::string& name);
bool HasInput();
bool HitTest(int x,int y);
void UpdateTradeContext(u32 now,std::map<u32,TradePeer> peers);
}
