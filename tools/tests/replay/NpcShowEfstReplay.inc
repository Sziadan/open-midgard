namespace {

std::vector<u8> NpcShowEfstPacket(u32 aid, u32 effectState, u32 level, u32 showEfst)
{
    std::vector<u8> bytes(18, 0);
    bytes[0] = 0x8a;
    bytes[1] = 0x02;
    const u32 values[] = { aid, effectState, level, showEfst };
    for (size_t field = 0; field < 4; ++field) {
        for (size_t byte = 0; byte < 4; ++byte) {
            bytes[2 + field * 4 + byte] = static_cast<u8>(values[field] >> (byte * 8));
        }
    }
    return bytes;
}

void DispatchNpcShowEfst(CGameMode& mode, CGameModePacketRouter& router,
    u32 aid, u32 effectState, u32 level, u32 showEfst)
{
    const std::vector<u8> bytes = NpcShowEfstPacket(aid, effectState, level, showEfst);
    PacketView packet{};
    int consumed = 0;
    Require(TryReadPacket(bytes.data(), static_cast<int>(bytes.size()), packet, consumed)
        && consumed == static_cast<int>(bytes.size()) && packet.packetId == 0x028a
        && packet.packetLength == 18, "NPC SHOWEFST fixture did not frame as literal 0x028A");
    Require(router.Dispatch(mode, packet), "production router did not dispatch 0x028A");
}

int CountActorEffect(const CGameActor& actor, int effectId)
{
    return static_cast<int>(std::count_if(actor.m_effectList.begin(), actor.m_effectList.end(),
        [effectId](const CRagEffect* effect) { return effect && effect->GetEffectType() == effectId; }));
}

void RequireActorEffectCount(const CGameActor& actor, int effectId, int expected, const char* message)
{
    Require(CountActorEffect(actor, effectId) == expected, message);
}

void SendShowEfstStatus(CGameActor& actor, int status, bool enabled)
{
    actor.SendMsg(&actor, 110, status, enabled ? 1 : 0, 0);
}

} // namespace

void NpcShowEfstReplay()
{
    SetCase("npc_show_efst_replay", "PK22.02", "PK22-02-selected-20080910-showefst-028a",
        "production 0x028A router -> CGameActor::SetShowEfst -> actor-owned effects");
    CMode* const oldMode = SkillPacketReplayAccess::Current();
    const int oldModeType = SkillPacketReplayAccess::CurrentType();
    const int oldEffectsEnabled = g_session.m_isEffectOn;
    CGameMode mode;
    CWorld world;
    CGameActor nonPc;
    CGameActor pc;
    CGameActor maskFirst;
    CGameActor statusFirst;
    CGameActor hidden;
    CGameActor packetStateOnly;
    nonPc.m_gid = 0x11000001u; nonPc.m_isPc = 0;
    pc.m_gid = 0x11000002u; pc.m_isPc = 1;
    maskFirst.m_gid = 0x11000003u; maskFirst.m_isPc = 0;
    statusFirst.m_gid = 0x11000004u; statusFirst.m_isPc = 0;
    hidden.m_gid = 0x11000005u; hidden.m_isPc = 0;
    packetStateOnly.m_gid = 0x11000006u; packetStateOnly.m_isPc = 0;
    CGameActor* const actors[] = { &nonPc, &pc, &maskFirst, &statusFirst, &hidden, &packetStateOnly };
    mode.m_world = &world;
    for (CGameActor* actor : actors) {
        world.m_actorList.push_back(actor);
        mode.m_runtimeActors.emplace(actor->m_gid, actor);
    }
    SkillPacketReplayAccess::SetCurrent(&mode);
    g_session.m_isEffectOn = 1;
    CGameModePacketRouter router;
    RegisterDefaultGameModePacketHandlers(router);

    struct RestoreNpcShowEfst {
        CGameMode& mode;
        CWorld& world;
        CGameActor* const* actors;
        size_t count;
        CMode* oldMode;
        int oldModeType;
        int oldEffectsEnabled;
        ~RestoreNpcShowEfst()
        {
            for (size_t i = 0; i < count; ++i) {
                world.NotifyActorDeleted(actors[i]);
                world.UnregisterActor(actors[i]);
            }
            world.UpdateGameObjects();
            mode.m_runtimeActors.clear();
            mode.m_world = nullptr;
            g_session.m_isEffectOn = oldEffectsEnabled;
            SkillPacketReplayAccess::Restore(oldMode, oldModeType);
        }
    } restore{mode, world, actors, std::size(actors), oldMode, oldModeType, oldEffectsEnabled};

    constexpr u32 allNonPcBits = 0x00018016u; // 420, 427, 254, 655 and 667 snapshot claims.
    DispatchNpcShowEfst(mode, router, nonPc.m_gid, 0, 1, allNonPcBits);
    for (const int id : {420, 427, 254, 655, 667}) {
        RequireActorEffectCount(nonPc, id, 1, "028A did not activate one expected non-PC effect");
    }
    DispatchNpcShowEfst(mode, router, nonPc.m_gid, 0, 1, allNonPcBits);
    for (const int id : {420, 427, 254, 655, 667}) {
        RequireActorEffectCount(nonPc, id, 1, "duplicate 028A duplicated an actor effect");
    }

    // Status 87/235 independently claim 254; status 97 claims 655. Drop the
    // OPT3 snapshot first, then release icon claims one by one.
    DispatchNpcShowEfst(mode, router, maskFirst.m_gid, 0, 1, 0x00010010u);
    SendShowEfstStatus(maskFirst, 87, true);
    SendShowEfstStatus(maskFirst, 235, true);
    SendShowEfstStatus(maskFirst, 97, true);
    SendShowEfstStatus(maskFirst, 87, true);
    SendShowEfstStatus(maskFirst, 235, true);
    SendShowEfstStatus(maskFirst, 97, true);
    RequireActorEffectCount(maskFirst, 254, 1, "duplicate status-110 claims duplicated effect 254");
    RequireActorEffectCount(maskFirst, 655, 1, "duplicate status-110 claim duplicated effect 655");
    DispatchNpcShowEfst(mode, router, maskFirst.m_gid, 0, 1, 0);
    RequireActorEffectCount(maskFirst, 254, 1, "mask end incorrectly removed shared 254 claims");
    RequireActorEffectCount(maskFirst, 655, 1, "mask end incorrectly removed shared 655 claim");
    SendShowEfstStatus(maskFirst, 87, false);
    RequireActorEffectCount(maskFirst, 254, 1, "status 87 end removed 254 still claimed by status 235");
    SendShowEfstStatus(maskFirst, 235, false);
    RequireActorEffectCount(maskFirst, 254, 0, "last 254 status claim did not remove the shared effect");
    SendShowEfstStatus(maskFirst, 97, false);
    RequireActorEffectCount(maskFirst, 655, 0, "last 655 status claim did not remove the shared effect");
    RequireActorEffectCount(maskFirst, 656, 1, "non-PC 655 status end did not create one terminal 656");

    // Release icon claims first and OPT3 snapshot last; terminal still occurs once.
    DispatchNpcShowEfst(mode, router, statusFirst.m_gid, 0, 1, 0x00010010u);
    SendShowEfstStatus(statusFirst, 87, true);
    SendShowEfstStatus(statusFirst, 235, true);
    SendShowEfstStatus(statusFirst, 97, true);
    SendShowEfstStatus(statusFirst, 87, false);
    SendShowEfstStatus(statusFirst, 235, false);
    SendShowEfstStatus(statusFirst, 97, false);
    RequireActorEffectCount(statusFirst, 254, 1, "status-first order lost OPTION 254 claim");
    RequireActorEffectCount(statusFirst, 655, 1, "status-first order lost OPTION 655 claim");
    DispatchNpcShowEfst(mode, router, statusFirst.m_gid, 0, 1, 0);
    RequireActorEffectCount(statusFirst, 254, 0, "OPTION end did not release shared 254 after status-first order");
    RequireActorEffectCount(statusFirst, 655, 0, "OPTION end did not release shared 655 after status-first order");
    RequireActorEffectCount(statusFirst, 656, 1, "status-first OPTION end did not create one terminal 656");

    // Non-PC OPT3 add/remove uses separate terminal effects exactly once.
    DispatchNpcShowEfst(mode, router, nonPc.m_gid, 0, 1, allNonPcBits);
    DispatchNpcShowEfst(mode, router, nonPc.m_gid, 0, 1, 0);
    RequireActorEffectCount(nonPc, 540, 1, "OPT3 bit 0x2 end did not launch terminal 540 once");
    RequireActorEffectCount(nonPc, 656, 1, "OPT3 bit 0x10000 end did not launch terminal 656 once");
    RequireActorEffectCount(nonPc, 420, 0, "OPT3 0x2 effect survived its mask end");
    RequireActorEffectCount(nonPc, 427, 0, "OPT3 0x4 effect survived its mask end");
    RequireActorEffectCount(nonPc, 667, 0, "OPT3 0x8000 effect survived its mask end");

    // PC actors intentionally expose only the 254/655 subset of these flags.
    DispatchNpcShowEfst(mode, router, pc.m_gid, 0, 1, allNonPcBits);
    RequireActorEffectCount(pc, 254, 1, "PC snapshot omitted effect 254");
    RequireActorEffectCount(pc, 655, 1, "PC snapshot omitted effect 655");
    for (const int id : {420, 427, 667}) {
        RequireActorEffectCount(pc, id, 0, "PC snapshot incorrectly activated non-PC effect");
    }
    DispatchNpcShowEfst(mode, router, pc.m_gid, 0, 1, 0);
    RequireActorEffectCount(pc, 656, 0, "PC OPTION mask end incorrectly launched non-PC terminal 656");

    // The incoming OPTION field is not authoritative for the current actor
    // concealment state: current visible actor state permits effect 254.
    DispatchNpcShowEfst(mode, router, packetStateOnly.m_gid, 0x4u, 1, 0x10u);
    RequireActorEffectCount(packetStateOnly, 254, 1,
        "incoming 028A effectState alone suppressed 254 while actor current OPTION was visible");

    DispatchNpcShowEfst(mode, router, hidden.m_gid, 0, 1, allNonPcBits);
    hidden.SetAttrState(0, 0x4, 0, false);
    RequireActorEffectCount(hidden, 254, 0, "current hidden actor retained effect 254");
    for (const int id : {420, 427, 655, 667}) {
        RequireActorEffectCount(hidden, id, 1, "concealment incorrectly removed another non-PC claim");
    }
    hidden.SetAttrState(0, 0, 0, false);
    RequireActorEffectCount(hidden, 254, 1, "revealed actor did not restore effect 254");

    DispatchNpcShowEfst(mode, router, packetStateOnly.m_gid, 0, 1, 0);
    CRagEffect* const external = packetStateOnly.LaunchEffect(254);
    Require(external, "unrelated effect setup failed");
    DispatchNpcShowEfst(mode, router, packetStateOnly.m_gid, 0, 1, 0x10u);
    RequireActorEffectCount(packetStateOnly, 254, 1, "snapshot stacked an unrelated 254 instance");
    DispatchNpcShowEfst(mode, router, packetStateOnly.m_gid, 0, 1, 0);
    Require(packetStateOnly.m_effectList.size() == 1 && packetStateOnly.m_effectList.front() == external,
        "snapshot clear detached an effect it did not create");

    const size_t worldEffectsBeforeUnknown = world.m_gameObjectList.size();
    DispatchNpcShowEfst(mode, router, 0xdeadbeefu, 0, 1, allNonPcBits);
    Require(mode.m_runtimeActors.size() == std::size(actors)
        && world.m_gameObjectList.size() == worldEffectsBeforeUnknown,
        "unknown actor 028A created actor or effect state");
    std::cout << "{\"case_id\":\"npc_show_efst_replay\",\"contract_id\":\"PK22.02\",\"literal_028a\":true,\"nonpc_effects\":[420,427,254,655,667],\"shared_claim_end_orders\":2,\"terminal_effects\":[540,656],\"pc_subset\":[254,655],\"hidden_254_suppression_reveal\":true,\"unknown_actor_ignored\":true,\"outcome\":\"PASS\"}" << std::endl;
}
