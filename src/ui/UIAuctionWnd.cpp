#include "UIAuctionWnd.h"

#include "UIChatRoomSettingsWnd.h"
#include "UIShopCommon.h"
#include "UIWindowMgr.h"
#include "core/SourceTextCodec.h"
#include "session/Session.h"

#include <algorithm>
#include <charconv>

namespace {
constexpr int kWidth = 480;
constexpr int kHeight = 267;
constexpr int kRowTop = 101;
constexpr int kRowHeight = 20;
constexpr std::size_t kRows = 5;

u32 ReadNumber(const UIEditCtrl& edit)
{
    const std::string_view input(edit.GetText());
    u32 number = 0;
    const auto parsed = std::from_chars(input.data(), input.data() + input.size(), number);
    return parsed.ec == std::errc{} && parsed.ptr == input.data() + input.size() ? number : 0;
}

std::string DisplayName(const std::string& name)
{
    return ro::DecodeSourceText(name, ro::SelectedSourceTextCodePage()).value_or(name);
}
}

const UIAuctionWnd::Control UIAuctionWnd::kControls[] = {
    {Action::Close, {444,2,476,18}, "Close"},
    {Action::Browse, {8,25,77,45}, "Browse"},
    {Action::Sales, {83,25,152,45}, "My sales"},
    {Action::Bids, {158,25,227,45}, "My bids"},
    {Action::Sell, {233,25,302,45}, "Register"},
    {Action::Armor, {308,25,345,45}, "Armor"},
    {Action::Weapon, {349,25,395,45}, "Weapon"},
    {Action::Card, {399,25,431,45}, "Card"},
    {Action::Etc, {435,25,472,45}, "Other"},
    {Action::Search, {224,52,295,72}, "Find item"},
    {Action::Price, {301,52,390,72}, "Max price"},
    {Action::Previous, {8,205,45,225}, "Prev"},
    {Action::Next, {52,205,89,225}, "Next"},
    {Action::Bid, {263,205,320,225}, "Bid"},
    {Action::Cancel, {326,205,392,225}, "Cancel sale"},
    {Action::Stop, {398,205,472,225}, "End sale"},
    {Action::Stage, {263,205,320,225}, "Stage"},
    {Action::Unstage, {326,205,392,225}, "Remove"},
    {Action::Register, {398,205,472,225}, "Register"},
};

UIAuctionWnd::UIAuctionWnd(int windowId)
{
    m_id = windowId;
    Create(kWidth, kHeight);
    const auto addEdit = [&](int x, int y, int width, int limit) {
        auto* edit = new UIEditCtrl;
        edit->Create(width, 20);
        edit->Move(x, y);
        edit->SetFrameColor(207, 215, 226);
        edit->m_maxchar = limit;
        AddChild(edit);
        return edit;
    };
    m_query = addEdit(54, 52, 164, 23);
    m_price = addEdit(112, 205, 119, 10);
    m_buyNow = addEdit(178, 52, 103, 10);
    m_hours = addEdit(344, 52, 40, 2);
    m_hours->SetText("24");
    m_buyNow->SetShow(0);
    m_hours->SetShow(0);
}

void UIAuctionWnd::Move(int x, int y)
{
    const int dx = x - m_x;
    const int dy = y - m_y;
    UIFrameWnd::Move(x, y);
    for (auto* child : m_children) child->Move(child->m_x + dx, child->m_y + dy);
}

bool UIAuctionWnd::IsVisible(Action action) const
{
    switch (action) {
    case Action::Search: case Action::Price: case Action::Armor: case Action::Weapon: case Action::Card:
    case Action::Etc: case Action::Bid: case Action::Cancel: case Action::Stop:
        return !m_sell;
    case Action::Stage: case Action::Unstage: case Action::Register:
        return m_sell;
    default:
        return true;
    }
}

std::vector<const ITEM_INFO*> UIAuctionWnd::InventoryPage() const
{
    std::vector<const ITEM_INFO*> page;
    std::size_t skipped = 0;
    for (const auto& item : g_session.GetInventoryItems()) {
        if (item.m_num <= 0 || !item.m_isIdentified || item.m_wearLocation || item.m_deleteTime) continue;
        if (skipped++ < m_inventoryOffset) continue;
        page.push_back(&item);
        if (page.size() == kRows) break;
    }
    return page;
}

void UIAuctionWnd::OnDraw()
{
    auto* mode = g_modeMgr.GetCurrentGameMode();
    if (!mode) return;
    HDC dc = AcquireDrawTarget();
    if (!dc) return;
    const auto fill = [&](RECT rect, COLORREF color) {
        OffsetRect(&rect, m_x, m_y);
        shopui::FillRectColor(dc, rect, color);
    };
    const auto label = [&](RECT rect, const std::string& value, unsigned alignment = DT_LEFT) {
        OffsetRect(&rect, m_x, m_y);
        shopui::DrawWindowTextRect(dc, rect, value, RGB(25, 39, 61),
            DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS | alignment);
    };
    fill({0,0,kWidth,kHeight}, RGB(251, 252, 254));
    fill({0,0,kWidth,20}, RGB(218, 229, 245));
    label({8,0,440,20}, "Auction");
    for (const auto& control : kControls) {
        if (!IsVisible(control.action)) continue;
        fill(control.bounds, control.action == m_pressed ? RGB(188, 208, 235) : RGB(225, 235, 248));
        label(control.bounds, control.label, DT_CENTER);
    }
    const auto& search = mode->m_auctionSearch;
    if (m_sell) {
        label({8,52,51,72}, "Start");
        label({112,52,175,72}, "Buy now");
        label({300,52,340,72}, "Hours");
        label({8,80,472,100}, "Select an inventory item");
        const auto page = InventoryPage();
        for (std::size_t i = 0; i < page.size(); ++i) {
            const int top = kRowTop + static_cast<int>(i) * kRowHeight;
            const auto& item = *page[i];
            if (item.m_itemIndex == m_inventoryIndex) fill({8,top,472,top+kRowHeight}, RGB(212, 227, 248));
            label({12,top,395,top+kRowHeight}, item.GetDisplayName());
            label({397,top,466,top+kRowHeight}, std::to_string(item.m_num), DT_RIGHT);
        }
    } else {
        label({8,52,51,72}, "Query");
        label({12,80,195,100}, "Item");
        label({198,80,312,100}, "Seller");
        label({314,80,390,100}, "Current", DT_RIGHT);
        label({392,80,466,100}, "Buy now", DT_RIGHT);
        for (std::size_t i = 0; i < std::min(search.rows.size(), kRows); ++i) {
            const int top = kRowTop + static_cast<int>(i) * kRowHeight;
            const auto& row = search.rows[i];
            if (row.auctionId == m_quote.auctionId) fill({8,top,472,top+kRowHeight}, RGB(212, 227, 248));
            label({12,top,195,top+kRowHeight}, g_ttemmgr.GetDisplayName(row.itemId, row.identified));
            label({198,top,312,top+kRowHeight}, DisplayName(row.seller));
            label({314,top,390,top+kRowHeight}, std::to_string(row.price), DT_RIGHT);
            label({392,top,466,top+kRowHeight}, std::to_string(row.buyNow), DT_RIGHT);
        }
        label({93,205,111,225}, "Bid");
    }
    std::string status = m_status;
    if (search.awaitingReply || mode->m_auctionStaging.pendingIndex
        || mode->m_auctionTransaction != CGameMode::AuctionTransaction::None) status = "Waiting for the auction server.";
    else if (m_sell && mode->m_auctionStaging.stagedItemId) {
        status = "Staged: " + g_ttemmgr.GetDisplayName(mode->m_auctionStaging.stagedItemId, true);
    } else if (!m_sell && status.empty() && search.pages) {
        status = "Page " + std::to_string(search.query.page) + " / " + std::to_string(search.pages);
    }
    label({8,232,472,255}, status);
    DrawChildrenToHdc(dc);
    ReleaseDrawTarget(dc);
}

UIAuctionWnd::Action UIAuctionWnd::HitControl(int x, int y) const
{
    const POINT point{x - m_x, y - m_y};
    for (const auto& control : kControls) {
        if (IsVisible(control.action) && PtInRect(&control.bounds, point)) return control.action;
    }
    return Action::None;
}

void UIAuctionWnd::OnLBtnDown(int x, int y)
{
    m_pressed = HitControl(x, y);
    if (m_pressed != Action::None) {
        Invalidate();
        return;
    }
    const int localY = y - m_y;
    if (localY >= kRowTop && localY < kRowTop + static_cast<int>(kRows) * kRowHeight) {
        const std::size_t index = (localY - kRowTop) / kRowHeight;
        if (m_sell) {
            const auto page = InventoryPage();
            m_inventoryIndex = index < page.size() ? static_cast<u16>(page[index]->m_itemIndex) : 0;
        } else if (const auto* mode = g_modeMgr.GetCurrentGameMode()) {
            const auto& rows = mode->m_auctionSearch.rows;
            m_quote = index < rows.size() ? rows[index] : CGameMode::AuctionSearchRow{};
        }
        Invalidate();
        return;
    }
    if (localY < 20) UIFrameWnd::OnLBtnDown(x, y);
}

void UIAuctionWnd::OnLBtnUp(int x, int y)
{
    const auto pressed = m_pressed;
    m_pressed = Action::None;
    if (pressed != Action::None && HitControl(x, y) == pressed) {
        RunAction(pressed);
        return;
    }
    UIFrameWnd::OnLBtnUp(x, y);
    Invalidate();
}

void UIAuctionWnd::OnWheel(int delta)
{
    if (delta) RunAction(delta > 0 ? Action::Previous : Action::Next);
}

void UIAuctionWnd::DragAndDrop(int, int, const DRAG_INFO* info)
{
    if (!m_sell || !info || info->type != static_cast<int>(DragType::ShortcutItem)
        || info->source != static_cast<int>(DragSource::InventoryWindow)
        || info->itemIndex > 65535) return;
    m_inventoryIndex = static_cast<u16>(info->itemIndex);
    RunAction(Action::Stage);
}

void UIAuctionWnd::Confirm(CGameMode& mode, Action action, const CGameMode::AuctionSearchRow& quote, u32 bid)
{
    if (g_windowMgr.m_modalWindow) return;
    using Transaction = CGameMode::AuctionTransaction;
    const auto transaction = action == Action::Bid ? Transaction::Bid
        : action == Action::Cancel ? Transaction::Cancel : Transaction::Stop;
    const auto generation = g_modeMgr.GetOperationGeneration();
    const std::string item = g_ttemmgr.GetDisplayName(quote.itemId, quote.identified);
    const auto text = action == Action::Bid ? "Bid " + std::to_string(bid) + " Zeny for " + item + "?"
        : (action == Action::Cancel ? "Cancel the sale of " : "End the sale of ") + item + "?";
    auto* box = new UIChatRoomMessageBox(text, 0);
    box->SetSourceOwner(this);
    box->ConfigureConfirmation(this, [this, &mode, generation, transaction, quote, bid](bool accepted) {
        if (accepted && g_modeMgr.GetOperationGeneration() == generation
            && g_modeMgr.GetCurrentGameMode() == &mode && g_windowMgr.ContainsWindow(this)) {
            m_status = mode.RequestAuctionTransaction(transaction, quote, bid) ? "" : "The selected auction or amount is no longer valid.";
            Invalidate();
        }
    });
    g_windowMgr.m_roomMessageBox = box;
    g_windowMgr.m_children.push_back(box);
    g_windowMgr.m_modalWindow = box;
    g_windowMgr.m_editWindow = nullptr;
}

void UIAuctionWnd::RunAction(Action action)
{
    auto* mode = g_modeMgr.GetCurrentGameMode();
    if (!mode) return;
    if (action == Action::Close) {
        if (!mode->CancelAuctionStaging()) {
            m_status = "Could not release the staged item.";
            Invalidate();
            return;
        }
        mode->CloseAuctionSearch();
        g_windowMgr.DeleteWindow(this);
        return;
    }
    m_status.clear();
    using Kind = CGameMode::AuctionSearchKind;
    CGameMode::AuctionSearchQuery query;
    bool search = false;
    switch (action) {
    case Action::Sell:
        m_sell = true;
        break;
    case Action::Browse: case Action::Sales: case Action::Bids:
        m_sell = false;
        query.kind = action == Action::Sales ? Kind::OwnSales : action == Action::Bids ? Kind::OwnBids : Kind::Name;
        search = true;
        break;
    case Action::Search:
        query.name = m_query->GetText();
        search = true;
        break;
    case Action::Armor: case Action::Weapon: case Action::Card: case Action::Etc:
        query.kind = action == Action::Armor ? Kind::Armor : action == Action::Weapon ? Kind::Weapon
            : action == Action::Card ? Kind::Card : Kind::Etc;
        search = true;
        break;
    case Action::Price:
        query.kind = Kind::Price;
        query.maximumPrice = ReadNumber(*m_query);
        search = query.maximumPrice != 0;
        if (!search) m_status = "Enter a maximum price in Zeny.";
        break;
    case Action::Previous: case Action::Next:
        if (m_sell) {
            if (action == Action::Previous && m_inventoryOffset) m_inventoryOffset -= kRows;
            if (action == Action::Next && InventoryPage().size() == kRows) {
                m_inventoryOffset += kRows;
                if (InventoryPage().empty()) m_inventoryOffset -= kRows;
            }
            m_inventoryIndex = 0;
        } else {
            query = mode->m_auctionSearch.query;
            if (action == Action::Previous && query.page > 1) {
                --query.page;
                search = true;
            } else if (action == Action::Next && query.page < mode->m_auctionSearch.pages && query.page < 32767) {
                ++query.page;
                search = true;
            }
        }
        break;
    case Action::Stage:
        if (!mode->RequestAuctionStageItem(m_inventoryIndex)) m_status = "Select an eligible inventory item.";
        break;
    case Action::Unstage:
        if (!mode->CancelAuctionStaging()) m_status = "Could not release the staged item.";
        break;
    case Action::Register: {
        const u32 hours = ReadNumber(*m_hours);
        const u32 start = ReadNumber(*m_query);
        const u32 buyNow = ReadNumber(*m_buyNow);
        if (hours < 1 || hours > 48 || !start || start >= buyNow || buyNow > 0x7fffffffu
            || !mode->m_auctionStaging.stagedIndex) {
            m_status = "Stage an item and enter valid prices and hours (1–48).";
            break;
        }
        if (g_windowMgr.m_modalWindow) break;
        const auto generation = g_modeMgr.GetOperationGeneration();
        const auto stagedIndex = mode->m_auctionStaging.stagedIndex;
        const auto stagedItem = mode->m_auctionStaging.stagedItemId;
        const auto text = "Register " + g_ttemmgr.GetDisplayName(stagedItem, true)
            + " for " + std::to_string(start) + " Zeny, with a buy-now price of "
            + std::to_string(buyNow) + " Zeny, for " + std::to_string(hours) + " hours?";
        auto* box = new UIChatRoomMessageBox(text, 0);
        box->SetSourceOwner(this);
        box->ConfigureConfirmation(this, [this, mode, generation, stagedIndex, stagedItem, start, buyNow, hours](bool accepted) {
            if (accepted && g_modeMgr.GetOperationGeneration() == generation
                && g_modeMgr.GetCurrentGameMode() == mode && g_windowMgr.ContainsWindow(this)) {
                const auto& staged = mode->m_auctionStaging;
                if (staged.stagedIndex != stagedIndex || staged.stagedItemId != stagedItem
                    || !mode->RequestAuctionRegistration(start, buyNow, static_cast<u16>(hours))) {
                    m_status = "The staged item or registration is no longer valid.";
                }
                Invalidate();
            }
        });
        g_windowMgr.m_roomMessageBox = box;
        g_windowMgr.m_children.push_back(box);
        g_windowMgr.m_modalWindow = box;
        g_windowMgr.m_editWindow = nullptr;
        break;
    }
    case Action::Bid: case Action::Cancel: case Action::Stop:
        if (!m_quote.auctionId) m_status = "Select a listing.";
        else if (action == Action::Bid && !ReadNumber(*m_price)) m_status = "Enter a bid amount.";
        else Confirm(*mode, action, m_quote, ReadNumber(*m_price));
        break;
    default: break;
    }
    if (search) {
        if (mode->RequestAuctionSearch(query)) m_quote = {};
        else m_status = "Could not send the search. Check the query and try again.";
    }
    if (m_sell && g_windowMgr.m_editWindow == m_price) g_windowMgr.m_editWindow = nullptr;
    if (!m_sell && (g_windowMgr.m_editWindow == m_buyNow || g_windowMgr.m_editWindow == m_hours)) {
        g_windowMgr.m_editWindow = nullptr;
    }
    m_price->SetShow(!m_sell);
    m_buyNow->SetShow(m_sell);
    m_hours->SetShow(m_sell);
    Invalidate();
}
