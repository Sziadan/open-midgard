#include "Commerce.h"
#include "GameMode.h"
#include "GameModePacket.h"
#include "ItemSnapshot.h"
#include "core/SourceTextCodec.h"
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
constexpr u32 kMailClaimWaitMs = 15000;
constexpr u32 kMailRemovalWaitMs = 15000;
constexpr u32 kMailStageWaitMs = 15000;
u32 read(const u8* p,int bytes=2){u32 v=0;for(int i=0;i<bytes;++i)v|=u32(p[i])<<(8*i);return v;}
void put(std::vector<u8>& p,int at,u32 v,int bytes=2){for(int i=0;i<bytes;++i)p[at+i]=u8(v>>(8*i));}
std::vector<u8> packet(u16 op,int size){std::vector<u8> p(size);put(p,0,op);return p;}
std::string text(const u8* p,int length){int n=0;while(n<length && p[n])++n;return std::string(reinterpret_cast<const char*>(p),n);}
std::string mailText(const u8* p, int length)
{
    auto bytes = text(p, length);
    return DecodeSourceText(bytes, SelectedSourceTextCodePage()).value_or(std::move(bytes));
}

bool receiveMailList(const u8* p, int length)
{
    constexpr int rowLength = 73;
    if (length < 8 || read(p + 2) != static_cast<u32>(length)) return false;
    const u32 count = read(p + 4, 4);
    if (count > 30 || length != 8 + rowLength * static_cast<int>(count)) return false;

    std::map<u32, MailHeader> headers;
    for (const u8* row = p + 8; row != p + length; row += rowLength) {
        const u32 id = read(row, 4);
        if (id == 0 || row[44] > 1) return false;
        MailHeader header{mailText(row + 4, 40), mailText(row + 45, 24),
            read(row + 69, 4), row[44] != 0};
        if (!headers.emplace(id, std::move(header)).second) return false;
    }

    auto& inbox = state.inbox;
    inbox.headers = std::move(headers);
    for (auto it = inbox.details.begin(); it != inbox.details.end();) {
        if (inbox.headers.count(it->first)) ++it;
        else it = inbox.details.erase(it);
    }
    if (!inbox.headers.count(inbox.selectedId)) inbox.selectedId = 0;
    return true;
}

bool receiveMailDetail(const u8* p, int length)
{
    if (length < 101 || read(p + 2) != static_cast<u32>(length)) return false;
    const int bodyLength = p[99];
    if (bodyLength > 199 || length != 101 + bodyLength || p[100 + bodyLength] != 0
        || std::memchr(p + 100, 0, bodyLength)) return false;
    const u32 id = read(p + 4, 4);
    if (id == 0) return false;

    auto& inbox = state.inbox;
    if (inbox.pendingReadId != id) return true;
    MailDetail detail;
    detail.title = mailText(p + 8, 40);
    detail.sender = mailText(p + 48, 24);
    std::copy_n(p + 48, detail.senderWire.size(), detail.senderWire.begin());
    detail.body = mailText(p + 100, bodyLength);
    detail.zeny = read(p + 76, 4);
    detail.itemAmount = read(p + 80, 4);
    detail.item.SetItemId(read(p + 84));
    detail.item.m_itemType = static_cast<int>(read(p + 86));
    detail.item.m_isIdentified = p[88];
    detail.item.m_isDamaged = p[89];
    detail.item.m_refiningLevel = p[90];
    for (int i = 0; i < 4; ++i) detail.item.m_slot[i] = static_cast<int>(read(p + 91 + 2 * i));

    inbox.pendingReadId = 0;
    if (auto header = inbox.headers.find(id); header != inbox.headers.end()) {
        header->second.read = true;
    }
    if (inbox.claim && inbox.claim->id == id && inbox.claim->terminalReadRequested) {
        auto& claim = *inbox.claim;
        claim.terminalReadRequested = false;
        if (claim.refusal || (!detail.itemAmount && !detail.zeny)) {
            state.status = claim.refusal == 2 ? "Attachment claim refused. Check inventory capacity."
                : claim.refusal ? "Attachment claim refused." : "Mail attachments received.";
            inbox.claim.reset();
        }
    }
    inbox.details.insert_or_assign(id, std::move(detail));
    return true;
}
bool send(const std::vector<u8>& p){if(CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(p.data()),int(p.size())))return true;state.status="Connection lost: request was not sent.";return false;}
bool simple(u16 op){return send(packet(op,2));}
bool resetMailDraft()
{
    // Selected legacy 0246 passes a zero-based index to a helper expecting +2.
    // Sending it cannot establish that an item was cleared.
    if (state.mailAttachment.pending || state.mailAttachment.staged) return false;
    if (!send(packet(0x246, 4))) return false;
    return true;
}
bool busy();
bool requestMailRead(u32 id)
{
    auto p = packet(0x241, 6);
    put(p, 2, id, 4);
    if (!send(p)) return false;
    auto& inbox = state.inbox;
    inbox.pendingReadId = id;
    if (inbox.claim && inbox.claim->id == id && inbox.claim->needsReadback) {
        inbox.claim->terminalReadRequested = true;
    }
    return true;
}

bool receiveMailRemoval(MailRemovalKind kind, u32 id, u32 result)
{
    auto& inbox = state.inbox;
    if (!inbox.removal || inbox.removal->kind != kind || inbox.removal->id != id) return false;
    inbox.removal.reset();
    if (result != 0) {
        state.status = kind == MailRemovalKind::Return ? "The server refused to return the message."
            : "The server refused to delete the message.";
        return true;
    }
    inbox.headers.erase(id);
    inbox.details.erase(id);
    if (inbox.selectedId == id) inbox.selectedId = 0;
    state.status = kind == MailRemovalKind::Return ? "Message returned." : "Message deleted.";
    return true;
}

void receiveMailClaimResult(u8 result)
{
    auto& inbox = state.inbox;
    if (!inbox.claim || inbox.claim->needsReadback) return;
    auto& claim = *inbox.claim;
    if (result == 0 && --claim.successesRemaining != 0) return;
    claim.refusal = result;
    claim.needsReadback = true;
    if (!inbox.pendingReadId && !state.mail.visible && !state.mailPending
        && !state.mailAttachment.pending && !state.mailAttachment.staged && !busy()) {
        requestMailRead(claim.id);
    }
}
void note(const std::string& message){state.status=message;g_windowMgr.PushChatEvent(message.c_str(),0x0080C0FFu,6);}
bool named(u16 op,const std::string& name,int size=26){if(name.empty()||name.size()>23)return false;auto p=packet(op,size);std::copy(name.begin(),name.end(),p.begin()+2);return send(p);}
void tradeReset(){state.trade.width=380;state.trade.height=170;state.tradeIncoming=state.tradeActive=state.tradeWaiting=state.localLocked=state.remoteLocked=state.lockPending=state.commitPending=state.tradeAddPending=false;state.tradeCancellation=TradeCancellation::None;state.localOffer.clear();state.remoteOffer.clear();state.localZeny=state.remoteZeny=0;}
bool tradeCancelPending(){return state.tradeCancellation != TradeCancellation::None;}
bool busy(){return tradeCancelPending()||state.tradeActive||state.tradeWaiting||state.tradeIncoming||state.vendingActive||state.vendingPending;}

void closeShopQuote()
{
    state.shop.visible=false;
    state.shopItems.clear();
    state.sellerId=0;
    state.requestedSellerId=0;
}

bool requestTradeCancellation(const char* message)
{
    if (tradeCancelPending() || (!state.tradeIncoming && !state.tradeWaiting && !state.tradeActive)) return false;
    if (state.tradeIncoming) {
        auto reply = packet(0xe6, 3);
        reply[2] = 4;
        if (!send(reply)) return false;
        state.tradeCancellation = TradeCancellation::Decline;
    } else {
        if (!simple(0xed)) return false;
        state.tradeCancellation = TradeCancellation::Cancel;
    }
    state.trade.visible = false;
    note(message);
    return true;
}
ITEM_INFO item(const u8* p,bool equip,int size){
    ITEM_INFO i;i.m_itemIndex=read(p);i.SetItemId(read(p+2));i.m_itemType=p[4];i.m_isIdentified=p[5];
    if(equip){
        i.m_num=1;
        i.m_location=read(p+6);
        i.m_wearLocation=read(p+8);
        i.m_isDamaged=p[10];
        i.m_refiningLevel=p[11];
        for(int n=0;n<4;++n)i.m_slot[n]=read(p+12+n*2);
        if(size>=24)i.m_deleteTime=int(read(p+20,4));
        if(size>=26)i.m_isYours=read(p+24);
    }
    else{i.m_num=int(read(p+6));i.m_location=read(p+8);if(size>=18)for(int n=0;n<4;++n)i.m_slot[n]=read(p+10+n*2);if(size>=22)i.m_deleteTime=int(read(p+18,4));}
    return i;
}
void remove(std::map<u32,ITEM_INFO>& list,u32 index,u32 amount){auto it=list.find(index);if(it==list.end())return;if(amount>=u32(it->second.m_num))list.erase(it);else it->second.m_num-=int(amount);}
Window* window(const std::string& name){if(name=="cart")return &state.cart;if(name=="vending")return &state.vending;if(name=="trade")return &state.trade;if(name=="shop")return &state.shop;if(name=="mail")return &state.mail;if(name=="equipment")return &state.equipment;if(name=="mailInbox")return &state.mailInbox;return nullptr;}
}
void Reset(){state=State{};state.vending.x=800;state.trade.x=360;state.trade.width=380;state.trade.height=170;state.shop.x=420;state.mail.x=480;state.equipment.x=480;}
void ResetForMapEntry()
{
    // Server snapshots omit empty categories. Ref Initialize clears item and
    // merchant/transaction lists before the load-end acknowledgement.
    g_session.ClearInventoryItems();
    g_session.CloseStorage();
    g_windowMgr.CloseStorageWindows();
    const Window previous[] = {state.cart, state.vending, state.trade,
        state.shop, state.mail, state.equipment, state.mailInbox};
    // A same-connection warp can be followed by the outstanding unkeyed mail result.
    const bool mailPending = state.mailPending && CRagConnection::instance()->IsOpen();
    MailInbox inbox;
    MailDraftAttachment attachment;
    std::string mailRecipient;
    if (CRagConnection::instance()->IsOpen()) {
        inbox = std::move(state.inbox);
        attachment = std::move(state.mailAttachment);
        if (attachment.pending || attachment.staged) {
            mailRecipient = std::move(state.mailTo);
            attachment.abandoned = attachment.pending.has_value();
            attachment.needsReset = attachment.staged.has_value();
        }
    }
    const int inventoryTab = g_windowMgr.m_itemWnd
        ? g_windowMgr.m_itemWnd->GetCurrentTab() : g_windowMgr.GetInventoryTabSelection();
    Reset();
    Window* windows[] = {&state.cart, &state.vending, &state.trade,
        &state.shop, &state.mail, &state.equipment, &state.mailInbox};
    for (size_t i = 0; i < std::size(windows); ++i) {
        windows[i]->x = previous[i].x;
        windows[i]->y = previous[i].y;
    }
    state.inventoryTab = inventoryTab;
    state.mailPending = mailPending;
    state.inbox = std::move(inbox);
    state.mailAttachment = std::move(attachment);
    state.mailTo = std::move(mailRecipient);
}
void CloseTransactions(){closeShopQuote();state.cart.visible=state.vending.visible=state.trade.visible=state.mail.visible=state.equipment.visible=state.mailInbox.visible=false;state.promptKind.clear();state.promptName.clear();state.promptId=state.promptChar=0;tradeReset();state.draft.clear();state.vendingActive=state.vendingPending=state.vendingOpenCanceled=false;state.shopBoards.clear();state.cartItems.clear();}
void UpdateTradeContext(u32 now,std::map<u32,TradePeer> peers){
    state.clock=now;state.tradePeers=std::move(peers);
    if(tradeCancelPending() || (!state.tradeWaiting&&!state.tradeIncoming))return;
    if(!state.partnerId)for(const auto& peer:state.tradePeers)if(peer.second.name==state.partner){state.partnerId=peer.first;break;}
    auto peer=state.tradePeers.find(state.partnerId);
    const bool expired=now-state.tradeRequestTick>=30000u;
    if(!state.partnerId&&!expired)return; // Old invitations carry a name without an AID; wait for identity resolution.
    if(peer!=state.tradePeers.end()&&peer->second.distance<=2&&!expired)return;
    const bool tooDistant=peer!=state.tradePeers.end()&&peer->second.distance>2;
    requestTradeCancellation(tooDistant?"The other player is too far away. Canceling trade."
        :expired?"Trade request expired. Canceling trade.":"The other player is no longer nearby. Canceling trade.");
}
bool HasInput(){return !state.shopBoards.empty()||state.cart.visible||state.vending.visible||state.trade.visible||state.shop.visible||state.mail.visible||state.equipment.visible||state.mailInbox.visible||!state.promptKind.empty()||(g_windowMgr.m_itemWnd && g_windowMgr.m_itemWnd->m_show)||(g_windowMgr.m_playerContextMenuWnd&&g_windowMgr.m_playerContextMenuWnd->m_show);}
bool HitTest(int x,int y){for(const auto* w:{&state.cart,&state.vending,&state.trade,&state.shop,&state.mail,&state.equipment,&state.mailInbox})if(w->visible && x>=w->x && y>=w->y && x<w->x+w->width && y<w->y+w->height)return true;return !state.promptKind.empty();}

bool Receive(CGameMode&,const PacketView& v){
    if(!v.data||v.packetLength<2)return false;const auto* p=v.data;const int n=v.packetLength;const auto op=v.packetId;
    int entry=0;bool equip=false;
    switch(op){case 0x123:entry=10;break;case 0x1ef:entry=18;break;case 0x2e9:entry=22;break;case 0x122:entry=20;equip=true;break;case 0x297:entry=24;equip=true;break;case 0x2d2:entry=26;equip=true;break;}
    if (entry) {
        std::map<u32, ITEM_INFO> next;
        using namespace item_snapshot;
        if (!Stage<kFirstInventoryIndex, kInventorySlots>(v, entry, equip, [&](const u8* row) {
                auto decoded = item(row, equip, entry);
                next.emplace(decoded.m_itemIndex, std::move(decoded));
                return true;
            })) return false;

        for (const auto& [index, existing] : state.cartItems) {
            if (IsEquipmentType(existing.m_itemType) != equip) next.try_emplace(index, existing);
        }
        state.cartItems.swap(next);
        state.hasCart = true;
        return true;
    }
    switch(op){
    case 0x110:if(n<10)return false;if(state.vendingPending && read(p+2)==41 && p[8]==0){state.vendingPending=false;state.status="Vending was rejected by the server. Check the skill, cart and selected items.";}return true;
    case 0x121:if(n<14)return false;state.hasCart=true;state.cartCount=read(p+2);state.cartCapacity=read(p+4);state.cartWeight=read(p+6,4);state.cartMaxWeight=read(p+10,4);return true;
    // ZC_CARTOFF clears the contents/window, not cart entitlement or status counters.
    case 0x12b:state.cartItems.clear();state.cart.visible=state.vending.visible=false;state.draft.clear();state.vendingActive=state.vendingPending=false;return true;
    case 0x124:case 0x1c5:{
        const bool typed=op==0x1c5;const int shift=typed?1:0;
        if(n<21+shift)return false;
        ITEM_INFO i;i.m_itemIndex=read(p+2);i.m_num=int(read(p+4,4));
        i.SetItemId(read(p+8));i.m_itemType=typed?p[10]:0;
        i.m_isIdentified=p[10+shift];i.m_isDamaged=p[11+shift];i.m_refiningLevel=p[12+shift];
        for(int k=0;k<4;++k)i.m_slot[k]=read(p+13+shift+k*2);
        auto old=state.cartItems.find(i.m_itemIndex);
        // Primary CSession::AddMerchantItem changes only quantity for an existing index.
        // In particular, this delta has no allowed-location, expiry or bind fields.
        if(old!=state.cartItems.end())old->second.m_num+=i.m_num;
        else state.cartItems[i.m_itemIndex]=i;
        state.hasCart=true;return true;
    }
    case 0x125:if(n<8)return false;remove(state.cartItems,read(p+2),read(p+4,4));return true;
    case 0x12c:if(n<3||p[2]>1)return false;note(p[2]==0?"Cart transfer failed: cart weight limit.":"Cart transfer failed: cart item capacity limit.");return true;
    case 0x12d:if(n!=4)return false;state.vendingSlots=std::min<u32>(12,read(p+2));state.vending.visible=state.cart.visible=true;state.draft.clear();state.vendingActive=state.vendingPending=state.vendingOpenCanceled=false;state.status="Drag cart items here, then set quantities and prices.";return true;
    case 0x136:case 0x133:{
        constexpr int headerSize = 8;
        constexpr int rowSize = 22;
        constexpr int maxOffers = 12;
        if(n<headerSize || read(p+2)!=n || (n-headerSize)%rowSize || (n-headerSize)/rowSize>maxOffers)return false;
        const u32 owner = read(p+4,4);
        if(!owner)return false;
        const bool ownShop = op==0x136;
        if(!ownShop && owner!=state.requestedSellerId)return true;
        std::array<bool,item_snapshot::kInventorySlots> seen{};
        std::vector<Offer> offers;
        offers.reserve((n-headerSize)/rowSize);
        for(int at=headerSize;at<n;at+=rowSize){
            const auto* row = p+at;
            const auto index = read(row+(ownShop?4:6));
            const auto amount = read(row+(ownShop?6:4));
            const auto id = read(row+9);
            const unsigned slot = index-item_snapshot::kFirstInventoryIndex;
            if(slot>=seen.size() || seen[slot] || !amount || amount>item_snapshot::kMaxStackAmount || !id)return false;
            seen[slot]=true;
            Offer offer;
            offer.price=read(row,4);
            offer.amount=amount;
            offer.item.m_itemIndex=index;
            offer.item.m_itemType=row[8];
            offer.item.SetItemId(id);
            offer.item.m_isIdentified=row[11];
            offer.item.m_isDamaged=row[12];
            offer.item.m_refiningLevel=row[13];
            offer.item.m_num=static_cast<int>(amount);
            for(int k=0;k<4;++k)offer.item.m_slot[k]=read(row+14+k*2);
            offers.push_back(std::move(offer));
        }
        if(ownShop){
            if(state.vendingOpenCanceled)return true;
            state.draft=std::move(offers);
            state.vendingActive=true;
            state.vendingPending=false;
            state.vending.visible=true;
            state.status="Shop is open.";
        }else{
            state.requestedSellerId=0;
            state.shopItems=std::move(offers);
            state.sellerId=owner;
            state.shop.visible=true;
            state.status="Select an item and quantity to purchase.";
        }
        return true;
    }
    case 0x137:{
        if(n!=6)return false;
        if(!state.vendingActive)return true;
        const auto index=read(p+2), amount=read(p+4);
        const auto sold=std::find_if(state.draft.begin(),state.draft.end(),[&](const auto& row){
            return row.item.m_itemIndex==index;
        });
        if(sold==state.draft.end() || !amount || amount>sold->amount)return true;
        note("Sold " + std::to_string(amount) + " × " + sold->item.GetDisplayName() + ".");
        sold->amount-=amount;
        sold->item.m_num=static_cast<int>(sold->amount);
        if(!sold->amount)state.draft.erase(sold);
        if(state.draft.empty()){
            note("Your shop is sold out.");
            if(simple(0x12e)){
                state.vendingActive=false;
                state.vending.visible=false;
            }
        }
        return true;
    }
    case 0x135:{
        if(n!=7)return false;
        // Selected rAthena sends failures here and updates inventory directly on success.
        // The result contains no seller identity and cannot revise the current quote.
        const char* message=nullptr;
        switch(p[6]){
        case 1:message="Purchase failed: not enough Zeny.";break;
        case 2:message="Purchase failed: the items are too heavy.";break;
        case 4:message="Purchase failed: the shop has insufficient stock.";break;
        case 5:message="Purchase failed: another trade is in progress.";break;
        case 6:case 7:message="Purchase failed: the shop listing is no longer available.";break;
        }
        if(message)g_windowMgr.PushChatEvent(message,0x006464FFu,6);
        return true;
    }
    case 0x138:return n==3; // Framed for compatibility. The selected server uses 0136 or skill failure.
    case 0x131:{
        if(n!=86)return false;
        const auto seller=read(p+2,4);
        if(!seller)return false;
        state.shopBoards[seller]=text(p+6,80);
        return true;
    }
    case 0x132:{
        if(n!=6)return false;
        const auto seller=read(p+2,4);
        if(!seller)return false;
        state.shopBoards.erase(seller);
        if(seller==state.sellerId || seller==state.requestedSellerId)closeShopQuote();
        if(seller==g_session.m_aid){state.vendingActive=false;state.vendingPending=false;}
        return true;
    }
    case 0xe5:case 0x1f4:if(n<(op==0xe5?26:32))return false;if(tradeCancelPending())return true;tradeReset();state.partnerId=0;state.tradeRequestTick=state.clock;state.partner=text(p+2,24);if(op==0x1f4)state.partnerId=read(p+26,4);state.tradeIncoming=true;state.trade.visible=true;state.status="Trade invitation. Review the player name before accepting.";return true;
    case 0xe7:case 0x1f5:{
        if(n!=(op==0xe7?3:9))return false;
        const auto result = p[2];
        if(state.tradeCancellation==TradeCancellation::Cancel){
            if(result==3){state.tradeActive=true;state.tradeWaiting=false;}
            return true;
        }
        if(state.tradeCancellation==TradeCancellation::Decline && result==3)return true;
        tradeReset();
        state.trade.visible = result==3;
        if(result==3){
            state.tradeActive=true;
            state.trade.width=660;
            state.trade.height=420;
            if(op==0x1f5)state.partnerId=read(p+3,4);
            state.status="Drag inventory items into your offer. Lock, then confirm after both players lock.";
        }else{
            const char* reasons[]={"Trade failed: the other player is too far away.","Trade failed: the other player is unavailable.","Trade request failed.","", "Trade request declined or canceled.","Trade failed: the other player is busy."};
            note(result<6?reasons[result]:"Trade request failed.");
        }
        return true;
    }
    case 0xe9:{
        if(n!=19 || !state.tradeActive)return false;
        const u32 amount=read(p+2,4), id=read(p+6);
        if(amount>static_cast<u32>(std::numeric_limits<int>::max()))return false;
        if(!id){
            state.remoteZeny=amount;
            return true;
        }
        if(!amount)return false;
        // Ref gives each incoming item its own display slot. The frame has no inventory index.
        if(state.remoteOffer.size()>=10)return true;
        Offer offer;
        offer.amount=amount;
        offer.item.SetItemId(id);
        offer.item.m_num=static_cast<int>(amount);
        offer.item.m_isIdentified=p[8];
        offer.item.m_isDamaged=p[9];
        offer.item.m_refiningLevel=p[10];
        for(int k=0;k<4;++k)offer.item.m_slot[k]=read(p+11+2*k);
        state.remoteOffer.push_back(std::move(offer));
        return true;
    }
    case 0xea:{
        if(n!=5 || p[4]>2)return false;
        if(!state.tradeActive || !state.tradeAddPending || read(p+2)!=state.pendingTrade.item.m_itemIndex)return true;
        if(p[4]==0){
            auto old=std::find_if(state.localOffer.begin(),state.localOffer.end(),[&](const auto& offer){
                return offer.item.m_itemIndex==state.pendingTrade.item.m_itemIndex;
            });
            if(old==state.localOffer.end())state.localOffer.push_back(state.pendingTrade);
            else old->amount+=state.pendingTrade.amount;
        }else state.status="Server rejected the offered item or amount.";
        state.tradeAddPending=false;
        state.pendingTrade={};
        return true;
    }
    case 0xec:
        if(n!=3 || p[2]>1)return false;
        if(!state.tradeActive)return false;
        if(p[2]==0){state.localLocked=true;state.lockPending=false;}
        else state.remoteLocked=true;
        return true;
    case 0xee:{
        if(n!=2)return false;
        const bool hadTrade = state.tradeActive || state.tradeWaiting || state.tradeIncoming;
        if(!hadTrade && !tradeCancelPending())return true;
        tradeReset();
        state.trade.visible=false;
        if(hadTrade)note("Trade canceled. No exchange was completed.");
        return true;
    }
    case 0xf0:
        if(n!=3 || p[2]>1)return false;
        if(!state.tradeActive || !state.commitPending)return true;
        if(state.tradeCancellation==TradeCancellation::Cancel){
            state.tradeActive=false;
            state.commitPending=false;
        }else{
            tradeReset();
            state.trade.visible=false;
        }
        note(p[2]?"Trade failed.":"Trade completed.");
        return true;
    case 0xf1:state.localOffer.clear();state.localZeny=0;state.localLocked=state.lockPending=false;return true;
    case 0x16a:if(n<30)return false;state.promptKind="guild";state.promptId=read(p+2,4);state.promptName=text(p+6,24);return true;
    case 0x207: {
        if (n != 34) return false;
        const u32 aid = read(p + 2, 4), cid = read(p + 6, 4);
        const auto name = text(p + 10, 24);
        if (!aid || !cid || name.empty()) return false;
        state.promptKind = "friend";
        state.promptId = aid;
        state.promptChar = cid;
        state.promptName = name;
        return true;
    }
    case 0x169:if(n<3)return false;note(p[2]==2?"Guild invitation accepted.":p[2]==3?"Guild invitation failed: guild is full.":p[2]==0?"Player already belongs to a guild.":"Guild invitation declined.");return true;
    case 0x2d7:{if(n<43||(n-43)%26)return false;std::vector<ITEM_INFO> items;for(int at=43;at<n;at+=26)items.push_back(item(p+at,true,26));state.viewedEquipment=std::move(items);state.equipment.visible=true;state.partner=text(p+4,24);state.status="Equipment shared by this player.";return true;}
    case 0x240:
        return receiveMailList(p, n);
    case 0x242:
        return receiveMailDetail(p, n);
    case 0x245:
        if (n != 3 || p[2] > 2) return false;
        receiveMailClaimResult(p[2]);
        return true;
    case 0x24a:
        if (n != 70) return false;
        if (read(p + 2, 4) && (!state.inbox.notification || state.inbox.notification->id != read(p + 2, 4))) {
            g_windowMgr.PushChatEvent("New mail has arrived.", 0x0080C0FFu, 6);
        }
        state.inbox.notification = MailNotification{read(p + 2, 4), mailText(p + 6, 40), mailText(p + 46, 24)};
        return true;
    case 0x255: {
        if (n != 5) return false;
        auto& attachment = state.mailAttachment;
        const u32 slot = read(p + 2);
        if (!attachment.pending || attachment.pending->item.m_itemIndex != slot) return true;
        if (p[4] == 0) {
            if (attachment.pending->amount == 0) {
                attachment.staged.reset();
                attachment.needsReset = false;
                state.status = "Attachment removed.";
            } else {
                attachment.staged = std::move(attachment.pending);
                attachment.needsReset = attachment.abandoned;
                state.status = "Item attached.";
            }
        } else {
            state.status = "The server refused the attachment.";
        }
        attachment.pending.reset();
        attachment.abandoned = false;
        return true;
    }
    case 0x260: {
        if (n != 6) return false;
        const u32 type = read(p + 2, 4);
        if (type > 1) return false;
        state.mailInbox.visible = type == 0;
        if (state.mailInbox.visible && !busy() && !state.mail.visible
            && !state.mailPending && !state.inbox.pendingReadId
            && !state.mailAttachment.pending && !state.mailAttachment.staged) {
            simple(0x23f);
        }
        return true;
    }
    case 0x257:
        if (n != 8) return false;
        receiveMailRemoval(MailRemovalKind::Delete, read(p + 2, 4), read(p + 6));
        return true;
    case 0x274:
        if (n != 8) return false;
        if (receiveMailRemoval(MailRemovalKind::Return, read(p + 2, 4), read(p + 6))) return true;
        if (state.inbox.pendingReadId && state.inbox.pendingReadId == read(p + 2, 4) && read(p + 6)) {
            state.inbox.details.erase(state.inbox.pendingReadId);
            if (auto& claim = state.inbox.claim; claim && claim->id == state.inbox.pendingReadId) {
                claim->terminalReadRequested = false;
            }
            state.inbox.pendingReadId = 0;
            state.status = "This message is no longer available. Refresh the inbox.";
        }
        return true;
    case 0x249:
        if (n < 3) return false;
        if (!state.mailPending) return true;
        state.mailPending = false;
        if (p[2] && state.mailAttachment.staged) state.mailAttachment.needsReset = true;
        else {
            state.mailAttachment.staged.reset();
            state.mailAttachment.needsReset = false;
        }
        note(p[2] ? "Mail could not be sent." : "Mail sent.");
        if (!p[2]) state.mail.visible = false;
        return true;
    default:return false;
    }
}

bool Action(const Command& c){
    state.status.clear();
    if(c.action=="inventoryMini" && g_windowMgr.m_itemWnd){
        return g_windowMgr.m_itemWnd->SendMsg(nullptr,6,136,0,0)!=0;
    }
    if(c.action=="resizeInventory" && g_windowMgr.m_itemWnd){
        g_windowMgr.m_itemWnd->ResizeInventory(c.x,c.y);
        if(c.amount)g_windowMgr.m_itemWnd->StoreInfo();
        return true;
    }
    if(c.action=="moveWindow"){
        if(auto* w=window(c.to)){w->x=std::max(0,c.x);w->y=std::max(0,c.y);return true;}
        if(c.to=="inventory" && g_windowMgr.m_itemWnd){
            g_windowMgr.m_itemWnd->Move(std::max(0,c.x),std::max(0,c.y));
            if(c.amount)g_windowMgr.m_itemWnd->StoreInfo();
            return true;
        }
        return false;
    }
    if(c.action=="inventoryTab"){
        if(g_windowMgr.m_itemWnd)g_windowMgr.m_itemWnd->SetCurrentTab(c.tab);
        else {
            g_windowMgr.SetInventoryTabSelection(c.tab);
            state.inventoryTab=g_windowMgr.GetInventoryTabSelection();
        }
        return true;
    }
    if(c.action=="cart"){if(state.hasCart){state.cart.visible=!state.cart.visible;return true;}return false;}
    if(c.action=="close"){
        if(c.to=="inventory" && g_windowMgr.m_itemWnd){g_windowMgr.CloseInventoryWindow();return true;}
        if(c.to=="trade")return requestTradeCancellation("Waiting for the server to cancel trade.");
        if(c.to=="shop"){closeShopQuote();return true;}
        if (c.to == "mail") {
            if (state.mailPending || (state.mailAttachment.pending && !MailStageTimedOut(GetTickCount()))) {
                state.status = "Waiting for the mail server.";
                return false;
            }
            if (!state.mail.visible) return false;
            auto& attachment = state.mailAttachment;
            if (attachment.pending) attachment.abandoned = true;
            else if (attachment.staged) attachment.needsReset = true;
            else if (!resetMailDraft()) return false;
        }
        if(c.to=="vending"){
            if(state.vendingActive){
                if(!simple(0x12e))return false;
                state.vendingOpenCanceled=true;
            }else{
                if(!state.vending.visible)return false;
                auto frames=packet(0x1b2,85);
                put(frames,2,85);
                if(state.vendingPending){
                    // The server processes close after open on the same stream. Clear setup too if open failed silently.
                    frames.insert(frames.begin(),{0x2e,0x01});
                }
                if(!send(frames))return false;
                state.vendingOpenCanceled=state.vendingPending;
            }
            state.vendingActive=state.vendingPending=false;
            state.draft.clear();
        }
        if(auto* w=window(c.to)){w->visible=false;return true;}return false;
    }
    if(c.action=="use"){if(busy())return false;const auto* i=g_session.GetInventoryItemByIndex(c.index);if(!i)return false;return g_modeMgr.SendMsg(i->m_wearLocation?CGameMode::GameMsg_RequestUnequipInventoryItem:i->m_location?CGameMode::GameMsg_RequestEquipInventoryItem:CGameMode::GameMsg_RequestUseInventoryItem,c.index,i->m_location,0)!=0;}
    if(c.action=="transfer"){
        if (!state.hasCart || busy() || !c.index || c.index > 65535 || !c.amount || c.amount > 32767) return false;
        const bool usesStorage = c.from == "storage" || c.to == "storage";
        if (usesStorage && !g_session.IsStorageOpen()) return false;
        const ITEM_INFO* item = nullptr;
        u16 opcode = 0;
        if (c.from == "inventory" && c.to == "cart") {
            item = g_session.GetInventoryItemByIndex(c.index);
            opcode = 0x0126;
        } else if (c.from == "storage" && c.to == "cart") {
            item = g_session.GetStorageItemByIndex(c.index);
            opcode = 0x0128;
        } else if (c.from == "cart" && (c.to == "inventory" || c.to == "storage")) {
            const auto found = state.cartItems.find(c.index);
            if (found != state.cartItems.end()) item = &found->second;
            if (std::any_of(state.draft.begin(), state.draft.end(),
                    [&](const Offer& offer) { return offer.item.m_itemIndex == c.index; })) {
                state.status = "Remove this item from the vending list first.";
                return false;
            }
            opcode = c.to == "storage" ? 0x0129 : 0x0127;
        }
        if (!item || item->m_wearLocation || item->m_num <= 0 || c.amount > u32(item->m_num)
            || (c.itemId && c.itemId != item->GetItemId())) return false;
        auto request = packet(opcode, 8);
        put(request, 2, c.index);
        put(request, 4, c.amount, 4);
        return send(request);
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
        if(!state.vending.visible||busy()||state.draft.empty()||state.draft.size()>state.vendingSlots||c.text.empty()||c.text.size()>79)return false;
        auto p=packet(0x1b2,85+int(state.draft.size())*8);put(p,2,u32(p.size()));std::copy(c.text.begin(),c.text.end(),p.begin()+4);p[84]=1;
        for(size_t k=0;k<state.draft.size();++k){const auto& o=state.draft[k];const auto i=state.cartItems.find(o.item.m_itemIndex);if(i==state.cartItems.end()||!o.amount||o.amount>u32(i->second.m_num)||!o.price)return false;put(p,85+int(k)*8,o.item.m_itemIndex);put(p,87+int(k)*8,o.amount);put(p,89+int(k)*8,o.price,4);}
        if(!send(p))return false;state.vendingPending=true;state.shopTitle=c.text;state.status="Opening shop; waiting for server confirmation.";return true;
    }
    if(c.action=="tradeReply"){
        if(!state.tradeIncoming || tradeCancelPending()) return false;
        if(!c.amount) return requestTradeCancellation("Waiting for the server to decline trade.");
        const auto peer = state.tradePeers.find(state.partnerId);
        if(peer==state.tradePeers.end() || peer->second.distance>2) {
            requestTradeCancellation("The other player is too far away or unavailable. Declining trade.");
            return false;
        }
        auto reply = packet(0xe6,3);
        reply[2] = 3;
        if(!send(reply)) return false;
        state.tradeIncoming = false;
        state.tradeWaiting = true;
        state.tradeRequestTick = state.clock;
        state.status = "Acceptance sent; waiting for the server.";
        return true;
    }
    if(c.action=="offerTrade"||c.action=="tradeZeny"){
        if(tradeCancelPending()||!state.tradeActive||state.localLocked||state.lockPending||state.tradeAddPending||(!c.amount && c.action!="tradeZeny"))return false;Offer o;
        if(c.action=="tradeZeny"){const auto* player=g_session.GetSelectedCharacterInfo();if(!player||c.amount>u32(std::max(0,player->money)))return false;o.amount=c.amount;}
        else{
            const auto* i=g_session.GetInventoryItemByIndex(c.index);
            if(!i || !i->GetItemId() || i->m_num<=0 || i->m_wearLocation || i->m_deleteTime || c.amount>32767)return false;
            const auto existing=std::find_if(state.localOffer.begin(),state.localOffer.end(),[&](const auto& row){
                return row.item.m_itemIndex==c.index;
            });
            if(existing==state.localOffer.end()){
                if(state.localOffer.size()>=10 || c.amount>u32(i->m_num))return false;
            }else if(existing->item.GetItemId()!=i->GetItemId() || u64(existing->amount)+c.amount>u32(i->m_num))return false;
            o={*i,c.amount,0};
        }
        auto p=packet(0xe8,8);put(p,2,o.item.m_itemIndex);put(p,4,c.amount,4);if(!send(p))return false;
        // rAthena does not echo a Zeny-add acknowledgement to the sender.
        // This is the editable offer, not the character's actual balance.
        if(c.action=="tradeZeny"){state.localZeny=c.amount;return true;}
        state.pendingTrade=o;state.tradeAddPending=true;return true;
    }
    if(c.action=="lockTrade"){if(tradeCancelPending()||!state.tradeActive||state.localLocked||state.lockPending||state.tradeAddPending)return false;if(!simple(0xeb))return false;state.lockPending=true;return true;}
    if(c.action=="commitTrade"){if(tradeCancelPending()||!state.tradeActive||!state.localLocked||!state.remoteLocked||state.commitPending)return false;if(!simple(0xef))return false;state.commitPending=true;state.status="Final confirmation sent; waiting for the server.";return true;}
    if(c.action=="buy"){
        if(!state.shop.visible || !state.sellerId || busy())return false;
        const auto found=std::find_if(state.shopItems.begin(),state.shopItems.end(),[&](const auto& row){
            return row.item.m_itemIndex==c.index;
        });
        if(found==state.shopItems.end() || !c.amount || c.amount>found->amount)return false;
        auto p=packet(0x134,12);
        put(p,2,12);put(p,4,state.sellerId,4);put(p,8,c.amount);put(p,10,c.index);
        if(!send(p))return false;
        closeShopQuote();
        return true;
    }
    if(c.action=="prompt"){
        if (c.from == "friend" || state.promptKind == "friend") {
            if (c.from != state.promptKind || !c.promptAid || !c.promptCid
                || c.promptAid != state.promptId || c.promptCid != state.promptChar
                || !SendFriendInvitationReply(c.promptAid, c.promptCid, c.amount != 0)) return false;
            state.promptKind.clear();
            state.promptName.clear();
            state.promptId = state.promptChar = 0;
            return true;
        }
        std::vector<u8> p;if(state.promptKind=="guild"){p=packet(0x16b,10);put(p,2,state.promptId,4);put(p,6,c.amount?1:0,4);}else if(state.promptKind=="alliance"){p=packet(0x172,10);put(p,2,state.promptId,4);put(p,6,c.amount?1:0,4);}else return false;
        if(!send(p))return false;state.promptKind.clear();return true;
    }
    if (c.action == "refreshMail" || c.action == "readMail" || c.action == "claimMail"
        || c.action == "checkMailClaim" || c.action == "deleteMail" || c.action == "returnMail") {
        auto& inbox = state.inbox;
        // Either request can reset the server's staged compose resources.
        if (!state.mailInbox.visible || busy() || state.mail.visible || state.mailPending
            || state.mailAttachment.pending || inbox.pendingReadId) return false;
        if (state.mailAttachment.staged) {
            state.status = "Reopen the mail composer to reset its attachment.";
            return false;
        }
        if (c.action == "refreshMail") return simple(0x23f);
        if (inbox.removal) return false;
        if (c.action == "checkMailClaim") {
            return MailClaimCanCheck(GetTickCount()) && requestMailRead(inbox.claim->id);
        }
        if (!inbox.headers.count(c.target) || c.target > static_cast<u32>((std::numeric_limits<int>::max)())) {
            return false;
        }
        if (c.action == "deleteMail" || c.action == "returnMail") {
            if (inbox.claim || inbox.selectedId != c.target) return false;
            const auto detail = inbox.details.find(c.target);
            if (detail == inbox.details.end()) return false;
            const bool returning = c.action == "returnMail";
            const auto& message = detail->second;
            if (returning ? message.senderWire.front() == 0 : message.itemAmount != 0 || message.zeny != 0) {
                return false;
            }
            auto p = packet(returning ? 0x273 : 0x243, returning ? 30 : 6);
            put(p, 2, c.target, 4);
            if (returning) std::copy(message.senderWire.begin(), message.senderWire.end(), p.begin() + 6);
            if (!send(p)) return false;
            inbox.removal = MailRemoval{returning ? MailRemovalKind::Return : MailRemovalKind::Delete,
                c.target, GetTickCount()};
            return true;
        }
        if (c.action == "claimMail") {
            if (inbox.claim || inbox.selectedId != c.target) return false;
            const auto detail = inbox.details.find(c.target);
            if (detail == inbox.details.end()) return false;
            const auto& attachment = detail->second;
            const unsigned components = (attachment.itemAmount != 0 && attachment.item.GetItemId() != 0)
                + (attachment.zeny != 0);
            if (!components) return false;
            auto p = packet(0x244, 6);
            put(p, 2, c.target, 4);
            if (!send(p)) return false;
            inbox.claim = MailClaim{c.target, GetTickCount(), components};
            return true;
        }
        if (!requestMailRead(c.target)) return false;
        inbox.selectedId = c.target;
        return true;
    }
    if (c.action == "stageMailItem" || c.action == "clearMailItem") {
        auto& attachment = state.mailAttachment;
        if (!state.mail.visible || state.mailPending || busy()) return false;
        if (attachment.pending) return false;
        const bool clearing = c.action == "clearMailItem";
        if (clearing != attachment.staged.has_value()) return false;
        const u32 index = clearing ? attachment.staged->item.m_itemIndex : c.index;
        const u32 itemId = clearing ? attachment.staged->item.GetItemId() : c.itemId;
        const u32 amount = clearing ? 0 : c.amount;
        // The selected pre-RODEX parser checks the inventory index before the
        // index-zero Zeny branch. It silently rejects that branch.
        if (index < item_snapshot::kFirstInventoryIndex
            || index >= item_snapshot::kFirstInventoryIndex + item_snapshot::kInventorySlots
            || (!clearing && !amount) || !itemId) return false;
        const auto* item = g_session.GetInventoryItemByIndex(index);
        if (!item || item->GetItemId() != itemId || item->m_wearLocation
            || item->m_num <= 0 || amount > static_cast<u32>(item->m_num)) {
            state.status = "The attachment slot changed. Reconnect to clear the server draft.";
            return false;
        }
        auto p = packet(0x247, 8);
        put(p, 2, index);
        put(p, 4, amount, 4);
        if (!send(p)) return false;
        attachment.pending = Offer{*item, amount};
        attachment.sentAt = GetTickCount();
        return true;
    }
    if (c.action == "mail") {
        if (state.mailPending) {
            state.status = "Sending mail; waiting for the server.";
            return false;
        }
        if (!state.mail.visible || busy() || state.mailAttachment.pending) return false;
        if (state.mailAttachment.needsReset) {
            state.status = "Remove the previous attachment before resending.";
            return false;
        }
        if (const auto& staged = state.mailAttachment.staged) {
            const auto* item = g_session.GetInventoryItemByIndex(staged->item.m_itemIndex);
            if (!item || item->GetItemId() != staged->item.GetItemId() || item->m_wearLocation
                || item->m_num <= 0 || staged->amount > static_cast<u32>(item->m_num)) {
                state.status = "The attached item changed. Remove it before sending.";
                return false;
            }
        }
        if (c.text.empty() || c.text.size() > 23 || c.subject.empty() || c.subject.size() > 39
            || c.body.size() > 199 || c.text.find('\0') != std::string::npos
            || c.subject.find('\0') != std::string::npos || c.body.find('\0') != std::string::npos) {
            state.status = "Use 1–23 bytes for the recipient, 1–39 for the subject, and up to 199 for the message.";
            return false;
        }
        auto p = packet(0x248, 70 + static_cast<int>(c.body.size()));
        put(p, 2, static_cast<u32>(p.size()));
        std::copy(c.text.begin(), c.text.end(), p.begin() + 4);
        std::copy(c.subject.begin(), c.subject.end(), p.begin() + 28);
        p[68] = static_cast<u8>(c.body.size() + 1);
        std::copy(c.body.begin(), c.body.end(), p.begin() + 69);
        if (!send(p)) {
            state.status = "Mail could not be submitted.";
            return false;
        }
        state.mailPending = true;
        state.status = "Sending mail; waiting for the server.";
        return true;
    }
    state.status="That action is unavailable in the current state.";return false;
}

bool MailClaimCanCheck(u32 now)
{
    const auto& claim = state.inbox.claim;
    return claim && (claim->needsReadback || now - claim->sentAt >= kMailClaimWaitMs);
}

bool MailRemovalTimedOut(u32 now)
{
    const auto& removal = state.inbox.removal;
    return removal && now - removal->sentAt >= kMailRemovalWaitMs;
}

bool MailStageTimedOut(u32 now)
{
    const auto& attachment = state.mailAttachment;
    return attachment.pending && now - attachment.sentAt >= kMailStageWaitMs;
}

bool SendFriendInvitationReply(u32 aid, u32 cid, bool accept)
{
    if (!aid || !cid) return false;
    auto reply = packet(0x208, 14);
    put(reply, 2, aid, 4);
    put(reply, 6, cid, 4);
    put(reply, 10, accept, 4);
    return send(reply);
}

bool PlayerAction(int action,u32 target,const std::string& name){
    state.status.clear();if(!target||target==g_session.m_aid||name.empty())return false;
    switch(action){
    case CGameMode::PlayerContext_RequestDeal:{if(busy())return false;auto peer=state.tradePeers.find(target);if(peer==state.tradePeers.end()||peer->second.distance>2){note(peer==state.tradePeers.end()?"Trade failed: the other player is no longer nearby.":"Trade failed: the other player is too far away.");return false;}state.tradeRequestTick=state.clock;auto p=packet(0xe4,6);put(p,2,target,4);if(!send(p))return false;state.partner=name;state.partnerId=target;state.tradeWaiting=true;state.trade.visible=true;state.trade.width=380;state.trade.height=170;state.status="Waiting for the other player to accept.";return true;}
    case CGameMode::PlayerContext_InviteJoinGuild:{if(!g_session.m_localGuild.guildId||(!g_session.m_localGuild.isMaster && !(g_session.m_localGuild.permissions&1)))return false;auto p=packet(0x168,14);put(p,2,target,4);put(p,6,g_session.m_aid,4);put(p,10,g_session.m_gid,4);return send(p);}
    case CGameMode::PlayerContext_RegisterFriend:return named(0x202,name);
    case CGameMode::PlayerContext_CheckEquipmentInfo:{auto p=packet(0x2d6,6);put(p,2,target,4);return send(p);}
    case CGameMode::PlayerContext_SendWhisper:if(g_windowMgr.m_chatWnd){g_windowMgr.m_chatWnd->SetShow(1);g_windowMgr.m_chatWnd->SetQtInputText(1,name);g_windowMgr.m_chatWnd->FocusQtInput(2);return true;}return false;
    case CGameMode::PlayerContext_RejectWhispering:
    case CGameMode::PlayerContext_AllowWhispering:{
        if(name.size()>23)return false;const bool allow=action==CGameMode::PlayerContext_AllowWhispering;
        auto p=packet(0xcf,27);std::copy(name.begin(),name.end(),p.begin()+2);p[26]=allow?1:0;
        const bool sent=send(p);
        // Original context actions update local exclusion independently of ack
        // or immediate send success; add appends, allow erases the first match.
        if(!allow){g_session.m_exNameList.push_back(name);g_session.m_exAidList.push_back(target);}
        else {const auto found=std::find(g_session.m_exNameList.begin(),g_session.m_exNameList.end(),name);
            if(found!=g_session.m_exNameList.end())g_session.m_exNameList.erase(found);
            const auto aid=std::find(g_session.m_exAidList.begin(),g_session.m_exAidList.end(),target);
            if(aid!=g_session.m_exAidList.end())g_session.m_exAidList.erase(aid);}
        return sent;
    }
    case CGameMode::PlayerContext_SendMail:
        if (state.inbox.pendingReadId) return false;
        if (state.mailPending) {
            state.status = "Sending mail; waiting for the server.";
            return false;
        }
        if (busy()) return false;
        if (state.mailAttachment.pending || state.mailAttachment.staged) {
            if (!state.mail.visible) {
                state.mail.visible = true;
                return true;
            }
            return false;
        }
        if (!resetMailDraft()) return false;
        state.mailTo = name;
        state.mail.visible = true;
        return true;
    case 8:{
        if(!target || !state.shopBoards.count(target) || busy())return false;
        auto p=packet(0x130,6);
        put(p,2,target,4);
        if(!send(p))return false;
        closeShopQuote();
        state.requestedSellerId=target;
        return true;
    }
    default:return false;
    }
}
}
