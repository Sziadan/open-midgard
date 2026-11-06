#include "core/ChatNotice.h"
#include "core/Globals.h"

void ChatNoticeRoutingReplay()
{
    SetCase("chat_notice_routing", "PK09.21", "source-notice-route-and-owner-literals",
        "manager1/20 and child37/115 -> owned split main histories -> unsplit detached mirrors");
    struct Restore {
        std::array<bool, 13> options = g_session.m_battleMsgOnOffSet;
        ServiceType service = g_serviceType;
        std::string name = g_session.GetPlayerName();
        bool sticky = g_windowMgr.m_chatWndStickOn;
        ~Restore() {
            g_windowMgr.RemoveAllWindows(); g_windowMgr.ClearChatEvents();
            g_session.m_battleMsgOnOffSet = options;
            g_serviceType = service;
            std::snprintf(g_session.m_playerName, sizeof(g_session.m_playerName), "%s", name.c_str());
            g_windowMgr.m_chatWndStickOn = sticky;
        }
    } restore;
    g_windowMgr.RemoveAllWindows(); g_windowMgr.ClearChatEvents();
    g_session.m_battleMsgOnOffSet.fill(true);
    g_serviceType = ServiceAmerica;
    std::snprintf(g_session.m_playerName, sizeof(g_session.m_playerName), "%s", "Self");
    const auto oldBacklog = g_windowMgr.GetSourceChatBacklog().size();
    const char* absent = "NO MSG";
    g_windowMgr.SendMsg(20, reinterpret_cast<msgparam_t>(absent), 0, 5, reinterpret_cast<msgparam_t>("LostTalker"));
    Require(!g_windowMgr.m_chatWnd && g_windowMgr.GetSourceChatBacklog().size() == oldBacklog + 1
        && g_windowMgr.GetSourceChatBacklog().back().text == "NO MSG"
        && g_windowMgr.GetSourceChatBacklog().back().color == 0xffffff
        && g_windowMgr.GetSourceChatBacklog().back().talker.empty(), "absent backlog filtered or fabricated speaker/category");
    g_windowMgr.EnsureChatWindowVisible();
    auto* chat = g_windowMgr.m_chatWnd;
    chat->OnProcess();
    Require(chat->GetSourceHistory(false).empty() && chat->GetSourceHistory(true).empty()
        && chat->GetLines().empty(), "main creation replayed/reclassified absent source backlog");
    chat->SetState(0);
    const auto send = [](int route, const std::string& text, int type, const char* speaker = nullptr) {
        g_windowMgr.SendMsg(route, reinterpret_cast<msgparam_t>(text.c_str()), 0x000000ff,
            type, reinterpret_cast<msgparam_t>(speaker));
    };
    send(1, "NO MSG", 5); send(20, "No Msg", 5);
    Require(chat->GetSourceHistory(false).empty() && chat->GetSourceHistory(true).empty(), "live sentinels entered histories");
    send(1, "No MSG", 0);
    Require(chat->GetSourceHistory(false).size() == 1 && chat->GetSourceHistory(true).empty(), "sentinel comparison lost exact case");
    const auto& preview = g_windowMgr.GetChatPreviewEvents(1);
    Require(preview.size() == 1 && preview[0].source.route == ro::chat::NoticeRoute::Normal
        && preview[0].source.type == ro::chat::NoticeType::None && preview[0].color == 0xff0000,
        "receipt preview lost typed metadata or COLORREF conversion");
    const auto normalCount = chat->GetSourceHistory(false).size();
    const auto commandCount = chat->GetSourceHistory(true).size();
    for (int type = 1; type <= 13; ++type) send(1, "Peer : typed notice", type);
    Require(chat->GetSourceHistory(false).size() == normalCount + 11
        && chat->GetSourceHistory(true).size() == commandCount + 13,
        "numeric notice routing lost type8/9 suppression or default command copies");
    Require(chat->GetSourceHistory(true)[0].talker == "Peer", "normal speech did not extract source talker delimiter");
    g_session.m_battleMsgOnOffSet.fill(false);
    const auto normal = chat->GetSourceHistory(false).size(), command = chat->GetSourceHistory(true).size();
    send(1, "normal remains", 5); send(1, "party item suppressed", 8);
    send(20, "command bypasses options", 0, "ExplicitTalker");
    Require(chat->GetSourceHistory(false).size() == normal + 1
        && chat->GetSourceHistory(true).size() == command + 1
        && chat->GetSourceHistory(true).back().talker == "ExplicitTalker", "battle options gated normal or failed command bypass");
    send(1, "Self : no self whisper", 1);
    Require(chat->GetSourceHistory(false).back().talker.empty(), "self talker remained actionable");
    g_serviceType = ServiceGermany;
    const auto beforeReject = chat->GetSourceHistory(false).size();
    send(1, "twenty-five-byte-talker!!! : rejected", 1);
    Require(chat->GetSourceHistory(false).size() == beforeReject, "Germany accepted over24-byte source speaker");
    g_serviceType = ServiceAmerica;
    auto* st = g_windowMgr.m_subChatWndST;
    auto* bt = g_windowMgr.m_subChatWndBT;
    const auto stSize = st->GetRows().size(), btSize = bt->GetRows().size();
    const std::string longText(95, 'x');
    const auto beforeSplit = chat->GetSourceHistory(false).size();
    send(1, longText, 0, "CopiedSpeaker");
    Require(chat->GetSourceHistory(false).size() == beforeSplit + 2
        && chat->GetSourceHistory(false)[beforeSplit].text == std::string(94, 'x')
        && chat->GetSourceHistory(false).back().text == "x", "main source rows lost94-byte split");
    Require(st->GetRows().size() == stSize + 2 && bt->GetRows().size() == btSize
        && st->GetRows()[stSize].text == longText && st->GetRows().back().text == longText
        && st->GetRows().back().talker == "CopiedSpeaker", "detached mirror did not receive repeated unsplit owned argument");
    Require(chat->GetExpansionState() == 0, "notice insertion expanded collapsed chat");
    chat->StickOnOff(false);
    g_windowMgr.m_onStSubChat = g_windowMgr.m_onBtSubChat = true;
    g_windowMgr.UpdateChatAuxiliaryVisibility();
    g_windowMgr.DeleteWindow(chat);
#if RO_ENABLE_QT6_UI
    QtUiStateAdapter adapter;
    adapter.syncChat();
    auto* ui = static_cast<QtUiState*>(adapter.stateObject());
    const auto retained = ui->chatWindowUi();
    Require(!ui->chatWindowVisible() && retained.value("mini").toMap().value("visible").toBool()
        && retained.value("histories").toList().size() == 2,
        "main-absent Qt projection discarded retained auxiliary owners");
#endif
    const auto retainedSt = st->GetRows().size(), retainedBt = bt->GetRows().size();
    send(20, "absent source remains unclassified", 5, "NotRetained");
    Require(st->GetRows().size() == retainedSt && bt->GetRows().size() == retainedBt,
        "absent main notice leaked into retained detached histories");
    g_windowMgr.EnsureChatWindowVisible(); chat = g_windowMgr.m_chatWnd;
    chat->OnProcess();
    Require(chat->GetSourceHistory(false).empty() && chat->GetSourceHistory(true).empty()
        && chat->GetLines().empty(), "source receipt cache repopulated a different main instance");
    // Original child dispatch uses the same source parser without manager sentinel filtering.
    chat->SendMsg(nullptr, 37, reinterpret_cast<msgparam_t>("NO MSG"), 0xff, 0);
    Require(chat->GetSourceHistory(false).size() == 1 && chat->GetSourceHistory(false).back().text == "NO MSG",
        "child37 incorrectly applied manager sentinel filter");
    for (int i = 0; i < 240; ++i) {
        const auto text = "retained-" + std::to_string(i);
        chat->SendMsg(nullptr, 115, reinterpret_cast<msgparam_t>(text.c_str()), 0xff, 0);
    }
    chat->SendMsg(nullptr, 115, reinterpret_cast<msgparam_t>("retained-240"), 0xff, 0);
    Require(chat->GetSourceHistory(true).size() == 121
        && chat->GetSourceHistory(true).front().text == "retained-120"
        && chat->GetSourceHistory(true).back().text == "retained-240", "main source history did not half-trim before append");
    std::cout << "{\"case_id\":\"chat_notice_routing\",\"status\":\"PASS\",\"types\":13,\"detached_unsplit_mirror\":true,\"backlog_replay\":false}\n";
}
