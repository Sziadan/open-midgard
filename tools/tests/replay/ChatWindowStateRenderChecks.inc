#include "ui/UINewChatWnd.h"

void CheckChatWindowStateUi(QGuiApplication& app, QQuickView& view, QtUiState* state,
    CGameMode& mode, QtUiStateAdapter& adapter, const QString& output)
{
    Require(state != nullptr, "chat state Qt view missing");
    SetRuntimeUiScalePercent(100);
    if (g_windowMgr.m_itemWnd) g_windowMgr.m_itemWnd->SetShow(0);
    if (g_windowMgr.m_storageWnd) g_windowMgr.m_storageWnd->SetShow(0);
    ro::commerce::state.cart.visible = false;
    g_windowMgr.EnsureChatWindowVisible();
    auto* chat = g_windowMgr.m_chatWnd;
    chat->Resize(600, chat->m_h);
    chat->SetWindowFocused(true);
    chat->SetQtInputText(UINewChatWnd::InputField_WhisperTarget, "SourceTarget");
    chat->SetQtInputText(UINewChatWnd::InputField_Message, "compact editor remains available");
    g_windowMgr.ClearChatEvents();
    g_windowMgr.PushChatEvent("SOURCE CHAT HISTORY MARKER", 0x00ff2020, 0);
    const auto redPixels = [](const QImage& image, QRect area) {
        int count = 0;
        area = area.intersected(image.rect());
        for (int y = area.top(); y <= area.bottom(); ++y)
            for (int x = area.left(); x <= area.right(); ++x) {
                const auto color = image.pixelColor(x, y);
                if (color.red() > 150 && color.green() < 120 && color.blue() < 120) ++count;
            }
        return count;
    };
    QJsonArray cases;
    for (const int value : {0, 1, 7, 15}) {
        chat->SetState(value);
        chat->Move(20, view.height() - 20 - chat->m_h);
        chat->OnProcess();
        Require(adapter.syncGameplay(mode, RenderBackendType::LegacyDirect3D7,
            RenderBackendType::LegacyDirect3D7, -1, -1), "chat state gameplay projection failed");
        Frames(app, view);
        auto* history = Find(view.rootObject(), "chatHistoryPane");
        auto* whisper = Find(view.rootObject(), "chatWhisperField");
        auto* message = Find(view.rootObject(), "chatMessageField");
        Require(history && whisper && message, "actual chat QML regions missing");
        Require(history->isVisible() == (value != 0) && whisper->isVisible() && message->isVisible(),
            "compact chat hid editors or retained history");
        const auto rects = chat->GetLayoutDisplay();
        const auto editorTop = message->mapToScene(QPointF(0, 0));
        Require(qAbs(editorTop.x() - chat->m_x - rects.messageInput.left) < .01
            && qAbs(editorTop.y() - chat->m_y - rects.messageInput.top) < .01
            && qRound(message->width()) == 460 && qRound(message->height()) == 16,
            "actual QML editor differs from native source rectangle");
        const auto point = message->mapToScene(QPointF(10, 8));
        Require(chat->HandleQtMouseDown(qRound(point.x()), qRound(point.y())), "source editor pointer rejected");
        chat->HandleQtMouseUp(qRound(point.x()), qRound(point.y()));
        Require(chat->IsMessageInputActive(), "source editor pointer failed to focus message input");
        adapter.syncChat();
        Frames(app, view);
        auto* editor = Find(view.rootObject(), "chatMessageEditor");
        Require(editor && editor->hasActiveFocus(), "actual Qt editor did not receive native owner focus");

        const QImage qt = view.grabWindow().convertToFormat(QImage::Format_RGB32);
        const QRect historyBounds(chat->m_x + rects.history.left, chat->m_y + rects.history.top,
            rects.history.right - rects.history.left, rects.history.bottom - rects.history.top);
        const int qtRed = redPixels(qt, historyBounds);
        ArgbDibSurface surface;
        Require(surface.EnsureSize(view.width(), view.height()), "native chat state DIB failed");
        std::fill_n(surface.GetPixels(), view.width() * view.height(), 0xff181b20u);
        chat->DrawNativeToHdc(surface.GetDC());
        const QImage native = QImage(reinterpret_cast<uchar*>(surface.GetPixels()), view.width(), view.height(),
            view.width() * 4, QImage::Format_RGB32).copy();
        const int nativeRed = redPixels(native, historyBounds);
        Require((qtRed > 20) == (value != 0) && (nativeRed > 20) == (value != 0),
            "native/Qt state did not paint or hide the source history");
        const QString stem = "chat-state-" + QString::number(value);
        Require(qt.save(output + "/" + stem + ".qml.png")
            && native.save(output + "/" + stem + ".native.png"), "chat state frame save failed");
        cases.push_back(QJsonObject{{"state",value},{"height",chat->m_h},{"bottom",chat->m_y+chat->m_h},
            {"qt_history_pixels",qtRed},{"native_history_pixels",nativeRed},{"editor_focused",true}});
    }
    QFile file(output + "/chat-state-results.json");
    Require(file.open(QIODevice::WriteOnly), "chat state results open failed");
    file.write(QJsonDocument(QJsonObject{{"cases",cases},{"main_state_phase","PASS"},
        {"auxiliary_lifecycle_pending",true}}).toJson());
}
