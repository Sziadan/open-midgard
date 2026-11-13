#include "QtCommerce.h"
#include "QtUiState.h"
#include "gamemode/Commerce.h"
#include "gamemode/GameMode.h"
#include "session/Session.h"
#include "ui/UIWindowMgr.h"
#include "ui/UIItemWnd.h"
#include "ui/UIItemInfoWnd.h"
#include "ui/UIPlayerContextMenuWnd.h"
#include "ui/UiScale.h"
#include "world/World.h"
#include "gamemode/View.h"
#include <QVariantMap>
#include <QColor>
#include <algorithm>
#include "ui/UIShopCommon.h"
#include "item/ItemTab.h"

namespace {
QString str(const std::string& s){return QString::fromUtf8(s.data(),int(s.size()));}
QVariantMap row(const ITEM_INFO& item,u32 count=0xffffffffu,u32 price=0){return {{"index",item.m_itemIndex},{"id",item.GetItemId()},{"name",str(shopui::GetItemDisplayName(item))},{"count",count==0xffffffffu?u32(item.m_num):count},{"price",price},{"identified",item.m_isIdentified!=0},{"equipped",item.m_wearLocation!=0},{"equip",item.m_location!=0},{"description",str(item.GetDescription())},{"hoverText",str(shopui::BuildItemHoverText(item))},{"nameColor",QColor::fromRgba(item.GetNameColorArgb()).name(QColor::HexArgb)}};}
QVariantMap geometry(const ro::commerce::Window& w,bool suppressed){return {{"visible",w.visible&&!suppressed},{"x",w.x},{"y",w.y},{"width",w.width},{"height",w.height}};}
QVariantList offers(const std::vector<ro::commerce::Offer>& list){QVariantList result;for(const auto& o:list)result.push_back(row(o.item,o.amount,o.price));return result;}

void DescribeCommerceItem(const QVariantMap& args)
{
    using namespace ro::commerce;
    const QString from = args.value("from").toString();
    const u32 index = args.value("index").toUInt();
    const ITEM_INFO* item = nullptr;
    ItemInfoSource source = ItemInfoSource::Snapshot;
    const auto findOffer = [index](const std::vector<Offer>& entries) -> const ITEM_INFO* {
        const auto found = std::find_if(entries.begin(), entries.end(), [index](const Offer& offer) {
            return offer.item.m_itemIndex == index && offer.amount != 0;
        });
        return found == entries.end() ? nullptr : &found->item;
    };
    if (from == "inventory") {
        item = g_session.GetInventoryItemByIndex(index);
        source = ItemInfoSource::Inventory;
    } else if (from == "cart") {
        const auto found = state.cartItems.find(index);
        if (found != state.cartItems.end()) item = &found->second;
        source = ItemInfoSource::Cart;
    } else if (from == "vending" && state.vending.visible) {
        item = findOffer(state.draft);
    } else if (from == "shop" && state.shop.visible) {
        item = findOffer(state.shopItems);
    } else if (from == "trade-local" && state.trade.visible && state.tradeActive) {
        item = findOffer(state.localOffer);
    } else if (from == "trade-remote" && state.trade.visible && state.tradeActive) {
        // 00E9 has no inventory index. The rendered row identifies an offer;
        // never resolve its default index zero against another item owner.
        bool valid = false;
        const int ordinal = args.value("row").toInt(&valid);
        if (valid && ordinal >= 0 && size_t(ordinal) < state.remoteOffer.size()
            && state.remoteOffer[size_t(ordinal)].amount != 0)
            item = &state.remoteOffer[size_t(ordinal)].item;
    } else if (from == "equipment" && state.equipment.visible) {
        const auto found = std::find_if(state.viewedEquipment.begin(), state.viewedEquipment.end(),
            [index](const ITEM_INFO& entry) { return entry.m_itemIndex == index; });
        if (found != state.viewedEquipment.end()) item = &*found;
    }
    // A queued UI callback must not describe a replacement row of another ID.
    const u32 expectedId = args.value("id").toUInt();
    if (item && expectedId && expectedId == item->GetItemId())
        g_windowMgr.ShowItemInfoWindow(*item, UiScaleRawToLogicalCoordinate(args.value("x").toInt()),
            UiScaleRawToLogicalCoordinate(args.value("y").toInt()), source);
}
}
void PopulateCommerceUi(QtUiState* ui,CGameMode& gameMode,bool suppressed){
    using namespace ro::commerce;auto& s=state;QVariantMap data;QVariantList inventory,cart,equipment;
    // The native inventory owns the selection; both projections share it.
    s.inventoryTab=g_windowMgr.m_itemWnd?g_windowMgr.m_itemWnd->GetCurrentTab():g_windowMgr.GetInventoryTabSelection();
    int inventoryCount=0;
    for(const auto& i:g_session.GetInventoryItems()){
        // Ref moves worn equipment into separate slots. The shared model keeps
        // every row; the bag mirrors native filtering, including its ammo exception.
        if(i.m_wearLocation!=0 && i.m_itemType!=10)continue;
        ++inventoryCount;
        if(ro::item::ItemTypeBelongsToTab(i.m_itemType,s.inventoryTab))inventory.push_back(row(i));
    }
    for(const auto& i:s.cartItems)cart.push_back(row(i.second));for(const auto& i:s.viewedEquipment)equipment.push_back(row(i));
    QVariantMap inv;if(auto* w=g_windowMgr.m_itemWnd)inv={{"visible",w->m_show!=0&&!suppressed},{"x",w->m_x},{"y",w->m_y},{"width",w->m_w},{"height",w->m_h}};
    if(auto* w=g_windowMgr.m_itemWnd)inv["mini"]=w->IsMiniMode();
    data["inventory"]=inv;data["inventoryItems"]=inventory;data["inventoryCount"]=inventoryCount;data["inventoryTab"]=s.inventoryTab;
    data["cart"]=geometry(s.cart,suppressed);data["cartItems"]=cart;data["hasCart"]=s.hasCart;data["cartCount"]=s.cartCount;data["cartCapacity"]=s.cartCapacity;data["cartWeight"]=s.cartWeight/10;data["cartMaxWeight"]=s.cartMaxWeight/10;
    data["vending"]=geometry(s.vending,suppressed);data["draft"]=offers(s.draft);data["vendingSlots"]=s.vendingSlots;data["vendingActive"]=s.vendingActive;data["vendingPending"]=s.vendingPending;
    data["trade"]=geometry(s.trade,suppressed);data["tradeIncoming"]=s.tradeIncoming;data["tradeActive"]=s.tradeActive;data["tradeWaiting"]=s.tradeWaiting;data["localLocked"]=s.localLocked;data["remoteLocked"]=s.remoteLocked;data["lockPending"]=s.lockPending;data["commitPending"]=s.commitPending;data["addPending"]=s.tradeAddPending;
    data["localOffer"]=offers(s.localOffer);data["remoteOffer"]=offers(s.remoteOffer);data["localZeny"]=s.localZeny;data["remoteZeny"]=s.remoteZeny;data["partner"]=str(s.partner);
    data["shop"]=geometry(s.shop,suppressed);data["shopItems"]=offers(s.shopItems);data["mail"]=geometry(s.mail,suppressed);data["mailTo"]=str(s.mailTo);data["mailPending"]=s.mailPending;data["equipment"]=geometry(s.equipment,suppressed);data["equipmentItems"]=equipment;
    data["promptKind"]=(suppressed || s.promptKind=="friend")?QString():str(s.promptKind);data["promptName"]=str(s.promptName);data["status"]=str(s.status);QVariantMap menu;
    if(auto* w=g_windowMgr.m_playerContextMenuWnd;w && w->m_show && !suppressed){
        menu={{"visible",true},{"x",w->m_x},{"y",w->m_y},{"width",w->m_w},{"height",w->m_h},{"name",str(w->GetTargetName())},{"target",w->GetTargetAid()}};
        const char* labels[]={"View equipment","Request trade","Invite to party","Invite to guild","Whisper","Add friend","Send mail","Block whispers"};QVariantList actions;
        for(int i=0;i<8;++i){bool enabled=true;if(i==3)enabled=g_session.m_localGuild.guildId && (g_session.m_localGuild.isMaster||(g_session.m_localGuild.permissions&1));actions.push_back(QVariantMap{{"id",i},{"label",QString::fromUtf8(labels[i])},{"enabled",enabled}});}
        for (const auto& entry : w->GetEntries()) {
            if (entry.actionId == CGameMode::PlayerContext_RequestAdoption) {
                actions.push_back(QVariantMap{{"id", entry.actionId}, {"label", str(entry.label)}, {"enabled", true}});
            }
        }
        if(s.shopBoards.count(w->GetTargetAid())){actions.prepend(QVariantMap{{"id",8},{"label","Browse shop"},{"enabled",true}});menu["height"]=w->m_h+30;}
        menu["actions"]=actions;
    }
    QVariantList boards;
    if(auto* mode=&gameMode;!suppressed&&mode->m_world&&mode->m_view){
        for(const auto& board:s.shopBoards){
            if(board.first==g_session.m_aid)continue;
            int x=0,y=0;
            if(mode->m_world->GetActorScreenMarker(mode->m_view->GetViewMatrix(),mode->m_view->GetCameraLongitude(),board.first,&x,&y))
                boards.push_back(QVariantMap{{"target",board.first},{"title",str(board.second)},{"x",UiScaleRawToLogicalCoordinate(x)},{"y",UiScaleRawToLogicalCoordinate(y)}});
        }
    }
    data["shopBoards"]=boards;data["playerMenu"]=menu;ui->setCommerceData(data);
}
void BindCommerceUi(QtUiState* ui){
    QObject::connect(ui,&QtUiState::commerceActionRequested,[](const QString& action,const QVariantMap& args){
        using namespace ro::commerce;
        if(action=="describeCardSlot"){
            const int x=args.value("x").toInt(),y=args.value("y").toInt();
            g_windowMgr.OnRBtnDown(x,y);g_windowMgr.OnRBtnUp(x,y);return;
        }
        if(action=="hoverCardSlot"){
            g_windowMgr.OnMouseMove(args.value("x").toInt(),args.value("y").toInt());return;
        }
        if(action=="clearCardSlotHover"){
            if(g_windowMgr.m_itemInfoWnd)g_windowMgr.m_itemInfoWnd->OnMouseHover(-1,-1);
            if(g_windowMgr.m_cardInfoWnd)g_windowMgr.m_cardInfoWnd->OnMouseHover(-1,-1);
            return;
        }
        if(action=="closeItemDetail"){
            auto* window=args.value("card").toBool()?g_windowMgr.m_cardInfoWnd:g_windowMgr.m_itemInfoWnd;
            if(window)g_windowMgr.DeleteWindow(window);
            return;
        }
        if(action=="browseShop"){const auto target=args.value("target").toUInt();auto it=state.shopBoards.find(target);if(it!=state.shopBoards.end())PlayerAction(8,target,it->second);return;}
        if(action=="contextClose"){g_windowMgr.ClosePlayerContextMenu();return;}
        if(action=="context"){
            auto* menu=g_windowMgr.m_playerContextMenuWnd;if(!menu||!menu->m_show)return;
            const auto id=menu->GetTargetAid();const auto name=menu->GetTargetName();const int command=args.value("id").toInt();menu->HideMenu();
            if(command==CGameMode::PlayerContext_AskJoinParty)g_modeMgr.SendMsg(CGameMode::GameMsg_RequestPartyInvite,id,0,0);
            else if(command==CGameMode::PlayerContext_RequestAdoption)
                g_modeMgr.SendMsg(CGameMode::GameMsg_RequestPlayerContextAction,id,command,0);
            else if(!PlayerAction(command,id,name)&&state.status.empty())g_windowMgr.PushChatEvent("That player action is unavailable right now.",0x00FF8080u,6);return;
        }
        Command c;c.action=action.toStdString();c.from=args.value("from").toString().toStdString();c.to=args.value("to").toString().toStdString();c.index=args.value("index").toUInt();c.amount=args.value("amount").toUInt();c.price=args.value("price").toUInt();c.text=args.value("text").toString().toUtf8().toStdString();c.subject=args.value("subject").toString().toUtf8().toStdString();c.body=args.value("body").toString().toUtf8().toStdString();c.x=args.value("x").toInt();c.y=args.value("y").toInt();c.tab=args.value("tab").toInt();
        if(action=="externalDrop"){
            const auto* item=g_session.GetInventoryItemByIndex(c.index);auto* mode=g_modeMgr.GetCurrentGameMode();
            if(!item||!mode||state.tradeActive||state.vendingActive
                || (args.contains("id") && item->GetItemId()!=args.value("id").toUInt()))return;
            mode->m_dragType=static_cast<int>(DragType::ShortcutItem);mode->m_dragInfo={};mode->m_dragInfo.type=static_cast<int>(DragType::ShortcutItem);mode->m_dragInfo.source=static_cast<int>(DragSource::InventoryWindow);
            mode->m_dragInfo.itemId=item->GetItemId();mode->m_dragInfo.itemIndex=item->m_itemIndex;mode->m_dragInfo.itemCount=item->m_num;mode->m_dragInfo.itemEquipLocation=item->m_location;
            g_windowMgr.OnLBtnUp(c.x,c.y);return;
        }
        if(action=="describe"){DescribeCommerceItem(args);return;}
        if(!Action(c) && state.status.empty())state.status="Action unavailable. Check the quantity and current transaction state.";
    });
}
