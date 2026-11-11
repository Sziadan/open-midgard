void MonsterEstimateReplay()
{
    SetCase("monster_estimate_018c", "PK26.02", "PK26-selected-018c-monster-estimate",
        "production packet reader -> monster estimate window and window manager lifecycle");
    CGameMode mode;
    CGameModePacketRouter router;
    RegisterDefaultGameModePacketHandlers(router);
    std::array<u8, 29> bytes{0x8c, 1, 0xea, 3, 77, 0, 2, 0, 0x78, 0x56, 0x34, 0xf2,
        0xf6, 0xff, 3, 0, 0xec, 0xff, 9, 0, 10, 20, 30, 40, 50, 60, 70, 80, 255};
    const auto dispatch = [&] {
        PacketView packet{};
        int consumed = 0;
        Require(TryReadPacket(bytes.data(), static_cast<int>(bytes.size()), packet, consumed)
            && packet.packetId == 0x018c && consumed == static_cast<int>(bytes.size())
            && router.Dispatch(mode, packet), "018C did not reach the monster estimate window through production");
    };

    dispatch();
    UIMonsterInfoWnd* const window = g_windowMgr.m_monsterInfoWnd;
    Require(window && window->m_w == 400 && window->m_h == 200, "018C did not open the estimate frame");
    const auto& estimate = window->GetEstimate();
    Require(estimate.job == 1002 && estimate.level == 77 && estimate.size == 2 && estimate.hp == 0xf2345678u
        && estimate.defense == -10 && estimate.magicDefense == -20 && estimate.race == 3 && estimate.element == 9
        && estimate.rates == std::array<u8, 9>{10, 20, 30, 40, 50, 60, 70, 80, 255},
        "018C lost a field width, sign, or element rate");

    bytes[2] = 5;
    bytes[3] = 5; // Ref excludes class 1285.
    dispatch();
    Require(g_windowMgr.m_monsterInfoWnd == window && window->GetEstimate().job == 1002,
        "excluded estimate replaced the visible result");
    bytes[2] = 0xea;
    bytes[3] = 3;
    bytes[4] = 88;
    dispatch();
    Require(g_windowMgr.m_monsterInfoWnd == window && window->GetEstimate().level == 88,
        "repeated estimate did not refresh the current frame");
    const int closeX = window->m_x + window->m_w - 30;
    const int closeY = window->m_y + window->m_h - 18;
    window->OnLBtnDown(closeX, closeY);
    window->OnLBtnUp(closeX, closeY);
    Require(g_windowMgr.m_monsterInfoWnd == nullptr, "estimate close button did not release its window");
    dispatch();
    mode.OnExit();
    Require(g_windowMgr.m_monsterInfoWnd == nullptr, "map exit retained the estimate window");
    std::cout << "{\"case_id\":\"monster_estimate_018c\",\"contract_id\":\"PK26.02\",\"outcome\":\"PASS\"}\n";
}
