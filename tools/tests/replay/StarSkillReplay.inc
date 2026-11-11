namespace {
void PutStarReplayU16(std::vector<u8>& bytes, size_t offset, u16 value)
{
    bytes[offset] = static_cast<u8>(value);
    bytes[offset + 1] = static_cast<u8>(value >> 8);
}

void DispatchStarReplayFrame(CGameModePacketRouter& router, CGameMode& mode,
    const std::vector<u8>& bytes, u16 id)
{
    PacketView packet{};
    int consumed = 0;
    Require(TryReadPacket(bytes.data(), static_cast<int>(bytes.size()), packet, consumed),
        "Star packet rejected by production frame reader");
    Require(consumed == static_cast<int>(bytes.size()) && packet.packetId == id,
        "Star packet boundary or opcode mismatch");
    Require(router.Dispatch(mode, packet), "production router did not dispatch Star packet");
}

void ClickStarReplayButton(UISkillSelectWnd& window, const UISkillSelectWnd::DisplayButton& button)
{
    const int x = button.x + button.width / 2;
    const int y = button.y + button.height / 2;
    window.OnLBtnDown(x, y);
    window.OnLBtnUp(x, y);
}
}

void StarSkillProductionReplay()
{
    constexpr const char* fixtureId = "PK26-020E-star-skill-and-0253-confirmation";
    SetCase("star_skill_production_replay", "production_router_replay", fixtureId,
        "TryReadPacket -> production 020E/0253 handlers -> star notice/confirmation -> 0254 sender");

    CMode* const previousMode = SkillPacketReplayAccess::Current();
    const int previousModeType = SkillPacketReplayAccess::CurrentType();
    const std::string previousPlayerName(g_session.m_playerName);
    const auto previousEffects = g_session.m_isEffectOn;
    std::snprintf(g_session.m_playerName, sizeof(g_session.m_playerName), "%s", "ReplayStar");
    CWorld world;
    CPlayer player;
    world.m_player = &player;
    g_session.m_isEffectOn = 1;
    CGameMode mode;
    mode.m_world = &world;
    SkillPacketReplayAccess::SetCurrent(&mode);
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    CGameModePacketRouter& router = GetProductionGameModePacketRouter();
    router.Clear();
    RegisterDefaultGameModePacketHandlers(router);

    // Each supported server result must replace the visible notice and preserve
    // its production color, font, and top-center placement.
    struct NoticeCase { u8 result; u8 star; int effect; };
    const std::array<NoticeCase, 6> notices = {{{0, 0, 0}, {1, 1, 543}, {10, 2, 0}, {11, 0, 543}, {20, 37, 543}, {30, 0, 432}}};
    constexpr char target[] = "abcdefghijklmnopqrstuvwx";
    std::string previousNoticeText;
    for (const NoticeCase sample : notices) {
        std::vector<u8> frame(32, 0);
        frame[0] = 0x0E; frame[1] = 0x02;
        if (sample.result != 11) {
            std::memcpy(frame.data() + 2, target, sizeof(target) - 1);
        }
        PutStarReplayU16(frame, 26, 1002);
        frame[30] = sample.star;
        frame[31] = sample.result;
        const size_t previousEffectCount = player.m_effectList.size();
        DispatchStarReplayFrame(router, mode, frame, 0x020E);
        Require(mode.m_starSkillNotice != nullptr, "supported Star result did not show notice");
        Require(!mode.m_starSkillNotice->GetLines().empty(), "Star notice had no rendered text lines");
        Require(mode.m_starSkillNotice->GetFontColor() == 0x00C8C8FF,
            "Star notice font color changed");
        const auto client = g_windowMgr.GetChatClientRect();
        Require(mode.m_starSkillNotice->m_y == 51
            && mode.m_starSkillNotice->m_x == (client.right + client.left - mode.m_starSkillNotice->m_w) / 2,
            "Star notice was not placed at the production top-center y coordinate");
        Require(mode.m_starSkillNoticeTick != 0, "Star notice did not record its replacement tick");
        std::string noticeText;
        for (const auto& line : mode.m_starSkillNotice->GetLines()) {
            noticeText += line;
        }
        Require(noticeText != previousNoticeText, "Star result did not replace the previous notice text");
        previousNoticeText = noticeText;
        if (sample.result != 30) {
            Require(noticeText.find(sample.result == 11 ? g_session.GetJobName(1002) : target) != std::string::npos,
                "Star notice lost the bounded target name or job fallback");
        }
        if (sample.result == 20) {
            Require(noticeText.find("37") != std::string::npos, "TaeKwon mission notice lost progress");
        }
        Require(player.m_effectList.size() == previousEffectCount + (sample.effect != 0)
            && (sample.effect == 0 || player.m_effectList.back()->GetEffectType() == sample.effect),
            "Star result attached an unexpected effect to the player");
    }

    // Unknown results and invalid celestial selectors are ignored without
    // replacing an already visible notice.
    UITransBalloonText* const lastNotice = mode.m_starSkillNotice;
    const u32 lastNoticeTick = mode.m_starSkillNoticeTick;
    for (const auto invalid : {std::pair<u8, u8>{0, 3}, {2, 0}}) {
        std::vector<u8> frame(32, 0);
        frame[0] = 0x0E; frame[1] = 0x02;
        frame[30] = invalid.first;
        frame[31] = invalid.second;
        DispatchStarReplayFrame(router, mode, frame, 0x020E);
        Require(mode.m_starSkillNotice == lastNotice && mode.m_starSkillNoticeTick == lastNoticeTick,
            "invalid Star result/selector replaced the active notice");
    }
    mode.UpdateStarSkillNotice(lastNoticeTick + 20000u);
    Require(mode.m_starSkillNotice == lastNotice, "Star notice expired at the 20-second boundary");
    mode.UpdateStarSkillNotice(lastNoticeTick + 20001u);
    Require(mode.m_starSkillNotice == nullptr, "Star notice survived beyond its 20-second expiry");

    // 0253 opens a confirmation for the offered Sun/Moon/Stars selector.
    std::vector<u8> offer = {0x53, 0x02, 2};
    DispatchStarReplayFrame(router, mode, offer, 0x0253);
    Require(g_windowMgr.m_skillSelectWnd != nullptr, "valid 0253 offer did not open confirmation");
    UISkillSelectWnd* window = g_windowMgr.m_skillSelectWnd;
    UISkillSelectWnd::DisplayData display{};
    Require(window->GetDisplayDataForQt(&display) && display.title == "Feeling"
        && !display.emptyText.empty() && display.rows.empty(),
        "0253 confirmation did not show the production prompt without selection rows");
    Require(window->IsStarPlaceConfirmation(2), "0253 confirmation lost its offered Stars selector");

    // A later valid offer replaces the selector in the same dialog; malformed
    // offers must leave the existing valid selector untouched.
    offer[2] = 1;
    DispatchStarReplayFrame(router, mode, offer, 0x0253);
    Require(g_windowMgr.m_skillSelectWnd == window && window->IsStarPlaceConfirmation(1)
        && !window->IsStarPlaceConfirmation(2), "replacement 0253 offer did not replace selector");
    offer[2] = 3;
    DispatchStarReplayFrame(router, mode, offer, 0x0253);
    Require(g_windowMgr.m_skillSelectWnd == window && window->IsStarPlaceConfirmation(1),
        "invalid 0253 selector replaced the active confirmation");

    // Cancel closes the prompt without producing a client response packet.
    LoopbackPeer peer(*connection);
    Require(window->GetDisplayDataForQt(&display), "could not read Star confirmation buttons");
    ClickStarReplayButton(*window, display.cancelButton);
    Require(g_windowMgr.m_skillSelectWnd == nullptr, "Star confirmation cancel did not close the dialog");
    Require(SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(50)) == 0,
        "Star confirmation cancel emitted a packet");

    // A disconnected acceptance retains the offer so the same selection can
    // be retried; success emits the exact 0254 selector byte.
    offer[2] = 0;
    DispatchStarReplayFrame(router, mode, offer, 0x0253);
    window = g_windowMgr.m_skillSelectWnd;
    Require(window && window->GetDisplayDataForQt(&display), "retry 0253 offer did not open dialog");
    connection->Disconnect();
    ClickStarReplayButton(*window, display.okButton);
    Require(g_windowMgr.m_skillSelectWnd == window && window->IsStarPlaceConfirmation(0),
        "disconnected 0254 send did not preserve confirmation for retry");
    LoopbackPeer retryPeer(*connection);
    Require(window->GetDisplayDataForQt(&display), "could not read retry confirmation buttons");
    ClickStarReplayButton(*window, display.okButton);
    const std::vector<u8> captured = retryPeer.ReceiveExact(3);
    const std::vector<u8> expected = {0x54, 0x02, 0};
    Require(captured == expected && g_windowMgr.m_skillSelectWnd == nullptr,
        "Star acceptance did not send exact 0254 selector and close dialog");

    // Mode exit owns both transient windows and must remove them.
    std::vector<u8> noticeFrame(32, 0);
    noticeFrame[0] = 0x0E; noticeFrame[1] = 0x02; noticeFrame[30] = 0; noticeFrame[31] = 30;
    DispatchStarReplayFrame(router, mode, noticeFrame, 0x020E);
    offer[2] = 2;
    DispatchStarReplayFrame(router, mode, offer, 0x0253);
    Require(mode.m_starSkillNotice && g_windowMgr.m_skillSelectWnd,
        "could not seed Star windows before map exit");
    mode.OnExit();
    Require(mode.m_starSkillNotice == nullptr && g_windowMgr.m_skillSelectWnd == nullptr,
        "map exit did not remove Star notice and confirmation");

    player.m_effectList.clear();
    while (!world.m_gameObjectList.empty()) {
        auto* object = world.m_gameObjectList.front();
        world.m_gameObjectList.pop_front();
        delete object;
    }
    g_session.m_isEffectOn = previousEffects;
    std::snprintf(g_session.m_playerName, sizeof(g_session.m_playerName), "%s", previousPlayerName.c_str());
    SkillPacketReplayAccess::Restore(previousMode, previousModeType);
    router.Clear();
    RegisterDefaultGameModePacketHandlers(router);
    std::cout << "{\"case_id\":\"star_skill_production_replay\",\"contract_id\":\"PK26.020E-0253-0254\",\"fixture_id\":\""
        << fixtureId << "\",\"notice_results\":[0,1,10,11,20,30],\"replacement_and_expiry\":true,\"invalid_inputs_ignored\":true,\"cancel_sent_nothing\":true,\"retry_hex\":\""
        << Hex(captured) << "\",\"map_exit_cleanup\":true,\"outcome\":\"PASS\"}\n";
}
