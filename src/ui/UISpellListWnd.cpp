#include "UISpellListWnd.h"

msgresult_t UISpellListWnd::SendMsg(UIWindow* sender,
                                    int msg,
                                    msgparam_t wparam,
                                    msgparam_t lparam,
                                    msgparam_t extra)
{
    switch (msg) {
    case 31:
        m_spellList.push_back(static_cast<int>(wparam));
        m_itemInfoList.emplace_back(ITEM_INFO{});
        SendMsg(this, 60, 0, 0, 0);
        return 0;
    case 60:
        RecalcScrollBarPos();
        Invalidate();
        return 0;
    default:
        return UIItemIdentifyWnd::SendMsg(sender, msg, wparam, lparam, extra);
    }
}

void UISpellListWnd::RecalcScrollBarPos()
{
    if (m_scrollBarRecalcCallback) {
        m_scrollBarRecalcCallback(*this);
    }
}
