void CheckNpcProgressRender(QGuiApplication& app, QQuickView& view, QtUiState* state,
    CGameMode& mode, CWorld& world, CView& camera, QtUiStateAdapter& adapter, const QString& output)
{
    CPlayer player;
    CPlayer* const previousPlayer = world.m_player;
    world.m_player = &player;
    player.m_gid = 0x11223344u;
    player.m_isVisible = 1;
    player.m_isPc = 1;
    mode.m_world = &world;
    Dispatch(mode, {0xf0, 0x02, 0x44, 0x33, 0x22, 0x11, 0xd0, 0x07, 0x00, 0x00});
    UIRechargeGage* const gauge = mode.GetNpcProgressGauge();
    Require(gauge != nullptr, "production 0x02f0 did not create NPC progress gauge");
    mode.StartNpcProgress(2, 10000);
    mode.UpdateNpcProgress(*CRagConnection::instance(), 11000);
    Require(gauge->GetAmount() == 1000 && gauge->GetTotalAmount() == 2000,
        "NPC progress render fixture did not reach deterministic half progress");

    matrix viewMatrix{};
    viewMatrix.m[0][0] = viewMatrix.m[1][1] = viewMatrix.m[2][2] = viewMatrix.m[3][3] = 1.0f;
    viewMatrix.m[3][2] = 500.0f;
    const int previousXOffset = g_renderer.m_xoffset;
    const int previousYOffset = g_renderer.m_yoffset;
    struct RestoreProjectionOffsets {
        int x;
        int y;
        ~RestoreProjectionOffsets() { g_renderer.m_xoffset = x; g_renderer.m_yoffset = y; }
    } restoreOffsets{previousXOffset, previousYOffset};
    g_renderer.m_xoffset = Width / 2;
    g_renderer.m_yoffset = Height / 2;
    world.ProcessActorSkillRechargeGages(viewMatrix, camera.GetCameraLongitude(), gauge);
    Require(gauge->m_show != 0 && gauge->m_x >= 0 && gauge->m_y >= 0
        && gauge->m_x + gauge->m_w <= Width && gauge->m_y + gauge->m_h <= Height,
        "world projection did not place the visible NPC progress gauge in the viewport");

    ArgbDibSurface surface;
    Require(surface.EnsureSize(Width, Height), "NPC progress native DIB allocation failed");
    const auto paintNative = [&] {
        std::fill_n(surface.GetPixels(), Width * Height, 0xff181b20u);
        for (auto* window : g_windowMgr.m_children) {
            if (auto* managed = dynamic_cast<UIRechargeGage*>(window)) {
                managed->DrawNativeToHdc(surface.GetDC());
            }
        }
        return QImage(reinterpret_cast<uchar*>(surface.GetPixels()), Width, Height,
            Width * 4, QImage::Format_RGB32).copy();
    };
    const QImage native = paintNative();
    Require(Pixels(native, QRect(gauge->m_x + 4, gauge->m_y + 2, 1, 1), 0xff307830u) > 0
        && Pixels(native, QRect(gauge->m_x + 45, gauge->m_y + 2, 1, 1), 0xfff4efe4u) > 0
        && Pixels(native, QRect(gauge->m_x, gauge->m_y + 2, 1, 1), 0xff523f2du) > 0,
        "native progress gauge missed half-fill, background, or edge pixels");
    Require(native.save(output + "/npc-progress-native-half.png"), "save native NPC progress gauge failed");

    Require(state != nullptr, "NPC progress render state missing");
    Require(adapter.syncGameplay(mode, RenderBackendType::LegacyDirect3D7,
        RenderBackendType::LegacyDirect3D7, -1, -1), "Qt gameplay adapter did not sync NPC progress gauge");
    auto* item = Find(view.rootObject(), "npcRechargeGauge");
    Require(item != nullptr, "actual QML NPC progress gauge item missing");
    Require(state->rechargeGaugeVisible() && state->rechargeGaugeX() == gauge->m_x
        && state->rechargeGaugeY() == gauge->m_y && state->rechargeGaugeWidth() == 60
        && state->rechargeGaugeHeight() == 6 && state->rechargeGaugeAmount() == 1000
        && state->rechargeGaugeTotal() == 2000,
        "Qt gameplay adapter did not project the half-filled NPC progress gauge");
    Frames(app, view);
    const QRect bounds = item->mapRectToScene(item->boundingRect()).toAlignedRect().intersected(QRect(0, 0, Width, Height));
    const QImage qtHalf = view.grabWindow().convertToFormat(QImage::Format_RGB32);
    Require(item->isVisible() && !bounds.isEmpty()
        && PixelsNear(qtHalf, QRect(gauge->m_x + 4, gauge->m_y + 2, 1, 1), 0xff537da2u) > 0
        && PixelsNear(qtHalf, QRect(gauge->m_x + 45, gauge->m_y + 2, 1, 1), 0xffffffffu) > 0,
        "actual QML gauge missed half-fill or background pixels");
    Require(qtHalf.save(output + "/npc-progress-qt-half.png"), "save QML NPC progress gauge failed");

    Dispatch(mode, {0xf2, 0x02});
    Require(mode.GetNpcProgressGauge() == nullptr, "production 0x02f2 did not remove NPC progress gauge");
    Require(adapter.syncGameplay(mode, RenderBackendType::LegacyDirect3D7,
        RenderBackendType::LegacyDirect3D7, -1, -1), "Qt gameplay adapter did not sync progress cancellation");
    Frames(app, view);
    const QImage qtCancelled = view.grabWindow().convertToFormat(QImage::Format_RGB32);
    Require(!item->isVisible() && PixelsNear(qtCancelled, bounds, 0xff537da2u) == 0,
        "QML progress gauge remained visible after production 0x02f2");
    Require(qtCancelled.save(output + "/npc-progress-qt-cancelled.png"), "save cancelled QML gauge frame failed");
    const QImage nativeCancelled = paintNative();
    Require(Pixels(nativeCancelled, bounds, 0xff307830u) == 0
        && Pixels(nativeCancelled, bounds, 0xfff4efe4u) == 0,
        "native gauge paint still contained the cancelled NPC progress gauge");
    Require(nativeCancelled.save(output + "/npc-progress-native-cancelled.png"),
        "save cancelled native gauge frame failed");
    mode.ClearNpcProgress();
    world.m_player = previousPlayer;
}
