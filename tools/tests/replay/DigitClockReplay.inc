void DigitClockReplay()
{
    SetCase("show_digit_01b1", "PK47.04", "PK47.04-DIGIT-CLOCK",
        "production packet loop -> 01B1 -> gameplay frame clock");
    CGameMode mode;
    auto& clock = g_session.m_digitClock;
    const auto saved = clock;
    clock = {};
    const auto send = [&](u8 type, s32 value) {
        std::vector<u8> frame{0xb1, 0x01, type, 0, 0, 0, 0};
        for (int byte = 0; byte < 4; ++byte) {
            frame[3 + byte] = static_cast<u8>(static_cast<u32>(value) >> (8 * byte));
        }
        FeedCompassReplayPacket(mode, frame);
    };

    send(0, 86405);
    Require(clock.AdvanceFrame(10000) == "01:00:00:05", "01B1 did not retain the day and zero groups");
    send(9, 99);
    Require(clock.AdvanceFrame(15000) == "01:00:00:05", "unknown digit mode replaced the current display");
    Require(clock.AdvanceFrame(15001) == "01:00:00:05" && clock.AdvanceFrame(15002).empty(),
        "static duration did not expire after its final frame");

    const auto beforeCountUp = timeGetTime();
    send(1, -61);
    auto after = timeGetTime();
    Require(after - beforeCountUp < 1000 && clock.AdvanceFrame(after) == "01:01",
        "count up lost its signed initial duration");
    Require(clock.AdvanceFrame(after + 1000) == "01:02", "count up did not advance using ticks");

    const auto beforeCountDown = timeGetTime();
    send(2, -61);
    after = timeGetTime();
    Require(after - beforeCountDown < 1000 && clock.AdvanceFrame(beforeCountDown) == "01:01",
        "countdown lost its signed initial duration");
    Require(clock.AdvanceFrame(beforeCountDown + 1000) == "01:00", "countdown did not advance using ticks");
    // Exercise the exact deadline without relying on timer resolution.
    clock.Start(2, static_cast<u32>(-1), 0xfffffff0u);
    Require(clock.AdvanceFrame(984) == "00" && clock.AdvanceFrame(985).empty(),
        "countdown did not expire on its exact deadline after clock rollover");

    send(3, 1);
    for (u32 frame = 0; frame < 30; ++frame) {
        Require(clock.AdvanceFrame(frame * 2000) == "01", "short countdown skipped a render frame");
    }
    Require(clock.AdvanceFrame(0) == "00" && clock.AdvanceFrame(0).empty(),
        "short countdown did not draw its final zero once");
    send(3, 99);
    Require(clock.AdvanceFrame(0) == "99", "short countdown lost its second digit");

    CGameModePacketRouter router;
    RegisterDefaultGameModePacketHandlers(router);
    const std::array<u8, 6> shortFrame{0xb1, 0x01, 0, 42, 0, 0};
    PacketView view{};
    int consumed = 0;
    Require(!TryReadPacket(shortFrame.data(), static_cast<int>(shortFrame.size()), view, consumed),
        "partial digit clock frame was consumed");
    Require(clock.AdvanceFrame(0) == "99", "partial digit clock frame changed the display");
    clock = saved;
    std::cout << "{\"case_id\":\"show_digit_01b1\",\"contract_id\":\"PK47.04-DIGIT-CLOCK\",\"outcome\":\"PASS\"}\n";
}

struct DigitClockReplayAccess {
    static void Draw(CGameMode& mode, u32 now) { mode.QueueDigitClock(now); }
};

void DigitClockRenderReplay(const char* assetArchive)
{
    SetCase("show_digit_01b1_render", "PK47.04", "PK47.04-DIGIT-CLOCK",
        "production 01B1 route -> TimeFont artwork -> Vulkan overlay queue");
    Require(GetRequestedRenderBackend() == RenderBackendType::Vulkan, "digit clock render needs Vulkan");
    HWND window = CreateWindowExA(0, "STATIC", "Packet digit clock fixture", WS_POPUP,
        0, 0, 640, 480, nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
    Require(window != nullptr, "digit clock fixture window creation failed");
    struct Cleanup {
        HWND window;
        ~Cleanup() {
            g_renderer.DiscardRenderList();
            GetRenderDevice().Shutdown();
            DestroyWindow(window);
        }
    } cleanup{window};
    RenderBackendBootstrapResult bootstrap{};
    Require(GetRenderDevice().Initialize(window, &bootstrap) && bootstrap.backend == RenderBackendType::Vulkan,
        "digit clock Vulkan initialization failed");
    g_renderer.Init();
    g_renderer.SetSize(640, 480);
    g_fileMgr.AddPak(assetArchive);
    g_resMgr.RegisterType("spr", "data\\sprite\\", new CSprRes());
    g_resMgr.RegisterType("act", "data\\sprite\\", new CActRes());
    CGameMode mode;
    FeedCompassReplayPacket(mode, {0xb1, 0x01, 0, 61, 0, 0, 0});
    const auto& quads = g_renderer.m_rpAlphaNoDepthList;
    DigitClockReplayAccess::Draw(mode, 1000);
    Require(quads.size() == 1 && quads.front().second->tex != nullptr,
        "active digit clock did not queue the selected TimeFont artwork");
    const auto* face = quads.front().second;
    const float left = face->verts[0].x;
    const float top = face->verts[0].y;
    Require(left == 192 && top == 3 && face->verts[3].x == 394 && face->verts[3].y == 76
        && face->srcAlphaMode == D3DBLEND_ONE, "digit clock quad did not use the source frame anchors");
    g_renderer.DiscardRenderList();
    g_renderer.SetSize(800, 600);
    DigitClockReplayAccess::Draw(mode, 6001);
    Require(quads.size() == 1 && quads.front().second->verts[0].x == left + 80,
        "digit clock did not re-center or lost its final frame");
    g_renderer.DiscardRenderList();
    DigitClockReplayAccess::Draw(mode, 6002);
    Require(quads.empty(), "expired digit clock still queued artwork");
    std::cout << "{\"case_id\":\"show_digit_01b1_render\",\"outcome\":\"PASS\"}\n";
}
