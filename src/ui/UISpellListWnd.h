#pragma once

#include "UIItemIdentifyWnd.h"
#include "item/Item.h"

#include <functional>
#include <utility>
#include <vector>

// Receiver model for the spell list window. These parallel receiver vectors
// intentionally do not update UIItemIdentifyWnd's private display/scroll state;
// rendering and scrollbar integration remain open for the full window port.
class UISpellListWnd : public UIItemIdentifyWnd {
public:
    using ScrollBarRecalcCallback = std::function<void(UISpellListWnd&)>;

    msgresult_t SendMsg(UIWindow* sender,
                        int msg,
                        msgparam_t wparam,
                        msgparam_t lparam,
                        msgparam_t extra) override;
    virtual void RecalcScrollBarPos();

    const std::vector<int>& GetSpellIds() const { return m_spellList; }
    const std::vector<ITEM_INFO>& GetDefaultItemRecords() const { return m_itemInfoList; }
    void SetScrollBarRecalcCallback(ScrollBarRecalcCallback callback)
    {
        m_scrollBarRecalcCallback = std::move(callback);
    }

private:
    std::vector<int> m_spellList;
    std::vector<ITEM_INFO> m_itemInfoList;
    ScrollBarRecalcCallback m_scrollBarRecalcCallback;
};
