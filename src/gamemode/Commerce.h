#pragma once
#include "item/Item.h"
#include <array>
#include <map>
#include <optional>
#include <vector>
class CGameMode;
struct PacketView;
namespace ro::commerce {
struct Window {bool visible=false;int x=380,y=130,width=400,height=420;};
struct Offer {ITEM_INFO item;u32 amount=0,price=0;};
struct TradePeer {std::string name;int distance=0;};
enum class TradeCancellation { None, Decline, Cancel };
struct MailHeader {
    std::string title, sender;
    u32 sentAt = 0;
    bool read = false;
};
struct MailDetail {
    std::string title, sender, body;
    std::array<u8, 24> senderWire{};
    u32 zeny = 0;
    u32 itemAmount = 0;
    ITEM_INFO item;
};
struct MailNotification {
    u32 id = 0;
    std::string title, sender;
};
struct MailClaim {
    u32 id = 0;
    u32 sentAt = 0;
    unsigned successesRemaining = 0;
    u8 refusal = 0;
    bool needsReadback = false;
    bool terminalReadRequested = false;
};
enum class MailRemovalKind { Delete, Return };
struct MailRemoval {
    MailRemovalKind kind;
    u32 id;
    u32 sentAt;
};
struct MailInbox {
    std::map<u32, MailHeader> headers;
    std::map<u32, MailDetail> details;
    std::optional<MailNotification> notification;
    u32 selectedId = 0;
    u32 pendingReadId = 0;
    std::optional<MailClaim> claim;
    std::optional<MailRemoval> removal;
};
struct Command {
    std::string action,from,to,text,subject,body;
    u32 index=0,amount=0,price=0,target=0;
    int x=0,y=0,tab=0;
    u32 promptAid=0,promptCid=0;
    u32 itemId=0;
};
struct State {
    Window cart,vending,trade,shop,mail,equipment,mailInbox;
    bool hasCart=false,vendingActive=false,vendingPending=false,tradeIncoming=false,tradeActive=false,tradeWaiting=false;
    bool localLocked=false,remoteLocked=false,lockPending=false,commitPending=false,mailPending=false;
    bool vendingOpenCanceled=false;
    TradeCancellation tradeCancellation=TradeCancellation::None;
    u32 cartCount=0,cartCapacity=100,cartWeight=0,cartMaxWeight=0,vendingSlots=0,partnerId=0,sellerId=0;
    u32 requestedSellerId=0;
    u32 localZeny=0,remoteZeny=0;
    u32 clock=0,tradeRequestTick=0;
    std::map<u32,TradePeer> tradePeers;
    int inventoryTab=0;
    std::string partner,shopTitle,mailTo,status;
    MailInbox inbox;
    std::map<u32,ITEM_INFO> cartItems;
    std::vector<Offer> draft,localOffer,remoteOffer,shopItems;
    std::map<u32,std::string> shopBoards;
    Offer pendingTrade;bool tradeAddPending=false;
    std::string promptKind,promptName;u32 promptId=0,promptChar=0;
    std::vector<ITEM_INFO> viewedEquipment;
};
extern State state;
void Reset();
// Fresh map snapshot: clear session items/storage and commerce server state,
// retaining local panel positions and the selected inventory tab.
void ResetForMapEntry();
void CloseTransactions();
bool Receive(CGameMode&,const PacketView&);
bool Action(const Command&);
bool MailClaimCanCheck(u32 now);
bool MailRemovalTimedOut(u32 now);
bool SendFriendInvitationReply(u32 aid, u32 cid, bool accept);
bool PlayerAction(int action,u32 target,const std::string& name);
bool HasInput();
bool HitTest(int x,int y);
void UpdateTradeContext(u32 now,std::map<u32,TradePeer> peers);
}
