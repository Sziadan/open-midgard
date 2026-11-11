void PetFeedActionReplay()
{
    SetCase("pet_feed_action_result", "PK27.PET-FEED", "01a3-and-01aa-production-routing",
        "production packet loop/router, 01A9 socket output, missing-food notice and actor message21");
    CWorld world;
    CGameMode mode;
    TestConnection input;
    auto& router = GetProductionGameModePacketRouter();
    class RecordingPet final : public CPc {
    public:
        int message = -1;
        msgparam_t arg1 = 0, arg2 = 0, arg3 = 0;
        void SendMsg(CGameObject*, int msg, msgparam_t a, msgparam_t b, msgparam_t c) override
        {
            message = msg; arg1 = a; arg2 = b; arg3 = c;
        }
    } actor;
    struct Restore {
        CWorld& world; CGameMode& mode; RecordingPet& actor; CGameModePacketRouter& router;
        ro::pet::Status pet = g_session.m_petStatus;
        std::vector<UIChatEvent> events = g_windowMgr.GetChatEvents();
        int sex = g_session.m_sex;
        int noTalk = g_session.m_isNoTalkMsg, noTalk2 = g_session.m_isNoTalkMsg2;
        std::map<u32, std::string> nameCache;
        std::string lastChat;
        int sameChatRepeat = 0, recordChatNum = 0;
        std::array<std::string, 11> recordChat;
        std::array<u32, 11> recordChatTime{};
        std::filesystem::path cwd = std::filesystem::current_path(), temporary;
        CMode* current = SkillPacketReplayAccess::Current();
        int currentType = SkillPacketReplayAccess::CurrentType();
        Restore(CWorld& w, CGameMode& m, RecordingPet& a, CGameModePacketRouter& r)
            : world(w), mode(m), actor(a), router(r), nameCache(m.m_characterNameCache), lastChat(m.m_lastChat),
              sameChatRepeat(m.m_sameChatRepeatCnt), recordChatNum(m.m_recordChatNum)
        {
            std::copy(std::begin(m.m_recordChat), std::end(m.m_recordChat), recordChat.begin());
            std::copy(std::begin(m.m_recordChatTime), std::end(m.m_recordChatTime), recordChatTime.begin());
        }
        ~Restore() {
            world.UnregisterActor(&actor);
            actor.ClearChatBubbleText();
            mode.m_world = nullptr;
            mode.m_characterNameCache = std::move(nameCache);
            mode.m_lastChat = std::move(lastChat);
            mode.m_sameChatRepeatCnt = sameChatRepeat;
            mode.m_recordChatNum = recordChatNum;
            std::copy(recordChat.begin(), recordChat.end(), std::begin(mode.m_recordChat));
            std::copy(recordChatTime.begin(), recordChatTime.end(), std::begin(mode.m_recordChatTime));
            SkillPacketReplayAccess::Restore(current, currentType);
            g_session.m_petStatus = pet;
            g_session.m_sex = sex;
            g_session.m_isNoTalkMsg = noTalk; g_session.m_isNoTalkMsg2 = noTalk2;
            g_windowMgr.ClearChatEvents();
            for (const auto& event : events)
                g_windowMgr.PushChatEvent(event.text.c_str(), event.color, event.channel, event.tick, event.source);
            std::filesystem::current_path(cwd);
            std::error_code ec;
            if (!temporary.empty() && temporary.parent_path() == std::filesystem::temp_directory_path()
                && temporary.filename().string().find("ro-pet-feed-") == 0)
                std::filesystem::remove_all(temporary, ec);
            router.Clear(); RegisterDefaultGameModePacketHandlers(router);
        }
    } restore(world, mode, actor, router);

    router.Clear(); RegisterDefaultGameModePacketHandlers(router);
    mode.m_world = &world;
    SkillPacketReplayAccess::SetCurrent(&mode);
    actor.m_gid = 0x12345678; actor.m_job = 1002; actor.m_isPc = 0;
    world.RegisterActor(&actor);
    g_session.m_petStatus = {};
    g_session.m_petStatus.job = 1002;
    g_session.m_sex = 1;
    g_windowMgr.ClearChatEvents();

    const auto put16 = [](std::vector<u8>& p, size_t o, u16 v) {
        p[o] = static_cast<u8>(v); p[o + 1] = static_cast<u8>(v >> 8);
    };
    const auto put32 = [](std::vector<u8>& p, size_t o, u32 v) {
        for (int i = 0; i < 4; ++i) p[o + i] = static_cast<u8>(v >> (8 * i));
    };
    const auto dispatch = [&](const std::vector<u8>& bytes) {
        Require(input.QueueBytes(bytes), "pet feed replay input queue failed");
        const auto result = ProcessGameModePacketLoop(mode, input, router, 1);
        Require(result.packetsProcessed == 1 && result.packetsDispatched == 1
            && input.BufferedBytes() == 0, "pet feed replay frame did not dispatch exactly once");
    };
    const auto statusChange = [&](u8 subtype, u32 value) {
        std::vector<u8> p(11); put16(p, 0, 0x01a4); p[2] = subtype;
        put32(p, 3, actor.m_gid); put32(p, 7, value); return p;
    };
    const auto feedResult = [&](u8 result, u16 food) {
        std::vector<u8> p(5); put16(p, 0, 0x01a3); p[2] = result; put16(p, 3, food); return p;
    };
    const auto checkNoOutput = [](SOCKET socketValue) {
        Require(SelectSocket(socketValue, true, false, std::chrono::milliseconds(25)) == 0,
            "pet feed path emitted unexpected outbound bytes");
    };

    CRagConnection* const output = CRagConnection::instance();
    output->Disconnect();
    {
        LoopbackPeer peer(*output);
        // Subtype2 snapshots old fullness before replacing it; subtype1 sets
        // relationship. These states select voice 50201 and feeding emotion 15.
        g_session.m_petStatus.fullness = 20;
        dispatch(statusChange(2, 50));
        dispatch(statusChange(1, 950));
        Require(g_session.m_petStatus.oldFullness == 20 && g_session.m_petStatus.fullness == 50
            && g_session.m_petStatus.relationship == 950 && g_session.m_petStatus.job == 1002,
            "pet feed fixture did not establish source fullness/history/relationship/job");
        dispatch(feedResult(1, 0));
        const std::vector<u8> expected{
            0xa9,0x01,0x19,0xc4,0x00,0x00, // 50201
            0xa9,0x01,0x0f,0x00,0x00,0x00  // emotion 15
        };
        Require(peer.ReceiveExact(expected.size()) == expected,
            "successful pet feed did not emit exact voice and feeding-emotion frames");

        // Voice IDs are gated below relationship 900; emotion remains visible.
        dispatch(statusChange(1, 890));
        dispatch(feedResult(1, 0));
        const std::vector<u8> emotionOnly{0xa9,0x01,0x12,0x00,0x00,0x00};
        Require(peer.ReceiveExact(emotionOnly.size()) == emotionOnly,
            "low-relationship feed did not emit only the emotion frame");

        const auto oldEvents = g_windowMgr.GetChatEvents().size();
        dispatch(feedResult(0, 501));
        Require(g_windowMgr.GetChatEvents().size() > oldEvents,
            "failed pet feed did not produce the missing-food chat notice");
        checkNoOutput(peer.peer.value);

        std::vector<u8> action(10); put16(action, 0, 0x01aa);
        put32(action, 2, actor.m_gid); put32(action, 6, static_cast<u32>(-7));
        actor.message = -1; dispatch(action);
        Require(actor.message == 21 && actor.arg1 == -7 && actor.arg2 == 0 && actor.arg3 == 0,
            "01AA direct action did not preserve signed action into actor message21");
        checkNoOutput(peer.peer.value);
        put32(action, 2, 0x87654321); actor.message = -1; dispatch(action);
        Require(actor.message == -1, "01AA missing actor was not ignored");
        checkNoOutput(peer.peer.value);

        // GetPetSpeech caches the first loaded table, so make the isolated
        // fixture current before its first lookup (after direct-action cases).
        restore.temporary = std::filesystem::temp_directory_path()
            / ("ro-pet-feed-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount()));
        Require(std::filesystem::create_directory(restore.temporary), "pet speech fixture directory already exists");
        std::filesystem::current_path(restore.temporary);
        {
            std::ofstream table("PetTalkTable.xml", std::ios::binary);
            table << "<monster_talk_table><poring><bit_hungry><feeding>Hungry Poring speaks.</feeding>"
                     "</bit_hungry></poring></monster_talk_table>";
            Require(static_cast<bool>(table), "pet speech fixture table write failed");
        }
        g_session.m_petStatus.actorId = actor.m_gid;
        g_session.m_petStatus.name = "Poring";
        mode.m_characterNameCache[actor.m_gid] = "Poring";
        g_session.m_isNoTalkMsg = 0; g_session.m_isNoTalkMsg2 = 0;
        const auto petAction = [&](int value) {
            std::vector<u8> p(10); put16(p, 0, 0x01aa); put32(p, 2, actor.m_gid);
            put32(p, 6, static_cast<u32>(value)); return p;
        };
        g_windowMgr.ClearChatEvents();
        dispatch(petAction(50201));
        Require(actor.GetChatBubbleText() == "Hungry Poring speaks.",
            "50000-series pet voice did not set the actor speech bubble");
        Require(!g_windowMgr.GetChatEvents().empty()
            && g_windowMgr.GetChatEvents().back().text == "Poring : Hungry Poring speaks.",
            "50000-series pet voice did not enter chat under the cached speaker name");
        g_windowMgr.ClearChatEvents();
        dispatch(petAction(90201));
        Require(actor.GetChatBubbleText() == "Hungry Poring speaks."
            && !g_windowMgr.GetChatEvents().empty()
            && g_windowMgr.GetChatEvents().back().text == "Poring : Hungry Poring speaks.",
            "90000-series pet voice branch did not set bubble and chat speech");
        g_windowMgr.ClearChatEvents();
        g_session.m_isNoTalkMsg = 1;
        dispatch(petAction(50201));
        Require(actor.GetChatBubbleText() == "Hungry Poring speaks." && g_windowMgr.GetChatEvents().empty(),
            "muted pet speech suppressed bubble or still wrote to chat");
        checkNoOutput(peer.peer.value);
    }
    output->Disconnect();
    std::cout << "{\"case_id\":\"pet_feed_action_result\",\"contract_id\":\"PK27.PET-FEED\",\"outcome\":\"PASS\",\"assertions\":\"01A4 fullness/history and relationship feed selection; exact 01A9 voice+emotion and low-relationship emotion-only output; 01A3 failure notice without bytes; 01AA signed actor message21 and missing-actor no-op\"}" << std::endl;
}
