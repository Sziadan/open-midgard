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

CRagEffect* FindActorEffect(CGameActor& actor, int effectId)
{
    const auto found = std::find_if(actor.m_effectList.begin(), actor.m_effectList.end(),
        [effectId](CRagEffect* effect) { return effect && effect->GetEffectType() == effectId; });
    return found == actor.m_effectList.end() ? nullptr : *found;
}

bool EffectPositionEquals(const CRagEffect* effect, float x, float y, float z)
{
    if (!effect) return false;
    const vector3d& position = effect->EffectPosition();
    return position.x == x && position.y == y && position.z == z;
}

void RequireActorEffectCount(const CGameActor& actor, int effectId, int expected, const char* message)
{
    Require(CountActorEffect(actor, effectId) == expected, message);
}

void SendShowEfstStatus(CGameActor& actor, int status, bool enabled)
{
    actor.SendMsg(&actor, 110, status, enabled ? 1 : 0, 0);
}

void DispatchShowEfstStatus(CGameMode& mode, CGameModePacketRouter& router,
    u32 gid, u16 status, bool enabled)
{
    std::array<u8, 9> bytes{0x96, 0x01, static_cast<u8>(status), static_cast<u8>(status >> 8),
        0, 0, 0, 0, static_cast<u8>(enabled)};
    for (unsigned byte = 0; byte < 4; ++byte) {
        bytes[4 + byte] = static_cast<u8>(gid >> (byte * 8));
    }
    PacketView packet{};
    int consumed = 0;
    Require(TryReadPacket(bytes.data(), static_cast<int>(bytes.size()), packet, consumed)
        && consumed == static_cast<int>(bytes.size()) && packet.packetId == 0x0196
        && router.Dispatch(mode, packet), "selected literal 0x0196 did not dispatch");
}

CGameActor* SpawnShowEfstReplayActor(CGameMode& mode, CGameModePacketRouter& router,
    u32 gid, u16 job, u16 opt3)
{
    std::array<u8, 53> spawn{0x79, 0};
    for (unsigned byte = 0; byte < 4; ++byte) {
        spawn[2 + byte] = static_cast<u8>(gid >> (byte * 8));
    }
    spawn[14] = static_cast<u8>(job);
    spawn[15] = static_cast<u8>(job >> 8);
    spawn[42] = static_cast<u8>(opt3);
    spawn[43] = static_cast<u8>(opt3 >> 8);
    spawn[51] = 1; // clevel
    PacketView view{};
    int consumed = 0;
    Require(ro::net::GetActiveMapReceiveProfile().actorSpawnLegacySpawn == 0x0079
        && TryReadPacket(spawn.data(), static_cast<int>(spawn.size()), view, consumed)
        && consumed == static_cast<int>(spawn.size()) && view.packetId == 0x0079
        && router.Dispatch(mode, view), "selected literal 0x0079 spawn packet did not dispatch");
    const auto found = mode.m_runtimeActors.find(gid);
    Require(found != mode.m_runtimeActors.end() && found->second,
        "selected literal 0x0079 spawn did not create a runtime actor");
    return found->second;
}

} // namespace

void NpcShowEfstMapExitReplay()
{
    SetCase("npc_show_efst_map_exit_replay", "PK22.04", "PK22-04-missing-texture-map-exit",
        "selected 0079 spawn -> effect processing -> CGameMode::OnExit -> world sweep");
    Require(g_world.m_actorList.empty() && g_world.m_gameObjectList.empty(),
        "ShowEFST map exit fixture needs an isolated world");
    constexpr char texture[] = "effect\\fire_ivy.bmp";
    const std::string missingTextureKey = std::string("@ck:") + texture;
    Require(ResolveTexturePath(texture).empty() && !g_texMgr.m_missingTexTable.count(missingTextureKey),
        "ShowEFST map exit fixture needs an uncached, missing Fire Ivy texture");

    CGameMode mode;
    mode.m_world = &g_world;
    CMode* const oldMode = SkillPacketReplayAccess::Current();
    const int oldModeType = SkillPacketReplayAccess::CurrentType();
    SkillPacketReplayAccess::SetCurrent(&mode);
    g_session.m_isEffectOn = 1;
    CGameModePacketRouter router;
    RegisterDefaultGameModePacketHandlers(router);
    constexpr u32 gid = 0x11f00002u;
    constexpr u32 fireIvy = 0x200u;
    CGameActor* const actor = SpawnShowEfstReplayActor(mode, router, gid, 1, fireIvy);
    Require(actor->m_isPc != 0, "Fire Ivy fixture did not spawn a PC");
    RequireActorEffectCount(*actor, 394, 1, "Fire Ivy OPT3 snapshot omitted effect 394");

    const int oldSkipFrame = g_isSkipFrame;
    g_isSkipFrame = 1;
    g_world.UpdateGameObjects();
    g_isSkipFrame = oldSkipFrame;
    Require(g_texMgr.m_missingTexTable.count(missingTextureKey) == 1,
        "effect 394 did not process its missing-texture branch");
    DispatchNpcShowEfst(mode, router, gid, 0, 1, fireIvy);
    RequireActorEffectCount(*actor, 394, 1, "refresh stacked a Fire Ivy effect after a missing texture");
    DispatchNpcShowEfst(mode, router, gid, 0, 1, 0);
    g_world.UpdateGameObjects();
    Require(actor->m_effectList.empty() && g_world.m_gameObjectList.empty(),
        "clearing Fire Ivy left a claim or world object after a missing texture");
    DispatchNpcShowEfst(mode, router, gid, 0, 1, fireIvy);
    RequireActorEffectCount(*actor, 394, 1, "Fire Ivy could not reattach after clear and sweep");

    mode.OnExit();
    Require(mode.m_runtimeActors.empty() && mode.m_deferredActorDeletes.empty()
        && g_world.m_actorList.empty() && !mode.m_world,
        "map exit left the actor or its mode ownership alive");
    g_world.UpdateGameObjects();
    Require(g_world.m_gameObjectList.empty(), "map exit left an active ShowEFST world object");
    SkillPacketReplayAccess::Restore(oldMode, oldModeType);
    std::cout << "{\"case_id\":\"npc_show_efst_map_exit_replay\",\"contract_id\":\"PK22.04\","
        "\"missing_texture_processed\":true,\"clear_and_reattach\":true,\"map_exit_swept_effect\":true,\"outcome\":\"PASS\"}\n";
}

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
    CGameActor pcHidden;
    CGameActor bladePc;
    CGameActor bladeNpc;
    CGameActor bladePose15;
    CGameActor bladePose4016;
    CGameActor bladePose4038;
    CGameActor bodyThenBlade;
    CGameActor bladeThenBody;
    nonPc.m_gid = 0x11000001u; nonPc.m_isPc = 0;
    pc.m_gid = 0x11000002u; pc.m_isPc = 1;
    maskFirst.m_gid = 0x11000003u; maskFirst.m_isPc = 0;
    statusFirst.m_gid = 0x11000004u; statusFirst.m_isPc = 0;
    hidden.m_gid = 0x11000005u; hidden.m_isPc = 0;
    packetStateOnly.m_gid = 0x11000006u; packetStateOnly.m_isPc = 0;
    pcHidden.m_gid = 0x11000007u; pcHidden.m_isPc = 1;
    bladePc.m_gid = 0x11000008u; bladePc.m_isPc = 1;
    bladeNpc.m_gid = 0x11000009u; bladeNpc.m_isPc = 0;
    bladePose15.m_gid = 0x1100000au; bladePose15.m_isPc = 1; bladePose15.m_job = 15;
    bladePose4016.m_gid = 0x1100000bu; bladePose4016.m_isPc = 1; bladePose4016.m_job = 4016;
    bladePose4038.m_gid = 0x1100000cu; bladePose4038.m_isPc = 1; bladePose4038.m_job = 4038;
    bodyThenBlade.m_gid = 0x1100000du; bodyThenBlade.m_isPc = 0;
    bladeThenBody.m_gid = 0x1100000eu; bladeThenBody.m_isPc = 0;
    CGameActor* const actors[] = { &nonPc, &pc, &maskFirst, &statusFirst, &hidden, &packetStateOnly, &pcHidden,
        &bladePc, &bladeNpc, &bladePose15, &bladePose4016, &bladePose4038, &bodyThenBlade, &bladeThenBody };
    mode.m_world = &world;
    for (CGameActor* actor : actors) {
        world.m_actorList.push_back(actor);
        mode.m_runtimeActors.emplace(actor->m_gid, actor);
    }
    SkillPacketReplayAccess::SetCurrent(&mode);
    g_session.m_isEffectOn = 1;
    CGameModePacketRouter router;
    RegisterDefaultGameModePacketHandlers(router);

    DispatchNpcShowEfst(mode, router, bladeNpc.m_gid, 0x40u, 99, 0);
    Require(!bladeNpc.m_is99 && bladeNpc.m_effectState == 0,
        "level-99 028A ignored the incoming cloak gate or changed the actor's OPTION");
    DispatchNpcShowEfst(mode, router, bladeNpc.m_gid, 0, 99, 0);
    Require(bladeNpc.m_is99 && bladeNpc.m_99 == 99,
        "uncloaked level-99 028A did not start aura presentation");

    g_session.m_isEffectOn = 0;
    DispatchNpcShowEfst(mode, router, bladeNpc.m_gid, 0, 1, 0x10u);
    RequireActorEffectCount(bladeNpc, 254, 0, "disabled effects did not suppress ShowEFST launch");
    g_session.m_isEffectOn = 1;
    DispatchNpcShowEfst(mode, router, bladeNpc.m_gid, 0, 1, 0x10u);
    RequireActorEffectCount(bladeNpc, 254, 1, "ShowEFST refresh did not retry a suppressed launch");
    DispatchNpcShowEfst(mode, router, bladeNpc.m_gid, 0, 1, 0);
    RequireActorEffectCount(bladeNpc, 254, 0, "ShowEFST clear did not detach the retried effect");

    // Blade-stop is an independent render claim. The three special PC jobs
    // render a fixed pose without mutating the actor's force-animation fields.
    CGameActor* const ordinary[] = { &bladePc, &bladeNpc };
    for (CGameActor* actor : ordinary) {
        actor->SetArgb(137, 121, 91, 41);
        const u32 underlyingTint = actor->GetRenderTint();
        DispatchNpcShowEfst(mode, router, actor->m_gid, 0, 1, 0x20u);
        Require(actor->IsMotionFrozen(), "OPT3 blade-stop did not freeze an ordinary PC/non-PC");
        Require(actor->GetRenderTint() == ((underlyingTint & 0xff000000u) | 0x00404040u),
            "blade-stop gray overlay changed alpha or failed to gray RGB");
        DispatchNpcShowEfst(mode, router, actor->m_gid, 0, 1, 0);
        Require(!actor->IsMotionFrozen(), "OPT3 blade-stop clear retained ordinary freeze");
        Require(actor->GetRenderTint() == underlyingTint,
            "blade-stop clear failed to restore underlying actor tint");
    }
    CGameActor* const poseActors[] = { &bladePose15, &bladePose4016, &bladePose4038 };
    for (CGameActor* actor : poseActors) {
        actor->m_forceAct = 37; actor->m_forceMot = 2;
        actor->m_isForceAnimation = 1; actor->m_isForceAnimFinish = 1;
        actor->SetAction(24, 1, 0);
        DispatchNpcShowEfst(mode, router, actor->m_gid, 0, 1, 0x20u);
        int poseAction = -1, poseMotion = -1;
        Require(actor->GetBladeStopPose(poseAction, poseMotion)
            && poseAction == 96 + actor->Get8Dir(actor->m_roty) && poseMotion == 4,
            "special blade-stop PC job did not expose directional action 96 motion 4");
        Require(actor->m_forceAct == 37 && actor->m_forceMot == 2
            && actor->m_isForceAnimation == 1 && actor->m_isForceAnimFinish == 1,
            "blade-stop pose overwrote independent forced-animation state");
        DispatchNpcShowEfst(mode, router, actor->m_gid, 0, 1, 0);
        Require(!actor->GetBladeStopPose(poseAction, poseMotion), "blade-stop clear retained special pose");
    }

    // Body-freeze and blade-stop must release independently in either order.
    bodyThenBlade.SetArgb(128, 90, 100, 110);
    bodyThenBlade.SetAttrState(1, 0, 0, false);
    DispatchNpcShowEfst(mode, router, bodyThenBlade.m_gid, 0, 1, 0x20u);
    bodyThenBlade.SetAttrState(0, 0, 0, false);
    Require(bodyThenBlade.IsMotionFrozen() && bodyThenBlade.GetRenderTint() == 0x80404040u,
        "clearing body state erased the remaining blade-stop freeze/tint");
    DispatchNpcShowEfst(mode, router, bodyThenBlade.m_gid, 0, 1, 0);
    Require(!bodyThenBlade.IsMotionFrozen() && bodyThenBlade.GetRenderTint() == 0x80ffffffu,
        "blade clear left stale freeze/tint after body-first release");

    bladeThenBody.SetArgb(128, 90, 100, 110);
    DispatchNpcShowEfst(mode, router, bladeThenBody.m_gid, 0, 1, 0x20u);
    bladeThenBody.SetAttrState(2, 0, 0, false);
    DispatchNpcShowEfst(mode, router, bladeThenBody.m_gid, 0, 1, 0);
    Require(bladeThenBody.IsMotionFrozen() && bladeThenBody.GetRenderTint() == 0x800080ffu,
        "clearing blade-stop erased body-owned freeze/tint in reverse order");
    bladeThenBody.SetAttrState(0, 0, 0, false);
    Require(!bladeThenBody.IsMotionFrozen() && bladeThenBody.GetRenderTint() == 0x80ffffffu,
        "body clear did not release body freeze and restore underlying sprite RGB/alpha");

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
    DispatchShowEfstStatus(mode, router, maskFirst.m_gid, 235, true);
    SendShowEfstStatus(maskFirst, 97, true);
    SendShowEfstStatus(maskFirst, 87, true);
    DispatchShowEfstStatus(mode, router, maskFirst.m_gid, 235, true);
    SendShowEfstStatus(maskFirst, 97, true);
    RequireActorEffectCount(maskFirst, 254, 1, "duplicate status-110 claims duplicated effect 254");
    RequireActorEffectCount(maskFirst, 655, 1, "duplicate status-110 claim duplicated effect 655");
    DispatchNpcShowEfst(mode, router, maskFirst.m_gid, 0, 1, 0);
    RequireActorEffectCount(maskFirst, 254, 1, "mask end incorrectly removed shared 254 claims");
    RequireActorEffectCount(maskFirst, 655, 1, "mask end incorrectly removed shared 655 claim");
    SendShowEfstStatus(maskFirst, 87, false);
    RequireActorEffectCount(maskFirst, 254, 1, "status 87 end removed 254 still claimed by status 235");
    DispatchShowEfstStatus(mode, router, maskFirst.m_gid, 235, false);
    RequireActorEffectCount(maskFirst, 254, 0, "last 254 status claim did not remove the shared effect");
    SendShowEfstStatus(maskFirst, 97, false);
    RequireActorEffectCount(maskFirst, 655, 0, "last 655 status claim did not remove the shared effect");
    RequireActorEffectCount(maskFirst, 656, 1, "non-PC 655 status end did not create one terminal 656");

    // Release icon claims first and OPT3 snapshot last; terminal still occurs once.
    DispatchNpcShowEfst(mode, router, statusFirst.m_gid, 0, 1, 0x00010010u);
    SendShowEfstStatus(statusFirst, 87, true);
    DispatchShowEfstStatus(mode, router, statusFirst.m_gid, 235, true);
    SendShowEfstStatus(statusFirst, 97, true);
    SendShowEfstStatus(statusFirst, 87, false);
    DispatchShowEfstStatus(mode, router, statusFirst.m_gid, 235, false);
    SendShowEfstStatus(statusFirst, 97, false);
    RequireActorEffectCount(statusFirst, 254, 1, "status-first order lost OPTION 254 claim");
    RequireActorEffectCount(statusFirst, 655, 1, "status-first order lost OPTION 655 claim");
    DispatchNpcShowEfst(mode, router, statusFirst.m_gid, 0, 1, 0);
    RequireActorEffectCount(statusFirst, 254, 0, "OPTION end did not release shared 254 after status-first order");
    RequireActorEffectCount(statusFirst, 655, 0, "OPTION end did not release shared 655 after status-first order");
    RequireActorEffectCount(statusFirst, 656, 1, "status-first OPTION end did not create one terminal 656");

    // Icon status 188 aliases the second selector's 166 representation. While
    // both owners exist, the OPT3 snapshot wins; after it clears, the icon's
    // option 128 representation remains until its own release.
    DispatchNpcShowEfst(mode, router, pc.m_gid, 0, 1, 0x1u);
    DispatchShowEfstStatus(mode, router, pc.m_gid, 188, true);
    RequireActorEffectCount(pc, 166, 1, "status 188 duplicated the shared 166 snapshot effect");
    Require(EffectPositionEquals(FindActorEffect(pc, 166), 200.0f, 200.0f, 0.0f),
        "OPT3 selector did not retain precedence over status 188's option 128");
    DispatchNpcShowEfst(mode, router, pc.m_gid, 0, 1, 0);
    RequireActorEffectCount(pc, 166, 1, "OPT3 clear removed a remaining status 188 claim");
    Require(EffectPositionEquals(FindActorEffect(pc, 166), 250.0f, 150.0f, 150.0f),
        "remaining status 188 claim did not restore marker option 128");
    DispatchShowEfstStatus(mode, router, pc.m_gid, 188, false);
    RequireActorEffectCount(pc, 166, 0, "status 188 release did not remove its final 166 claim");

    SendShowEfstStatus(pc, 2, true);
    SendShowEfstStatus(pc, 25, true);
    SendShowEfstStatus(pc, 31, true);
    RequireActorEffectCount(pc, 166, 1, "selector status icons stacked shared effect 166");
    Require(EffectPositionEquals(FindActorEffect(pc, 166), 180.0f, 180.0f, 250.0f),
        "latest selector status did not supply its 166 variant");
    SendShowEfstStatus(pc, 2, true);
    Require(EffectPositionEquals(FindActorEffect(pc, 166), 200.0f, 200.0f, 0.0f),
        "refreshing an active selector did not restore its precedence");
    SendShowEfstStatus(pc, 161, true);
    SendShowEfstStatus(pc, 2, false);
    Require(EffectPositionEquals(FindActorEffect(pc, 166), 200.0f, 200.0f, 0.0f),
        "status 2 release removed the variant still owned by status 161");
    SendShowEfstStatus(pc, 161, false);
    SendShowEfstStatus(pc, 31, false);
    Require(EffectPositionEquals(FindActorEffect(pc, 166), 250.0f, 150.0f, 150.0f),
        "ending later selectors did not expose the remaining status 25 variant");
    SendShowEfstStatus(pc, 25, false);
    RequireActorEffectCount(pc, 166, 0, "ending the final selector status left effect 166 attached");

    // Non-PC OPT3 add/remove uses separate terminal effects exactly once.
    DispatchNpcShowEfst(mode, router, nonPc.m_gid, 0, 1, allNonPcBits);
    DispatchNpcShowEfst(mode, router, nonPc.m_gid, 0, 1, 0);
    RequireActorEffectCount(nonPc, 540, 1, "OPT3 bit 0x2 end did not launch terminal 540 once");
    RequireActorEffectCount(nonPc, 656, 1, "OPT3 bit 0x10000 end did not launch terminal 656 once");
    RequireActorEffectCount(nonPc, 420, 0, "OPT3 0x2 effect survived its mask end");
    RequireActorEffectCount(nonPc, 427, 0, "OPT3 0x4 effect survived its mask end");
    RequireActorEffectCount(nonPc, 667, 0, "OPT3 0x8000 effect survived its mask end");

    // PC selector precedence and independent effect mask. The all-bits snapshot
    // must select only the first 166 color variant and still launch each other
    // independently selected effect once.
    constexpr u32 allPcBits = 0x0003ffffu;
    DispatchNpcShowEfst(mode, router, pc.m_gid, 0, 1, allPcBits);
    const int pcEffectIds[] = {166,203,254,418,544,419,424,586,655,474,382,368,394,396,375};
    for (const int id : pcEffectIds) {
        RequireActorEffectCount(pc, id, 1, "PC snapshot omitted or duplicated an expected OPT3 effect");
    }
    RequireActorEffectCount(pc, 617, 0, "PC 0x4000 selector overrode the earlier 0x1 selector");
    Require(EffectPositionEquals(FindActorEffect(pc, 166), 200.0f, 200.0f, 0.0f),
        "PC selector precedence did not choose 0x1 / marker option 130");
    DispatchNpcShowEfst(mode, router, pc.m_gid, 0, 1, allPcBits);
    RequireActorEffectCount(pc, 166, 1, "duplicate PC snapshot stacked selector effect 166");
    DispatchNpcShowEfst(mode, router, pc.m_gid, 0, 1, 0x2u);
    Require(EffectPositionEquals(FindActorEffect(pc, 166), 250.0f, 150.0f, 150.0f),
        "PC 0x2 selector did not set marker option 128");
    DispatchNpcShowEfst(mode, router, pc.m_gid, 0, 1, 0x4u);
    Require(EffectPositionEquals(FindActorEffect(pc, 166), 180.0f, 180.0f, 250.0f),
        "PC 0x4 selector did not set marker option 169");
    DispatchNpcShowEfst(mode, router, pc.m_gid, 0, 1, 0x4000u);
    RequireActorEffectCount(pc, 166, 0, "PC 0x4000 selector retained obsolete 166");
    RequireActorEffectCount(pc, 617, 1, "PC 0x4000 selector omitted effect 617");
    DispatchNpcShowEfst(mode, router, pc.m_gid, 0, 1, 0);
    for (const int id : pcEffectIds) {
        RequireActorEffectCount(pc, id, 0, "PC OPT3 mask clear retained an effect claim");
    }
    RequireActorEffectCount(pc, 617, 0, "PC OPT3 mask clear retained effect 617");
    RequireActorEffectCount(pc, 656, 0, "PC OPTION mask end incorrectly launched non-PC terminal 656");

    // Status-110 icons overlap these OPT3-owned effects. Release the snapshot
    // first and require each icon claim (including status 86's extra 166) to
    // survive until its own status end.
    struct SharedIconOverlap {
        int status;
        u32 opt3;
        std::vector<int> effects;
    };
    const SharedIconOverlap sharedIconOverlaps[] = {
        {86, 0x8u, {203, 166}},
        {103, 0x40u, {367, 474}},
        {121, 0x400u, {396}}, {122, 0x400u, {396}},
        {107, 0x80u, {368}}, {113, 0x100u, {382}},
        {123, 0x200u, {394}}, {153, 0x2000u, {419}},
        {165, 0x1000u, {418, 544}}, {166, 0x1000u, {418, 544}},
        {167, 0x1000u, {418, 544}}, {232, 0x1000u, {418, 544}},
        {207, 0x4000u, {617}}, {263, 0x20000u, {586}},
    };
    for (const SharedIconOverlap& overlap : sharedIconOverlaps) {
        SendShowEfstStatus(pc, overlap.status, true);
        DispatchNpcShowEfst(mode, router, pc.m_gid, 0, 1, overlap.opt3);
        for (const int id : overlap.effects) {
            RequireActorEffectCount(pc, id, 1,
                "shared icon/OPT3 overlap omitted or duplicated a requested effect");
        }
        DispatchNpcShowEfst(mode, router, pc.m_gid, 0, 1, 0);
        for (const int id : overlap.effects) {
            RequireActorEffectCount(pc, id, 1,
                "OPT3 clear removed a still-active status-110 shared effect claim");
        }
        SendShowEfstStatus(pc, overlap.status, false);
        for (const int id : overlap.effects) {
            RequireActorEffectCount(pc, id, 0,
                "status-110 end retained a shared effect after its final claim");
        }
    }
    Require(pc.LaunchEffect(395) != nullptr, "could not stage status 121's independent effect 395");
    SendShowEfstStatus(pc, 121, true);
    RequireActorEffectCount(pc, 395, 0, "status 121 did not release its independent effect 395");
    SendShowEfstStatus(pc, 121, false);

    CRagEffect* const external418 = pc.LaunchEffect(418);
    Require(external418 != nullptr, "could not stage an existing effect 418");
    DispatchNpcShowEfst(mode, router, pc.m_gid, 0, 1, 0x1000u);
    RequireActorEffectCount(pc, 418, 1, "OPT3 stacked an existing 418");
    RequireActorEffectCount(pc, 544, 0, "OPT3 started companion 544 despite the existing 418 guard");
    SendShowEfstStatus(pc, 165, true);
    RequireActorEffectCount(pc, 544, 1, "existing 418 blocked an independent status claim on 544");
    DispatchNpcShowEfst(mode, router, pc.m_gid, 0, 1, 0);
    RequireActorEffectCount(pc, 544, 1, "OPT3 clear detached icon-owned companion 544");
    SendShowEfstStatus(pc, 165, false);
    RequireActorEffectCount(pc, 544, 0, "icon release left companion 544 attached");
    Require(FindActorEffect(pc, 418) == external418, "shared claim release detached unrelated 418");
    pc.SendMsg(&pc, 109, 418, 0, 0);

    // Hidden PC OPT3 suppresses these four effects and reveals them again from
    // the retained snapshot; the non-PC set differs (tested below).
    constexpr u32 pcHideSensitive = 0x00021018u; // 203, 254, 418, 586.
    pcHidden.SetAttrState(0, 0x4, 0, false);
    Require(pcHidden.LaunchEffect(203) && pcHidden.LaunchEffect(203),
        "could not stage unrelated instances for hidden ShowEFST cleanup");
    DispatchNpcShowEfst(mode, router, pcHidden.m_gid, 0, 1, pcHideSensitive);
    RequireActorEffectCount(pcHidden, 203, 1, "hidden snapshot did not remove exactly one unrelated 203");
    DispatchNpcShowEfst(mode, router, pcHidden.m_gid, 0, 1, pcHideSensitive);
    for (const int id : {203,254,418,586}) {
        RequireActorEffectCount(pcHidden, id, 0, "hidden PC retained a hide-sensitive OPT3 effect");
    }
    pcHidden.SetAttrState(0, 0, 0, false);
    for (const int id : {203,254,418,586}) {
        RequireActorEffectCount(pcHidden, id, 1, "revealed PC did not restore a hide-sensitive OPT3 effect");
    }

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

    DispatchNpcShowEfst(mode, router, pc.m_gid, 0, 1, 0x800u);
    SendShowEfstStatus(pc, 110, false);
    RequireActorEffectCount(pc, 375, 1,
        "removal-only status 110 removed the remaining OPT3-owned Assumptio claim");
    DispatchNpcShowEfst(mode, router, pc.m_gid, 0, 1, 0);
    RequireActorEffectCount(pc, 375, 0,
        "clearing the final OPT3-owned Assumptio claim retained effect 375");

    CRagEffect* const external375 = pc.LaunchEffect(375);
    Require(external375, "unrelated 375 effect setup failed");
    SendShowEfstStatus(pc, 110, false);
    RequireActorEffectCount(pc, 375, 0,
        "removal-only status 110 did not remove an unclaimed Assumptio effect");
    SendShowEfstStatus(pc, 110, true);
    RequireActorEffectCount(pc, 375, 0,
        "removal-only status 110 incorrectly created an Assumptio effect on activation");

    const size_t worldEffectsBeforeUnknown = world.m_gameObjectList.size();
    DispatchNpcShowEfst(mode, router, 0xdeadbeefu, 0, 1, allNonPcBits);
    Require(mode.m_runtimeActors.size() == std::size(actors)
        && world.m_gameObjectList.size() == worldEffectsBeforeUnknown,
        "unknown actor 028A created actor or effect state");

    // Isolated heap-owned actor lifetime through selected 0079 spawn, 028A
    // refresh, and 0080 vanish. The selected 0079 layout carries OPT3@42.
    CGameMode lifecycleMode;
    lifecycleMode.m_world = &g_world;
    CMode* const modeBeforeLifecycle = SkillPacketReplayAccess::Current();
    const int modeTypeBeforeLifecycle = SkillPacketReplayAccess::CurrentType();
    std::list<CGameActor*> savedGlobalActors;
    std::list<CGameObject*> savedGlobalObjects;
    savedGlobalActors.swap(g_world.m_actorList);
    savedGlobalObjects.swap(g_world.m_gameObjectList);
    struct RestoreShowEfstLifecycle {
        CGameMode& mode;
        std::list<CGameActor*>& actors;
        std::list<CGameObject*>& objects;
        CMode* oldMode;
        int oldModeType;
        ~RestoreShowEfstLifecycle()
        {
            for (auto& entry : mode.m_runtimeActors) {
                if (mode.m_world) {
                    mode.m_world->NotifyActorDeleted(entry.second);
                    mode.m_world->UnregisterActor(entry.second);
                }
                delete entry.second;
            }
            mode.m_runtimeActors.clear();
            for (CGameActor* actor : mode.m_deferredActorDeletes) delete actor;
            mode.m_deferredActorDeletes.clear();
            if (mode.m_world) mode.m_world->UpdateGameObjects();
            actors.swap(g_world.m_actorList);
            objects.swap(g_world.m_gameObjectList);
            mode.m_world = nullptr;
            SkillPacketReplayAccess::Restore(oldMode, oldModeType);
        }
    } restoreLifecycle{lifecycleMode, savedGlobalActors, savedGlobalObjects,
        modeBeforeLifecycle, modeTypeBeforeLifecycle};
    SkillPacketReplayAccess::SetCurrent(&lifecycleMode);
    constexpr u32 lifecycleGid = 0x11f00001u;
    CGameActor* const spawnedActor = SpawnShowEfstReplayActor(lifecycleMode, router, lifecycleGid, 1002, 0x30);
    Require(spawnedActor->IsMotionFrozen(),
        "selected 0x0079 spawn did not instantiate heap actor with its OPT3 snapshot");
    RequireActorEffectCount(*spawnedActor, 254, 1,
        "selected 0x0079 OPT3 spawn omitted its owned effect 254");
    DispatchNpcShowEfst(lifecycleMode, router, lifecycleGid, 0, 1, 0x30u);
    RequireActorEffectCount(*spawnedActor, 254, 1,
        "real 0x028A refresh stacked the spawned actor's owned effect");
    CRagEffect* const detachedEffect = FindActorEffect(*spawnedActor, 254);

    std::array<u8, 7> vanish{0x80, 0};
    for (unsigned byte = 0; byte < 4; ++byte) {
        vanish[2 + byte] = static_cast<u8>(lifecycleGid >> (byte * 8));
    }
    vanish[6] = 0; // ordinary vanish/removal
    PacketView vanishView{};
    int vanishConsumed = 0;
    Require(TryReadPacket(vanish.data(), static_cast<int>(vanish.size()), vanishView, vanishConsumed)
        && vanishConsumed == static_cast<int>(vanish.size()) && vanishView.packetId == 0x0080
        && router.Dispatch(lifecycleMode, vanishView),
        "literal 0x0080 vanish did not dispatch");
    Require(lifecycleMode.m_runtimeActors.count(lifecycleGid) == 0
        && lifecycleMode.m_deferredActorDeletes.size() == 1
        && std::find(g_world.m_actorList.begin(), g_world.m_actorList.end(), spawnedActor) == g_world.m_actorList.end()
        && spawnedActor->m_effectList.empty(),
        "0x0080 vanish did not remove the actor and detach its effect before deferred deletion");
    Require(g_world.m_gameObjectList.size() == 1 && detachedEffect && detachedEffect->OnProcess() == 0,
        "0x0080 vanish did not leave the detached effect to the world sweep");
    g_world.UpdateGameObjects();
    // The frame can sweep detached effects before deferred actor deletion.
    delete lifecycleMode.m_deferredActorDeletes.front();
    lifecycleMode.m_deferredActorDeletes.clear();
    Require(g_world.m_actorList.empty() && g_world.m_gameObjectList.empty()
        && lifecycleMode.m_runtimeActors.empty() && lifecycleMode.m_deferredActorDeletes.empty(),
        "normal world sweep retained the vanished actor or detached effect");
    std::cout << "{\"case_id\":\"npc_show_efst_replay\",\"contract_id\":\"PK22.02\",\"lifecycle_contract_id\":\"PK22.04\",\"literal_028a\":true,\"spawn_vanish_lifecycle\":{\"spawn\":\"0079\",\"spawn_opt3\":true,\"refresh\":\"028A\",\"vanish\":\"0080\",\"owned_effect_detached\":true,\"normal_world_cleanup\":true},\"blade_stop\":{\"ordinary_freeze\":true,\"special_jobs\":[15,4016,4038],\"forced_fields_preserved\":true,\"gray_rgb_alpha_preserved\":true,\"body_release_orders\":2},\"nonpc_effects\":[420,427,254,655,667],\"pc_effect_matrix\":[166,203,254,418,544,419,424,586,655,474,382,368,394,396,375,617],\"selector_options\":[130,128,169],\"shared_claim_end_orders\":2,\"shared_icon_overlap_statuses\":[86,103,121,122,107,113,123,153,165,166,167,232],\"status188_snapshot_dual_release\":true,\"terminal_effects\":[540,656],\"hidden_pc_effects\":[203,254,418,586],\"removal_only_110_375\":true,\"unknown_actor_ignored\":true,\"outcome\":\"PASS\"}" << std::endl;
}
