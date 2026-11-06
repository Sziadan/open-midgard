#include <optional>

void ChatPrimarySpeechReplay()
{
    SetCase("chat_primary_speech_source_metadata", "PK09.21",
        "PK09.21-selected-no-room-speech-producers",
        "ProcessGameModePacketLoop -> production router -> 008D/008E/0109/017F chat handlers");

    CGameMode mode;
    TestConnection connection;
    CGameModePacketRouter& router = GetProductionGameModePacketRouter();
    struct Restore {
        std::vector<UIChatEvent> events{g_windowMgr.GetChatEvents()};
        std::list<u32> excludedAids{g_session.m_exAidList};
        std::array<bool, 13> battleOptions = g_session.m_battleMsgOnOffSet;
        std::map<u32, u32> aidTicks;
        std::array<std::string, 11> recordChat;
        std::array<u32, 11> recordChatTimes{};
        CGameMode& mode;
        CMode* currentMode;
        int currentModeType;
        int chatSticky, managerChatShow, managerChatStatus, subChatSTEnabled, subChatBTEnabled;
        int noTalk, noTalk2, recordChatCount, repeatCount;
        u32 aid, gid;
        ServiceType serviceType;
        std::string lastChat;
        explicit Restore(CGameMode& m) : mode(m), currentMode(SkillPacketReplayAccess::Current()),
            currentModeType(SkillPacketReplayAccess::CurrentType()),
            chatSticky(g_windowMgr.m_chatWndStickOn), managerChatShow(g_windowMgr.m_chatWndShow),
            managerChatStatus(g_windowMgr.m_chatWndStatus), subChatSTEnabled(g_windowMgr.m_onStSubChat),
            subChatBTEnabled(g_windowMgr.m_onBtSubChat), noTalk(g_session.m_isNoTalkMsg),
            noTalk2(g_session.m_isNoTalkMsg2), recordChatCount(m.m_recordChatNum),
            repeatCount(m.m_sameChatRepeatCnt), aid(g_session.m_aid), gid(g_session.m_gid),
            serviceType(g_serviceType), lastChat(m.m_lastChat)
        {
            aidTicks = m.m_aidList;
            for (size_t i = 0; i < recordChat.size(); ++i) {
                recordChat[i] = m.m_recordChat[i];
                recordChatTimes[i] = m.m_recordChatTime[i];
            }
        }
        ~Restore() {
            g_windowMgr.RemoveAllWindows();
            g_windowMgr.m_chatWndStickOn = chatSticky;
            g_windowMgr.m_chatWndShow = managerChatShow;
            g_windowMgr.m_chatWndStatus = managerChatStatus;
            g_windowMgr.m_onStSubChat = subChatSTEnabled;
            g_windowMgr.m_onBtSubChat = subChatBTEnabled;
            g_windowMgr.ClearChatEvents();
            for (const UIChatEvent& event : events)
                g_windowMgr.PushChatEvent(event.text.c_str(), event.color, event.channel, event.tick, event.source);
            g_session.m_exAidList = excludedAids;
            g_session.m_battleMsgOnOffSet = battleOptions;
            g_session.m_isNoTalkMsg = noTalk;
            g_session.m_isNoTalkMsg2 = noTalk2;
            g_session.m_aid = aid;
            g_session.m_gid = gid;
            g_serviceType = serviceType;
            mode.m_aidList = aidTicks;
            mode.m_recordChatNum = recordChatCount;
            mode.m_sameChatRepeatCnt = repeatCount;
            mode.m_lastChat = lastChat;
            for (size_t i = 0; i < recordChat.size(); ++i) {
                mode.m_recordChat[i] = recordChat[i];
                mode.m_recordChatTime[i] = recordChatTimes[i];
            }
            SkillPacketReplayAccess::Restore(currentMode, currentModeType);
            GetProductionGameModePacketRouter().Clear();
            RegisterDefaultGameModePacketHandlers(GetProductionGameModePacketRouter());
        }
    } restore(mode);

    // IsNameYellow reads the parsed AID vector directly; keep this fixture
    // independent of clientinfo XML and restore the original vector afterward.
    struct YellowAidOverride {
        std::vector<u32> original{s_dwYellowAID};
        void SetOnly(std::optional<u32> aid) {
            s_dwYellowAID.clear();
            if (aid) s_dwYellowAID.push_back(*aid);
        }
        ~YellowAidOverride() {
            s_dwYellowAID = std::move(original);
        }
    } yellowOverride;
    g_windowMgr.RemoveAllWindows();
    g_windowMgr.ClearChatEvents();
    router.Clear();
    RegisterDefaultGameModePacketHandlers(router);
    SkillPacketReplayAccess::SetCurrent(&mode);
    g_session.m_aid = 0x13572468;
    g_session.m_gid = 0x24681357;
    g_session.m_exAidList.clear();
    g_session.m_battleMsgOnOffSet.fill(true);
    g_session.m_isNoTalkMsg = 0;
    g_session.m_isNoTalkMsg2 = 0;
    g_windowMgr.m_onStSubChat = true;
    g_windowMgr.m_onBtSubChat = true;
    g_windowMgr.m_chatWndStickOn = true;
    g_windowMgr.EnsureChatWindowVisible();
    UINewChatWnd* const chatWnd = g_windowMgr.m_chatWnd;
    Require(chatWnd != nullptr, "managed chat window was not created");

    const auto put16 = [](std::vector<u8>& p, size_t offset, u16 value) {
        p[offset] = static_cast<u8>(value);
        p[offset + 1] = static_cast<u8>(value >> 8);
    };
    const auto put32 = [](std::vector<u8>& p, size_t offset, u32 value) {
        for (int i = 0; i < 4; ++i) p[offset + static_cast<size_t>(i)] = static_cast<u8>(value >> (8 * i));
    };
    const auto variableFrame = [&](u16 id, size_t payloadOffset, const std::string& payload, u32 speaker = 0) {
        std::vector<u8> packet(payloadOffset + payload.size());
        put16(packet, 0, id);
        put16(packet, 2, static_cast<u16>(packet.size()));
        if (payloadOffset == 8) put32(packet, 4, speaker);
        std::copy(payload.begin(), payload.end(), packet.begin() + static_cast<std::ptrdiff_t>(payloadOffset));
        return packet;
    };
    const auto dispatch = [&](const std::vector<u8>& packet, const char* name) {
        Require(connection.QueueBytes(packet), std::string("queue failed: ") + name);
        const auto result = ProcessGameModePacketLoop(mode, connection, router, 1);
        Require(result.packetsProcessed == 1 && result.packetsDispatched == 1 && connection.BufferedBytes() == 0,
            std::string("production route failed: ") + name);
    };
    const auto requireEvent = [&](size_t index, ro::chat::NoticeType type, u32 color) {
        const auto& events = g_windowMgr.GetChatEvents();
        Require(events.size() == index + 1, "unexpected chat event count");
        const auto& event = events.back();
        Require(event.source.route == ro::chat::NoticeRoute::Normal && event.source.type == type
            && event.source.talker.empty(), "speech source metadata differs");
        Require(event.color == color, "speech UI color differs");
    };
    const auto requireRows = [&](size_t normal, size_t command) {
        Require(chatWnd->GetSourceHistory(false).size() == normal
            && chatWnd->GetSourceHistory(true).size() == command,
            "ST/BT source histories differ");
    };

    chatWnd->SetState(7);
    chatWnd->SetState(0);
    Require(chatWnd->GetExpansionState() == 0, "chat did not enter collapsed state");
    g_serviceType = ServiceKorea;

    // Normal speech retains route/type and does not open a collapsed chat.
    dispatch(variableFrame(0x008D, 8, "Speaker : ordinary", 0x11000001), "normal speech");
    requireEvent(0, ro::chat::NoticeType::Chat, 0x00FFFFFFu);
    requireRows(1, 1);
    Require(chatWnd->GetExpansionState() == 0, "ordinary speech expanded collapsed chat");

    // Either mute suppresses ordinary speech.
    g_session.m_isNoTalkMsg = 1;
    dispatch(variableFrame(0x008D, 8, "Muted : first", 0x11000002), "first ordinary-chat mute");
    Require(g_windowMgr.GetChatEvents().size() == 1, "first mute failed to suppress ordinary speech");
    g_session.m_isNoTalkMsg = 0;
    g_session.m_isNoTalkMsg2 = 1;
    dispatch(variableFrame(0x008D, 8, "Muted : second", 0x11000003), "second ordinary-chat mute");
    Require(g_windowMgr.GetChatEvents().size() == 1, "second mute failed to suppress ordinary speech");
    g_session.m_isNoTalkMsg = 1;

    // Specific AIDs are red and use type 0, bypassing both ordinary mutes.
    g_serviceType = ServiceIndonesia;
    yellowOverride.SetOnly(std::nullopt);
    const u32 specificAids[] = {100001, 100064, 100065, 100068, 100075, 100076, 441470, 441474, 441487, 540397};
    size_t eventCount = 1;
    size_t stRows = 1;
    size_t btRows = 1;
    for (u32 aid : specificAids) {
        dispatch(variableFrame(0x008D, 8, "Specific : red", aid), "specific-color AID");
        ++eventCount;
        ++stRows;
        requireEvent(eventCount - 1, ro::chat::NoticeType::None, 0x00FF0000u);
        requireRows(stRows, btRows);
        Require(chatWnd->GetExpansionState() == 0, "specific-color notice expanded collapsed chat");
    }

    // One miss and one valid AID under the wrong service tag are muted.
    dispatch(variableFrame(0x008D, 8, "Specific miss : muted", 100077), "specific-color boundary miss");
    Require(g_windowMgr.GetChatEvents().size() == eventCount, "specific-color boundary miss was displayed");
    g_serviceType = ServiceKorea;
    dispatch(variableFrame(0x008D, 8, "Wrong service : muted", 100001), "specific-color wrong service");
    Require(g_windowMgr.GetChatEvents().size() == eventCount, "specific-color AID displayed for wrong service");

    // Yellow has precedence over Indonesia-specific red and bypasses both mutes.
    yellowOverride.SetOnly(100001u);
    g_serviceType = ServiceIndonesia;
    dispatch(variableFrame(0x008D, 8, "Yellow : cyan", 100001), "yellow over specific precedence");
    ++eventCount;
    ++stRows;
    ++btRows;
    requireEvent(eventCount - 1, ro::chat::NoticeType::Chat, 0x00FFFF00u);
    requireRows(stRows, btRows);
    Require(chatWnd->GetExpansionState() == 0, "yellow speech expanded collapsed chat");
    g_serviceType = ServiceThai;
    dispatch(variableFrame(0x008D, 8, "Yellow Thai : cyan", 100001), "Thai yellow speech");
    ++eventCount;
    ++stRows;
    ++btRows;
    requireEvent(eventCount - 1, ro::chat::NoticeType::Chat, 0x0000FFFFu);
    requireRows(stRows, btRows);
    g_serviceType = ServiceGermany;
    dispatch(variableFrame(0x008D, 8, "Yellow German : orange", 100001), "German yellow speech");
    ++eventCount;
    ++stRows;
    ++btRows;
    requireEvent(eventCount - 1, ro::chat::NoticeType::Chat, 0x00FF8D1Du);
    requireRows(stRows, btRows);
    yellowOverride.SetOnly(std::nullopt);

    // Player speech is green/type Chat; a local party notice is orange and does not expand.
    g_serviceType = ServiceKorea;
    g_session.m_isNoTalkMsg = 0;
    g_session.m_isNoTalkMsg2 = 0;
    dispatch(variableFrame(0x008E, 4, "Player : words"), "player speech");
    ++eventCount;
    ++stRows;
    ++btRows;
    requireEvent(eventCount - 1, ro::chat::NoticeType::Chat, 0x0000FF00u);
    requireRows(stRows, btRows);
    chatWnd->SetState(0);
    dispatch(variableFrame(0x0109, 8, "Self party", g_session.m_aid), "self party speech");
    ++eventCount;
    ++stRows;
    ++btRows;
    requireEvent(eventCount - 1, ro::chat::NoticeType::Party, 0x00FFC800u);
    requireRows(stRows, btRows);
    Require(chatWnd->GetExpansionState() == 0, "self party speech expanded collapsed chat");

    // Remote party speech explicitly expands via manager message 3 before appending.
    chatWnd->SetState(0);
    dispatch(variableFrame(0x0109, 8, "Remote party", 0x22000001), "remote party speech");
    ++eventCount;
    ++stRows;
    ++btRows;
    requireEvent(eventCount - 1, ro::chat::NoticeType::Party, 0x00FFC8C8u);
    requireRows(stRows, btRows);
    Require(chatWnd->GetExpansionState() == 1, "remote party message did not expand collapsed chat");

    // Guild payload forces its final byte to NUL; type Guild uses the green tint.
    chatWnd->SetState(0);
    const size_t beforeGuild = g_windowMgr.GetChatEvents().size();
    dispatch(variableFrame(0x017F, 4, "Guild!"), "guild chat final-byte termination");
    ++eventCount;
    ++stRows;
    ++btRows;
    requireEvent(eventCount - 1, ro::chat::NoticeType::Guild, 0x00B4FFB4u);
    requireRows(stRows, btRows);
    Require(g_windowMgr.GetChatEvents().back().text == "Guild",
        "guild chat did not force the final payload byte to NUL");
    Require(chatWnd->GetExpansionState() == 0, "guild chat expanded collapsed chat");

    g_session.m_isNoTalkMsg = 0;
    g_session.m_isNoTalkMsg2 = 1;
    dispatch(variableFrame(0x017F, 4, "Muted"), "guild second mute");
    Require(g_windowMgr.GetChatEvents().size() == beforeGuild + 1,
        "second mute failed to suppress guild speech");
    g_session.m_isNoTalkMsg = 1;
    g_session.m_isNoTalkMsg2 = 0;
    dispatch(variableFrame(0x017F, 4, "Allowed"), "guild first mute does not suppress");
    ++eventCount;
    ++stRows;
    ++btRows;
    requireEvent(eventCount - 1, ro::chat::NoticeType::Guild, 0x00B4FFB4u);

    // Exclusion takes precedence over all speaker color/mute rules.
    const size_t beforeExcluded = g_windowMgr.GetChatEvents().size();
    g_session.m_isNoTalkMsg = 0;
    g_session.m_exAidList.push_back(0x33000001);
    dispatch(variableFrame(0x008D, 8, "Excluded", 0x33000001), "excluded AID speech");
    Require(g_windowMgr.GetChatEvents().size() == beforeExcluded,
        "excluded AID speech reached the chat event stream");

    std::cout << "{\"case_id\":\"chat_primary_speech_source_metadata\",\"contract_id\":\"PK09.21\",\"fixture_id\":\"PK09.21-selected-no-room-speech-producers\",\"production_entry\":\"ProcessGameModePacketLoop -> production router -> 008D/008E/0109/017F chat handlers\",\"specific_aids_tested\":10,\"specific_wrong_service_checked\":true,\"yellow_precedence_checked\":true,\"normal_mute_boundaries_checked\":true,\"remote_party_expansion_checked\":true,\"guild_termination_and_mute_checked\":true,\"bubble_actor_setup\":\"omitted: no bounded actor fixture\",\"outcome\":\"PASS\"}\n";
}
