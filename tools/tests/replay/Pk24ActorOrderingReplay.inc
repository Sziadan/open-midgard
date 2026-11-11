void Pk24ActorOrderingReplay()
{
    SetCase("pk24_selected_actor_ordering", "PK24.03", "PK24-selected-actor-ordering",
        "production state/look/spawn/vanish handlers and map exit");

    Require(g_world.m_actorList.empty() && g_world.m_gameObjectList.empty(),
        "actor ordering replay requires its isolated test process");
    class ResourcePlayer final : public CPlayer {
    public:
        std::vector<msgparam_t> numbers;
        void SendMsg(CGameObject* src, int msg, msgparam_t a, msgparam_t b, msgparam_t c) override
        {
            if (msg == 88) {
                numbers.push_back(a);
                return;
            }
            CPlayer::SendMsg(src, msg, a, b, c);
        }
    } player;
    player.m_gid = 1;
    g_session.m_aid = player.m_gid;
    g_session.m_gid = player.m_gid;
    g_session.m_isEffectOn = 1;
    g_world.m_player = &player;
    CGameMode mode;
    mode.m_world = &g_world;
    SkillPacketReplayAccess::SetCurrent(&mode);
    CGameModePacketRouter router;
    RegisterDefaultGameModePacketHandlers(router);
    const auto dispatch = [&](const auto& bytes, u16 opcode) {
        PacketView packet{};
        int consumed = 0;
        Require(TryReadPacket(bytes.data(), static_cast<int>(bytes.size()), packet, consumed)
            && packet.packetId == opcode && consumed == static_cast<int>(bytes.size())
            && router.Dispatch(mode, packet), "actor ordering frame failed production dispatch: " + std::to_string(opcode));
    };
    constexpr u32 gid = 0x1234abcdu;
    const auto actorFor = [&]() -> CGameActor* {
        const auto it = mode.m_runtimeActors.find(gid);
        return it == mode.m_runtimeActors.end() ? nullptr : it->second;
    };
    const auto worldCount = [&] {
        return std::count_if(g_world.m_actorList.begin(), g_world.m_actorList.end(),
            [](const CGameActor* actor) { return actor != nullptr && actor->m_gid == gid; });
    };

    dispatch(std::array<u8, 8>{0xb0, 0, 5, 0, 1, 0, 0, 0x80}, 0x00b0);
    dispatch(std::array<u8, 8>{0xb0, 0, 7, 0, 2, 0, 0, 0x80}, 0x00b0);
    dispatch(std::array<u8, 15>{0x29, 0x02, 0xcd, 0xab, 0x34, 0x12, 1, 0, 2, 0,
        0x80, 0x00, 0x00, 0x40, 3}, 0x0229);
    CGameActor* const placeholder = actorFor();
    Require(placeholder && placeholder->m_bodyState == 1 && placeholder->m_healthState == 2
        && placeholder->m_effectState == 0x40000080 && placeholder->m_pkState == 3,
        "selected state update did not populate the pre-spawn actor");
    dispatch(std::array<u8, 11>{0xd7, 0x01, 0xcd, 0xab, 0x34, 0x12, 0, 0xa1, 0x0f, 0, 0}, 0x01d7);
    Require(actorFor() == placeholder && placeholder->m_job == 4001,
        "pre-spawn look did not update the state-created actor");

    // The fixed 02ED header places the GID directly after the opcode.
    std::array<u8, 59> spawn{0xed, 0x02, 0xcd, 0xab, 0x34, 0x12, 100};
    spawn[16] = 0xa1;
    spawn[17] = 0x0f;
    spawn[49] = 1;
    spawn[50] = 0x20;
    spawn[51] = 0x20;
    spawn[52] = 0x20;
    spawn[55] = 50;
    spawn[57] = 2;
    dispatch(spawn, 0x02ed);
    CGameActor* const spawned = actorFor();
    Require(dynamic_cast<CPc*>(spawned) && spawned->m_job == 4001 && spawned->m_clevel == 50
        && spawned->m_effectState == 0 && g_world.GetGameActorByAID(gid) == spawned && worldCount() == 1,
        "selected spawn did not reconcile the placeholder to one correctly classified world actor");
    dispatch(spawn, 0x02ed);
    Require(actorFor() == spawned && worldCount() == 1,
        "duplicate selected spawn replaced the resolved actor or duplicated its world entry");

    dispatch(std::array<u8, 18>{0x8a, 0x02, 0xcd, 0xab, 0x34, 0x12, 0, 0, 0, 0,
        50, 0, 0, 0, 0x10}, 0x028a);
    Require(spawned->m_effectList.size() == 1, "ShowEFST did not attach a status effect to the spawned actor");
    dispatch(std::array<u8, 7>{0x80, 0, 0xcd, 0xab, 0x34, 0x12, 0}, 0x0080);
    Require(!actorFor() && worldCount() == 0 && spawned->m_effectList.empty()
        && mode.m_deferredActorDeletes.size() == 1,
        "vanish did not detach the effect and remove both actor lookups before deferred deletion");
    g_world.UpdateGameObjects();
    dispatch(spawn, 0x02ed);
    Require(actorFor() && actorFor() != spawned && worldCount() == 1 && actorFor()->m_effectList.empty(),
        "reappearance did not create one fresh actor without stale effects");
    Require(player.m_Hp == 0x80000001u && player.m_Sp == 0x80000002u
        && g_session.GetCharacterVitals().hp == player.m_Hp && g_session.GetCharacterVitals().sp == player.m_Sp,
        "remote lifecycle changed local high-bit resources");

    const auto requireVitals = [&](u32 hp, u32 sp) {
        const auto& vitals = g_session.GetCharacterVitals();
        Require(player.m_Hp == hp && vitals.hp == hp && player.m_Sp == sp && vitals.sp == sp,
            "actor and session resources disagreed with the selected event ordering");
    };
    const std::array<u8, 8> hpSnapshot{0xb0, 0, 5, 0, 0x50, 0xc3, 0, 0}; // 50,000
    const std::array<u8, 14> healVisual{0x84, 2, 1, 0, 0, 0, 0x38, 1, 0, 0, 123, 0, 0, 0}; // EF_HEAL 312
    const std::array<u8, 6> recovery{0x3d, 1, 5, 0, 17, 0};
    dispatch(std::array<u8, 8>{0xb0, 0, 7, 0, 0x84, 3, 0, 0}, 0x00b0); // 900 SP
    dispatch(std::array<u8, 9>{0x96, 1, 34, 0, 1, 0, 0, 0, 1}, 0x0196);
    Require(g_session.IsStatusIconActive(34), "Hallucination did not start through its selected status route");
    dispatch(hpSnapshot, 0x00b0);
    std::array<u8, 33> displayDamage{0xde, 1, 18, 0, 0xcd, 0xab, 0x34, 0x12, 1};
    displayDamage[24] = 9;
    displayDamage[25] = 3; // 777 shown by the server during Hallucination.
    displayDamage[28] = 1;
    displayDamage[30] = 1;
    displayDamage[32] = 4;
    dispatch(displayDamage, 0x01de);
    Require(player.m_willBeAttackedList.size() == 1 && player.m_willBeAttackedList.back().damage == 777,
        "Hallucination damage display did not preserve the server's queued hit value");
    requireVitals(50000, 900);
    dispatch(healVisual, 0x0284);
    requireVitals(50000, 900);
    dispatch(recovery, 0x013d);
    requireVitals(50017, 900);
    dispatch(healVisual, 0x0284);
    requireVitals(50017, 900);
    dispatch(hpSnapshot, 0x00b0);
    requireVitals(50000, 900);
    dispatch(std::array<u8, 15>{0x1a, 1, 28, 0, 199, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1}, 0x011a);
    requireVitals(50000, 900);
    Require(player.numbers == std::vector<msgparam_t>({123, 17, 123, 199}),
        "selected visual/recovery routes did not dispatch their presentation values independently");
    dispatch(std::array<u8, 9>{0x96, 1, 34, 0, 1, 0, 0, 0, 0}, 0x0196);
    Require(!g_session.IsStatusIconActive(34), "Hallucination did not end through its selected status route");

    // Historical local EF_HPTIME/EF_SPTIME still have their own additive branch.
    dispatch(std::array<u8, 14>{0x84, 2, 1, 0, 0, 0, 0x4b, 1, 0, 0, 9, 0, 0, 0}, 0x0284);
    dispatch(std::array<u8, 14>{0x84, 2, 1, 0, 0, 0, 0x4c, 1, 0, 0, 11, 0, 0, 0}, 0x0284);
    requireVitals(50009, 911);
    g_world.NotifyActorDeleted(&player);
    mode.OnExit();
    g_world.UpdateGameObjects();
    Require(mode.m_runtimeActors.empty() && mode.m_deferredActorDeletes.empty()
        && g_world.m_actorList.empty() && g_world.m_gameObjectList.empty(),
        "map exit retained actors or attached effects from the ordering replay");
    SkillPacketReplayAccess::SetCurrent(nullptr);
    std::cout << "{\"case_id\":\"pk24_selected_actor_ordering\",\"contract_id\":\"PK24.03\",\"outcome\":\"PASS\"}\n";
}
