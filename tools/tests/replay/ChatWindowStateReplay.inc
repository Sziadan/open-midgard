#include "ui/ChatWindowState.h"
#if RO_ENABLE_QT6_UI
#include "qtui/QtUiState.h"
#endif

void ChatWindowStateReplay()
{
    SetCase("chat_window_state", "PK09.21", "primary-ui-state-messages",
        "managed chat messages -> source state/anchor/capture -> shared Qt rectangles");
    g_windowMgr.RemoveAllWindows();
    g_windowMgr.ClearChatEvents();
    const int previousStatus = g_windowMgr.m_chatWndStatus;
    const bool previousSticky = g_windowMgr.m_chatWndStickOn;
    struct Cleanup {
        int status; bool sticky;
        ~Cleanup() {
            g_windowMgr.m_captureWindow = nullptr;
            if (g_windowMgr.m_chatWnd) g_windowMgr.m_chatWnd->SetState(7);
            g_windowMgr.RemoveAllWindows();
            g_windowMgr.m_chatWndStatus = status;
            g_windowMgr.m_chatWndStickOn = sticky;
        }
    } cleanup{previousStatus, previousSticky};
    g_windowMgr.SendMsg(2, 7, 0);
    Require(!g_windowMgr.m_chatWnd, "manager state message created absent chat");
    g_windowMgr.m_chatWndStickOn = true;
    g_windowMgr.SendMsg(3, 0, 0);
    auto* chat = g_windowMgr.m_chatWnd;
    Require(chat && chat->m_show, "manager show message did not create/show chat");
    chat->Resize(600, chat->m_h);
    chat->Move(20, 200 - chat->m_h);
    const int heights[] = {24,96,110,124,138,152,166,180,194,208,222,236,250,264,278,292};
    for (int state = 0; state < 16; ++state) {
        chat->SendMsg(nullptr, 15, state, 0, 0);
        Require(chat->GetExpansionState() == state && chat->m_h == heights[state]
            && chat->m_y + chat->m_h == 200, "direct chat state lost source height/bottom");
        chat->OnProcess();
        Require(chat->m_y + chat->m_h == 200, "process layout clamped the source bottom anchor");
        const auto rects = chat->GetLayoutDisplay();
        Require(rects.messageInput.bottom - rects.messageInput.top == 16
            && rects.messageInput.bottom == heights[state] - 4
            && rects.messageInput.left == 110 && rects.messageInput.right == 570
            && rects.whisperInput.left == 6 && rects.whisperInput.right == 96,
            "chat editor geometry lost primary columns/band");
#if RO_ENABLE_QT6_UI
        QtUiStateAdapter adapter;
        adapter.syncChat();
        auto* ui = static_cast<QtUiState*>(adapter.stateObject());
        const auto data = ui->chatWindowUi();
        const auto message = data.value("sourceRects").toMap().value("message").toMap();
        Require(ui->chatWindowHeight() == heights[state] && data.value("expansionState").toInt() == state
            && data.value("expanded").toBool() == (state != 0)
            && message.value("y").toInt() == rects.messageInput.top
            && message.value("height").toInt() == 16, "Qt chat state/geometry diverged from native owner");
#endif
    }
    const int up[] = {1,4,4,7,7,7,10,10,10,13,13,13,0,0,0,0};
    const int down[] = {15,0,0,0,1,1,4,4,4,7,7,7,10,10,10,13};
    for (int state = 0; state < 16; ++state) {
        chat->SetState(state); chat->SendMsg(nullptr, 2, 0, 0, 0);
        Require(chat->GetExpansionState() == up[state], "managed forward chat cycle changed");
        chat->SetState(state); chat->SendMsg(nullptr, 2, 1, 0, 0);
        Require(chat->GetExpansionState() == down[state], "managed signed backward chat cycle changed");
    }
    chat->SetState(7);
    g_windowMgr.m_captureWindow = chat;
    chat->SendMsg(nullptr, 15, 0, 0, 0);
    Require(chat->GetExpansionState() == 7, "captured chat frame accepted a source state change");
    chat->CancelPointerCapture();
    Require(g_windowMgr.m_captureWindow == nullptr, "cancelled native chat capture retained manager pointer");
    const auto gap = chat->GetLayoutDisplay();
    const int gapX = chat->m_x + gap.tabs[0].right + 2, gapY = chat->m_y + 8;
    Require(chat->HandleQtMouseDown(gapX, gapY) && chat->IsQtPointerCaptured(),
        "Qt chat frame did not acquire its own capture");
    chat->SendMsg(nullptr, 15, 0, 0, 0);
    Require(chat->GetExpansionState() == 7, "Qt captured chat frame accepted a source state change");
    chat->CancelPointerCapture();
    Require(!chat->IsQtPointerCaptured(), "chat capture cancellation retained local capture");
    chat->SetState(0);
    Require(chat->GetExpansionState() == 0, "cancelled chat capture kept blocking state changes");
    chat->SetState(7);
    const int resizeX = chat->m_x + 300, resizeY = chat->m_y + 2;
    Require(chat->HandleQtMouseDown(resizeX, resizeY) && chat->IsQtPointerCaptured(),
        "Qt inline resizer did not capture");
    chat->SendMsg(nullptr, 15, 0, 0, 0);
    Require(chat->GetExpansionState() == 0, "inline child resizer was mistaken for frame capture");
    chat->HandleQtMouseUp(resizeX, resizeY);
    for (const auto pair : {std::pair<int,int>{71,0}, {72,1}, {85,1}, {86,2}, {268,15}, {0x7fffffff,15}}) {
        chat->SendMsg(nullptr, 14, 2, 0, pair.first);
        Require(chat->GetExpansionState() == pair.second && chat->m_y + chat->m_h == 200,
            "managed resize value lost source threshold/anchor");
    }
    chat->SetState(0);
    g_windowMgr.PushChatEvent("notice stays collapsed", 0x00ffff00, 6);
    chat->OnProcess();
    Require(chat->GetExpansionState() == 0 && !chat->GetLines().empty(), "notice expanded or lost collapsed chat");
    g_windowMgr.m_chatWndStickOn = false;
    chat->SendMsg(nullptr, 2, 0, 0, 0);
    chat->SendMsg(nullptr, 14, 2, 0, 268);
    g_windowMgr.SendMsg(3, 0, 0);
    Require(chat->GetExpansionState() == 0, "sticky-off chat accepted conditional expansion");
    g_windowMgr.m_chatWndStickOn = true;
    g_windowMgr.SendMsg(3, 0, 0);
    Require(chat->GetExpansionState() == 1 && chat->m_h == 96, "explicit show did not expand collapsed sticky chat");
    g_windowMgr.SendMsg(2, -1, 0);
    Require(chat->GetExpansionState() == 15, "negative direct chat state did not wrap to15");
    g_windowMgr.SendMsg(2, 16, 0);
    Require(chat->GetExpansionState() == 0, "overflow direct chat state did not wrap to0");
    // Complete family ownership is separate from the pending source producer parser.
    chat->SetState(10);
    chat->SetQtInputText(UINewChatWnd::InputField_Message, "sticky draft");
    chat->SendMsg(nullptr, 6, 292, 0, 0);
    Require(!g_windowMgr.m_chatWndStickOn && chat->GetExpansionState() == 0,
        "detach did not save state/collapse");
    auto* mini = g_windowMgr.m_subChatMiniWnd;
    auto* st = g_windowMgr.m_subChatWndST;
    auto* bt = g_windowMgr.m_subChatWndBT;
    Require(mini && st && bt && mini->m_w == 64 && mini->m_h == 32
        && st->m_id == 137 && bt->m_id == 138 && st->GetKind() == 0 && bt->GetKind() == 1,
        "managed chat family missing or source kind collided with manager ID");
    Require(mini->m_show && mini->m_x == chat->m_x + chat->m_w
        && mini->m_y == chat->m_y + chat->m_h - 32, "mini did not follow collapsed main");
    for (const int x : {0, 1, 32, 33, 63}) {
        g_windowMgr.m_onStSubChat = g_windowMgr.m_onBtSubChat = false;
        mini->OnLBtnDown(mini->m_x + x, mini->m_y);
        Require(g_windowMgr.m_onStSubChat == (x > 0 && x <= 32)
            && g_windowMgr.m_onBtSubChat == (x == 0 || x > 32), "mini source boundary predicate changed");
    }
    g_windowMgr.m_onStSubChat = g_windowMgr.m_onBtSubChat = true;
    mini->UpdateHistoryVisibility();
    Require(st->m_show && bt->m_show, "separate mini flags did not show histories");
    chat->SetShow(0);
    Require(!mini->m_show && !st->m_show && !bt->m_show
        && g_windowMgr.m_onStSubChat && g_windowMgr.m_onBtSubChat, "main hide erased flags or left auxiliaries visible");
    chat->SetShow(1);
    Require(mini->m_show && st->m_show && bt->m_show, "main show did not restore detached visibility");
    st->SendMsg(nullptr, 6, 136, 0, 0);
    Require(!st->m_show && !g_windowMgr.m_onStSubChat && bt->m_show
        && g_windowMgr.m_subChatWndST == st, "history close deleted owner or toggled other stream");
    for (int i = 0; i < 241; ++i) {
        std::string text = "owned-" + std::to_string(i), talker = "talker-" + std::to_string(i);
        st->Append(text.c_str(), static_cast<u32>(i), talker.c_str());
    }
    Require(st->GetRows().size() == 121 && st->GetRows().front().text == "owned-120"
        && st->GetRows().front().color == 120 && st->GetRows().front().talker == "talker-120"
        && bt->GetRows().empty(), "detached rows lost independent aligned half-trim ownership");
    st->SendMsg(nullptr, 14, 7, 0x7fffffff, 0x7fffffff);
    Require(st->m_w == 504 && st->m_h == 362, "history resize clamp/snap changed");
    st->Move(20, 30); st->Resize(300, 95);
    g_windowMgr.DeleteWindow(st);
    Require(!g_windowMgr.m_subChatWndST && g_windowMgr.m_subChatInfo[0].x == 20
        && g_windowMgr.m_subChatInfo[0].height == 95, "history deletion did not store direct geometry");
    st = static_cast<UISubChatHisWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_SUBCHATST));
    Require(st->m_x == 20 && st->m_y == 30 && st->m_w == 300 && st->m_h == 95
        && st->GetRows().empty() && !st->m_show, "history recreation conflated cached dimensions/rows/show");
    chat->SendMsg(nullptr, 6, 291, 0, 0);
    Require(g_windowMgr.m_chatWndStickOn && chat->GetExpansionState() == 10
        && !mini->m_show && !bt->m_show && chat->GetInputText() == "sticky draft",
        "attach lost saved expansion/input or retained auxiliaries");
    chat->SetState(0); chat->StickOnOff(false); chat->StickOnOff(true);
    Require(chat->GetExpansionState() == 1, "reattach did not promote saved collapsed state to1");
    g_windowMgr.DeleteWindow(chat);
    g_windowMgr.EnsureChatWindowVisible();
    Require(!g_windowMgr.m_subChatWndST && !g_windowMgr.m_subChatWndBT && !g_windowMgr.m_subChatMiniWnd,
        "main recreation violated source delete-or-conditionally-create rule");
    g_windowMgr.RemoveAllWindows();
    Require(g_windowMgr.Init() && g_windowMgr.m_chatWnd && g_windowMgr.m_subChatWndST
        && g_windowMgr.m_subChatWndBT && g_windowMgr.m_subChatMiniWnd
        && g_windowMgr.m_children.back() == g_windowMgr.m_chatWnd,
        "initial main construction did not register auxiliaries before the main owner");
    std::cout << "{\"case_id\":\"chat_window_state\",\"contract_id\":\"PK09.21\",\"status\":\"PASS\",\"states\":16,\"cycles\":32,\"auxiliary_lifecycle_pending\":true}\n";
}
