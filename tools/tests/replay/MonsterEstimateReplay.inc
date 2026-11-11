#include "render/DC.h"

void MonsterEstimateReplay()
{
    SetCase("monster_estimate_018c", "PK26.02", "PK26-selected-018c-monster-estimate",
        "production packet reader -> monster estimate window and window manager lifecycle");
    CGameMode mode;
    CGameModePacketRouter router;
    RegisterDefaultGameModePacketHandlers(router);

    const auto previousCwd = std::filesystem::current_path();
    const auto fixtureRoot = std::filesystem::temp_directory_path()
        / ("monster-estimate-pk26-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(fixtureRoot / "data" / "sprite" / "monster");
    struct RestorePortraitFixture {
        std::filesystem::path cwd;
        std::filesystem::path root;
        ~RestorePortraitFixture() {
            g_resMgr.UnloadResByExt("act");
            g_resMgr.UnloadResByExt("spr");
            std::filesystem::current_path(cwd);
            std::filesystem::remove_all(root);
        }
    } restorePortrait{previousCwd, fixtureRoot};
    std::filesystem::current_path(fixtureRoot);
    // The packet replay executable does not run the game's resource bootstrap.
    g_resMgr.RegisterType("spr", "data\\sprite\\", new CSprRes());
    g_resMgr.RegisterType("act", "data\\sprite\\", new CActRes());
    // SPR101: one opaque 240x220 indexed image, with a solid red palette entry.
    const auto put16 = [](std::vector<u8>& out, u16 value) {
        out.push_back(static_cast<u8>(value));
        out.push_back(static_cast<u8>(value >> 8));
    };
    const auto put32 = [](std::vector<u8>& out, u32 value) {
        for (int shift = 0; shift < 32; shift += 8) out.push_back(static_cast<u8>(value >> shift));
    };
    const auto writeFixture = [](const std::filesystem::path& path, const std::vector<u8>& bytes) {
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        Require(file.good(), "synthetic monster portrait resource write failed");
    };
    constexpr int portraitWidth = 240;
    constexpr int portraitHeight = 220;
    std::vector<u8> spr{'S', 'P', 1, 1};
    put16(spr, 1);
    put16(spr, portraitWidth);
    put16(spr, portraitHeight);
    spr.insert(spr.end(), portraitWidth * portraitHeight, 1);
    std::array<u8, 1024> palette{};
    palette[4] = 255; // palette entry 1, red channel
    spr.insert(spr.end(), palette.begin(), palette.end());
    writeFixture(fixtureRoot / "data" / "sprite" / "monster" / "PORING.spr", spr);

    // ACT205: action 0, motion 0, first clip points at the full-size SPR image.
    std::vector<u8> act{'A', 'C', 5, 2, 1, 0};
    act.resize(16, 0);
    put32(act, 1);          // motion count
    act.resize(act.size() + 32, 0); // range1 + range2
    put32(act, 1);          // sprite clip count
    put32(act, 0); put32(act, 0); put32(act, 0); put32(act, 0); // center, sprite index, flags
    act.insert(act.end(), {255, 255, 255, 255});
    put32(act, 0x3f800000); put32(act, 0x3f800000); // zoom X/Y = 1.0
    put32(act, 0); put32(act, 0); // angle, indexed SPR bank
    put32(act, portraitWidth); put32(act, portraitHeight); // ACT205 logical clip size
    put32(act, 0xffffffff); // no motion event
    put32(act, 0);          // no attachment points
    put32(act, 0);          // no ACT events
    put32(act, 0x40800000); // one action, 4.0 frame delay
    writeFixture(fixtureRoot / "data" / "sprite" / "monster" / "PORING.act", act);
    g_resMgr.UnloadResByExt("act");
    g_resMgr.UnloadResByExt("spr");
    char resolvedAct[MAX_PATH]{};
    char resolvedSpr[MAX_PATH]{};
    Require(ResolveNonPcSpritePaths(1002, resolvedAct, resolvedSpr)
        && std::string(resolvedAct) == "data\\sprite\\monster\\PORING.act"
        && std::string(resolvedSpr) == "data\\sprite\\monster\\PORING.spr",
        "job-1002 resolver did not select the synthetic Poring resources");
    auto* fixtureAct = g_resMgr.GetAs<CActRes>(resolvedAct);
    auto* fixtureSpr = g_resMgr.GetAs<CSprRes>(resolvedSpr);
    const SprImg* fixtureImage = fixtureSpr ? fixtureSpr->GetSprite(0, 0) : nullptr;
    Require(fixtureAct && fixtureAct->GetMotionCount(0) == 1 && fixtureAct->GetMotion(0, 0)
        && fixtureAct->GetMotion(0, 0)->sprClips.size() == 1 && fixtureImage
        && fixtureImage->width == portraitWidth && fixtureImage->height == portraitHeight,
        "synthetic Poring ACT/SPR did not load through production resource loaders");

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

    ArgbDibSurface portraitSurface;
    Require(portraitSurface.EnsureSize(window->m_w, window->m_h), "monster estimate portrait DIB allocation failed");
    std::fill_n(portraitSurface.GetPixels(), window->m_w * window->m_h, 0xff181b20u);
    window->DrawToHdc(portraitSurface.GetDC());
    int portraitColorPixels = 0;
    int minPortraitX = window->m_w;
    int minPortraitY = window->m_h;
    int maxPortraitX = -1;
    int maxPortraitY = -1;
    for (int y = 0; y < window->m_h; ++y) {
        for (int x = 0; x < window->m_w; ++x) {
            const u32 pixel = portraitSurface.GetPixels()[y * window->m_w + x];
            const int red = int((pixel >> 16) & 0xff);
            const int green = int((pixel >> 8) & 0xff);
            const int blue = int(pixel & 0xff);
            if (red > 200 && green < 80 && blue < 80) {
                ++portraitColorPixels;
                minPortraitX = (std::min)(minPortraitX, x);
                minPortraitY = (std::min)(minPortraitY, y);
                maxPortraitX = (std::max)(maxPortraitX, x);
                maxPortraitY = (std::max)(maxPortraitY, y);
            }
        }
    }
    Require(portraitColorPixels > 1000 && minPortraitX <= 1 && maxPortraitX >= 118
        && minPortraitY >= 46 && minPortraitY <= 48 && maxPortraitY >= 155 && maxPortraitY <= 157,
        "production 018C did not center and half-scale its synthetic job-1002 portrait");

    bytes[2] = 5;
    bytes[3] = 5; // Ref excludes class 1285.
    dispatch();
    Require(g_windowMgr.m_monsterInfoWnd == window && window->GetEstimate().job == 1002,
        "excluded estimate replaced the visible result");
    bytes[2] = bytes[3] = 0xff; // A valid frame with no matching actor asset.
    dispatch();
    window->DrawToHdc(portraitSurface.GetDC());
    Require(g_windowMgr.m_monsterInfoWnd == window && window->GetEstimate().job == 0xffff
        && (portraitSurface.GetPixels()[102 * window->m_w + 60] & 0x00ffffffu) != 0x00ff0000u,
        "estimate with an unavailable portrait lost the window or retained the previous sprite");
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
