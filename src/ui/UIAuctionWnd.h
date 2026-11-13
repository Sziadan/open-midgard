#pragma once

#include "UIFrameWnd.h"
#include "gamemode/GameMode.h"
#include "item/Item.h"

class UIAuctionWnd final : public UIFrameWnd {
public:
    explicit UIAuctionWnd(int windowId);
    void Move(int x, int y) override;
    void OnDraw() override;
    void OnLBtnDown(int x, int y) override;
    void OnLBtnUp(int x, int y) override;
    void OnWheel(int delta) override;
    void DragAndDrop(int x, int y, const DRAG_INFO* info) override;

private:
    enum class Action { None, Close, Browse, Search, Price, Sales, Bids, Sell, Armor, Weapon, Card, Etc, Previous, Next,
        Bid, Cancel, Stop, Stage, Unstage, Register };
    struct Control { Action action; RECT bounds; const char* label; };
    static const Control kControls[];
    bool IsVisible(Action action) const;
    Action HitControl(int x, int y) const;
    void RunAction(Action action);
    void Confirm(CGameMode& mode, Action action, const CGameMode::AuctionSearchRow& quote, u32 bid);
    std::vector<const ITEM_INFO*> InventoryPage() const;

    UIEditCtrl* m_query = nullptr;
    UIEditCtrl* m_price = nullptr;
    UIEditCtrl* m_buyNow = nullptr;
    UIEditCtrl* m_hours = nullptr;
    bool m_sell = false;
    std::size_t m_inventoryOffset = 0;
    u16 m_inventoryIndex = 0;
    CGameMode::AuctionSearchRow m_quote;
    Action m_pressed = Action::None;
    std::string m_status;
};
