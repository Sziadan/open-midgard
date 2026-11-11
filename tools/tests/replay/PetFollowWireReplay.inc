void PetFollowWireReplay()
{
    SetCase("pet_follow_wire", "PK27.PET-CONTROL", "selected-pet-actor-wire-lifecycle",
        "selected 007c spawn, 02ec walk, and 0080 vanish through production reader/router");

    Require(g_world.m_actorList.empty() && g_world.m_gameObjectList.empty(),
        "pet follow wire replay requires an isolated actor world");
    const auto savedPet = g_session.m_petStatus;
    const u32 savedAid = g_session.m_aid;
    const u32 savedGid = g_session.m_gid;
    CGameMode mode;
    mode.m_world = &g_world;
    CGameModePacketRouter router;
    RegisterDefaultGameModePacketHandlers(router);
    TestConnection connection;
    const u32 gid = 0x76543210;
    const auto dispatch = [&](const std::vector<u8>& bytes, u16 opcode) {
        Require(connection.QueueBytes(bytes), "pet follow wire input queue failed");
        const auto result = ProcessGameModePacketLoop(mode, connection, router, 1);
        Require(result.packetsProcessed == 1 && result.packetsDispatched == 1
            && connection.BufferedBytes() == 0,
            "pet follow wire frame failed production dispatch: " + std::to_string(opcode));
    };
    const auto put16 = [](std::vector<u8>& p, size_t offset, u16 value) {
        p[offset] = static_cast<u8>(value);
        p[offset + 1] = static_cast<u8>(value >> 8);
    };
    const auto put32 = [](std::vector<u8>& p, size_t offset, u32 value) {
        for (int i = 0; i < 4; ++i) p[offset + i] = static_cast<u8>(value >> (8 * i));
    };
    const auto actor = [&]() -> CGameActor* {
        const auto it = mode.m_runtimeActors.find(gid);
        return it == mode.m_runtimeActors.end() ? nullptr : it->second;
    };
    const auto packPosDir = [](std::vector<u8>& p, size_t offset, int x, int y, int dir) {
        p[offset] = static_cast<u8>(x >> 2);
        p[offset + 1] = static_cast<u8>(((x & 3) << 6) | (y >> 4));
        p[offset + 2] = static_cast<u8>(((y & 15) << 4) | (dir & 15));
    };
    const auto packSrcDst = [](std::vector<u8>& p, size_t offset, int sx, int sy, int dx, int dy, int sd, int dd) {
        p[offset] = static_cast<u8>(sx >> 2);
        p[offset + 1] = static_cast<u8>(((sx & 3) << 6) | (sy >> 4));
        p[offset + 2] = static_cast<u8>(((sy & 15) << 4) | (dx >> 6));
        p[offset + 3] = static_cast<u8>(((dx & 63) << 2) | (dy >> 8));
        p[offset + 4] = static_cast<u8>(dy);
        p[offset + 5] = static_cast<u8>(((sd & 15) << 4) | (dd & 15));
    };

    g_session.m_petStatus = {};
    std::vector<u8> bind(11);
    put16(bind, 0, 0x01a4); bind[2] = 0; put32(bind, 3, gid);

    // 007c legacy non-player spawn: objectType 7 is the pet marker. Hair is
    // deliberately non-100 to distinguish appearance from that marker.
    std::vector<u8> spawn(42);
    put16(spawn, 0, 0x007c); spawn[2] = 7; put32(spawn, 3, gid);
    put16(spawn, 7, 150); put16(spawn, 15, 12); put16(spawn, 21, 1002);
    put16(spawn, 29, 3); put16(spawn, 31, 4); spawn[36] = 1;
    packPosDir(spawn, 37, 120, 130, 2);
    dispatch(spawn, 0x007c);
    CGameActor* const spawned = actor();
    Require(spawned && spawned->m_objectType == 7 && spawned->m_job == 1002
        && spawned->m_headType == 100 && g_session.m_petStatus.actorId == 0,
        "007c pet spawn did not mark the actor or claimed it for the local session");
    dispatch(bind, 0x01a4);
    Require(g_session.m_petStatus.actorId == gid && g_session.m_petStatus.job == 1002,
        "pet owner subtype 0 did not bind the visible pet actor");

    std::vector<u8> hairLook(11);
    put16(hairLook, 0, 0x01d7); put32(hairLook, 2, gid);
    hairLook[6] = 1; put32(hairLook, 7, 23);
    dispatch(hairLook, 0x01d7);
    Require(actor() == spawned && spawned->m_objectType == 7 && spawned->m_headType == 100
        && g_session.m_petStatus.actorId == gid,
        "01d7 hair look overwrote the pet marker or owner binding");

    // 02ec selected fixed move layout with objectType at byte 2, a six-byte path,
    // and the marker retained independently from the transmitted hair style.
    std::vector<u8> walk(67);
    put16(walk, 0, 0x02ec); walk[2] = 7; put32(walk, 3, gid);
    put16(walk, 7, 150); put16(walk, 17, 1002); put16(walk, 19, 12);
    packSrcDst(walk, 55, 120, 130, 121, 131, 2, 3);
    dispatch(walk, 0x02ec);
    Require(actor() == spawned && spawned->m_objectType == 7 && spawned->m_headType == 100
        && mode.m_actorPosList.at(gid).x == 120
        && mode.m_actorPosList.at(gid).y == 130 && g_session.m_petStatus.actorId == gid
        && spawned->m_moveDestX == 121 && spawned->m_moveDestY == 131,
        "02ec pet walk lost marker, hair, source-cell position, or owner binding");
    Require(mode.OpenPetContextMenu(spawned, 123, 234), "following owned pet could not open its context menu");
    g_windowMgr.DeleteWindow(g_windowMgr.m_chatRoomMenuWnd);

    std::vector<u8> vanish(7);
    put16(vanish, 0, 0x0080); put32(vanish, 2, gid); vanish[6] = 0;
    dispatch(vanish, 0x0080);
    Require(!actor() && g_session.m_petStatus.actorId == gid,
        "0080 out-of-sight vanish failed to remove the actor or cleared owned pet identity");
    dispatch(spawn, 0x007c);
    Require(actor() && actor()->m_objectType == 7
        && actor()->m_headType == 100
        && g_session.m_petStatus.actorId == gid,
        "pet actor did not reappear from 007c while retaining owner identity and marker");

    mode.OnExit();
    g_world.UpdateGameObjects();
    g_session.m_petStatus = savedPet;
    g_session.m_aid = savedAid;
    g_session.m_gid = savedGid;
    Require(mode.m_runtimeActors.empty() && g_world.m_actorList.empty() && g_world.m_gameObjectList.empty(),
        "pet follow wire replay left runtime actors in the world");
    std::cout << "{\"case_id\":\"pet_follow_wire\",\"contract_id\":\"PK27.PET-CONTROL\",\"outcome\":\"PASS\"}\n";
}
