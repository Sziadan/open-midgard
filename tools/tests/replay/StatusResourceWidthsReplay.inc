void StatusResourceWidthsReplay()
{
    SetCase("status_resource_widths", "PK24.02", "PK24-selected-status-and-client-widths",
        "production 0141/00BD/00BC/00BE/00B0/00B1 framing and status handlers");

    g_session.SetSelectedCharacterAppearance(CHARACTER_INFO{});
    CWorld world;
    CPlayer player;
    CGameMode mode;
    world.m_player = &player;
    mode.m_world = &world;
    CGameModePacketRouter router;
    RegisterDefaultGameModePacketHandlers(router);
    const auto write = [](auto& bytes, std::size_t offset, auto value) {
        using Wire = std::make_unsigned_t<decltype(value)>;
        const auto bits = static_cast<Wire>(value);
        for (std::size_t byte = 0; byte < sizeof(value); ++byte) {
            bytes[offset + byte] = static_cast<u8>(bits >> (8 * byte));
        }
    };
    const auto dispatch = [&](const auto& bytes, u16 opcode) {
        PacketView packet{};
        int consumed = 0;
        Require(TryReadPacket(bytes.data(), static_cast<int>(bytes.size()), packet, consumed)
            && packet.packetId == opcode && consumed == static_cast<int>(bytes.size())
            && router.Dispatch(mode, packet), "status-width frame did not route through production");
    };
    const auto stat = [&](s32 base, s32 bonus, u32 kind = 13) {
        std::array<u8, 14> bytes{0x41, 0x01};
        write(bytes, 2, kind);
        write(bytes, 6, base);
        write(bytes, 10, bonus);
        dispatch(bytes, 0x0141);
    };
    const auto param = [&](u16 opcode, u16 kind, u32 value) {
        std::array<u8, 8> bytes{};
        write(bytes, 0, opcode);
        write(bytes, 2, kind);
        write(bytes, 4, value);
        dispatch(bytes, opcode);
    };

    stat(40, -60);
    Require(player.m_Str == 0 && g_session.m_plusStr == -60,
        "signed 0141 bonus underflowed the primary stat total");
    stat(INT32_MAX, INT32_MAX);
    Require(player.m_Str == UINT16_MAX && g_session.m_plusStr == INT32_MAX,
        "0141 stat total overflowed before narrowing to actor width");
    stat(40, -5);
    Require(player.m_Str == 35 && g_session.m_plusStr == -5, "0141 bonus did not lower the actor total");

    std::array<u8, 44> summary{0xbd, 0x00, 0x41, 0x01, 40, 250};
    write(summary, 24, s16{-10});
    write(summary, 26, s16{-20});
    write(summary, 28, s16{-6});
    write(summary, 30, s16{-16});
    write(summary, 40, s16{500});
    dispatch(summary, 0x00bd);
    const CHARACTER_INFO* const selected = g_session.GetSelectedCharacterInfo();
    Require(selected && selected->sppoint == 321 && player.m_Str == 35 && g_session.m_standardStr == 250
        && g_session.m_itemDefPower == -10 && g_session.m_plusDefPower == -20
        && g_session.m_mdefPower == -6 && g_session.m_plusMdefPower == -16 && g_session.m_aspd == 150,
        "00BD summary lost signed combat values, points/cost widths, or the active stat bonus");
    stat(1000, 2, 14);
    param(0x00b0, 9, 70000);
    dispatch(std::array<u8, 6>{0xbc, 0, 13, 0, 0, 255}, 0x00bc);
    Require(selected->Str == 40 && player.m_Str == 35 && player.m_Agi == 1002,
        "failed 00BC acknowledgement changed a primary stat");
    dispatch(std::array<u8, 6>{0xbc, 0, 13, 0, 1, 255}, 0x00bc);
    dispatch(std::array<u8, 5>{0xbe, 0, 32, 0, 255}, 0x00be);
    Require(selected->Str == 255 && player.m_Str == 250 && g_session.m_standardStr == 255
        && player.m_Agi == 1002 && g_session.GetStatusPoints() == 70000 && selected->sppoint == INT16_MAX,
        "00BC/00BE discarded a bonus, refreshed an unrelated stat, or changed the status-point total");
    UIStatusWnd statusWindow;
    UIStatusWnd::DisplayData display{};
    Require(statusWindow.GetDisplayDataForQt(&display) && display.statusPoint == 70000,
        "status display narrowed the full-width point total");
    param(0x00b0, 9, static_cast<u32>(INT32_MAX));
    Require(statusWindow.GetDisplayDataForQt(&display) && display.statusPoint == INT32_MAX,
        "selected signed status-point ceiling did not reach the display");
    param(0x00b0, 9, static_cast<u32>(-1));
    Require(g_session.GetStatusPoints() == 0 && selected->sppoint == 0,
        "negative status-point parameter appeared as a positive balance");

    param(0x00b0, 5, 0x80000001u);
    param(0x00b0, 6, 0xffffffffu);
    param(0x00b0, 7, 0xf1234567u);
    param(0x00b0, 8, 0xffffffffu);
    const auto& vitals = g_session.GetCharacterVitals();
    Require(vitals.hp == 0x80000001u && player.m_Hp == vitals.hp
        && vitals.sp == 0xf1234567u && player.m_Sp == vitals.sp
        && vitals.maxHp == UINT32_MAX && player.m_MaxHp == vitals.maxHp
        && vitals.maxSp == UINT32_MAX && player.m_MaxSp == vitals.maxSp,
        "synthetic high-bit HP or SP was truncated in the session or actor");

    param(0x00b1, 1, 0x80000000u);
    param(0x00b1, 2, 0xc0000000u);
    param(0x00b1, 22, UINT32_MAX);
    param(0x00b1, 23, UINT32_MAX);
    int basePercent = 0;
    int jobPercent = 0;
    Require(g_session.TryGetBaseExpPercent(&basePercent) && basePercent == 50
        && g_session.TryGetJobExpPercent(&jobPercent) && jobPercent == 75,
        "high-bit base and job EXP aliased, truncated, or overflowed percentage math");
    param(0x00b1, 22, 0);
    Require(!g_session.TryGetBaseExpPercent(&basePercent)
        && g_session.TryGetJobExpPercent(&jobPercent) && jobPercent == 75,
        "zero next-base EXP changed the independent job percentage");
    std::cout << "{\"case_id\":\"status_resource_widths\",\"contract_id\":\"PK24.02\",\"outcome\":\"PASS\"}\n";
}
