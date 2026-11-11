// Included by ItemColourRenderChecks.cpp.
#include "ui/UIChooseWnd.h"
#include "world/GameActor.h"

namespace {
QQuickItem* FindRecoveryLabel(QQuickItem* root)
{
    if (!root) return nullptr;
    if (root->property("text").toString() == QStringLiteral("Use Token of Siegfried")) {
        return root;
    }
    for (auto* child : root->childItems()) {
        if (auto* label = FindRecoveryLabel(child)) return label;
    }
    return nullptr;
}
}

void CheckStandingResurrectionMenu(QGuiApplication& app, QQuickView& view,
    QtUiState& state, CGameMode& mode, UIItemWnd& inventory, QtUiStateAdapter& adapter,
    const QString& output)
{
    QDir textures(QString::fromUtf8(RO_PACKET_SOURCE_DIR) + "/Ref/GRF-Content/data/texture");
    QString bitmapDirectory;
    for (const auto& directory : textures.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (textures.exists(directory + "/esc_05a.bmp")) {
            bitmapDirectory = textures.absoluteFilePath(directory);
            break;
        }
    }
    Require(!bitmapDirectory.isEmpty(), "source recovery menu bitmap directory missing");
    for (int entry = 1; entry <= 5; ++entry) {
        for (const QChar variant : QStringLiteral("abc")) {
            const QString name = QString("esc_0%1%2.bmp").arg(entry).arg(variant);
            const QString target = output + "/" + name;
            if (QFile::exists(target)) QFile::remove(target);
            Require(QFile::copy(bitmapDirectory + "/" + name, target), "stage source recovery bitmap failed");
        }
    }

    inventory.SetShow(0);
    g_windowMgr.m_storageWnd->SetShow(0);
    ro::commerce::state.cart.visible = false;
    CPlayer player;
    player.m_gid = 0x12345678;
    g_session.m_gid = player.m_gid;
    mode.m_world->m_player = &player;
    mode.m_runtimeActors[player.m_gid] = &player;
    ITEM_INFO token{};
    token.m_itemIndex = 7;
    token.m_num = 1;
    token.SetItemId(7621);
    g_session.SetInventoryItem(token);

    auto& router = GetProductionGameModePacketRouter();
    const auto dispatch = [&](const auto& bytes) {
        PacketView packet{};
        int consumed = 0;
        Require(TryReadPacket(bytes.data(), static_cast<int>(bytes.size()), packet, consumed)
            && consumed == static_cast<int>(bytes.size()) && router.Dispatch(mode, packet),
            "recovery menu fixture failed to route a packet");
    };
    dispatch(std::array<u8, 7>{0x80, 0, 0x78, 0x56, 0x34, 0x12, 1});
    auto* menu = g_windowMgr.m_chooseWnd;
    Require(mode.m_isPlayerDead && menu && menu->m_show, "local death did not open the recovery menu");
    menu->Move(20, 20);
    const QRect fifthRow(29, 124, 221, 20);
    QImage eligibleQt;
    QJsonArray results;

    for (const bool eligible : {true, false}) {
        if (!eligible) {
            dispatch(std::array<u8, 4>{0x99, 1, 2, 0}); // Legacy event PvP
        }
        Require(adapter.syncGameplay(mode, RenderBackendType::LegacyDirect3D7,
            RenderBackendType::LegacyDirect3D7, -1, -1), "recovery menu Qt projection failed");
        Frames(app, view);
        Require(state.chooseMenuVisible() && state.chooseMenuOptions().size() == (eligible ? 5 : 4)
            && menu->m_h == (eligible ? 136 : 113), "recovery menu geometry or option count mismatch");
        const auto* label = FindRecoveryLabel(view.rootObject());
        Require((label && label->isVisible()) == eligible, "recovery label visibility mismatch");
        const QImage qt = view.grabWindow().convertToFormat(QImage::Format_RGB32);
        Require(!qt.isNull() && qt.rect().contains(fifthRow), "recovery menu Qt capture failed");
        int qtChanged = 0;
        if (eligible) {
            eligibleQt = qt;
        } else {
            for (int y = fifthRow.top(); y <= fifthRow.bottom(); ++y) {
                for (int x = fifthRow.left(); x <= fifthRow.right(); ++x) {
                    qtChanged += qt.pixel(x, y) != eligibleQt.pixel(x, y);
                }
            }
            Require(qtChanged > 100, "Qt recovery row did not change the captured frame");
        }

        ArgbDibSurface surface;
        Require(surface.EnsureSize(view.width(), view.height()), "recovery menu native DIB failed");
        std::fill_n(surface.GetPixels(), view.width() * view.height(), 0xff181b20u);
        menu->DrawNativeToHdc(surface.GetDC());
        const QImage native = QImage(reinterpret_cast<uchar*>(surface.GetPixels()), view.width(), view.height(),
            view.width() * 4, QImage::Format_RGB32).copy();
        const int nativeInk = PixelsNear(native, fifthRow, qRgb(0, 0, 0));
        Require((nativeInk > 100) == eligible, "native recovery bitmap ink did not follow eligibility");
        if (!eligible) {
            const QRect outsidePanel(29, 133, 221, 11);
            Require(PixelsNear(native, outsidePanel, qRgb(24, 27, 32)) == outsidePanel.width() * outsidePanel.height(),
                "restricted native menu painted beyond the fourth entry");
        }

        const QString stem = output + (eligible ? "/recovery-menu-eligible" : "/recovery-menu-restricted");
        Require(qt.save(stem + ".qml.png") && native.save(stem + ".native.png"), "recovery menu capture write failed");
        results.push_back(QJsonObject{{"eligible", eligible}, {"height", menu->m_h},
            {"nativeTokenInk", nativeInk}, {"qtChangedPixels", qtChanged}});
    }
    QFile result(output + "/recovery-menu-render-results.json");
    Require(result.open(QIODevice::WriteOnly), "open recovery render report failed");
    result.write(QJsonDocument(results).toJson(QJsonDocument::Indented));
    mode.m_runtimeActors.clear();
    mode.m_world->m_player = nullptr;
}
