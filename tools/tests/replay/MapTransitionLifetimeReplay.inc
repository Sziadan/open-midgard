#include "gamemode/View.h"
#include "ui/UIStorageWnd.h"
#include "world/GameActor.h"
#include "world/World.h"

namespace {

constexpr std::array<u8, 12> kLegacyMapRuleFrames{
    0x99, 0x01, 2, 0, // Event PvP
    0xd6, 0x01, 19, 0, // Battleground and PK
    0x99, 0x01, 3, 0, // Siege and forced minimum effects
};

struct MapTransitionLifetimeSnapshot {
    u32 aid = g_session.m_aid;
    u32 gid = g_session.m_gid;
    u32 authCode = g_session.m_authCode;
    u8 sex = g_session.m_sex;
    int playerX = g_session.m_playerPosX;
    int playerY = g_session.m_playerPosY;
    int playerDir = g_session.m_playerDir;
    int pendingReturn = g_session.m_pendingReturnToCharSelect;
    int mapType = g_session.m_curMapType;
    bool minEffect = g_session.m_isMinEffect;
    bool minEffectPreference = g_session.m_isMinEffectOld;
    ro::SourceMapTypeTable mapTypeTable = g_session.m_mapTypeInfo;
    std::string map = g_session.m_curMap;
    LocalGuildInfo localGuild = g_session.m_localGuild;
    ro::guild::WindowState guildWindow = g_session.m_guildWindow;
    std::vector<FRIEND_INFO> party{g_session.GetPartyList().begin(), g_session.GetPartyList().end()};
    std::string partyName = g_session.m_partyName;
    bool partyMaster = g_session.m_amIPartyMaster;
    bool expShare = g_session.m_partyExpShare;
    bool itemDiv = g_session.m_itemDivType;
    bool itemCollect = g_session.m_itemCollectType;
    std::vector<ITEM_INFO> inventory{g_session.GetInventoryItems().begin(), g_session.GetInventoryItems().end()};
    ro::commerce::State commerce = ro::commerce::state;
    std::vector<UIChatEvent> chatEvents = g_windowMgr.GetChatEvents();
    int requestedMode = g_modeMgr.GetRequestedModeType();
    CMode* currentMode = SkillPacketReplayAccess::Current();
    int currentModeType = SkillPacketReplayAccess::CurrentType();

    void Restore()
    {
        g_session.m_aid = aid;
        g_session.m_gid = gid;
        g_session.m_authCode = authCode;
        g_session.m_sex = sex;
        g_session.m_playerPosX = playerX;
        g_session.m_playerPosY = playerY;
        g_session.m_playerDir = playerDir;
        g_session.m_pendingReturnToCharSelect = pendingReturn;
        g_session.m_curMapType = mapType;
        g_session.m_isMinEffect = minEffect;
        g_session.m_isMinEffectOld = minEffectPreference;
        g_session.m_mapTypeInfo = mapTypeTable;
        std::snprintf(g_session.m_curMap, sizeof(g_session.m_curMap), "%s", map.c_str());
        g_session.m_localGuild = localGuild;
        g_session.m_guildWindow = guildWindow;

        g_session.ClearParty();
        for (const FRIEND_INFO& member : party) {
            g_session.AddMemberToParty(member);
            if (member.partyPositionValid) {
                g_session.SetPartyMemberPosition(member.AID, member.partyX, member.partyY);
            }
        }
        g_session.m_partyName = partyName;
        g_session.m_amIPartyMaster = partyMaster;
        g_session.m_partyExpShare = expShare;
        g_session.m_itemDivType = itemDiv;
        g_session.m_itemCollectType = itemCollect;
        g_session.ClearInventoryItems();
        for (const ITEM_INFO& item : inventory) g_session.SetInventoryItem(item);
        ro::commerce::state = commerce;

        g_windowMgr.ClearChatEvents();
        for (const UIChatEvent& event : chatEvents) {
            g_windowMgr.PushChatEvent(event.text.c_str(), event.color, event.channel, event.tick);
        }

        CRagConnection::instance()->Disconnect();
        ClearPendingDisconnectAction();
        GetProductionGameModePacketRouter().Clear();
        RegisterDefaultGameModePacketHandlers(GetProductionGameModePacketRouter());
        SkillPacketReplayAccess::Restore(currentMode, currentModeType);
        g_modeMgr.Switch(requestedMode, "");
    }
};

void SeedMapTransitionMarker(const char* map)
{
    g_session.ClearParty();
    g_session.m_aid = 0x10101010u;
    g_session.m_gid = 0x20202020u;
    std::snprintf(g_session.m_curMap, sizeof(g_session.m_curMap), "%s", map);
    FRIEND_INFO member{};
    member.isValid = 1;
    member.AID = 0x30303030u;
    member.characterName = "MarkerPeer";
    member.mapName = std::string(map) + ".gat";
    member.state = 0;
    g_session.AddMemberToParty(member);
    Require(g_session.SetPartyMemberPosition(member.AID, 25, 35), "could not seed map-transition marker");
}

void RequirePartyInvalidated(const u64 beforeRevision, const u64 beforeToken, const char* caseId)
{
    const FRIEND_INFO* const member = g_session.FindPartyMemberByAid(0x30303030u);
    UIRoMapWnd minimap;
    UIRoMapWnd::DisplayData display{};
    Require(minimap.GetDisplayDataForQt(&display), std::string(caseId) + " could not read Qt minimap token");
    Require(member && !member->partyPositionValid && member->partyX == 0 && member->partyY == 0,
        std::string(caseId) + " retained the previous party coordinates");
    Require(!g_session.IsPartyMarkerEligible(*member, g_session.m_curMap),
        std::string(caseId) + " left the marker eligible for native/Qt consumers");
    Require(g_session.GetNumParty() == 1 && member->characterName == "MarkerPeer",
        std::string(caseId) + " removed party membership while invalidating coordinates");
    Require(g_session.GetPartyPositionRevision() > beforeRevision && display.imageRevision != beforeToken,
        std::string(caseId) + " did not advance the position revision and minimap token");
}

std::vector<u8> MapChangeFrame(u16 opcode, const char* map, u16 x, u16 y,
    u32 ip = 0, u16 port = 0)
{
    const size_t length = opcode == 0x0092 ? 28 : 22;
    std::vector<u8> frame(length, 0);
    frame[0] = static_cast<u8>(opcode & 0xffu);
    frame[1] = static_cast<u8>(opcode >> 8);
    std::strncpy(reinterpret_cast<char*>(frame.data() + 2), map, 15);
    frame[18] = static_cast<u8>(x & 0xffu);
    frame[19] = static_cast<u8>(x >> 8);
    frame[20] = static_cast<u8>(y & 0xffu);
    frame[21] = static_cast<u8>(y >> 8);
    if (opcode == 0x0092) {
        frame[22] = static_cast<u8>(ip & 0xffu);
        frame[23] = static_cast<u8>((ip >> 8) & 0xffu);
        frame[24] = static_cast<u8>((ip >> 16) & 0xffu);
        frame[25] = static_cast<u8>((ip >> 24) & 0xffu);
        frame[26] = static_cast<u8>(port & 0xffu);
        frame[27] = static_cast<u8>(port >> 8);
    }
    return frame;
}

void FeedProductionMapFrame(CGameMode& mode, CGameModePacketRouter& router,
    const std::vector<u8>& frame, GameModePacketLoopResult& loop)
{
    TestConnection connection;
    LoopbackPeer peer(connection);
    peer.Send(frame);
    WaitKernelPending(connection, static_cast<int>(frame.size()));
    loop = ProcessGameModePacketLoop(mode, connection, router, 8);
}

} // namespace

void MapCellLoadLifecycleReplay()
{
    SetCase("map_cell_load_lifecycle", "PK18.03", "loose-gat",
        "map stream -> game mode update -> terrain bootstrap");
    MapTransitionLifetimeSnapshot saved;
    const auto fixture = std::filesystem::current_path()
        / ("pk18" + std::to_string(GetCurrentProcessId() % 1000000) + ".gat");
    struct Cleanup {
        MapTransitionLifetimeSnapshot& saved;
        const std::filesystem::path& fixture;
        ~Cleanup()
        {
            std::error_code error;
            std::filesystem::remove(fixture, error);
            saved.Restore();
        }
    } cleanup{saved, fixture};

    // Two 3x3 dimensions and nine flat, walkable GAT cells.
    std::array<u8, 14 + 9 * 20> gat{'G', 'R', 'A', 'T', 1, 2, 3, 0, 0, 0, 3};
    std::ofstream out(fixture, std::ios::binary);
    out.write(reinterpret_cast<const char*>(gat.data()), gat.size());
    out.close();
    Require(out.good(), "could not write the GAT lifecycle fixture");
    const std::string map = fixture.stem().string();
    const std::string worldName = map + ".rsw";
    const std::string gatName = map + ".gat";
    g_resMgr.RegisterType("gat", "", new C3dAttr());
    g_session.m_gid = 0;
    g_session.m_pendingReturnToCharSelect = 0;
    auto* connection = CRagConnection::instance();
    connection->Disconnect();
    LoopbackPeer peer(*connection);

    const auto sendUpdates = [&](CGameMode& mode, std::vector<u8> packets) {
        peer.Send(packets);
        std::vector<char> pending(packets.size());
        const auto deadline = Clock::now() + kTimeout;
        while (connection->Recv(pending.data(), static_cast<int>(pending.size()), true) == 0) {
            Require(Clock::now() < deadline, "timed out waiting for early map cell packets");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        Require(connection->IsOpen(), "connection closed before early map cell packets arrived");
        mode.OnUpdate();
    };
    const auto bindGat = [](CGameMode& mode) {
        for (int step = 0; step < 8 && mode.m_world && !mode.m_world->m_attr; ++step) {
            mode.OnUpdate();
        }
        Require(mode.m_world && mode.m_world->m_attr, "production bootstrap did not bind the GAT fixture");
    };

    CGameMode first;
    first.OnInit(worldName.c_str());
    auto updates = MapCellUpdateFrame(1, 1, 5, gatName);
    g_session.m_mapTypeInfo.Clear();
    g_session.m_isMinEffectOld = false;
    updates.insert(updates.end(), kLegacyMapRuleFrames.begin(), kLegacyMapRuleFrames.end());
    for (const auto& update : {
            MapCellUpdateFrame(0, 1, 5, gatName),
            MapCellUpdateFrame(0, 1, 0, gatName),
            MapCellUpdateFrame(1, 0, 5, "other.gat"),
            MapCellUpdateFrame(3, 0, 5, gatName),
            MapCellUpdateFrame(2, 1, 7, gatName)}) {
        updates.insert(updates.end(), update.begin(), update.end());
    }
    sendUpdates(first, std::move(updates));
    Require(first.m_world && !first.m_world->m_attr, "early update was not received before GAT binding");
    bindGat(first);
    C3dAttr* const base = first.m_world->m_attr;
    Require(first.m_world->m_isPKZone && first.m_world->m_isSiegeMode
        && first.m_world->m_isBattleFieldMode && first.m_world->m_isEventPVPMode
        && g_session.m_curMapType == 19 && g_session.m_isMinEffect,
        "old world did not receive its map rules");
    Require(first.m_world->GetMapCellType(1, 1) == 5
        && first.m_world->IsMapCellWalkable(0, 1) && first.m_world->IsMapCellWalkable(1, 0)
        && first.m_world->IsMapCellWalkable(0, 2) && first.m_world->IsMapCellWalkable(2, 1)
        && base->GetCell(1, 1)->flag == 0,
        "binding GAT lost a valid update or applied a rejected update");
    first.OnExit();
    Require(g_world.m_attr == nullptr, "game mode exit retained terrain");

    // A real mode replacement may reopen the same cached asset before the old load finishes.
    CGameMode replacement;
    replacement.OnInit(worldName.c_str());
    Require(replacement.m_world == &g_world && !g_world.m_isPKZone && !g_world.m_isSiegeMode
        && !g_world.m_isBattleFieldMode && !g_world.m_isEventPVPMode,
        "replacement inherited the previous world's map rules");
    Require(g_session.m_curMapType == 19 && g_session.m_isMinEffect && !g_session.m_isMinEffectOld,
        "world replacement reset packet-owned session state");
    auto replacementUpdates = MapCellUpdateFrame(2, 2, 5, gatName);
    replacementUpdates.insert(replacementUpdates.end(), {0x99, 0x01, 0, 0});
    sendUpdates(replacement, std::move(replacementUpdates));
    Require(!g_session.m_isMinEffect && g_session.m_curMapType == 19,
        "neutral map property failed to restore the saved effect preference");
    Require(replacement.m_world && !replacement.m_world->m_attr, "replacement skipped its loading boundary");
    bindGat(replacement);
    Require(replacement.m_world->m_attr == base && replacement.m_world->IsMapCellWalkable(1, 1)
        && replacement.m_world->GetMapCellType(2, 2) == 5,
        "replacement lost its early update or inherited the old world's cell");
    replacement.OnExit();

    // Leaving before GAT exists must discard even an accepted pending update.
    CGameMode abandoned;
    abandoned.OnInit(worldName.c_str());
    sendUpdates(abandoned, MapCellUpdateFrame(1, 2, 5, gatName));
    Require(abandoned.m_world && !abandoned.m_world->m_attr, "abandoned update was not pending");
    abandoned.OnExit();
    CGameMode reentry;
    reentry.OnInit(worldName.c_str());
    bindGat(reentry);
    Require(reentry.m_world->m_attr == base && reentry.m_world->IsMapCellWalkable(1, 2)
        && reentry.m_world->IsMapCellWalkable(2, 2),
        "reentry inherited active or pending cell updates");
    reentry.OnExit();
    connection->Disconnect();
    std::cout << "{\"case_id\":\"map_cell_load_lifecycle\",\"contract_id\":\"PK18.03\",\"outcome\":\"PASS\"}" << std::endl;
}

void StandingResurrectionReplay()
{
    SetCase("standing_resurrection", "PK19.03", "PK01.02-final-0292",
        "self-death -> recovery menu -> mode request -> loopback wire -> resurrection");
    MapTransitionLifetimeSnapshot saved;
    const auto service = g_serviceType;
    g_serviceType = ServiceKorea;
    if (g_windowMgr.m_chooseWnd) {
        g_windowMgr.DeleteWindow(g_windowMgr.m_chooseWnd);
    }
    g_session.ClearInventoryItems();

    CGameMode mode;
    CWorld world;
    CPlayer player;
    constexpr u32 gid = 0x12345678;
    g_session.m_gid = gid;
    player.m_gid = gid;
    world.m_player = &player;
    mode.m_world = &world;
    mode.m_runtimeActors[gid] = &player;
    SkillPacketReplayAccess::SetCurrent(&mode);
    auto* connection = CRagConnection::instance();
    connection->Disconnect();
    LoopbackPeer peer(*connection);

    const auto request = [&] {
        return mode.SendMsg(CGameMode::GameMsg_RequestStandingResurrection, 0, 0, 0);
    };
    Require(!request(), "standing resurrection was offered while alive");
    ITEM_INFO token{};
    token.m_itemIndex = 7;
    token.m_num = 2;
    token.SetItemId(7621);
    g_session.SetInventoryItem(token);
    Require(!request(), "token allowed an alive player to send standing resurrection");

    CGameModePacketRouter router;
    RegisterDefaultGameModePacketHandlers(router);
    const auto dispatch = [&](const auto& frame) {
        PacketView packet{};
        int consumed = 0;
        Require(TryReadPacket(frame.data(), static_cast<int>(frame.size()), packet, consumed)
            && consumed == static_cast<int>(frame.size()) && router.Dispatch(mode, packet),
            "standing resurrection fixture failed to route a packet");
    };
    dispatch(std::array<u8, 7>{0x80, 0x00, 0x78, 0x56, 0x34, 0x12, 1});
    auto* menu = g_windowMgr.m_chooseWnd;
    Require(mode.m_isPlayerDead && menu && menu->m_show && menu->GetEntryCount() == 5,
        "eligible self-death did not open the recovery option");
#if RO_ENABLE_QT6_UI
    QtUiStateAdapter adapter;
    const auto projectedOptions = [&] {
        Require(adapter.syncGameplay(mode, RenderBackendType::LegacyDirect3D7,
            RenderBackendType::LegacyDirect3D7, -1, -1), "recovery menu did not project to Qt");
        return adapter.stateObject()->property("chooseMenuOptions").toList();
    };
    const auto options = projectedOptions();
    Require(options.size() == 5 && options.back().toString() == QStringLiteral("Use Token of Siegfried"),
        "Qt menu did not expose the standing resurrection option");
#endif
    const std::array<int CWorld::*, 3> blockedRules{
        &CWorld::m_isPKZone, &CWorld::m_isSiegeMode, &CWorld::m_isEventPVPMode};
    for (const auto rule : blockedRules) {
        world.*rule = 1;
        Require(menu->GetEntryCount() == 4 && !request(),
            "restricted map exposed or sent standing resurrection");
        world.*rule = 0;
    }
    g_session.ClearInventoryItems();
    Require(menu->GetEntryCount() == 4 && !request(), "missing token permitted standing resurrection");
#if RO_ENABLE_QT6_UI
    Require(projectedOptions().size() == 4, "Qt menu retained an unavailable recovery option");
#endif
    g_session.SetInventoryItem(token);
    Require(SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(20)) == 0,
        "blocked standing resurrection sent bytes");
    menu->OnKeyDown(VK_UP);
    menu->OnKeyDown(VK_UP);
    Require(menu->GetSelectedIndex() == 4, "keyboard could not select the recovery option");
    menu->OnKeyDown(VK_RETURN);
    Require(peer.ReceiveExact(2) == std::vector<u8>{0x92, 0x02}
        && !menu->m_show && mode.m_isPlayerDead && g_session.GetInventoryItems().front().m_num == 2,
        "recovery request changed death or inventory state locally");

    dispatch(std::array<u8, 8>{0x48, 0x01, 0x78, 0x56, 0x34, 0x12, 1, 0});
    Require(!mode.m_isPlayerDead && menu->GetEntryCount() == 4 && !request(),
        "self resurrection did not remove the recovery option");
    connection->Disconnect();
    g_windowMgr.DeleteWindow(menu);
    mode.m_world = nullptr;
    g_serviceType = service;
    saved.Restore();
    std::cout << "{\"case_id\":\"standing_resurrection\",\"contract_id\":\"PK19.03\",\"outcome\":\"PASS\"}" << std::endl;
}

void MapTransitionLifetimeReplay()
{
    SetCase("map_transition_lifetime", "PK14.06", "PK14.06-map-transition-selected-0091-0092-0071",
        "CRagConnection::RecvPacket -> production game-mode router -> HandleMapChange / CLoginMode::PollNetwork");
    MapTransitionLifetimeSnapshot saved;
    CRagConnection* const singleton = CRagConnection::instance();
    singleton->Disconnect();
    ClearPendingDisconnectAction();
    CGameModePacketRouter& router = GetProductionGameModePacketRouter();
    router.Clear();
    RegisterDefaultGameModePacketHandlers(router);

    constexpr u32 kMoveIp = 0x0100000au;
    constexpr u16 kMovePort = 6121;
    const std::vector<u8> mapChangeAtoB = MapChangeFrame(0x0091, "geffen", 41, 52);
    const std::vector<u8> serverMove = MapChangeFrame(0x0092, "geffen", 61, 72, kMoveIp, kMovePort);
    const std::vector<u8> characterChange = {
        0x71, 0x00, 0x80, 0x70, 0x60, 0x50,
        'g','e','f','f','e','n','.','g','a','t',0,0,0,0,0,0,
        10,0,0,1,0xe9,0x17
    };

    // A literal 0x0091 map A -> B frame takes the production non-reconnect branch.
    {
        SeedMapTransitionMarker("prontera");
        const u64 revision = g_session.GetPartyPositionRevision();
        UIRoMapWnd minimap;
        UIRoMapWnd::DisplayData before{};
        Require(minimap.GetDisplayDataForQt(&before), "could not capture pre-map-change visual token");
        CGameMode mode;
        int presentations = 0;
        int reconnects = 0;
        int handoffs = 0;
        int handoffType = -1;
        std::string handoffWorld;
        MapTransitionServices services;
        services.presentLoading = [&](const char* message, float progress) {
            ++presentations;
            Require(std::string(message) == "Loading new map..." && progress > 0.0f,
                "map-change presentation request differed from production defaults");
        };
        services.reconnect = [&](u32, u16) { ++reconnects; return true; };
        services.requestMode = [&](int type, const char* worldName) {
            ++handoffs;
            handoffType = type;
            handoffWorld = worldName ? worldName : "";
            g_modeMgr.Switch(type, worldName);
        };
        mode.SetMapTransitionServices(&services);
        GameModePacketLoopResult loop{};
        FeedProductionMapFrame(mode, router, mapChangeAtoB, loop);
        Require(loop.packetsProcessed == 1 && loop.packetsDispatched == 1 && loop.modeOperationRequested,
            "production 0x0091 map change was not dispatched through the handoff");
        Require(g_session.m_curMap == std::string("geffen") && g_session.m_playerPosX == 41 && g_session.m_playerPosY == 52,
            "production 0x0091 did not apply the selected map and position");
        Require(presentations == 1 && reconnects == 0 && handoffs == 1
            && handoffType == 1 && handoffWorld == "geffen.rsw",
            "0x0091 selected the wrong presentation/reconnect/mode branch");
        Require(g_modeMgr.GetLastOperation() == ModeOperationKind::Switch && g_modeMgr.GetRequestedModeType() == 1,
            "0x0091 request callback did not schedule a production game-mode switch");
        RequirePartyInvalidated(revision, before.imageRevision, "map A to B");
    }

    // An already-loaded same-map warp takes production reuse, invalidates old dots,
    // and sends the normal load-end ACK through the real singleton connection.
    {
        SeedMapTransitionMarker("prontera");
        const u64 revision = g_session.GetPartyPositionRevision();
        UIRoMapWnd minimap;
        UIRoMapWnd::DisplayData before{};
        Require(minimap.GetDisplayDataForQt(&before), "could not capture pre-warp visual token");
        TestConnection mapInput;
        LoopbackPeer inputPeer(mapInput);
        LoopbackPeer ackPeer(*singleton);
        CWorld world;
        CPlayer player;
        player.m_gid = g_session.m_gid;
        CView view;
        C3dAttr attr;
        attr.m_width = 3;
        attr.m_height = 3;
        attr.m_cells.resize(9);
        world.SetMapTerrain(&attr);
        world.m_player = &player;
        world.m_ground = new C3dGround{};
        world.m_ground->m_width = 200;
        world.m_ground->m_height = 200;
        world.m_ground->m_zoom = 5.0f;
        view.SetWorld(&world);
        CGameMode mode;
        mode.m_world = &world;
        mode.m_view = &view;
        mode.m_mapLoadingStage = CGameMode::MapLoading_None;
        mode.m_isOnQuest = 1;
        mode.m_isPlayerDead = 1;
        std::snprintf(mode.m_rswName, sizeof(mode.m_rswName), "%s", "prontera.rsw");
        mode.m_partyPosList[0x30303030u].x = 25;
        mode.m_partyPosList[0x30303030u].y = 35;
        int presentations = 0;
        int handoffs = 0;
        MapTransitionServices services;
        services.presentLoading = [&](const char*, float) { ++presentations; };
        services.reconnect = [](u32, u16) -> bool { throw std::runtime_error("same-map fixture attempted reconnect"); };
        services.requestMode = [&](int type, const char* name) { ++handoffs; g_modeMgr.Switch(type, name); };
        mode.SetMapTransitionServices(&services);
        ITEM_INFO oldEquipment{};
        mode.m_characterNameCache[0x12345678]="Old producer";
        mode.m_characterNameReqTimer[0x12345678]=42;
        oldEquipment.m_itemIndex = 0x123;
        oldEquipment.SetItemId(0x4d2);
        oldEquipment.m_itemType = 5;
        oldEquipment.m_num = 1;
        g_session.SetInventoryItem(oldEquipment);
        g_session.OpenStorage(1, 600);
        g_session.SetStorageItem(oldEquipment);
        g_windowMgr.MakeWindow(UIWindowMgr::WID_STORAGEWND);
        Require(g_windowMgr.m_storageWnd && g_windowMgr.m_storageWnd->m_show,
            "could not open the real storage window before same-map warp");
        const auto frame = MapChangeFrame(0x0091, "prontera", 80, 90);
        auto cellThenWarp = MapCellUpdateFrame(1, 1, 5, "prontera.gat");
        g_session.m_mapTypeInfo.Clear();
        g_session.m_isMinEffectOld = false;
        cellThenWarp.insert(cellThenWarp.end(), kLegacyMapRuleFrames.begin(), kLegacyMapRuleFrames.end());
        cellThenWarp.insert(cellThenWarp.end(), frame.begin(), frame.end());
        inputPeer.Send(cellThenWarp);
        WaitKernelPending(mapInput, static_cast<int>(cellThenWarp.size()));
        const GameModePacketLoopResult loop = ProcessGameModePacketLoop(mode, mapInput, router, 8);
        Require(loop.packetsDispatched == 5 && !loop.modeOperationRequested,
            "same-map warp did not stay on the loaded map");
        Require(presentations == 0 && handoffs == 0, "loaded same-map warp selected external orchestration");
        const std::vector<u8> ack = ackPeer.ReceiveExact(2);
        const u16 expectedAck = ro::net::GetActiveMapGameplaySendProfile().notifyActorInit;
        Require(ack == std::vector<u8>{static_cast<u8>(expectedAck & 0xffu), static_cast<u8>(expectedAck >> 8)},
            "same-map warp did not send the production load-end acknowledgement");
        Require(g_session.GetInventoryItems().empty(),
            "same-map load-end acknowledgement retained old inventory snapshot rows");
        Require(!g_session.IsStorageOpen() && g_session.GetStorageItems().empty()
            && g_windowMgr.m_storageWnd && !g_windowMgr.m_storageWnd->m_show,
            "same-map warp retained the old storage session or native window");
        Require(g_session.m_curMap == std::string("prontera") && g_session.m_playerPosX == 80 && g_session.m_playerPosY == 90,
            "same-map warp did not apply the new coordinates");
        RequirePartyInvalidated(revision, before.imageRevision, "same-map warp reuse");
        Require(mode.m_mapLoadingStage == CGameMode::MapLoading_None && mode.m_world == &world && mode.m_view == &view,
            "same-map reuse discarded the loaded world/view");
        Require(!mode.m_isPlayerDead && !mode.m_isOnQuest,
            "same-map reuse retained the previous death state");
        Require(world.GetMapCellType(1, 1) == 5 && attr.GetCell(1, 1)->flag == 0,
            "same-map reuse lost the active map cell update");
        Require(world.m_isPKZone && world.m_isSiegeMode && world.m_isBattleFieldMode
            && world.m_isEventPVPMode && g_session.m_curMapType == 19 && g_session.m_isMinEffect,
            "same-map reuse lost the active map rules");
        Require(mode.m_characterNameCache.empty() && mode.m_characterNameReqTimer.empty(),"same-map reuse retained character-name state");
        Require(mode.m_partyPosList.empty() && mode.m_runtimeActors.at(g_session.m_gid) == &player,
            "same-map reuse did not clear legacy dots while retaining the player");

        // The same name alone must not select reuse while map loading is active.
        SeedMapTransitionMarker("prontera");
        const u64 loadingRevision = g_session.GetPartyPositionRevision();
        UIRoMapWnd::DisplayData loadingBefore{};
        Require(minimap.GetDisplayDataForQt(&loadingBefore), "could not capture loading-map token");
        mode.m_mapLoadingStage = CGameMode::MapLoading_BootstrapAssets;
        inputPeer.Send(frame);
        WaitKernelPending(mapInput, static_cast<int>(frame.size()));
        const auto loadingLoop = ProcessGameModePacketLoop(mode, mapInput, router, 8);
        Require(loadingLoop.modeOperationRequested && presentations == 1 && handoffs == 1,
            "same-map request incorrectly reused an incompletely loaded map");
        RequirePartyInvalidated(loadingRevision, loadingBefore.imageRevision, "same-map while loading");

        // A server-move endpoint must reconnect even if that map is fully loaded.
        SeedMapTransitionMarker("prontera");
        const u64 transferRevision = g_session.GetPartyPositionRevision();
        UIRoMapWnd::DisplayData transferBefore{};
        Require(minimap.GetDisplayDataForQt(&transferBefore), "could not capture same-map server-move token");
        int sameMapReconnects = 0;
        services.reconnect = [&](u32 ip, u16 port) {
            ++sameMapReconnects;
            Require(ip == kMoveIp && port == kMovePort, "loaded same-map server move lost its endpoint");
            return true;
        };
        CGameMode transferMode;
        transferMode.m_world = &world;
        transferMode.m_view = &view;
        transferMode.m_mapLoadingStage = CGameMode::MapLoading_None;
        std::snprintf(transferMode.m_rswName, sizeof(transferMode.m_rswName), "%s", "prontera.rsw");
        transferMode.SetMapTransitionServices(&services);
        const auto transfer = MapChangeFrame(0x0092, "prontera", 91, 92, kMoveIp, kMovePort);
        GameModePacketLoopResult transferLoop{};
        FeedProductionMapFrame(transferMode, router, transfer, transferLoop);
        Require(transferLoop.modeOperationRequested && sameMapReconnects == 1 && presentations == 2 && handoffs == 2,
            "loaded same-map server move incorrectly selected local warp reuse");
        RequirePartyInvalidated(transferRevision, transferBefore.imageRevision, "loaded same-map server move");
        transferMode.m_world = nullptr;
        transferMode.m_view = nullptr;
        mode.m_world = nullptr;
        mode.m_view = nullptr;
        mode.m_runtimeActors.clear();
        world.m_player = nullptr;
        singleton->Disconnect();
    }

    // Server-move reconnect success and failure both exercise production parser/state mutation;
    // only network reconnect and loading presentation are replaced by callbacks.
    for (const bool reconnectSucceeds : {true, false}) {
        SeedMapTransitionMarker("prontera");
        g_session.m_pendingReturnToCharSelect = reconnectSucceeds ? 0 : 1;
        Require(BeginPendingDisconnectAction(PendingDisconnectAction::ExitToWindows),
            "could not seed pending action before server move");
        MarkPendingDisconnectActionSent(1);
        const u64 revision = g_session.GetPartyPositionRevision();
        UIRoMapWnd minimap;
        UIRoMapWnd::DisplayData before{};
        Require(minimap.GetDisplayDataForQt(&before), "could not capture pre-server-move visual token");
        CGameMode mode;
        int presentations = 0;
        int reconnects = 0;
        int handoffs = 0;
        int handoffType = -1;
        std::string handoffWorld;
        u32 observedIp = 0;
        u16 observedPort = 0;
        MapTransitionServices services;
        services.presentLoading = [&](const char*, float) { ++presentations; };
        services.reconnect = [&](u32 ip, u16 port) {
            ++reconnects;
            const auto* member = g_session.FindPartyMemberByAid(0x30303030u);
            Require(member && !member->partyPositionValid && g_session.m_curMap == std::string("geffen")
                && GetPendingDisconnectAction() == PendingDisconnectAction::None,
                "server-move state was not invalidated/cleared before reconnect orchestration");
            observedIp = ip;
            observedPort = port;
            return reconnectSucceeds;
        };
        services.requestMode = [&](int type, const char* worldName) {
            ++handoffs;
            handoffType = type;
            handoffWorld = worldName ? worldName : "";
            g_modeMgr.Switch(type, worldName);
        };
        mode.SetMapTransitionServices(&services);
        GameModePacketLoopResult loop{};
        FeedProductionMapFrame(mode, router, serverMove, loop);
        Require(loop.packetsDispatched == 1 && loop.modeOperationRequested,
            "production 0x0092 server move did not dispatch and request a handoff");
        Require(observedIp == kMoveIp && observedPort == kMovePort && reconnects == 1 && presentations == 1,
            "0x0092 did not pass the literal server endpoint to reconnect service");
        Require(GetPendingDisconnectAction() == PendingDisconnectAction::None,
            "0x0092 handler did not clear pending disconnect state before reconnect");
        Require(g_session.m_curMap == std::string("geffen") && g_session.m_playerPosX == 61 && g_session.m_playerPosY == 72,
            "0x0092 did not apply map state before reconnect result");
        if (reconnectSucceeds) {
            Require(handoffs == 1 && handoffType == 1 && handoffWorld == "geffen.rsw",
                "successful 0x0092 reconnect did not schedule the target map");
            Require(g_modeMgr.GetLastOperation() == ModeOperationKind::Switch && g_modeMgr.GetRequestedModeType() == 1,
                "successful 0x0092 did not preserve the game-mode handoff");
        } else {
            Require(handoffs == 1 && handoffType == 0 && handoffWorld.empty(),
                "failed 0x0092 reconnect did not return to Login");
            Require(g_session.m_pendingReturnToCharSelect == 0,
                "failed 0x0092 reconnect retained a stale return-to-character intent");
            Require(g_modeMgr.GetLastOperation() == ModeOperationKind::Switch && g_modeMgr.GetRequestedModeType() == 0,
                "failed 0x0092 did not preserve the Login handoff");
        }
        RequirePartyInvalidated(revision, before.imageRevision,
            reconnectSucceeds ? "server move success" : "server move failure");
    }

    // Unknown and malformed-short map frames must leave the current marker intact.
    {
        SeedMapTransitionMarker("prontera");
        const u64 revision = g_session.GetPartyPositionRevision();
        TestConnection connection;
        LoopbackPeer peer(connection);
        peer.Send({0xff, 0xff});
        WaitKernelPending(connection, 2);
        CGameMode mode;
        const GameModePacketLoopResult loop = ProcessGameModePacketLoop(mode, connection, router, 8);
        Require(loop.packetsProcessed == 0 && connection.GetLastReceiveError() == CRagConnection::ReceiveError::UnknownOpcode,
            "unknown input reached the map-change handler");
        const FRIEND_INFO* member = g_session.FindPartyMemberByAid(0x30303030u);
        Require(member && member->partyPositionValid && member->partyX == 25 && member->partyY == 35
            && g_session.GetPartyPositionRevision() == revision,
            "unknown input mutated party marker state");

        std::array<u8, 21> shortPacket{};
        shortPacket[0] = 0x91;
        shortPacket[1] = 0x00;
        std::memcpy(shortPacket.data() + 2, "geffen", 6);
        PacketView shortView{0x0091, static_cast<u16>(shortPacket.size()), shortPacket.data()};
        Require(router.Dispatch(mode, shortView), "production router did not register the active map-change handler");
        member = g_session.FindPartyMemberByAid(0x30303030u);
        Require(member && member->partyPositionValid && g_session.m_curMap == std::string("prontera")
            && g_session.GetPartyPositionRevision() == revision,
            "short malformed map-change packet mutated marker or map state");
    }

    // The actual 0x0071 PollNetwork/OnNotifyZonesvr boundary invalidates before
    // changing the selected character/map; OnExit remains safe and idempotent afterward.
    for (const bool sameMap : {false, true}) {
        SeedMapTransitionMarker("prontera");
        const u64 revision = g_session.GetPartyPositionRevision();
        UIRoMapWnd minimap;
        UIRoMapWnd::DisplayData before{};
        Require(minimap.GetDisplayDataForQt(&before), "could not capture pre-character-change token");
        singleton->Disconnect();
        LoopbackPeer charPeer(*singleton);
        LoginModeReplayAccess mode;
        LoginModeReplayAccess::SetConnected(mode, true);
        auto frame = characterChange;
        const std::string expectedMap = sameMap ? "prontera" : "geffen";
        std::fill(frame.begin() + 6, frame.begin() + 22, 0);
        std::copy(expectedMap.begin(), expectedMap.end(), frame.begin() + 6);
        charPeer.Send(frame);
        const auto deadline = Clock::now() + kTimeout;
        while (singleton->IsOpen() && Clock::now() < deadline) {
            LoginModeReplayAccess::RunPollNetwork(mode);
            if (singleton->IsOpen()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        Require(g_session.m_gid == 0x50607080u && g_session.m_curMap == expectedMap,
            "actual 0x0071 PollNetwork path did not select the new character and map");
        RequirePartyInvalidated(revision, before.imageRevision, "character change notification");
        Require(mode.NextSubMode() == CLoginMode::LoginSubMode_ZoneConnect
            && !LoginModeReplayAccess::IsConnected(mode) && !singleton->IsOpen(),
            "0x0071 did not close the character connection and schedule ZoneConnect");
        const u64 afterNotifyRevision = g_session.GetPartyPositionRevision();
        mode.OnExit();
        Require(g_session.GetPartyPositionRevision() > afterNotifyRevision,
            "actual LoginMode::OnExit did not preserve its idempotent marker invalidation boundary");
        const FRIEND_INFO* member = g_session.FindPartyMemberByAid(0x30303030u);
        Require(member && !member->partyPositionValid && g_session.GetNumParty() == 1,
            "LoginMode::OnExit removed party membership or revived old coordinates");
    }

    saved.Restore();
    std::cout << "{\"case_id\":\"map_transition_lifetime\",\"contract_id\":\"PK14.06\",\"map_0091_literal\":\"" << Hex(mapChangeAtoB)
        << "\",\"map_0092_literal\":\"" << Hex(serverMove)
        << "\",\"zone_0071_literal\":\"" << Hex(characterChange)
        << "\",\"outcome\":\"PASS\",\"assertions\":\"production framing/router, map A to B, same-map reuse ACK, server-move success/failure, invalid short/unknown inputs, actual character notification then login exit, membership/revision/token cleanup\"}\n";
}
