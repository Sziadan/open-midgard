namespace {

std::vector<u8> NpcProgressPacket(u16 opcode, u32 color, u32 seconds)
{
    std::vector<u8> packet(10, 0);
    packet[0] = static_cast<u8>(opcode);
    packet[1] = static_cast<u8>(opcode >> 8);
    for (size_t i = 0; i < 4; ++i) {
        packet[2 + i] = static_cast<u8>(color >> (i * 8));
        packet[6 + i] = static_cast<u8>(seconds >> (i * 8));
    }
    return packet;
}

void FeedNpcProgressPacket(CGameMode& mode, LoopbackPeer& peer, const std::vector<u8>& packet)
{
    peer.Send(packet);
    const auto deadline = Clock::now() + kTimeout;
    while (Clock::now() < deadline) {
        int pending = 0;
#if RO_PLATFORM_WINDOWS
        Require(ioctlsocket(PacketStreamReplayAccess::Native(*CRagConnection::instance()), FIONREAD,
            reinterpret_cast<u_long*>(&pending)) == 0, "NPC progress FIONREAD failed");
#else
        Require(ioctl(PacketStreamReplayAccess::Native(*CRagConnection::instance()), FIONREAD, &pending) == 0,
            "NPC progress FIONREAD failed");
#endif
        if (pending >= static_cast<int>(packet.size())) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    Require(Clock::now() < deadline, "NPC progress bytes did not reach receive socket");
    CGameModePacketRouter& router = GetProductionGameModePacketRouter();
    const GameModePacketLoopResult result = ProcessGameModePacketLoop(mode, *CRagConnection::instance(), router, 1);
    Require(result.packetsProcessed == 1 && result.packetsDispatched == 1
        && result.packetsUnhandled == 0 && PacketStreamReplayAccess::BufferedBytes(*CRagConnection::instance()) == 0,
        "NPC progress fixture did not traverse production receive/router path");
}

} // namespace

void NpcProgressReplay()
{
    SetCase("npc_progress_replay", "PK21.02", "PK21-02-selected-20080910-progressbar-02f0-02f2",
        "production receive/router -> NPC progress gauge -> production 0x02f1 send");
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    CMode* const oldMode = SkillPacketReplayAccess::Current();
    const int oldModeType = SkillPacketReplayAccess::CurrentType();
    CGameMode mode;
    CWorld world;
    CPlayer player;
    player.m_gid = 0x10203040u;
    player.m_isVisible = 1;
    world.m_player = &player;
    mode.m_world = &world;
    SkillPacketReplayAccess::SetCurrent(&mode);
    CGameModePacketRouter& router = GetProductionGameModePacketRouter();
    router.Clear();
    RegisterDefaultGameModePacketHandlers(router);

    {
        LoopbackPeer peer(*connection);
        FeedNpcProgressPacket(mode, peer, NpcProgressPacket(0x02f0, 0x11223344u, 5));
        UIRechargeGage* const gauge = mode.GetNpcProgressGauge();
        Require(gauge && gauge->GetTotalAmount() == 5000 && gauge->GetAmount() == 0,
            "fixed 10-byte 0x02f0 did not create a five-second gauge");
        // The routed 0x02f0 path deliberately uses wall-clock time. Reset with
        // an explicit tick to exercise the timer boundary deterministically.
        mode.StartNpcProgress(5, 1000);
        UIRechargeGage* const timedGauge = mode.GetNpcProgressGauge();
        Require(timedGauge && !timedGauge->m_show,
            "NPC progress expiry replay requires an unprojected hidden gauge");
        mode.UpdateNpcProgress(*connection, 2000);
        Require(timedGauge->GetAmount() == 1000 && timedGauge->GetTotalAmount() == 5000,
            "NPC progress amount did not track relative elapsed ticks");
        mode.UpdateNpcProgress(*connection, 5999);
        Require(timedGauge->GetAmount() == 4999 && timedGauge->GetTotalAmount() == 5000,
            "NPC progress gauge missed the pre-expiry boundary");
        mode.UpdateNpcProgress(*connection, 6000);
        Require(mode.GetNpcProgressGauge() == timedGauge && timedGauge->GetAmount() == 5000,
            "NPC progress did not remain complete at the exact inclusive end tick");
        Require(!SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(0)),
            "NPC progress acknowledged before the first tick after its inclusive end");
        mode.UpdateNpcProgress(*connection, 6001);
        Require(mode.GetNpcProgressGauge() == nullptr,
            "NPC progress gauge remained active after its inclusive end tick");
        Require(peer.ReceiveExact(2) == std::vector<u8>({0xf1, 0x02}),
            "NPC progress expiry did not emit exactly one 0x02f1 acknowledgement");
        mode.UpdateNpcProgress(*connection, 7000);
        Require(!SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(0)),
            "NPC progress expiry emitted a duplicate acknowledgement");
    }

    {
        LoopbackPeer peer(*connection);
        mode.StartNpcProgress(8, 10000);
        UIRechargeGage* const first = mode.GetNpcProgressGauge();
        Require(first && first->GetTotalAmount() == 8000, "replacement setup progress gauge missing");
        mode.UpdateNpcProgress(*connection, 11000);
        Require(first->GetAmount() == 1000, "replacement setup elapsed amount incorrect");
        FeedNpcProgressPacket(mode, peer, NpcProgressPacket(0x02f0, 0x01020304u, 4));
        UIRechargeGage* const replacement = mode.GetNpcProgressGauge();
        Require(replacement && replacement == first && replacement->GetTotalAmount() == 4000
            && replacement->GetAmount() == 0, "new 0x02f0 did not restart progress");

        player.SendMsg(&player, 82, 30000, 0, 0);
        UIRechargeGage* const castGauge = player.m_skillRechargeGage;
        matrix view{};
        view.m[0][0] = view.m[1][1] = view.m[2][2] = view.m[3][3] = 1.0f;
        view.m[3][2] = 500.0f;
        world.ProcessActorSkillRechargeGages(view, 0, replacement);
        Require(castGauge && replacement->m_show && replacement->m_w == 60 && replacement->m_h == 6
            && replacement->m_y + replacement->m_h < castGauge->m_y,
            "NPC progress gauge did not project above the ordinary casting gauge");
#if RO_ENABLE_QT6_UI
        QtUiStateAdapter adapter;
        Require(adapter.syncGameplay(mode, RenderBackendType::LegacyDirect3D7,
                RenderBackendType::LegacyDirect3D7, -1, -1)
            && adapter.stateObject()->property("rechargeGaugeVisible").toBool()
            && adapter.stateObject()->property("rechargeGaugeY").toInt() == replacement->m_y
            && adapter.stateObject()->property("rechargeGaugeTotal").toInt() == 4000,
            "Qt projection did not select the local NPC progress gauge");
#endif
        FeedNpcProgressPacket(mode, peer, {0xf2, 0x02});
        Require(mode.GetNpcProgressGauge() == nullptr, "0x02f2 did not clear active progress gauge");
        Require(player.m_skillRechargeGage == castGauge, "NPC abort cleared an ordinary casting gauge");
        player.SendMsg(&player, 83, 0, 0, 0);
        Require(peer.ReceiveExact(2) == std::vector<u8>({0xf1, 0x02}),
            "active 0x02f2 did not emit one 0x02f1 acknowledgement");
        mode.UpdateNpcProgress(*connection, 20000);
        Require(!SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(0)),
            "cancelled NPC progress emitted a duplicate acknowledgement");
        mode.StartNpcProgress(2, 30000);
        mode.ClearNpcProgress();
        Require(mode.GetNpcProgressGauge() == nullptr, "silent progress clear left an active gauge");
        Require(!SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(0)),
            "silent progress clear emitted an acknowledgement");
    }

    mode.StartNpcProgress(4, timeGetTime());
    UIRechargeGage* const disconnectGauge = mode.GetNpcProgressGauge();
    connection->Disconnect();
    Require(disconnectGauge && !mode.ProcessNetworkUpdate(*connection, router, 1, GetTickCount())
        && !mode.GetNpcProgressGauge() && !g_windowMgr.ContainsWindow(disconnectGauge),
        "terminal network update retained the NPC progress gauge");
    SkillPacketReplayAccess::Restore(oldMode, oldModeType);
    connection->Disconnect();
    std::cout << "{\"case_id\":\"npc_progress_replay\",\"contract_id\":\"PK21.02\",\"packet_02f0_fixed_10\":true,\"color_ignored\":true,\"relative_gauge_ticks\":true,\"expiry_ack_once\":true,\"cancel_ack_once\":true,\"replacement_restarts\":true,\"silent_clear\":true,\"outcome\":\"PASS\"}" << std::endl;
}
