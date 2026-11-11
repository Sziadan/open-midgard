void StatusSummary214Replay()
{
    SetCase("status_summary_0214_display_only", "PK24.02",
        "PK24-0214-selected-20080910-signed-display",
        "production default router -> HandleGmStatusSummary -> UIWindowMgr::SendMsg -> chat events");

    CHARACTER_INFO character{};
    character.Str = 42;
    character.sppoint = 321;
    g_session.SetSelectedCharacterAppearance(character);

    const std::array<int, 6> oldPlus{
        g_session.m_plusStr, g_session.m_plusAgi, g_session.m_plusVit,
        g_session.m_plusInt, g_session.m_plusDex, g_session.m_plusLuk};
    const std::array<int, 6> oldCost{
        g_session.m_standardStr, g_session.m_standardAgi, g_session.m_standardVit,
        g_session.m_standardInt, g_session.m_standardDex, g_session.m_standardLuk};
    const std::array<int, 14> oldCombat{
        g_session.m_attPower, g_session.m_refiningPower, g_session.m_maxMatkPower,
        g_session.m_minMatkPower, g_session.m_itemDefPower, g_session.m_plusDefPower,
        g_session.m_mdefPower, g_session.m_plusMdefPower, g_session.m_hitSuccessValue,
        g_session.m_avoidSuccessValue, g_session.m_plusAvoidSuccessValue,
        g_session.m_criticalSuccessValue, g_session.m_aspd, g_session.m_plusAspd};
    const CharacterVitals oldVitals = g_session.GetCharacterVitals();
    const CHARACTER_INFO* selected = g_session.GetSelectedCharacterInfo();
    const bool hadSelected = selected != nullptr;
    const CHARACTER_INFO oldSelected = hadSelected ? *selected : CHARACTER_INFO{};
    const auto oldEvents = g_windowMgr.GetChatEvents();

    CPlayer player;
    player.m_Hp = 321;
    player.m_MaxHp = 654;
    player.m_Sp = 123;
    player.m_MaxSp = 456;
    player.m_Str = 71;
    player.m_Agi = 72;
    player.m_Vit = 73;
    player.m_Int = 74;
    player.m_Dex = 75;
    player.m_Luk = 76;
    const std::array<u32, 10> oldActor{
        player.m_Hp, player.m_MaxHp, player.m_Sp, player.m_MaxSp,
        player.m_Str, player.m_Agi, player.m_Vit, player.m_Int, player.m_Dex, player.m_Luk};
    CWorld world;
    world.m_player = &player;
    CGameMode mode;
    mode.m_world = &world;

    std::array<u8, 42> frame{};
    frame[0] = 0x14;
    frame[1] = 0x02;
    const std::array<u8, 12> statsAndCosts{12, 1, 34, 2, 56, 3, 78, 4, 90, 5, 111, 6};
    std::copy(statsAndCosts.begin(), statsAndCosts.end(), frame.begin() + 2);
    const std::array<s16, 14> signedCombat{
        -2, -1234, 32767, -32768, -5, 6, -7, 8, -9, 10, -11, 12, -13, -14};
    for (size_t i = 0; i < signedCombat.size(); ++i) {
        const u16 value = static_cast<u16>(signedCombat[i]);
        frame[14 + i * 2] = static_cast<u8>(value & 0xff);
        frame[15 + i * 2] = static_cast<u8>(value >> 8);
    }

    g_windowMgr.ClearChatEvents();
    CGameModePacketRouter router;
    RegisterDefaultGameModePacketHandlers(router);
    PacketView packet{0x0214, static_cast<u16>(frame.size()), frame.data()};
    Require(router.Dispatch(mode, packet), "production router did not dispatch 0x0214");

    const auto& events = g_windowMgr.GetChatEvents();
    Require(events.size() == 5, "0x0214 did not emit exactly five Ref status-summary lines");
    const std::array<u32, 5> expectedColors{0x00ffff00u, 0x0037ff00u, 0x00ffff00u, 0x0037ff00u, 0x00ffff00u};
    const auto formatLine = [](const char* format, auto... values) {
        std::array<char, 256> buffer{};
        const int count = std::snprintf(buffer.data(), buffer.size(), format, values...);
        Require(count >= 0 && static_cast<size_t>(count) < buffer.size(), "expected status-summary line overflowed");
        return std::string(buffer.data(), static_cast<size_t>(count));
    };
    const std::array<std::string, 5> expected{
        formatLine("             STR=%3d      AGI=%3d      VIT=%3d      INT=%3d      DEX=%3d      LUK=%d", 12, 34, 56, 78, 90, 111),
        formatLine("standard STR=%3d      AGI=%3d      VIT=%3d      INT=%3d      DEX=%3d      LUK=%2d", 1, 2, 3, 4, 5, 6),
        formatLine("    attPower=%3d    refiningPow=%3d    MAXmatPow=%3d    MINmatPower=%3d       ASPD=%3d", -2, -1234, 32767, -32768, -13),
        formatLine("itemdefPow=%3d     plusdefPow=%3d    mdefPower=%3d    plusmdefPow=%3d    plusASPD=%3d", -5, 6, -7, 8, -14),
        formatLine("hitSuccessVal=%3d    avoidSuccessVal=%3d    plusAvoidSuccessValue=%3d", -9, 10, -11),
    };
    for (size_t i = 0; i < expected.size(); ++i) {
        Require(events[i].text == expected[i], "0x0214 chat output differed from exact Ref line " + std::to_string(i + 1));
        Require(events[i].color == expectedColors[i], "0x0214 status-summary line color differed from Ref");
    }
    Require(events[2].text.find("attPower= -2") != std::string::npos
        && events[2].text.find("refiningPow=-1234") != std::string::npos
        && events[2].text.find("MINmatPower=-32768") != std::string::npos
        && events[4].text.find("hitSuccessVal= -9") != std::string::npos,
        "0x0214 signed combat fields were not rendered as signed values");

    Require(g_session.GetCharacterVitals().hp == oldVitals.hp
        && g_session.GetCharacterVitals().maxHp == oldVitals.maxHp
        && g_session.GetCharacterVitals().sp == oldVitals.sp
        && g_session.GetCharacterVitals().maxSp == oldVitals.maxSp,
        "0x0214 changed authoritative session HP/SP");
    Require(std::array<int, 6>{g_session.m_plusStr, g_session.m_plusAgi, g_session.m_plusVit,
            g_session.m_plusInt, g_session.m_plusDex, g_session.m_plusLuk} == oldPlus,
        "0x0214 changed session primary-stat modifiers");
    Require(std::array<int, 6>{g_session.m_standardStr, g_session.m_standardAgi, g_session.m_standardVit,
            g_session.m_standardInt, g_session.m_standardDex, g_session.m_standardLuk} == oldCost,
        "0x0214 changed session stat-point costs");
    Require(std::array<int, 14>{g_session.m_attPower, g_session.m_refiningPower,
            g_session.m_maxMatkPower, g_session.m_minMatkPower, g_session.m_itemDefPower,
            g_session.m_plusDefPower, g_session.m_mdefPower, g_session.m_plusMdefPower,
            g_session.m_hitSuccessValue, g_session.m_avoidSuccessValue,
            g_session.m_plusAvoidSuccessValue, g_session.m_criticalSuccessValue,
            g_session.m_aspd, g_session.m_plusAspd} == oldCombat,
        "0x0214 changed session combat-status state");
    Require(std::array<u32, 10>{player.m_Hp, player.m_MaxHp, player.m_Sp, player.m_MaxSp,
            player.m_Str, player.m_Agi, player.m_Vit, player.m_Int, player.m_Dex, player.m_Luk} == oldActor,
        "0x0214 changed local actor HP/SP or primary stats");
    if (hadSelected) {
        const CHARACTER_INFO* after = g_session.GetSelectedCharacterInfo();
        Require(after && after->Str == oldSelected.Str && after->Agi == oldSelected.Agi
            && after->Vit == oldSelected.Vit && after->Int == oldSelected.Int
            && after->Dex == oldSelected.Dex && after->Luk == oldSelected.Luk
            && after->sppoint == oldSelected.sppoint,
            "0x0214 changed selected-character stats or stat points");
    }

    g_windowMgr.ClearChatEvents();
    for (const auto& event : oldEvents) {
        g_windowMgr.PushChatEvent(event.text.c_str(), event.color, event.channel, event.tick, event.source);
    }
    mode.m_world = nullptr;
    std::cout << "{\"case_id\":\"status_summary_0214_display_only\",\"contract_id\":\"PK24.02\",\"fixture_id\":\"PK24-0214-selected-20080910-signed-display\",\"input_packet_hex\":\"" << Hex(std::vector<u8>(frame.begin(), frame.end()))
        << "\",\"chat_line_count\":5,\"signed_negative_values_preserved\":true,\"authoritative_resources_and_stats_unchanged\":true,\"outcome\":\"PASS\"}" << std::endl;
}
